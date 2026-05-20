[![Review Assignment Due Date](https://classroom.github.com/assets/deadline-readme-button-22041afd0340ce965d47ae6ef1cefeee28c7c493a6346c4f15d667ab976d596c.svg)](https://classroom.github.com/a/My8SEkXV)
# MP4

Name: Kaiyang Teng

NetID: kteng4

## Project Introduction

The objective of this MP4 is to implement a user-level file system checker for the provided CS 423 file system image format. The checker reads a file system image, examines the on-disk metadata and file system structures, and reports consistency errors using the required error functions from log.h.

Unlike a repair tool, this implementation focuses on detecting file system inconsistencies. If the image is valid, the checker exits silently with status code 0. If an error is detected, the checker prints the corresponding MP4_ERR message and exits immediately with status code 1.

## Implementation

I implemented the checker in myfsck.c. The program first validates the command-line arguments, opens the input image, and maps it into memory using mmap(). After the image is mapped, the checker reads the superblock from block 1 and locates the inode table starting from block 2. The superblock information is then used to calculate the inode region, bitmap region, and valid data block range.

The checker scans all inodes in the inode table. For each inode, it first verifies that the inode type is either free or one of the valid file system types: directory, regular file, or device. For every in-use inode, the checker validates all direct block addresses and the indirect block address. Each used address must point to a valid data block and must also be marked as used in the bitmap.

While scanning direct and indirect blocks, the checker records which data blocks are actually used. This information is later compared against the bitmap. If a bitmap bit marks a data block as used but no inode or indirect block actually uses it, the checker reports a bitmap inconsistency. The checker also detects duplicate block usage across in-use inodes.

For directory inodes, the checker scans all directory entries in the directory data blocks. It verifies that every directory contains both . and .. entries, and that the . entry points back to the directory itself. During this scan, the checker also records how many times each inode is referenced by directory entries. This reference count is used to detect in-use inodes that are not found in any directory, directory entries that refer to free inodes, incorrect file reference counts, and directories that appear more than once in the file system.

The checker also validates file size consistency. For each in-use inode, the number of data blocks used by that inode is compared with the inode size field. The file size must fit within the range allowed by the number of allocated data blocks.

## Extra Credit Checks

I also implemented the extra credit checks for directory parent relationships and directory reachability.

For the parent directory check, the checker records the inode number stored in each directory’s .. entry. It also records the actual parent-child relationship observed from normal directory entries. After scanning the file system, the checker compares these two relationships. For the root directory, .. must point back to the root itself. For every other directory, the .. entry must match the parent directory that actually points to it.

For the directory reachability check, the checker follows the .. chain of every directory. Each directory must eventually trace back to the root directory. If the checker detects an invalid parent, a non-directory parent, a missing parent, or a loop in the parent chain, it reports an inaccessible directory error.

## Testing

I tested the checker using the provided good.img and bad_direct_addr.img images. The valid image exits with status code 0 and produces no output. The corrupted direct-address image correctly reports:
MP4_ERR: bad direct address in inode.
I also used the provided mkfs and lsfs tools to generate and inspect additional file system images during testing.

