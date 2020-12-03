/*
** listener.c -- a datagram sockets "server" demo
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
#include <limits.h>

#include <net/if.h>
#include <sys/ioctl.h>

#include <linux/can.h>
#include <linux/can/raw.h>

#include <poll.h>

#define MYPORT0 "4950"	// the port users will be connecting to
#define MYPORT1 "4951"
#define ETHPORT "4952"

//#define MAXBUFLEN 100

// get sockaddr, IPv4 or IPv6:
void *get_in_addr(struct sockaddr *sa)
{
	if (sa->sa_family == AF_INET) {
		return &(((struct sockaddr_in*)sa)->sin_addr);
	}

	return &(((struct sockaddr_in6*)sa)->sin6_addr);
}

int main(void)
{

	int sockfd0, sockfd1, sock_eth;
	struct addrinfo hints, *servinfo, *p;
	int rv;
	int numbytes;
	struct sockaddr_storage their_addr;
	//char buf[MAXBUFLEN];
	struct can_frame frame_eth;
	socklen_t addr_len;
	char s[INET6_ADDRSTRLEN];

	memset(&hints, 0, sizeof hints);
	hints.ai_family = AF_INET6; // set to AF_INET to use IPv4
	hints.ai_socktype = SOCK_DGRAM;
	hints.ai_flags = AI_PASSIVE; // use my IP

  //0
	if ((rv = getaddrinfo(NULL, MYPORT0, &hints, &servinfo)) != 0) {
		fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
		return 1;
	}
	// loop through all the results and bind to the first we can
	for(p = servinfo; p != NULL; p = p->ai_next) {
		if ((sockfd0 = socket(p->ai_family, p->ai_socktype,
				p->ai_protocol)) == -1) {
			perror("listener: socket");
			continue;
		}
		if (bind(sockfd0, p->ai_addr, p->ai_addrlen) == -1) {
			close(sockfd0);
			perror("listener: bind");
			continue;
		}

		break;
	}
	if (p == NULL) {
		fprintf(stderr, "listener: failed to bind socket\n");
		return 2;
	}

	//1
	if ((rv = getaddrinfo(NULL, MYPORT1, &hints, &servinfo)) != 0) {
		fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
		return 1;
	}
	// loop through all the results and bind to the first we can
	for(p = servinfo; p != NULL; p = p->ai_next) {
		if ((sockfd1 = socket(p->ai_family, p->ai_socktype,
				p->ai_protocol)) == -1) {
			perror("listener: socket");
			continue;
		}
		if (bind(sockfd1, p->ai_addr, p->ai_addrlen) == -1) {
			close(sockfd1);
			perror("listener: bind");
			continue;
		}

		break;
	}
	if (p == NULL) {
		fprintf(stderr, "listener: failed to bind socket\n");
		return 2;
	}

	//2
	if ((rv = getaddrinfo(NULL, ETHPORT, &hints, &servinfo)) != 0) {
		fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
		return 1;
	}
	// loop through all the results and bind to the first we can
	for(p = servinfo; p != NULL; p = p->ai_next) {
		if ((sock_eth = socket(p->ai_family, p->ai_socktype,
				p->ai_protocol)) == -1) {
			perror("listener: socket");
			continue;
		}


		break;
	}
	if (p == NULL) {
		fprintf(stderr, "listener: failed to bind socket\n");
		return 2;
	}

	freeaddrinfo(servinfo);

	printf("listener: waiting to recvfrom...\n");

	// Start off with room for 5 connections
	// (We'll realloc as necessary)
	int fd_count = 4;
	int fd_size = 5;
	struct pollfd *pfds = malloc(sizeof *pfds * fd_size);

	// Add the sockets to set
	pfds[0].fd = sockfd0;
	pfds[0].events = POLLIN;
	pfds[1].fd = sockfd1;
	pfds[1].events = POLLIN;

	addr_len = sizeof their_addr;


  //CAN sockets
	int sc0, sc1;
	int nbytes;
	struct sockaddr_can addr0, addr1;
	struct can_frame frame, frame_input;
	struct ifreq ifr0, ifr1;
	const char *ifname0 = "vcan0";
	const char *ifname1 = "vcan1";

	//0
	if((sc0 = socket(PF_CAN, SOCK_RAW, CAN_RAW)) == -1) {
			perror("Error while opening socket");
			return -1;
	}
	strcpy(ifr0.ifr_name, ifname0);
	ioctl(sc0, SIOCGIFINDEX, &ifr0);
	addr0.can_family  = AF_CAN;
	addr0.can_ifindex = ifr0.ifr_ifindex;
	printf("%s at index %d\n", ifname0, ifr0.ifr_ifindex);
	if(bind(sc0, (struct sockaddr *)&addr0, sizeof(addr0)) == -1) {
			perror("Error in socket bind");
			return -2;
	}

	//1
	if((sc1 = socket(PF_CAN, SOCK_RAW, CAN_RAW)) == -1) {
			perror("Error while opening socket");
			return -1;
	}
	strcpy(ifr1.ifr_name, ifname1);
	ioctl(sc1, SIOCGIFINDEX, &ifr1);
	addr1.can_family  = AF_CAN;
	addr1.can_ifindex = ifr1.ifr_ifindex;
	printf("%s at index %d\n", ifname1, ifr1.ifr_ifindex);
	if(bind(sc1, (struct sockaddr *)&addr1, sizeof(addr1)) == -1) {
			perror("Error in socket bind");
			return -2;
	}

	int can_sockets[2];
	unsigned int memo[4];
	unsigned int errors[4];
	memo[0] = 0;
	memo[1] = 0;
	memo[2] = 0;
	memo[3] = 0;
	errors[0] = 0;
	errors[1] = 0;
	errors[2] = 0;
	errors[3] = 0;

	can_sockets[0] = sc0;
	can_sockets[1] = sc1;

	pfds[2].fd = sc0;
	pfds[2].events = POLLIN;
	pfds[3].fd = sc1;
	pfds[3].events = POLLIN;

	//////////////// POLL LOOP ///////////////////////////////
	// Main loop
	for(;;) {
			int poll_count = poll(pfds, fd_count, -1);

			if (poll_count == -1) {
					perror("poll");
					exit(1);
			}

			// Run through the existing connections looking for data to read
			for(int i = 0; i < fd_count; i++) {
					// Check if someone's ready to read
					if (pfds[i].revents & POLLIN) { // We got one!!
						// If not the listener, we're just a regular client
						if ((numbytes = recvfrom(pfds[i].fd, &frame_eth, sizeof(struct can_frame) , 0,
							(struct sockaddr *)&their_addr, &addr_len)) == -1) {
							perror("recvfrom");
							exit(1);
						}

						//printf("listener: got packet from %s\n",
						//	inet_ntop(their_addr.ss_family,
						//		get_in_addr((struct sockaddr *)&their_addr),
						//		s, sizeof s));
						//printf("listener: packet is %d bytes long\n", numbytes);
						//buf[numbytes] = '\0';

						if (frame_eth.can_id - memo[i] > 1)
						{
							errors[i]++;
						  //printf("%d ooooooooooooooooooooooooooo buf: %d, memo: %d\n",i, *buf, memo[i]);
						}
						memo[i] = frame_eth.can_id;
						printf("%d %d %c %c %c %d %d %d %d\n", i, frame_eth.can_id, frame_eth.data[0], frame_eth.data[1], frame_eth.data[2], errors[0], errors[1], errors[2], errors[3]);
						//printf("%d %d %c %c %c\n", i, frame_eth.can_id, frame_eth.data[0], frame_eth.data[1], frame_eth.data[2]);

            if (i<=1)
						{
							frame.can_id  = frame_eth.can_id;
							frame.can_dlc = 3;
							frame.data[0] = 0x63; //c - converter
							frame.data[1] = 0x62; //b - bus
							if (i == 0){
								frame.data[2] = 0x30; //vcan0
							}
							else{
								frame.data[2] = 0x31; //vcan1
							}

							//nbytes = write(s, &frame, sizeof(struct can_frame));
							nbytes = send(can_sockets[i], &frame, sizeof(struct can_frame), 0);
							//printf("Wrote %d bytes, out of %lu bytes\n", nbytes, sizeof(struct can_frame));
						}
						else
						{
							frame.can_id  = frame_eth.can_id;
							frame.can_dlc = 3;
							frame.data[0] = 0x63; //c - converter
							frame.data[1] = 0x65; //e - ethernet
							if (i == 2){
								frame.data[2] = 0x30; //vcan0
							}
							else{
								frame.data[2] = 0x31; //vcan1
							}

							if ((nbytes = sendto(sock_eth, &frame, sizeof(struct can_frame), 0, p->ai_addr, p->ai_addrlen)) == -1) {
								perror("talker: sendto1");
								exit(1);
							}
							//printf("%d %d %d %d\n", frame_eth.can_id, frame_eth.data[0], frame_eth.data[1], frame_eth.data[2]);
						}


					} // END got ready-to-read from poll()
			} // END looping through file descriptors
	} // END for(;;)--and you thought it would never end!
	//////////////// POLL LOOP ///////////////////////////////

	close(sockfd0);
	close(sockfd1);
	close(sc0);
	close(sc1);

	return 0;
}
