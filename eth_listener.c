/*
** talker.c -- a datagram "client" demo
*/

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <time.h>

#include <linux/can.h>
#include <linux/can/raw.h>

#define SERVERPORT "4952"	// the port users will be connecting to

int main(int argc, char *argv[])
{
	int sockfd;
	struct addrinfo hints, *servinfo, *p;
	int rv;
	int numbytes;


	memset(&hints, 0, sizeof hints);
	hints.ai_family = AF_INET6; // set to AF_INET to use IPv4
	hints.ai_socktype = SOCK_DGRAM;

	if ((rv = getaddrinfo(NULL, SERVERPORT, &hints, &servinfo)) != 0) {
		fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
		return 1;
	}

	// loop through all the results and make a socket
	for(p = servinfo; p != NULL; p = p->ai_next) {
		printf("%d\n", p->ai_family);
		if ((sockfd = socket(p->ai_family, p->ai_socktype,
				p->ai_protocol)) == -1) {
			perror("talker: socket");
			continue;
		}
    if (bind(sockfd, p->ai_addr, p->ai_addrlen) == -1) {
      close(sockfd);
      perror("listener: bind");
      continue;
    }

		break;
	}

	if (p == NULL) {
		fprintf(stderr, "talker: failed to create socket\n");
		return 2;
	}

  struct can_frame frame;

  unsigned int memo = 0;
  unsigned int errors = 0;

  while(1){
    if ((numbytes = recv(sockfd, &frame, sizeof(struct can_frame), 0)) == -1) {
      perror("talker: sendto");
      exit(1);
    }

    if (frame.can_id - memo > 1)
    {
      errors++;
      //printf("%d ooooooooooooooooooooooooooo buf: %d, memo: %d\n",i, *buf, memo[i]);
    }
    memo = frame.can_id;
    //printf("%d %d\n", frame.can_id, errors);
    printf("%d %c %c %c %d\n", frame.can_id, frame.data[0], frame.data[1], frame.data[2], errors);
	//nanosleep(&timer_test, &tim);
  }

	freeaddrinfo(servinfo);

	printf("talker: sent %d bytes to %s\n", numbytes, argv[1]);
	close(sockfd);

	return 0;
}
