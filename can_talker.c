/*
** can_talker.c -- CAN talker for testing, imitates a CAN device writing messages to bus. 
** These messages are sent to a specified IP address through the converter.
*/

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>

#include <net/if.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/ioctl.h>

#include <linux/can.h>
#include <linux/can/raw.h>

#include <time.h>

int main(int argc, char *argv[]) {
  if (argc < 2) {
    fprintf(stderr,"usage: can_talker can_interface [print waitfor s ns]\n");
    exit(1);
  }
  
  //create CAN socket
  int sockfd;
  int numbytes;
  struct sockaddr_can addr;
  struct can_frame frame;
  struct ifreq ifr;
  const char *ifname = argv[1];
  if((sockfd = socket(PF_CAN, SOCK_RAW, CAN_RAW)) == -1) {
    perror("can_talker: error while opening socket");
    return 1;
  }
  strcpy(ifr.ifr_name, ifname);
  ioctl(sockfd, SIOCGIFINDEX, &ifr);
  addr.can_family  = AF_CAN;
  addr.can_ifindex = ifr.ifr_ifindex;

  printf("%s at index %d\n", ifname, ifr.ifr_ifindex);

  if(bind(sockfd, (struct sockaddr *)&addr, sizeof(addr)) == -1) {
    perror("can_talker: error in socket bind");
    return 1;
  }

  //wait between transmissions
  struct timespec timer_test, tim;
  timer_test.tv_sec = 0;
  timer_test.tv_nsec = 0;
  if (argc == 6 && strcmp(argv[3], "waitfor") == 0) {
		timer_test.tv_sec = argv[4][0] - '0';
		timer_test.tv_nsec = argv[5][0] - '0';
		printf("wait each cycle for %lds %ldns\n", timer_test.tv_sec, timer_test.tv_nsec);
	}

  unsigned int mes_counter = 0;

  while(1) {
    frame.can_id  = mes_counter; //can_id is incremented in each message
    frame.can_dlc = 4; //4 bytes sent in data field
    frame.data[0] = 0x74; //t - talker (source)
    frame.data[1] = 0x62; //b - bus (destination)
    frame.data[2] = argv[1][4] - '0'; //CAN bus num to which the message is sent [0-9]
    frame.data[3] = 0; //let know the converter to print its results for demo

    //print sent messages for demo
    if (argc >= 3 && strcmp(argv[2], "print") == 0) {
      printf("%d %c %c %d\n", frame.can_id, frame.data[0], frame.data[1], frame.data[2]);
      frame.data[3] = 1;
    }

    //send
    if ((numbytes = send(sockfd, &frame, sizeof(struct can_frame), 0)) == -1) {
      perror("talker: sendto");
      exit(1);
    }

    //increment counter
    mes_counter++;
    if (mes_counter > 4294967290) {mes_counter = 0;}

    //wait if necessary
		if (argc == 6 && strcmp(argv[3], "waitfor") == 0) {
		  nanosleep(&timer_test, &tim);
		}
  }

  close(sockfd);
  return 0;
}
