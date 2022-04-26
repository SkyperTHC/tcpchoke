
#ifndef __CK_COMMON_H__
#define __CK_COMMON_H__

#define DEBUG 1
#ifdef HAVE_CONFIG_H
# include <config.h>
#endif

#include <sys/types.h>
#include <sys/time.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <sys/wait.h>
#include <sys/epoll.h>

#include <netinet/in.h>
#ifdef HAVE_NETINET_IN_SYSTM_H
# include <netinet/in_systm.h>
#endif
#include <netinet/ip.h>
#include <netinet/tcp.h>
#include <netinet/udp.h>
#include <netinet/ip_icmp.h>
#include <arpa/inet.h>
#include <netdb.h>              // gethostbyname
#include <errno.h>
#ifdef HAVE_UNISTD_H
# include <unistd.h>
#endif
#include <stdio.h>
#include <string.h>
#include <stdarg.h>
#include <stdlib.h>
#include <stdint.h>
#include <ctype.h>
#include <inttypes.h>
#include <signal.h>
#include <fcntl.h>
// LibEvent
#include <event2/event.h>
#include <event2/event_struct.h>
#include <event2/bufferevent.h>
#include <event2/buffer.h>

#define D_RED(a)	"\033[0;31m"a"\033[0m"
#define D_GRE(a)	"\033[0;32m"a"\033[0m"
#define D_YEL(a)	"\033[0;33m"a"\033[0m"
#define D_BLU(a)	"\033[0;34m"a"\033[0m"
#define D_MAG(a)	"\033[0;35m"a"\033[0m"
#define D_BRED(a)	"\033[1;31m"a"\033[0m"
#define D_BGRE(a)	"\033[1;32m"a"\033[0m"
#define D_BYEL(a)	"\033[1;33m"a"\033[0m"
#define D_BBLU(a)	"\033[1;34m"a"\033[0m"
#define D_BMAG(a)	"\033[1;35m"a"\033[0m"
#ifdef DEBUG
# define DEBUGF(a...)   do{ fprintf(stderr, "%s:%d: ", __func__, __LINE__); fprintf(stderr, a); }while(0)
# define DEBUGF_R(a...) do{ fprintf(stderr, "%s:%d: ", __func__, __LINE__); fprintf(stderr, "\033[1;31m"); fprintf(stderr, a); fprintf(stderr, "\033[0m"); }while(0)
# define DEBUGF_G(a...) do{ fprintf(stderr, "%s:%d: ", __func__, __LINE__); fprintf(stderr, "\033[1;32m"); fprintf(stderr, a); fprintf(stderr, "\033[0m"); }while(0)
# define DEBUGF_B(a...) do{ fprintf(stderr, "%s:%d: ", __func__, __LINE__); fprintf(stderr, "\033[1;34m"); fprintf(stderr, a); fprintf(stderr, "\033[0m"); }while(0)
# define DEBUGF_Y(a...) do{ fprintf(stderr, "%s:%d: ", __func__, __LINE__); fprintf(stderr, "\033[1;33m"); fprintf(stderr, a); fprintf(stderr, "\033[0m"); }while(0)
# define DEBUGF_M(a...) do{ fprintf(stderr, "%s:%d: ", __func__, __LINE__); fprintf(stderr, "\033[1;35m"); fprintf(stderr, a); fprintf(stderr, "\033[0m"); }while(0)
# define DEBUGF_C(a...) do{ fprintf(stderr, "%s:%d: ", __func__, __LINE__); fprintf(stderr, "\033[1;36m"); fprintf(stderr, a); fprintf(stderr, "\033[0m"); }while(0)
# define DEBUGF_W(a...) do{ fprintf(stderr, "%s:%d: ", __func__, __LINE__); fprintf(stderr, "\033[1;37m"); fprintf(stderr, a); fprintf(stderr, "\033[0m"); }while(0)
# define DEBUG_SETID(xgs)    gs_did = (xgs)->fd
#else
# define DEBUGF(a...)
# define DEBUGF_R(a...)
# define DEBUGF_G(a...)
# define DEBUGF_B(a...)
# define DEBUGF_Y(a...)
# define DEBUGF_M(a...)
# define DEBUGF_C(a...)
# define DEBUGF_W(a...)
# define DEBUG_SETID(xgs)
#endif

#define int_ntoa(x)     inet_ntoa(*((struct in_addr *)&x))

#ifndef MAX
# define MAX(X, Y) (((X) < (Y)) ? (Y) : (X))
#endif


#ifndef XASSERT
# define XASSERT(expr, a...) do { \
	if (!(expr)) { \
		fprintf(stderr, "%s:%d:%s() ASSERT(%s) ", __FILE__, __LINE__, __func__, #expr); \
		fprintf(stderr, a); \
		fprintf(stderr, " Exiting...\n"); \
		exit(255); \
	} \
} while (0)
#endif

#endif /* !__CK_COMMON_H__ */
