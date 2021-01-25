#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <pthread.h>
#include <unistd.h>
#include <math.h>

#include "dgram.h"
#include "debug.h"
#include "rdpd.h"

struct confirm_args {
	int sock_fd;
	char *packet_received;
	size_t packet_total;
};

void try_exit_thread(size_t packet_total, const char *packet_received) {
//	D("try_exit_thread", "start");
	int sum = 0;
	for (int i = 0; i < packet_total; i++) {
		sum += packet_received[i];
	}

	if (sum == packet_total) {
//		D("try_exit_thread", "exit");
		pthread_exit(0);
	}
//	D("try_exit_thread", "continue");
}

void *receive_confirmations(struct confirm_args *args) {
	D("receive_confirmations", "start");
	struct sockaddr_in from;
	unsigned int from_len;
	int packet_num;

	while (1) {
		if (recvfrom(args->sock_fd, &packet_num, sizeof(int), 0,
			(struct sockaddr * ) &from, &from_len) < 0) {
			exit(12);
		}
		D("receive_confirmations", "confirmation received");

		if (packet_num >= args->packet_total) {
			exit(13);
		}
		D("receive_confirmations", "packet_num < args->packet_total");

		args->packet_received[packet_num] = 1;

		D("receive_confirmations", "try exit");
		try_exit_thread(args->packet_total, args->packet_received);
	}
}

struct resend_args {
	int sock_fd;
	char *packet_received;
	size_t packet_total;
	const void *buf;
	size_t rtt;
	size_t last_chunk_size;
	struct sockaddr_in *serv_addr;
};

int send_dgram(int sock_fd, struct sockaddr_in *serv_addr, struct dgram *dgram);

int try_resend_packet(struct resend_args *args, int packet_num) {
	D("try_resend_packet", "start", "args->packet_received[packet_num] = %d\n", args->packet_received[packet_num]);

	if (!args->packet_received[packet_num]) {
		struct dgram dgram = {
			.packet_num = packet_num,
		};

		char *char_buf = (char *)args->buf;
		if (packet_num == args->packet_total - 1 && args->last_chunk_size != 0) {
			memcpy(dgram.chunk, &char_buf[packet_num * CHUNK_SIZE], args->last_chunk_size);
		} else {
			memcpy(dgram.chunk, &char_buf[packet_num * CHUNK_SIZE], CHUNK_SIZE);
		}

		if (send_dgram(args->sock_fd, args->serv_addr, &dgram) < 0) {
			return EXIT_FAILURE;
		}
	}

	return EXIT_SUCCESS;
}

void *resend_chunks(struct resend_args *args) {
	D("resend_chunks", "start");

	while (1) {
		usleep(args->rtt * 1);
		try_exit_thread(args->packet_total, args->packet_received);
		D("resend_chunks", "still here");

		for (int i = 0; i < args->packet_total; i++) {
			D("resend_chunks", "try resend packet");
			if (try_resend_packet(args, i) < 0) {
				exit(18);
			}
			D("resend_chunks", "good package receiving");
		}
	}
}

int create_timeout_socket();

int sock_bind(int sock_fd, const struct sockaddr *);

int send_cur_dgram(int sock_fd, struct sockaddr_in *serv_addr, struct dgram *dgram,
	int curr_packet, size_t packet_total, size_t last_chunk_size, size_t len, const void *buf);

int init_serv_addr(struct sockaddr_in *serv_addr, const char *host, size_t port);

int rdp_hello(const char *host) {
	int sock_fd = socket(AF_UNIX, SOCK_DGRAM, 0);
	struct sockaddr_in serv_addr = {
		.sin_family = AF_UNIX,
		.sin_port = INNER_PORT,
		.sin_addr.s_addr = INADDR_ANY,
	};

	sendto(sock_fd, host, strlen(host), 0,
	       (const struct sockaddr *) &serv_addr, sizeof(serv_addr));

	close(sock_fd);
}

int rdp_send(const char *host, unsigned int port, const void *buf, unsigned int len) {
	D("rdp_send", "start");

	int sock_fd = create_timeout_socket();
	if (sock_fd < 0) { return EXIT_FAILURE; }
	D("rdp_send", "socket created");

	pthread_t confirm_thread, resend_thread;
	struct dgram dgram = { 0 };

	struct sockaddr_in serv_addr = { 0 };
	if (init_serv_addr(&serv_addr, host, port) < 0) {
		return EXIT_FAILURE;
	}
	D("rdp_send", "server address initialized");

	sock_bind(sock_fd, (const struct sockaddr *) &serv_addr);
	D("rdp_send", "socket binded");

	size_t last_chunk_size = len % CHUNK_SIZE;
	size_t packet_total = ceil(len / (double) CHUNK_SIZE);
	D("rdp_recv", "total and last size counted");

	char *packet_received = calloc(packet_total, sizeof(char));
	if (packet_received == NULL) {
		return EXIT_FAILURE;
	}
	D("rdp_send", "packet received buf allocated");

	int curr_packet = 0;
	if (send_cur_dgram(sock_fd, &serv_addr, &dgram,
		    curr_packet, packet_total, last_chunk_size, len, buf) < 0) {
		return EXIT_FAILURE;
	}
	D("rdp_send", "1-st dgram sended");

	curr_packet++;

	struct confirm_args confirm_args = {
		.packet_received = packet_received,
		.packet_total = packet_total,
		.sock_fd = sock_fd,
	};

	if (pthread_create(&confirm_thread, NULL,
	                   (void *(*)(void *)) receive_confirmations, &confirm_args) != EXIT_SUCCESS) {
		return EXIT_FAILURE;
	}
	D("rdp_send", "confirm thread created");

	struct resend_args resend_args = {
		.serv_addr = &serv_addr,
		.sock_fd = sock_fd,
		.packet_total = packet_total,
		.last_chunk_size = last_chunk_size,
		.buf = buf,
		.rtt = 20000,
		.packet_received = packet_received,
	};

	if (pthread_create(&resend_thread, NULL,
	                   (void *(*)(void *)) resend_chunks, &resend_args) != EXIT_SUCCESS) {
		return EXIT_FAILURE;
	}
	D("rdp_send", "resend thread created");

	while (curr_packet < packet_total) {
		D("rdp_send", "curr_packet < packet_total");

		if (send_cur_dgram(sock_fd, &serv_addr, &dgram,
		     curr_packet, packet_total, last_chunk_size, len, buf) < 0) {
			return EXIT_FAILURE;
		}
		D("rdp_send", "current dgram send");

		curr_packet++;
	}
	D("rdp_send", "curr_packet >= packet_total");

	pthread_join(confirm_thread, NULL);
	pthread_join(resend_thread, NULL);
	D("rdp_send", "threads joined");

	close(sock_fd);
	D("rdp_send", "end");

	return 0;
}

int init_serv_addr(struct sockaddr_in *serv_addr, const char *host, size_t port) {
	struct hostent *server = gethostbyname(host); // заменить на обращение к демону
	if (server == NULL) {
		return EXIT_FAILURE;
	}

	serv_addr->sin_family = AF_INET;
	bcopy((char *) server->h_addr, (char *) &serv_addr->sin_addr.s_addr, server->h_length);
	serv_addr->sin_port = htons(port);

	return EXIT_SUCCESS;
}

int send_cur_dgram(int sock_fd, struct sockaddr_in *serv_addr, struct dgram *dgram,
	int curr_packet, size_t packet_total, size_t last_chunk_size, size_t len, const void *buf) {
	const char *char_buf = (const char *)buf;
	dgram->packet_num = curr_packet;
	if (curr_packet == packet_total - 1 && last_chunk_size != 0) {
		memcpy(dgram->chunk, &char_buf[curr_packet * CHUNK_SIZE], last_chunk_size);
	} else {
		memcpy(dgram->chunk, &char_buf[curr_packet * CHUNK_SIZE], CHUNK_SIZE);
	}
	dgram->message_size = len;
	if (send_dgram(sock_fd, serv_addr, dgram) < 0) {
		return EXIT_FAILURE;
	}
	return EXIT_SUCCESS;
}

int send_dgram(int sock_fd, struct sockaddr_in *serv_addr, struct dgram *dgram) {

	return sendto(sock_fd, dgram, sizeof(*dgram), 0,
		(const struct sockaddr *) serv_addr, sizeof(*serv_addr));
}

int create_timeout_socket() {
	return socket(AF_INET, SOCK_DGRAM, 0);
}

int sock_bind(int sock_fd, const struct sockaddr *sock_addr) {
	D("sock_bind", "start");
	return bind(sock_fd, (struct sockaddr *) sock_addr, sizeof(*sock_addr));
}

int main() {
	char buf[] = "qwer";
	size_t len = sizeof(buf);

	if (rdp_send("localhost", 5000, buf, len)) {
		perror("error occured");
	}
	return 0;
}
