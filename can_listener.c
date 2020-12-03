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
  	//timer_test.tv_nsec = 999999999;
    timer_test.tv_nsec = 0;

    unsigned int mes_counter = 0;
    unsigned char data0 = 0;


    unsigned int memo = 0;
    unsigned int errors = 0;
    while(1){
      if ((numbytes = recv(s, &frame, sizeof(struct can_frame), 0)) == -1) {
        perror("talker: sendto");
        exit(1);
      }

      if (frame.can_id - memo > 1)
      {
        errors++;
        //printf("%d ooooooooooooooooooooooooooo buf: %d, memo: %d\n",i, *buf, memo[i]);
      }
      memo = frame.can_id;
      //printf("%d %d\n", frame.can_id, errors);
      printf("%d %c %c %c %d\n", frame.can_id, frame.data[0], frame.data[1], frame.data[2], errors);
  	  //nanosleep(&timer_test, &tim);
    }



    return 0;
}
