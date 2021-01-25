#include <stdio.h>
#include <stdlib.h>
#include <string.h>
//#include <sys/types.h>
#include <sys/socket.h>
//#include <sys/stat.h>
#include <netinet/in.h>
//#include <time.h>
#include <unistd.h>
#include <math.h>

#include "server.h"
#include "dgram.h"
#include "debug.h"

void write_chunk(char **char_user_buf, struct dgram *dgram, size_t last_chunk_size, size_t packet_total);

int create_inet_socket();

int sock_bind(int sock_fd, unsigned int port);

int receive_dgram(int sock_fd, struct dgram *dgram, struct sockaddr_in *from, unsigned int *from_len);

int send_confirmation(unsigned int from_len, int sock_fd, struct dgram *dgram, struct sockaddr_in *from);

ssize_t rdp_recv(unsigned int port, void **user_buf, size_t **len) {
	D("rdp_recv", "start");
	char **char_user_buf = (char **)user_buf;

	int sock_fd = create_inet_socket();
	if (sock_fd < 0) { return EXIT_FAILURE; }
	D("rdp_recv", "socket created");

	if (sock_bind(sock_fd, port) < 0) { return EXIT_FAILURE; }
	D("rdp_recv", "socket binded");

	struct dgram dgram = {0};
	struct sockaddr_in from = {0};
	unsigned int from_len = sizeof(from);
	if (receive_dgram(sock_fd, &dgram, &from, &from_len) < 0) {
		return EXIT_FAILURE;
	}
	D("rdp_recv", "1-st dgram received");

	size_t packet_numeral = 1;
	if ((*char_user_buf = malloc(dgram.message_size + 1)) == NULL) {
		return EXIT_FAILURE;
	}
	(*char_user_buf)[dgram.message_size] = 0;
	D("rdp_recv", "buf memory allocated");

	if ((*len = malloc(sizeof(size_t))) == NULL) { return EXIT_FAILURE; }
	D("rdp_recv", "len memory allocated");

	size_t last_chunk_size = dgram.message_size % CHUNK_SIZE;
	size_t packet_total = ceil(
		dgram.message_size / (double) CHUNK_SIZE
	);
	D("rdp_recv", "total and last size counted");

	char *packet_check = calloc(packet_total, sizeof(char));
	if (packet_check == NULL) { return EXIT_FAILURE; }
	packet_check[dgram.packet_num] = 1;
	D("rdp_recv", "packet check memory allocated");

	write_chunk(char_user_buf, &dgram, last_chunk_size, packet_total);

	if (send_confirmation(from_len, sock_fd, &dgram, &from) < 0) {
		return EXIT_FAILURE;
	}
	D("rdp_recv", "1-st packet confirmation sent");

	while (packet_numeral < packet_total) {
		if (receive_dgram(sock_fd, &dgram, &from, &from_len) < 0) {
			return EXIT_FAILURE;
		}
		D("rdp_recv cycle", "dgram received");

		packet_numeral++;

		if (packet_check[dgram.packet_num] == 0) {
			D("rdp_recv cycle", "new dgram");
			write_chunk(char_user_buf, &dgram, last_chunk_size, packet_total);

			packet_check[dgram.packet_num] = 1;
			if (send_confirmation(from_len, sock_fd, &dgram, &from) < 0) {
				return EXIT_FAILURE;
			}
			D("rdp_recv cycle", "packet confirmation sent");
		}
	}
	D("rdp_recv", "end cycle");

	free(packet_check);
	close(sock_fd);
}

void write_chunk(char **char_user_buf, struct dgram *dgram, size_t last_chunk_size, size_t packet_total) {
	if ((*dgram).packet_num + 1 == packet_total) {
		memcpy((void *) &(*char_user_buf)[(*dgram).packet_num * CHUNK_SIZE],
		       &(*dgram).chunk, last_chunk_size);
	} else {
		memcpy((void *) &(*char_user_buf)[(*dgram).packet_num * CHUNK_SIZE],
		       &(*dgram).chunk, CHUNK_SIZE);
	}
}

int send_confirmation(unsigned int from_len, int sock_fd, struct dgram *dgram, struct sockaddr_in *from) {
	return sendto(sock_fd, &(*dgram).packet_num, sizeof((*dgram).packet_num), 0,
	       (const struct sockaddr *) from, from_len);
}

int receive_dgram(int sock_fd, struct dgram *dgram, struct sockaddr_in *from, unsigned int *from_len) {
	return recvfrom(sock_fd, dgram, sizeof(*dgram), 0,
	         (struct sockaddr *) from, from_len);
}

int create_inet_socket() {
	return socket(AF_INET, SOCK_DGRAM, 0);
}

int sock_bind(int sock_fd, unsigned int port) {
	struct sockaddr_in serv_addr = { 0 };
	serv_addr.sin_family = AF_INET;
	serv_addr.sin_addr.s_addr = INADDR_ANY;
	serv_addr.sin_port = htons(port);
	return bind(sock_fd, (struct sockaddr *) &serv_addr, sizeof(serv_addr));
}

int main() {
	char *buf = NULL;
	size_t *len = NULL;

	if (rdp_recv(5000, (void **) &buf, &len)) {
		perror("error occured");
	}

	printf("message: '%s'\n", buf);
	free(buf);
	free(len);

	return 0;
}
