
#if defined(WIN32)
  #include "tinylib/windows/net/udp_peer.h"
#elif defined(__linux__)
  #include "tinylib/linux/net/udp_peer.h"
#endif
