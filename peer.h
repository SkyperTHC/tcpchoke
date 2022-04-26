
#ifndef __CK_PEER_H__
#define __CK_PEER_H__ 1

struct peer
{
	int fd;
	int is_connected;
	uint8_t buf[BUF_SIZE];
	ssize_t len;     // data in buffer
	ssize_t pos;     // bytes written (= 0..len)
	uint64_t bytes_read;
	uint64_t usec_first_read;
	uint64_t bps;
	struct bufferevent *bev;
	struct event *ev;
};

struct peer_mgr
{
	int n;
	struct peer **peers;
};

void pm_init(int max_sockets);
void peer_read_stats(struct peer *p, ssize_t n);
void peer_write_stats(struct peer *p, ssize_t n);
void peer_free(struct peer *p);
struct peer *peer_new(int sox);
struct peer *peer_accept(int ls);

#endif /* !__CK_PEER_H__ */
