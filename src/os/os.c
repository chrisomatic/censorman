#if OS == OS_WINDOWS
#include "os_core_win32.c"
#else
#include "os_core_linux.c"
#endif

#include "os_helpers.c"
