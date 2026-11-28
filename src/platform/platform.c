/** @file platform.c
 * @brief NGene platform implementation.
 */
#include "platform/platform.h"
#include <stdlib.h>
#include <fcntl.h>
#include <errno.h>
#ifndef _WIN32
#include <unistd.h>
#include <time.h>
#include <sys/resource.h>
#endif
#ifdef __APPLE__
#include <pthread/qos.h>
#endif
/** Query the process CPU allowance without binding workers to particular processors. */
unsigned NgLogicalProcessorCount(void)
{
#ifdef _WIN32
    DWORD_PTR available = 0, systemMask = 0;
    unsigned count = 0;
    if (GetProcessAffinityMask(GetCurrentProcess(), &available, &systemMask)) {
        while (available) {
            count += (unsigned)(available & 1);
            available >>= 1;
        }
    }
    if (!count) {
        SYSTEM_INFO info;
        GetSystemInfo(&info);
        count = (unsigned)info.dwNumberOfProcessors;
    }
    return count ? count : 1u;
#else
    long count = sysconf(_SC_NPROCESSORS_ONLN);
    return count > 0 ? (unsigned)count : 1u;
#endif
}

/** Give foreground GNSS work precedence over replaceable acquisition/plot work.
 * Windows uses thread priorities, macOS quality-of-service classes and Linux the
 * per-thread nice value. Linux raises no priority: that would need privileges. */
void NgThreadSchedulingPriority(int priority)
{
#ifdef _WIN32
    (void)SetThreadPriority(GetCurrentThread(), priority > 0   ? THREAD_PRIORITY_ABOVE_NORMAL
                                                : priority < 0 ? THREAD_PRIORITY_BELOW_NORMAL
                                                               : THREAD_PRIORITY_NORMAL);
#elif defined(__APPLE__)
    (void)pthread_set_qos_class_self_np(priority > 0   ? QOS_CLASS_USER_INTERACTIVE
                                        : priority < 0 ? QOS_CLASS_UTILITY
                                                       : QOS_CLASS_USER_INITIATED,
                                        0);
#elif defined(__linux__)
    /* NPTL keeps the nice value per thread; who = 0 names the calling thread. */
    if (priority < 0) {
        (void)setpriority(PRIO_PROCESS, 0, 10);
    }
#else
    (void)priority;
#endif
}

/** CPU time consumed by the calling thread, for signal-time CPU budgets. */
double NgThreadCpuSeconds(void)
{
#ifdef _WIN32
    FILETIME creation, exited, kernel, user;
    if (!GetThreadTimes(GetCurrentThread(), &creation, &exited, &kernel, &user)) {
        return 0;
    }
    return ((double)(((uint64_t)kernel.dwHighDateTime << 32) | kernel.dwLowDateTime) +
            (double)(((uint64_t)user.dwHighDateTime << 32) | user.dwLowDateTime)) *
           1e-7;
#else
    struct timespec now;
    if (clock_gettime(CLOCK_THREAD_CPUTIME_ID, &now) != 0) {
        return 0;
    }
    return (double)now.tv_sec + (double)now.tv_nsec * 1e-9;
#endif
}

/** @brief Owned startup wrapper freed by the new thread; context remains borrowed. */
typedef struct {
    /** Borrowed callback invoked exactly once by the new thread. */
    TNgThreadFn function;
    /** Borrowed callback context retained until the owning thread is joined. */
    void *context;
} TNgThreadStartData;
#ifdef _WIN32
#include <process.h>
#include <io.h>
#include <sys/stat.h>

/**
 * @brief Run the portable callback and free its startup wrapper.
 *
 * @details
 * Extracts callback/context, frees only the wrapper, then calls the callback. Context
 * ownership remains with the joining thread.
 *
 * @param argument Owned startup wrapper allocated by NgThreadStart.
 *
 * @return OS thread entry return (zero/NULL after callback completes).
 */
static unsigned __stdcall ThreadEntry(void *argument)
{
    TNgThreadStartData data = *(TNgThreadStartData *)argument;
    free(argument);
    data.function(data.context);
    return 0;
}

/**
 * @brief Initialize a private cross-platform mutex.
 *
 * @details
 * Creates Win32 critical-section or POSIX mutex state; destroy after users stop.
 *
 * @param mutex Uninitialized caller-owned mutex storage.
 *
 * @return Zero on success; -1 on initialization failure.
 */
int NgMutexInit(TNgMutex *mutex)
{
    InitializeCriticalSection(mutex);
    return 0;
}

/**
 * @brief Destroy an initialized mutex after all users stop.
 *
 * @details
 * The mutex must be unlocked and no thread may wait on or acquire it.
 *
 * @param mutex Initialized mutex storage.
 */
void NgMutexDestroy(TNgMutex *mutex)
{
    DeleteCriticalSection(mutex);
}

/**
 * @brief Acquire the mutex, waiting until it becomes available.
 *
 * @details
 * Pair with NgMutexUnlock on the same thread. Not appropriate for optional diagnostic producer
 * taps.
 *
 * @param mutex Initialized mutex.
 */
void NgMutexLock(TNgMutex *mutex)
{
    EnterCriticalSection(mutex);
}

/**
 * @brief Attempt to acquire a mutex without waiting.
 *
 * @details
 * On success the caller must unlock. Used to skip optional status/diagnostic work when
 * contention would delay samples.
 *
 * @param mutex Initialized mutex.
 *
 * @return Nonzero if acquired; zero if unavailable.
 */
int NgMutexTryLock(TNgMutex *mutex)
{
    return TryEnterCriticalSection(mutex) != 0;
}

/**
 * @brief Release the mutex held by this thread.
 *
 * @details
 * The calling thread must own the mutex.
 *
 * @param mutex Mutex held by the calling thread.
 */
void NgMutexUnlock(TNgMutex *mutex)
{
    LeaveCriticalSection(mutex);
}

/**
 * @brief Initialize a portable condition-variable notification.
 *
 * @details
 * POSIX uses a monotonic-clock deadline; macOS uses relative waiting. Pair with destroy after
 * users stop.
 *
 * @param condition Uninitialized condition storage.
 *
 * @return Zero on success; -1 on failure.
 */
int NgConditionInit(TNgCondition *condition)
{
    InitializeConditionVariable(condition);
    return 0;
}

/**
 * @brief Destroy condition notification after waiters stop.
 *
 * @details
 * Caller must ensure no outstanding wait operation uses the object.
 *
 * @param condition Initialized condition storage.
 */
void NgConditionDestroy(TNgCondition *condition)
{
    (void)condition;
}

/**
 * @brief Wake threads waiting on a condition notification.
 *
 * @details
 * Broadcasts a notification. Callers must keep their predicate under the associated mutex;
 * waking does not imply it became true.
 *
 * @param condition Initialized condition notification.
 */
void NgConditionSignal(TNgCondition *condition)
{
    WakeAllConditionVariable(condition);
}

/**
 * @brief Temporarily release a mutex while waiting for a notification or timeout.
 *
 * @details
 * Caller owns the mutex on entry and again on return. May return spuriously; recheck the
 * shared predicate in a loop.
 *
 * @param condition Condition notification to wait on.
 * @param mutex Associated mutex already owned by the calling thread.
 * @param timeoutMs Maximum wait duration in milliseconds.
 */
void NgConditionWait(TNgCondition *condition, TNgMutex *mutex, unsigned timeoutMs)
{
    (void)SleepConditionVariableCS(condition, mutex, timeoutMs);
}

/**
 * @brief Start a portable thread with a C callback and opaque context.
 *
 * @details
 * Allocates a small startup wrapper freed by the new thread. The caller owns a successful
 * handle and must join it before destroying callback context.
 *
 * @param thread Receives the thread handle on success.
 * @param function Callback to execute once in the new thread.
 * @param context Borrowed callback context, valid until join.
 *
 * @return Zero on success; -1 if allocation/thread creation fails.
 */
int NgThreadStart(TNgThread *thread, TNgThreadFn function, void *context)
{
    TNgThreadStartData *data = malloc(sizeof(*data));
    if (data == NULL) {
        return -1;
    }
    data->function = function;
    data->context = context;
    *thread = (HANDLE)_beginthreadex(NULL, 0, ThreadEntry, data, 0, NULL);
    if (*thread == NULL) {
        free(data);
        return -1;
    }
    return 0;
}

/**
 * @brief Wait for a started thread and release its OS handle.
 *
 * @details
 * Must not join the calling thread. Callback context may be released afterwards.
 *
 * @param thread Handle from a successful NgThreadStart.
 */
void NgThreadJoin(TNgThread thread)
{
    WaitForSingleObject(thread, INFINITE);
    CloseHandle(thread);
}

/**
 * @brief Load a portable atomic integer with acquire ordering.
 *
 * @details
 * Used for thread stop/failure flags. The underlying storage must remain valid.
 *
 * @param value Atomic integer storage.
 *
 * @return Current integer value.
 */
int NgAtomicGet(TNgAtomicInt *value)
{
    return (int)InterlockedCompareExchange(&value->value, 0, 0);
}

/**
 * @brief Store a portable atomic integer with release ordering.
 *
 * @details
 * Used to publish stop/failure flags; does not make unrelated accesses atomic.
 *
 * @param value Atomic integer storage.
 * @param desired Value to publish.
 */
void NgAtomicSet(TNgAtomicInt *value, int desired)
{
    InterlockedExchange(&value->value, (LONG)desired);
}

/**
 * @brief Read the monotonic process clock in seconds.
 *
 * @details
 * Use for elapsed durations and subscription deadlines; this is not UTC, GNSS time or a sample
 * timestamp.
 *
 * @return Monotonic seconds as double.
 */
double NgMonotonicSeconds(void)
{
    LARGE_INTEGER counter;
    LARGE_INTEGER frequency;
    QueryPerformanceCounter(&counter);
    QueryPerformanceFrequency(&frequency);
    return (double)counter.QuadPart / (double)frequency.QuadPart;
}

/**
 * @brief Suspend the current thread for at least the requested scheduler delay.
 *
 * @details
 * No exact timing guarantee; used by control/test loops rather than per-sample processing.
 *
 * @param milliseconds Delay in milliseconds.
 */
void NgSleepMs(unsigned milliseconds)
{
    Sleep(milliseconds);
}

/**
 * @brief Create a new binary output file without replacing an existing file.
 *
 * @details
 * Uses an exclusive OS open, then returns a C stream. Caller must close it and check
 * write/flush errors.
 *
 * @param path New file path, parent directory already present.
 *
 * @return Owned writable binary FILE stream, or NULL on failure.
 */
FILE *NgFopenExclusive(const char *path)
{
    int fd = _open(path, _O_CREAT | _O_EXCL | _O_WRONLY | _O_BINARY, _S_IREAD | _S_IWRITE);
    FILE *file;
    if (fd < 0) {
        return NULL;
    }
    file = _fdopen(fd, "wb");
    if (file == NULL) {
        _close(fd);
    }
    return file;
}
#else
#include <unistd.h>
#include <time.h>

/**
 * @brief Run the portable callback and free its startup wrapper.
 *
 * @details
 * Extracts callback/context, frees only the wrapper, then calls the callback. Context
 * ownership remains with the joining thread.
 *
 * @param argument Owned startup wrapper allocated by NgThreadStart.
 *
 * @return OS thread entry return (zero/NULL after callback completes).
 */
static void *ThreadEntry(void *argument)
{
    TNgThreadStartData data = *(TNgThreadStartData *)argument;
    free(argument);
    data.function(data.context);
    return NULL;
}

/**
 * @brief Initialize a private cross-platform mutex.
 *
 * @details
 * Creates Win32 critical-section or POSIX mutex state; destroy after users stop.
 *
 * Parameter mutex: Uninitialized caller-owned mutex storage.
 *
 * Result: Zero on success; -1 on initialization failure.
 */
int NgMutexInit(TNgMutex *mutex)
{
    return pthread_mutex_init(mutex, NULL);
}

/**
 * @brief Destroy an initialized mutex after all users stop.
 *
 * @details
 * The mutex must be unlocked and no thread may wait on or acquire it.
 *
 * Parameter mutex: Initialized mutex storage.
 */
void NgMutexDestroy(TNgMutex *mutex)
{
    (void)pthread_mutex_destroy(mutex);
}

/**
 * @brief Acquire the mutex, waiting until it becomes available.
 *
 * @details
 * Pair with NgMutexUnlock on the same thread. Not appropriate for optional diagnostic producer
 * taps.
 *
 * Parameter mutex: Initialized mutex.
 */
void NgMutexLock(TNgMutex *mutex)
{
    (void)pthread_mutex_lock(mutex);
}

/**
 * @brief Attempt to acquire a mutex without waiting.
 *
 * @details
 * On success the caller must unlock. Used to skip optional status/diagnostic work when
 * contention would delay samples.
 *
 * Parameter mutex: Initialized mutex.
 *
 * Result: Nonzero if acquired; zero if unavailable.
 */
int NgMutexTryLock(TNgMutex *mutex)
{
    return pthread_mutex_trylock(mutex) == 0;
}

/**
 * @brief Release the mutex held by this thread.
 *
 * @details
 * The calling thread must own the mutex.
 *
 * Parameter mutex: Mutex held by the calling thread.
 */
void NgMutexUnlock(TNgMutex *mutex)
{
    (void)pthread_mutex_unlock(mutex);
}

/**
 * @brief Initialize a portable condition-variable notification.
 *
 * @details
 * POSIX uses a monotonic-clock deadline; macOS uses relative waiting. Pair with destroy after
 * users stop.
 *
 * Parameter condition: Uninitialized condition storage.
 *
 * Result: Zero on success; -1 on failure.
 */
int NgConditionInit(TNgCondition *condition)
{
#ifdef __APPLE__
    return pthread_cond_init(condition, NULL);
#else
    pthread_condattr_t attr;
    int status = pthread_condattr_init(&attr);
    if (status != 0) {
        return status;
    }
    status = pthread_condattr_setclock(&attr, CLOCK_MONOTONIC);
    if (status == 0) {
        status = pthread_cond_init(condition, &attr);
    }
    pthread_condattr_destroy(&attr);
    return status;
#endif
}

/**
 * @brief Destroy condition notification after waiters stop.
 *
 * @details
 * Caller must ensure no outstanding wait operation uses the object.
 *
 * Parameter condition: Initialized condition storage.
 */
void NgConditionDestroy(TNgCondition *condition)
{
    (void)pthread_cond_destroy(condition);
}

/**
 * @brief Wake threads waiting on a condition notification.
 *
 * @details
 * Broadcasts a notification. Callers must keep their predicate under the associated mutex;
 * waking does not imply it became true.
 *
 * Parameter condition: Initialized condition notification.
 */
void NgConditionSignal(TNgCondition *condition)
{
    (void)pthread_cond_broadcast(condition);
}

/**
 * @brief Temporarily release a mutex while waiting for a notification or timeout.
 *
 * @details
 * Caller owns the mutex on entry and again on return. May return spuriously; recheck the
 * shared predicate in a loop.
 *
 * Parameter condition: Condition notification to wait on.
 * Parameter mutex: Associated mutex already owned by the calling thread.
 * Parameter timeoutMs: Maximum wait duration in milliseconds.
 */
void NgConditionWait(TNgCondition *condition, TNgMutex *mutex, unsigned timeoutMs)
{
#ifdef __APPLE__
    struct timespec relative = {(time_t)(timeoutMs / 1000u), (long)(timeoutMs % 1000u) * 1000000L};
    (void)pthread_cond_timedwait_relative_np(condition, mutex, &relative);
#else
    struct timespec deadline;
    clock_gettime(CLOCK_MONOTONIC, &deadline);
    deadline.tv_sec += (time_t)(timeoutMs / 1000u);
    deadline.tv_nsec += (long)(timeoutMs % 1000u) * 1000000L;
    if (deadline.tv_nsec >= 1000000000L) {
        ++deadline.tv_sec;
        deadline.tv_nsec -= 1000000000L;
    }
    (void)pthread_cond_timedwait(condition, mutex, &deadline);
#endif
}

/**
 * @brief Start a portable thread with a C callback and opaque context.
 *
 * @details
 * Allocates a small startup wrapper freed by the new thread. The caller owns a successful
 * handle and must join it before destroying callback context.
 *
 * Parameter thread: Receives the thread handle on success.
 * Parameter function: Callback to execute once in the new thread.
 * Parameter context: Borrowed callback context, valid until join.
 *
 * Result: Zero on success; -1 if allocation/thread creation fails.
 */
int NgThreadStart(TNgThread *thread, TNgThreadFn function, void *context)
{
    TNgThreadStartData *data = malloc(sizeof(*data));
    if (data == NULL) {
        return -1;
    }
    data->function = function;
    data->context = context;
    if (pthread_create(thread, NULL, ThreadEntry, data) != 0) {
        free(data);
        return -1;
    }
    return 0;
}

/**
 * @brief Wait for a started thread and release its OS handle.
 *
 * @details
 * Must not join the calling thread. Callback context may be released afterwards.
 *
 * Parameter thread: Handle from a successful NgThreadStart.
 */
void NgThreadJoin(TNgThread thread)
{
    (void)pthread_join(thread, NULL);
}

/**
 * @brief Load a portable atomic integer with acquire ordering.
 *
 * @details
 * Used for thread stop/failure flags. The underlying storage must remain valid.
 *
 * Parameter value: Atomic integer storage.
 *
 * Result: Current integer value.
 */
int NgAtomicGet(TNgAtomicInt *value)
{
    return __atomic_load_n(&value->value, __ATOMIC_ACQUIRE);
}

/**
 * @brief Store a portable atomic integer with release ordering.
 *
 * @details
 * Used to publish stop/failure flags; does not make unrelated accesses atomic.
 *
 * Parameter value: Atomic integer storage.
 * Parameter desired: Value to publish.
 */
void NgAtomicSet(TNgAtomicInt *value, int desired)
{
    __atomic_store_n(&value->value, desired, __ATOMIC_RELEASE);
}

/**
 * @brief Read the monotonic process clock in seconds.
 *
 * @details
 * Use for elapsed durations and subscription deadlines; this is not UTC, GNSS time or a sample
 * timestamp.
 *
 * Result: Monotonic seconds as double.
 */
double NgMonotonicSeconds(void)
{
    struct timespec now;
    clock_gettime(CLOCK_MONOTONIC, &now);
    return (double)now.tv_sec + (double)now.tv_nsec * 1e-9;
}

/**
 * @brief Suspend the current thread for at least the requested scheduler delay.
 *
 * @details
 * No exact timing guarantee; used by control/test loops rather than per-sample processing.
 *
 * Parameter milliseconds: Delay in milliseconds.
 */
void NgSleepMs(unsigned milliseconds)
{
    struct timespec delay = {(time_t)(milliseconds / 1000u),
                             (long)(milliseconds % 1000u) * 1000000L};
    while (nanosleep(&delay, &delay) != 0 && errno == EINTR) {
    }
}

/**
 * @brief Create a new binary output file without replacing an existing file.
 *
 * @details
 * Uses an exclusive OS open, then returns a C stream. Caller must close it and check
 * write/flush errors.
 *
 * Parameter path: New file path, parent directory already present.
 *
 * Result: Owned writable binary FILE stream, or NULL on failure.
 */
FILE *NgFopenExclusive(const char *path)
{
    int fd = open(path, O_CREAT | O_EXCL | O_WRONLY, 0666);
    FILE *file;
    if (fd < 0) {
        return NULL;
    }
    file = fdopen(fd, "wb");
    if (file == NULL) {
        close(fd);
    }
    return file;
}
#endif
