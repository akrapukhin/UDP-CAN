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
#define CYCLES 100000
#define UDP_CAN 0
#define CAN_UDP 1

//#define MAXBUFLEN 100

pthread_mutex_t lock;

struct link {
	int sock_rx;
	int sock_tx;
	int type;
	struct addrinfo *p;
};


int make_canudp_links(int can_sockets[], int size, struct link canudp_links[], const char *ip_address, const char *port_num){
    struct addrinfo hints, *servinfo, *p;
    int rv;
    int sock_descr;

    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_INET; // set to AF_INET to use IPv4
    hints.ai_socktype = SOCK_DGRAM;
    hints.ai_flags = AI_PASSIVE; // use my IP
    
    if ((rv = getaddrinfo(ip_address, port_num, &hints, &servinfo)) != 0) {
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
        exit(1);
    }
    
    // loop through all the results and bind to the first we can
    for(p = servinfo; p != NULL; p = p->ai_next) {
        printf("%d %d %d\n", p->ai_family, p->ai_socktype, p->ai_protocol);
        if ((sock_descr = socket(p->ai_family, p->ai_socktype, p->ai_protocol)) == -1) {
            perror("listener: socket");
            continue;
        }
        printf("socket %d created\n", sock_descr);

        break;
    }
    if (p == NULL) {
        fprintf(stderr, "listener: failed to bind socket\n");
        exit(1);
    }
    freeaddrinfo(servinfo);
    
    int i;
    for (i = 0; i < size; ++i) {
        canudp_links[i].sock_rx = can_sockets[i];
        canudp_links[i].sock_tx = sock_descr;
        canudp_links[i].type = CAN_UDP;
        canudp_links[i].p = p;
    }
    return 0;
}


// get sockaddr, IPv4 or IPv6:
void *get_in_addr(struct sockaddr *sa)
{
	if (sa->sa_family == AF_INET) {
		return &(((struct sockaddr_in*)sa)->sin_addr);
	}

	return &(((struct sockaddr_in6*)sa)->sin6_addr);
}



void *runSocket(void *sockinf)
{
	 int sock_in, sock_out, stype;
	 sock_in = ((struct link*)sockinf)->sock_rx;
	 sock_out = ((struct link*)sockinf)->sock_tx;
	 stype = ((struct link*)sockinf)->type;
	 struct addrinfo *p = ((struct link*)sockinf)->p;

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
	 printf("%d %d %d %d\n", sock_in, sock_out, stype, get_in_addr(p->ai_addr));

	 for(;;){
		 //printf("%d\n", sock_in);
		 if ((numbytes = recvfrom(sock_in, &frame_eth, sizeof(struct can_frame) , 0,
			 (struct sockaddr *)&their_addr, &addr_len)) == -1) {
			 //perror("recvfromm");
			 exit(1);
		 }
		 
		 //printf("%d\n", ((struct sockaddr_in*)&their_addr)->sin_addr.s_addr);
		 
		 

		 if (frame_eth.can_id - memo > 1)
		 {
			 errors++;
		 }
		 memo = frame_eth.can_id;
		 printf("%d %c %c %c\n", frame_eth.can_id, frame_eth.data[0], frame_eth.data[1], frame_eth.data[2]);
		 //printf("%d %d\n", counters, frame_eth.can_id);

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
			 
			 //pthread_mutex_lock (&lock);
			 nbytes = sendto(sock_out, &frame, sizeof(struct can_frame), 0, p->ai_addr, p->ai_addrlen);
			 //pthread_mutex_unlock (&lock);

			 if (nbytes == -1) {
				 //perror("talker: sendto1");
				 exit(1);
			 }
			 
			 if (nbytes < sizeof(struct can_frame)) {
				 perror("WAHT");
				 exit(1);
			 }
		 }

		 if (counters == 0){first_vals = frame_eth.can_id;}

		 counters++;
		 if (counters > CYCLES && results == 0.0){
			 results = (float)counters / (float)(frame_eth.can_id);// - first_vals);
		 }

		 if (results>0 && results_printed == 0){
			 printf("%d->%d %d %f\n", sock_in, sock_out, stype, results);
			 results_printed = 1;
			 //pthread_exit(NULL);
		 }
   }


}

int init_can_interface(const char *ifname){
    int sock_can_descr;
    struct ifreq ifr;
    struct sockaddr_can addr;
    
    printf("%d %d %d\n", PF_CAN, SOCK_RAW, CAN_RAW);
    if((sock_can_descr = socket(PF_CAN, SOCK_RAW, CAN_RAW)) == -1) {
        perror("Error while opening socket");
        exit(1);
    }
    printf("socket %d created\n", sock_can_descr);
	strcpy(ifr.ifr_name, ifname);
	ioctl(sock_can_descr, SIOCGIFINDEX, &ifr);
	addr.can_family  = AF_CAN;
	addr.can_ifindex = ifr.ifr_ifindex;
	printf("%s at index %d\n", ifname, ifr.ifr_ifindex);
	if(bind(sock_can_descr, (struct sockaddr *)&addr, sizeof(addr)) == -1) {
        perror("Error in socket bind");
		exit(1);
	}
    return sock_can_descr;
}


int main(void)
{
	pthread_mutex_init(&lock, NULL);

	int sockfd0, sockfd1;
	struct addrinfo hints, *servinfo, *p;
	int rv;


	//char buf[MAXBUFLEN];

	char s[INET6_ADDRSTRLEN];

	memset(&hints, 0, sizeof hints);
	hints.ai_family = AF_INET; // set to AF_INET to use IPv4
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


	freeaddrinfo(servinfo);


	// Start off with room for 5 connections
	// (We'll realloc as necessary)
	int fd_count = 4;
	int fd_size = 5;
	
	/////////
	//CAN sockets
	int sc0, sc1;
    sc0 = init_can_interface("vcan0");
    sc1 = init_can_interface("vcan1");
    int can_sockets[2] = {sc0, sc1};
    struct link canudp_links[2];
    make_canudp_links(can_sockets, 2, canudp_links, "192.168.1.6", ETHPORT);
	//////

	pthread_t threads[4];
  int rc;
  long t;

	struct link si0, si1, si2, si3;

	si0.sock_rx = sockfd0;
	si0.sock_tx = sc0;
	si0.type = 0;
	si0.p = p;

	si1.sock_rx = sockfd1;
	si1.sock_tx = sc1;
	si1.type = 0;
	si1.p = p;
	
	si2.sock_rx = canudp_links[0].sock_rx;
	si2.sock_tx = canudp_links[0].sock_tx;
	si2.type = canudp_links[0].type;
	si2.p = canudp_links[0].p;

	si3.sock_rx = canudp_links[1].sock_rx;
	si3.sock_tx = canudp_links[1].sock_tx;
	si3.type = canudp_links[1].type;
	si3.p = canudp_links[1].p;
	
	printf("eth 4950: %d\n", sockfd0);
	printf("eth 4951: %d\n", sockfd1);
	printf("bus 0: %d\n", sc0);
	printf("bus 1: %d\n", sc1);
	printf("eth 4952: %d\n", canudp_links[0].sock_tx);
	
	printf("thread %d %d %d\n", si0.sock_rx, si0.sock_tx, si0.type);
	printf("thread %d %d %d\n", si1.sock_rx, si1.sock_tx, si1.type);
	printf("thread %d %d %d\n", si2.sock_rx, si2.sock_tx, si2.type);
	printf("thread %d %d %d\n", si3.sock_rx, si3.sock_tx, si3.type);

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
	close(canudp_links[0].sock_tx);
  /* Last thing that main() should do */
  printf("all closed lol");
  pthread_exit(NULL);
  

	return 0;
}
