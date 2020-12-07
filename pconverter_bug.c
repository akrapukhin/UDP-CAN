/*
** converter.c -- udp-can converter
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

#include <net/if.h>
#include <sys/ioctl.h>

#include <linux/can.h>
#include <linux/can/raw.h>

#include <pthread.h>
#include <poll.h>

//one port for each device (define new ports here to add new devices)
#define MYPORT0 4950
#define MYPORT1 4951

//data from all CAN interfaces is sent to this ip address and port
#define IPADDR "127.0.0.1" //loopback for testing #define IPADDR "192.168.1.6"
#define ETHPORT 4952

//type of connection (from Ethernet to CAN or vice versa)
#define UDP_CAN 0
#define CAN_UDP 1

//number of CAN interfaces
#define NUM_CAN 2

//number of devices connected to CAN interfaces
#define NUM_DEV 2

//for testing
#define CYCLES 100000

/*
** Each device has an individual port and a udp socket listening to this port.
** Data read from this socket is written to a CAN socket corresponding to a
** specific CAN interface to which the device is connected to. This relationship
** is captured by this "struct link".

** When data flows from CAN to Ethernet, the process is similar. A corresponding
** CAN socket is listening to a specific CAN interface. Data is then written to
** a UDP socket and then sent to a specified address (ip and port)

** Both of these relationships are captured by the "struct link" defined below.

** sock_rx - socket receiving data from Ethernet or from CAN
** sock_tx - socket sending data to Ethernet or to CAN
** type - UDP_CAN or CAN_UDP - from Ethernet to CAN or vice versa
** addr - destination address (ip and port for canudp links) or receiving
**        address (for udpcan links)
*/
struct link {
	int sock_rx;
	int sock_tx;
	int type;
	struct sockaddr_in addr;
};


/*
** Creates all canudp links
** Data from all CAN interfaces is sent to a single address, so the sock_tx
** socket in all corresponding links is the same/

** can_sockets[] - array of socket descriptors, one for each CAN interface
** size - size of the array
** canudp_links[] - array of links which is filled by the function
** ip_address - destination ip address
** port_num - specific port on the destination address
*/
int make_canudp_links(int can_sockets[], int size, struct link canudp_links[], const char *ip_address, int port_num){
	//ip address and port
	struct sockaddr_in addr;
	int sock_descr;
	addr.sin_family = AF_INET; //AF_INET6 for ipv6
	addr.sin_port = htons(port_num); //host-to-network (if host is little endian)
	inet_pton(AF_INET, ip_address, &(addr.sin_addr));
	if ((sock_descr = socket(AF_INET, SOCK_DGRAM, 0)) == -1) {
		perror("listener: socket");
	}

	//fill in all links
	int i;
	for (i = 0; i < size; ++i) {
		canudp_links[i].sock_rx = can_sockets[i];
		canudp_links[i].sock_tx = sock_descr;
		canudp_links[i].type = CAN_UDP;
		canudp_links[i].addr = addr;
	}
	return 0;
}


/*
** each link is processed by a separate pthread
** link_ptr - pointer to struct link
*/
void *run_link(void *link_ptr)
{
	//data from link
	int sock_in = ((struct link*)link_ptr)->sock_rx;
	int sock_out = ((struct link*)link_ptr)->sock_tx;
	int stype = ((struct link*)link_ptr)->type;
	struct sockaddr_in addr = ((struct link*)link_ptr)->addr;

	struct can_frame frame_received, frame_tosend;

	//address of a sender filled by recvfrom()
	struct sockaddr_storage their_addr;
	socklen_t addr_len;
	addr_len = sizeof their_addr;

	//number of bytes sent/received. If it's not equal to sizeof(struct can_frame),
	//the packet can be sent again (not implemented here)
	int numbytes;

	//for testing
	unsigned int memo = 0;
	unsigned int errors = 0;
	unsigned int counters = 0;
	float results = 0;
	int results_printed = 0;

	for(;;){

		//receive data
		if ((numbytes = recvfrom(sock_in, &frame_received, sizeof(struct can_frame) , 0,
		(struct sockaddr *)&their_addr, &addr_len)) == -1) {
			perror("recvfrom");
			exit(1);
		}

		//for testing
		if (frame_received.can_id - memo > 1){
			errors++;
		}
		memo = frame_received.can_id;
		//printf("%d %c %c %c\n", frame_received.can_id, frame_received.data[0], frame_received.data[1], frame_received.data[2]);

		//some processing can be done with the received data. Here source of the
		//frame is specified in frame_tosend.data[0] and destination in
		//frame_tosend.data[0]. data[2] and can_id are taken from the frame_received
		//and left unchanged. This is done for testing purposes.
		if (stype==UDP_CAN){
			frame_tosend.can_id  = frame_received.can_id;
			frame_tosend.can_dlc = 3;
			frame_tosend.data[0] = 0x63; //c - converter
			frame_tosend.data[1] = 0x62; //b - bus
			frame_tosend.data[2] = frame_received.data[2];
			numbytes = send(sock_out, &frame_tosend, sizeof(struct can_frame), 0);
		}
		else if (stype==CAN_UDP){
			frame_tosend.can_id  = frame_received.can_id;
			frame_tosend.can_dlc = 3;
			frame_tosend.data[0] = 0x63; //c - converter
			frame_tosend.data[1] = 0x65; //e - ethernet
			frame_tosend.data[2] = frame_received.data[2];

			//pthread_mutex_lock (&lock);
			numbytes = sendto(sock_out, &frame_tosend, sizeof(struct can_frame), 0, (struct sockaddr*)&addr, sizeof addr);
			//pthread_mutex_unlock (&lock);

			if (numbytes == -1) {
				//perror("talker: sendto1");
				exit(1);
			}

			if (numbytes < sizeof(struct can_frame)) {
				perror("WAHT");
				exit(1);
			}
		}
		else{
			perror("unknown link type");
			exit(1);
		}

		//for testing
		counters++;
		if (counters > CYCLES && results == 0.0){
			results = (float)counters / (float)(frame_received.can_id);
		}
		if (results>0 && results_printed == 0){
			printf("%d->%d %d %f\n", sock_in, sock_out, stype, results);
			results_printed = 1;
		}
	}
	pthread_exit(NULL);
}




int init_can_interface(const char *ifname){
	int sock_can_descr;
	struct ifreq ifr;
	struct sockaddr_can addr;

	if((sock_can_descr = socket(PF_CAN, SOCK_RAW, CAN_RAW)) == -1) {
		perror("Error while opening socket");
		exit(1);
	}
	strcpy(ifr.ifr_name, ifname);
	ioctl(sock_can_descr, SIOCGIFINDEX, &ifr);
	addr.can_family  = AF_CAN;
	addr.can_ifindex = ifr.ifr_ifindex;
	if(bind(sock_can_descr, (struct sockaddr *)&addr, sizeof(addr)) == -1) {
		perror("Error in socket bind");
		exit(1);
	}
	printf("CAN socket %d is bound to CAN interface %s at index %d\n", sock_can_descr, ifname, ifr.ifr_ifindex);
	return sock_can_descr;
}


/*
** Creates a udpcan link between a port and a CAN interface for each device
** Each device has a separate unique port. The link is made between this port
** and a CAN socket corresponding to the CAN interface to which the device is
** connected to. This CAN socket may be not unique because multiple devices can
** be connected to the same CAN bus.
**
** port_num - port dedicated to the device
** can_socket - CAN socket corresponding to the CAN interface
*/
struct link make_udpcan_link(int port_num, int can_socket){
	//fill in address
	struct sockaddr_in addr;
	int sock_descr;
	addr.sin_family = AF_INET;
	addr.sin_port = htons(port_num);
	addr.sin_addr.s_addr = INADDR_ANY; //set to the host ip
	if ((sock_descr = socket(AF_INET, SOCK_DGRAM, 0)) == -1) {
		perror("listener: socket");
	}

	//bind socket to port
	if (bind(sock_descr, (struct sockaddr*)&addr, sizeof addr) == -1) {
		close(sock_descr);
		perror("listener: bind");
	}

	//create link
	struct link res = {sock_descr, can_socket, UDP_CAN, addr};
	return res;
}

void unite_links(struct link canudp_links[], struct link udpcan_links[], struct link united_links[], int canudp_size, int udpcan_size){
	int i;
	for (i=0; i<udpcan_size; ++i){
		united_links[i] = udpcan_links[i];
	}
	for (i=0; i<canudp_size; ++i){
		united_links[udpcan_size + i] = canudp_links[i];
	}
}

void print_links(struct link links[], int size){
	char str[INET_ADDRSTRLEN];
	int i;
	printf("\nLinks:\n");
	for (i=0; i<size; ++i){
		if (links[i].type == UDP_CAN){
			printf("Ethernet -> UDP socket %d (port %d) -> CAN socket %d -> CAN interface\n",
			       links[i].sock_rx, htons(links[i].addr.sin_port), links[i].sock_tx);
		}
		else{
			printf("CAN interface -> CAN socket %d -> UDP socket %d -> Ethernet (ip %s, port %d)\n",
			       links[i].sock_rx, links[i].sock_tx, inet_ntop(AF_INET, &(links[i].addr.sin_addr),
						 str, INET_ADDRSTRLEN), htons(links[i].addr.sin_port));
		}
	}
	printf("\n");
}

void close_all_sockets(struct link links[], int size){
	int i;
	int sock_tx_udp_open = 1;
	for (i=0; i<size; ++i){
		if (links[i].type == UDP_CAN){
			close(links[i].sock_rx); //close UDP sockets corresponding to devices
		}
		else{
			close(links[i].sock_rx); //close CAN sockets corresponding to CAN interfaces
			//close UDP socket responsible to sending data outside only once
			if (sock_tx_udp_open){
				close(links[i].sock_tx);
				sock_tx_udp_open = 0;
			}
		}
	}
}

void poll_links(struct link links[], int size){
	//array of file descriptors to poll
	struct pollfd *pfds = malloc(sizeof *pfds * size);

	//fill in array
	int i;
	for (i=0; i<size; ++i){
		pfds[i].fd = links[i].sock_rx;
		pfds[i].events = POLLIN;
	}

	struct can_frame frame_received, frame_tosend;

	//address of a sender filled by recvfrom()
	struct sockaddr_storage their_addr;
	socklen_t addr_len;
	addr_len = sizeof their_addr;

	//number of bytes sent/received. If it's not equal to sizeof(struct can_frame),
	//the packet can be sent again (not implemented here)
	int numbytes;

	//for testing
	unsigned int memo[size];
	unsigned int errors[size];
	unsigned int counters[size];
	float results[size];
	int results_ready = 0;
	int results_printed = 0;

	for(;;) {
		int poll_count = poll(pfds, size, -1);

		if (poll_count == -1) {
			perror("poll");
			exit(1);
		}

		// Run through the existing connections looking for data to read
		for(int i = 0; i < size; i++) {
			// Check if someone's ready to read
			if (pfds[i].revents & POLLIN) { //data available for read from pfds[i]
				//receive data
				if ((numbytes = recvfrom(pfds[i].fd, &frame_received, sizeof(struct can_frame) , 0,
				(struct sockaddr *)&their_addr, &addr_len)) == -1) {
					perror("recvfrom");
					exit(1);
				}

				//for testing
				if (frame_received.can_id - memo[i] > 1){
					errors[i]++;
				}
				memo[i] = frame_received.can_id;
				//printf("%d %c %c %c\n", frame_received.can_id, frame_received.data[0], frame_received.data[1], frame_received.data[2]);

				//some processing can be done with the received data. Here source of the
				//frame is specified in frame_tosend.data[0] and destination in
				//frame_tosend.data[0]. data[2] and can_id are taken from the frame_received
				//and left unchanged. This is done for testing purposes.
				if (links[i].type==UDP_CAN){
					frame_tosend.can_id  = frame_received.can_id;
					frame_tosend.can_dlc = 3;
					frame_tosend.data[0] = 0x63; //c - converter
					frame_tosend.data[1] = 0x62; //b - bus
					frame_tosend.data[2] = frame_received.data[2];
					numbytes = send(links[i].sock_tx, &frame_tosend, sizeof(struct can_frame), 0);
				}
				else if (links[i].type==CAN_UDP){
					frame_tosend.can_id  = frame_received.can_id;
					frame_tosend.can_dlc = 3;
					frame_tosend.data[0] = 0x63; //c - converter
					frame_tosend.data[1] = 0x65; //e - ethernet
					frame_tosend.data[2] = frame_received.data[2];

					//pthread_mutex_lock (&lock);
					numbytes = sendto(links[i].sock_tx, &frame_tosend, sizeof(struct can_frame), 0, (struct sockaddr*)&links[i].addr, sizeof links[i].addr);
					//pthread_mutex_unlock (&lock);

					if (numbytes == -1) {
						//perror("talker: sendto1");
						exit(1);
					}

					if (numbytes < sizeof(struct can_frame)) {
						perror("WAHT");
						exit(1);
					}
				}
				else{
					perror("unknown link type");
					exit(1);
				}

				//for testing
				counters[i]++;
				if (counters[i] > CYCLES && results[i] == 0.0){
					results[i] = (float)counters[i] / (float)(frame_received.can_id);
				}
				int r;
				results_ready = 1;
				for (r=0; r < size; r++){
					if (results[r] == 0.0){
						results_ready = 0;
						break;
					}
				}
				if (results_ready>0 && results_printed == 0){
					for (r = 0; r < size; r++){
						printf("%d->%d %d %f\n", links[r].sock_rx, links[r].sock_tx, links[r].type, results[r]);
					}
					results_printed = 1;
				}
			} //if (pfds[i].revents & POLLIN)
		} //for(int i = 0; i < size; i++)
	} //end for(;;)
}


int main(int argc, char *argv[]){
  if (argc != 2) {
		fprintf(stderr,"usage: converter mode\n");
		printf("mode is either 'pthread' or 'poll'\n");
		exit(1);
	}

	if (!(strcmp(argv[1], "pthread") == 0 || strcmp(argv[1], "poll") == 0)){
		fprintf(stderr,"unknown mode %s; should be 'pthread' or 'poll'\n", argv[1]);
		exit(1);
	}
	printf("mode: %s\n", argv[1]);

  //total number of links
	int num_links = NUM_CAN + NUM_DEV;

	//initialize CAN interfaces and corresponding sockets
	//virtual vcan interfaces are used here
	int can_sockets[NUM_CAN];
	printf("\nCAN interfaces:\n");
	can_sockets[0] = init_can_interface("vcan0");
	can_sockets[1] = init_can_interface("vcan1");

	//create canudp links (from CAN to Ethernet), one for each CAN interface
	struct link canudp_links[NUM_CAN];
	make_canudp_links(can_sockets, NUM_CAN, canudp_links, IPADDR, ETHPORT);

	//create udpcan links (from Ethernet to CAN), one for each device
	struct link udpcan_links[NUM_DEV];
	udpcan_links[0] = make_udpcan_link(MYPORT0, can_sockets[0]);
	udpcan_links[1] = make_udpcan_link(MYPORT1, can_sockets[1]);

  //unite links for convenience
	struct link all_links[num_links];
	unite_links(canudp_links, udpcan_links, all_links, NUM_CAN, NUM_DEV);

  //print info
	print_links(all_links, num_links);

	//create threads
	pthread_t threads[num_links];
	int result_code;
	int t;
	for (t = 0; t < num_links; t++){
		result_code = pthread_create(&threads[t], NULL, run_link, &all_links[t]);
		if (result_code){
			printf("ERROR; return code from pthread_create() is %d\n", result_code);
			exit(-1);
		}
	}

	//wait for all threads
	for (t = 0; t < num_links; t++) {
		pthread_join(threads[t], NULL);
	}

	close_all_sockets(all_links, num_links);
	return 0;
}
