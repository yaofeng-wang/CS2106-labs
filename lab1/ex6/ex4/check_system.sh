#!/bin/bash

####################
# Lab 1 Exercise 4
# Name:
# Student No:
# Lab Group: 
####################

# fill the below up
hostname=$(uname -n)
kernel_version=$(uname -r)
process_cnt=$(ps -e | wc -l)
user_process_cnt=$(ps | wc -l)
mem_usage=$(free | grep Mem | awk '{print $3/$2*100.0}')
swap_usage=$(free | grep Swap | awk '{print $3/$2*100.0}')

echo "Hostname: $hostname"
echo "Linux Kernel Version: $kernel_version"
echo "Total Processes: $process_cnt"
echo "User Processes: $user_process_cnt"
echo "Memory Used (%): $mem_usage"
echo "Swap Used (%): $swap_usage"
