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
#include <dirent.h>

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
#define TERMUX_EXEC__UTMP_FILE_PATH TERMUX__PREFIX "/var/run/utmp"



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
static _Thread_local FILE *sPasswdFile;
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

void setpwentIntercept(void) {
    if (sPasswdFile != NULL) fclose(sPasswdFile);
    const char *path = getenv(ENV__TERMUX_EXEC__PASSWD_FILE);
    if (path == NULL || path[0] == '\0') path = TERMUX_EXEC__PASSWD_FILE_PATH;
    sPasswdFile = fopen(path, "r");
}

struct passwd *getpwentIntercept(void) {
    if (sPasswdFile == NULL) setpwentIntercept();
    if (sPasswdFile == NULL) return NULL;

    while (fgets(sPasswdLine, sizeof(sPasswdLine), sPasswdFile) != NULL) {
        sPasswdLine[strcspn(sPasswdLine, "\r\n")] = '\0';
        if (sPasswdLine[0] == '\0' || sPasswdLine[0] == '#') continue;
        char *cursor = sPasswdLine;
        char *name = nextPasswdField(&cursor);
        char *password = nextPasswdField(&cursor);
        char *uidString = nextPasswdField(&cursor);
        char *gidString = nextPasswdField(&cursor);
        char *gecos = nextPasswdField(&cursor);
        char *home = nextPasswdField(&cursor);
        char *shell = cursor;
        char *end = NULL;
        unsigned long uid = strtoul(uidString, &end, 10);
        if (end == uidString || *end != '\0' || uid > UINT_MAX) continue;
        end = NULL;
        unsigned long gid = strtoul(gidString, &end, 10);
        if (end == gidString || *end != '\0' || gid > UINT_MAX) continue;
        sPasswdEntry.pw_name = name;
        sPasswdEntry.pw_passwd = password;
        sPasswdEntry.pw_uid = (uid_t) uid;
        sPasswdEntry.pw_gid = (gid_t) gid;
        sPasswdEntry.pw_gecos = gecos;
        sPasswdEntry.pw_dir = home;
        sPasswdEntry.pw_shell = shell;
        return &sPasswdEntry;
    }
    return NULL;
}

void endpwentIntercept(void) {
    if (sPasswdFile != NULL) {
        fclose(sPasswdFile);
        sPasswdFile = NULL;
    }
}

static int copyPasswdEntry(const struct passwd *source, struct passwd *result, char *buffer,
                           size_t bufferSize, struct passwd **resultPointer) {
    size_t needed = strlen(source->pw_name) + 1 + strlen(source->pw_passwd) + 1 +
                    strlen(source->pw_gecos) + 1 + strlen(source->pw_dir) + 1 +
                    strlen(source->pw_shell) + 1;
    if (needed > bufferSize) {
        *resultPointer = NULL;
        return ERANGE;
    }

    char *cursor = buffer;
#define COPY_PASSWD_FIELD(field) \
    do { \
        size_t fieldSize = strlen(source->field) + 1; \
        memcpy(cursor, source->field, fieldSize); \
        result->field = cursor; \
        cursor += fieldSize; \
    } while (0)
    COPY_PASSWD_FIELD(pw_name);
    COPY_PASSWD_FIELD(pw_passwd);
    result->pw_uid = source->pw_uid;
    result->pw_gid = source->pw_gid;
    COPY_PASSWD_FIELD(pw_gecos);
    COPY_PASSWD_FIELD(pw_dir);
    COPY_PASSWD_FIELD(pw_shell);
#undef COPY_PASSWD_FIELD
    *resultPointer = result;
    return 0;
}

static int getpwuidRFallback(uid_t uid, struct passwd *result, char *buffer, size_t bufferSize,
                             struct passwd **resultPointer) {
    static int (*function)(uid_t, struct passwd *, char *, size_t, struct passwd **);
    if (function == NULL) function = dlsym(RTLD_NEXT, "getpwuid_r");
    if (function == NULL) {
        *resultPointer = NULL;
        return ENOSYS;
    }
    return function(uid, result, buffer, bufferSize, resultPointer);
}

static int getpwnamRFallback(const char *name, struct passwd *result, char *buffer, size_t bufferSize,
                             struct passwd **resultPointer) {
    static int (*function)(const char *, struct passwd *, char *, size_t, struct passwd **);
    if (function == NULL) function = dlsym(RTLD_NEXT, "getpwnam_r");
    if (function == NULL) {
        *resultPointer = NULL;
        return ENOSYS;
    }
    return function(name, result, buffer, bufferSize, resultPointer);
}

int getpwuidRIntercept(uid_t uid, struct passwd *result, char *buffer, size_t bufferSize,
                       struct passwd **resultPointer) {
    struct passwd configured;
    int configuredResult = termuxExec_getConfiguredPasswdEntry(uid, NULL, &configured);
    if (configuredResult == 0)
        return copyPasswdEntry(&configured, result, buffer, bufferSize, resultPointer);
    if (configuredResult < 0) {
        *resultPointer = NULL;
        return errno;
    }
    return getpwuidRFallback(uid, result, buffer, bufferSize, resultPointer);
}

int getpwnamRIntercept(const char *name, struct passwd *result, char *buffer, size_t bufferSize,
                       struct passwd **resultPointer) {
    struct passwd configured;
    int configuredResult = termuxExec_getConfiguredPasswdEntry(0, name, &configured);
    if (configuredResult == 0)
        return copyPasswdEntry(&configured, result, buffer, bufferSize, resultPointer);
    if (configuredResult < 0) {
        *resultPointer = NULL;
        return errno;
    }
    return getpwnamRFallback(name, result, buffer, bufferSize, resultPointer);
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

static int copyGroupEntry(const struct group *source, struct group *result, char *buffer,
                          size_t bufferSize, struct group **resultPointer) {
    size_t needed = strlen(source->gr_name) + 1 + strlen(source->gr_passwd) + 1;
    size_t memberCount = 0;
    while (source->gr_mem != NULL && source->gr_mem[memberCount] != NULL) {
        needed += strlen(source->gr_mem[memberCount]) + 1;
        memberCount++;
    }
    needed += (memberCount + 1) * sizeof(char *);
    if (needed > bufferSize) {
        *resultPointer = NULL;
        return ERANGE;
    }

    char *cursor = buffer;
    size_t stringOffset = (memberCount + 1) * sizeof(char *);
    char **members = (char **) cursor;
    cursor += stringOffset;
    size_t fieldSize = strlen(source->gr_name) + 1;
    memcpy(cursor, source->gr_name, fieldSize);
    result->gr_name = cursor;
    cursor += fieldSize;
    fieldSize = strlen(source->gr_passwd) + 1;
    memcpy(cursor, source->gr_passwd, fieldSize);
    result->gr_passwd = cursor;
    cursor += fieldSize;
    result->gr_gid = source->gr_gid;
    for (size_t i = 0; i < memberCount; i++) {
        fieldSize = strlen(source->gr_mem[i]) + 1;
        memcpy(cursor, source->gr_mem[i], fieldSize);
        members[i] = cursor;
        cursor += fieldSize;
    }
    members[memberCount] = NULL;
    result->gr_mem = members;
    *resultPointer = result;
    return 0;
}

static int getgrgidRFallback(gid_t gid, struct group *result, char *buffer, size_t bufferSize,
                             struct group **resultPointer) {
    static int (*function)(gid_t, struct group *, char *, size_t, struct group **);
    if (function == NULL) function = dlsym(RTLD_NEXT, "getgrgid_r");
    if (function == NULL) {
        *resultPointer = NULL;
        return ENOSYS;
    }
    return function(gid, result, buffer, bufferSize, resultPointer);
}

static int getgrnamRFallback(const char *name, struct group *result, char *buffer, size_t bufferSize,
                             struct group **resultPointer) {
    static int (*function)(const char *, struct group *, char *, size_t, struct group **);
    if (function == NULL) function = dlsym(RTLD_NEXT, "getgrnam_r");
    if (function == NULL) {
        *resultPointer = NULL;
        return ENOSYS;
    }
    return function(name, result, buffer, bufferSize, resultPointer);
}

int getgrgidRIntercept(gid_t gid, struct group *result, char *buffer, size_t bufferSize,
                       struct group **resultPointer) {
    struct group configured;
    int configuredResult = termuxExec_getConfiguredGroupEntry(gid, NULL, &configured);
    if (configuredResult == 0)
        return copyGroupEntry(&configured, result, buffer, bufferSize, resultPointer);
    if (configuredResult < 0) {
        *resultPointer = NULL;
        return errno;
    }
    return getgrgidRFallback(gid, result, buffer, bufferSize, resultPointer);
}

int getgrnamRIntercept(const char *name, struct group *result, char *buffer, size_t bufferSize,
                       struct group **resultPointer) {
    struct group configured;
    int configuredResult = termuxExec_getConfiguredGroupEntry(0, name, &configured);
    if (configuredResult == 0)
        return copyGroupEntry(&configured, result, buffer, bufferSize, resultPointer);
    if (configuredResult < 0) {
        *resultPointer = NULL;
        return errno;
    }
    return getgrnamRFallback(name, result, buffer, bufferSize, resultPointer);
}

static _Thread_local FILE *sUtmpFile;
static _Thread_local char sUtmpFilePath[PATH_MAX];
static _Thread_local struct utmp sUtmpEntry;

static int utmpProcessIsShell(const char *name) {
    return strcmp(name, "bash") == 0 || strcmp(name, "sh") == 0 || strcmp(name, "zsh") == 0 ||
           strcmp(name, "fish") == 0 || strcmp(name, "login") == 0;
}

static int readProcessUid(const char *path, uid_t *uid) {
    FILE *file = fopen(path, "r");
    if (file == NULL) return -1;
    char line[256];
    int result = -1;
    while (fgets(line, sizeof(line), file) != NULL) {
        unsigned long value;
        if (sscanf(line, "Uid:\t%lu", &value) == 1 && value <= UINT_MAX) {
            *uid = (uid_t) value;
            result = 0;
            break;
        }
    }
    fclose(file);
    return result;
}

static void refreshUtmpFile(const char *path) {
    struct utmp previousRecords[256];
    size_t previousCount = 0;
    FILE *previousFile = fopen(path, "rb");
    if (previousFile != NULL) {
        while (previousCount < sizeof(previousRecords) / sizeof(previousRecords[0]) &&
               fread(&previousRecords[previousCount], sizeof(previousRecords[previousCount]), 1, previousFile) == 1) {
            previousCount++;
        }
        fclose(previousFile);
    }

    char temporaryPath[PATH_MAX];
    if (snprintf(temporaryPath, sizeof(temporaryPath), "%s.termux-exec.%ld", path, (long) getpid()) < 0 ||
        strlen(temporaryPath) >= sizeof(temporaryPath)) return;
    FILE *file = fopen(temporaryPath, "wb");
    if (file == NULL) return;

    DIR *directory = opendir("/proc");
    if (directory != NULL) {
        struct dirent *entry;
        while ((entry = readdir(directory)) != NULL) {
            char *end = NULL;
            unsigned long pidValue = strtoul(entry->d_name, &end, 10);
            if (end == entry->d_name || *end != '\0' || pidValue > INT_MAX) continue;

            char commPath[PATH_MAX];
            char fdPath[PATH_MAX];
            char linkPath[PATH_MAX];
            char command[64];
            if (snprintf(commPath, sizeof(commPath), "/proc/%s/comm", entry->d_name) < 0 ||
                snprintf(fdPath, sizeof(fdPath), "/proc/%s/fd/0", entry->d_name) < 0) continue;
            FILE *commFile = fopen(commPath, "r");
            if (commFile == NULL || fgets(command, sizeof(command), commFile) == NULL) {
                if (commFile != NULL) fclose(commFile);
                continue;
            }
            fclose(commFile);
            command[strcspn(command, "\r\n")] = '\0';
            if (!utmpProcessIsShell(command)) continue;
            ssize_t linkLength = readlink(fdPath, linkPath, sizeof(linkPath) - 1);
            if (linkLength <= 9 || strncmp(linkPath, "/dev/pts/", 9) != 0) continue;
            linkPath[linkLength] = '\0';

            char uidPath[PATH_MAX];
            uid_t uid;
            if (snprintf(uidPath, sizeof(uidPath), "/proc/%s/status", entry->d_name) < 0 ||
                readProcessUid(uidPath, &uid) != 0) continue;
            struct passwd *passwd = getpwuidIntercept(uid);
            if (passwd == NULL) continue;

            struct utmp record = {0};
            record.ut_type = USER_PROCESS;
            record.ut_pid = (pid_t) pidValue;
            snprintf(record.ut_line, sizeof(record.ut_line), "%s", linkPath + 5);
            snprintf(record.ut_id, sizeof(record.ut_id), "%s", linkPath + 9);
            snprintf(record.ut_user, sizeof(record.ut_user), "%s", passwd->pw_name);
            record.ut_time = time(NULL);
            for (size_t index = 0; index < previousCount; index++) {
                if (strncmp(previousRecords[index].ut_line, record.ut_line, sizeof(record.ut_line)) == 0 &&
                    previousRecords[index].ut_type == USER_PROCESS) {
                    record.ut_time = previousRecords[index].ut_time;
                    break;
                }
            }
            if (fwrite(&record, sizeof(record), 1, file) != 1) break;
        }
        closedir(directory);
    }
    if (fclose(file) == 0) rename(temporaryPath, path);
    else unlink(temporaryPath);
}

static const char *utmpFilePath(void) {
    const char *path = getenv("TERMUX_EXEC__UTMP_FILE");
    return path == NULL || path[0] == '\0' ? TERMUX_EXEC__UTMP_FILE_PATH : path;
}

int utmpnameIntercept(const char *path) {
    if (path == NULL || path[0] == '\0') {
        errno = EINVAL;
        return -1;
    }
    if (strlen(path) >= sizeof(sUtmpFilePath)) {
        errno = ENAMETOOLONG;
        return -1;
    }
    if (sUtmpFile != NULL) {
        fclose(sUtmpFile);
        sUtmpFile = NULL;
    }
    strcpy(sUtmpFilePath, path);
    return 0;
}

void setutentIntercept(void) {
    if (sUtmpFile != NULL) fclose(sUtmpFile);
    const char *path = sUtmpFilePath[0] == '\0' ? utmpFilePath() : sUtmpFilePath;
    refreshUtmpFile(path);
    sUtmpFile = fopen(path, "rb+");
    if (sUtmpFile == NULL && errno == ENOENT) sUtmpFile = fopen(path, "rb");
}

struct utmp *getutentIntercept(void) {
    if (sUtmpFile == NULL) setutentIntercept();
    if (sUtmpFile == NULL) return NULL;
    return fread(&sUtmpEntry, sizeof(sUtmpEntry), 1, sUtmpFile) == 1 ? &sUtmpEntry : NULL;
}

struct utmp *pututlineIntercept(const struct utmp *entry) {
    if (entry == NULL) {
        errno = EINVAL;
        return NULL;
    }
    if (sUtmpFile == NULL) setutentIntercept();
    if (sUtmpFile == NULL || fseek(sUtmpFile, 0, SEEK_END) != 0 ||
        fwrite(entry, sizeof(*entry), 1, sUtmpFile) != 1 || fflush(sUtmpFile) != 0)
        return NULL;
    sUtmpEntry = *entry;
    return &sUtmpEntry;
}

void endutentIntercept(void) {
    if (sUtmpFile != NULL) {
        fclose(sUtmpFile);
        sUtmpFile = NULL;
    }
}
