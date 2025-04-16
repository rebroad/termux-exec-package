#include <stdbool.h>

#include <termux/termux_core__nos__c/v1/logger/FileLoggerImpl.h>
#include <termux/termux_core__nos__c/v1/logger/Logger.h>

#include <termux/termux_exec__nos__c/v1/termux/os/process/termux_exec/TermuxExecProcess.h>
#include <termux/termux_exec__nos__c/v1/termux/shell/command/environment/termux_exec/TermuxExecShellEnvironment.h>

static bool sInitLogger = true;
static bool sIgnoreExit = false;



int termuxExec_process_initProcess(const char *versionToLog, const char *logFilePath) {
    // Sometimes when a process starts, errno is set to values like
    // EINVAL (22) and (ECHILD 10). Noticed on Android 11 (aarch64) and
    // Android 14 (x86_64).
    // It is not being caused by `termux-exec` as it happens even
    // without `LD_PRELOAD` being set.
    // Moreover, errno is 0 before `execveSyscall()` is called by
    // `execveIntercept()` to replace the process, but in the `main()`
    // of new process, errno is not zero, so something happens during
    // the `syscall()` itself or in callers of `main()`. And manually
    // setting errno before `execveSyscall()` does not transfer it
    // to `main()` of new process.
    // Programs should set errno to `0` at init themselves.
    // We unset it here since programs should have already handled their
    // errno if it was set by something else and `termux-exec` library
    // also has checks to error out if errno is set in various places,
    // like initially in `stringToInt()` called by `termuxExec_logLevel_get()`.
    // Saving errno is useless as it will not be transferred anyways.
    // - https://wiki.sei.cmu.edu/confluence/display/c/ERR30-C.+Take+care+when+reading+errno
    errno = 0;

    return termuxExec_process_initLogger(versionToLog, logFilePath);
}

int termuxExec_process_initLogger(const char *versionToLog, const char *logFilePath) {
    if (sInitLogger) {
        setDefaultLogTagAndPrefix(TERMUX__LNAME);
        setCurrentLogLevel(termuxExec_logLevel_get());
        setCacheLogPid(true);
        if (logFilePath != NULL) {
            setLogFormatMode(LOG_FORMAT_MODE__PID_PRIORITY_TAG_AND_MESSAGE);
            setLoggerImpl(&sFileLoggerImpl);
            if (setLogFilePath(logFilePath) == -1) {
                return -1;
            }
        } else {
            setLogFormatMode(LOG_FORMAT_MODE__PID_PRIORITY_TAG_AND_MESSAGE);
        }
        sInitLogger = false;

        if (versionToLog != NULL) {
            logErrorVVerbose("", "TERMUX_EXEC__VERSION: '%s'", versionToLog);
        }
    }
    return 0;
}



void termuxExec_process_setIgnoreExit(bool state) {
    sIgnoreExit = state;
}

int termuxExec_process_exitProcess() {
    if (!sIgnoreExit) {
        closeLogFile();
        sInitLogger = true;
    }
    return 0;
}
