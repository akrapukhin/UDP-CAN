#!/bin/sh
gnome-terminal --tab --title="pconverter" -- "./pconverter"
gnome-terminal --tab --title="eth_talker 4950" -- "./eth_talker" 4950
gnome-terminal --tab --title="eth_talker 4951" -- "./eth_talker" 4951
gnome-terminal --tab --title="eth_listener" -- "./eth_listener"
gnome-terminal --tab --title="can_talker vcan0" -- "./can_talker" vcan0
gnome-terminal --tab --title="can_talker vcan1" -- "./can_talker" vcan1
gnome-terminal --tab --title="can_listener vcan0" -- "./can_listener" vcan0
gnome-terminal --tab --title="can_listener vcan1" -- "./can_listener" vcan1
