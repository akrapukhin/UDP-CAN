/*
** converter.c udp-can converter.
** Receives UDP datagrams, processes them and sends to CAN interfaces.
**
** В тестовом задании написано, что есть несколько интерфейсов CAN, и каждому 
** устройству соответствует определенный приемный UDP-порт. Как я понял, под 
** "устройством" здесь понимается какой-либо контроллер или датчик, подключенный
** к CAN-шине, а не сам интерфейс. То есть каждый интерфейс подключен к своей 
** CAN-шине, а к CAN-шине могут быть подключены несколько устройств. Конвертер 
** получает какое-то сообщение на определенный порт, соответствующий конкретному
** устройству, обрабатывает его и посылает на соответствующую CAN-шину, к которой
** подключено устройство. Сообщение при этом видят все устройства, подключенные к 
** шине. Таким образом, количество UDP-портов равно количеству устройств. Я нарисовал
** небольшую схему, ее можно посмотреть в файле sketch.jpg. Если же я неправильно 
** понял, и количество портов равно количеству CAN-интерфейсов, то код легко 
** поправить в таком случае. Кроме того, в коде для простоты два интерфейса и одно 
** устройство на каждом, поэтому в данном случае разницы нет. 
**
** Также из задания не совсем понятно, соответствует ли всем CAN-интерфейсам один 
** ip/port, на который отсылаются сообщения, или же информация с каждого CAN-интерфейса 
** посылается на свой ip/port. Я при выполнении задания предполагал, что информация со
** всех CAN-интерфейсов передается на один ip/port. Опять же, это легко поправить, если 
** необходимо использовать свой ip/port для каждого CAN-интерфейса.
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

#include <net/if.h>
#include <sys/ioctl.h>

#include <linux/can.h>
#include <linux/can/raw.h>

#include <pthread.h>
#include <poll.h>

//one receiving UDP-port for each device (define new ports here to add new devices)
#define MYPORT0 4950
#define MYPORT1 4951

//data from all CAN interfaces is sent to this ip address and port
#define IPADDR "127.0.0.1" //loopback for testing
#define ETHPORT 4952

//type of connection (from Ethernet to CAN or vice versa)
#define UDP_CAN 0
#define CAN_UDP 1

//number of CAN interfaces
#define NUM_CAN 2

//number of devices connected to CAN interfaces
#define NUM_DEV 2

//for testing (number of messages to receive before computing results)
#define CYCLES 100000


/*
** Each device has an associated UDP port and a udp socket listening to this port.
** Data read from this socket is written to a CAN socket corresponding to a
** specific CAN interface to which the device is connected to.
**
** When data flows from CAN to Ethernet, the process is similar. A corresponding
** CAN socket is listening to a specific CAN interface. Data is then written to
** a UDP socket and sent to a specified address (ip and port)
**
** Both of these relationships are captured by the struct link defined below.
**
** @sock_rx - socket receiving data from Ethernet or from CAN
** @sock_tx - socket sending data to Ethernet or to CAN
** @type - UDP_CAN or CAN_UDP - from Ethernet to CAN or vice versa
** @addr - destination address (ip and port for canudp links) or receiving
**         address (for udpcan links)
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
** socket in all corresponding links is the same.

** @can_sockets[] - array of socket descriptors, one for each CAN interface
** @size - size of the array
** @canudp_links[] - array of links which is filled by the function
** @ip_address - destination ip address
** @port_num - specific port on the destination address
** @unique - flag setting to have unique sockets for writing to outside ip/port
**           added for potentially improving multithreading, but I didn't see
**           any difference
*/
void make_canudp_links(int can_sockets[], int size, struct link canudp_links[], const char *ip_address, int port_num, bool unique);


/*
** Creates a udpcan link between a port and a CAN interface for each device.
** Each device has a separate unique port. The link is made between this port
** and a CAN socket corresponding to the CAN interface to which the device is
** connected to. This CAN socket may be not unique because multiple devices can
** be connected to the same CAN bus.
**
** @port_num - port dedicated to a device
** @can_socket - CAN socket corresponding to the CAN interface
**
** returns:
** a udpcan link
*/
struct link make_udpcan_link(int port_num, int can_socket);


/*
** connects to a CAN interface.
** Here virtual CAN interfaces are used for testing.
** Interfaces must be initialized before running. Example initializing vcan0:
** sudo modprobe vcan
** sudo ip link add dev vcan0 type vcan
** sudo ip link set up vcan0
**
** @ifname - name of the CAN interface
**
** returns:
** a file descriptor of the CAN socket bound to the interface.
*/
int connect_to_can(const char *ifname);


/*
** each link is processed by a separate pthread.
**
** There can be problems with using threads for each link, as
** the threads can read and write from/to the same sockets if 
** links are created that way. For example, two threads can read data
** from different UDP socket and then use the same CAN socket to send it.
** Another example: Data from CAN can be read by different CAN sockets but
** then sent to the same UDP socket. From what I've read, however, it seems
** that UDP and CAN sockets can be used in such a way with no problems, as
** read/write operations in this case are atomic and thread-safe. Also, I 
** didn't run into such issues while testing this function. However, these 
** possible  problems can be avoided if links are created in main() in such 
** a way that all sockets in them are unique. The function itself doesn't 
** need any modification.
** 
** An alternative to using pthreads which does not have these possible problems
** is to use poll() or select() functions. Function using poll() is declared and
** implemented below.
** 
** @link_ptr - pointer to struct link
*/
void *process_link_pthread(void *link_ptr);


/*
** process all links by polling listening rx sockets in links
*/
void process_links_poll(struct link links[], int size);


/*
** combines two arrays of links into one
*/
void unite_links(struct link canudp_links[], struct link udpcan_links[], struct link united_links[], int canudp_size, int udpcan_size);


/*
** prints info about all links
*/
void print_links(struct link links[], int size);


/*
** close all sockets found in links
*/
void close_all_sockets(struct link links[], int size);


/*
** main function
*/
int main(int argc, char *argv[]){
  if (argc < 2) {
    fprintf(stderr,"usage: converter mode\n");
    printf("mode is either 'pthread' or 'poll'\n");
    exit(1);
  }

  //two modes, pthread or poll
  if (!(strcmp(argv[1], "pthread") == 0 || strcmp(argv[1], "poll") == 0)){
    fprintf(stderr,"unknown mode %s; should be 'pthread' or 'poll'\n", argv[1]);
    exit(1);
  }
  printf("mode: %s\n", argv[1]);

  //total number of links
  int num_links = NUM_CAN + NUM_DEV;

  //connect to CAN interfaces (virtual vcan interfaces are used here)
  int can_sockets[NUM_CAN];
  printf("\nCAN interfaces:\n");
  can_sockets[0] = connect_to_can("vcan0");
  can_sockets[1] = connect_to_can("vcan1");

  //create canudp links (from CAN to Ethernet), one for each CAN interface
  struct link canudp_links[NUM_CAN];
  make_canudp_links(can_sockets, NUM_CAN, canudp_links, IPADDR, ETHPORT, false);

  //create udpcan links (from Ethernet to CAN), one for each device
  struct link udpcan_links[NUM_DEV];
  udpcan_links[0] = make_udpcan_link(MYPORT0, can_sockets[0]);
  udpcan_links[1] = make_udpcan_link(MYPORT1, can_sockets[1]);

  //unite links for convenience
  struct link all_links[num_links];
  unite_links(canudp_links, udpcan_links, all_links, NUM_CAN, NUM_DEV);

  //print info
  print_links(all_links, num_links);

  //run links
  if (strcmp(argv[1], "pthread") == 0){ //thread mode
    //create threads
    pthread_t threads[num_links];
    int result_code;
    for (int t = 0; t < num_links; t++){
      result_code = pthread_create(&threads[t], NULL, process_link_pthread, &all_links[t]);
      if (result_code){
        printf("ERROR; return code from pthread_create() is %d\n", result_code);
        exit(1);
      }
    }

    //wait for all threads
    for (int t = 0; t < num_links; t++) {
      pthread_join(threads[t], NULL);
    }
  }
  else { //poll mode
    process_links_poll(all_links, num_links);
  }

  close_all_sockets(all_links, num_links);
  return 0;
}


void make_canudp_links(int can_sockets[], int size, struct link canudp_links[], const char *ip_address, int port_num, bool unique){
  //ip address and port
  struct sockaddr_in addr;
  int sock_descr;
  addr.sin_family = AF_INET; //AF_INET6 for ipv6
  addr.sin_port = htons(port_num); //host-to-network (if host is little endian)
  inet_pton(AF_INET, ip_address, &(addr.sin_addr));
  if ((sock_descr = socket(AF_INET, SOCK_DGRAM, 0)) == -1) {
    perror("tx socket: socket");
  }

  //fill in all links
  for (int i = 0; i < size; ++i) {
    if (unique){
      if ((sock_descr = socket(AF_INET, SOCK_DGRAM, 0)) == -1) {
        perror("tx socket: socket");
      }
    }
    canudp_links[i].sock_rx = can_sockets[i];
    canudp_links[i].sock_tx = sock_descr;
    canudp_links[i].type = CAN_UDP;
    canudp_links[i].addr = addr;
  }
}


struct link make_udpcan_link(int port_num, int can_socket){
  //fill in address
  struct sockaddr_in addr;
  int sock_descr;
  addr.sin_family = AF_INET;
  addr.sin_port = htons(port_num);
  addr.sin_addr.s_addr = INADDR_ANY; //set to the host ip
  if ((sock_descr = socket(AF_INET, SOCK_DGRAM, 0)) == -1) {
    perror("make_udpcan_link: socket");
  }

  //bind socket to port
  if (bind(sock_descr, (struct sockaddr*)&addr, sizeof addr) == -1) {
    close(sock_descr);
    perror("make_udpcan_link: bind");
  }

  //create link
  struct link res = {sock_descr, can_socket, UDP_CAN, addr};
  return res;
}


int connect_to_can(const char *ifname){
  //create CAN socket
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
  
  //bind socket to CAN interface
  if(bind(sock_can_descr, (struct sockaddr *)&addr, sizeof(addr)) == -1) {
    perror("Error in socket bind");
    exit(1);
  }
  printf("CAN socket %d is bound to CAN interface %s at index %d\n", sock_can_descr, ifname, ifr.ifr_ifindex);
  return sock_can_descr;
}


void *process_link_pthread(void *link_ptr)
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
  bool results_printed = 0;

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

    //some processing can be done with the received data. Here source of the
    //frame is specified in frame_tosend.data[0] and destination in
    //frame_tosend.data[1]. data[2] and can_id are taken from the frame_received
    //and left unchanged. This is done for testing purposes.
    if (stype==UDP_CAN){
      frame_tosend.can_id  = frame_received.can_id;
      frame_tosend.can_dlc = 3;
      frame_tosend.data[0] = 'c'; //c - converter 63
      frame_tosend.data[1] = 'b'; //b - bus 62
      frame_tosend.data[2] = frame_received.data[2];
      numbytes = send(sock_out, &frame_tosend, sizeof(struct can_frame), 0);
      if (numbytes == -1) {
        perror("converter: send");
        exit(1);
      }
    }
    else if (stype==CAN_UDP){
      frame_tosend.can_id  = frame_received.can_id;
      frame_tosend.can_dlc = 3;
      frame_tosend.data[0] = 'c'; //c - converter 63
      frame_tosend.data[1] = 'e'; //e - ethernet 65
      frame_tosend.data[2] = frame_received.data[2];
      numbytes = sendto(sock_out, &frame_tosend, sizeof(struct can_frame), 0, (struct sockaddr*)&addr, sizeof addr);
      if (numbytes == -1) {
        perror("converter: sendto");
        exit(1);
      }
    }
    else{
      perror("unknown link type");
      exit(1);
    }

    //print if signal from testbench says so
    if(frame_received.data[3]){
      printf("%d %c %c %d -> %d %c %c %d\n", frame_received.can_id, frame_received.data[0], frame_received.data[1], frame_received.data[2],
                                             frame_tosend.can_id, frame_tosend.data[0], frame_tosend.data[1], frame_tosend.data[2]);
    }

    //for testing (compute results if enough messages were received)
    counters++;
    if (counters > CYCLES && results == 0.0){
      results = (float)counters / (float)(frame_received.can_id);
    }
    if (results>0 && !results_printed){
      printf("%d->%d %d %f (fraction of messages received)\n", sock_in, sock_out, stype, results);
      results_printed = true;
    }
  } //end for(;;). Probably need to specify condition on which the infinite loop exits
  pthread_exit(NULL);
}


void process_links_poll(struct link links[], int size){
  //array of file descriptors to poll
  struct pollfd *pfds = malloc(sizeof *pfds * size);

  //fill in the array
  for (int i = 0; i < size; ++i){
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

  for(;;) {
    int poll_count = poll(pfds, size, -1);

    if (poll_count == -1) {
      perror("poll");
      exit(1);
    }

    //look for data to read
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

        //some processing can be done with the received data. Here source of the
        //frame is specified in frame_tosend.data[0] and destination in
        //frame_tosend.data[0]. data[2] and can_id are taken from the frame_received
        //and left unchanged. This is done for testing purposes.
        if (links[i].type==UDP_CAN){
          frame_tosend.can_id  = frame_received.can_id;
          frame_tosend.can_dlc = 3;
          frame_tosend.data[0] = 'c'; //c - converter 63
          frame_tosend.data[1] = 'b'; //b - bus 62
          frame_tosend.data[2] = frame_received.data[2];
          numbytes = send(links[i].sock_tx, &frame_tosend, sizeof(struct can_frame), 0);
          if (numbytes == -1) {
            perror("converter: send");
            exit(1);
          }
        }
        else if (links[i].type==CAN_UDP){
          frame_tosend.can_id  = frame_received.can_id;
          frame_tosend.can_dlc = 3;
          frame_tosend.data[0] = 'c'; //c - converter 63
          frame_tosend.data[1] = 'e'; //e - ethernet 65
          frame_tosend.data[2] = frame_received.data[2];
          numbytes = sendto(links[i].sock_tx, &frame_tosend, sizeof(struct can_frame), 0, (struct sockaddr*)&links[i].addr, sizeof links[i].addr);
          if (numbytes == -1) {
            perror("converter: sendto");
            exit(1);
          }
        }
        else{
          perror("unknown link type");
          exit(1);
        }

        //print if signal from testbench says so
        if(frame_received.data[3]){
          printf("%d %c %c %d -> %d %c %c %d\n", frame_received.can_id, frame_received.data[0], frame_received.data[1], frame_received.data[2],
                                                 frame_tosend.can_id, frame_tosend.data[0], frame_tosend.data[1], frame_tosend.data[2]);
        }

        //for testing (compute results if enough messages were received)
        counters[i]++;
        if (counters[i] > CYCLES && results_computed[i] == false){
          results[i] = (float)counters[i] / (float)(frame_received.can_id);
          results_computed[i] = true;
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
          printf("fraction of messages received:\n");
          for (r = 0; r < size; r++){
            printf("%d->%d %d %f\n", links[r].sock_rx, links[r].sock_tx, links[r].type, results[r]);
          }
          results_printed = true;
        }
      } //end if (pfds[i].revents & POLLIN)
    } //end for(int i = 0; i < size; i++)
  } //end for(;;)
  free(pfds);
}


void unite_links(struct link canudp_links[], struct link udpcan_links[], struct link united_links[], int canudp_size, int udpcan_size){
  for (int i=0; i<udpcan_size; ++i){
    united_links[i] = udpcan_links[i];
  }
  for (int i=0; i<canudp_size; ++i){
    united_links[udpcan_size + i] = canudp_links[i];
  }
}


void print_links(struct link links[], int size){
  char str[INET_ADDRSTRLEN];
  printf("\nLinks:\n");
  for (int i=0; i<size; ++i){
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
  int sock_tx_udp_open = 1;
  for (int i=0; i<size; ++i){
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

