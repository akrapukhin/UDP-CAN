# udp_can
udp to can converter

## Demo
To run demo, run the following commands:

1) ./compile_all.sh
compiles everything needed

2) ./init_buses
initializes two virtual CAN interfaces vcan0 and vcan1

3) ./demo_poll.sh 
   ./demo_pthread.sh
Runs demo. Two Ethernet-talkers will send messages to two udp ports
in the convertor (4950 and 4951), one message per second. Messages will be of the form
"counter-source-destination-channel". For example, 53 e c 1 means that the 
eth_talker 1 sent message number 53, from ethernet (e) to converter (c).

The messages from the eth-talkers are received by the converter 
and modified. 53 e c 1 -> 53 c b 1 means that the converter sends
the message to interface 1 (vcan1).

This message 53 c b 1 then can be seen by can_listener 0.

CAN-talkers send messages to the bus, which can be seen by 
CAN-listeners as well as the converter, which again modifies 
received messages. 478 t b 0 coming to converter becomes
478 c e 0. This message is then sent to the Eth_listener (4952), which receives
messages from all (two in this demo) CAN-interfaces


## Measurements
./measure_poll.sh
./measure_pthread.sh
Apart from demo, there are also two scripts for measuring 
performance. The system is the same as in the demo, but messages
are sent not at 1 sec intervals, but as fast as possible. After
listeners receive 100000 messages, the results are printed to
terminals, indicating the fraction of messages received by all
listeners and by the converter.

