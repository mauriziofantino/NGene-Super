/** @file files.c
 * @brief NGene platform implementation.
 */
#include "platform/files.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <limits.h>
#ifdef _WIN32
#include <windows.h>
#include <bcrypt.h>
#include <direct.h>

/**
 * @brief Fill a buffer from the OS cryptographic random source.
 *
 * @details
 * Used for unpredictable web tokens/capture suffixes, not GNSS signal generation. No new
 * caller-owned allocation occurs.
 *
 * @param bytes Writable destination with count bytes.
 * @param count Number of random bytes requested.
 *
 * @return Zero if fully filled; -1 on source/range/read failure.
 */
int NgRandomBytes(unsigned char *bytes, size_t count)
{
    return count <= ULONG_MAX &&
                   BCryptGenRandom(NULL, bytes, (ULONG)count, BCRYPT_USE_SYSTEM_PREFERRED_RNG) == 0
               ? 0
               : -1;
}

/**
 * @brief Create or resolve one directory and return its absolute path.
 *
 * @details
 * Does not recursively create missing parents. Uses narrow OS path APIs; Windows non-ASCII
 * paths are not yet a portable contract.
 *
 * @param path Existing directory or one new child directory path.
 * @param absolute Receives NUL-terminated absolute directory on success.
 * @param capacity Destination capacity including terminator.
 *
 * @return Zero on success; -1 for filesystem or capacity errors.
 */
int NgDirectoryPrepare(const char *path, char *absolute, size_t capacity)
{
    DWORD n;
    if (_mkdir(path) != 0 && errno != EEXIST) {
        return -1;
    }
    n = GetFullPathNameA(path, (DWORD)capacity, absolute, NULL);
    if (n == 0 || n >= capacity) {
        return -1;
    }
    return (GetFileAttributesA(absolute) != INVALID_FILE_ATTRIBUTES &&
            (GetFileAttributesA(absolute) & FILE_ATTRIBUTE_DIRECTORY))
               ? 0
               : -1;
}

/**
 * @brief Atomically replace a destination path with a prepared temporary file.
 *
 * @details
 * Both files must be on the same filesystem. Caller writes and closes temporary contents
 * before calling; it becomes the destination on success.
 *
 * @param temporary Closed prepared temporary file path.
 * @param destination Destination file path.
 *
 * @return Zero on success; -1 on filesystem failure.
 */
int NgFileReplace(const char *temporary, const char *destination)
{
    return MoveFileExA(temporary, destination, MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)
               ? 0
               : -1;
}

/**
 * @brief Check a nonsymlink regular file and read its size.
 *
 * @details
 * Rejects directories and Windows reparse points. This is metadata inspection, not an opened-
 * file lease; callers must account for external filesystem changes.
 *
 * @param path File path to inspect.
 * @param bytes Receives byte length on success.
 *
 * @return Zero for a regular file; -1 otherwise.
 */
int NgRegularFile(const char *path, uint64_t *bytes)
{
    WIN32_FILE_ATTRIBUTE_DATA data;
    if (GetFileAttributesExA(path, GetFileExInfoStandard, &data) == 0 ||
        (data.dwFileAttributes & (FILE_ATTRIBUTE_DIRECTORY | FILE_ATTRIBUTE_REPARSE_POINT))) {
        return -1;
    }
    *bytes = ((uint64_t)data.nFileSizeHigh << 32) | data.nFileSizeLow;
    return 0;
}

/**
 * @brief Visit immediate nonsymlink regular files in a directory.
 *
 * @details
 * No recursion. Calls the visitor synchronously with a filename borrowed for that call.
 * Enumeration order is unspecified and caller retains context ownership.
 *
 * @param path Directory to enumerate.
 * @param visitor Visitor invoked for each regular entry.
 * @param context Borrowed visitor context for this enumeration.
 *
 * @return Zero on successful enumeration; -1 if directory cannot be opened/read.
 */
int NgDirectoryFiles(const char *path, TNgFileVisitor visitor, void *context)
{
    char pattern[1024];
    WIN32_FIND_DATAA data;
    HANDLE handle;
    if (snprintf(pattern, sizeof(pattern), "%s/*", path) >= (int)sizeof(pattern)) {
        return -1;
    }
    handle = FindFirstFileA(pattern, &data);
    if (handle == INVALID_HANDLE_VALUE) {
        return -1;
    }
    do {
        if ((data.dwFileAttributes & (FILE_ATTRIBUTE_DIRECTORY | FILE_ATTRIBUTE_REPARSE_POINT)) ==
            0) {
            visitor(context, data.cFileName,
                    ((uint64_t)data.nFileSizeHigh << 32) | data.nFileSizeLow);
        }
    } while (FindNextFileA(handle, &data) != 0);
    FindClose(handle);
    return 0;
}

/**
 * @brief Remove one immediate regular directory entry.
 *
 * @details
 * Caller must validate name has no path separators/traversal. POSIX uses a directory
 * descriptor and no-follow metadata; Windows rejects reparse points. Does not recursively
 * remove directories or follow file symlinks.
 *
 * @param directory Absolute selected directory.
 * @param name Validated immediate filename.
 *
 * @return Zero if removed; 1 if absent; -1 for nonregular entries or filesystem errors.
 */
int NgDirectoryRemoveFile(const char *directory, const char *name)
{
    char path[1024];
    uint64_t bytes;
    if (snprintf(path, sizeof(path), "%s/%s", directory, name) >= (int)sizeof(path)) {
        return -1;
    }
    if (NgRegularFile(path, &bytes) != 0) {
        return GetLastError() == ERROR_FILE_NOT_FOUND ? 1 : -1;
    }
    return DeleteFileA(path) ? 0 : -1;
}
#else
#include <sys/stat.h>
#include <dirent.h>
#include <unistd.h>
#include <limits.h>
#include <fcntl.h>

/**
 * @brief Fill a buffer from the OS cryptographic random source.
 *
 * @details
 * Used for unpredictable web tokens/capture suffixes, not GNSS signal generation. No new
 * caller-owned allocation occurs.
 *
 * Parameter bytes: Writable destination with count bytes.
 * Parameter count: Number of random bytes requested.
 *
 * Result: Zero if fully filled; -1 on source/range/read failure.
 */
int NgRandomBytes(unsigned char *bytes, size_t count)
{
    FILE *file = fopen("/dev/urandom", "rb");
    size_t got;
    if (file == NULL) {
        return -1;
    }
    got = fread(bytes, 1, count, file);
    fclose(file);
    return got == count ? 0 : -1;
}

/**
 * @brief Create or resolve one directory and return its absolute path.
 *
 * @details
 * Does not recursively create missing parents. Uses narrow OS path APIs; Windows non-ASCII
 * paths are not yet a portable contract.
 *
 * Parameter path: Existing directory or one new child directory path.
 * Parameter absolute: Receives NUL-terminated absolute directory on success.
 * Parameter capacity: Destination capacity including terminator.
 *
 * Result: Zero on success; -1 for filesystem or capacity errors.
 */
int NgDirectoryPrepare(const char *path, char *absolute, size_t capacity)
{
    char *resolved;
    struct stat status;
    if (mkdir(path, 0700) != 0 && errno != EEXIST) {
        return -1;
    }
    resolved = realpath(path, NULL);
    if (resolved == NULL) {
        return -1;
    }
    if (strlen(resolved) >= capacity || stat(resolved, &status) != 0 ||
        S_ISDIR(status.st_mode) == 0) {
        free(resolved);
        return -1;
    }
    memcpy(absolute, resolved, strlen(resolved) + 1);
    free(resolved);
    return 0;
}

/**
 * @brief Atomically replace a destination path with a prepared temporary file.
 *
 * @details
 * Both files must be on the same filesystem. Caller writes and closes temporary contents
 * before calling; it becomes the destination on success.
 *
 * Parameter temporary: Closed prepared temporary file path.
 * Parameter destination: Destination file path.
 *
 * Result: Zero on success; -1 on filesystem failure.
 */
int NgFileReplace(const char *temporary, const char *destination)
{
    return rename(temporary, destination);
}

/**
 * @brief Check a nonsymlink regular file and read its size.
 *
 * @details
 * Rejects directories and Windows reparse points. This is metadata inspection, not an opened-
 * file lease; callers must account for external filesystem changes.
 *
 * Parameter path: File path to inspect.
 * Parameter bytes: Receives byte length on success.
 *
 * Result: Zero for a regular file; -1 otherwise.
 */
int NgRegularFile(const char *path, uint64_t *bytes)
{
    struct stat status;
    if (lstat(path, &status) != 0 || S_ISREG(status.st_mode) == 0 || status.st_size < 0) {
        return -1;
    }
    *bytes = (uint64_t)status.st_size;
    return 0;
}

/**
 * @brief Visit immediate nonsymlink regular files in a directory.
 *
 * @details
 * No recursion. Calls the visitor synchronously with a filename borrowed for that call.
 * Enumeration order is unspecified and caller retains context ownership.
 *
 * Parameter path: Directory to enumerate.
 * Parameter visitor: Visitor invoked for each regular entry.
 * Parameter context: Borrowed visitor context for this enumeration.
 *
 * Result: Zero on successful enumeration; -1 if directory cannot be opened/read.
 */
int NgDirectoryFiles(const char *path, TNgFileVisitor visitor, void *context)
{
    DIR *directory = opendir(path);
    struct dirent *entry;
    char full[1024];
    if (directory == NULL) {
        return -1;
    }
    while ((entry = readdir(directory)) != NULL) {
        uint64_t bytes;
        if (snprintf(full, sizeof(full), "%s/%s", path, entry->d_name) < (int)sizeof(full) &&
            NgRegularFile(full, &bytes) == 0) {
            visitor(context, entry->d_name, bytes);
        }
    }
    closedir(directory);
    return 0;
}

/**
 * @brief Remove one immediate regular directory entry.
 *
 * @details
 * Caller must validate name has no path separators/traversal. POSIX uses a directory
 * descriptor and no-follow metadata; Windows rejects reparse points. Does not recursively
 * remove directories or follow file symlinks.
 *
 * Parameter directory: Absolute selected directory.
 * Parameter name: Validated immediate filename.
 *
 * Result: Zero if removed; 1 if absent; -1 for nonregular entries or filesystem errors.
 */
int NgDirectoryRemoveFile(const char *directory, const char *name)
{
    struct stat status;
    int result;
    int descriptor = open(directory, O_RDONLY | O_DIRECTORY | O_NOFOLLOW);
    if (descriptor < 0) {
        return -1;
    }
    result = fstatat(descriptor, name, &status, AT_SYMLINK_NOFOLLOW);
    if (result != 0) {
        result = errno == ENOENT ? 1 : -1;
    } else if (S_ISREG(status.st_mode) == 0) {
        result = -1;
    } else {
        result = unlinkat(descriptor, name, 0);
    }
    close(descriptor);
    return result;
}
#endif
