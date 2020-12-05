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

#define DUMMY 0

//#define SERVERPORT "4950"	// the port users will be connecting to

int main(int argc, char *argv[])
{
	int sockfd;
	struct addrinfo hints, *servinfo, *p;
	int rv;
	int numbytes;

	if (argc != 2) {
		fprintf(stderr,"usage: talker portnumber\n");
		exit(1);
	}

	memset(&hints, 0, sizeof hints);
	hints.ai_family = AF_INET6; // set to AF_INET to use IPv4
	hints.ai_socktype = SOCK_DGRAM;

	if ((rv = getaddrinfo(NULL, argv[1], &hints, &servinfo)) != 0) {
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

		break;
	}

	if (p == NULL) {
		fprintf(stderr, "talker: failed to create socket\n");
		return 2;
	}

	struct timespec timer_test, tim;
	timer_test.tv_sec = 0;
  timer_test.tv_nsec = 0;

  struct can_frame frame;
  unsigned int mes_counter = 0;
	unsigned int dummy_counter = 0;
  unsigned char data0 = 0;
  unsigned char data1 = 61;
  unsigned char data2 = 133;

  while(1){
		//printf("%d\n", mes_counter);
    frame.can_id  = mes_counter;
    frame.can_dlc = 4;
    frame.data[0] = 0x65; //e - eth
    frame.data[1] = 0x63; //c - converter
		if (argv[1][3] == '0'){
			frame.data[2] = 0x30;
		}
		else{
			frame.data[2] = 0x31;
		}

		if (mes_counter < 100000)
		{
			frame.data[3] = 0x00;
		}
		else{
			frame.data[3] = 0xff;
		}


    if ((numbytes = sendto(sockfd, &frame, sizeof(struct can_frame), 0, p->ai_addr, p->ai_addrlen)) == -1) {
      perror("talker: sendto");
      exit(1);
    }
    mes_counter++;
    data0++;
    data1++;
    data2++;
    if (mes_counter > 4294967290) {mes_counter=0;}
    if (data0 >= 255) {data0=0;}
    if (data1 >= 255) {data1=0;}
    if (data2 >= 255) {data2=0;}

		for (int j=0; j<DUMMY; j++){
			dummy_counter = dummy_counter + 1;
		}
		dummy_counter = 0;
    //nanosleep(&timer_test, &tim);
  }

	freeaddrinfo(servinfo);

	printf("talker: sent %d bytes to %s\n", numbytes, argv[1]);
	close(sockfd);

	return 0;
}
