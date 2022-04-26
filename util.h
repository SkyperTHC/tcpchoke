
#ifndef __CK_UTIL_H__
#define __CK_UTIL_H__ 1

#define TV_TO_USEC(tv)       ((uint64_t)(tv)->tv_sec * 1000000 + (tv)->tv_usec)
#define TV_TO_MSEC(tv)       ((uint64_t)(tv)->tv_sec * 1000 + (tv)->tv_usec/1000)
#define USEC_TO_SEC(usec)    ((usec) / 1000000)
#define USEC_TO_MSEC(usec)   ((usec) / 1000)


uint64_t get_usec(void);
void format_bps(char *dst, size_t size, int64_t bytes, const char *suffix);
int fd_net_bind(int fd, uint32_t ip, uint16_t port);
int fd_net_listen(int fd, uint16_t port);
int fd_new_socket(int type);
int fd_net_connect(int fd, uint32_t ip, uint16_t port);
int fd_net_accept(int listen_fd);
void init_defaults(void);
void init_vars(int argc, char *argv[]);

#endif /* !__CK_UTIL_H__ */