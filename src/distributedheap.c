/*
 * distributedheap.c
 *
 *  Created on: 30 Mar 2016
 *      Author: nick
 */

#include "distmem.h"
#include "distmem_mpi.h"
#include "directory.h"
#include <memkind/internal/memkind_default.h>
#include <memkind/internal/memkind_arena.h>
#include <stdio.h>

static void* globalDistributedMemoryHeapCurrentBottom;
static void* distributedheap_malloc(struct distmem*, size_t, size_t, int, ...);
static void distributedheap_free(struct memkind*, void*);

static struct distmem_ops DISTRIBUTED_MEMORY_VTABLE = {.dist_malloc = distributedheap_malloc, .dist_create = distmem_arena_create};

memkind_t DISTRIBUTEDHEAP_CONTIGUOUS_KIND;

void initialise_distributed_heap(void* globalDistributedMemoryHeapBottomA) {
  globalDistributedMemoryHeapCurrentBottom = globalDistributedMemoryHeapBottomA;
  DISTRIBUTED_MEMORY_VTABLE.memkind_operations = (struct memkind_ops*)memkind_malloc(MEMKIND_DEFAULT, sizeof(struct memkind_ops));
  DISTRIBUTED_MEMORY_VTABLE.memkind_operations->create = memkind_arena_create;
  DISTRIBUTED_MEMORY_VTABLE.memkind_operations->destroy = memkind_arena_destroy;
  DISTRIBUTED_MEMORY_VTABLE.memkind_operations->malloc = memkind_arena_malloc;
  DISTRIBUTED_MEMORY_VTABLE.memkind_operations->calloc = memkind_arena_calloc;
  DISTRIBUTED_MEMORY_VTABLE.memkind_operations->posix_memalign = memkind_arena_posix_memalign;
  DISTRIBUTED_MEMORY_VTABLE.memkind_operations->realloc = memkind_arena_realloc;
  DISTRIBUTED_MEMORY_VTABLE.memkind_operations->free = distributedheap_free;
  DISTRIBUTED_MEMORY_VTABLE.memkind_operations->get_size = memkind_default_get_size;
  DISTRIBUTED_MEMORY_VTABLE.memkind_operations->get_arena = memkind_thread_get_arena;
  int err = distmem_create(&DISTRIBUTED_MEMORY_VTABLE, "distributedcontiguous", &DISTRIBUTEDHEAP_CONTIGUOUS_KIND);
  if (err) {
    fprintf(stderr, "Error allocating distributed kind\n");
  }
}

static void distributedheap_free(struct memkind* kind, void* ptr) {
  struct distmem_mpi_memory_information* mpi_memory_info = distmem_mpi_get_info(kind, ptr);
  void* baseGlobalAddress = getGlobalAddress(ptr);
  // free the elements per process in the mpi bit too
  int myRank;
  MPI_Comm_rank(mpi_memory_info->communicator, &myRank);
  int i;
  for (i = myRank - 1; i >= 0; i--) {
    baseGlobalAddress -= mpi_memory_info->elements_per_process[i] * mpi_memory_info->element_size;
  }
  for (i = 0; i < mpi_memory_info->procs_distributed_over; i++) {
    removeMemoryByGlobalAddress(baseGlobalAddress);
    baseGlobalAddress += mpi_memory_info->elements_per_process[i] * mpi_memory_info->element_size;
  }
  distmem_free(kind, ptr);
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
