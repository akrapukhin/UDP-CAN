#!/bin/sh
#gnome-terminal --tab -e "pwd" --tab -e "ls"
#gnome-terminal -x bash -c "pwd; exec bash"
#gnome-terminal -x bash -c "ls; exec bash"
#gnome-terminal --tab -e "pwd; exec bash" --tab -e "ls; exec bash" --tab -e "top"
#gnome-terminal -- pwd --tab -- ls --tab -- pwd
#gnome-terminal -- ls
#gnome-terminal -- pwd

#alias eth_talker_0='./eth_talker 4950'
#alias eth_talker_0='./eth_talker 4951'

gnome-terminal --tab --title="converter" -- "./converter"
gnome-terminal --tab --title="eth_talker 4950" -- "./eth_talker" 4950
gnome-terminal --tab --title="eth_talker 4951" -- "./eth_talker" 4951
gnome-terminal --tab --title="eth_listener" -- "./eth_listener"
gnome-terminal --tab --title="can_talker vcan0" -- "./can_talker" vcan0
gnome-terminal --tab --title="can_talker vcan1" -- "./can_talker" vcan1
gnome-terminal --tab --title="can_listener vcan0" -- "./can_listener" vcan0
gnome-terminal --tab --title="can_listener vcan1" -- "./can_listener" vcan1

#gnome-terminal --tab -- bash -ic "ls; exec bash"
#gnome-terminal --tab -- bash -ic "pwd"

#gnome-terminal -e 'command "./eth_talker 4950"' --tab -e 'command "./eth_talker 4951"'
