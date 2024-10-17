/* Author: Joey Hong
   Date:4/23/24
    */
    
    
    
/***
 *      ______ .___  ___. .______     _______.  ______              ____    __   __  
 *     /      ||   \/   | |   _  \   /       | /      |            |___ \  /_ | /_ | 
 *    |  ,----'|  \  /  | |  |_)  | |   (----`|  ,----'              __) |  | |  | | 
 *    |  |     |  |\/|  | |   ___/   \   \    |  |                  |__ <   | |  | | 
 *    |  `----.|  |  |  | |  |   .----)   |   |  `----.             ___) |  | |  | | 
 *     \______||__|  |__| | _|   |_______/     \______|            |____/   |_|  |_| 
 *                                                                                   
 */


#include <stdio.h>
#include <string.h>
#include <assert.h>

#include "cache.h"
#include "mdadm.h"
#include "util.h"
#include "jbod.h"
#include "net.h"

uint32_t pack_bytes(uint32_t a, uint32_t b, uint32_t c, uint32_t d){
  uint32_t retval = 0x0, tempa, tempb, tempc, tempd;

  tempa = a;
  tempb = b << 14;
  tempc = c << 20;
  tempd = d << 28;
  retval = tempa|tempb|tempc|tempd;

  return retval;
}
int mounted = 0;

int mdadm_mount(void) {
  /* YOUR CODE */
  //sets the JBOD_MOUNT num and converts it to a 32 bit num
  uint8_t *block = NULL;
  int test = jbod_client_operation(pack_bytes(0,JBOD_MOUNT,0,0),block);
  if (test == 0){
    mounted = 1;
    return 1;
  }
  
  return -1;
}

int mdadm_unmount(void) {
  /* YOUR CODE */
  uint8_t *block = NULL;
  int test = jbod_client_operation(pack_bytes(0,JBOD_UNMOUNT,0,0),block);
  if (test == 0){
    mounted = 0;
    return 1;
  }
  return -1;
}


int mdadm_read(uint32_t addr, uint32_t len, uint8_t *buf) {
  /* YOUR CODE */
   //right now reads the memory addr as one address in a linear space containing all the disk memory, have to split it up by disks, blocks, and block size
  //check if mounted or unmounted
  if (mounted == 0){
    return -1;
  }
  //check if it is not an address in the correct range with a length that is not zero
  if (buf == NULL  && len > 0){
    return -1;
  }
  //check if addr is greater than disk space or len greater than 1024 bits
  if (addr+len > JBOD_NUM_DISKS*JBOD_DISK_SIZE || len > 1024){
    return -1;
  }
  if (len == 0 && buf == NULL){
    return 0;
  }

  
  uint8_t *block = NULL;
  int disk_num = addr/JBOD_DISK_SIZE;
  //Finds the block and how many blocks into the disk it is
  uint32_t block_num = ((addr%JBOD_DISK_SIZE) / JBOD_BLOCK_SIZE);
  //temp buffer
  uint8_t temp[256];
  int remaining_len = len;
  int bytes_read = 0;
  jbod_client_operation(pack_bytes(0,JBOD_SEEK_TO_DISK,0,15), block);
  jbod_client_operation(pack_bytes(0,JBOD_SEEK_TO_BLOCK,block_num,0), block);
  while (remaining_len > 0){
    //Address plus the bytes read remainder 256
    uint32_t offset = (addr+bytes_read)%256;
    
    int cur_off = 256-offset;
    
    //gets min of remaining len or cur_off
    uint32_t len1 = remaining_len>cur_off?cur_off:remaining_len;
   
    //seeks to disk and block
    jbod_client_operation(pack_bytes(0,JBOD_SEEK_TO_DISK,0,disk_num), block);
    jbod_client_operation(pack_bytes(0,JBOD_SEEK_TO_BLOCK,block_num,0), block);
    
    
    if (cache_enabled()){
      if(cache_lookup(disk_num, block_num, temp) == 1){
	memcpy(buf+bytes_read,temp+offset,len1);
      }
      else {
	jbod_client_operation(pack_bytes(0,JBOD_READ_BLOCK,0,0),temp);
	cache_insert(disk_num, block_num, temp);
	memcpy(buf+bytes_read,temp+offset,len1);
      }
    }
    else{
      jbod_client_operation(pack_bytes(0,JBOD_READ_BLOCK,0,0),temp);
      //Copies from after read_bytes, 0 unless across disks/blocks
      memcpy(buf+bytes_read,temp+offset,len1);
    }
      //after copying adds to how many bytes read
      bytes_read += len1;
      
      remaining_len -= len1;
    
    //increments disk num and changes block num if reaches end of disk
    if (block_num == 255){
      disk_num += 1;
      block_num = 0;
    }
    else{
      block_num += 1;
    }
  }
  return len;
}

int mdadm_write(uint32_t addr, uint32_t len, const uint8_t *buf) {
  /* YOUR CODE */
  //right now reads the memory addr as one address in a linear space containing all the disk memory, have to split it up by disks, blocks, and block size
  //check if mounted or unmounted
  if (mounted == 0){
    return -1;
  }
  //check if it is not an address in the correct range with a length that is not zero
  if (buf == NULL  && len > 0){
    return -1;
  }
  //check if addr is greater than disk space or len greater than 1024 bits
  if (addr+len > JBOD_NUM_DISKS*JBOD_DISK_SIZE || len > 1024){
    return -1;
  }
  if (len == 0 && buf == NULL){
    return 0;
  }
  uint8_t *block = NULL;
  int disk_num = addr/JBOD_DISK_SIZE;
  //Finds the block and how many blocks into the disk it is
  uint32_t block_num = ((addr%JBOD_DISK_SIZE) / JBOD_BLOCK_SIZE);
  uint8_t temp[256];
  int remaining_len = len;
  int bytes_written = 0;
  //seeks to disk and block
  jbod_client_operation(pack_bytes(0,JBOD_SEEK_TO_DISK,0,disk_num), block);
  jbod_client_operation(pack_bytes(0,JBOD_SEEK_TO_BLOCK,block_num,0), block);

  while (remaining_len > 0){

    //Address plus the bytes read remainder 256
    uint32_t offset = (addr+bytes_written)%256;
    int cur_off = 256-offset;
    //256 is block size, 
    //gets min of remaining len or cur_off
    uint32_t len1 = remaining_len>cur_off?cur_off:remaining_len;
    if (cache_enabled()){
      if (cache_lookup(disk_num,block_num,temp) == 1){
	memcpy(temp+offset, buf + bytes_written, len1);
      }
      else{
	jbod_client_operation(pack_bytes(0,JBOD_SEEK_TO_DISK,0,disk_num), block);
	jbod_client_operation(pack_bytes(0,JBOD_SEEK_TO_BLOCK,block_num,0), block);
	jbod_client_operation(pack_bytes(0,JBOD_READ_BLOCK,0,0),temp);
	memcpy(temp+offset, buf + bytes_written, len1);
	cache_insert(disk_num,block_num,temp);	
      }
    }
    else {
      //reads from the start of the block, reads the entire block
      jbod_client_operation(pack_bytes(0,JBOD_SEEK_TO_DISK,0,disk_num), block);
      jbod_client_operation(pack_bytes(0,JBOD_SEEK_TO_BLOCK,block_num,0), block);
      jbod_client_operation(pack_bytes(0,JBOD_READ_BLOCK,0,0),temp);
      
      //Copies into temp, adding the changing offset, copies buf, changing the index based on written bytes
      memcpy(temp+offset, buf+bytes_written, len1);
    }
    //seeks to disk and block
    jbod_client_operation(pack_bytes(0,JBOD_SEEK_TO_DISK,0,disk_num), block);
    jbod_client_operation(pack_bytes(0,JBOD_SEEK_TO_BLOCK,block_num,0), block);


    
    jbod_client_operation(pack_bytes(0,JBOD_WRITE_BLOCK,0,0),temp);
    cache_update(disk_num, block_num, temp);
      
    

      
    //after copying adds to how many bytes read
    bytes_written += len1;
    remaining_len -= len1;
    
    //increments disk num and changes block num if reaches end of disk
    if (block_num == 255){
      disk_num += 1;
      block_num = 0;
    }
    else{
      block_num += 1;
    }
  }
  return len;
}
