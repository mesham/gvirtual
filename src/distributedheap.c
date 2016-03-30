/*
 * distributedheap.c
 *
 *  Created on: 30 Mar 2016
 *      Author: nick
 */

#include "distmem.h"
#include "distmem_mpi.h"
#include <stdio.h>

static void* globalDistributedMemoryHeapCurrentBottom;
static void* distributedheap_malloc(struct distmem*, size_t, size_t, int, ...);

static struct distmem_ops DISTRIBUTED_MEMORY_VTABLE = {.dist_malloc = distributedheap_malloc, .dist_create = distmem_arena_create};

memkind_t DISTRIBUTEDHEAP_CONTIGUOUS_KIND;

void initialise_distributed_heap(void* globalDistributedMemoryHeapBottomA) {
  globalDistributedMemoryHeapCurrentBottom = globalDistributedMemoryHeapBottomA;
  int err = distmem_create_default(&DISTRIBUTED_MEMORY_VTABLE, "distributedcontiguous", &DISTRIBUTEDHEAP_CONTIGUOUS_KIND);
  if (err) {
    fprintf(stderr, "Error allocating distributed kind\n");
  }
}

static void* distributedheap_malloc(struct distmem* dist_kind, size_t element_size, size_t number_elements, int nargs, ...) {
  va_list ap;
  va_start(ap, nargs);
  MPI_Comm communicator = va_arg(ap, MPI_Comm);
  void* localAddress = distmem_mpi_arena_malloc(dist_kind, element_size, number_elements, nargs, communicator);
  struct distmem_mpi_memory_information* mpi_memory_info = distmem_mpi_get_info(dist_kind->memkind, localAddress);
  int i, myRank;
  size_t data_per_process;
  MPI_Comm_rank(communicator, &myRank);
  for (i = 0; i < mpi_memory_info->procs_distributed_over; i++) {
    data_per_process = mpi_memory_info->elements_per_process[i] * element_size;
    if (myRank == i) {
      registerLocalMemory(globalDistributedMemoryHeapCurrentBottom, localAddress, data_per_process, myRank);
    } else {
      registerRemoteMemory(globalDistributedMemoryHeapCurrentBottom, data_per_process, i);
    }
    globalDistributedMemoryHeapCurrentBottom += data_per_process;
  }
  return localAddress;
}
