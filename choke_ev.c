
#include "common.h"

#include "choke.h"
#include "util.h"
#include "peer.h"

// The lower the number the higher is the priority
#define PRIORITY_HIGH   0
#define PRIORITY_LOW    9

struct _gopt gopt;
static struct event_base *evb;
static void cb_read(struct bufferevent *bev, void *arg);
static void cb_read_client(struct bufferevent *bev, void *arg);
static void cb_write_client(struct bufferevent *bev, void *arg);

// Called by LibEvent if socket received an error.
static void
cb_on_error(struct bufferevent *bev, short what, void *arg)
{
	struct peer *p = (struct peer *)arg;
	int ret;

	if (what == BEV_EVENT_CONNECTED)
	{
		// CLIENT
		// DEBUGF("%d: connected!\n", p->fd);
		gopt.n_conn_completed += 1;
		if (gopt.n_conn_completed >= gopt.max_sockets)
		{
			printf("All connect() completed after %0.3fms\n", (float)(get_usec() - gopt.conn_start) / 1000);
		}

		event_free(p->ev);

		p->bev = bufferevent_socket_new(evb, p->fd, 0);
		ret = bufferevent_priority_set(p->bev, PRIORITY_LOW);
		XASSERT(ret == 0, "bufferevent_priority_set(): Error\n");

		bufferevent_setcb(p->bev, cb_read_client, cb_write_client, cb_on_error, p);
		bufferevent_enable(p->bev, EV_READ | EV_WRITE | EV_PERSIST);

		// Start writing data....
		bufferevent_write(p->bev, p->buf, sizeof p->buf);
		peer_write_stats(p, sizeof p->buf);
		return;
	}

	DEBUGF("%d: on-error what=%d %s\n", p->fd, what, strerror(errno));
	bufferevent_free(p->bev);
	peer_free(p);
	if (gopt.n_sox <= 0)
	{
		DEBUGF_G("all done.\n");
		exit(0);
	}
}

static void
cb_read(struct bufferevent *bev, void *arg)
{
	struct peer *p = (struct peer *)arg;
	size_t n;

	n = bufferevent_read(bev, p->buf, sizeof p->buf);
	peer_read_stats(p, n);

	if (n <= 0)
	{
		DEBUGF_R("EOF\n");
		// FIXME: do we still have to free or was cb_buffered_on_error already called?
		return;
	}

	bufferevent_write(p->bev, p->buf, n);
	peer_write_stats(p, n);
}

// Server
static void
cb_accept(int fd, short ev, void *arg)
{
	struct peer *p;

	p = peer_accept(fd);

	p->bev = bufferevent_socket_new(evb, p->fd, 0);
	bufferevent_setcb(p->bev, cb_read, NULL, cb_buffered_on_error, p);
	bufferevent_enable(p->bev, EV_READ);

}

static void
main_server(int argc, char *argv[])
{
	int ls;
	struct event ev_accept;
	int ret;

	ls = fd_new_socket(SOCK_STREAM);
	ret = fd_net_listen(ls, 31337);
	XASSERT(ret >= 0, "fd_net_listen()=%d: %s\n", ret, strerror(errno));

	event_assign(&ev_accept, evb, ls, EV_READ|EV_PERSIST, cb_accept, NULL);
	event_add(&ev_accept, NULL);

	event_base_dispatch(evb);
}

// Client
static void
cb_read_client(struct bufferevent *bev, void *arg)
{
	struct peer *p = (struct peer *)arg;
	size_t n;

	n = bufferevent_read(bev, p->buf, sizeof p->buf);
	peer_read_stats(p, n);

	if (n <= 0)
	{
		DEBUGF_R("EOF\n");
		return;
	}
}

static void
cb_write_client(struct bufferevent *bev, void *arg)
{
	struct peer *p = (struct peer *)arg;

	bufferevent_write(p->bev, p->buf, sizeof p->buf);
	peer_write_stats(p, sizeof p->buf);
}

static void
main_client(int argc, char *argv[])
{
	int sox;
	int ret;
	struct peer *p;

	gopt.conn_start = get_usec();
	while (1)
	{
		if (gopt.n_conn >= gopt.max_sockets)
			break;

		sox = fd_new_socket(SOCK_STREAM);
		XASSERT(sox >= 0, "socket(): %s\n", strerror(errno));

		ret = fd_net_bind(sox, gopt.src_ip, 0);
		XASSERT(ret >= 0, "bind(): %s\n", strerror(errno));

		ret = fd_net_connect(sox, gopt.dst_ip, gopt.port);
		XASSERT(ret != -2, "connect(): %s\n", strerror(errno));

		gopt.n_conn += 1;
		if (gopt.n_conn % CON_PER_IP == 0)
			gopt.src_ip += 1; // Move to next IP...

		p = peer_new(sox);

		p->ev = event_socket_new(evb, p->fd, 0);

		// p->bev = bufferevent_socket_new(evb, p->fd, 0);

		ret = event_priority_set(p->ev, PRIORITY_HIGH);
		// ret = bufferevent_priorsity_set(p->bev, PRIORITY_HIGH);
		XASSERT(ret == 0, "bufferevent_priority_set(): Error\n");

		ret = event_socket_connect(p->ev, NULL, 0);
		// ret = bufferevent_socket_connect(p->bev, NULL, 0);
		XASSERT(ret == 0, "bev_socket_connect: %s\n", strerror(errno));

		event_setcb(p->ev, NULL, NULL, cb_on_error, p);
		event_enable(p->ev, 0);

// stop here: i think we need to use different ev-bases for real priority (as long as events are happening on first
// 	nbase do not schedule second base....check first base first.)
// stop also: why is choke_epoll not transfering data until all cons where opened? try to remote host?
		// bufferevent_setcb(p->bev, NULL, NULL, cb_on_error, p);
		// bufferevent_enable(p->bev, 0);
		stop here: record inside how much kernel memory is used...

		ret = event_base_loop(evb, EVLOOP_ONCE | EVLOOP_NONBLOCK);
		XASSERT(ret == 0, "event_base_loop(): Error\n");
	}

	printf("All connect() issued after %0.3fms\n", (float)(get_usec() - gopt.conn_start) / 1000);
	printf("Connected: %d/%d (%d pending)\n", gopt.n_conn_completed, gopt.n_conn, gopt.n_conn - gopt.n_conn_completed);

	if (gopt.n_conn < gopt.max_sockets)
		DEBUGF_Y("Warning: Only %d of %d sockets created (Try 'ulimit -Sn unlimited'.)\n", gopt.n_conn, gopt.max_sockets);

	ret = event_base_dispatch(evb);
	DEBUGF("FOOBAR ret=%d\n", ret);
}

int
main(int argc, char *argv[])
{
	evb = event_base_new();
	XASSERT(evb != NULL, "Could not initialize libevent!\n");

	event_base_priority_init(evb, 10);

	init_defaults();
	init_vars(argc, argv);

	if (argc > 1)
	{
		if (argv[1][0] == 'c')
			main_client(argc, argv);
		else if (argv[1][0] == 's')
			main_server(argc, argv);
	}

	exit(0);
	return 0;
}
