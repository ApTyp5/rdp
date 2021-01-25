
#ifndef RDP_RDPD_H
#define RDP_RDPD_H

#define INNER_PORT      5002
#define OUTER_PORT      5003
#define MAX_FAMILIARS   100
#define RECORD_BUF    100

struct record {
	char hostname[RECORD_BUF];
	char addr[RECORD_BUF];
	size_t rtt_ms;
};

#endif //RDP_RDPD_H
