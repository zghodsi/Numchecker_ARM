# NumChecker

NumChecker is a Virtual Machine Monitor (VMM) based framework to detect control-flow modifying kernel rootkits in a guest Virtual Machine (VM). NumChecker detects malicious modifications to a system call in the guest VM by checking the number of certain hardware events that occur during the system call's execution. To automatically count these events, NumChecker leverages the **Hardware Performance Counters (HPCs)**, which exist in most modern processors. For more information, check out the [NumChecker](https://dl.acm.org/citation.cfm?id=2488831) paper. This implementation is a prototype of NumChecker on **ARM** platform running on a Chromebook.

This repository contains the modified host and guest kernels for NumChecker. In the guest kernel, certain system calls are modified to enable monitoring, and in the host kernel, the hardware performance counters are enabled to monitor the guest system calls.
