# UDP-to-CAN converter
Receives UDP datagrams, processes them and sends to CAN interfaces, and vice versa.<br><br>
Each device in a car (like a wheel speed sensor or an engine controller or many other electronic control units (ECU)) is connected to a CAN-bus (Controller Area Network). Cars can have many CAN-buses. Here it's assumed that each device has its own corresponding UDP-port in the converter. An external user needs to know the  address of the UDP-port to send messages to a specific device. When a UDP-message comes through the Ethernet to a specific UDP-port, the UDP-CAN converter processes it and sends it further to the corresponding CAN-bus to which the device is connected. Devices can also send messages to its bus. All the bus messages are received by the converter, processed and sent to a specified UDP-port where a user can receive them and analyze the CAN-bus traffic. <br>

It is also easy to modify the code such that each bus corresponds to a specific UDP-port. In that case number of UDP-ports would be equal to the number of buses instead of the number of devices. 

<br>
A sketch of the converter can be seen below. Here five devices are connected to two buses. Two CAN-interfaces are connected to these buses. Each CAN-interface has a corresponding sending/receiving CAN-socket. There are five receiving UDP-sockets, one for each device, and one transmitting UDP-socket which sends all the bus messages outside.

![sketch](https://user-images.githubusercontent.com/34278438/120108818-71d13a00-c16f-11eb-8173-2ce1a9344e26.jpg)
 

## Demo
To run the demo, enter the following commands:
1) Compile everything needed
```
./compile_all.sh
```

2) Initialize two virtual CAN interfaces vcan0 and vcan1
```
./init_buses.sh
```

3) Run the demo. There are two versions: poll and pthread. The poll version processes all connections in a sequential way by polling all the receiving sockets. The pthread version runs each connection in parallel in a separate thread.
```
./demo_poll.sh
```
```
./demo_pthread.sh
```

Two Ethernet-talkers will start sending messages to two UDP-ports in the convertor (ports 4950 and 4951), one message per second. Messages will be of the form
"counter-source-destination-channel". For example, message "53 e c 1" means "eth_talker 1 sent message number 53, from ethernet (e) to converter (c).

The messages from the ethernet-talkers are received by the converter and modified. "53 e c 1 -> 53 c b 1" means that the converter converted "53 e c 1" to "53 c b 1". Message number 53 is sent from the converter (c) to the bus 1 (b 1). This message "53 c b 1" then can be seen by can_listener 1.

CAN-talkers send messages to the bus, which can be seen by CAN-listeners as well as the converter. The converter modifies received messages, for example the message "478 t b 0" coming from CAN-talker (t) to the bus 0 (b 0) becomes "478 c e 0" (message number 478 coming from bus0, sent from converter to ethernet). This message is then sent to the Eth_listener (4952), which receives messages from all (two in this demo) CAN-buses.


## Measurements
Apart from the demo, there are also two scripts for measuring  performance. The system is the same as in the demo, but messages are sent not at 1 sec intervals, but as fast as possible. After listeners receive 100000 messages, the results are printed to terminals, indicating the fraction of messages received by all
listeners and by the converter. This way we can see how well the poll and the pthread versions are able to keep up with the coming avalanche of messages and compare the perfomance of the two versions. 
```
./measure_poll.sh
```
```
./measure_pthread.sh
```

