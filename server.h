#ifndef RDP_SERVER_H
#define RDP_SERVER_H

#include <sys/types.h>

ssize_t rdp_recv(unsigned int port, void **user_buf, size_t **len);

#endif //RDP_SERVER_H
