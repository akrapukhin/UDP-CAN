/*
** eth_talker.c -- a datagram ethernet talker
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

int main(int argc, char *argv[])
{
	if (argc < 2) {
		fprintf(stderr,"usage: eth_talker portnumber [print wait s ns]\n");
		exit(1);
	}

  //create socket to send messages
	int sockfd;
	struct addrinfo hints, *servinfo, *p;
	int rv;
	int numbytes;
	memset(&hints, 0, sizeof hints);
	hints.ai_family = AF_INET; // set to AF_INET to use IPv4
	hints.ai_socktype = SOCK_DGRAM;
	hints.ai_flags = AI_PASSIVE; // use my IP
	if ((rv = getaddrinfo(NULL, argv[1], &hints, &servinfo)) != 0) {
		fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
		return 1;
	}

	// loop through all the results and make a socket
	for(p = servinfo; p != NULL; p = p->ai_next) {
		printf("%d\n", p->ai_family);
		if ((sockfd = socket(p->ai_family, p->ai_socktype,
			p->ai_protocol)) == -1) {
				perror("eth_talker: socket");
				continue;
			}

			break;
	}
	if (p == NULL) {
		fprintf(stderr, "eth_talker: failed to create socket\n");
		return 2;
	}

		//wait between transmissions
		struct timespec timer_test, tim;
		timer_test.tv_sec = 0;
		timer_test.tv_nsec = 0;
		if(argc == 6 && strcmp(argv[3], "wait")){
			timer_test.tv_sec = argv[4][0] - '0';
			timer_test.tv_nsec = argv[5][0] - '0';
			printf("wait each cycle for %lds %ldns\n", timer_test.tv_sec, timer_test.tv_nsec);
		}

		struct can_frame frame;
		unsigned int mes_counter = 0;

		while(1){
			//can_id is incremented in each message
			frame.can_id  = mes_counter;
			frame.can_dlc = 4; //3 bytes sent in data field
			frame.data[0] = 0x65; //e - eth (source)
			frame.data[1] = 0x63; //c - converter (destination)
			frame.data[2] = argv[1][3] - '0'; //device num to which the message is sent [0-9]
			frame.data[3] = 0;

			//print sent messages for demo
			if (argc >= 3 && strcmp(argv[2], "print") == 0){
				printf("%d %c %c %d\n", frame.can_id, frame.data[0], frame.data[1], frame.data[2]);
				frame.data[3] = 1;
			}

			//send
			if ((numbytes = sendto(sockfd, &frame, sizeof(struct can_frame), 0, p->ai_addr, p->ai_addrlen)) == -1) {
				perror("talker: sendto");
				exit(1);
			}

			//increment counter
			mes_counter++;
			if (mes_counter > 4294967290) {mes_counter=0;}

			//wait if necessary
			if (argc == 6 && strcmp(argv[3], "wait") == 0){
				nanosleep(&timer_test, &tim);
			}
		}

		freeaddrinfo(servinfo);
		close(sockfd);
		return 0;
	}
