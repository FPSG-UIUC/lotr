#!/usr/bin/env bash

# Load MSR
sudo modprobe msr

# Turn on/off prefetchers
# NOTE: write 0 to enable them, 15 to disable them
sudo wrmsr -a 0x1a4 15

# Enable some hugepages
echo 2048 | sudo tee /proc/sys/vm/nr_hugepages

# Disable Hyper Threading (optional)
echo off | sudo tee /sys/devices/system/cpu/smt/control

# Fix the CPU frequency (optional)
echo "performance" | sudo tee /sys/devices/system/cpu/cpu*/cpufreq/scaling_governor

# Disable Turboboost (optional)
echo "1" | sudo tee /sys/devices/system/cpu/intel_pstate/no_turbo