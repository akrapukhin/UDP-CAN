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

int
main(void)
{
    int s;
    int nbytes;
    struct sockaddr_can addr;
    struct can_frame frame;
    struct ifreq ifr;

    const char *ifname = "vcan0";

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

    frame.can_id  = 0x124;
    frame.can_dlc = 3;
    frame.data[0] = 0x1a;
    frame.data[1] = 0x2b;
    frame.data[2] = 0x3c;

    //nbytes = write(s, &frame, sizeof(struct can_frame));
    nbytes = send(s, &frame, sizeof(struct can_frame), 0);

    printf("Wrote %d bytes, out of %lu bytes\n", nbytes, sizeof(struct can_frame));


    return 0;
}
