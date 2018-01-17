
#if defined(WIN32)
  #include "tinylib/windows/net/tcp_client.h"
#elif defined(__linux__)
  #include "tinylib/linux/net/tcp_client.h"
#endif
