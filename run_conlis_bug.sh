#!/bin/sh
gnome-terminal --tab --title="pconverter_bug" -- "./pconverter_bug"
gnome-terminal --tab --title="can_listener vcan0" -- "./can_listener" vcan0
gnome-terminal --tab --title="can_listener vcan1" -- "./can_listener" vcan1
