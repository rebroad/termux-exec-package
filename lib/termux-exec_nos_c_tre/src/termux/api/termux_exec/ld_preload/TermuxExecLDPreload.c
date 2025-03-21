#define _GNU_SOURCE
#include <errno.h>
#include <fcntl.h>
#include <libgen.h>
#include <limits.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <sys/syscall.h>

#include <termux/termux_core__nos__c/v1/android/shell/command/environment/AndroidShellEnvironment.h>
#include <termux/termux_core__nos__c/v1/data/DataUtils.h>
#include <termux/termux_core__nos__c/v1/logger/Logger.h>
#include <termux/termux_core__nos__c/v1/termux/file/TermuxFile.h>
#include <termux/termux_core__nos__c/v1/termux/shell/command/environment/TermuxShellEnvironment.h>
#include <termux/termux_core__nos__c/v1/unix/os/selinux/UnixSeLinuxUtils.h>

#include <termux/termux_exec__nos__c/v1/TermuxExecLibraryConfig.h>
#include <termux/termux_exec__nos__c/v1/termux/api/termux_exec/ld_preload/TermuxExecLDPreload.h>
#include <termux/termux_exec__nos__c/v1/termux/shell/command/environment/termux_exec/TermuxExecShellEnvironment.h>

static const char* LOG_TAG = "ld-preload";

static int sSystemLinkerExecEnabled = -1;



int isSystemLinkerExecEnabled() {
     if (sSystemLinkerExecEnabled == 0 || sSystemLinkerExecEnabled == 1) {
        return sSystemLinkerExecEnabled;
    }

    bool isRunningTests = libtermux_exec__nos__c__getIsRunningTests();

    int systemLinkerExecMode = termuxExec_systemLinkerExec_mode_get();
    if (!isRunningTests) {
        logErrorVVerbose(LOG_TAG, "system_linker_exec_mode: '%d'", systemLinkerExecMode);
    }

    int systemLinkerExecEnabled = 1;
    if (systemLinkerExecMode == 0) { // disable
        systemLinkerExecEnabled = 1; // disable

    } else if (systemLinkerExecMode == 2) { // force
        int androidBuildVersionSdk = android_buildVersionSdk_get();
        if (!isRunningTests) {
            logErrorVVerbose(LOG_TAG, "android_build_version_sdk: '%d'", androidBuildVersionSdk);
        }

        bool systemLinkerExecAvailable = false;
        // If running on Android `>= 10`.
        systemLinkerExecAvailable = androidBuildVersionSdk >= 29;
        if (!isRunningTests) {
            logErrorVVerbose(LOG_TAG, "system_linker_exec_available: '%d'", systemLinkerExecAvailable);
        }

        if (systemLinkerExecAvailable) {
            systemLinkerExecEnabled = 0; // enable
        }

    } else { // enable
        if (systemLinkerExecMode != 1) {
            logErrorDebug(LOG_TAG, "Warning: Ignoring invalid system_linker_exec_mode value and using '1' instead");
        }

        bool appDataFileExecExempted = false;

        int androidBuildVersionSdk = android_buildVersionSdk_get();
        if (!isRunningTests) {
            logErrorVVerbose(LOG_TAG, "android_build_version_sdk: '%d'", androidBuildVersionSdk);
        }

        // If running on Android `>= 10`.
        if (androidBuildVersionSdk >= 29) {
            // If running as root or shell user, then the process will
            // be assigned a different process context like
            // `PROCESS_CONTEXT__AOSP_SU`,
            // `PROCESS_CONTEXT__MAGISK_SU` or
            // `PROCESS_CONTEXT__SHELL`, which will not be the same
            // as the one that's exported in
            // `ENV__TERMUX__SE_PROCESS_CONTEXT`, so we need to check
            // effective uid equals `0` or `2000` instead. Moreover,
            // other su providers may have different contexts, so we
            // cannot just check AOSP or MAGISK contexts.
            // - https://man7.org/linux/man-pages/man2/getuid.2.html
            uid_t uid = geteuid();
            if (uid == 0 || uid == 2000) {
                logErrorVVerbose(LOG_TAG, "uid: '%d'", uid);
                appDataFileExecExempted = true;
            } else {
                char seProcessContext[80];
                bool getSeProcessContextSuccess = false;

                if (getSeProcessContextFromEnv(LOG_TAG, ENV__TERMUX__SE_PROCESS_CONTEXT,
                        seProcessContext, sizeof(seProcessContext))) {
                    if (!isRunningTests) {
                        logErrorVVerbose(LOG_TAG, "se_process_context_from_env: '%s'", seProcessContext);
                    }
                    getSeProcessContextSuccess = true;
                } else if (getSeProcessContextFromFile(LOG_TAG,
                        seProcessContext, sizeof(seProcessContext))) {
                    if (!isRunningTests) {
                        logErrorVVerbose(LOG_TAG, "se_process_context_from_file: '%s'", seProcessContext);
                    }
                    getSeProcessContextSuccess = true;
                }

                if (getSeProcessContextSuccess) {
                    appDataFileExecExempted = stringStartsWith(seProcessContext, PROCESS_CONTEXT_PREFIX__UNTRUSTED_APP_25) ||
                        stringStartsWith(seProcessContext, PROCESS_CONTEXT_PREFIX__UNTRUSTED_APP_27);
                }
            }

            if (!isRunningTests) {
                logErrorVVerbose(LOG_TAG, "app_data_file_exec_exempted: '%d'", appDataFileExecExempted);
            }

            if (!appDataFileExecExempted) {
                systemLinkerExecEnabled = 0; // enable
            }
        }
    }

    sSystemLinkerExecEnabled = systemLinkerExecEnabled;

    if (!isRunningTests) {
        logErrorVVerbose(LOG_TAG, "system_linker_exec_enabled: '%d'",
            sSystemLinkerExecEnabled == 0 ? true : false);
    }

    return sSystemLinkerExecEnabled;
}

int shouldEnableSystemLinkerExecForFile(const char *executablePath) {
    int systemLinkerExecResult = isSystemLinkerExecEnabled();
    // If error or disabled, then just return.
    if (systemLinkerExecResult != 0) {
        return systemLinkerExecResult;
    }

    bool isRunningTests = libtermux_exec__nos__c__getIsRunningTests();

    int isExecutableUnderTermuxAppDataDir = termuxApp_dataDir_isPathUnder(LOG_TAG,
        executablePath, NULL, NULL);
    if (isExecutableUnderTermuxAppDataDir < 0) {
        return -1;
    }

    if (!isRunningTests) {
        logErrorVVerbose(LOG_TAG, "is_exe_under_termux_app_data_dir: '%d'",
            isExecutableUnderTermuxAppDataDir == 0 ? true : false);
    }

    bool shouldEnableSystemLinkerExec = isExecutableUnderTermuxAppDataDir == 0;

    if (!isRunningTests) {
        logErrorVVerbose(LOG_TAG, "system_linker_exec_enabled_for_file: '%d'",
            shouldEnableSystemLinkerExec);
    }

    return shouldEnableSystemLinkerExec ? 0 : 1;
}
