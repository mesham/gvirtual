/*
 * localheap.c
 *
 *  Created on: 29 Mar 2016
 *      Author: nick
 */

#include "localheap.h"
#include "gvirtual.h"
#include "directory.h"
#include <memkind.h>
#include <memkind/internal/memkind_pmem.h>
#include <sys/param.h>
#include <sys/mman.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <math.h>

void *baseLocalHeapAddress;

memkind_t LOCALHEAP_KIND;
static void *my_pmem_mmap(struct memkind *, void *, size_t);

void *initialise_local_heap_space(int myRank, int totalRanks, void *global_base_address) {
  int i;
  struct memkind_ops *my_memkind_ops = (struct memkind_ops *)memkind_malloc(MEMKIND_DEFAULT, sizeof(struct memkind_ops));
  memcpy(my_memkind_ops, &MEMKIND_PMEM_OPS, sizeof(struct memkind_ops));
  my_memkind_ops->mmap = my_pmem_mmap;

  struct distmem_ops *localheap_vtable = (struct distmem_ops *)memkind_malloc(MEMKIND_DEFAULT, sizeof(struct distmem_ops));
  localheap_vtable->dist_malloc = NULL;
  localheap_vtable->dist_create = distmem_arena_create;
  localheap_vtable->memkind_operations = my_memkind_ops;
  localheap_vtable->dist_determine_distribution = NULL;

  distmem_create(localheap_vtable, "localheap", &LOCALHEAP_KIND);

  unsigned long start_addresses[totalRanks];
  if (myRank == 0) {
    size_t jemk_chunksize_exponent;
    size_t s = sizeof(jemk_chunksize_exponent);
    jemk_mallctl("opt.lg_chunk", &jemk_chunksize_exponent, &s, NULL, 0);

    int jemk_chunksize = (int)pow(2.0, jemk_chunksize_exponent);
    for (i = 0; i < totalRanks; i++) {
      if (i == 0) {
        start_addresses[i] = (unsigned long)global_base_address;
      } else {
        start_addresses[i] = start_addresses[i - 1] + LOCAL_HEAP_SIZE;
      }
      start_addresses[i] = roundup(start_addresses[i], jemk_chunksize);
    }
  }
  MPI_Bcast(start_addresses, totalRanks, MPI_UNSIGNED_LONG, 0, MPI_COMM_WORLD);

  struct memkind_pmem *priv = LOCALHEAP_KIND->priv;
  priv->fd = 0;
  priv->addr = (void *)start_addresses[myRank];
  priv->max_size = LOCAL_HEAP_SIZE;
  priv->offset = 0;

  mlock(priv->addr, priv->max_size);

  for (i = 0; i < totalRanks; i++) {
    registerMemory((void *)start_addresses[i], LOCAL_HEAP_SIZE, i);
  }
  MPI_Win win;
  MPI_Win_create(priv->addr, LOCAL_HEAP_SIZE, 1, MPI_INFO_NULL, MPI_COMM_WORLD, &win);
  return priv->addr;
}

static void *my_pmem_mmap(struct memkind *kind, void *addr, size_t size) { return ((struct memkind_pmem *)kind->priv)->addr; }
