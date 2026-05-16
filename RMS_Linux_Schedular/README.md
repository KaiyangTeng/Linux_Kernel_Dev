# UIUC CS 423 MP2

Your Name: Kaiyang Teng

Your NetID: Kteng4

## Project Introduction
This project implements a real-time scheduler based on the Linux Kernel Module, using the RMS algorithm. User-space processes communicate with the kernel module through `/proc/mp2/status` to complete task registration, yielding, and deregistration. The module maintains a task linked list, timers, and a dispatcher thread to perform the scheduling. 
## Main Function
After the module is loaded, `/proc/mp2/status` will be created. Commands written by user processes can perform three operations: register a task `R, pid, period, computation_time`, voluntarily release the CPU `Y, pid`, or unregister a task `D, pid`. When a task is registered, admission control will be performed first. Only if the utilization requirements are met will it be included in the scheduler. After registration is successful, the module will create a PCB for the task, add it to the linked list and initialize the timer. After each job is completed by the task, it will enter sleep and be awakened when the next cycle arrives. 
## Scheduling Logic
This project uses RMS, where the shorter the period, the higher the priority. In the code, three task states are maintained: SLEEPING, READY, and RUNNING. The dispatcher thread selects the task with the shortest period from all the READY tasks to run. If the currently running task has a lower priority, a preemption will occur. To ensure that the selected task actually runs, the current task to be run will be set to `SCHED_FIFO` with a priority of 98, while other tasks will revert to `SCHED_NORMAL`. 
## Core Data Structure
Each task is associated with an `mp2pcb` structure, which stores the pid, period, computation time, status, next wake-up time, `task_struct` pointer, and timer information. All tasks are managed uniformly through the kernel linked list. The PCB memory is allocated and released through the slab cache. 
## Module Flow
When the module is loaded, a proc interface is created, the linked list, lock and slab cache are initialized, and the dispatcher thread is started. When a task yields, the next wake-up timer is set and the task sleeps; when the period arrives, the timer callback sets the task as READY and wakes up the dispatcher to re-schedule. When the module is unloaded, the dispatcher thread is stopped, the proc interface is deleted, the tasks and timers are cleaned up, and the slab cache is destroyed. 
## Compilation and Execution
Execute `make` in the project directory to compile the module. Use `insmod mp2.ko` to load it, and use `rmmod mp2` to uninstall it. You can view the task information by using `cat /proc/mp2/status`.