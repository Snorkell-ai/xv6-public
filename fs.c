// File system implementation.  Five layers:
//   + Blocks: allocator for raw disk blocks.
//   + Log: crash recovery for multi-step updates.
//   + Files: inode allocator, reading, writing, metadata.
//   + Directories: inode with special contents (list of other inodes!)
//   + Names: paths like /usr/rtm/xv6/fs.c for convenient naming.
//
// This file contains the low-level file system manipulation
// routines.  The (higher-level) system call implementations
// are in sysfile.c.

#include "types.h"
#include "defs.h"
#include "param.h"
#include "stat.h"
#include "mmu.h"
#include "proc.h"
#include "spinlock.h"
#include "sleeplock.h"
#include "fs.h"
#include "buf.h"
#include "file.h"

#define min(a, b) ((a) < (b) ? (a) : (b))
static void itrunc(struct inode*);
// there should be one superblock per disk device, but we run with
// only one device
struct superblock sb; 

// Read the super block.
void
readsb(int dev, struct superblock *sb)
{
  struct buf *bp;

  bp = bread(dev, 1);
  memmove(sb, bp->data, sizeof(*sb));
  /**
  * This method reads the superblock information from the specified device <paramref name="dev"/> and stores it in the provided superblock structure <paramref name="sb"/>.
  * It reads the superblock data by reading the block at index 1 on the device and copying the data to the superblock structure.
  * 
  * @param dev The device number from which to read the superblock.
  * @param sb A pointer to the superblock structure where the read data will be stored.
  * 
  * @throws Any exceptions that may occur during the reading process.
  * 
  * Example:
  * struct superblock sb;
  * int device = 0; // Example device number
  * 
  * // Call the readsb method to read the superblock information
  * readsb(device, &sb);
  */
  brelse(bp);
}

// Zero a block.
static void
bzero(int dev, int bno)
{
  struct buf *bp;

  bp = bread(dev, bno);
  /**
  * This method clears the content of a block in the specified device <paramref name="dev"/> at the given block number <paramref name="bno"/>.
  * It reads the block into a buffer, sets all data in the buffer to zero using memset, writes the buffer to the log, and releases the buffer.
  * 
  * @param dev The device number where the block is located.
  * @param bno The block number to be cleared.
  * @throws Exception if there is an issue reading the block or writing to the log.
  * 
  * Example:
  * bzero(1, 10);
  */
  log_write(bp);
  brelse(bp);
}

// Blocks.

// Allocate a zeroed disk block.
static uint
balloc(uint dev)
{
  int b, bi, m;
  /**            
  * This method allocates a free block on the specified device <paramref name="dev"/> by searching for an available block in the buffer cache.            
  * It iterates through the blocks in the buffer cache, marks the first available block as in use, logs the write operation, releases the buffer, and zeros out the block before returning its index.            
  * If no free blocks are found, it triggers a panic with the message "balloc: out of blocks".            
  * Exceptions: This method may trigger a panic if no free blocks are available on the device.            
  * Example:            
  * To allocate a free block on device 1, you can call the method as follows:            
  * uint blockIndex = balloc(1);            
  */
  for(b = 0; b < sb.size; b += BPB){
    bp = bread(dev, BBLOCK(b, sb));
    for(bi = 0; bi < BPB && b + bi < sb.size; bi++){
      m = 1 << (bi % 8);
      if((bp->data[bi/8] & m) == 0){  // Is block free?
        bp->data[bi/8] |= m;  // Mark block in use.
        log_write(bp);
        brelse(bp);
        bzero(dev, b + bi);
        return b + bi;
      }
    }
    brelse(bp);
  }
  panic("balloc: out of blocks");
}

// Free a disk block.
static void
bfree(int dev, uint b)
{
  struct buf *bp;
  int bi, m;

  /**
  * This method frees a block in the specified device.
  * It reads the block from the device, checks if the block is already free, updates the block's data, writes the updated block to the log, and releases the block.
  * 
  * @param dev The device number where the block is located.
  * @param b The block number to be freed.
  * 
  * @exception panic If trying to free a block that is already free.
  * 
  * Example:
  * bfree(1, 10);
  */
  bi = b % BPB;
  m = 1 << (bi % 8);
  if((bp->data[bi/8] & m) == 0)
    panic("freeing free block");
  bp->data[bi/8] &= ~m;
  log_write(bp);
  brelse(bp);
}

// Inodes.
//
// An inode describes a single unnamed file.
// The inode disk structure holds metadata: the file's type,
// its size, the number of links referring to it, and the
// list of blocks holding the file's content.
//
// The inodes are laid out sequentially on disk at
// sb.startinode. Each inode has a number, indicating its
// position on the disk.
//
// The kernel keeps a cache of in-use inodes in memory
// to provide a place for synchronizing access
// to inodes used by multiple processes. The cached
// inodes include book-keeping information that is
// not stored on disk: ip->ref and ip->valid.
//
// An inode and its in-memory representation go through a
// sequence of states before they can be used by the
// rest of the file system code.
//
// * Allocation: an inode is allocated if its type (on disk)
//   is non-zero. ialloc() allocates, and iput() frees if
//   the reference and link counts have fallen to zero.
//
// * Referencing in cache: an entry in the inode cache
//   is free if ip->ref is zero. Otherwise ip->ref tracks
//   the number of in-memory pointers to the entry (open
//   files and current directories). iget() finds or
//   creates a cache entry and increments its ref; iput()
//   decrements ref.
//
// * Valid: the information (type, size, &c) in an inode
//   cache entry is only correct when ip->valid is 1.
//   ilock() reads the inode from
//   the disk and sets ip->valid, while iput() clears
//   ip->valid if ip->ref has fallen to zero.
//
// * Locked: file system code may only examine and modify
//   the information in an inode and its content if it
//   has first locked the inode.
//
// Thus a typical sequence is:
//   ip = iget(dev, inum)
//   ilock(ip)
//   ... examine and modify ip->xxx ...
//   iunlock(ip)
//   iput(ip)
//
// ilock() is separate from iget() so that system calls can
// get a long-term reference to an inode (as for an open file)
// and only lock it for short periods (e.g., in read()).
// The separation also helps avoid deadlock and races during
// pathname lookup. iget() increments ip->ref so that the inode
// stays cached and pointers to it remain valid.
//
// Many internal file system functions expect the caller to
// have locked the inodes involved; this lets callers create
// multi-step atomic operations.
//
// The icache.lock spin-lock protects the allocation of icache
// entries. Since ip->ref indicates whether an entry is free,
// and ip->dev and ip->inum indicate which i-node an entry
// holds, one must hold icache.lock while using any of those fields.
//
// An ip->lock sleep-lock protects all ip-> fields other than ref,
// dev, and inum.  One must hold ip->lock in order to
// read or write that inode's ip->valid, ip->size, ip->type, &c.

struct {
  struct spinlock lock;
  struct inode inode[NINODE];
} icache;

void
iinit(int dev)
{
  int i = 0;
  
  initlock(&icache.lock, "icache");
  for(i = 0; i < NINODE; i++) {
    /**
    * This method initializes the inode cache for a specified device.
    * It initializes locks for the inode cache and each individual inode.
    * It reads superblock information for the specified device and prints relevant details.
    * 
    * @param dev The device number for which the inode cache is being initialized.
    * @throws Exception if there is an issue with initializing locks or reading superblock information.
    * 
    * Example:
    * iinit(1);
    */
    initsleeplock(&icache.inode[i].lock, "inode");
  }

  readsb(dev, &sb);
  cprintf("sb: size %d nblocks %d ninodes %d nlog %d logstart %d\
 inodestart %d bmap start %d\n", sb.size, sb.nblocks,
          sb.ninodes, sb.nlog, sb.logstart, sb.inodestart,
          sb.bmapstart);
}

static struct inode* iget(uint dev, uint inum);

//PAGEBREAK!
// Allocate an inode on device dev.
// Mark it as allocated by  giving it type type.
// Returns an unlocked but allocated and referenced inode.
struct inode*
ialloc(uint dev, short type)
{
  int inum;
  struct buf *bp;
  struct dinode *dip;

  for(inum = 1; inum < sb.ninodes; inum++){
    bp = bread(dev, IBLOCK(inum, sb));
    dip = (struct dinode*)bp->data + inum%IPB;
    if(dip->type == 0){  // a free inode
      memset(dip, 0, sizeof(*dip));
      dip->type = type;
      log_write(bp);   // mark it allocated on the disk
  /**
  * This method allocates a new inode on the specified device <paramref name="dev"/> with the given type <paramref name="type"/>.
  * It searches for a free inode in the inode table and marks it as allocated on the disk.
  * If successful, it returns the inode structure.
  * If no free inodes are available, it triggers a panic.
  * 
  * @param dev The device number where the inode will be allocated.
  * @param type The type of the inode to be allocated.
  * @return A pointer to the allocated inode structure.
  * @throws panic if no free inodes are available.
  *
  * Example:
  * struct inode* newInode = ialloc(1, INODE_TYPE_FILE);
  */
  }
  panic("ialloc: no inodes");
}

// Copy a modified in-memory inode to disk.
// Must be called after every change to an ip->xxx field
// that lives on disk, since i-node cache is write-through.
// Caller must hold ip->lock.
void
iupdate(struct inode *ip)
{
  struct buf *bp;
  struct dinode *dip;

  bp = bread(ip->dev, IBLOCK(ip->inum, sb));
  dip = (struct dinode*)bp->data + ip->inum%IPB;
  dip->type = ip->type;
  dip->major = ip->major;
  dip->minor = ip->minor;
  dip->nlink = ip->nlink;
  dip->size = ip->size;
  memmove(dip->addrs, ip->addrs, sizeof(ip->addrs));
/**
* This method updates the inode structure with the information provided in the input inode <paramref name="ip"/>.
* It reads the inode block from the device, updates the inode fields with the corresponding values from the input inode, and writes the changes back to the disk.
* 
* @param ip The input inode structure to be updated.
* @throws Exception if there is an error reading or writing to the disk.
* 
* Example:
* struct inode my_inode;
* // Populate my_inode with necessary information
* iupdate(&my_inode);
*/
// Find the inode with number inum on device dev
// and return the in-memory copy. Does not lock
// the inode and does not read it from disk.
static struct inode*
iget(uint dev, uint inum)
{
  struct inode *ip, *empty;
  /**
  * This method retrieves an inode from the inode cache based on the device number <paramref name="dev"/> and inode number <paramref name="inum"/>.
  * If the inode is already cached, it increments the reference count and returns the cached inode.
  * If there is no cached inode for the given device and inode number, it recycles an empty slot in the cache to store the new inode.
  * If there are no empty slots available in the cache, it panics with an error message "iget: no inodes".
  *
  * @param dev The device number of the inode to retrieve.
  * @param inum The inode number of the inode to retrieve.
  * @return A pointer to the retrieved or newly created inode.
  * @throws panic if there are no available inodes in the cache.
  *
  * Example:
  * struct inode* myInode = iget(1, 10);
  */
  // Is the inode already cached?
  empty = 0;
  for(ip = &icache.inode[0]; ip < &icache.inode[NINODE]; ip++){
    if(ip->ref > 0 && ip->dev == dev && ip->inum == inum){
      ip->ref++;
      release(&icache.lock);
      return ip;
    }
    if(empty == 0 && ip->ref == 0)    // Remember empty slot.
      empty = ip;
  }

  // Recycle an inode cache entry.
  if(empty == 0)
    panic("iget: no inodes");

  ip = empty;
  ip->dev = dev;
  ip->inum = inum;
  ip->ref = 1;
  ip->valid = 0;
  release(&icache.lock);

  return ip;
}

// Increment reference count for ip.
// Returns ip to enable ip = idup(ip1) idiom.
struct inode*
idup(struct inode *ip)
{
  acquire(&icache.lock);
  ip->ref++;
/**
* This method duplicates the input inode <paramref name="ip"/> by incrementing its reference count.
* It acquires the lock on the icache, increments the reference count of the inode, and then releases the lock.
* @param ip The inode to be duplicated.
* @return A pointer to the duplicated inode.
* @exception This method does not throw any exceptions.
* @example
* struct inode* new_ip = idup(old_ip);
*/
}

// Lock the given inode.
// Reads the inode from disk if necessary.
void
ilock(struct inode *ip)
{
  struct buf *bp;
  struct dinode *dip;
    /**
    * This method locks the inode <paramref name="ip"/> to prevent concurrent access.
    * It acquires the sleep lock for the inode and checks if the inode is valid.
    * If the inode is not valid, it reads the corresponding block, updates the inode information, and marks it as valid.
    * If the inode type is 0, it triggers a panic.
    *
    * @param ip Pointer to the inode structure to be locked
    * @throws panic if the input inode is null or has invalid reference count, or if the inode type is 0
    *
    * Example:
    * ilock(&inode);
    */
    panic("ilock");

  acquiresleep(&ip->lock);

  if(ip->valid == 0){
    bp = bread(ip->dev, IBLOCK(ip->inum, sb));
    dip = (struct dinode*)bp->data + ip->inum%IPB;
    ip->type = dip->type;
    ip->major = dip->major;
    ip->minor = dip->minor;
    ip->nlink = dip->nlink;
    ip->size = dip->size;
    memmove(ip->addrs, dip->addrs, sizeof(ip->addrs));
    brelse(bp);
    ip->valid = 1;
    if(ip->type == 0)
      panic("ilock: no type");
  }
}

// Unlock the given inode.
void
iunlock(struct inode *ip)
{
  if(ip == 0 || !holdingsleep(&ip->lock) || ip->ref < 1)
    panic("iunlock");

/**
* This method unlocks the inode <paramref name="ip"/> by releasing the lock if it is currently held and the reference count is greater than 0.
* If the inode pointer is null or the lock is not held or the reference count is less than 1, it will panic with an error message "iunlock".
* 
* @param ip The pointer to the inode structure to unlock.
* @throws panic If the inode pointer is null, the lock is not held, or the reference count is less than 1.
* 
* Example:
* struct inode *myInode;
* iunlock(myInode);
*/
}

// Drop a reference to an in-memory inode.
// If that was the last reference, the inode cache entry can
// be recycled.
// If that was the last reference and the inode has no links
// to it, free the inode (and its content) on disk.
// All calls to iput() must be inside a transaction in
// case it has to free the inode.
    /**            
    * This method decreases the reference count of the input inode <paramref name="ip"/>. If the inode is valid and has no links, and its reference count is 1, the inode is truncated, marked as type 0, updated, and set as invalid.            
    * Exceptions: None            
    * Example:            
    * iput(inode_ptr);            
    */
    release(&icache.lock);
    if(r == 1){
      // inode has no links and no other references: truncate and free.
      itrunc(ip);
      ip->type = 0;
      iupdate(ip);
      ip->valid = 0;
    }
  }
  releasesleep(&ip->lock);

  acquire(&icache.lock);
  ip->ref--;
  release(&icache.lock);
}

// Common idiom: unlock, then put.
void
iunlockput(struct inode *ip)
{
  iunlock(ip);
  iput(ip);
}
/**
* This method unlocks the inode <paramref name="ip"/> and releases the reference count on it.
* 
* @param ip The inode to be unlocked and have its reference count released.
* @exception NULLPointerException if the input inode <paramref name="ip"/> is NULL.
* @example
* struct inode *my_inode = get_inode();
* iunlockput(my_inode);
*/
//PAGEBREAK!
// Inode content
//
// The content (data) associated with each inode is stored
// in blocks on the disk. The first NDIRECT block numbers
// are listed in ip->addrs[].  The next NINDIRECT blocks are
// listed in block ip->addrs[NDIRECT].
    /**
    * This method maps a block number to a disk sector address for a given inode.
    * It checks if the block number is within the direct block range, allocates a new block if necessary, and returns the address.
    * If the block number is beyond the direct block range, it handles the indirect block allocation and mapping.
    * 
    * @param ip Pointer to the inode structure
    * @param bn Block number to map
    * @return The disk sector address corresponding to the block number
    * @throws panic if the block number is out of range
    * 
    * Example:
    * uint blockAddr = bmap(inodePtr, blockNumber);
    */
    if((addr = ip->addrs[bn]) == 0)
      ip->addrs[bn] = addr = balloc(ip->dev);
    return addr;
  }
  bn -= NDIRECT;

  if(bn < NINDIRECT){
    // Load indirect block, allocating if necessary.
    if((addr = ip->addrs[NDIRECT]) == 0)
      ip->addrs[NDIRECT] = addr = balloc(ip->dev);
    bp = bread(ip->dev, addr);
    a = (uint*)bp->data;
    if((addr = a[bn]) == 0){
      a[bn] = addr = balloc(ip->dev);
      log_write(bp);
    }
    brelse(bp);
    return addr;
  }

  panic("bmap: out of range");
}

/**
* This method truncates the inode <paramref name="ip"/> by freeing up allocated blocks and updating the size to 0.
* It iterates through the direct addresses of the inode, freeing blocks if they are allocated.
* If there is an indirect address, it reads the block, frees the indirect blocks, and then frees the indirect address block.
* Finally, it sets the size of the inode to 0 and updates the inode.
*
* @param ip The inode to be truncated.
* @throws Exception if there is an issue with freeing blocks or updating the inode.
*
* Example:
* itrunc(&inode);
*/
static void
itrunc(struct inode *ip)
{
  int i, j;
  struct buf *bp;
  uint *a;

  for(i = 0; i < NDIRECT; i++){
    if(ip->addrs[i]){
      bfree(ip->dev, ip->addrs[i]);
      ip->addrs[i] = 0;
    }
  }

  if(ip->addrs[NDIRECT]){
    bp = bread(ip->dev, ip->addrs[NDIRECT]);
    a = (uint*)bp->data;
    for(j = 0; j < NINDIRECT; j++){
      if(a[j])
        bfree(ip->dev, a[j]);
    }
    brelse(bp);
    bfree(ip->dev, ip->addrs[NDIRECT]);
    ip->addrs[NDIRECT] = 0;
  }

  ip->size = 0;
  iupdate(ip);
}

/**
* This method populates the stat structure <paramref name="st"/> with information from the inode structure <paramref name="ip"/>.
* It assigns the device number, inode number, type, number of links, and size from the inode structure to the stat structure.
* 
* @param ip The input inode structure from which information is retrieved.
* @param st The output stat structure that will be populated with information.
* @exception NULLPointerException if either ip or st is NULL.
* 
* Example:
* struct inode *input_ip;
* struct stat output_st;
* 
* // Populate input_ip with relevant information
* // Call stati(input_ip, &output_st);
*/
void
stati(struct inode *ip, struct stat *st)
{
  st->dev = ip->dev;
  st->ino = ip->inum;
  st->type = ip->type;
  st->nlink = ip->nlink;
  st->size = ip->size;
}

/**
* This method reads data from a specified inode <paramref name="ip"/> starting at the given offset <paramref name="off"/> with a maximum length of <paramref name="n"/>.
* If the inode's type is T_DEV, it checks if the device is valid and reads data from the device using the appropriate device read function.
* If the inode's type is not T_DEV, it reads data from the inode's buffer blocks based on the offset and length provided.
* 
* @param ip The inode from which to read data.
* @param dst The destination buffer where the read data will be stored.
* @param off The offset within the inode's data to start reading from.
* @param n The maximum number of bytes to read.
* @return The number of bytes successfully read, or -1 if an error occurs.
* 
* @exception -1 is returned if the inode's type is invalid or if the offset and length provided are out of bounds.
* 
* Example:
* struct inode *ip;
* char buffer[100];
* uint offset = 0;
* uint length = 50;
* int bytes_read = readi(ip, buffer, offset, length);
*/
int
readi(struct inode *ip, char *dst, uint off, uint n)
{
  uint tot, m;
  struct buf *bp;

  if(ip->type == T_DEV){
    if(ip->major < 0 || ip->major >= NDEV || !devsw[ip->major].read)
      return -1;
    return devsw[ip->major].read(ip, dst, n);
  }

  if(off > ip->size || off + n < off)
    return -1;
  if(off + n > ip->size)
    n = ip->size - off;

  for(tot=0; tot<n; tot+=m, off+=m, dst+=m){
    bp = bread(ip->dev, bmap(ip, off/BSIZE));
    m = min(n - tot, BSIZE - off%BSIZE);
    memmove(dst, bp->data + off%BSIZE, m);
    brelse(bp);
  }
  return n;
}

/**
* This method writes data from the source buffer <paramref name="src"/> to the specified inode <paramref name="ip"/> at the given offset <paramref name="off"/> for a specified length <paramref name="n"/>.
* If the inode type is a device, it checks if the device is valid and calls the corresponding write function from the device switch table.
* If the inode type is not a device, it writes data to the inode's buffer blocks based on the provided offset and length.
* If the write operation extends the inode's size, it updates the inode's size accordingly.
* 
* @param ip The pointer to the inode structure where data will be written.
* @param src The source buffer containing the data to be written.
* @param off The offset within the inode where writing will start.
* @param n The number of bytes to be written.
* @return Returns the number of bytes successfully written, or -1 if an error occurs.
* @exception Returns -1 if the inode type is invalid, offset is out of bounds, or if there is an error during the write operation.
* 
* Example:
* struct inode *ip;
* char *data = "Hello, World!";
* uint offset = 0;
* uint length = strlen(data);
* int bytes_written = writei(ip, data, offset, length);
*/
int
writei(struct inode *ip, char *src, uint off, uint n)
{
  uint tot, m;
  struct buf *bp;

  if(ip->type == T_DEV){
    if(ip->major < 0 || ip->major >= NDEV || !devsw[ip->major].write)
      return -1;
    return devsw[ip->major].write(ip, src, n);
  }

  if(off > ip->size || off + n < off)
    return -1;
  if(off + n > MAXFILE*BSIZE)
    return -1;

  for(tot=0; tot<n; tot+=m, off+=m, src+=m){
    bp = bread(ip->dev, bmap(ip, off/BSIZE));
    m = min(n - tot, BSIZE - off%BSIZE);
    memmove(bp->data + off%BSIZE, src, m);
    log_write(bp);
    brelse(bp);
  }

  if(n > 0 && off > ip->size){
    ip->size = off;
    iupdate(ip);
  }
  return n;
}


/**
* This method compares two strings <paramref name="s"/> and <paramref name="t"/> based on their names.
* It returns an integer value indicating the comparison result.
* 
* @param s The first string to compare.
* @param t The second string to compare.
* @return An integer value representing the comparison result.
* @exception None
* 
* Example:
* const char *str1 = "apple";
* const char *str2 = "banana";
* int result = namecmp(str1, str2);
* // result will be a value indicating the comparison result of "apple" and "banana"
*/
int
namecmp(const char *s, const char *t)
{
  return strncmp(s, t, DIRSIZ);
}

/**
* This method searches for a directory entry with the specified name within the given directory inode.
* If the entry is found, it returns the inode associated with the entry.
* If the entry is not found, it returns 0.
*
* @param dp The directory inode to search within.
* @param name The name of the directory entry to search for.
* @param poff Pointer to store the offset of the found directory entry.
* @return The inode associated with the directory entry if found, otherwise 0.
* @throws panic("dirlookup not DIR") if the provided inode is not a directory.
* @throws panic("dirlookup read") if an error occurs while reading the directory entries.
*
* Example:
* struct inode* result = dirlookup(directory_inode, "example_file", &offset);
* if(result != 0) {
*     // Directory entry found, handle the inode
* } else {
*     // Directory entry not found
* }
*/
struct inode*
dirlookup(struct inode *dp, char *name, uint *poff)
{
  uint off, inum;
  struct dirent de;

  if(dp->type != T_DIR)
    panic("dirlookup not DIR");

  for(off = 0; off < dp->size; off += sizeof(de)){
    if(readi(dp, (char*)&de, off, sizeof(de)) != sizeof(de))
      panic("dirlookup read");
    if(de.inum == 0)
      continue;
    if(namecmp(name, de.name) == 0){
      // entry matches path element
      if(poff)
        *poff = off;
      inum = de.inum;
      return iget(dp->dev, inum);
    }
  }

  return 0;
}

/**
* This method creates a directory entry linking the specified inode with the given name in the parent inode.
* If the name is already present, it returns -1.
* 
* @param dp The parent inode where the directory entry will be created.
* @param name The name of the directory entry to be created.
* @param inum The inode number to be linked with the directory entry.
* 
* @return Returns 0 on success, -1 if the name is already present in the parent inode.
* 
* @throws panic("dirlink read") if there is an error reading the directory entry.
* @throws panic("dirlink") if there is an error writing the directory entry.
* 
* Example:
* struct inode *parent_inode;
* char *entry_name = "new_entry";
* uint inode_number = 123;
* int result = dirlink(parent_inode, entry_name, inode_number);
* if(result == 0) {
*     printf("Directory entry created successfully.\n");
* } else {
*     printf("Failed to create directory entry.\n");
* }
*/
int
dirlink(struct inode *dp, char *name, uint inum)
{
  int off;
  struct dirent de;
  struct inode *ip;

  // Check that name is not present.
  if((ip = dirlookup(dp, name, 0)) != 0){
    iput(ip);
    return -1;
  }

  // Look for an empty dirent.
  for(off = 0; off < dp->size; off += sizeof(de)){
    if(readi(dp, (char*)&de, off, sizeof(de)) != sizeof(de))
      panic("dirlink read");
    if(de.inum == 0)
      break;
  }

  strncpy(de.name, name, DIRSIZ);
  de.inum = inum;
  if(writei(dp, (char*)&de, off, sizeof(de)) != sizeof(de))
    panic("dirlink");

  return 0;
}

/**            
* This method skips over elements in the input path string until it reaches the next element name.            
*            
* @param path The input path string to be processed.            
* @param name The output name of the next element in the path.            
* @return A pointer to the next element in the path after skipping over the current element.            
* @throws None            
*            
* Example:            
* char path[] = "/dir1/dir2/file.txt";            
* char name[DIRSIZ];            
* char *nextElement = skipelem(path, name);            
* // After execution, nextElement will point to "dir2/file.txt" and name will contain "dir1".            
*/
static char*
skipelem(char *path, char *name)
{
  char *s;
  int len;

  while(*path == '/')
    path++;
  if(*path == 0)
    return 0;
  s = path;
  while(*path != '/' && *path != 0)
    path++;
  len = path - s;
  if(len >= DIRSIZ)
    memmove(name, s, DIRSIZ);
  else {
    memmove(name, s, len);
    name[len] = 0;
  }
  while(*path == '/')
    path++;
  return path;
}

/**            
* This method searches for a specified file or directory in the file system based on the provided path and name.            
* If the path starts with '/', it searches from the root directory; otherwise, it searches from the current working directory of the process.            
* It iterates through the path components, locking each directory inode, checking if it is a directory, and then looking up the next component.            
* If the 'nameiparent' flag is set and the path ends early, it stops one level before reaching the final component.            
* If the specified file or directory is found, it returns the corresponding inode; otherwise, it returns 0.            
* If 'nameiparent' is set and the search completes successfully, it returns the parent inode of the final component.            
* If an error occurs during the search process, it returns 0.            
* 
* @param path The path to search for the file or directory.
* @param nameiparent Flag indicating whether to return the parent inode if found.
* @param name The name of the file or directory to search for.
* @return The inode of the specified file or directory if found; otherwise, 0.
* @exception If an error occurs during the search process, it returns 0.
* 
* Example:
* struct inode *result = namex("/dir1/dir2/file.txt", 0, "file.txt");
* // This will search for "file.txt" in the directory "/dir1/dir2" and return its inode if found.
*/
static struct inode*
namex(char *path, int nameiparent, char *name)
{
  struct inode *ip, *next;

  if(*path == '/')
    ip = iget(ROOTDEV, ROOTINO);
  else
    ip = idup(myproc()->cwd);

  while((path = skipelem(path, name)) != 0){
    ilock(ip);
    if(ip->type != T_DIR){
      iunlockput(ip);
      return 0;
    }
    if(nameiparent && *path == '\0'){
      // Stop one level early.
      iunlock(ip);
      return ip;
    }
    if((next = dirlookup(ip, name, 0)) == 0){
      iunlockput(ip);
      return 0;
    }
    iunlockput(ip);
    ip = next;
  }
  if(nameiparent){
    iput(ip);
    return 0;
  }
  return ip;
}

/**
* This method takes a path as input and returns a pointer to an inode structure.
* It extracts the name from the path and calls the namex function to retrieve the inode pointer.
* 
* @param path The input path for which the inode pointer needs to be retrieved.
* @return A pointer to the inode structure corresponding to the given path.
* @throws None
* 
* Example:
* 
* struct inode* result = namei("/home/user/file.txt");
*/
struct inode*
namei(char *path)
{
  char name[DIRSIZ];
  return namex(path, 0, name);
}

/**
* This method returns the parent inode of a given path and name.
* It calls the namex function with the provided path and name parameters.
* 
* @param path The path of the file/directory.
* @param name The name of the file/directory.
* @return A pointer to the parent inode of the specified path and name.
* @throws NULLPointerException if the path or name is NULL.
* 
* Example:
* struct inode* parent = nameiparent("/home/user/documents", "file.txt");
*/
struct inode*
nameiparent(char *path, char *name)
{
  return namex(path, 1, name);
}
