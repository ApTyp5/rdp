#include <stdlib.h>
#include <sys/socket.h>
#include <pthread.h>
#include <netdb.h>
#include <string.h>
#include <time.h>
#include <math.h>

#include "rdpd.h"
#include "debug.h"
#include "thread_errors.h"


size_t fam_count = 0;
struct record familiars[MAX_FAMILIARS];

int find_familiar(char *hostname);

int fill_record(int idx, struct record *record);

int say_hello(char *hostname);

_Noreturn void *outer_daemon() {
	D("outer_daemon", "start");
	int socket_fd = socket(AF_INET, SOCK_DGRAM, 0);
	if (socket_fd < 0) { pthread_exit(&SOCK_ERR); }
	D("outer_daemon", "socket created");

	struct sockaddr_in addr = { 0 }, from;
	unsigned int from_len = sizeof(from);
	addr.sin_family = AF_INET;
	addr.sin_port = htons(OUTER_PORT);
	addr.sin_addr.s_addr = INADDR_ANY;

	from.sin_family = AF_INET;
	from.sin_port = htons(INNER_PORT);
	from.sin_addr.s_addr = INADDR_ANY;

	if (bind(socket_fd, (const struct sockaddr *) &addr, sizeof(addr)) < 0) {
		pthread_exit(&BIND_ERR);
	}
	D("outer_daemon", "socket binded");

	size_t hello_num = 0;

	while (1) {
		recvfrom(socket_fd, &hello_num, sizeof(hello_num), 0,
			(struct sockaddr *) &from, &from_len);
		D("outer_daemon", "hello received", "%zu\n", hello_num);
		hello_num++;
		sendto(socket_fd, &hello_num, sizeof(hello_num), 0,
		       (struct sockaddr *) &from, from_len);
		D("outer_daemon", "hello sent", "%zu\n", hello_num);
	}
}

_Noreturn void *inner_daemon() {
	D("inner_daemon", "start");
	int socket_fd = socket(AF_INET, SOCK_DGRAM, 0);
	if (socket_fd < 0) { pthread_exit(&SOCK_ERR); }
	D("inner_daemon", "socket created");

	unsigned int from_len;
	struct sockaddr_in addr = { 0 }, from;
	addr.sin_family = AF_INET;
	addr.sin_port = htons(INNER_PORT);
	addr.sin_addr.s_addr = INADDR_ANY;

	if (bind(socket_fd, (const struct sockaddr *) &addr, sizeof(addr)) < 0) {
		pthread_exit(&BIND_ERR);
	}
	D("inner_daemon", "socket binded");

	struct record record = { 0 };

	while (1) {
		size_t n = recvfrom(socket_fd, record.hostname, sizeof(record.hostname), 0,
			(struct sockaddr *) &from, &from_len);
		D("inner_daemon", "host received");

		record.hostname[n] = 0;

		int idx = find_familiar(record.hostname);
		if (idx < 0) {
			D("inner_daemon", "fam not found");

			if (fam_count ==MAX_FAMILIARS) {
				pthread_exit(&REC_OVERFLOW);
			}
			D("inner_daemon", "fam not found: no overflow");

			if (say_hello(record.hostname) == EXIT_SUCCESS) {
				D("inner_daemon", "successful hello");
				idx = fam_count;
				fam_count++;
			}
		}
		if (fill_record(idx, &record) < 0) { pthread_exit(&REC_OVERFLOW); }
		D("inner_daemon", "successful fill record", "%s:%zu\n", record.hostname, record.rtt_ms);

		sendto(socket_fd, &record, sizeof(record), 0,
		       (const struct sockaddr *) &from, from_len);
		D("inner_daemon", "response sended");
	}
}

int say_hello(char *hostname) {
	D("say_hello", "start");
	int sock_fd = socket(AF_INET, SOCK_DGRAM, 0);
	D("say_hello", "socket created");
	struct hostent *host = gethostbyname(hostname);
	if (host == NULL) pthread_exit(&RESOLVE_HOST);
	D("say_hello", "host resolved");

	unsigned int addr_len;
	struct sockaddr_in outer_addr = {};
	bcopy((char *) host->h_addr, (char *) &outer_addr.sin_addr.s_addr, host->h_length);
	outer_addr.sin_port = htons(OUTER_PORT);
	D("say_hello", "addr inited");


	size_t ping = rand(), pong = 0;
	struct timespec start, stop;

	clock_gettime(CLOCK_REALTIME, &start);
	sendto(sock_fd, &ping, sizeof(ping), 0,
		(const struct sockaddr *) &outer_addr, sizeof(outer_addr));
	D("say_hello", "ping sent");

	recvfrom(sock_fd, &pong, sizeof(pong), 0,
	         (struct sockaddr *) &outer_addr, &addr_len);
	D("say_hello", "pong received");

	clock_gettime(CLOCK_REALTIME, &stop);

	if (ping + 1 != pong) { return EXIT_FAILURE; }
	D("say_hello", "successful hello");

	strncpy(familiars[fam_count].hostname, hostname, RECORD_BUF);
	bcopy((char *) host->h_addr, (char *) familiars[fam_count].addr, host->h_length);
	familiars[fam_count].rtt_ms = floor(((double) stop.tv_sec - start.tv_sec) * 1e+3 +
		(double)(stop.tv_nsec - start.tv_nsec) * 1e-6);
	D("say_hello", "data copied");
	if (familiars[fam_count].rtt_ms < 1e-6) {
		familiars[fam_count].rtt_ms = 20.0;
	}

	return EXIT_SUCCESS;
}

int fill_record(int idx, struct record *record) {
	if (idx >= fam_count) return EXIT_FAILURE;
	memcpy(record, &familiars[idx], sizeof(struct record));
	return EXIT_SUCCESS;
}

int find_familiar(char *hostname) {
	for (int i = 0; i < fam_count; i++) {
		if (strcmp(hostname, familiars[i].hostname) == 0) {
			return i;
		}
	}
	return -1;
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
