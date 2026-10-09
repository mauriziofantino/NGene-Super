/** @file error.c
 * @brief NGene core implementation.
 */
#include "ngene/types.h"
#include <stdarg.h>
#include <stdio.h>

/**
 * @brief Format a bounded diagnostic message and return NG_ERROR.
 *
 * @details
 * Accepts printf-style arguments; escape a literal percent as %% in format. A NULL error
 * destination is supported and still returns failure.
 *
 * Parameter error: Optional error structure to fill.
 * Parameter format: printf-style format string.
 * Parameter ...: Arguments required by the format specifiers.
 *
 * Result: NG_ERROR in all cases.
 */
int NgFail(TNgError *error, const char *format, ...)
{
    if (error != NULL) {
        va_list args;
        va_start(args, format);
        (void)vsnprintf(error->message, sizeof(error->message), format, args);
        va_end(args);
    }
    return NG_ERROR;
}
