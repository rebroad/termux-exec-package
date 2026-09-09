#define _GNU_SOURCE
#include <assert.h>
#include <elf.h>
#include <errno.h>
#include <fcntl.h>
#include <grp.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <sys/stat.h>
#include <sys/types.h>

#include <termux/termux_core__nos__c/v1/TermuxCoreLibraryConfig.h>
#include <termux/termux_core__nos__c/v1/data/AssertUtils.h>
#include <termux/termux_core__nos__c/v1/data/DataUtils.h>
#include <termux/termux_core__nos__c/v1/logger/Logger.h>
#include <termux/termux_core__nos__c/v1/termux/file/TermuxFile.h>
#include <termux/termux_core__nos__c/v1/termux/shell/command/environment/TermuxShellEnvironment.h>
#include <termux/termux_core__nos__c/v1/unix/file/UnixFileUtils.h>
#include <termux/termux_core__nos__c/v1/unix/shell/command/environment/UnixShellEnvironment.h>

#include <termux/termux_exec__nos__c/v1/TermuxExecLibraryConfig.h>
#include <termux/termux_exec__nos__c/v1/termux/api/termux_exec/service/ld_preload/TermuxExecLDPreload.h>
#include <termux/termux_exec__nos__c/v1/termux/shell/command/environment/termux_exec/TermuxExecShellEnvironment.h>



static const char* LOG_TAG = "ub-tests";


static void init();
static void initLogger();
static void runTests();



#include "termux/api/termux_exec/service/ld_preload/direct/exec/ExecIntercept_UnitBinaryTests.c"

static void test__getConfiguredHostname();
static void test__getConfiguredPasswd();
static void test__getConfiguredGroup();



__attribute__((visibility("default")))
int main() {
    init();

    logVVerbose(LOG_TAG, "main()");

    runTests();

    return 0;
}



static void init() {
    errno = 0;

    libtermux_core__nos__c__setIsRunningTests(true);
    libtermux_exec__nos__c__setIsRunningTests(true);

    initLogger();
}

static void initLogger() {
    setDefaultLogTagAndPrefix("lib" TERMUX__LNAME "-exec_c");
    setCurrentLogLevel(termuxExec_tests_logLevel_get());
    setLogFormatMode(LOG_FORMAT_MODE__TAG_AND_MESSAGE);
}



void runTests() {

    logDebug(LOG_TAG, "runTests(start)");

    ExecIntercept_runTests();
    test__getConfiguredHostname();
    test__getConfiguredPasswd();
    test__getConfiguredGroup();

    logDebug(LOG_TAG, "runTests(end)");

}

static void test__getConfiguredHostname() {
    logVerbose(LOG_TAG, "test__getConfiguredHostname()");

    char template[PATH_MAX];
#ifdef __ANDROID__
    const char *tmpDir = TERMUX__PREFIX "/tmp";
#else
    const char *tmpDir = "/var/tmp";
#endif
    int__AEqual(0, mkdir(tmpDir, 0700) == 0 || errno == EEXIST ? 0 : -1);
    snprintf(template, sizeof(template), "%s/termux-exec-hostname-test.XXXXXX", tmpDir) < 0 ? abort() : (void)0;
    int fd = mkstemp(template);
    state__ATrue(fd >= 0);

    const char *hostname = "termux-test-host\n";
    ssize_t bytesWritten = write(fd, hostname, strlen(hostname));
    int__AEqual((int) strlen(hostname), (int) bytesWritten);
    int__AEqual(0, close(fd));

    int__AEqual(0, setenv(ENV__TERMUX_EXEC__HOSTNAME_FILE, template, 1));

    char buffer[HOST_NAME_MAX + 1];
    int__AEqual(0, termuxExec_getConfiguredHostname(buffer, sizeof(buffer)));
    string__AEqual("termux-test-host", buffer);

    memset(buffer, 0, sizeof(buffer));
    int__AEqual(0, gethostnameIntercept(buffer, sizeof(buffer)));
    string__AEqual("termux-test-host", buffer);

    char smallBuffer[5];
    errno = 0;
    int__AEqual(-1, termuxExec_getConfiguredHostname(smallBuffer, sizeof(smallBuffer)));
    int__AEqual(ENAMETOOLONG, errno);

    int__AEqual(0, unlink(template));
    int__AEqual(0, unsetenv(ENV__TERMUX_EXEC__HOSTNAME_FILE));
    errno = 0;
}

static void test__getConfiguredPasswd() {
    logVerbose(LOG_TAG, "test__getConfiguredPasswd()");

    char template[PATH_MAX];
#ifdef __ANDROID__
    const char *tmpDir = TERMUX__PREFIX "/tmp";
#else
    const char *tmpDir = "/var/tmp";
#endif
    int__AEqual(0, mkdir(tmpDir, 0700) == 0 || errno == EEXIST ? 0 : -1);
    snprintf(template, sizeof(template), "%s/termux-exec-passwd-test.XXXXXX", tmpDir) < 0 ? abort() : (void)0;
    int fd = mkstemp(template);
    state__ATrue(fd >= 0);

    const char *passwd = "testuser:x:12345:12345:Test User:/data/data/com.termux/files/home:/data/data/com.termux/files/usr/bin/bash\n";
    ssize_t bytesWritten = write(fd, passwd, strlen(passwd));
    int__AEqual((int) strlen(passwd), (int) bytesWritten);
    int__AEqual(0, close(fd));

    int__AEqual(0, setenv(ENV__TERMUX_EXEC__PASSWD_FILE, template, 1));

    struct passwd entry;
    int__AEqual(0, termuxExec_getConfiguredPasswdEntry(12345, NULL, &entry));
    string__AEqual("testuser", entry.pw_name);
    string__AEqual("/data/data/com.termux/files/home", entry.pw_dir);
    int__AEqual(12345, (int) entry.pw_uid);

    struct passwd *lookup = getpwuidIntercept(12345);
    state__ATrue(lookup != NULL);
    string__AEqual("testuser", lookup->pw_name);
    lookup = getpwnamIntercept("testuser");
    state__ATrue(lookup != NULL);
    int__AEqual(12345, (int) lookup->pw_uid);

    char buffer[256];
    struct passwd reentrantEntry;
    struct passwd *reentrantLookup = NULL;
    int__AEqual(0, getpwnamRIntercept("testuser", &reentrantEntry, buffer, sizeof(buffer), &reentrantLookup));
    state__ATrue(reentrantLookup == &reentrantEntry);
    string__AEqual("testuser", reentrantEntry.pw_name);
    int__AEqual(12345, (int) reentrantEntry.pw_uid);

    reentrantLookup = NULL;
    int__AEqual(ERANGE, getpwnamRIntercept("testuser", &reentrantEntry, buffer, 1, &reentrantLookup));
    state__ATrue(reentrantLookup == NULL);

    int__AEqual(0, unlink(template));
    int__AEqual(0, unsetenv(ENV__TERMUX_EXEC__PASSWD_FILE));
    errno = 0;
}

static void test__getConfiguredGroup() {
    logVerbose(LOG_TAG, "test__getConfiguredGroup()");

    char template[PATH_MAX];
#ifdef __ANDROID__
    const char *tmpDir = TERMUX__PREFIX "/tmp";
#else
    const char *tmpDir = "/var/tmp";
#endif
    int__AEqual(0, mkdir(tmpDir, 0700) == 0 || errno == EEXIST ? 0 : -1);
    snprintf(template, sizeof(template), "%s/termux-exec-group-test.XXXXXX", tmpDir) < 0 ? abort() : (void)0;
    int fd = mkstemp(template);
    state__ATrue(fd >= 0);

    const char *group = "testgroup:x:12345:\n";
    ssize_t bytesWritten = write(fd, group, strlen(group));
    int__AEqual((int) strlen(group), (int) bytesWritten);
    int__AEqual(0, close(fd));

    int__AEqual(0, setenv(ENV__TERMUX_EXEC__GROUP_FILE, template, 1));

    struct group entry;
    int__AEqual(0, termuxExec_getConfiguredGroupEntry(12345, NULL, &entry));
    string__AEqual("testgroup", entry.gr_name);
    int__AEqual(12345, (int) entry.gr_gid);

    struct group *lookup = getgrgidIntercept(12345);
    state__ATrue(lookup != NULL);
    string__AEqual("testgroup", lookup->gr_name);
    lookup = getgrnamIntercept("testgroup");
    state__ATrue(lookup != NULL);
    int__AEqual(12345, (int) lookup->gr_gid);

    char buffer[256];
    struct group reentrantEntry;
    struct group *reentrantLookup = NULL;
    int__AEqual(0, getgrnamRIntercept("testgroup", &reentrantEntry, buffer, sizeof(buffer), &reentrantLookup));
    state__ATrue(reentrantLookup == &reentrantEntry);
    string__AEqual("testgroup", reentrantEntry.gr_name);
    int__AEqual(12345, (int) reentrantEntry.gr_gid);

    int__AEqual(0, unlink(template));
    int__AEqual(0, unsetenv(ENV__TERMUX_EXEC__GROUP_FILE));
    errno = 0;
}
