
/*
 *

### Retrieve INFOS
cat /proc/sys/fs/file-max

# Check socket memory allocation
grep TCP /proc/net/sockstat

# check states
netstat -nta | egrep "State|31337"

# Check Recv-Q / Send-Q
ss -nta '( dport = :31337 )'

# Check SOMAX
cat /proc/sys/net/core/somaxconn
cat /proc/sys/net/ipv4/tcp_max_syn_backlog

# - tcp_mem (in 4k pages) is the total memory of all networking applications
# - tcp_rmem/tcp_wmem (in bytes) of read/write buffer _per open socket_
grep . /proc/sys/net/ipv4/tcp*mem
/proc/sys/net/ipv4/tcp_mem:2394 3194    4788
/proc/sys/net/ipv4/tcp_rmem:4096        131072  1635808
/proc/sys/net/ipv4/tcp_wmem:4096        16384   1635808

# Default send/receive buffer in bytes per socket:
# - How soon will socket EAGAIN
# - The smaller the less memory is needed but less troughput.
cat /proc/sys/net/core/rmem_default
cat /proc/sys/net/core/wmem_default

### SETUP
# Disable Kernel cache (free pagecache, dentries and inodes)
free && sync && echo 3 > /proc/sys/vm/drop_caches && free

# Disable all swaps
swapoff -a

# Remove Linux dmesg SYNC FLOOD warning and increasing listen(, backlog) value
sudo sysctl net.ipv4.tcp_syncookies=0
echo 1048576 >/proc/sys/net/core/somaxconn
echo 1048576 >/proc/sys/net/ipv4/tcp_max_syn_backlog

# Reduce FIN TIMEPUT from 60 to 5 seconds
echo 5 >/proc/sys/net/ipv4/tcp_fin_timeout

# Enable TIME-WAIT re-use
echo 2 >/proc/sys/net/ipv4/tcp_tw_reuse

# Disable metrics savings
echo 1 >/proc/sys/net/ipv4/tcp_no_metrics_save

# Decrease orphans - Ophaned TCP connections should be killed fast.
echo 1024 >/proc/sys/net/ipv4/tcp_max_orphans

# Disable connection tracking
sudo iptables -t raw -A PREROUTING -p tcp --dport 31337 -j NOTRACK

# Increase ephemeral port range
sudo sysctl net.ipv4.ip_local_port_range="1024 65535"

# CLIENT setup (not needed on linux):
# Set up IP addresses:
for y in `seq 0 2`; do for x in `seq 2 254`; do ip addr add 127.0.$y.$x dev lo:$((y*256+x)); done done

# Adjust TCP Stack memory
echo 4096 >/proc/sys/net/core/rmem_default
echo 8192 >/proc/sys/net/core/wmem_default

# Max values that can be set with setsockopt:
# cat /proc/sys/net/core/rmem_max
# cat /proc/sys/net/core/wmem_max

# DEFAULTS
# echo 2394 3193 4788 >/proc/sys/net/ipv4/tcp_mem
# echo 4096        131072  1635808 >/proc/sys/net/ipv4/tcp_rmem
# echo 4096        16384  1635808 >/proc/sys/net/ipv4/tcp_wmem

# PERFORMANCE
# Set this to 80% of physical memory
echo 2097152 2097152 2097152 >/proc/sys/net/ipv4/tcp_mem  #=> 8GB (4k pages)
echo 4194304 4194304 4194304 >/proc/sys/net/ipv4/tcp_mem  #=> 16GB (4k pages)

#echo 1024  4096 4096 >/proc/sys/net/ipv4/tcp_rmem
#echo 1024  4096 4096 >/proc/sys/net/ipv4/tcp_wmem
# -> 8k per socket. 1.000.000 sockets = 7,8GB

# echo 1024  8192 8192 >/proc/sys/net/ipv4/tcp_rmem
# echo 1024  8192 8192 >/proc/sys/net/ipv4/tcp_wmem

# echo 1024  16384 16384 >/proc/sys/net/ipv4/tcp_rmem
# echo 1024  16384 16384 >/proc/sys/net/ipv4/tcp_wmem

echo 1024  32768 32768 >/proc/sys/net/ipv4/tcp_rmem
echo 1024  32768 32768 >/proc/sys/net/ipv4/tcp_wmem

# echo 1024  65536 65536 >/proc/sys/net/ipv4/tcp_rmem
# echo 1024  65536 65536 >/proc/sys/net/ipv4/tcp_wmem

# echo 1024  131072 131072 >/proc/sys/net/ipv4/tcp_rmem
# echo 1024  131072 131072 >/proc/sys/net/ipv4/tcp_wmem

# echo 1024  3145728 3145728 >/proc/sys/net/ipv4/tcp_rmem
# echo 1024  3145728 3145728 >/proc/sys/net/ipv4/tcp_wmem

### SETUP per session
# Increase per process fd limit
ulimit -Sn unlimited

Example:
200,000 TCP connections
16384 bytes r/wmem TCP buffer
Memory usage: 200000 * 16384 * 2 / 1024 / 1024 / 1024 = 6.1GB

STOP HERE: record stats for every peer

 */
#include "common.h"
#include "choke.h"
#include "util.h"
#include "peer.h"

struct _gopt gopt;

static void
event_peer(int efd, int type, int events, struct peer *p)
{
	struct epoll_event ev;
	int ret;

	ev.events = events;
	ev.data.ptr = p;

	ret = epoll_ctl(efd, type, p->fd, &ev);
	XASSERT(ret == 0, "epoll_ctl()=%d: %s\n", ret, strerror(errno));
}

static void
cb_accept(int ls, int efd)
{
	struct peer *p;

	p = peer_accept(ls);

	event_peer(efd, EPOLL_CTL_ADD, EPOLLIN, p);
}

/*
 * Return 0 on success (all written)
 * Return -2 if peer was destroyed.
 */
static int
peer_write(struct peer *p, int efd)
{
	ssize_t sz;
	ssize_t len;

	XASSERT(p->len > p->pos, "p->len=%zd <= p->pos=%zd\n", p->len, p->pos);
	len = p->len - p->pos;

	sz = write(p->fd, p->buf + p->pos, len);
	if (sz > 0)
		gopt.bytes_sent += sz;

	if (sz >= len)
	{
		p->pos = 0;
		return 0; // successfully written all data.
	}

	if (sz == 0)
	{
		// EOF
		peer_free(p);
		return -2;
	}

	if (sz < 0)
	{
		if (errno == EAGAIN)
			return -1;

		peer_free(p);
		return -2;
	}

	// Partial write. 
	p->pos += sz;

	return -1;
}

static void
peer_server_write(struct peer *p, int efd)
{
	int ret;

	ret = peer_write(p, efd);
	if (ret == -2)
		return; // peer was free'd

	if (ret != 0)
	{
		// Stop reading until write() has completed.
		event_peer(efd, EPOLL_CTL_MOD, EPOLLOUT, p);
		return;
	}

	// Start reading again
	event_peer(efd, EPOLL_CTL_MOD, EPOLLIN, p);
	p->len = 0;
}

static int
peer_client_write(struct peer *p, int efd)
{
	int ret;

	ret = peer_write(p, efd);

	if (ret == 0)
	{
		// All data successfully written.
		// Get more data to write...
		p->len = sizeof p->buf;
		return 0;
	}

	return ret;
}

// Return length of read data. Return 0 otherwise (including free'd peer)
static ssize_t
peer_read(struct peer *p, void *data, ssize_t sz)
{
	ssize_t len;
	// XASSERT( == 0, "p->len is %zd\n", p->len);
	len = read(p->fd, data, sz);

	peer_read_stats(p, len);

	if (len == 0)
	{
		if (gopt.is_ddos_test) { exit(0); }
		peer_free(p);
		return 0;
	}

	if (len < 0)
	{
		if (errno != EAGAIN)
		{
			if (gopt.is_ddos_test) { exit(0); }
			peer_free(p);
			return 0;
		}

		// Triggered read-event but read() fails with EAGAIN?!?
		fprintf(stderr, "CAN NOT HAPPEN\n");
		exit(254);
	}

	return len;
}

static ssize_t
peer_server_read(struct peer *p)
{
	return peer_read(p, p->buf, sizeof p->buf);
}

static void
peer_client_read(struct peer *p)
{
	uint8_t buf[sizeof p->buf];

	peer_read(p, buf, sizeof buf);
}

static int
peer_complete_connect(struct peer *p, int efd)
{
	int ret;

	ret = fd_net_connect(p->fd, gopt.dst_ip, gopt.port);
	if (ret == -2)
	{
		peer_free(p);
		return -2;
	}
	if (ret == -1)
	{
		// Still EINPROGRESS/EAGAIN
		DEBUGF("What OS returns this twice????\n");
		return -1;
	}
	p->is_connected = 1;
	event_peer(efd, EPOLL_CTL_ADD, EPOLLIN | EPOLLOUT, p);

	return 0;
}

// Client
static void
client_event_dispatch(int efd, struct epoll_event *events, int nfds)
{
	int n;
	struct epoll_event *e;
	struct peer *p;
	int ret;

	for (n = 0; n < nfds; n++)
	{
		e = &events[n];
		p = (struct peer *)e->data.ptr;
		ret = 0;
		if (e->events & EPOLLOUT)
		{
			if (p->is_connected)
			{
				ret = peer_client_write(p, efd);
			} else {
				// Complete or fail/free peer
				ret = peer_complete_connect(p, efd);
			}
			// Peer got free'd. Continue.
			if (ret == -2)
				continue;
		}
		if (e->events & EPOLLIN)
		{
			peer_client_read(p);
		}
	}
}

static void
add_wevent(int efd, int wefd)
{
	int ret;
	struct epoll_event ev;
	ev.events = EPOLLIN;
	ev.data.fd = wefd;

	ret = epoll_ctl(efd, EPOLL_CTL_ADD, wefd, &ev);
	XASSERT(ret == 0, "epoll_ctl()=%d: %s\n", ret, strerror(errno));
}

static int
epoll_wait2(int efd, struct epoll_event *events, int max, int timeout)
{
	int nfds;

	while (1)
	{
		nfds = epoll_wait(efd, events, max, timeout);
		// DEBUGF("nfds=%d\n", nfds);
		if (nfds >= 0)
			break;
		if (errno != EINTR)
		{
			DEBUGF_R("epoll_wait(): %s\n", strerror(errno));
			break;
		}
	}

	return nfds;
}

static void
client_check_wevent(int wefd, int timeout)
{
	int nfds;
	struct epoll_event events[MAX_EVENTS_XFER];

	nfds = epoll_wait2(wefd, events, MAX_EVENTS_XFER, timeout);
	// DEBUGF("nfds=%d\n", nfds);

	client_event_dispatch(wefd, events, nfds);
}

static void
main_client(int argc, char *argv[], int efd, int wefd)
{
	int sox;
	int nfds;
	struct epoll_event events[MAX_EVENTS_CONN];
	int ret;
	struct peer *p;

	// add_wevent(efd, wefd); // un-comment to start I/O while still establishing connections

	// Open a new TCP connection every iteration.
	gopt.conn_start = get_usec();
	int is_conn_issued_output = 0;
	while (1)
	{
		if (gopt.n_conn < gopt.max_sockets)
		{
			// Open a new TCP connection:
			sox = fd_new_socket(SOCK_STREAM);
			XASSERT(sox >= 0, "socket(): %s\n", strerror(errno));

			ret = fd_net_bind(sox, gopt.src_ip, 0);
			XASSERT(ret >= 0, "bind(): %s\n", strerror(errno));

			ret = fd_net_connect(sox, gopt.dst_ip, gopt.port);
			if (ret == -2)
				break;

			gopt.n_conn += 1;
			p = peer_new(sox);
			p->len = sizeof (p->buf);

			if (ret == 0)
			{
				DEBUGF("Which OS connects immediately on non-blocking connect()?\n");
				p->is_connected = 1;
				event_peer(efd,  EPOLL_CTL_DEL, 0, p);
				event_peer(wefd, EPOLL_CTL_ADD, EPOLLIN | EPOLLOUT, p);
			} else {
				// EINPROGRESS or EAGAIN
				event_peer(efd, EPOLL_CTL_ADD, EPOLLOUT, p);
			}

			if (gopt.n_conn % CON_PER_IP == 0)
				gopt.src_ip += 1; // Move to next IP...
		}

		if (gopt.n_conn >= gopt.max_sockets)
		{
			if (is_conn_issued_output == 0)
			{
				printf("All connect() issued after %0.3fms\n", (float)(get_usec() - gopt.conn_start) / 1000);
				printf("Connected: %d/%d (%d pending)\n", gopt.n_conn_completed, gopt.n_conn, gopt.n_conn - gopt.n_conn_completed);
				is_conn_issued_output = 1;
			}
			if (gopt.n_conn_completed >= gopt.n_conn)
			{
				printf("All connect() completed after %0.3fms\n", (float)(get_usec() - gopt.conn_start) / 1000);
				break;
			}
		}
		// Check if any fds need attention
		nfds = epoll_wait2(efd, events, MAX_EVENTS_CONN, 0 /* DONT WAIT */);

		// First priority: Check all connect() that did not complete yet
		int n;
		int is_worker_set = 0;
		for (n = 0; n < nfds; n++)
		{
			// Skip the worker-efd
			if (events[n].data.fd == wefd)
			{
				is_worker_set = 1;
				continue;
			}

			if (events[n].events & EPOLLOUT)
			{
				p = (struct peer *)events[n].data.ptr;
				// Remove fd from connecting-epoll-fd
				event_peer(efd, EPOLL_CTL_DEL, 0, p);
				// Add fd to worker-epoll-fd
				ret = peer_complete_connect(p, wefd);
			} else {
				DEBUGF_R("Can not happen\n");
			}
		}

		if (is_worker_set == 0)
			continue;

		// Second priority: Check all worker-fds
		client_check_wevent(wefd, 0 /* DO NOT WAIT */);
	}


	if (gopt.n_conn < gopt.max_sockets)
		DEBUGF_Y("Warning: Only %d of %d sockets created (Try 'ulimit -Sn unlimited'.)\n", gopt.n_conn, gopt.max_sockets);

	if (gopt.is_ddos_test)
		exit(0);

	while (1)
	{
		client_check_wevent(wefd, -1);

		if (gopt.n_sox <= 0)
			break;
	}

	DEBUGF_G("all done.\n");
	exit(0);
}

// Check worker-events
static void
server_check_wevent(int wefd)
{
	int nfds;
	int n;
	struct epoll_event events[MAX_EVENTS_XFER];
	struct peer *p;
	ssize_t len;

	nfds = epoll_wait2(wefd, events, MAX_EVENTS_XFER, 0);

	for (n = 0; n < nfds; n++)
	{
		p = (struct peer *)events[n].data.ptr;
		if (events[n].events & EPOLLIN)
		{
			len = peer_server_read(p);
			if (len <= 0)
				continue;

			p->len = len;
			peer_server_write(p, wefd);

			continue;
		}

		if (events[n].events & EPOLLOUT)
		{
			peer_server_write(p, wefd);

			continue;
		}
	}
}

static void
main_server(int argc, char *argv[], int efd, int wefd)
{
	int ls, nfds;
	int ret;
	int n;

	ls = fd_new_socket(SOCK_STREAM);
	ret = fd_net_listen(ls, 31337);
	XASSERT(ret >= 0, "fd_net_listen()=%d: %s\n", ret, strerror(errno));

	struct epoll_event ev;
	ev.events = EPOLLIN;
	ev.data.fd = ls; // could be ptr

	ret = epoll_ctl(efd, EPOLL_CTL_ADD, ls, &ev);
	XASSERT(ret == 0, "epoll_ctl()=%d: %s\n", ret, strerror(errno));

	add_wevent(efd, wefd);

	// It's either the listen-fd or the worker-efd.
	struct epoll_event events[2];
	for (;;)
	{
		nfds = epoll_wait2(efd, events, 2, -1);

		// First check listen-fd
		for (n = 0; n < nfds; n++)
		{
			if (events[n].data.fd != ls)
				continue;

			// Add to worker-event-fd (wefd)
			cb_accept(ls, wefd);
			nfds--;
			break;
		}

		if (nfds <= 0)
			continue;

		// Then check all worker-fd's (read/write loops).
		server_check_wevent(wefd);

		if (gopt.n_sox <= 0)
			break;
	}

	DEBUGF_G("all done.\n");
	exit(0);
}

int
main(int argc, char *argv[])
{
	int efd;

	efd = epoll_create1(EPOLL_CLOEXEC);
	XASSERT(efd >= 0, "epoll_create1(): %s\n", strerror(errno));


	/* EPOLL is a terrible design. There is no native support to
	 * prioritze fd's. This app needs to prioritize the listening
	 * socket. Otherwise already established TCP connection
	 * can flood EPOLL and no new client could connect...
	 * One solution is to have _two_ epoll fd's: One for all
	 * established tcp connections (wefd) and one containing
	 * the listening socket and the wefd and then nest the wefd
	 * inside the efd (doh!).
	 */
	int wefd;
	wefd = epoll_create1(EPOLL_CLOEXEC);
	XASSERT(wefd >= 0, "epoll_create1(): %s\n", strerror(errno));


	// printf("Press any key to continue\n");
	// char c;
	// read(0, &c, 1);
	init_defaults();
	init_vars(argc, argv);

	if (gopt.is_client)
	{
		main_client(argc, argv, efd, wefd);
	} else {
		main_server(argc, argv, efd, wefd);
	}

	exit(0); // good habbit
	return 0;
}

