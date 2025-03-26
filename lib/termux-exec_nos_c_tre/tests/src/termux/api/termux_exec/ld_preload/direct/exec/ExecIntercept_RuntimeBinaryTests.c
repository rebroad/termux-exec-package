#include <termux/termux_core__nos__c/v1/android/shell/command/environment/AndroidShellEnvironment.h>
#include <termux/termux_core__nos__c/v1/unix/file/UnixFileUtils.h>

#include <termux/termux_exec__nos__c/v1/termux/api/termux_exec/ld_preload/direct/exec/ExecVariantsIntercept.h>



void test__execIntercept();



void ExecIntercept_runTests() {
    logVerbose(LOG_TAG, "ExecIntercept_runTests()");

    test__execIntercept();

    int__AEqual(0, errno);
}





#if defined __ANDROID__ && __ANDROID_API__ >= 28
#define FEXECVE_SUPPORTED 1
#endif

#if defined FEXECVE_SUPPORTED
#define FEXECVE_CALL_IMPL()                                                                        \
int fexecveCall(int fd, char *const *argv, char *const *envp) {                                    \
    return fexecve(fd, argv, envp);                                                                \
}
#else
#define FEXECVE_SUPPORTED 0
#define FEXECVE_CALL_IMPL()                                                                        \
int fexecveCall(int fd, char *const *argv, char *const *envp) {                                    \
    (void)fd; (void)argv; (void)envp;                                                              \
    logStrerror(LOG_TAG, "fexecve not supported on __ANDROID_API__ %d and requires api level >= %d", __ANDROID_API__, 28); \
    return -1;                                                                                     \
}
#endif

FEXECVE_CALL_IMPL()
#undef FEXECVE_CALL_IMPL



#define execWrapper(variant, name, envp, ...)                                                      \
    if (1) {                                                                                       \
    /* Construct argv */                                                                           \
    char *argv[] = {__VA_ARGS__};                                                                  \
                                                                                                   \
    switch (variant) {                                                                             \
        case ExecVE: {                                                                             \
            actualReturnValue = execve(name, argv, envp);                                          \
            break;                                                                                 \
        } case ExecL: {                                                                            \
            actualReturnValue = execl(name, __VA_ARGS__);                                          \
            break;                                                                                 \
        } case ExecLP: {                                                                           \
            actualReturnValue = execlp(name, __VA_ARGS__);                                         \
            break;                                                                                 \
        } case ExecLE: {                                                                           \
            actualReturnValue = execle(name, __VA_ARGS__, envp);                                   \
            break;                                                                                 \
        } case ExecV: {                                                                            \
            actualReturnValue = execv(name, argv);                                                 \
            break;                                                                                 \
        } case ExecVP: {                                                                           \
            actualReturnValue = execvp(name, argv);                                                \
            break;                                                                                 \
        } case ExecVPE: {                                                                          \
            actualReturnValue = execvpe(name, argv, envp);                                         \
            break;                                                                                 \
        } case FExecVE: {                                                                          \
            int fd = open(name, 0);                                                                \
            if (fd == -1) {                                                                        \
                logStrerror(LOG_TAG, "open() call failed");                                        \
                exit(1);                                                                           \
            }                                                                                      \
                                                                                                   \
            actualReturnValue = fexecveCall(fd, argv, envp);                                       \
            close(fd);                                                                             \
            break;                                                                                 \
        } default: {                                                                               \
            logStrerror(LOG_TAG, "Unknown exec() variant %d", variant);                            \
            exit(1);                                                                               \
        }                                                                                          \
    }                                                                                              \
                                                                                                   \
    } else ((void)0)

#define runExecTest(testName,                                                                      \
    expectedReturnValue, expectedErrno,                                                            \
    expectedExitCode, expectedOutputRegex, expectedOutputRegexFlags,                               \
    variant, name, envp, ...)                                                                      \
    if (1) {                                                                                       \
    logVVVerbose(LOG_TAG, "%s_exec_%s()", testName, EXEC_VARIANTS_STR[variant]);                   \
                                                                                                   \
    INIT_FORK_INFO(info);                                                                          \
    info.parentLogTag = LOG_TAG;                                                                   \
    info.childLogTag = LOG_TAG;                                                                    \
    info.onChildFork = initChild;                                                                  \
    int result = forkChild(&info);                                                                 \
    if (result != 0) {                                                                             \
        logError(LOG_TAG, "Unexpected return value for forkChild '%d'", result);                   \
        exit(1);                                                                                   \
    }                                                                                              \
                                                                                                   \
    if (info.isChild) {                                                                            \
        int actualReturnValue;                                                                     \
        execWrapper(variant, name, envp, __VA_ARGS__);                                             \
        /* Do not log failure logs to stdout of parent and log them to original stderr of child,   \
           otherwise they would get captured in `output` of parent and compared with               \
           `output_regex`. */                                                                      \
        if (dup2(info.stderrFd, STDERR_FILENO) == -1) {                                            \
            logStrerror(LOG_TAG, "Failed to restore child stderr");                                \
            exitForkWithError(&info, 1);                                                           \
        }                                                                                          \
        int actualErrno = errno;                                                                   \
        int testFailed = 0;                                                                        \
        if (actualReturnValue != expectedReturnValue) {                                            \
            logError(LOG_TAG, "FAILED: '%s' '%s()' test", testName, EXEC_VARIANTS_STR[variant]);   \
            logError(LOG_TAG, "Expected return_value does not equal actual return_value");         \
            testFailed=1;                                                                          \
        } else if (actualErrno != expectedErrno) {                                                 \
            logError(LOG_TAG, "FAILED: '%s' '%s()' test", testName, EXEC_VARIANTS_STR[variant]);   \
            logError(LOG_TAG, "Expected errno does not equal actual errno");                       \
            testFailed=1;                                                                          \
        }                                                                                          \
                                                                                                   \
        if (testFailed == 1) {                                                                     \
            logError(LOG_TAG, "actual_return_value: '%d'", actualReturnValue);                     \
            logError(LOG_TAG, "expected_return_value: '%d'", expectedReturnValue);                 \
            logError(LOG_TAG, "actual_errno: '%d'", actualErrno);                                  \
            logError(LOG_TAG, "expected_errno: '%d'", expectedErrno);                              \
            exitForkWithError(&info, 100);                                                         \
        } else {                                                                                   \
            exit(0);                                                                               \
        }                                                                                          \
    } else {                                                                                       \
        if (WIFEXITED(info.status)) {                                                              \
            ;                                                                                      \
        } else if (WIFSIGNALED(info.status)) {                                                     \
            logInfo(LOG_TAG, "Killed by signal %d\n", WTERMSIG(info.status));                      \
        } else if (WIFSTOPPED(info.status)) {                                                      \
            logInfo(LOG_TAG, "Stopped by signal %d\n", WSTOPSIG(info.status));                     \
        } else if (WIFCONTINUED(info.status)) {                                                    \
            logInfo(LOG_TAG, "Continued");                                                         \
        } else {                                                                                   \
            logInfo(LOG_TAG, "CANCELLED");                                                         \
            exit(2);                                                                               \
        }                                                                                          \
                                                                                                   \
        int actualExitCode = info.exitCode;                                                        \
        int testFailed = 0;                                                                        \
        int regexMatchResult = 1;                                                                  \
        if (expectedOutputRegex != NULL &&                                                         \
            (regexMatchResult = regexMatch(info.output, expectedOutputRegex, expectedOutputRegexFlags)) != 0) { \
            logError(LOG_TAG, "FAILED: '%s' '%s()' test", testName, EXEC_VARIANTS_STR[variant]);   \
            logError(LOG_TAG, "Expected output_regex does not equal match actual output");         \
            testFailed=1;                                                                          \
        } else if (actualExitCode != expectedExitCode) {                                           \
            logError(LOG_TAG, "FAILED: '%s' '%s()' test", testName, EXEC_VARIANTS_STR[variant]);   \
            logError(LOG_TAG, "Expected exit_code does not equal actual exit_code");               \
            testFailed=1;                                                                          \
        }                                                                                          \
                                                                                                   \
        if (testFailed == 1) {                                                                     \
            logError(LOG_TAG, "actual_exit_code: '%d'", actualExitCode);                           \
            logError(LOG_TAG, "expected_exit_code: '%d'", expectedExitCode);                       \
            logError(LOG_TAG, "actual_output: '%s'", info.output);                                 \
            logError(LOG_TAG, "expected_output_regex: '%s' (%d)", expectedOutputRegex, expectedOutputRegexFlags); \
            if (regexMatchResult != 1) {                                                           \
                logError(LOG_TAG, "regexMatchResult: '%d'", regexMatchResult);                     \
            }                                                                                      \
            exitForkWithError(&info, 100);                                                         \
        } else {                                                                                   \
            /* logDebug(LOG_TAG, "PASSED"); */                                                     \
            free(info.output);                                                                     \
            errno = 0;                                                                             \
        }                                                                                          \
    }                                                                                              \
                                                                                                   \
    } else ((void)0)

#define runAllExecWrappersTest(testName,                                                           \
    expectedReturnValue, expectedErrno, expectedReturnValueP, expectedErrnoP,                      \
    expectedExitCode, expectedOutputRegex, expectedExitCodeP, expectedOutputRegexP, expectedOutputRegexFlags, \
    file, path, envp, ...)                                                                         \
    if (1) {                                                                                       \
                                                                                                   \
    logVVerbose(LOG_TAG, "%s_exec()", testName);                                                   \
                                                                                                   \
    {                                                                                              \
        /* ExecVE */                                                                               \
        runExecTest(testName, expectedReturnValue, expectedErrno,                                  \
            expectedExitCode, expectedOutputRegex, expectedOutputRegexFlags,                       \
            ExecVE, path, envp, path, __VA_ARGS__);                                                \
    }                                                                                              \
                                                                                                   \
                                                                                                   \
    {                                                                                              \
        /* ExecL */                                                                                \
        runExecTest(testName, expectedReturnValue, expectedErrno,                                  \
            expectedExitCode, expectedOutputRegex, expectedOutputRegexFlags,                       \
            ExecL, path, NULL, path, __VA_ARGS__);                                                 \
    }                                                                                              \
    {                                                                                              \
        if (file != NULL) {                                                                        \
            /* ExecLP */                                                                           \
            runExecTest(testName, expectedReturnValueP, expectedErrnoP,                            \
                expectedExitCodeP, expectedOutputRegexP, expectedOutputRegexFlags,                 \
                ExecLP, file, NULL, file, __VA_ARGS__);                                            \
        }                                                                                          \
    }                                                                                              \
    {                                                                                              \
        /* ExecLE */                                                                               \
        runExecTest(testName, expectedReturnValue, expectedErrno,                                  \
            expectedExitCode, expectedOutputRegex, expectedOutputRegexFlags,                       \
            ExecLE, path, envp, path, __VA_ARGS__);                                                \
    }                                                                                              \
                                                                                                   \
                                                                                                   \
    {                                                                                              \
        /* ExecV */                                                                                \
        runExecTest(testName, expectedReturnValue, expectedErrno,                                  \
            expectedExitCode, expectedOutputRegex, expectedOutputRegexFlags,                       \
            ExecV, path, NULL, path, __VA_ARGS__);                                                 \
    }                                                                                              \
    {                                                                                              \
        if (file != NULL) {                                                                        \
            /* ExecVP */                                                                           \
            runExecTest(testName, expectedReturnValueP, expectedErrnoP,                            \
                expectedExitCodeP, expectedOutputRegexP, expectedOutputRegexFlags,                 \
                ExecVP, file, NULL, file, __VA_ARGS__);                                            \
        }                                                                                          \
    }                                                                                              \
    {                                                                                              \
        if (file != NULL) {                                                                        \
            /* ExecVPE */                                                                          \
            runExecTest(testName, expectedReturnValueP, expectedErrnoP,                            \
                expectedExitCodeP, expectedOutputRegexP, expectedOutputRegexFlags,                 \
                ExecVPE, file, envp, file, __VA_ARGS__);                                           \
        }                                                                                          \
    }                                                                                              \
                                                                                                   \
                                                                                                   \
    {                                                                                              \
        if (FEXECVE_SUPPORTED == 1) {                                                              \
            /* FExecVE */                                                                          \
            runExecTest(testName, expectedReturnValue, expectedErrno,                              \
                expectedExitCode, expectedOutputRegex, expectedOutputRegexFlags,                   \
                FExecVE, path, envp, path, __VA_ARGS__);                                           \
        }                                                                                          \
    }                                                                                              \
                                                                                                   \
    } else ((void)0)


#define asprintf_wrapper(strp, fmt, ...)                                                           \
    if (1) {                                                                                       \
        if (asprintf(strp, fmt, __VA_ARGS__) == -1) {                                              \
            errno = ENOMEM;                                                                        \
            logStrerrorDebug(LOG_TAG, "asprintf failed for new '%s'", fmt, __VA_ARGS__);           \
            exit(1);                                                                               \
        }                                                                                          \
    } else ((void)0)




void test__execIntercept__Basic();
void test__execIntercept__Files(const char* termuxExec_tests_testsPath, const char* currentPath, char* envCurrentPath);
void test__execIntercept__PackageManager();

void test__execIntercept() {
    logVerbose(LOG_TAG, "test__execIntercept()");


    char termuxExec_tests_testsPathBuffer[PATH_MAX];
    const char* termuxExec_tests_testsPath;
    int result = getPathFromEnv(LOG_LEVEL__NORMAL, LOG_TAG,
        "tests_path", ENV__TERMUX_EXEC__TESTS__TESTS_PATH,
        true, 0, true, true,
        termuxExec_tests_testsPathBuffer, sizeof(termuxExec_tests_testsPathBuffer));
    if (result < 0) {
        exit(1);
    } else if (result == 1 || strlen(termuxExec_tests_testsPathBuffer) < 1) {
        termuxExec_tests_testsPath = TERMUX_EXEC__TESTS__TESTS_PATH;
    } else {
        termuxExec_tests_testsPath = termuxExec_tests_testsPathBuffer;
    }
    logErrorVVerbose(LOG_TAG, "tests_path: '%s'", termuxExec_tests_testsPath);


    const char* currentPath = getenv(ENV__PATH);
    char* envCurrentPath = NULL;

    if (currentPath == NULL || strlen(currentPath) < 1) {
        envCurrentPath = ENV_PREFIX__PATH;
    } else {
        if (asprintf(&envCurrentPath, "%s%s", ENV_PREFIX__PATH, currentPath) == -1) {
            errno = ENOMEM;
            logStrerrorDebug(LOG_TAG, "asprintf failed for current '%s%s'", ENV_PREFIX__PATH, currentPath);
            exit(1);
        }
    }


    // TODO: Port tests from bionic.
    // - https://cs.android.com/android/_/android/platform/bionic/+/refs/tags/android-14.0.0_r18:tests/unistd_test.cpp;l=1364
    // - https://cs.android.com/android/platform/superproject/+/android-14.0.0_r18:bionic/tests/utils.h;l=200

    test__execIntercept__Basic();
    test__execIntercept__Files(termuxExec_tests_testsPath, currentPath, envCurrentPath);
    test__execIntercept__PackageManager();

    int__AEqual(0, errno);

    // We cannot free this in a test function that sets it as later test cases will use it.
    free(envCurrentPath);
}



void test__execIntercept__Basic() {
    logVVerbose(LOG_TAG, "test__execIntercept__Basic()");

    runAllExecWrappersTest("rootfs",
        -1, EISDIR, -1, EISDIR,
        0, NULL, 0, NULL, 0,
        "../../", TERMUX__ROOTFS, environ,
        NULL);
}

void test__execIntercept__Files(const char* termuxExec_tests_testsPath, const char* currentPath, char* envCurrentPath) {
    logVVerbose(LOG_TAG, "test__execIntercept__Files()");



    char* termuxExec__execTestFilesPath = NULL;
    asprintf_wrapper(&termuxExec__execTestFilesPath, "%s/%s",
        termuxExec_tests_testsPath, "lib/termux-exec_nos_c_tre/scripts/termux/api/termux_exec/ld_preload/direct/exec/files");


    // execlp(), execvp() and execvpe() search for file to be executed in $PATH,
    // so set it with test exec files directory appended at end.
    char* envNewPath = NULL;

    if (currentPath == NULL || strlen(currentPath) < 1) {
        if (asprintf(&envNewPath, "%s%s", ENV_PREFIX__PATH, termuxExec__execTestFilesPath) == -1) {
            errno = ENOMEM;
            logStrerrorDebug(LOG_TAG, "asprintf failed for new '%s%s'", ENV_PREFIX__PATH, termuxExec__execTestFilesPath);
            exit(1);
        }
    } else {
        if (asprintf(&envNewPath, "%s%s:%s", ENV_PREFIX__PATH, currentPath, termuxExec__execTestFilesPath) == -1) {
            errno = ENOMEM;
            logStrerrorDebug(LOG_TAG, "asprintf failed for new '%s%s:%s'", ENV_PREFIX__PATH, currentPath, termuxExec__execTestFilesPath);
            exit(1);
        }
    }


    putenv(envNewPath);


    char* testFilePath = NULL;

    int expectedReturnValue = 0;
    int expectedErrno = 0;
    int expectedReturnValueP = 0;
    int expectedErrnoP = 0;

    int expectedExitCode = 0;
    char* expectedOutputRegex = "^goodbye-world$";
    int expectedExitCodeP = 0;
    char* expectedOutputRegexP = "^goodbye-world$";


    // If `argv[0]` length is `>= 128` on Android `< 6`, then commands
    // would normally fail with exit code 1 without any error on stderr,
    // but `termux-exec` will prevent this by returning `-1` from
    // `execveIntercept()` with `ENAMETOOLONG` errno.
    // Check `checkExecArg0BufferOverflow()` function in `ExecIntercept.h`.
    if (android_buildVersionSdk_get() < 23) {
        asprintf_wrapper(&testFilePath, "%s/%s", termuxExec__execTestFilesPath, "print-args-binary");
        runAllExecWrappersTest("print-args-binary",
            -1, ENAMETOOLONG, expectedReturnValueP, expectedErrnoP,
            0, "^$", expectedExitCodeP, expectedOutputRegexP, REG_EXTENDED,
            "print-args-binary", testFilePath, environ,
            "goodbye-world", NULL);
        free(testFilePath);

        asprintf_wrapper(&testFilePath, "%s/%s", termuxExec__execTestFilesPath, "print-args-binary.sym");
        runAllExecWrappersTest("print-args-binary.sym",
            -1, ENAMETOOLONG, expectedReturnValueP, expectedErrnoP,
            0, "^$", expectedExitCodeP, expectedOutputRegexP, REG_EXTENDED,
            "print-args-binary.sym", testFilePath, environ,
            "goodbye-world", NULL);
        free(testFilePath);
    } else {
        asprintf_wrapper(&testFilePath, "%s/%s", termuxExec__execTestFilesPath, "print-args-binary");
        runAllExecWrappersTest("print-args-binary",
            expectedReturnValue, expectedErrno, expectedReturnValueP, expectedErrnoP,
            expectedExitCode, expectedOutputRegex, expectedExitCodeP, expectedOutputRegexP, REG_EXTENDED,
            "print-args-binary", testFilePath, environ,
            "goodbye-world", NULL);
        free(testFilePath);

        asprintf_wrapper(&testFilePath, "%s/%s", termuxExec__execTestFilesPath, "print-args-binary.sym");
        runAllExecWrappersTest("print-args-binary.sym",
            expectedReturnValue, expectedErrno, expectedReturnValueP, expectedErrnoP,
            expectedExitCode, expectedOutputRegex, expectedExitCodeP, expectedOutputRegexP, REG_EXTENDED,
            "print-args-binary.sym", testFilePath, environ,
            "goodbye-world", NULL);
        free(testFilePath);
    }


    asprintf_wrapper(&testFilePath, "%s/%s", termuxExec__execTestFilesPath, "print-args-linux-script.sh");
    runAllExecWrappersTest("print-args-linux-script.sh",
        expectedReturnValue, expectedErrno, expectedReturnValueP, expectedErrnoP,
        expectedExitCode, expectedOutputRegex, expectedExitCodeP, expectedOutputRegexP, REG_EXTENDED,
        "print-args-linux-script.sh", testFilePath, environ,
        "goodbye-world", NULL);
    free(testFilePath);

    asprintf_wrapper(&testFilePath, "%s/%s", termuxExec__execTestFilesPath, "print-args-linux-script.sh.sym");
    runAllExecWrappersTest("print-args-linux-script.sh.sym",
        expectedReturnValue, expectedErrno, expectedReturnValueP, expectedErrnoP,
        expectedExitCode, expectedOutputRegex, expectedExitCodeP, expectedOutputRegexP, REG_EXTENDED,
        "print-args-linux-script.sh.sym", testFilePath, environ,
        "goodbye-world", NULL);
    free(testFilePath);


    asprintf_wrapper(&testFilePath, "%s/%s", termuxExec__execTestFilesPath, "print-args-termux-script.sh");
    runAllExecWrappersTest("print-args-termux-script.sh",
        expectedReturnValue, expectedErrno, expectedReturnValueP, expectedErrnoP,
        expectedExitCode, expectedOutputRegex, expectedExitCodeP, expectedOutputRegexP, REG_EXTENDED,
        "print-args-termux-script.sh", testFilePath, environ,
        "goodbye-world", NULL);
    free(testFilePath);

    asprintf_wrapper(&testFilePath, "%s/%s", termuxExec__execTestFilesPath, "print-args-termux-script.sh.sym");
    runAllExecWrappersTest("print-args-termux-script.sh.sym",
        expectedReturnValue, expectedErrno, expectedReturnValueP, expectedErrnoP,
        expectedExitCode, expectedOutputRegex, expectedExitCodeP, expectedOutputRegexP, REG_EXTENDED,
        "print-args-termux-script.sh.sym", testFilePath, environ,
        "goodbye-world", NULL);
    free(testFilePath);


    putenv(envCurrentPath);


    free(termuxExec__execTestFilesPath);
    free(envNewPath);

}

void test__execIntercept__PackageManager() {
    logVVerbose(LOG_TAG, "test__execIntercept__PackageManager()");

    if (UID == 0) {
        logStrerrorVerbose(LOG_TAG, "Not running 'package-manager' 'exec()' wrapper tests since running as root");
        return;
    }

    char* termuxPackageManager = getenv(ENV__TERMUX_ROOTFS__PACKAGE_MANAGER);
    if (termuxPackageManager == NULL || strlen(termuxPackageManager) < 1) {
        logStrerrorVerbose(LOG_TAG, "Not running 'package-manager' 'exec()' wrapper tests since '%s' environment variable not set",
            ENV__TERMUX_ROOTFS__PACKAGE_MANAGER);
        return;
    }

    char* termuxPackageManagerPath = NULL;
    asprintf_wrapper(&termuxPackageManagerPath, "%s/bin/%s", TERMUX__PREFIX, termuxPackageManager);

    // In case bootstrap was built without a package manager.
    if (access(termuxPackageManagerPath, X_OK) != 0) {
        logStrerrorVerbose(LOG_TAG, "Not running 'package-manager' 'exec()' wrapper tests since failed to access package manager executable path '%s'",
            termuxPackageManagerPath);
        free(termuxPackageManagerPath);
        errno = 0;
        return;
    }



    // apt: `apt x.x.x (<arch>)`
    // pacman: `Pacman vx.x.x` Also can icon and license info
    char* termuxPackageManagerVersionRegex = NULL;
    if (asprintf(&termuxPackageManagerVersionRegex, "^.*%s v?[0-9][.][0-9][.][0-9].*$", termuxPackageManager) == -1) {
        errno = ENOMEM;
        logStrerrorDebug(LOG_TAG, "asprintf failed for new '^.*%s v?[0-9][.][0-9][.][0-9].*$'", termuxPackageManager);
        exit(1);
    }

    runAllExecWrappersTest("package-manager-version",
        0, 0, 0, 0,
        0, termuxPackageManagerVersionRegex, 0, termuxPackageManagerVersionRegex, REG_EXTENDED | REG_ICASE,
        termuxPackageManager, termuxPackageManagerPath, environ,
        "--version", NULL);

    free(termuxPackageManagerVersionRegex);



    free(termuxPackageManagerPath);

}
