

#include "common.h"
#include "choke.h"
#include "peer.h"
#include "util.h"

extern struct _gopt gopt;

static struct peer_mgr pm;
static uint64_t g_free_start;

void
pm_init(int max_sockets)
{
	pm.peers = calloc(max_sockets, sizeof (struct peer *));
	XASSERT(pm.peers != NULL, "calloc(): %s\n", strerror(errno));
}

static void
pm_out_distribution(int *np, int sz, uint64_t chunk, const char *suffix)
{
	char bps_str[10];
	int i;

	for (i = 0; i < sz; i++)
	{
		format_bps(bps_str, sizeof bps_str, chunk * (i+1), suffix);
		printf("<%9.9s|", bps_str);
	}
	printf("\n");

	for (i = 0; i < sz; i++)
	{
		printf(" % 9d|", np[i]);
	}
	printf("\n");
	fflush(stdout);
}

static void
pm_stats_out(void)
{
	int n_peers[8];

	int n;
	uint64_t max = 0;
	uint64_t min = 0xFFFFFFFFFFFFFFFF;
	uint64_t new;
	for (n = 0; n < pm.n; n++)
	{
		new = pm.peers[n]->bytes_read;
		if (max < new)
			max = new;

		if (min > new)
			min = new;
	}

	printf("Read min: %"PRIu64"\n", min);
	printf("Read max: %"PRIu64"\n", max);

	if (max <= 0)
		return;

	// Total bytes distribution
	printf("Distribution of %d peers (bytes read):\n", pm.n);
	int i;
	memset(n_peers, 0, sizeof n_peers);
	uint64_t chunk = max / (sizeof n_peers / sizeof *n_peers);
	for (n = 0; n < pm.n; n++)
	{
		i = pm.peers[n]->bytes_read / (chunk + 1);
		n_peers[i] += 1;
	}

	pm_out_distribution(n_peers, sizeof n_peers / sizeof *n_peers, chunk, NULL);

	// Bytes/sec distribution
	/////////////////////////
	printf("Distribution of %d peers (bytes/sec):\n", pm.n);
	uint64_t usec = g_free_start;
	memset(n_peers, 0, sizeof n_peers);
	max = 0;
	uint64_t duration;
	uint64_t bps;
	for (n = 0; n < pm.n; n++)
	{
		duration = usec - pm.peers[n]->usec_first_read;
		bps = (pm.peers[n]->bytes_read * 1000000 / duration);
		pm.peers[n]->bps = bps;

		if (bps > max)
			max = bps;
	// format_bps(bps, sizeof bps, (amount * 1000000 / duration), "/s");
	}

	printf("Max bps: %"PRIu64"\n", max); 
	chunk = max / (sizeof n_peers / sizeof *n_peers);
	for (n = 0; n < pm.n; n++)
	{
		i = pm.peers[n]->bps / (chunk + 1);
		n_peers[i] += 1;
	}

	pm_out_distribution(n_peers, sizeof n_peers / sizeof *n_peers, chunk, "/s");
}

void
peer_read_stats(struct peer *p, ssize_t n)
{
	if (n < 0)
		return;

	gopt.bytes_read += n;
	p->bytes_read += n;
	if (p->usec_first_read == 0)
		p->usec_first_read = get_usec();
}

void
peer_write_stats(struct peer *p, ssize_t n)
{
	if (n < 0)
		return;

	gopt.bytes_sent += n;
}

void
peer_free(struct peer *p)
{
	// DEBUGF_G("#%d, peer_free(%d) %p\n", gopt.n_sox, p->fd, p);
	if (g_free_start == 0)
	{
		g_free_start = get_usec();
		// Gather stats per peer.
		pm_stats_out();
	}

	close(p->fd);
	free(p);

	gopt.n_sox -= 1;
}


struct peer *
peer_new(int sox)
{
	XASSERT(pm.n < gopt.max_sockets, "To many peers. MAX=%d\n", gopt.max_sockets);

	struct peer *p = calloc(1, sizeof (struct peer));
	XASSERT(p != NULL, "calloc(): %s\n", strerror(errno));
	p->fd = sox;
	gopt.n_sox += 1;

	pm.peers[pm.n] = p;
	pm.n += 1;

	return p;
}

struct peer *
peer_accept(int ls)
{
	int sox;

	sox = fd_net_accept(ls);
	if (sox < 0)
	{
		// Max Number of file descriptors reached. Stop accepting more conns.
		DEBUGF_Y("accept()=%d, %s\n", sox, strerror(errno));
		if (errno == EMFILE)
			DEBUGF_Y("Try 'ulimit -Sn unlimited && ulimit -Hn'\n");
		exit(3); // FIXME remove this
		close(ls);
		return NULL;
	}

	return peer_new(sox);
}

