#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <stdbool.h>

#include <net/if.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/ioctl.h>

#include <linux/can.h>
#include <linux/can/raw.h>

#include <time.h>

#define CYCLES 100000

int main(int argc, char *argv[])
{

  if (argc < 4) {
    fprintf(stderr,"usage: can_listener can_interface num_dev_total num_dev_interface [print]\n");
    exit(1);
  }

    int s;
    int numbytes;
    struct sockaddr_can addr;
    struct can_frame frame;
    struct ifreq ifr;
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

    int size = argv[2][0] - '0';

    int num_dev = argv[3][0] - '0';

  	//arrays necessary to compute results
  	unsigned int memo[size+1];
  	memset(memo, 0, (size+1)*sizeof(int));
  	unsigned int errors[size+1];
  	memset(errors, 0, (size+1)*sizeof(int));
  	unsigned int counters[size+1];
  	memset(counters, 0, (size+1)*sizeof(int));
  	float results[size+1];
  	memset(results, 0, (size+1)*sizeof(float));
  	bool results_computed[size+1];
  	memset(results_computed, false, (size+1)*sizeof(bool));
  	bool all_results_ready = false;
  	bool results_printed = false;

  	printf("\n\n\n");
    while(1){
      if ((numbytes = recv(s, &frame, sizeof(struct can_frame), 0)) == -1) {
        perror("talker: sendto");
        exit(1);
      }

      if (argc >= 4 && strcmp(argv[3], "print") == 0){
        printf("%d %c %c %d\n", frame.can_id, frame.data[0], frame.data[1], frame.data[2]);
      }

      if (frame.data[0] == 0x63){ //data from converter
        int source_converter = (int)frame.data[2];
        counters[source_converter]++;


        if (frame.can_id - memo[source_converter] > 1){
          errors[source_converter]++;
        }
        if (counters[source_converter] > CYCLES && !results_computed[source_converter]){
          results[source_converter] = (float)counters[source_converter] / (float)(frame.can_id);
          results_computed[source_converter] = true;
        }
      }
      else if (frame.data[0] == 0x74){
        counters[size]++;
        if (frame.can_id - memo[size] > 1){
          errors[size]++;
        }
        if (counters[size] > CYCLES && !results_computed[size]){
          results[size] = (float)counters[size] / (float)(frame.can_id);
          results_computed[size] = true;
        }
      }
      else{
        printf("CONVERTER OR TALKER?\n");
      }

      int r;
      int res_sum = 0;
      for (r=0; r <= size; r++){
        res_sum += results_computed[r];
      }
      if (res_sum == num_dev+1){
        all_results_ready = true;
      }
      else{
        all_results_ready = false;
      }
      if (all_results_ready && !results_printed){
        for (r = 0; r < size; r++){
          if(results_computed[r]){
            printf("converter %d->listener %s %f %d\n", r, argv[1], results[r], errors[r]);
          }
        }
        printf("can_talker %s->listener %s %f %d\n", argv[1], argv[1], results[size], errors[size]);
        results_printed = true;
      }
    }

    return 0;
}
