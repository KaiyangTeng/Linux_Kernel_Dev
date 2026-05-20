#include "types.h"
#include "log.h"
#include "fs.h"
#include <stddef.h>
#include <stdio.h>
#include <fcntl.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <unistd.h>
#include <stdbool.h>

char * imgpath;
void * img;
struct superblock * spblcok;
struct dinode * startinode;
uint sbsize,nblocks,ninodes,datblkstnum,datblkednum;

// Check whether the superblock layout is consistent.
// Also calculates the valid data block range.
void check_size(void)
{
    uint inodezie=ninodes/IPB+1,bitmapsize=sbsize/BPB+1;
    datblkstnum=inodezie+bitmapsize+2;
    datblkednum=nblocks+datblkstnum;
    if(sbsize!=datblkednum)
    {
        superblock_error();
        exit(1);
    }
}

// Check whether an inode type is valid.
void check_node_type(struct dinode * temp)
{
    short type=temp->type;
    if(type!=0&&type!=1&&type!=2&&type!=3)
    {
        bad_inode_error();
        exit(1);
    }
}

// Return whether a block is marked as used in the bitmap.
bool bit_is_used(uint blknum)
{
    char * start=(char*)img+BBLOCK(blknum,ninodes)*BSIZE;
    uint bytepos=(blknum%BPB)/8;
    uint bitpos=blknum%8;
    return start[bytepos]&(1<<bitpos);
}


// Traverse one directory data block.
// This checks "." and "..", counts inode references,
// and records parent information for extra credit checks.
void traverse_dirent(uint blknum,ushort currinodenum,int * curcnt,int * parcnt,int inodeindirent[],int parentdotdot[],int parententry[])
{
    struct dirent * startptr=(struct dirent *)((char *)img+blknum*BSIZE);
    int nentry=BSIZE/sizeof(struct dirent);
    for(int i=0;i<nentry;i++)
    {
        struct dirent * tempdirent=startptr+i;
        if(tempdirent->inum!=0)
        {
            if(strncmp(".",tempdirent->name,DIRSIZ)==0)
            {
                // "." must point to the directory itself.
                if(tempdirent->inum!=currinodenum)
                {
                    bad_directory_format_error();
                    exit(1);
                }
                if(currinodenum==ROOTINO) inodeindirent[ROOTINO]+=1;
                (*curcnt)++;
            }
            else if(strncmp("..",tempdirent->name,DIRSIZ)==0) 
            {
                // Record the parent inode stored in "..".
                parentdotdot[currinodenum]=tempdirent->inum;
                (*parcnt)++;
            }
            else 
            {
                // Count normal directory references.
                inodeindirent[tempdirent->inum]+=1;
                // If the child is a directory, record its actual parent.
                struct dinode * child=startinode+tempdirent->inum;
                if(child->type==1) parententry[tempdirent->inum]=currinodenum;
            }
        }
    }
}


// Check all direct addresses of one inode.
// It also records used blocks and traverses directory blocks if needed.
int check_direct_address(struct dinode * inode,bool visited[],ushort currinodenum,int inodeindirent[],int * curcnt,int * parcnt,int parentdotdot[],int parententry[])
{
    int blkcnt=0;
    for(int i=0;i<NDIRECT;i++)
    {
        uint tempblknum=inode->addrs[i];
        // Direct block address must be inside the valid data block range.
        if(tempblknum!=0&&(tempblknum<datblkstnum||tempblknum>=datblkednum))
        {
            bad_direct_address_error();
            exit(1);
        }
        // A direct data block cannot be used more than once.
        if(tempblknum!=0&&visited[tempblknum])
        {
            bad_direct_address_once_error();
            exit(1);
        }
        if(tempblknum!=0)
        {
            // Any block used by an inode must be marked used in bitmap.
            if(!bit_is_used(tempblknum))
            {
                bad_used_address_error();
                exit(1);
            }
            visited[tempblknum]=1;
            blkcnt++;
            // If this inode is a directory, scan its directory entries.
            if(inode->type==1) 
            {
                traverse_dirent(tempblknum,currinodenum,curcnt,parcnt,inodeindirent,parentdotdot,parententry);
            }
        }
    }
    return blkcnt;
}

// Check the indirect block and all data blocks referenced by it.
int check_indirect_address(struct dinode * inode,bool visited[],ushort currinodenum,int inodeindirent[],int * curcnt,int * parcnt,int parentdotdot[],int parententry[])
{
    int blkcnt=0;
    uint ndirblknum=inode->addrs[NDIRECT];
    if(ndirblknum!=0)
    {
        // The indirect block itself must be a valid data block.
        if(ndirblknum>=datblkstnum&&ndirblknum<datblkednum)
        {
            if(visited[ndirblknum])
            {
                bad_indirect_address_once_error();
                exit(1);
            }
            if(!bit_is_used(ndirblknum))
            {
                bad_used_address_error();
                exit(1);
            }
            visited[ndirblknum]=1;
            // blkcnt++;
            uint * startptr=(uint *)((char *)img+ndirblknum*BSIZE);
            for(int i=0;i<128;i++)
            {
                uint temp=startptr[i];
                // Each block number inside the indirect block must be valid.
                if(temp!=0&&(temp<datblkstnum||temp>=datblkednum))
                {
                    bad_indirect_address_error();
                    exit(1);    
                }
                if(temp!=0&&visited[temp])
                {
                    bad_indirect_address_once_error();
                    exit(1);
                }
                if(temp!=0) 
                {
                    if(!bit_is_used(temp))
                    {
                        bad_used_address_error();
                        exit(1);
                    }
                    visited[temp]=1;
                    blkcnt++;
                    if(inode->type==1) 
                    {
                        traverse_dirent(temp,currinodenum,curcnt,parcnt,inodeindirent,parentdotdot,parententry);
                    }
                }
            }
        }
        else 
        {
            bad_indirect_address_error();
            exit(1);
        }
    }
    return blkcnt;
}

void helper()
{
    // blkvisited records blocks actually used by inode addresses.
    // usedinode records in-use inodes.
    // inodeindirent counts how many times each inode is referenced by directories.
    bool blkvisited[sbsize+1];
    bool usedinode[ninodes+1];
    int inodeindirent[ninodes+1];
    // Extra credit parent tracking.
    // parentdotdot[i] is the parent stored in inode i's ".." entry.
    // parententry[i] is the actual parent that contains inode i as a child entry.
    int parentdotdot[ninodes+1];
    int parententry[ninodes+1];
    memset(blkvisited,0,sizeof(blkvisited));
    memset(usedinode,0,sizeof(usedinode));
    memset(inodeindirent,0,sizeof(inodeindirent));
    memset(parentdotdot,-1,sizeof(parentdotdot));
    memset(parententry,-1,sizeof(parententry));
    // First pass: scan all inodes, validate addresses, scan directories,
    // and collect block/inode reference information.
    for(uint i=0;i<ninodes;i++)
    {
        struct dinode * temp=startinode+i;
        check_node_type(temp);
        if(i!=0&&temp->type!=0) 
        {
            int curcnt=0,parcnt=0;
            int dircnt=check_direct_address(temp,blkvisited,i,inodeindirent,&curcnt,&parcnt,parentdotdot,parententry);
            int indircnt=check_indirect_address(temp,blkvisited,i,inodeindirent,&curcnt,&parcnt,parentdotdot,parententry);
            int sum=dircnt+indircnt;
            // A directory must contain exactly one "." and one ".." entry.
            if(temp->type==1&&(curcnt!=1||parcnt!=1)) 
            {
                bad_directory_format_error();
                exit(1);
            }
            // File size must match the number of data blocks used.
            if(sum==0)
            {
                if(temp->size!=0)
                {
                    bad_file_size_error();
                    exit(1);
                }
            }
            else 
            {
                if(temp->size<=(uint)(sum-1)*BSIZE||temp->size>(uint)sum*BSIZE)
                {
                    bad_file_size_error();
                    exit(1);
                }
            }
            usedinode[i]=1;
        }
    }

    // Bitmap check: every block marked used must actually be used.
    for(uint i=datblkstnum;i<datblkednum;i++)
    {
        if(bit_is_used(i)&&!blkvisited[i])
        {
            bad_bitmap_marked_used_error();
            exit(1);
        }
    }

    // Check inode reference consistency and directory reference rules.
    for(uint i=1;i<ninodes;i++)
    {
        struct dinode * temp=startinode+i;
        if(usedinode[i]&&!inodeindirent[i])
        {
            bad_used_inode_not_found_error();
            exit(1);
        }
        if(!usedinode[i]&&inodeindirent[i])
        {
            bad_inode_referred_marked_error();
            exit(1);
        }
        if(temp->type==2&&temp->nlink!=inodeindirent[i])
        {
            bad_reference_count_error();
            exit(1);
        }
        if(temp->type==1&&inodeindirent[i]!=1)
        {
            bad_directory_once_error();
            exit(1);
        }
        // Extra credit: check parent relationship and root reachability.
        if(temp->type==1)
        {
            if(i==ROOTINO)
            {
                if(parentdotdot[i]!=ROOTINO)
                {
                    bad_parent_directory_error();
                    exit(1);
                }
            }
            else
            {
                if(parententry[i]==-1||parentdotdot[i]!=parententry[i])
                {
                    bad_parent_directory_error();
                    exit(1);
                }
            }
            // Follow ".." chain. It must eventually reach ROOTINO.
            bool seen[ninodes+1];
            memset(seen,0,sizeof(seen));
            int cur=i;
            while(cur!=ROOTINO)
            {
                if(seen[cur])
                {
                    bad_directory_error();
                    exit(1);
                }
                seen[cur]=1;
                cur=parentdotdot[cur];
                if(cur==-1)
                {
                    bad_directory_error();
                    exit(1);
                }
            }
        }
    }

    
}

int main(int argc, char *argv[]) 
{
    if(argc==2)
    {
        imgpath=argv[1];
    }
    else 
    {
        fprintf(stderr, "Usage: myfsck <file_system_image>\n");
        exit(1);
    }
    int fd=open(imgpath,O_RDONLY);
    if(fd<0)
    {
        fprintf(stderr, "image not found.\n"); 
        exit(1);
    }
    struct stat st;
    if(fstat(fd,&st)<0)
    {
        close(fd);
        exit(1);
    }
    // Map the whole file system image into memory.
    img=mmap(NULL,st.st_size,PROT_READ, MAP_PRIVATE, fd, 0);
    if(img==MAP_FAILED) 
    {
        close(fd);
        exit(1);
    }
    // Locate superblock and inode table.
    spblcok=(struct superblock *)((char*)img+BSIZE);
    startinode=(struct dinode *)((char*)spblcok+BSIZE);
    sbsize=spblcok->size;
    nblocks=spblcok->nblocks;
    ninodes=spblcok->ninodes;
    check_size();
    helper();
    return 0;
}




















