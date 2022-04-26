
#ifndef __CK_CHOKE_H__
#define __CK_CHOKE_H__ 1

#define MAX_EVENTS_CONN (64)      // For completing tcp connections
#define MAX_EVENTS_XFER (64)      // For I/O
// FIXME: Anything above 1k connections and the througput goes down to 1/2 (WHY???)
// #define MAX_SOCKETS     (4)
#define DFL_MAX_SOCKETS     (200*1000)
#define SOMAXCON        (64*1024) // listen()
// Large buffer needs large tcp_wmem/tcp_rmem (2-3x the size of buffer size):
// With 10 concurrent connections:
// 2k   -> 325 MB/s
// 4k   ->  80 MB/s (rmem/wmem 1024 4096    4096 )
// 4k   -> 165 MB/s (rmem/wmem 1024 8192    8192 )
// 4k   -> 252 MB/s (rmem/wmem 1024 16384   16384 )
// 4k   -> 330 MB/s (rmwm/wmem 1024 32768   32768 )
// 4k   -> 470 MB/s (rmem/wmem 1024 65536   65536 )
// 4k   -> 520 MB/s (rmem/wmem 1014 131072  131072 )
// 4k   -> 550 MB/s (rmem/wmem 1024 3145728 3145728 )
// 8k   -> 315 MB/s (rmem/wmem 1024 16384   16384 )
// 8k   -> 480 MB/s (rmem/wmem 1024 32768   32768 )
// 8k   -> 850 MB/s
// 16k  -> 1.0 GB/s
// 32k  -> 1.3 GB/s
// 64k  -> 1.5 GB/s
// 128k -> 1.8 GB/s
// 256k -> 2.0 GB/s
// 512k -> 2.0 GB/s
#define BUF_SIZE        (8*1024)
#define SRC_IP_STR      "127.0.0.1"
// Estblishing more than 4k TCP connections from the same SRC-IP is super slow on linux
// (and eventually will exhaust all ephermeral source ports). Instead limit source ip to
// CON_PER_IP connections and then move to next IP address (SRC_IP++).
#define CON_PER_IP      (4*1000)


struct _gopt
{
	uint32_t dst_ip;
	uint32_t src_ip;
	uint16_t port;
	int max_sockets;
	int is_ddos_test;
	int is_client;
	int n_sox;
	int n_conn;
	int n_conn_completed;
	int n_conn_error;
	int n_socket_error;
	int n_accept_error;
	uint64_t bytes_sent;
	uint64_t bytes_read;
	uint64_t conn_start;
};

#endif /* !__CK_CHOKE_H__ */
