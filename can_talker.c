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

#define DUMMY 0

int main(int argc, char *argv[])
{
    int s;
    int numbytes;
    struct sockaddr_can addr;
    struct can_frame frame;
    struct ifreq ifr;

    if (argc != 2) {
      fprintf(stderr,"usage: talker can_interface\n");
      exit(1);
    }

    const char *ifname = argv[1];

    if((s = socket(PF_CAN, SOCK_RAW, CAN_RAW)) == -1) {
        perror("Error while opening socket");
        return -1;
    }

    strcpy(ifr.ifr_name, ifname);
    ioctl(s, SIOCGIFINDEX, &ifr);

    addr.can_family  = AF_CAN;
    addr.can_ifindex = ifr.ifr_ifindex;

    printf("%s at index %d\n", ifname, ifr.ifr_ifindex);

    if(bind(s, (struct sockaddr *)&addr, sizeof(addr)) == -1) {
        perror("Error in socket bind");
        return -2;
    }

    struct timespec timer_test, tim;
  	timer_test.tv_sec = 0;
    timer_test.tv_nsec = 0;

    unsigned int mes_counter = 0;
    unsigned char data0 = 0;

    unsigned int dummy_counter = 0;

    while(1){
      frame.can_id  = mes_counter;
      frame.can_dlc = 3;
      frame.data[0] = 0x74; //t - talker
      frame.data[1] = 0x62; //b - bus
      if (argv[1][4] == '0'){
        frame.data[2] = 0x30; //vcan0
      }
      else{
        frame.data[2] = 0x31; //vcan1
      }
      if ((numbytes = send(s, &frame, sizeof(struct can_frame), 0)) == -1) {
        perror("talker: sendto");
        exit(1);
      }
      mes_counter++;
      data0++;
      if (mes_counter > 4294967290) {mes_counter=0;}
      if (data0 >= 255) {data0=0;}

      for (int j=0; j<DUMMY; j++){
  			dummy_counter = dummy_counter + 1;
  		}
  		dummy_counter = 0;
  	  nanosleep(&timer_test, &tim);
    }



    return 0;
}
