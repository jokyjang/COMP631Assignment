/* Sample UDP client */

#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <zlib.h>
#include <netdb.h>
//#include <time.h>
#include <sys/time.h>

#define MSG_LENGTH 512
#define ACK_LENGTH 512
#define END_OF_MESSAGE "-END OF MESSAGE-\n"

char *getNextMessage(FILE *f) {
	char *msg = NULL;
	char *line = NULL;
	size_t line_len, length, msg_len;

	while ((length = getline(&line, &line_len, f)) != -1) {
		// detect End of Message
		//printf("|%s|", line);
		if (!strcmp(line, END_OF_MESSAGE))
			break;
		if(msg == NULL) {
			msg = malloc(sizeof(char) * MSG_LENGTH);
			msg[0] = '\0';
			msg_len = MSG_LENGTH;
		}
		while(strlen(line)+strlen(msg) >= msg_len) {
			msg = realloc(msg, sizeof(char) * (2*msg_len));
			msg_len *= 2;
		}
		strcat(msg, line);
	}
	free(line);
	return msg;
}

int buildMessage(struct sockaddr_in *addr, char *msg1, char *msg, char **dg) {
	struct timeval start, end;
	char *ptr = malloc(sizeof(char) * (14+strlen(msg)));
	gettimeofday(&start, NULL);
	unsigned int crc32_value = crc32(0L, (unsigned char *)msg1, strlen(msg1));
	gettimeofday(&end, NULL);
	printf("crc_time\t%lu\t%lu\n", 14+strlen(msg),
					((end.tv_sec-start.tv_sec)*1000000 + end.tv_usec-start.tv_usec));
	int msg_len = strlen(msg);
	*dg = ptr;
	memcpy(ptr, &(addr->sin_addr.s_addr), sizeof(unsigned int));
	ptr += sizeof(unsigned int);
	memcpy(ptr, &(addr->sin_port), sizeof(short));
	ptr += sizeof(short);
	memcpy(ptr, &msg_len, sizeof(int));
	ptr += sizeof(int);
	memcpy(ptr, &crc32_value, sizeof(int));
	ptr += sizeof(int);
	memcpy(ptr, msg, strlen(msg));
	return 14+strlen(msg);
}

int main(int argc, char**argv) {

	int sockfd, ret;
	struct sockaddr_in servaddr,cliaddr;
	socklen_t cliaddr_len = sizeof(cliaddr);

	if (argc != 5) {
		fprintf(stderr, "Usage: %s <Server> <Port> <file1> <file2>\n", argv[0]);
		exit(1);
	}

	sockfd = socket(AF_INET, SOCK_DGRAM, 0);
	memset(&servaddr, 0, sizeof(servaddr));
	servaddr.sin_family = AF_INET;
	//servaddr.sin_addr.s_addr = inet_addr(argv[1]);
	struct hostent *host = gethostbyname(argv[1]);
	servaddr.sin_addr = *(struct in_addr *)host->h_addr;
	servaddr.sin_port = htons(atoi(argv[2]));

	if(connect(sockfd, (struct sockaddr *)&servaddr, sizeof(servaddr)) == -1) {
		fprintf(stderr, "error in connecting!\n");
		exit(1);
	}
	if(getsockname(sockfd, (struct sockaddr *)&cliaddr, &cliaddr_len) == -1) {
		fprintf(stderr, "error in getsockname!\n");
		exit(1);
	}

	FILE *f1 = fopen(argv[3], "r");
	FILE *f2 = fopen(argv[4], "r");
	char *msg1 = getNextMessage(f1);
	char *msg2 = getNextMessage(f2);
	char *datagram = NULL;
	char ack_msg[ACK_LENGTH];

	struct timeval start, end;
	while (msg1 != NULL && msg2 != NULL) {

		int dg_len = buildMessage(&cliaddr, msg1, msg2, &datagram);
		//printf("port number: %d\n", cliaddr.sin_port);
		//printf("%x\n", crc32_value);

		if (sendto(sockfd, datagram, dg_len, 0,
				(struct sockaddr *) &servaddr, sizeof(servaddr)) == -1) {
			fprintf(stderr, "error in sending!\n");
			exit(1);
		}
		gettimeofday(&start, NULL);
		if (recvfrom(sockfd, ack_msg, ACK_LENGTH, 0, NULL, NULL) == -1) {
			fprintf(stderr, "error in receiving!\n");
			exit(1);
		}
		gettimeofday(&end, NULL);
		//printf("successfully received!\n");

		printf("transmit_time\t%d\t%lu\n", dg_len,
				((end.tv_sec-start.tv_sec)*1000000 + end.tv_usec-start.tv_usec));

		free(msg1);
		free(msg2);
		free(datagram);
		msg1 = getNextMessage(f1);
		msg2 = getNextMessage(f2);
	}
}
