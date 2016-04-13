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

static int myRank, totalRanks;
void *baseLocalHeapAddress;

memkind_t LOCALHEAP_KIND;
static void *my_pmem_mmap(struct memkind *, void *, size_t);

// needs to be modified for multiple processes

void *initialise_local_heap_space(int myRanka, int totalRanksa, void *global_base_address) {
  struct memkind_ops *my_memkind_ops = (struct memkind_ops *)memkind_malloc(MEMKIND_DEFAULT, sizeof(struct memkind_ops));
  memcpy(my_memkind_ops, &MEMKIND_PMEM_OPS, sizeof(struct memkind_ops));
  my_memkind_ops->mmap = my_pmem_mmap;

  myRank = myRanka;
  totalRanks = totalRanksa;
  struct distmem_ops LOCALHEAP_MEMORY_VTABLE = {
      .dist_malloc = NULL, .dist_create = distmem_arena_create, .memkind_operations = my_memkind_ops};
  distmem_create(&LOCALHEAP_MEMORY_VTABLE, "localheap", &LOCALHEAP_KIND);

  size_t jemk_chunksize_exponent;
  size_t s = sizeof(jemk_chunksize_exponent);
  jemk_mallctl("opt.lg_chunk", &jemk_chunksize_exponent, &s, NULL, 0);

  int jemk_chunksize = (int)pow(2.0, jemk_chunksize_exponent);

  struct memkind_pmem *priv = LOCALHEAP_KIND->priv;
  priv->fd = 0;
  priv->addr = (void *)roundup((uintptr_t)global_base_address, jemk_chunksize);
  priv->max_size = LOCAL_HEAP_SIZE;
  priv->offset = 0;

  mlock(priv->addr, priv->max_size);

  int i;
  for (i = 0; i < totalRanks; i++) {
    registerMemory(priv->addr + (i * LOCAL_HEAP_SIZE), LOCAL_HEAP_SIZE, i);
  }
  MPI_Win win;
  MPI_Win_create(priv->addr, LOCAL_HEAP_SIZE, 1, MPI_INFO_NULL, MPI_COMM_WORLD, &win);
  return priv->addr;
}

static void *my_pmem_mmap(struct memkind *kind, void *addr, size_t size) { return ((struct memkind_pmem *)kind->priv)->addr; }
