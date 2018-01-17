
#if defined(WIN32)
  #include "tinylib/windows/net/tcp_connection.h"
#elif defined(__linux__)
  #include "tinylib/linux/net/tcp_connection.h"
#endif
