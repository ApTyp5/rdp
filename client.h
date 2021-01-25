#ifndef RDP_CLIENT_H
#define RDP_CLIENT_H

int rdp_send(
	const char *host,
	unsigned int port,
	const void *buf,
	unsigned int len
);

int rdp_hello(const char *host, int with_receive);

#endif //RDP_CLIENT_H
