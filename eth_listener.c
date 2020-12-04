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
#define CYCLES 1000000

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
  unsigned int counters[2] = {0};
	unsigned int first_vals[2] = {0};
  float results[2] = {0};

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

	//nanosleep(&timer_test, &tim);

    if (frame.data[2] == 0x30){
			if (counters[0] == 0){first_vals[0] = frame.can_id;}
      counters[0]++;
    }
    else if (frame.data[2] == 0x31){
			if (counters[1] == 0){first_vals[1] = frame.can_id;}
      counters[1]++;
    }
    else{
      printf("WHICH BUS?\n");
    }

		///printf("%d %c %c %c %d %d\n", frame.can_id, frame.data[0], frame.data[1], frame.data[2], errors, counters[0]);

    if (counters[0] > CYCLES && frame.data[2] == 0x30 && results[0] == 0.0){
      results[0] = (float)counters[0] / (float)(frame.can_id - first_vals[0]);
    }

    if (counters[1] > CYCLES && frame.data[2] == 0x31 && results[1] == 0.0){
      results[1] = (float)counters[1] / (float)(frame.can_id - first_vals[1]);
    }

    if (results[0]>0 && results[1]>0){
      printf("eth_listeners:\n");
      printf("vcan0-ethernet %f\n", results[0]);
      printf("vcan1-ethernet %f\n", results[1]);
      exit(0);
    }

  }

	freeaddrinfo(servinfo);

	printf("talker: sent %d bytes to %s\n", numbytes, argv[1]);
	close(sockfd);

	return 0;
}
