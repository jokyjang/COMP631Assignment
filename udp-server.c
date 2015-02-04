/* Sample UDP server */

#include <strings.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <netdb.h>
#include <zlib.h>

#define ACK_LENGTH 5
#define ACK_MARK 0xaa;
#define END_OF_MESSAGE "-END OF MESSAGE-\n"
#define MAX_BUFFER_SIZE (64<<10)

struct Datagram {
	unsigned int address;
	unsigned short port;
	unsigned int length;
	unsigned int crc32;
	char *msg;
};

unsigned int extractMessage(unsigned char *msg, struct Datagram *datagram) {
	memcpy(&datagram->address, msg, sizeof(int));
	msg += sizeof(unsigned int);
	memcpy(&datagram->port, msg, sizeof(unsigned short));
	msg += sizeof(unsigned short);
	memcpy(&datagram->length, msg, sizeof(unsigned int));
	msg += sizeof(unsigned int);
	memcpy(&datagram->crc32, msg, sizeof(unsigned int));
	msg += sizeof(unsigned int);
	datagram->msg = malloc(sizeof(char) * (datagram->length+1));
	memcpy(datagram->msg, msg, datagram->length);
	datagram->msg[datagram->length] = '\0';
	return 14 + datagram->length;
}

int main(int argc, char**argv) {
	int sockfd, n;
	struct sockaddr_in servaddr, cliaddr, ackaddr;
	socklen_t len = sizeof(cliaddr);
	unsigned char mesg[MAX_BUFFER_SIZE];
	unsigned char ack[ACK_LENGTH];
	unsigned int datagram_len;

	if (argc != 3) {
		fprintf(stderr, "Usage: %s <port> <outfile>\n", argv[0]);
		exit(1);
	}

	if ((sockfd = socket(AF_INET, SOCK_DGRAM, 0)) == -1) {
		fprintf(stderr, "error in building socket!\n");
		exit(1);
	}

	memset(&servaddr, 0, sizeof(servaddr));
	servaddr.sin_family = AF_INET;
	servaddr.sin_addr.s_addr = htonl(INADDR_ANY);
	servaddr.sin_port = htons(atoi(argv[1]));
	if (bind(sockfd, (struct sockaddr *) &servaddr, sizeof(servaddr)) == -1) {
		fprintf(stderr, "error in binding!\n");
		exit(1);
	}

	FILE *f = fopen(argv[2], "w");
	for (;;) {
		if (recvfrom(sockfd, mesg, MAX_BUFFER_SIZE, 0, (struct sockaddr *) &cliaddr, &len) == -1) {
			fprintf(stderr, "error in recvfrom!\n");
			exit(1);
		}
		struct Datagram datagram;
		datagram_len = extractMessage(mesg, &datagram);

		struct hostent *client = gethostbyaddr(&cliaddr.sin_addr, sizeof(cliaddr.sin_addr), AF_INET);

		memset(&ackaddr, 0, sizeof(ackaddr));
		ackaddr.sin_family = AF_INET;
		ackaddr.sin_addr.s_addr = datagram.address;
		ackaddr.sin_port = datagram.port;

		//printf("recv: %x, %x; ack: %x, %x\n",
		//		cliaddr.sin_addr.s_addr, cliaddr.sin_port, datagram.address, datagram.port);

		//struct hostent *host = gethostbyaddr(&cliaddr, datagram.address);
		ack[0] = ACK_MARK;
		memcpy(ack+1, &datagram_len, sizeof(datagram_len));
		sendto(sockfd, ack, ACK_LENGTH, 0, (struct sockaddr *) &ackaddr, sizeof(ackaddr));
		//mesg[n] = 0;
		//printf("Received the following:\n");
		//printf("%d\n", datagram.length);
		unsigned int crc32_value = crc32(0L, (unsigned char *)datagram.msg, datagram.length);

		if (crc32_value == datagram.crc32) {
			printf("Error free message received from %s, %d\n",
					client->h_name, cliaddr.sin_port);
		} else {
			printf("Error in message received from %s, %d\n",
					client->h_name, cliaddr.sin_port);
		}
		fprintf(f, "%s%s", datagram.msg, END_OF_MESSAGE);
		fflush(f);
		free(datagram.msg);
	}
	fclose(f);
}
