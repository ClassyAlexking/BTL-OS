/*
 * Copyright (C) 2024 pdnguyen of the HCMC University of Technology
 */
/*
 * Source Code License Grant: Authors hereby grants to Licensee 
 * a personal to use and modify the Licensed Source Code for 
 * the sole purpose of studying during attending the course CO2018.
 */
//#ifdef MM_TLB
/*
 * Memory physical based TLB Cache
 * TLB cache module tlb/tlbcache.c
 *
 * TLB cache is physically memory phy
 * supports random access 
 * and runs at high speed
 */


#include "mm.h"
#include <stdlib.h>
#include <stdio.h>
#include <pthread.h> //ADDED

#define init_tlbcache(mp,sz,...) init_memphy(mp, sz, (1, ##__VA_ARGS__))

#define TLB_PTE_SIZE        4
#define VALID_BIT_MASK      0x80        // Mask to extract the valid bit (first bit) of byte
#define HI_7_BIT_MASK       0x7F        // Mask to extract 7-bits low of byte
#define HI_2_BIT_MASK       0xC0        // Mask to extract 2-bits hight of byte
#define LO_6_BIT_MASK       0x3F        // Mask to extract 6-bits low of byte
#define BYTE_MASK           0xFF        // Mask to extract a byte (usigned)

#define FRAMENUM_TLB_BITS   14         // A entry has 14 bits for frame number

static pthread_mutex_t tlb_lock = PTHREAD_MUTEX_INITIALIZER;

// Function to extract the valid bit (1 or 0)
int get_valid_bit(BYTE *storage, int index) {
   BYTE firstByte = *(BYTE *)(storage + index * TLB_PTE_SIZE + 0);
   return (firstByte & VALID_BIT_MASK) ? 1 : 0;
}
// Function to toggle the valid bit (1 or 0)
void toggle_valid_bit(BYTE *storage, int index) {
   BYTE firstByte = *(BYTE *)(storage + index * TLB_PTE_SIZE + 0);

   firstByte ^= VALID_BIT_MASK;

   *(BYTE *)(storage + index * TLB_PTE_SIZE + 0) = firstByte;
}


// // Function to extract the PID (17 bits)
int get_pid(BYTE *storage, int index) {
   BYTE firstByte = *(BYTE *)(storage + index * TLB_PTE_SIZE + 0);
   BYTE secondByte = *(BYTE *)(storage + index * TLB_PTE_SIZE + 1);
   BYTE thirdByte = *(BYTE *)(storage + index * TLB_PTE_SIZE + 2);
   
   unsigned int result = 0;
   result = (thirdByte & HI_2_BIT_MASK) >> 6;
   result += ((secondByte & BYTE_MASK) << 2);
   result += ((firstByte & HI_7_BIT_MASK) >> 1) << 10;

   return result;
}

// Function to extract the frame number (14 bits)
int get_frame_num(BYTE *storage, int index) {
   BYTE thirdByte = *(BYTE *)(storage + index * TLB_PTE_SIZE + 2);
   BYTE forthByte = *(BYTE *)(storage + index * TLB_PTE_SIZE + 3);
   
   unsigned int result = 0;
   result += (forthByte & BYTE_MASK);
   result += ((thirdByte & LO_6_BIT_MASK)) << 8;

   return result;
}

// Assign address of page in TLB to storage
void assignToBytes(BYTE *storage, int index, unsigned long int addr) {
   for (int i = 0; i < 4; ++i) {
      storage[index*TLB_PTE_SIZE + i] = (addr >> ((3-i) * 8)) & BYTE_MASK;
   }
}
// -------------------------------------------------------------------------

/*
 *  tlb_cache_read read TLB cache device
 *  @mp: memphy struct
 *  @pid: process id
 *  @pgnum: page number
 *  @value: obtained value
 */
int tlb_cache_read(struct memphy_struct * mp, int pid, int pgnum, int *value) // read the frame num
{
   /* TODO: the identify info is mapped to cache line by employing:
      direct mapped
    */
   
   if (mp == NULL) {
      init_tlbmemphy(mp, 0x10000);  // FIXED MAXSIZE = 0x10000 (2^16)
      return -1;
   }

   int index = pgnum % (mp->maxsz / TLB_PTE_SIZE);
   printf("tlb_cache_read -- Page number: %d and Index: %d\n", pgnum, index);
   
   // printf("@@@@@@@@@@@@@@@@@@@@@@@@@@@@@\n");
   // MEMPHY_dump(mp);
   // printf("@@@@@@@@@@@@@@@@@@@@@@@@@@@@@\n");
   if(get_valid_bit(mp->storage, index)) {
      if(get_pid(mp->storage, index) == pid) {
         // HIT
         *value = get_frame_num(mp->storage, index);
         printf("Frame number: %d\n", *value);
         return 0;
      }
      // else {
      //    // MISS
      //    *value = -1;
      // }
   }
   *value = -1;
   printf("Frame number: %d\n", *value);
   return 0;
}

/*
 *  tlb_cache_write write TLB cache device
 *  @mp: memphy struct
 *  @pid: process id
 *  @pgnum: page number
 *  @value: obtained value
 */
int tlb_cache_write(struct memphy_struct *mp, int pid, int pgnum, int value) // write the framenum
{
   /* TODO: the identify info is mapped to cache line by employing:
      direct mapped
    */
   pthread_mutex_lock(&tlb_lock);
   if (mp == NULL) {
      init_tlbmemphy(mp, 0x10000);  // FIXED MAXSIZE = 0x10000 (2^16)
      return -1;
   }

   int index = pgnum % (mp->maxsz / TLB_PTE_SIZE);

   printf("tlb_cache_write -- Page number: %d and Index: %d\n", pgnum, index);
   printf("Old Frame number: %d\n", value);

   if(get_valid_bit(mp->storage, index)) {
      if(get_pid(mp->storage, index) == pid) {
         // HIT
         if(get_frame_num(mp->storage, index) == value) {
            // Nothing to do here
            pthread_mutex_unlock(&tlb_lock);
            return 0; 
         }
      }

   }   
   unsigned long int newAddress = value + (pid << FRAMENUM_TLB_BITS);
   assignToBytes(mp->storage, index, newAddress);
   if(get_valid_bit(mp->storage, index) == 0) toggle_valid_bit(mp->storage, index);

   printf("New Frame number: %lu\n", newAddress);
   pthread_mutex_unlock(&tlb_lock);
   return 0;
}

/*
 *  TLBMEMPHY_read natively supports MEMPHY device interfaces
 *  @mp: memphy struct
 *  @addr: address
 *  @value: obtained value
 */
int TLBMEMPHY_read(struct memphy_struct * mp, int addr, BYTE *value)
{
   if (mp == NULL)
     return -1;

   /* TLB cached is random access by native */
   *value = mp->storage[addr];

   return 0;
}

/*
 *  TLBMEMPHY_write natively supports MEMPHY device interfaces
 *  @mp: memphy struct
 *  @addr: address
 *  @data: written data
 */
int TLBMEMPHY_write(struct memphy_struct * mp, int addr, BYTE data) //redundant
{
   if (mp == NULL)
     return -1;

   /* TLB cached is random access by native */
   mp->storage[addr] = data;

   return 0;
}

/*
 *  TLBMEMPHY_format natively supports MEMPHY device interfaces
 *  @mp: memphy struct
 */
int TLBMEMPHY_dump(struct memphy_struct * mp)
{
   /*TODO dump memphy contnt mp->storage 
    *     for tracing the memory content
    */
   printf("===== TLB MEMORY DUMP =====\n");
   if (mp == NULL) {
      printf("Error: Null pointer encountered.\n");
      return -1;
   }
   // Loop through the storage and print each value
   for (int i = 0; i < mp->maxsz/TLB_PTE_SIZE; ++i) {
      printf("Address %d: %d\n", i, *(uint32_t *)(mp->storage + i * TLB_PTE_SIZE));

   }
   printf("===== TLB MEMORY DUMP =====\n");
   return 0;
}

/*
 *  Init TLBMEMPHY struct
 */
int init_tlbmemphy(struct memphy_struct *mp, int max_size)
{  
   // Fixed max size = 2^16 / 4 (0x10000)

   mp->storage = (BYTE *)malloc(max_size*sizeof(BYTE));
   mp->maxsz = max_size;

   mp->rdmflg = 1;

   return 0;
}

//#endif
