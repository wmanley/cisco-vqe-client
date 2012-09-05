#ifndef _STUN_INCLUDES
#define _STUN_INCLUDES

#include <assert.h>
#include <netdb.h>
#include <stdio.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

typedef u_char	ttl_t;
typedef int	fd_t;

#ifndef EXIT_SUCCESS
#define EXIT_SUCCESS 0
#endif

#ifndef EXIT_SUCCESS
#define EXIT_FAILURE 7
#endif

#define FALSE 0
#define TRUE 1


#define MIN(a,b) ((a) < (b) ? (a) : (b))

#define MALLOC_STRUCTURE(a) ((a *)malloc(sizeof(a)))
#define CHECK_AND_FREE(a) if ((a) != NULL) { free((void *)(a)); (a) = NULL;}
#define NUM_ELEMENTS_IN_ARRAY(name) ((sizeof((name))) / (sizeof(*(name))))


#ifndef __cplusplus
#ifndef bool
 typedef unsigned char bool;
 #ifndef false
 #define false FALSE
 #endif
 #ifndef true
 #define true TRUE
 #endif
#endif
#endif

#endif //_STUN_INCLUDES

//#include <limits.h>
//#include <sys/types.h>
//#include <sys/time.h>
//#include <time.h>
//#include <sys/resource.h>
//#include <pwd.h>
//#include <signal.h>
//#include <ctype.h>
//#include <stdio.h>
//#include <stdarg.h>
//#include <memory.h>
//#include <errno.h>
//#include <math.h>
//#include <string.h>
//#include <fcntl.h>
//#include <sys/types.h>
//#include <sys/socket.h>
//#include <sys/stat.h>
//#include <sys/uio.h>
//#include <netinet/in.h>
//#include <unistd.h>
//#include <sys/param.h>
//#include <sys/fcntl.h>
//#include <sys/ioctl.h>
//#include <arpa/inet.h>
//#include <syslog.h>
//#include <stdint.h>
//#include <net/if.h>

 /*************************************************
 * Old stun_includes follow below                 *
 *************************************************/

//#include <errno.h>
//#include <stdlib.h>
//#include <inttypes.h>
//#include <stdint.h>
//#include <unistd.h>
//#include <fcntl.h>
//#include <netinet/in.h>
//#include <sys/types.h>
//#include <sys/socket.h>
//#include <netinet/in.h>
//#include <syslog.h>
//#include <string.h>
//#include <ctype.h>
//#include <netdb.h>
//#include <sys/stat.h>
//#include <sys/time.h>
//#include <sys/param.h>
//#include <stdarg.h>
