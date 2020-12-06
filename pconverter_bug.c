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

#define MYPORT0 4950	// the port users will be connecting to
#define MYPORT1 4951
#define ETHPORT 4952
#define CYCLES 100000
#define UDP_CAN 0
#define CAN_UDP 1

//#define MAXBUFLEN 100

pthread_mutex_t lock;

struct link {
	int sock_rx;
	int sock_tx;
	int type;
	struct sockaddr_in addr;
};

// get sockaddr, IPv4 or IPv6:
void *get_in_addr(struct sockaddr *sa)
{
	if (sa->sa_family == AF_INET) {
		return &(((struct sockaddr_in*)sa)->sin_addr);
	}

	return &(((struct sockaddr_in6*)sa)->sin6_addr);
}


int make_canudp_links(int can_sockets[], int size, struct link canudp_links[], const char *ip_address, int port_num){
    struct sockaddr_in addr;
    int sock_descr;
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port_num);
    inet_pton(AF_INET, ip_address, &(addr.sin_addr));
	if ((sock_descr = socket(AF_INET, SOCK_DGRAM, 0)) == -1) {
		perror("listener: socket");
	}
    
    int i;
    for (i = 0; i < size; ++i) {
        canudp_links[i].sock_rx = can_sockets[i];
        canudp_links[i].sock_tx = sock_descr;
        canudp_links[i].type = CAN_UDP;
        canudp_links[i].addr = addr;
    }
    return 0;
}


void *runSocket(void *sockinf)
{
	 int sock_in, sock_out, stype;
	 sock_in = ((struct link*)sockinf)->sock_rx;
	 sock_out = ((struct link*)sockinf)->sock_tx;
	 stype = ((struct link*)sockinf)->type;
	 struct sockaddr_in addr = ((struct link*)sockinf)->addr;

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
	 //printf("%d %d %d %d\n", sock_in, sock_out, stype, get_in_addr(p->ai_addr));

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
			 nbytes = sendto(sock_out, &frame, sizeof(struct can_frame), 0, (struct sockaddr*)&addr, sizeof addr);
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
    printf("cansocket %d created\n", sock_can_descr);
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


struct link make_udpcan_link(int port_num, int can_socket){
    struct sockaddr_in addr;
    int sock_descr;
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port_num);
    addr.sin_addr.s_addr = INADDR_ANY; 
	if ((sock_descr = socket(AF_INET, SOCK_DGRAM, 0)) == -1) {
		perror("listener: socket");
	}
	if (bind(sock_descr, (struct sockaddr*)&addr, sizeof addr) == -1) {
		close(sock_descr);
		perror("listener: bind");
	}
    
    struct link res = {sock_descr, can_socket, UDP_CAN, addr};
    return res;
}


int main(void)
{
	pthread_mutex_init(&lock, NULL);

	int sockfd0, sockfd1;
	
	//CAN sockets
	int sc0, sc1;
    sc0 = init_can_interface("vcan0");
    sc1 = init_can_interface("vcan1");
    int can_sockets[2] = {sc0, sc1};
    struct link canudp_links[2];
    make_canudp_links(can_sockets, 2, canudp_links, "192.168.1.6", ETHPORT);
    
	struct link udpcan_links[2];
    udpcan_links[0] = make_udpcan_link(MYPORT0, 0);
    udpcan_links[1] = make_udpcan_link(MYPORT1, 0);
    sockfd0 = udpcan_links[0].sock_rx;
    sockfd1 = udpcan_links[1].sock_rx;


	struct link si0, si1, si2, si3;
	
	si0.sock_rx = sockfd0;
	si0.sock_tx = sc0;
	si0.type = 0;
	si0.addr = udpcan_links[0].addr;

	si1.sock_rx = sockfd1;
	si1.sock_tx = sc1;
	si1.type = 0;
	si1.addr = udpcan_links[0].addr;
	
	si2 = canudp_links[0];
	si3 = canudp_links[1];
	
	printf("eth 4950: %d\n", sockfd0);
	printf("eth 4951: %d\n", sockfd1);
	printf("bus 0: %d\n", sc0);
	printf("bus 1: %d\n", sc1);
	printf("eth 4952: %d\n", canudp_links[0].sock_tx);
	
	printf("thread %d %d %d\n", si0.sock_rx, si0.sock_tx, si0.type);
	printf("thread %d %d %d\n", si1.sock_rx, si1.sock_tx, si1.type);
	printf("thread %d %d %d\n", si2.sock_rx, si2.sock_tx, si2.type);
	printf("thread %d %d %d\n", si3.sock_rx, si3.sock_tx, si3.type);

	pthread_t threads[4];
    int rc;
    long t;
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
