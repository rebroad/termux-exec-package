#include <sys/utsname.h>

void TermuxExecLDPreload_runTests(void) {
  logVerbose(LOG_TAG, "TermuxExecLDPreload_runTests()");

#if defined(__aarch64__)
  const char *interceptEnabled = getenv("TERMUX_EXEC__UNAME_INTERCEPT");
  if (interceptEnabled != NULL && strcmp(interceptEnabled, "1") != 0)
    return;

  char configuredHostname[HOST_NAME_MAX + 1];
  if (termuxExec_getConfiguredHostname(configuredHostname,
                                       sizeof(configuredHostname)) != 0)
    return;

  struct utsname result;
  int__AEqual(0, (int)syscall(SYS_uname, &result));
  string__AEqual(configuredHostname, result.nodename);
#endif
}
