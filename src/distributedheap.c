/*
 * distributedheap.c
 *
 *  Created on: 30 Mar 2016
 *      Author: nick
 */

#include "distmem.h"
#include "distmem_mpi.h"
#include "directory.h"
#include "gvirtual.h"
#include <memkind/internal/memkind_default.h>
#include <memkind/internal/memkind_arena.h>
#include <memkind/internal/memkind_pmem.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#define MASTER_RANK 0

// MPI datatype used to communicate the distributed blocks of memory
MPI_Datatype GVM_DISTRIBUTED_BLOCKS;

memkind_t DISTRIBUTEDHEAP_CONTIGUOUS_KIND;

// The distributed block structure that is communicated from the master to other processes
struct global_distributed_block {
  unsigned long startAddress, endAddress;
  int owner_pid;
};

// Allocated memory structure that holds a specific memory allocation within the global virtual address space
struct gvm_allocated_memory {
  unsigned long startAddress, endAddress;
  struct gvm_allocated_memory* next, *prev;
};
// Root memory allocation in GVM, this is an ordered list (ordered ascending by address) held on the master process only
struct gvm_allocated_memory* distributedMemoryAllocations = NULL;
struct global_address_space_descriptor address_space_descriptor;

static void* distributedheap_contiguous_malloc(struct distmem*, size_t, size_t, struct distmem_block*, int, int, ...);
static void distributedheap_free(struct memkind*, void*);
static unsigned long findAndAllocateHoleInGVMForAllocation(size_t);
static unsigned long findHoleInGVMForAllocation(size_t);

/**
 * Will initialise the distributed heap, starting at a specific memory address
 */
void gvi_distributedHeap_initialise(struct global_address_space_descriptor address_space_desc) {
  address_space_descriptor = address_space_desc;
  struct memkind_ops* my_memkind_ops = (struct memkind_ops*)memkind_malloc(MEMKIND_DEFAULT, sizeof(struct memkind_ops));
  memcpy(my_memkind_ops, &MEMKIND_PMEM_OPS, sizeof(struct memkind_ops));
  my_memkind_ops->free = distributedheap_free;

  struct distmem_ops* distributed_heap_vtable = (struct distmem_ops*)memkind_malloc(MEMKIND_DEFAULT, sizeof(struct distmem_ops));
  distributed_heap_vtable->dist_malloc = distributedheap_contiguous_malloc;
  distributed_heap_vtable->dist_create = distmem_arena_create;
  distributed_heap_vtable->dist_determine_distribution = distmem_mpi_contiguous_distributer;
  distributed_heap_vtable->memkind_operations = my_memkind_ops;

  int err = distmem_create(distributed_heap_vtable, "distributedcontiguous", &DISTRIBUTEDHEAP_CONTIGUOUS_KIND);

  MPI_Datatype oldtypes[] = {MPI_UNSIGNED_LONG, MPI_UNSIGNED_LONG, MPI_INT};
  int blockCounts[3] = {1, 1, 1};
  MPI_Aint offsets[3];
  offsets[0] = offsetof(struct global_distributed_block, startAddress);
  offsets[1] = offsetof(struct global_distributed_block, endAddress);
  offsets[2] = offsetof(struct global_distributed_block, owner_pid);
  MPI_Type_struct(3, blockCounts, offsets, oldtypes, &GVM_DISTRIBUTED_BLOCKS);
  MPI_Type_commit(&GVM_DISTRIBUTED_BLOCKS);

  if (err) {
    fprintf(stderr, "Error allocating distributed kind\n");
  }
}

/**
 * Frees an entry in the distributed heap, this removes all corresponding entries from the directory
 */
static void distributedheap_free(struct memkind* kind, void* ptr) { gvi_directory_removeAllMemoriesByUUID((unsigned long)ptr); }

/**
 * The malloc for the contiguous distributed heap allocation. The master process will determine the addresses and send these out to all
 * other processes, who will register them in their directories and handle their own specific local allocation (pin the memory, set up
 * the RMA window & return the handle to this.)
 */
static void* distributedheap_contiguous_malloc(struct distmem* dist_kind, size_t element_size, size_t number_elements,
                                               struct distmem_block* allocation_blocks, int number_blocks, int nargs, ...) {
  va_list ap;
  va_start(ap, nargs);
  int my_rank;
  MPI_Comm communicator = va_arg(ap, MPI_Comm);
  MPI_Comm_rank(communicator, &my_rank);

  int i;
  struct global_distributed_block* address_blocks =
      (struct global_distributed_block*)memkind_malloc(MEMKIND_DEFAULT, sizeof(struct global_distributed_block) * number_blocks);
  void* my_start_address = NULL;
  size_t local_mem_size = 0;
  if (my_rank == MASTER_RANK) {
    unsigned long allocation_start_address = findAndAllocateHoleInGVMForAllocation(element_size * number_elements);
    for (i = 0; i < number_blocks; i++) {
      address_blocks[i].startAddress = allocation_start_address + (allocation_blocks[i].startElement * element_size);
      address_blocks[i].endAddress = allocation_start_address + (((allocation_blocks[i].endElement + 1) * element_size) - 1);
      address_blocks[i].owner_pid = allocation_blocks[i].process;
    }
  }
  MPI_Bcast(address_blocks, number_blocks, GVM_DISTRIBUTED_BLOCKS, MASTER_RANK, communicator);
  for (i = 0; i < number_blocks; i++) {
    if (address_blocks[i].owner_pid == my_rank) {
      my_start_address = (void*)address_blocks[i].startAddress;
      local_mem_size = (address_blocks[i].endAddress - address_blocks[i].startAddress) + 1;
    }
  }
  // Done in two stages to "tag" each entry in directory with the start address, this is so we can free memory in the directory
  for (i = 0; i < number_blocks; i++) {
    gvi_directory_registerMemoryStartEnd((void*)address_blocks[i].startAddress, (void*)address_blocks[i].endAddress,
                                         address_blocks[i].owner_pid, (unsigned long)my_start_address);
  }
  memkind_free(MEMKIND_DEFAULT, address_blocks);
  if (local_mem_size > 0) mlock(my_start_address, local_mem_size);
  MPI_Win win;
  MPI_Win_create(my_start_address, (MPI_Aint)local_mem_size, 1, MPI_INFO_NULL, MPI_COMM_WORLD, &win);

  return my_start_address;
}

/**
 * Finds and hole in the GVM and will then store this in the allocations table or raise an error if such a hole can not be found
 */
static unsigned long findAndAllocateHoleInGVMForAllocation(size_t requestedSize) {
  unsigned long startAddress = findHoleInGVMForAllocation(requestedSize);
  if (startAddress != -1) {
    struct gvm_allocated_memory* newnode =
        (struct gvm_allocated_memory*)memkind_malloc(MEMKIND_DEFAULT, sizeof(struct gvm_allocated_memory));
    newnode->startAddress = startAddress;
    newnode->endAddress = startAddress + requestedSize;

    struct gvm_allocated_memory* head = distributedMemoryAllocations, *prevhead = distributedMemoryAllocations;
    while (head != NULL) {
      if (head->startAddress > startAddress) {
        newnode->next = head;
        newnode->prev = head->prev;
        if (head->prev != NULL) head->prev->next = newnode;
        head->prev = newnode;
        if (distributedMemoryAllocations == head) distributedMemoryAllocations = newnode;
        return startAddress;
      }
      prevhead = head;
      head = head->next;
    }
    newnode->next = NULL;
    newnode->prev = prevhead;
    if (prevhead != NULL) prevhead->next = newnode;
    if (distributedMemoryAllocations == NULL) distributedMemoryAllocations = newnode;
    return startAddress;
  } else {
    fprintf(stderr, "Can not find space in distributed heap for a memory allocation of %ld bytes\n", requestedSize);
    abort();
    return 0;
  }
}

/**
 * Finds an appropriately sized hole in the global virtual address space that can hold memory of this required size
 */
static unsigned long findHoleInGVMForAllocation(size_t requestedSize) {
  unsigned long previousAddress = (unsigned long)address_space_descriptor.distributedMemoryHeapGlobalAddressStart;
  struct gvm_allocated_memory* head = distributedMemoryAllocations;
  while (head != NULL) {
    if (head->startAddress - previousAddress >= requestedSize) {
      return previousAddress;
    }
    previousAddress = head->endAddress;
    head = head->next;
  }
  if ((unsigned long)address_space_descriptor.globalAddressSpaceEnd - previousAddress >= requestedSize) {
    return previousAddress;
  } else {
    return -1;
  }
}
