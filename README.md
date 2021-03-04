# Overview

This repository contains the source code to reproduce the experiments of the paper:

- [_Lord of the Ring(s): Side Channel Attacks on the CPU On-Chip Ring Interconnect Are Practical_][paper] (__USENIX 2021__)

**NB:** This source code is CPU-specific.
Before you proceed, check that you are on the right branch.
If you cannot find a branch for your CPU, extra work may be needed to port the code to your CPU (see below).

## Tested Setup

The code of this branch was tested on a **bare-metal** machine with an Intel **i7-9700 CPU**, running **Ubuntu Server 16.04 LTS**.
We do not guarantee that it works in virtualized environments, on other CPUs, or with other OSs.

## Materials

This repository contains the following materials:
- `01-ring-topology`: this is the code that we used to measure the load latency for each core/slice configuration.
It confirmed the linear ring interconnect topology for our client-class CPUs.
- `02-ring-contention`: this is the code that we used to reverse engineer the ring interconnect communication protocols. 
It contains the basic code to monitor and create contention on the ring interconnect.
There is also a script to compare the timing distributions under different configurations.
- `03-cc`: this is the code of a covert channel where the sender transmits ones and zeros.
It also contains the scripts to measure the channel capacity under different configurations.
- `04-cc`: this is the code of a side channel attack against vulnerable cryptographic code (RSA and EdDSA).
It also contains the code to train a classifier to distinguish the victim's secret bit.
- `05-keystroke-timing-attack`: this is the code of another side channel attack that leaks the timing of keystrokes pressed by a user.

Inside each folder you will find instructions for how to run the experiments.
Notice that the preliminary steps described in the different READMEs differ.
We suggest to proceed in order, from 01 to 05.

## How can I port this code to other CPUs?

To run this code on other CPUs, you will need to write the code that maps physical addresses to LLC slices on your CPUs.
There are multiple ways to do this, e.g., with performance counters or with knowledge of the slice mapping function.
The difficulty of this step depends on the CPU that you are using. 
You can find some code to learn more about this in the `util` subdirectory.

## Citation

If you make any use of this code for academic purposes, please cite the paper:

```tex
@inproceedings{paccagnella2020lotr,
    author = {Riccardo Paccagnella and Licheng Luo and Christopher W. Fletcher},
    title = {Lord of the Ring(s): Side Channel Attacks on the {CPU} On-Chip Ring Interconnect Are Practical},
    booktitle = {Proc.\ of the USENIX Security Symposium (USENIX)},
    year = {2021},
}
```

[paper]: https://arxiv.org/pdf/2103.03443.pdf
