#define _GNU_SOURCE
#include <errno.h>
#include <fcntl.h>
#include <grp.h>
#include <libgen.h>
#include <limits.h>
#include <pwd.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <dlfcn.h>

#include <sys/syscall.h>

#include <termux/termux_core__nos__c/v1/android/shell/command/environment/AndroidShellEnvironment.h>
#include <termux/termux_core__nos__c/v1/data/DataUtils.h>
#include <termux/termux_core__nos__c/v1/logger/Logger.h>
#include <termux/termux_core__nos__c/v1/termux/file/TermuxFile.h>
#include <termux/termux_core__nos__c/v1/termux/shell/command/environment/TermuxShellEnvironment.h>
#include <termux/termux_core__nos__c/v1/unix/os/selinux/UnixSeLinuxUtils.h>

#include <termux/termux_exec__nos__c/v1/TermuxExecLibraryConfig.h>
#include <termux/termux_exec__nos__c/v1/termux/api/termux_exec/service/ld_preload/TermuxExecLDPreload.h>
#include <termux/termux_exec__nos__c/v1/termux/shell/command/environment/termux_exec/TermuxExecShellEnvironment.h>

static const char* LOG_TAG = "ld-preload";

static int sSystemLinkerExecEnabled = -1;

#define TERMUX_EXEC__HOSTNAME_FILE_PATH TERMUX__PREFIX "/etc/termux/hostname"
#define TERMUX_EXEC__PASSWD_FILE_PATH TERMUX__PREFIX "/etc/passwd"
#define TERMUX_EXEC__GROUP_FILE_PATH TERMUX__PREFIX "/etc/group"



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
                } else {
                    // If even '/proc/self/attr/current' is not accessible,
                    // then SeLinux may not be supported on the device.
                    logErrorVVerbose(LOG_TAG, "se_process_context_available: '0'");
                    appDataFileExecExempted = true;
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

int termuxExec_getConfiguredHostname(char *buffer, size_t bufferSize) {
    if (buffer == NULL || bufferSize == 0) {
        errno = EINVAL;
        return -1;
    }

    const char *hostnameFilePath = getenv(ENV__TERMUX_EXEC__HOSTNAME_FILE);
    if (hostnameFilePath == NULL || strlen(hostnameFilePath) < 1) {
        hostnameFilePath = TERMUX_EXEC__HOSTNAME_FILE_PATH;
    }

    FILE *file = fopen(hostnameFilePath, "r");
    if (file == NULL) {
        if (errno == ENOENT) {
            errno = 0;
            return 1;
        }
        return -1;
    }

    char hostname[HOST_NAME_MAX + 2];
    if (fgets(hostname, sizeof(hostname), file) == NULL) {
        int savedErrno = errno;
        fclose(file);
        errno = savedErrno;
        if (errno == 0) {
            return 1;
        }
        return -1;
    }

    int closeResult = fclose(file);
    if (closeResult != 0) {
        return -1;
    }

    hostname[strcspn(hostname, "\r\n")] = '\0';
    size_t hostnameLength = strlen(hostname);
    if (hostnameLength < 1) {
        return 1;
    }

    if (hostnameLength >= bufferSize) {
        errno = ENAMETOOLONG;
        return -1;
    }

    memcpy(buffer, hostname, hostnameLength + 1);
    return 0;
}

int gethostnameIntercept(char *name, size_t len) {
    int savedErrno = errno;
    int configuredHostnameResult = termuxExec_getConfiguredHostname(name, len);
    if (configuredHostnameResult == 0) {
        errno = savedErrno;
        return 0;
    }
    if (configuredHostnameResult < 0) {
        return -1;
    }

    errno = savedErrno;
#ifdef SYS_gethostname
    return (int) syscall(SYS_gethostname, name, len);
#else
    errno = ENOSYS;
    return -1;
#endif
}

static _Thread_local struct passwd sPasswdEntry;
static _Thread_local char sPasswdLine[4096];
static _Thread_local struct group sGroupEntry;
static _Thread_local char sGroupLine[4096];
static _Thread_local char *sGroupMembers[1] = {NULL};

static char *nextPasswdField(char **cursor) {
    char *field = *cursor;
    char *separator = strchr(field, ':');
    if (separator != NULL) {
        *separator = '\0';
        *cursor = separator + 1;
    } else {
        *cursor = field + strlen(field);
    }
    return field;
}

int termuxExec_getConfiguredPasswdEntry(uid_t uid, const char *name, struct passwd *result) {
    if (name == NULL && result == NULL) {
        errno = EINVAL;
        return -1;
    }

    const char *passwdFilePath = getenv(ENV__TERMUX_EXEC__PASSWD_FILE);
    if (passwdFilePath == NULL || strlen(passwdFilePath) < 1) {
        passwdFilePath = TERMUX_EXEC__PASSWD_FILE_PATH;
    }

    FILE *file = fopen(passwdFilePath, "r");
    if (file == NULL) {
        if (errno == ENOENT) {
            errno = 0;
            return 1;
        }
        return -1;
    }

    while (fgets(sPasswdLine, sizeof(sPasswdLine), file) != NULL) {
        sPasswdLine[strcspn(sPasswdLine, "\r\n")] = '\0';
        if (sPasswdLine[0] == '\0' || sPasswdLine[0] == '#') continue;

        char *cursor = sPasswdLine;
        char *entryName = nextPasswdField(&cursor);
        (void) nextPasswdField(&cursor); // password
        char *uidString = nextPasswdField(&cursor);
        char *gidString = nextPasswdField(&cursor);
        char *gecos = nextPasswdField(&cursor);
        char *home = nextPasswdField(&cursor);
        char *shell = cursor;
        char *end = NULL;
        unsigned long entryUid = strtoul(uidString, &end, 10);
        if (end == uidString || *end != '\0' || entryUid > UINT_MAX) continue;
        end = NULL;
        unsigned long entryGid = strtoul(gidString, &end, 10);
        if (end == gidString || *end != '\0' || entryGid > UINT_MAX) continue;

        if ((name != NULL && strcmp(name, entryName) != 0) ||
            (name == NULL && (uid_t) entryUid != uid)) continue;

        sPasswdEntry.pw_name = entryName;
        sPasswdEntry.pw_passwd = (char *) "x";
        sPasswdEntry.pw_uid = (uid_t) entryUid;
        sPasswdEntry.pw_gid = (gid_t) entryGid;
        sPasswdEntry.pw_gecos = gecos;
        sPasswdEntry.pw_dir = home;
        sPasswdEntry.pw_shell = shell;
        if (result != NULL) *result = sPasswdEntry;
        fclose(file);
        return 0;
    }

    int savedErrno = errno;
    fclose(file);
    errno = savedErrno;
    if (errno == 0) return 1;
    return -1;
}

static struct passwd *getpwuidFallback(uid_t uid) {
    static struct passwd *(*function)(uid_t);
    if (function == NULL) function = dlsym(RTLD_NEXT, "getpwuid");
    if (function == NULL) {
        errno = ENOSYS;
        return NULL;
    }
    return function(uid);
}

static struct passwd *getpwnamFallback(const char *name) {
    static struct passwd *(*function)(const char *);
    if (function == NULL) function = dlsym(RTLD_NEXT, "getpwnam");
    if (function == NULL) {
        errno = ENOSYS;
        return NULL;
    }
    return function(name);
}

struct passwd *getpwuidIntercept(uid_t uid) {
    struct passwd result;
    int configuredResult = termuxExec_getConfiguredPasswdEntry(uid, NULL, &result);
    if (configuredResult == 0) return &sPasswdEntry;
    if (configuredResult < 0) return NULL;
    return getpwuidFallback(uid);
}

struct passwd *getpwnamIntercept(const char *name) {
    struct passwd result;
    int configuredResult = termuxExec_getConfiguredPasswdEntry(0, name, &result);
    if (configuredResult == 0) return &sPasswdEntry;
    if (configuredResult < 0) return NULL;
    return getpwnamFallback(name);
}

static struct group *getgrgidFallback(gid_t gid) {
    static struct group *(*function)(gid_t);
    if (function == NULL) function = dlsym(RTLD_NEXT, "getgrgid");
    if (function == NULL) {
        errno = ENOSYS;
        return NULL;
    }
    return function(gid);
}

static struct group *getgrnamFallback(const char *name) {
    static struct group *(*function)(const char *);
    if (function == NULL) function = dlsym(RTLD_NEXT, "getgrnam");
    if (function == NULL) {
        errno = ENOSYS;
        return NULL;
    }
    return function(name);
}

static char *nextGroupField(char **cursor) {
    char *field = *cursor;
    char *separator = strchr(field, ':');
    if (separator != NULL) {
        *separator = '\0';
        *cursor = separator + 1;
    } else {
        *cursor = field + strlen(field);
    }
    return field;
}

int termuxExec_getConfiguredGroupEntry(gid_t gid, const char *name, struct group *result) {
    if (name == NULL && result == NULL) {
        errno = EINVAL;
        return -1;
    }

    const char *groupFilePath = getenv(ENV__TERMUX_EXEC__GROUP_FILE);
    if (groupFilePath == NULL || strlen(groupFilePath) < 1)
        groupFilePath = TERMUX_EXEC__GROUP_FILE_PATH;

    FILE *file = fopen(groupFilePath, "r");
    if (file == NULL) {
        if (errno == ENOENT) {
            errno = 0;
            return 1;
        }
        return -1;
    }

    while (fgets(sGroupLine, sizeof(sGroupLine), file) != NULL) {
        sGroupLine[strcspn(sGroupLine, "\r\n")] = '\0';
        if (sGroupLine[0] == '\0' || sGroupLine[0] == '#') continue;

        char *cursor = sGroupLine;
        char *entryName = nextGroupField(&cursor);
        (void) nextGroupField(&cursor); // password
        char *gidString = nextGroupField(&cursor);
        (void) nextGroupField(&cursor); // members
        char *end = NULL;
        unsigned long entryGid = strtoul(gidString, &end, 10);
        if (end == gidString || *end != '\0' || entryGid > UINT_MAX) continue;

        if ((name != NULL && strcmp(name, entryName) != 0) ||
            (name == NULL && (gid_t) entryGid != gid)) continue;

        sGroupEntry.gr_name = entryName;
        sGroupEntry.gr_passwd = (char *) "x";
        sGroupEntry.gr_gid = (gid_t) entryGid;
        sGroupEntry.gr_mem = sGroupMembers;
        if (result != NULL) *result = sGroupEntry;
        fclose(file);
        return 0;
    }

    int savedErrno = errno;
    fclose(file);
    errno = savedErrno;
    if (errno == 0) return 1;
    return -1;
}

struct group *getgrgidIntercept(gid_t gid) {
    struct group result;
    int configuredResult = termuxExec_getConfiguredGroupEntry(gid, NULL, &result);
    if (configuredResult == 0) return &sGroupEntry;
    if (configuredResult < 0) return NULL;
    return getgrgidFallback(gid);
}

struct group *getgrnamIntercept(const char *name) {
    struct group result;
    int configuredResult = termuxExec_getConfiguredGroupEntry(0, name, &result);
    if (configuredResult == 0) return &sGroupEntry;
    if (configuredResult < 0) return NULL;
    return getgrnamFallback(name);
}
