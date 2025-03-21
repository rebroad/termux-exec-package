#ifndef LIBTERMUX_EXEC__NOS__C__TERMUX_EXEC_SHELL_ENVIRONMENT___H
#define LIBTERMUX_EXEC__NOS__C__TERMUX_EXEC_SHELL_ENVIRONMENT___H

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif



/*
 * Environment for `termux-exec`.
 */

/**
 * Environment variable for the log level for `termux-exec`.
 *
 * Type: `int`
 * Default key: `TERMUX_EXEC__LOG_LEVEL`
 * Default value: DEFAULT_LOG_LEVEL
 * Values:
 * - `0` (`OFF`) - Log nothing.
 * - `1` (`NORMAL`) - Log error, warn and info messages and stacktraces.
 * - `2` (`DEBUG`) - Log debug messages.
 * - `3` (`VERBOSE`) - Log verbose messages.
 * - `4` (`VVERBOSE`) - Log very verbose messages.
 */
#define ENV__TERMUX_EXEC__LOG_LEVEL TERMUX_ENV__S_TERMUX_EXEC "LOG_LEVEL"





/**
 * Termux environment variables `termux-exec` `execve()` call scope.
 *
 * Default value: `TERMUX_EXEC__EXECVE_CALL__`
 */
#define TERMUX_ENV__S_TERMUX_EXEC__EXECVE_CALL TERMUX_ENV__S_TERMUX_EXEC "EXECVE_CALL__"

/**
 * Environment variable for whether `termux-exec` should intercept
 * `execve()` wrapper declared in `unistd.h`.
 *
 * Type: `string`
 * Default key: `TERMUX_EXEC__EXECVE_CALL__INTERCEPT`
 * Default value: ENV_DEF_VAL__TERMUX_EXEC__EXECVE_CALL__INTERCEPT
 * Values:
 * - `disable` - Intercept `execve()` will be disabled.
 * - `enable` - Intercept `execve()` will be enabled.
 */
#define ENV__TERMUX_EXEC__EXECVE_CALL__INTERCEPT TERMUX_ENV__S_TERMUX_EXEC__EXECVE_CALL "INTERCEPT"
static const int ENV_DEF_VAL__TERMUX_EXEC__EXECVE_CALL__INTERCEPT = 1;


/**
 * Returns the `termux-exec` config for `Logger` log level
 * based on the `ENV__TERMUX_EXEC__LOG_LEVEL` env variable.
 *
 * @return Return the value if `ENV__TERMUX_EXEC__LOG_LEVEL` is
 * set, otherwise defaults to `DEFAULT_LOG_LEVEL`.
 */
int termuxExec_logLevel_get();



/**
 * Returns the `termux-exec` config for whether `execve` should be
 * intercepted based on the `ENV__TERMUX_EXEC__EXECVE_CALL__INTERCEPT` env variable.
 *
 * @return Return `0` if `ENV__TERMUX_EXEC__EXECVE_CALL__INTERCEPT` is
 * set to `disable`, `1` if set to `enable`, otherwise defaults to
 * `1` (`enable`).
 */
int termuxExec_execveCall_intercept_get();



#ifdef __cplusplus
}
#endif

#endif // LIBTERMUX_EXEC__NOS__C__TERMUX_EXEC_SHELL_ENVIRONMENT___H
