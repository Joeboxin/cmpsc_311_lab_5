#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include "cache.h"

static cache_entry_t *cache = NULL;
static int cache_size = 0;
static int clock = 0;
static int num_queries = 0;
static int num_hits = 0;

int cache_made = 0;

//creates a cache, checks if within enry size and if a cache was already made
int cache_create(int num_entries) {
  if (num_entries < 2 || num_entries > 4096){
    return -1;
  }
  if (cache_made == 1){
      return -1;
    }
  //sets the cache variables
  cache = calloc(num_entries, sizeof(cache_entry_t));
  cache_size = num_entries;
  cache_made = 1;
  //sets all the empty cache variables to false and untargetable disk and block nums
  for (int i = 0; i<cache_size;i++){
    cache[i].valid = false;
    cache[i].disk_num = -1;
    cache[i].block_num = -1;
  }
  return 1;
  
}

//Destroys the cache
int cache_destroy(void) {
  //Checks if the cache is made or not and if it is a valid cache
  if (cache_enabled() ==  false || cache_made == 0){
    return -1;
  }
  //clears the cache and resets all the variables
  free(cache);
  cache = NULL;
  cache_made = 0;
  clock = 0;
  return 1;
}

int cache_lookup(int disk_num, int block_num, uint8_t *buf) {
  //checks if it is an enabled cache, and that there is a made cache
  if (cache_enabled() == false || cache_made == 0){
    return -1;
  }
  //Checks if the buff is empty, the cache size is zero, the cache is null, or if the first cache index is valid
  if (buf == NULL|| cache_size == 0 || cache == NULL || !cache[0].valid){
    return -1;
  }
  //Checks if the disk num and block nums are within the correct parameters
  if (disk_num < 0|| disk_num > 15 || block_num < 0 || block_num > 255){
    return -1;
  }
  //counts how many caches are empty/free spaces
  int num_false = 0;
  for (int i = 0;i<cache_size;i++){
    if (cache[i].valid == false){
      num_false += 1;
    }
  }
  //if the false counter == cache_size cannot look up anything cause empty
  if (num_false == cache_size){
    return -1;
  }
  
  num_queries += 1;
  //loops through the cache
  for (int i = 0;i<cache_size;i++){
    if (cache[i].block_num == block_num && cache[i].disk_num == disk_num && cache[i].valid == true){
      //copies block into the buff if matching block num and disk num and a valid cache
      memcpy(buf,cache[i].block,JBOD_BLOCK_SIZE);
      //increments the num hits, clock then sets sets the access_time
      num_hits += 1;
      clock += 1;
      cache[i].access_time = clock;
      return 1;
    }
  }
  return -1;
}

void cache_update(int disk_num, int block_num, const uint8_t *buf) {
  //goes through the cache
    if (cache_made != 0 && buf != NULL && cache != NULL && cache_enabled() == true){
    
  
  if (block_num >= 0 && block_num  <= 255 && disk_num >= 0 && disk_num <= 15 && cache_size != 0){
   
  
  for (int i = 0; i < cache_size; i++){
    //checks if the cache has the same block and disk num given
    //changes the data in the block with data from the buf and increments clock and access time
    if (cache[i].block_num == block_num && cache[i].disk_num == disk_num){
      memcpy(cache[i].block,buf,JBOD_BLOCK_SIZE);
      clock += 1;
      cache[i].access_time = clock;
    }
  }
  }
    }
}


//Three cases - Full cache, disk and block addr same, normal(free space in cache)
int cache_insert(int disk_num, int block_num, const uint8_t *buf) {
  //checks all constraints
  if (cache_made == 0 || buf == NULL || cache == NULL || cache_enabled() == false){
    return -1;
  }
  if (block_num < 0 || block_num  > 255|| disk_num < 0 || disk_num > 15 || cache_size == 0){
    return -1;
  }
  //goes through cache
  for (int i = 0; i<cache_size;i++){
    //checks if the block and disk nums are the same as the cache blocks and disks
    if (cache[i].block_num == block_num && cache[i].disk_num == disk_num){
      return -1;
    }
    //Checks if empty cache sets all variables and copies buf into cache block
    if (cache[i].valid == false){
      cache[i].disk_num = disk_num;
      cache[i].block_num = block_num;
      cache[i].valid = true;
      memcpy(cache[i].block,buf, JBOD_BLOCK_SIZE);
      clock += 1;
      cache[i].access_time = clock;
      return 1;
    }
  }
  //finds lowest access time
 int low_access_time = cache[0].access_time;
 int low_access_time_index = 0;
  for (int k =0; k < cache_size; k++){
    if (cache[k].access_time < low_access_time){
      low_access_time_index = k;
    }
  }
  //changes all disk and block nums and compies the buf into the lowest cache following LRU policy
  cache[low_access_time_index].disk_num = disk_num;
  cache[low_access_time_index].block_num = block_num;
  memcpy(cache[low_access_time_index].block,buf, JBOD_BLOCK_SIZE);
  clock += 1;
  cache[low_access_time_index].access_time = clock;
  return 1;
}




bool cache_enabled(void) {
  if (cache_size >= 2 && cache_size >= 4096){
    return 1;
  }
  
  return -1;
}

void cache_print_hit_rate(void) {
  fprintf(stderr, "Hit rate: %5.1f%%\n", 100 * (float) num_hits / num_queries);
}
