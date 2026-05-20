#ifndef LOG_H
#define LOG_H

#include <stdio.h>
#include <stdlib.h>

static inline void superblock_error() // whether the super block have the correct size
{
    fprintf(stderr, "MP4_ERR: superblock is corrupted.\n");
}

static inline void bad_inode_error() // type of inode is not 0 1 2 3
{
    fprintf(stderr, "MP4_ERR: bad inode.\n");
}

static inline void bad_directory_format_error()// directory dont have . / ..
{
    fprintf(stderr, "MP4_ERR: directory not properly formatted.\n");
}

static inline void bad_direct_address_error() // invalid block number / not in data range
{
    fprintf(stderr, "MP4_ERR: bad direct address in inode.\n");
}

static inline void bad_direct_address_once_error()// same direct blcok number appear in more than one inodes
{
    fprintf(stderr, "MP4_ERR: direct address used more than once.\n");
}

static inline void bad_inode_referred_marked_error()// for all inode of dirent structs, it must have a vaild type
{
    fprintf(stderr,
            "MP4_ERR: inode referred to in directory but marked free.\n");
}

static inline void bad_indirect_address_error() // invalid indirect block number/ block number within indirect block
{
    fprintf(stderr, "MP4_ERR: bad indirect address in inode.\n");
}

static inline void bad_file_size_error()// size of inode do not match actual match
{
    fprintf(stderr, "MP4_ERR: incorrect file size in inode.\n");
}

static inline void bad_directory_once_error()// a directory can only have one parent
{
    fprintf(stderr,
            "MP4_ERR: directory appears more than once in file system.\n");
}

static inline void bad_reference_count_error()// for type=file, the nlink in inode struct must be correct
{
    fprintf(stderr, "MP4_ERR: bad reference count for file.\n");
}

static inline void bad_bitmap_marked_used_error()// bit map mark the blcok but block not in use
{
    fprintf(stderr,
            "MP4_ERR: bitmap marks block in use but it is not in use.\n");
}

static inline void bad_used_inode_not_found_error()// if inode type is 123 but no dirent struct have the inode number
{
    fprintf(stderr,
            "MP4_ERR: inode marked used but not found in a directory.\n");
}

static inline void bad_used_address_error()// the block which inode using not in bit map
{
    fprintf(stderr,
            "MP4_ERR: address used by inode but marked free in bitmap.\n");
}




static inline void bad_parent_directory_error() {
    fprintf(stderr, "MP4_ERR: parent directory mismatch.\n");
}

static inline void bad_directory_error() {
    fprintf(stderr, "MP4_ERR: inaccessible directory.\n");
}

static inline void bad_indirect_address_once_error() {
    fprintf(stderr, "MP4_ERR: indirect address used more than once.\n");
}


#endif // LOG_H