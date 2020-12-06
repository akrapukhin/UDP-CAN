#!/bin/bash
gcc eth_talker.c -o eth_talker
gcc eth_listener.c -o eth_listener
gcc converter.c -o converter
gcc can_talker.c -o can_talker
gcc can_listener.c -o can_listener
gcc -pthread pconverter.c -o pconverter
gcc -pthread pconverter_impr.c -o pconverter_impr
gcc -pthread pconverter_bug.c -o pconverter_bug
