
#if defined(WIN32)
  #include "tinylib/windows/net/loop.h"
#elif defined(__linux__)
  #include "tinylib/linux/net/loop.h"
#endif
