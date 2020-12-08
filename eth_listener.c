/*
** eth_listener.c -- a datagram ethernet listener
** all CAN interfaces send messages here through the udp-can converter
*/

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <stdbool.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <time.h>

#include <linux/can.h>
#include <linux/can/raw.h>

#define PORT "4952"	// port to listen
#define CYCLES 100000

int main(int argc, char *argv[])
{
	if (argc < 2) {
		fprintf(stderr,"usage: eth_listener num_can_interfaces [print]\n");
		exit(1);
	}

	int sockfd;
	struct addrinfo hints, *servinfo, *p;
	int rv;
	int numbytes;

	memset(&hints, 0, sizeof hints);
	hints.ai_family = AF_INET; // set to AF_INET to use IPv4
	hints.ai_socktype = SOCK_DGRAM;

	if ((rv = getaddrinfo(NULL, PORT, &hints, &servinfo)) != 0) {
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
        printf("P\n");
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

	int size = argv[1][0] - '0';

	//arrays necessary to compute results
	unsigned int memo[size];
	memset(memo, 0, size*sizeof(int));
	unsigned int errors[size];
	memset(errors, 0, size*sizeof(int));
	unsigned int counters[size];
	memset(counters, 0, size*sizeof(int));
	float results[size];
	memset(results, 0, size*sizeof(float));
	bool results_computed[size];
	memset(results_computed, false, size*sizeof(bool));
	bool all_results_ready = false;
	bool results_printed = false;

	printf("\n\n\n");

  while(1){
    if ((numbytes = recv(sockfd, &frame, sizeof(struct can_frame), 0)) == -1) {
      perror("talkerrrr: sendto");
      exit(1);
    }

		if (argc >= 3 && strcmp(argv[2], "print") == 0){
			printf("%d %c %c %d\n", frame.can_id, frame.data[0], frame.data[1], frame.data[2]);
		}

		int source = (int)frame.data[2];

    if (frame.can_id - memo[source] > 1)
    {
      errors[source]++;
    }
    memo[source] = frame.can_id;

		counters[source]++;

    if (counters[source] > CYCLES && !results_computed[source]){
      results[source] = (float)counters[source] / (float)(frame.can_id);
			results_computed[source] = true;
    }

		int r;
		all_results_ready = true;
		for (r=0; r < size; r++){
			if (!results_computed[r]){
				all_results_ready = false;
				break;
			}
		}
		if (all_results_ready && !results_printed){
			for (r = 0; r < size; r++){
				printf("%d->port %s %f %d\n", source, PORT, results[r], errors[r]);
			}
			results_printed = true;
		}
  }

	freeaddrinfo(servinfo);
	close(sockfd);
	return 0;
}
