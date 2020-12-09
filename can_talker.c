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

    if (argc < 2) {
      fprintf(stderr,"usage: can_talker can_interface [print wait s ns]\n");
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
    if(argc == 6 && strcmp(argv[3], "wait")){
			timer_test.tv_sec = argv[4][0] - '0';
			timer_test.tv_nsec = argv[5][0] - '0';
			printf("wait each cycle for %lds %ldns\n", timer_test.tv_sec, timer_test.tv_nsec);
		}

    unsigned int mes_counter = 0;

    while(1){
      frame.can_id  = mes_counter;
      frame.can_dlc = 4;
      frame.data[0] = 0x74; //t - talker
      frame.data[1] = 0x62; //b - bus
      frame.data[2] = argv[1][4] - '0';
      frame.data[3] = 0;

      //print sent messages for demo
      if (argc >= 3 && strcmp(argv[2], "print") == 0){
        printf("%d %c %c %d\n", frame.can_id, frame.data[0], frame.data[1], frame.data[2]);
        frame.data[3] = 1;
      }

      //send
      if ((numbytes = send(s, &frame, sizeof(struct can_frame), 0)) == -1) {
        perror("talker: sendto");
        exit(1);
      }

      mes_counter++;
      if (mes_counter > 4294967290) {mes_counter=0;}

      //wait if necessary
			if (argc == 6 && strcmp(argv[3], "wait") == 0){
				nanosleep(&timer_test, &tim);
			}
    }

    close(s);
    return 0;
}
