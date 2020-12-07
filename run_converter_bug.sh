#!/bin/sh
gnome-terminal --title="pconverter_bug" -- "./pconverter_bug" pthread 
gnome-terminal --title="eth_talker 4950" -- "./eth_talker" 4950
gnome-terminal --title="eth_talker 4951" -- "./eth_talker" 4951
gnome-terminal --title="eth_listener" -- "./eth_listener"
gnome-terminal --title="can_talker vcan0" -- "./can_talker" vcan0
gnome-terminal --title="can_talker vcan1" -- "./can_talker" vcan1
gnome-terminal --title="can_listener vcan0" -- "./can_listener" vcan0
gnome-terminal --title="can_listener vcan1" -- "./can_listener" vcan1
