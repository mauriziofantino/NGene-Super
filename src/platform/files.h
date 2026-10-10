/** @file files.h @brief Local service files and session entropy. Paths use OS narrow APIs. */
#ifndef NGENE_FILES_H
#define NGENE_FILES_H
#include <stddef.h>
#include <stdint.h>
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
int NgRandomBytes(unsigned char *bytes, size_t count);
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
int NgDirectoryPrepare(const char *path, char *absolute, size_t capacity);
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
int NgFileReplace(const char *temporary, const char *destination);
/* Only regular, non-symlink files immediately inside the selected directory. */
/** @brief Visitor for one immediate regular directory entry.
 * @param context Borrowed caller-owned accumulator.
 * @param name Borrowed filename valid only during the callback.
 * @param bytes File length in bytes.
 */
typedef void (*TNgFileVisitor)(void *context, const char *name, uint64_t bytes);
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
int NgDirectoryFiles(const char *path, TNgFileVisitor visitor, void *context);
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
int NgRegularFile(const char *path, uint64_t *bytes);
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
int NgDirectoryRemoveFile(const char *directory, const char *name);

/** @brief Visitor for one immediate browsable directory or regular file.
 * @param context Borrowed caller accumulator.
 * @param name Borrowed immediate name (or absolute drive root), valid during this call.
 * @param bytes Regular-file length, or zero for directories.
 * @param directory Nonzero for a directory.
 */
typedef void (*TNgPathVisitor)(void *context, const char *name, uint64_t bytes, int directory);

/** @brief Resolve an existing directory without creating or modifying it.
 * @param path Existing directory.
 * @param absolute Receives its NUL-terminated absolute path.
 * @param capacity Destination capacity, including terminator.
 * @return Zero on success; -1 on path, type or capacity failure.
 */
int NgDirectoryResolve(const char *path, char *absolute, size_t capacity);

/** @brief Enumerate immediate nonsymlink files and subdirectories for a receiver-side picker.
 * @details Read-only, synchronous and nonrecursive; order is unspecified.
 * @param path Existing directory.
 * @param visitor Callback retaining no borrowed names.
 * @param context Borrowed accumulator.
 * @return Zero on success; -1 on directory or enumeration failure.
 */
int NgDirectoryBrowse(const char *path, TNgPathVisitor visitor, void *context);

/** @brief List existing Windows drive roots, or the POSIX filesystem root.
 * @param visitor Synchronous root visitor.
 * @param context Borrowed accumulator.
 */
void NgDirectoryRoots(TNgPathVisitor visitor, void *context);

/** @brief Resolve an existing nonsymlink regular file without changing it.
 * @param path Existing file.
 * @param absolute Receives absolute path.
 * @param capacity Destination capacity including terminator.
 * @return Zero on success; -1 if absent, nonregular or too long.
 */
int NgFileAbsolute(const char *path, char *absolute, size_t capacity);

/** @brief Compare two resolved paths with the target OS's filename case convention.
 * @param first NUL-terminated resolved path.
 * @param second NUL-terminated resolved path.
 * @return Nonzero when equal.
 */
int NgPathEqual(const char *first, const char *second);

/** @brief Determine whether an entry exists, including nonregular entries.
 * @param path NUL-terminated path.
 * @return Zero when absent; nonzero for an existing entry or an inspection error.
 */
int NgPathExists(const char *path);
#endif
