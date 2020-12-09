/*
** eth_listener.c - ethernet listener for testing
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

#define PORT "4952"	//port to listen
#define CYCLES 100000 //for testing (number of messages to receive before computing results)

int main(int argc, char *argv[])
{
	if (argc < 2) {
		fprintf(stderr,"usage: eth_listener num_can_interfaces [print]\n");
		exit(1);
	}

  //create socket to receive messages
	int sockfd;
	struct addrinfo hints, *servinfo, *p;
	int rv;
	int numbytes;
	memset(&hints, 0, sizeof hints);
	hints.ai_family = AF_INET;
	hints.ai_socktype = SOCK_DGRAM;

	if ((rv = getaddrinfo(NULL, PORT, &hints, &servinfo)) != 0) {
		fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
		return 1;
	}

	for(p = servinfo; p != NULL; p = p->ai_next) {
		if ((sockfd = socket(p->ai_family, p->ai_socktype,
				p->ai_protocol)) == -1) {
			perror("eth_listener: socket");
			continue;
		}
    if (bind(sockfd, p->ai_addr, p->ai_addrlen) == -1) {
      close(sockfd);
      perror("eth_listener: bind");
      continue;
    }

		break;
	}

	if (p == NULL) {
		fprintf(stderr, "eth_listener: failed to create socket\n");
		return 1;
	}

  struct can_frame frame;

	int size = argv[1][0] - '0';

	//arrays necessary to compute results
	unsigned int memo[size];
	memset(memo, 0, size * sizeof(int));
	unsigned int errors[size];
	memset(errors, 0, size * sizeof(int));
	unsigned int counters[size];
	memset(counters, 0, size * sizeof(int));
	float results[size];
	memset(results, 0, size * sizeof(float));
	bool results_computed[size];
	memset(results_computed, false, size * sizeof(bool));
	bool all_results_ready = false;
	bool results_printed = false;

	printf("\n\n\n");

  while(1){
    //receive message
    if ((numbytes = recv(sockfd, &frame, sizeof(struct can_frame), 0)) == -1) {
      perror("eth_listener: recv");
      exit(1);
    }
    
    //print received messages for demo
		if (argc >= 3 && strcmp(argv[2], "print") == 0){
			printf("%d %c %c %d\n", frame.can_id, frame.data[0], frame.data[1], frame.data[2]);
		}

    //which CAN interface this message from
		int source = (int)frame.data[2];

    //if some frames are missed, increment errors
    if (frame.can_id - memo[source] > 1)
    {
      errors[source]++;
    }
    memo[source] = frame.can_id;

    //number of messages received
		counters[source]++;

    //compute results if there are enough messages received
    if (counters[source] > CYCLES && !results_computed[source]) {
      results[source] = (float)counters[source] / (float)(frame.can_id);
			results_computed[source] = true;
    }

    //print results if they are ready
		int r;
		all_results_ready = true;
		for (r=0; r < size; r++) {
			if (!results_computed[r]) {
				all_results_ready = false;
				break;
			}
		}
		if (all_results_ready && !results_printed) {
                        printf("fraction of messages received:\n");
			for (r = 0; r < size; r++) {
				printf("vcan%d->port%s %f\n", r, PORT, results[r]);
			}
			results_printed = true;
		}
  }

	freeaddrinfo(servinfo);
	close(sockfd);
	return 0;
}
