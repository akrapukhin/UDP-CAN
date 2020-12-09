#!/bin/bash
sudo modprobe vcan

#vcan0
sudo ip link add dev vcan0 type vcan
sudo ip link set up vcan0

#vcan1
sudo ip link add dev vcan1 type vcan
sudo ip link set up vcan1
