/** @file platform.h @brief Private OS portability boundary. */
#ifndef NGENE_PLATFORM_H
#define NGENE_PLATFORM_H
#include <stdint.h>
#include <stdio.h>
#ifdef _WIN32
#ifndef _WIN32_WINNT
#define _WIN32_WINNT 0x0601
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
/** @brief Native mutex storage isolated behind the portable NgMutex API. */
typedef CRITICAL_SECTION TNgMutex;
/** @brief Native condition storage isolated behind the portable notification API. */
typedef CONDITION_VARIABLE TNgCondition;
/** @brief Native thread handle retained until NgThreadJoin. */
typedef HANDLE TNgThread;

/** @brief Private atomic integer published with acquire/release ordering. */
typedef struct {
    /** OS-compatible atomic integer storage; access only through NgAtomicGet/Set. */
    volatile LONG value;
} TNgAtomicInt;
#else
#include <pthread.h>
/** @brief Native mutex storage isolated behind the portable NgMutex API. */
typedef pthread_mutex_t TNgMutex;
/** @brief Native condition storage isolated behind the portable notification API. */
typedef pthread_cond_t TNgCondition;
/** @brief Native thread handle retained until NgThreadJoin. */
typedef pthread_t TNgThread;

/** @brief Private atomic integer published with acquire/release ordering. */
typedef struct {
    /** OS-compatible atomic integer storage; access only through NgAtomicGet/Set. */
    int value;
} TNgAtomicInt;
#endif
/** @brief Portable thread callback, invoked once by NgThreadStart.
 * @param context Borrowed owner context, retained until NgThreadJoin.
 */
typedef void (*TNgThreadFn)(void *context);
/** Logical processors available to this process; at least one. */
unsigned NgLogicalProcessorCount(void);
/** Advisory current-thread scheduling: negative background, positive foreground, zero normal.
 * Windows uses normal-class thread priorities; other targets retain their OS defaults. */
void NgThreadSchedulingPriority(int priority);
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
int NgMutexInit(TNgMutex *mutex);
/**
 * @brief Destroy an initialized mutex after all users stop.
 *
 * @details
 * The mutex must be unlocked and no thread may wait on or acquire it.
 *
 * @param mutex Initialized mutex storage.
 */
void NgMutexDestroy(TNgMutex *mutex);
/**
 * @brief Acquire the mutex, waiting until it becomes available.
 *
 * @details
 * Pair with NgMutexUnlock on the same thread. Not appropriate for optional diagnostic producer
 * taps.
 *
 * @param mutex Initialized mutex.
 */
void NgMutexLock(TNgMutex *mutex);
/**
 * @brief Release the mutex held by this thread.
 *
 * @details
 * The calling thread must own the mutex.
 *
 * @param mutex Mutex held by the calling thread.
 */
void NgMutexUnlock(TNgMutex *mutex);
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
int NgMutexTryLock(TNgMutex *mutex);
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
int NgConditionInit(TNgCondition *condition);
/**
 * @brief Destroy condition notification after waiters stop.
 *
 * @details
 * Caller must ensure no outstanding wait operation uses the object.
 *
 * @param condition Initialized condition storage.
 */
void NgConditionDestroy(TNgCondition *condition);
/**
 * @brief Wake threads waiting on a condition notification.
 *
 * @details
 * Broadcasts a notification. Callers must keep their predicate under the associated mutex;
 * waking does not imply it became true.
 *
 * @param condition Initialized condition notification.
 */
void NgConditionSignal(TNgCondition *condition);
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
void NgConditionWait(TNgCondition *condition, TNgMutex *mutex, unsigned timeoutMs);
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
int NgThreadStart(TNgThread *thread, TNgThreadFn function, void *context);
/**
 * @brief Wait for a started thread and release its OS handle.
 *
 * @details
 * Must not join the calling thread. Callback context may be released afterwards.
 *
 * @param thread Handle from a successful NgThreadStart.
 */
void NgThreadJoin(TNgThread thread);
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
int NgAtomicGet(TNgAtomicInt *value);
/**
 * @brief Store a portable atomic integer with release ordering.
 *
 * @details
 * Used to publish stop/failure flags; does not make unrelated accesses atomic.
 *
 * @param value Atomic integer storage.
 * @param desired Value to publish.
 */
void NgAtomicSet(TNgAtomicInt *value, int desired);
/**
 * @brief Read the monotonic process clock in seconds.
 *
 * @details
 * Use for elapsed durations and subscription deadlines; this is not UTC, GNSS time or a sample
 * timestamp.
 *
 * @return Monotonic seconds as double.
 */
double NgMonotonicSeconds(void);
/**
 * @brief Suspend the current thread for at least the requested scheduler delay.
 *
 * @details
 * No exact timing guarantee; used by control/test loops rather than per-sample processing.
 *
 * @param milliseconds Delay in milliseconds.
 */
void NgSleepMs(unsigned milliseconds);
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
FILE *NgFopenExclusive(const char *path);
#endif
