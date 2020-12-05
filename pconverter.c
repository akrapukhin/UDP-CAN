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

#include <pthread.h>

#define MYPORT0 "4950"	// the port users will be connecting to
#define MYPORT1 "4951"
#define ETHPORT "4952"
#define CYCLES 1000000

//#define MAXBUFLEN 100

struct socket_info {
	int in_id;
	int out_id;
	int sock_type;
	struct addrinfo *p;
};

void *runSocket(void *sockinf)
{
	 int sock_in, sock_out, stype;
	 sock_in = ((struct socket_info*)sockinf)->in_id;
	 sock_out = ((struct socket_info*)sockinf)->out_id;
	 stype = ((struct socket_info*)sockinf)->sock_type;
	 struct addrinfo *p = ((struct socket_info*)sockinf)->p;

	 struct can_frame frame_eth, frame;
	 struct sockaddr_storage their_addr;
	 socklen_t addr_len;
	 addr_len = sizeof their_addr;
	 unsigned int memo = 0;
	 unsigned int errors = 0;
	 unsigned int counters = 0;
	 unsigned int first_vals = 0;
	 float results = 0;
	 int results_printed = 0;
	 int numbytes;
	 int nbytes;
	 
	 //printf("%d %d %d\n", sock_in, sock_out, stype);

	 for(;;){
		 //printf("%d\n", sock_in);
		 if ((numbytes = recvfrom(sock_in, &frame_eth, sizeof(struct can_frame) , 0,
			 (struct sockaddr *)&their_addr, &addr_len)) == -1) {
			 //perror("recvfromm");
			 exit(1);
		 }

		 if (frame_eth.can_id - memo > 1)
		 {
			 errors++;
		 }
		 memo = frame_eth.can_id;
		 printf("%d %c %c %c %d\n", frame_eth.can_id, frame_eth.data[0], frame_eth.data[1], frame_eth.data[2], errors);

			if (stype==0)
		 {
			 frame.can_id  = frame_eth.can_id;
			 frame.can_dlc = 3;
			 frame.data[0] = 0x63; //c - converter
			 frame.data[1] = 0x62; //b - bus
			 frame.data[2] = frame_eth.data[2];
			 nbytes = send(sock_out, &frame, sizeof(struct can_frame), 0);
		 }
		 else
		 {
			 frame.can_id  = frame_eth.can_id;
			 frame.can_dlc = 3;
			 frame.data[0] = 0x63; //c - converter
			 frame.data[1] = 0x65; //e - ethernet
			 frame.data[2] = frame_eth.data[2];

			 if ((nbytes = sendto(sock_out, &frame, sizeof(struct can_frame), 0, p->ai_addr, p->ai_addrlen)) == -1) {
				 //perror("talker: sendto1");
				 exit(1);
			 }
		 }

		 if (counters == 0){first_vals = frame_eth.can_id;}

		 counters++;
		 if (counters > CYCLES && results == 0.0){
			 results = (float)counters / (float)(frame_eth.can_id);// - first_vals);
		 }

		 if (results>0){
			 //printf("STRANGE %d->%d %d %f\n", sock_in, sock_out, stype, results);
			 pthread_exit(NULL);
		 }
   }


}

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


	//char buf[MAXBUFLEN];

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


	// Start off with room for 5 connections
	// (We'll realloc as necessary)
	int fd_count = 4;
	int fd_size = 5;


  //CAN sockets
	int sc0, sc1;
	struct sockaddr_can addr0, addr1;
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

	pthread_t threads[4];
  int rc;
  long t;

	struct socket_info si0, si1, si2, si3;

	si0.in_id = sockfd0;
	si0.out_id = sc0;
	si0.sock_type = 0;
	si0.p = p;

	si1.in_id = sockfd1;
	si1.out_id = sc1;
	si1.sock_type = 0;
	si1.p = p;

	si2.in_id = sc0;
	si2.out_id = sock_eth;
	si2.sock_type = 1;
	si2.p = p;

	si3.in_id = sc1;
	si3.out_id = sock_eth;
	si3.sock_type = 1;
	si3.p = p;

	pthread_create(&threads[0], NULL, runSocket, &si0);
	pthread_create(&threads[1], NULL, runSocket, &si1);
	pthread_create(&threads[2], NULL, runSocket, &si2);
	pthread_create(&threads[3], NULL, runSocket, &si3);

	//if (rc){
	//	printf("ERROR; return code from pthread_create() is %d\n", rc);
	//	exit(-1);
	//}
	
	/// wait all threads by joining them
	for (int i = 0; i < 4; i++) {
		pthread_join(threads[i], NULL);
	}
	
	close(sockfd0);
	close(sockfd1);
	close(sc0);
	close(sc1);
	close(sock_eth);
  /* Last thing that main() should do */
  printf("all closed lol");
  pthread_exit(NULL);
  

	return 0;
}
