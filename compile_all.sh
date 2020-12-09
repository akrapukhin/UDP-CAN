#!/bin/bash
gcc eth_talker.c -o eth_talker
gcc eth_listener.c -o eth_listener
gcc -pthread converter.c -o converter
gcc can_talker.c -o can_talker
gcc can_listener.c -o can_listener
