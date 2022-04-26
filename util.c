
#include "common.h"
#include "choke.h"
#include "util.h"
#include "peer.h"

extern struct _gopt gopt;

static void
sig_alarm(int sig)
{
	static uint64_t bytes_sent_old;
	static uint64_t usec_old;
	char bps[64];
	uint64_t amount = gopt.bytes_sent - bytes_sent_old;
	uint64_t usec = get_usec();
	uint64_t duration = usec - usec_old;

	format_bps(bps, sizeof bps, (amount * 1000000 / duration), "/s");
	bytes_sent_old = gopt.bytes_sent;
	usec_old = usec;

	printf("stats: sox=%d [%d/%d] [%s TX=%"PRIu64"/RX=%"PRIu64" bytes] [s=%d/c=%d/a=%d]\n", gopt.n_sox, \
		gopt.n_conn, gopt.n_conn_completed, \
		bps, \
		gopt.bytes_sent, gopt.bytes_read, \
		gopt.n_socket_error, gopt.n_conn_error, gopt.n_accept_error);
	fflush(stdout);
	alarm(1);
}

uint64_t
get_usec(void)
{
	struct timeval tv;

	gettimeofday(&tv, NULL);
	return TV_TO_USEC(&tv);
}

void
init_defaults(void)
{
	// Must ingore SIGPIPE. Can happen the epoll() signals that socket
	// is ready for writing but by the time write() is called the socket
	// may have been closed by the remote host.
	signal(SIGPIPE, SIG_IGN);

	gopt.port = 31337;
	gopt.dst_ip = ntohl(inet_addr("127.0.0.1"));
	gopt.src_ip = ntohl(inet_addr(SRC_IP_STR));

	gopt.max_sockets = DFL_MAX_SOCKETS;

	signal(SIGALRM, sig_alarm);
	alarm(1);
}

static void
usage(void)
{
	fprintf(stderr, ""
" -p <port>     TCP port [default=31337]\n"
" -s <ip>       Source IP [default="SRC_IP_STR"]\n"
" -d <ip>       Destination IP [default=127.0.0.1]\n"
" -n <num>      Number of connections [default=%d]\n"
" -c            Client [default=server]\n"
"", DFL_MAX_SOCKETS);
	exit(0);
}

void
init_vars(int argc, char *argv[])
{
	int c;

	opterr = 0;
	while ((c = getopt(argc, argv, "p:n:s:d:kch")) != -1)
	{
		switch (c)
		{
			case 'p':
				gopt.port = atoi(optarg);
				break;
			case 's':
				gopt.src_ip = ntohl(inet_addr(optarg));
				break;
			case 'd':
				gopt.dst_ip = ntohl(inet_addr(optarg));
				break;
			case 'n':
				gopt.max_sockets = atoi(optarg);
				break;
			case 'k':
				gopt.is_ddos_test = 1;
				break;
			case 'c':
				gopt.is_client = 1;
				break;
			case 'h':
				usage();
				break;
		}
	}

	pm_init(gopt.max_sockets);
}


// 7 readable characters + suffix + 0
static const char unit[] = "BKMGT";
void
format_bps(char *dst, size_t size, int64_t bytes, const char *suffix)
{
	int i;

	if (suffix == NULL)
		suffix = "";

	if (bytes < 1000)
	{
		snprintf(dst, size, "%3d.0 B%s", (int)bytes, suffix);
		return;
	}
	bytes *= 100;

	for (i = 0; bytes >= 100*1000 && unit[i] != 'T'; i++)
		bytes = (bytes + 512) / 1024;
	snprintf(dst, size, "%3lld.%1lld%c%s%s",
            (long long) (bytes + 5) / 100,
            (long long) (bytes + 5) / 10 % 10,
            unit[i],
            i ? "B" : " ", suffix);
}

int
fd_net_bind(int fd, uint32_t ip, uint16_t port)
{
	struct sockaddr_in addr;
	int ret;

	memset(&addr, 0, sizeof addr);
	addr.sin_family = AF_INET;
	addr.sin_addr.s_addr = htonl(ip);
	addr.sin_port = htons(port);

	ret = bind(fd, (struct sockaddr *)&addr, sizeof addr);
	if (ret < 0)
		return ret;

	return 0;
}

int
fd_net_listen(int fd, uint16_t port)
{
	int ret;

	setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &(int){1}, sizeof (int));

	ret = fd_net_bind(fd, INADDR_ANY, port);
	if (ret < 0)
		return ret;

	ret = listen(fd, SOMAXCON);
	if (ret != 0)
		return -1;

	return 0;
}

int
fd_new_socket(int type)
{
	int fd;
	int ret;

	fd = socket(PF_INET, type, 0);
	gopt.n_socket_error += 1;
	if (fd < 0)
		return -2;

	ret = fcntl(fd, F_SETFL, O_NONBLOCK | fcntl(fd, F_GETFL, 0));
	if (ret != 0)
		return -2;

	return fd;
}


/*
 * Complete the connect() call
 * Return -1 on waiting.
 * Return -2 on fatal.
 * Return 0 on success
 */
int
fd_net_connect(int fd, uint32_t ip, uint16_t port)
{
	struct sockaddr_in addr;
	int ret;

	memset(&addr, 0, sizeof addr);
	addr.sin_family = AF_INET;
	addr.sin_addr.s_addr = htonl(ip);
	addr.sin_port = htons(port);
	errno = 0;
	ret = connect(fd, (struct sockaddr *)&addr, sizeof addr);
	if (ret != 0)
	{
		if ((errno == EINPROGRESS) || (errno == EAGAIN))
		{
			if (errno == EAGAIN) // What OS returns EAGAIN on non-blocking connect() calls?
				DEBUGF("LOOSER OS. Why return EGAIN if EINPROGRESS would do?\n");
			return -1;
		}
		if (errno != EISCONN)
		{
			gopt.n_conn_error += 1;
			DEBUGF_R("connect(%d): %s\n", fd, strerror(errno));
			return -2;
		}
	}
	gopt.n_conn_completed += 1;
	/* HERRE: ret == 0 or errno == EISCONN (Socket is already connected) */

	return 0;
}


int
fd_net_accept(int listen_fd)
{
	int sox;
	int ret;

	sox = accept(listen_fd, NULL, NULL);
	if (sox < 0)
	{
		gopt.n_accept_error += 1;
		return -2;
	}

	ret = fcntl(sox, F_SETFL, O_NONBLOCK | fcntl(sox, F_GETFL, 0));
	if (ret != 0)
		return -2;

	return sox;
}
