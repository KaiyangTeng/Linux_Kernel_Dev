
# Linux Kernel Development

A collection of four systems programming projects focused on Linux kernel development, real-time scheduling, virtual memory profiling, and file-system consistency checking.

This repository consolidates a full semester of low-level operating systems work. The projects move from basic Linux kernel module programming to more advanced kernel mechanisms such as procfs communication, kernel timers, workqueues, process scheduling, character devices, `mmap()`, page-fault profiling, and file-system image validation.

## Project Overview

| Project | Topic | Main Focus |
|---|---|---|
| CPU_Usage_Tracker | Linux Kernel CPU-Time Tracking | Procfs interface, kernel linked lists, timers, workqueues, mutexes, and process CPU-time tracking |
| RMS_Linux_Schedular | Rate-Monotonic CPU Scheduling | Real-time scheduling, admission control, dispatcher thread, task state transitions, and Linux scheduler APIs |
| Virtual_Memory_Page_Fault_Profiler | Virtual Memory Profiling | Page-fault monitoring, CPU-utilization sampling, vmalloc buffer, character device, and mmap-based kernel-user sharing |
| Myfsck | File System Checking | Superblock, inode, bitmap, directory-entry validation, reference counting, and file-system consistency checks |

## Environment

These projects were developed and tested in a Linux VM environment using a custom Linux kernel source tree.

Typical environment:

```text
Language: C
Kernel: Linux 5.15.x
Build system: Makefile
Platform: Linux VM / QEMU VM
Core interfaces: procfs, kernel timers, workqueues, character devices, mmap
```

Kernel modules should be tested inside a VM rather than directly on a personal machine, because kernel bugs can crash the entire system.

## CPU_Usage_Tracker

`CPU_Usage_Tracker` implements a Linux kernel module that tracks the user-space CPU time of registered processes.

The module exposes a procfs interface at:

```text
/proc/mp1/status
```

User-space programs can register a PID by writing to this proc entry. The kernel module stores registered processes in a kernel linked list and periodically updates their user CPU time.

### Key Features

* Created a Linux kernel module from scratch.
* Implemented a `/proc/mp1/status` interface for kernel-user communication.
* Used `copy_from_user()` and `copy_to_user()` to safely transfer data across the user/kernel boundary.
* Maintained registered processes with the Linux kernel linked-list API.
* Used a kernel timer to trigger periodic CPU-time updates.
* Used a workqueue to defer expensive work outside interrupt context.
* Protected shared process-tracking state with kernel mutexes.
* Used slab allocation for kernel-side process metadata.
* Removed dead processes from the tracking list.
* Cleaned up procfs entries, timers, workqueues, and dynamically allocated memory during module unload.

### High-Level Design

```text
User Program
    |
    | write PID
    v
/proc/mp1/status
    |
    v
Kernel Module
    |
    ├── Registered process list
    ├── Timer callback
    ├── Workqueue update function
    └── CPU-time query helper
```

The timer acts as the top half: it fires periodically and schedules deferred work. The workqueue function acts as the bottom half: it iterates over the registered process list, updates CPU usage, removes exited processes, and keeps the module state consistent.

### Example Usage

```bash
make
sudo insmod mp1.ko

echo "<PID>" | sudo tee /proc/mp1/status
cat /proc/mp1/status

sudo rmmod mp1
dmesg
```

## RMS_Linux_Schedular

`RMS_Linux_Schedular` implements a simplified Rate-Monotonic Scheduler as a Linux kernel module for single-core periodic real-time tasks.

The scheduler exposes one procfs entry:

```text
/proc/mp2/status
```

Applications interact with the scheduler through three message types:

```text
R,PID,PERIOD,COMPUTATION
Y,PID
D,PID
```

Where:

* `R` registers a real-time task.
* `Y` yields after the task completes its current job.
* `D` deregisters the task.
* `PERIOD` and `COMPUTATION` are specified in milliseconds.

### Key Features

* Implemented a real-time scheduler based on Rate-Monotonic Scheduling.
* Maintained task states: `READY`, `RUNNING`, and `SLEEPING`.
* Used task periods as static priorities: shorter period means higher priority.
* Implemented utilization-based admission control.
* Avoided floating-point arithmetic in kernel space by using integer/fixed-point calculations.
* Implemented a dispatcher kernel thread to make scheduling decisions.
* Used per-task wake-up timers to release periodic jobs.
* Used Linux scheduler APIs to promote and demote tasks:

  * `sched_setattr_nocheck()`
  * `wake_up_process()`
  * `set_current_state()`
  * `schedule()`
* Used `SCHED_FIFO` to run selected real-time tasks with high priority.
* Demoted preempted tasks back to normal Linux scheduling policy.
* Supported registration, yield, deregistration, and status queries through procfs.

### RMS State Transitions

```text
SLEEPING -> READY
    A task reaches the beginning of its next period.

READY -> RUNNING
    The dispatcher selects the highest-priority ready task.

RUNNING -> READY
    A higher-priority task becomes ready and preempts the current task.

RUNNING -> SLEEPING
    The task finishes its current job and yields until the next period.
```

### High-Level Design

```text
User RT Application
    |
    | R / Y / D messages
    v
/proc/mp2/status
    |
    v
RMS Kernel Module
    |
    ├── Registered task list
    ├── Admission control
    ├── Per-task wake-up timers
    ├── Dispatcher kernel thread
    └── Linux scheduler API integration
```

### Example Usage

```bash
make
sudo insmod mp2.ko

echo "R,1234,1000,200" | sudo tee /proc/mp2/status
echo "Y,1234" | sudo tee /proc/mp2/status
echo "D,1234" | sudo tee /proc/mp2/status

cat /proc/mp2/status

sudo rmmod mp2
dmesg
```

## Virtual_Memory_Page_Fault_Profiler

`Virtual_Memory_Page_Fault_Profiler` implements a Linux kernel profiler that samples page-fault counts and CPU utilization for registered processes.

The module uses both procfs and a character device:

```text
/proc/mp3/status
/dev node with major number 423
```

The procfs interface is used for process registration and unregistration. The character device is used to map a kernel-allocated profiling buffer into user space through `mmap()`.

### Key Features

* Implemented a kernel module for virtual-memory behavior profiling.
* Registered and unregistered target processes through `/proc/mp3/status`.
* Tracked registered PIDs using a kernel linked list.
* Used a delayed workqueue to sample profiling data at 20 Hz.
* Collected:

  * Linux `jiffies`
  * Soft page faults
  * Hard page faults
  * CPU utilization using `utime + stime`
* Allocated a virtually contiguous kernel buffer with `vmalloc()`.
* Marked vmalloc-backed pages as reserved for safe user-space mapping.
* Implemented a character device with major number `423`.
* Implemented the `mmap()` file operation using:

  * `vmalloc_to_pfn()`
  * `remap_pfn_range()`
* Exposed the profiling buffer to a user-space monitor without repeated kernel-user copying.
* Analyzed workload behavior under different memory locality and multiprogramming configurations.

### Sampling Buffer Design

Each sample contains four `unsigned long` values:

```text
[jiffies, minor_faults, major_faults, cpu_utilization]
```

The profiler periodically aggregates data across all registered processes and writes one sample into the shared buffer.

```text
Work processes
    |
    | register PIDs
    v
/proc/mp3/status
    |
    v
Kernel profiler
    |
    ├── Delayed workqueue at 20 Hz
    ├── Page-fault and CPU-usage collection
    ├── vmalloc profiling buffer
    └── Character device mmap interface
             |
             v
User-space monitor
```

### Example Usage

```bash
make
sudo insmod mp3.ko

cat /proc/devices
sudo mknod node c 423 0

echo "R <PID>" | sudo tee /proc/mp3/status
cat /proc/mp3/status

./monitor > profile.data

echo "U <PID>" | sudo tee /proc/mp3/status

sudo rmmod mp3
dmesg
```

### Analysis Workloads

The profiler was designed to study how memory behavior affects system performance.

Example experiments include:

* Random-access workloads with large memory footprints.
* Locality-based access patterns.
* Increasing degree of multiprogramming.
* Page-fault count over time.
* CPU utilization under memory pressure and possible thrashing.

These experiments help show how virtual memory behavior can directly affect throughput and CPU utilization.

## Myfsck

`Myfsck` implements a user-space file-system checker for a simple VSFS-like file-system image.

Unlike the other three projects, `Myfsck` is not a kernel module. It is a user-space consistency checker that directly inspects an on-disk file-system image.

### Key Features

* Parsed a raw file-system image using on-disk metadata structures.
* Used `mmap()` to access the file-system image efficiently.
* Validated superblock metadata.
* Checked inode allocation and inode types.
* Verified direct and indirect block addresses.
* Checked directory formatting, including `.` and `..` entries.
* Verified consistency between inode block usage and the block bitmap.
* Detected duplicated direct block usage.
* Checked file size consistency against allocated data blocks.
* Verified that allocated inodes are reachable from directories.
* Verified that directory entries do not reference free inodes.
* Checked regular-file reference counts.
* Detected illegal multiple directory links.
* Reported errors using the exact required error interface.
* Exited immediately on the first detected inconsistency.

### Consistency Checks

The checker validates the following categories of file-system state:

```text
Superblock consistency
Inode type validity
Direct block address validity
Indirect block address validity
Directory format correctness
Inode-to-bitmap consistency
Bitmap-to-inode consistency
Duplicate data block usage
File size correctness
Allocated inode reachability
Directory reference validity
Regular file link counts
Directory link constraints
```

### Example Usage

```bash
make myfsck

./myfsck good.img
echo $?

./myfsck bad_direct_addr.img
echo $?
```

Expected behavior:

```text
Consistent image:
    exit code 0
    no output

Corrupted image:
    print the first detected error to stderr
    exit code 1
```

## Skills Demonstrated

### Linux Kernel Development

* Linux kernel module structure
* Module initialization and cleanup
* Safe kernel-user communication
* `copy_from_user()`
* `copy_to_user()`
* `printk()` / `dmesg` debugging
* Proc filesystem API
* Kernel linked lists
* Kernel timers
* Workqueues and delayed workqueues
* Kernel mutexes
* Slab allocation
* Character device registration
* `mmap()` implementation in a kernel driver
* `vmalloc()` memory management
* Page frame number translation
* Process state and scheduler interaction

### Operating Systems Concepts

* User/kernel boundary
* Interrupt context vs process context vs kernel-thread context
* Two-half interrupt handling design
* Real-time scheduling
* Rate-Monotonic Scheduling
* Admission control
* Periodic task model
* Preemption
* Virtual memory
* Page faults
* CPU utilization
* Thrashing
* File-system layout
* Superblocks, inodes, bitmaps, and directory entries
* Crash consistency and fsck-style validation

### Systems Programming

* C programming
* Pointer arithmetic
* Raw binary data parsing
* Memory-mapped file access
* Linked data structures
* Concurrency control
* Resource cleanup
* Defensive error handling
* VM-based kernel debugging

## Build Notes

Each project is intended to be built independently.

For kernel-module projects:

```bash
cd CPU_Usage_Tracker
make
sudo insmod mp1.ko
sudo rmmod mp1
```

For the RMS scheduler:

```bash
cd RMS_Linux_Schedular
make
sudo insmod mp2.ko
sudo rmmod mp2
```

For the virtual memory profiler:

```bash
cd Virtual_Memory_Page_Fault_Profiler
make
sudo insmod mp3.ko
sudo rmmod mp3
```

For the file-system checker:

```bash
cd Myfsck
make myfsck
./myfsck <file_system_image>
```

The kernel-module Makefiles may require updating `KERNEL_SRC` to point to the local Linux kernel source tree.

Example:

```make
KERNEL_SRC := /path/to/linux-5.15.165
```

## Why This Repository Matters

This repository demonstrates hands-on operating systems development beyond ordinary user-space programming. The projects require working with real Linux kernel APIs, handling concurrency inside the kernel, understanding scheduling behavior, exposing kernel memory safely to user space, and validating low-level file-system metadata.

Together, these projects show practical experience with:

* Building Linux kernel modules from scratch.
* Designing kernel-user interfaces.
* Implementing real-time scheduling policies.
* Profiling virtual memory behavior with low overhead.
* Understanding how file systems encode and protect persistent state.

