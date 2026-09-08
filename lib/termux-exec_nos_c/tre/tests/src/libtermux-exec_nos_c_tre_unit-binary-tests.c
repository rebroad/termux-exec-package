#define _GNU_SOURCE
#include <assert.h>
#include <elf.h>
#include <errno.h>
#include <fcntl.h>
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

    logDebug(LOG_TAG, "runTests(end)");

}

static void test__getConfiguredHostname() {
    logVerbose(LOG_TAG, "test__getConfiguredHostname()");

    char template[PATH_MAX];
#ifdef __ANDROID__
    const char *tmpDir = getenv("TMPDIR");
    if (tmpDir == NULL || strlen(tmpDir) < 1) tmpDir = TERMUX__PREFIX "/tmp";
#else
    const char *tmpDir = "/var/tmp";
#endif
    int__AEqual(0, mkdir(tmpDir, 0700) == 0 || errno == EEXIST ? 0 : -1);
    snprintf(template, sizeof(template), "%s/termux-exec-hostname-test.XXXXXX", tmpDir) < 0 ? abort() : (void)0;
    int fd = mkstemp(template);
    state__ATrue(fd >= 0);

    const char *hostname = "termux\n";
    ssize_t bytesWritten = write(fd, hostname, strlen(hostname));
    int__AEqual((int) strlen(hostname), (int) bytesWritten);
    int__AEqual(0, close(fd));

    int__AEqual(0, setenv(ENV__TERMUX_EXEC__HOSTNAME_FILE, template, 1));

    char buffer[HOST_NAME_MAX + 1];
    int__AEqual(0, termuxExec_getConfiguredHostname(buffer, sizeof(buffer)));
    string__AEqual("termux", buffer);

    memset(buffer, 0, sizeof(buffer));
    int__AEqual(0, gethostnameIntercept(buffer, sizeof(buffer)));
    string__AEqual("termux", buffer);

    char smallBuffer[5];
    errno = 0;
    int__AEqual(-1, termuxExec_getConfiguredHostname(smallBuffer, sizeof(smallBuffer)));
    int__AEqual(ENAMETOOLONG, errno);

    int__AEqual(0, unlink(template));
    int__AEqual(0, unsetenv(ENV__TERMUX_EXEC__HOSTNAME_FILE));
    errno = 0;
}
