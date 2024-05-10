/*
 * Copyright (C) 2024 pdnguyen of the HCMC University of Technology
 */
/*
 * Source Code License Grant: Authors hereby grants to Licensee 
 * a personal to use and modify the Licensed Source Code for 
 * the sole purpose of studying during attending the course CO2018.
 */
//#ifdef CPU_TLB
/*
 * CPU TLB
 * TLB module cpu/cpu-tlb.c
 */
 
#include "mm.h"
#include <stdlib.h>
#include <stdio.h>
#include <pthread.h> //ADDED

static pthread_mutex_t tlb_lock = PTHREAD_MUTEX_INITIALIZER;


int tlb_change_all_page_tables_of(struct pcb_t *proc,  struct memphy_struct * mp)
{
  /* TODO update all page table directory info 
   *      in flush or wipe TLB (if needed)
   */

  /* No action needed for direct mapped TLB on process context change
     since page table updates only affect the page directory entries. */

  return 0;
}

// int get_pgnum(struct pcb_t *proc, uint32_t source, uint32_t offset)
// {
//   struct vm_rg_struct *currg = get_symrg_byid(proc->mm, source);
//   int addr = currg->rg_start + offset;
//   return PAGING_PGN(addr);
// }

int tlb_flush_tlb_of(struct pcb_t *proc, struct memphy_struct * mp)
{
  /* TODO flush tlb cached*/
  // Flush the TLB cache for the given page table

  return 0;
}

/*tlballoc - CPU TLB-based allocate a region memory
 *@proc:  Process executing the instruction
 *@size: allocated size 
 *@reg_index: memory region ID (used to identify variable in symbole table)
 */
int tlballoc(struct pcb_t *proc, uint32_t size, uint32_t reg_index)
{
  pthread_mutex_lock(&tlb_lock);
  int addr, val;

  /* By default using vmaid = 0 */
  val = __alloc(proc, 0, reg_index, size, &addr);

  /* TODO update TLB CACHED frame num of the new allocated page(s)*/
  /* by using tlb_cache_read()/tlb_cache_write()*/

  /* Update TLB CACHED frame num of the new allocated page(s) */
  int page_num = PAGING_PAGE_ALIGNSZ(size)/PAGING_PAGESZ;
  //get first allocated page in page table
  int pgn = PAGING_PGN(addr);
  int fpn;
  for(int i = 0; i<page_num; i++){
    fpn = PAGING_FPN(proc->mm->pgd[pgn+i]);
    tlb_cache_write(proc->tlb, proc->pid, pgn+i, fpn);
  }
  pthread_mutex_unlock(&tlb_lock);
  return val;
}

/*pgfree - CPU TLB-based free a region memory
 *@proc: Process executing the instruction
 *@size: allocated size 
 *@reg_index: memory region ID (used to identify variable in symbole table)
 */
int tlbfree_data(struct pcb_t *proc, uint32_t reg_index)
{
  pthread_mutex_lock(&tlb_lock);
  __free(proc, 0, reg_index);

  /* TODO update TLB CACHED frame num of freed page(s)*/
  /* by using tlb_cache_read()/tlb_cache_write()*/

  /* Update TLB CACHED frame num of freed page(s) */
  struct vm_rg_struct *currg = get_symrg_byid(proc->mm, reg_index);
  int page_num = PAGING_PAGE_ALIGNSZ(currg->rg_end - currg->rg_start)/PAGING_PAGESZ;
  int pgn =  PAGING_PGN(currg->rg_start);
  int fpn;
  for(int i = 0; i<page_num; i++){
    fpn = PAGING_FPN(proc->mm->pgd[pgn+i]);
    tlb_cache_write(proc->tlb, proc->pid, pgn+i, fpn);
  }
  pthread_mutex_unlock(&tlb_lock);
  return 0;
}


/*tlbread - CPU TLB-based read a region memory
 *@proc: Process executing the instruction
 *@source: index of source register
 *@offset: source address = [source] + [offset]
 *@destination: destination storage
 */
int tlbread(struct pcb_t * proc, uint32_t source,
            uint32_t offset, 	uint32_t destination) 
{
  BYTE data = -1;
  int val;
  int frmnum = -1;
	
  /* TODO retrieve TLB CACHED frame num of accessing page(s)*/
  /* by using tlb_cache_read()/tlb_cache_write()*/
  /* frmnum is return value of tlb_cache_read/write value*/

  // struct vm_rg_struct rgnode = proc->mm->symrgtbl[destination];
  pthread_mutex_lock(&tlb_lock);
  printf("TLB read - PID=%d, Source: %u, Destination: %u, Offset: %u\n", proc->pid, source, destination, offset);
  struct vm_rg_struct *currg = get_symrg_byid(proc->mm, source);
  struct vm_area_struct *cur_vma = get_vma_by_num(proc->mm, 0);

  if(currg == NULL || cur_vma == NULL){
    printf("Invalid MEMORY.\n");
    pthread_mutex_unlock(&tlb_lock);
    return -1;
  }
  addr_t addr = currg->rg_start + offset;
  //printf("Address: %u\n", addr);
  if(addr >= currg->rg_end) {
    printf("INVALID Address%u\n", currg->rg_start);
    pthread_mutex_unlock(&tlb_lock);
    return -1;
  }

  int pgn = PAGING_PGN(addr);
  int off = PAGING_OFFST(addr);
  
  val = tlb_cache_read(proc->tlb, proc->pid, pgn, &frmnum);
  //printf("Frame num: %d\n", frmnum);
  if(val < 0){
      pthread_mutex_unlock(&tlb_lock);
      return -1;
    }

#ifdef IODUMP
  if (frmnum >= 0) {
    printf("----------------->TLB hit at read region=%d offset=%d<-----------------\n", 
	         source, offset);
    int phyaddr = (frmnum << PAGING_ADDR_FPN_LOBIT) + off;
    val = MEMPHY_read(proc->mram, phyaddr, &data);
    printf("read value=%d\n", data);
    if(val < 0){
      pthread_mutex_unlock(&tlb_lock);
      return -1;
    }
  }
  else {
    printf("----------------->TLB miss at read region=%d offset=%d<-----------------\n", 
	         source, offset);
    val = tlb_cache_write(proc->tlb, proc->pid, pgn, &frmnum);
  }

#ifdef PAGETBL_DUMP
  print_pgtbl(proc, 0, -1); //print max TBL
#endif
  // MEMPHY_dump(proc->mram);
#endif

  val = __read(proc, 0, source, offset, &data);
  if(val < 0){
      pthread_mutex_unlock(&tlb_lock);
      return -1;
    }

  destination = (uint32_t) data;

  /* TODO update TLB CACHED with frame num of recent accessing page(s)*/
  /* by using tlb_cache_read()/tlb_cache_write()*/
  // frmnum = PAGING_FPN(destination);
  // printf("===== New Frame number: %d ====\n", frmnum);

  // tlb_cache_write(proc->tlb, proc->pid, pgnum, frmnum);
  pthread_mutex_unlock(&tlb_lock);
  return 0;
}

/*tlbwrite - CPU TLB-based write a region memory
 *@proc: Process executing the instruction
 *@data: data to be wrttien into memory
 *@destination: index of destination register
 *@offset: destination address = [destination] + [offset]
 */
int tlbwrite(struct pcb_t * proc, BYTE data,
             uint32_t destination, uint32_t offset)
{
  int val;
  int frmnum = -1;
  /* TODO retrieve TLB CACHED frame num of accessing page(s))*/
  /* by using tlb_cache_read()/tlb_cache_write()
  frmnum is return value of tlb_cache_read/write value*/
  pthread_mutex_lock(&tlb_lock);
  printf("TLB write  - PID=%d, Destination: %u, Offset: %u\n", proc->pid, destination, offset);
  struct vm_rg_struct *currg = get_symrg_byid(proc->mm, destination);
  struct vm_area_struct *cur_vma = get_vma_by_num(proc->mm, 0);

  if(currg == NULL || cur_vma == NULL){
    printf("Invalid MEMORY.\n");
    pthread_mutex_unlock(&tlb_lock);
    return -1;
  }
  addr_t addr = currg->rg_start + offset;
  printf("Address: %u\n", addr);
  if(addr >= currg->rg_end) {
    printf("INVALID Address\n");
    pthread_mutex_unlock(&tlb_lock);
    return -1;
  }
  int pgn = PAGING_PGN(addr);
  int off = PAGING_OFFST(addr);
  
  val = tlb_cache_read(proc->tlb, proc->pid, pgn, &frmnum);

  printf("Frame num: %d\n", frmnum);
  if(val < 0){
      pthread_mutex_unlock(&tlb_lock);
      return -1;
    }

#ifdef IODUMP
  if (frmnum >= 0){
    printf("----------------->TLB hit at write region=%d offset=%d value=%d<-----------------\n",
	          destination, offset, data);
    int phyaddr = (frmnum << PAGING_ADDR_FPN_LOBIT) + off; //ADDED
    printf("phyaddr: %d and off: %d\n", phyaddr, off); //ADDED
    MEMPHY_write(proc->mram, phyaddr, data);//ADDED
    if(val < 0){
      pthread_mutex_unlock(&tlb_lock);
      return -1;
    }//ADDED
  }else{
    printf("----------------->TLB miss at write region=%d offset=%d value=%d<-----------------\n",
            destination, offset, data);
    val = tlb_cache_write(proc->tlb, proc->pid, pgn, &frmnum);
    val = __write(proc, 0, destination, offset, data);//ADDED
  }
#ifdef PAGETBL_DUMP
  print_pgtbl(proc, 0, -1); //print max TBL
#endif
  // MEMPHY_dump(proc->mram);
#endif

  //val = __write(proc, 0, destination, offset, data);
  pthread_mutex_unlock(&tlb_lock);
  return 0;
}

//#endif
