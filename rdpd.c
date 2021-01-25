#include <stdlib.h>
#include <sys/socket.h>
#include <pthread.h>
#include <netdb.h>
#include <string.h>
#include <time.h>
#include <math.h>

#include "rdpd.h"
#include "thread_errors.h"


size_t fam_count = 0;
struct record familiars[MAX_FAMILIARS];

int create_inet_socket();

int find_familiar(char *hostname);

int fill_record(int idx, struct record *record);

int say_hello(char *hostname);

_Noreturn void *outer_daemon() {
	int socket_fd = create_inet_socket();
	if (socket_fd < 0) { pthread_exit(&SOCK_ERR); }

	unsigned int from_len;
	struct sockaddr_in addr = { 0 }, from;
	addr.sin_family = AF_INET;
	addr.sin_port = htonl(OUTER_PORT);
	addr.sin_addr.s_addr = INADDR_ANY;

	if (bind(socket_fd, (const struct sockaddr *) &addr, sizeof(addr)) < 0) {
		pthread_exit(&BIND_ERR);
	}

	size_t hello_num = 0;

	while (1) {
		recvfrom(socket_fd, &hello_num, sizeof(hello_num), 0,
			(struct sockaddr *) &from, &from_len);
		hello_num++;
		sendto(socket_fd, &hello_num, sizeof(hello_num), 0,
		       (struct sockaddr *) &from, from_len);
	}
}

_Noreturn void *inner_daemon() {
	int socket_fd = socket(AF_UNIX, SOCK_DGRAM, 0);

	if (socket_fd < 0) { pthread_exit(&SOCK_ERR); }

	unsigned int from_len;
	struct sockaddr_in addr = { 0 }, from;
	addr.sin_family = AF_UNIX;
	addr.sin_port = htonl(INNER_PORT);
	addr.sin_addr.s_addr = INADDR_ANY;

	if (bind(socket_fd, (const struct sockaddr *) &addr, sizeof(addr)) < 0) {
		pthread_exit(&BIND_ERR);
	}

	struct record record = {};

	while (1) {
		size_t n = recvfrom(socket_fd, record.hostname, sizeof(record.hostname), 0,
			(struct sockaddr *) &from, &from_len);
		record.hostname[n] = 0;

		int idx = find_familiar(record.hostname);
		if (idx < 0) {
			if (fam_count ==MAX_FAMILIARS) {
				pthread_exit(&REC_OVERFLOW);
			}

			if (say_hello(record.hostname) == EXIT_SUCCESS) {
				idx = fam_count;
				fam_count++;
			}
		}

		if (fill_record(idx, &record) < 0) { pthread_exit(&REC_OVERFLOW); }
		sendto(socket_fd, &record, sizeof(record), 0,
		       (const struct sockaddr *) &from, from_len);
	}
}

int say_hello(char *hostname) {
	int sock_fd = create_inet_socket();
	struct hostent *host = gethostbyname(hostname);
	if (host == NULL) pthread_exit(&RESOLVE_HOST);

	unsigned int addr_len;
	struct sockaddr_in outer_addr = {};
	bcopy((char *) host->h_addr, (char *) &outer_addr.sin_addr.s_addr, host->h_length);
	outer_addr.sin_port = htonl(OUTER_PORT);

	size_t ping = rand(), pong = 0;
	struct timespec start, stop;

	clock_gettime(CLOCK_REALTIME, &start);
	sendto(sock_fd, &ping, sizeof(ping), 0,
		(const struct sockaddr *) &outer_addr, sizeof(outer_addr));
	recvfrom(sock_fd, &pong, sizeof(pong), 0,
	         (struct sockaddr *) &outer_addr, &addr_len);
	clock_gettime(CLOCK_REALTIME, &stop);

	if (ping + 1 != pong) { return EXIT_FAILURE; }

	strncpy(familiars[fam_count].hostname, hostname, RECORD_BUF);
	bcopy((char *) host->h_addr, (char *) familiars[fam_count].addr, host->h_length);
	familiars[fam_count].rtt_ms = floor(((double) stop.tv_sec - start.tv_sec) * 1e+3 +
		(double)(stop.tv_nsec - start.tv_nsec) * 1e-6);

	return EXIT_SUCCESS;
}

int fill_record(int idx, struct record *record) {
	if (idx >= fam_count) return EXIT_FAILURE;
	memcpy(record, &familiars[idx], sizeof(struct record));
	return EXIT_SUCCESS;
}

int find_familiar(char *hostname) {
	return 0;
}

int main() {
	pthread_t outer_thread;
	if (pthread_create(&outer_thread, NULL,
		    (void *(*)(void *)) outer_daemon, NULL) != EXIT_SUCCESS) {
		return EXIT_FAILURE;
	}

	pthread_t inner_thread;
	if (pthread_create(&inner_thread, NULL,
	                   (void *(*)(void *)) inner_daemon, NULL) != EXIT_SUCCESS) {
		return EXIT_FAILURE;
	}

	pthread_join(outer_thread, NULL);
	pthread_join(inner_thread, NULL);
	return EXIT_SUCCESS;
}

int create_inet_socket() {
	int socket_fd = socket(AF_INET, SOCK_DGRAM, 0);
	struct timeval timeout = {
		.tv_sec = 0,
		.tv_usec = 300000,
	};

	if (setsockopt(socket_fd, AF_INET, SO_RCVTIMEO,
	               &timeout, sizeof(timeout)) < 0) {
		pthread_exit(&SET_SOCK_OPT);
	}

	return socket_fd;
}
