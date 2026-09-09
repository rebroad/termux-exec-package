#define _GNU_SOURCE
#include <stdarg.h>
#include <stdbool.h>
#include <unistd.h>

#include <termux/termux_exec__nos__c/v1/termux/api/termux_exec/service/ld_preload/direct/exec/ExecIntercept.h>
#include <termux/termux_exec__nos__c/v1/termux/api/termux_exec/service/ld_preload/direct/exec/ExecVariantsIntercept.h>
#include <termux/termux_exec__nos__c/v1/termux/api/termux_exec/service/ld_preload/TermuxExecLDPreload.h>
#include <termux/termux_exec__nos__c/v1/termux/os/process/termux_exec/TermuxExecProcess.h>

#undef getpwuid
#undef getpwnam
#undef getpwuid_r
#undef getpwnam_r
#undef getgrgid
#undef getgrnam
#undef getgrgid_r
#undef getgrnam_r

/**
 * This file defines functions intercepted by `libtermux-exec-linker-ld-preload.so` using `$LD_PRELOAD`.
 *
 * All exported functions must explicitly enable `default` visibility
 * with `__attribute__((visibility("default")))` as `libtermux-exec-linker-ld-preload.so`
 * is compiled with `-fvisibility=hidden` so that no other internal
 * functions are exported.
 *
 * You can check exported symbols for dynamic linking after building with:
 * `nm --demangle --dynamic --defined-only --extern-only /home/builder/.termux-build/termux-exec/src/build/output/usr/lib/libtermux-exec-linker-ld-preload.so`.
 */

#define LIBTERMUX_EXEC_LINKER_LD_PRELOAD__VERSION_NAME TERMUX_EXEC_PKG__VERSION
#define LIBTERMUX_EXEC_LINKER_LD_PRELOAD__VERSION_STRING "libtermux-exec-linker-ld-preload version=" TERMUX_EXEC_PKG__VERSION " org=" TERMUX__REPOS_HOST_ORG_NAME " project=termux-exec-package"



void termuxExec_linkerLdPreload_initProcess() {
    termuxExec_process_initProcess(LIBTERMUX_EXEC_LINKER_LD_PRELOAD__VERSION_STRING, NULL);
}



__attribute__((visibility("default")))
int execl(const char *name, const char *arg, ...) {
    termuxExec_linkerLdPreload_initProcess();

    va_list ap;
    va_start(ap, arg);
    int result = execlIntercept(true, ExecL, name, arg, ap);
    va_end(ap);
    return result;
}

__attribute__((visibility("default")))
int execlp(const char *name, const char *arg, ...) {
    termuxExec_linkerLdPreload_initProcess();

    va_list ap;
    va_start(ap, arg);
    int result = execlIntercept(true, ExecLP, name, arg, ap);
    va_end(ap);
    return result;
}

__attribute__((visibility("default")))
int execle(const char *name, const char *arg, ...) {
    termuxExec_linkerLdPreload_initProcess();

    va_list ap;
    va_start(ap, arg);
    int result = execlIntercept(true, ExecLE, name, arg, ap);
    va_end(ap);
    return result;
}

__attribute__((visibility("default")))
int execv(const char *name, char *const *argv) {
    termuxExec_linkerLdPreload_initProcess();

    return execvIntercept(true, name, argv);
}

__attribute__((visibility("default")))
int execvp(const char *name, char *const *argv) {
    termuxExec_linkerLdPreload_initProcess();

    return execvpIntercept(true, name, argv);
}

__attribute__((visibility("default")))
int execvpe(const char *name, char *const *argv, char *const *envp) {
    termuxExec_linkerLdPreload_initProcess();

    return execvpeIntercept(true, name, argv, envp);
}

__attribute__((visibility("default")))
int fexecve(int fd, char *const *argv, char *const *envp) {
    termuxExec_linkerLdPreload_initProcess();

    return fexecveIntercept(true, fd, argv, envp);
}

__attribute__((visibility("default")))
int execve(const char *name, char *const argv[], char *const envp[]) {
    termuxExec_linkerLdPreload_initProcess();

    return execveIntercept(true, name, argv, envp);
}

__attribute__((visibility("default")))
int gethostname(char *name, size_t len) {
    termuxExec_linkerLdPreload_initProcess();

    return gethostnameIntercept(name, len);
}

__attribute__((visibility("default")))
struct passwd *getpwuid(uid_t uid) {
    termuxExec_linkerLdPreload_initProcess();
    return getpwuidIntercept(uid);
}

__attribute__((visibility("default")))
struct passwd *getpwnam(const char *name) {
    termuxExec_linkerLdPreload_initProcess();
    return getpwnamIntercept(name);
}

__attribute__((visibility("default")))
int getpwuid_r(uid_t uid, struct passwd *result, char *buffer, size_t bufferSize,
               struct passwd **resultPointer) {
    termuxExec_linkerLdPreload_initProcess();
    return getpwuidRIntercept(uid, result, buffer, bufferSize, resultPointer);
}

__attribute__((visibility("default")))
int getpwnam_r(const char *name, struct passwd *result, char *buffer, size_t bufferSize,
               struct passwd **resultPointer) {
    termuxExec_linkerLdPreload_initProcess();
    return getpwnamRIntercept(name, result, buffer, bufferSize, resultPointer);
}

__attribute__((visibility("default")))
struct group *getgrgid(gid_t gid) {
    termuxExec_linkerLdPreload_initProcess();
    return getgrgidIntercept(gid);
}

__attribute__((visibility("default")))
struct group *getgrnam(const char *name) {
    termuxExec_linkerLdPreload_initProcess();
    return getgrnamIntercept(name);
}

__attribute__((visibility("default")))
int getgrgid_r(gid_t gid, struct group *result, char *buffer, size_t bufferSize,
               struct group **resultPointer) {
    termuxExec_linkerLdPreload_initProcess();
    return getgrgidRIntercept(gid, result, buffer, bufferSize, resultPointer);
}

__attribute__((visibility("default")))
int getgrnam_r(const char *name, struct group *result, char *buffer, size_t bufferSize,
               struct group **resultPointer) {
    termuxExec_linkerLdPreload_initProcess();
    return getgrnamRIntercept(name, result, buffer, bufferSize, resultPointer);
}

__attribute__((visibility("default"))) int utmpname(const char *path) { return utmpnameIntercept(path); }
__attribute__((visibility("default"))) void setutent(void) { setutentIntercept(); }
__attribute__((visibility("default"))) struct utmp *getutent(void) { return getutentIntercept(); }
__attribute__((visibility("default"))) struct utmp *pututline(const struct utmp *entry) { return pututlineIntercept(entry); }
__attribute__((visibility("default"))) void endutent(void) { endutentIntercept(); }
