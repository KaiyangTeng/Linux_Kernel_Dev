# CPU Usage Tracker

## Introduction

This project is a Linux kernel module that tracks the user CPU time of selected processes through the proc filesystem. User-space programs register a PID by writing to `/proc/mp1/status`, and the module periodically refreshes CPU usage for all tracked processes. The current tracking state can then be read back from the same proc entry.

The implementation is built around a small but realistic kernel design: procfs for the user/kernel interface, a linked list for process bookkeeping, a timer for periodic scheduling, a workqueue for deferred updates, a mutex for synchronization, and a slab cache for node allocation.

A small user-space test program is included to register itself, generate CPU load, and verify that the module reports the expected output.

---

## Features

* Registers processes by PID through `/proc/mp1/status`
* Tracks per-process user CPU time
* Refreshes tracked values periodically in the kernel
* Automatically removes processes that are no longer alive
* Exposes the current state through a procfs text interface
* Uses deferred work instead of doing heavy updates directly in the timer callback
* Cleans up all kernel resources correctly on module unload

---

## How It Works

The module maintains an internal list of tracked processes. Each entry stores:

* process ID
* last observed user CPU time
* linked-list metadata

When a PID is written into `/proc/mp1/status`, the module adds a new tracking node for that process.

A kernel timer fires every 5 seconds. The timer callback itself stays lightweight and only schedules a workqueue job. The workqueue handler walks the tracked list, calls `get_cpu_use()` for each PID, updates the stored CPU time, and removes entries whose processes have already exited.

Reading `/proc/mp1/status` returns a snapshot of the tracked list in text form:

```text
<pid>: <cpu_time>
```

This makes the module easy to test from normal shell tools like `echo` and `cat`.

---

## Project Layout

```text
.
├── mp1.c        # kernel module
├── userapp.c    # user-space test program
├── Makefile
└── README.md
```

---

## Build

Build the kernel module and test program from the project directory:

```bash
make
```

This should generate:

* `mp1.ko`
* `userapp`

If you need to build manually:

```bash
make -C /path/to/linux-5.15.165 M=$(PWD) modules
gcc -O2 -Wall userapp.c -o userapp
```

Make sure the kernel source path in the Makefile matches your local environment.

---

## Load the Module

```bash
sudo insmod mp1.ko
```

Check that the proc entry was created:

```bash
ls /proc/mp1
cat /proc/mp1/status
```

Unload the module:

```bash
sudo rmmod mp1
```

---

## Usage

### Register a process

Write a PID into the proc file:

```bash
echo 1234 | sudo tee /proc/mp1/status
```

### Read tracked results

```bash
cat /proc/mp1/status
```

Example output:

```text
1234: 450
5678: 1290
```

Each line contains one tracked PID and its current user CPU time.

---

## Running the Test Program

The included user program exercises the whole pipeline:

1. gets its own PID
2. writes that PID into `/proc/mp1/status`
3. performs CPU-intensive work
4. reads the proc file back and prints the result

Run it with:

```bash
./userapp
```

A typical workflow looks like this:

```bash
sudo insmod mp1.ko
./userapp
cat /proc/mp1/status
sudo rmmod mp1
```

---

## Example Session

```bash
$ sudo insmod mp1.ko
$ ./userapp
userapp pid = 4321
$ cat /proc/mp1/status
4321: 872
$ sudo rmmod mp1
```

---

## Design Overview

### Procfs interface

The module exposes a single proc entry:

```text
/proc/mp1/status
```

This file is both readable and writable.

* `write`: register a PID
* `read`: return the current tracked state

This keeps the interface minimal and easy to inspect from user space.

### Internal data structure

Tracked processes are stored in a kernel linked list. Each node represents one PID currently under observation.

A slab cache is used for node allocation so that all tracking objects are created and destroyed through a dedicated kernel allocator path.

### Periodic refresh path

The refresh logic is split into two stages:

* timer callback
* workqueue handler

The timer only schedules work and rearms itself. The real update happens in process context inside the workqueue handler, where it is safe to walk the list, call helper routines, and delete dead entries.

### Synchronization

A mutex protects the tracking list from concurrent access across:

* proc writes
* proc reads
* periodic updates
* module cleanup

Without this lock, registration, removal, and snapshot generation could race with each other.

---

## Implementation Notes

### Registration path

When user space writes a PID string into `/proc/mp1/status`, the module:

1. copies the input into a kernel buffer
2. trims and parses it
3. checks whether the PID is already tracked
4. allocates a new list node
5. inserts it into the tracking list

Newly registered entries start with CPU time `0` and are updated during the next periodic refresh.

### Update path

Every 5 seconds, the workqueue handler scans all tracked entries.

For each PID:

* if `get_cpu_use()` succeeds, the stored CPU time is refreshed
* if `get_cpu_use()` fails, the process is treated as dead and removed from the list

This keeps the proc output current without requiring reads to perform heavy work.

### Read path

The read handler formats the current list into a kernel buffer and copies it out to user space. It supports file offsets correctly, so commands like `cat` behave as expected instead of looping forever.

### Cleanup path

When the module is unloaded, it:

1. stops the timer
2. flushes pending work
3. frees all remaining list nodes
4. destroys the slab cache
5. removes proc entries

This ensures no asynchronous work is still touching memory after teardown.

---

## Error Handling

The module rejects invalid input and propagates standard kernel-style errors where appropriate.

Examples include:

* malformed PID input
* failed user-kernel copies
* duplicate registration
* allocation failures

This makes failures visible and easier to diagnose from user space.

---

## Limitations

This project is intentionally small and focused.

A few practical limits remain:

* the proc read path uses a bounded kernel buffer for formatting output
* registration expects plain PID text input
* CPU usage is refreshed periodically, not continuously
* this module tracks user CPU time only through the provided helper interface

These are acceptable tradeoffs for a clean and focused kernel module design.

---

## Why the Design Looks Like This

This implementation tries to follow kernel style in a simple way.

The main design choices are deliberate:

* **procfs** provides a minimal user/kernel interface
* **linked list** keeps process bookkeeping straightforward
* **timer + workqueue** separates scheduling from heavier update work
* **mutex** prevents list corruption and read/update races
* **slab cache** gives fixed-size allocation for tracking nodes

For a project of this size, that combination is simple, robust, and easy to reason about.

---

## Development and Debugging

Useful commands while testing:

```bash
dmesg | tail
dmesg -w
cat /proc/mp1/status
echo $$ | sudo tee /proc/mp1/status
ps -p <pid>
```

To start with a clean kernel log:

```bash
sudo dmesg -C
```

To rebuild and reload quickly:

```bash
make
sudo rmmod mp1 2>/dev/null
sudo insmod mp1.ko
```

---

## Summary

This project implements a small kernel-side process tracker with a clean procfs interface and a realistic internal structure. Even though it is compact, it touches several important kernel programming patterns: procfs operations, linked-list management, synchronization, deferred work, timers, slab allocation, and safe module teardown.

It is a good example of how to build a stateful kernel module that interacts cleanly with user space while keeping asynchronous kernel activity under control.


