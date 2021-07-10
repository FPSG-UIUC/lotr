#!/usr/bin/env bash

# Re-enable Turboboost (optional)
echo "0" | sudo tee /sys/devices/system/cpu/intel_pstate/no_turbo

# Unfix the CPU frequency (optional)
echo "powersave" | sudo tee /sys/devices/system/cpu/cpu*/cpufreq/scaling_governor

# Re-enable Hyper Threading (optional)
echo on | sudo tee /sys/devices/system/cpu/smt/control

# Disable hugepages
echo 0 | sudo tee /proc/sys/vm/nr_hugepages

# Turn on prefetchers
# NOTE: write 0 to enable them, 15 to disable them
sudo wrmsr -a 0x1a4 0

# Unload MSR
sudo modprobe -r msr