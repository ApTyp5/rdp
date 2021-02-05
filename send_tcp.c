
#include <netdb.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
#include "config.h"

void func(int sockfd)
{
	char buff[BUFSIZ];
	int n;
	for (;;) {
		bzero(buff, sizeof(buff));
		printf("Enter the string : ");
		n = 0;
		while ((buff[n++] = getchar()) != '\n');
		write(sockfd, buff, sizeof(buff));
		bzero(buff, sizeof(buff));
		read(sockfd, buff, sizeof(buff));
		printf("From Server : %s", buff);
		if ((strncmp(buff, "exit", 4)) == 0) {
			printf("Client Exit...\n");
			break;
		}
	}
}

int main() {
	int sockfd;
	struct sockaddr_in servaddr;

	// socket create and varification
	sockfd = socket(AF_INET, SOCK_STREAM, 0);
	if (sockfd == -1) {
		printf("socket creation failed...\n");
		exit(0);
	}
	else
		printf("Socket successfully created..\n");
	bzero(&servaddr, sizeof(servaddr));

	// assign IP, PORT
	servaddr.sin_family = AF_INET;
	struct hostent *host = gethostbyname(HOST_NAME);
	bcopy((char *) host->h_addr, (char *) &servaddr.sin_addr.s_addr, host->h_length);
	servaddr.sin_port = htons(TCP_PORT);

	FILE *f = fopen(SENT_FILE, "rb");
	fseek(f, 0, SEEK_END);
	size_t fsize = ftell(f);

	fseek(f, 0, SEEK_SET);
	char *buf = malloc(fsize + 1);
	if (buf == 0) {
		fprintf(stderr, "bad alloc");
		exit(-1);
	}

	buf[fsize] = 0;
	fread(buf, 1, fsize, f);
	fclose(f);

	size_t len = fsize;

	// connect the client socket to server socket
	if (connect(sockfd, (const struct sockaddr *) &servaddr, sizeof(servaddr)) != 0) {
		printf("connection with the server failed...\n");
		exit(0);
	}
	else
		printf("connected to the server..\n");

	time_t start = clock();
	send(sockfd, buf, len, 0);
	time_t stop = clock();

	printf("time: %f\n", ((double)stop - start) / CLOCKS_PER_SEC);

	// close the socket
	close(sockfd);
}
