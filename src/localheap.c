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
#include "cache.h"

#define MASTER_RANK 0

memkind_t LOCALHEAP_KIND;

static void *my_pmem_mmap(struct memkind *, void *, size_t);

/**
 * Initialises the local heap space, the master will determine the local heap start address for each process. This is broadcast to all
 * processes who will then set up their own space via memkind, pin it & allocate the RMA window. All heaps are added into the directory
 * here. Note that there might be slight gaps between local heaps and the start of the global address space and the first heap, this is
 * because jemalloc requires the start address to divide into its chunk size so we have to round up to this.
 */
void *gvi_localHeap_initialise(int myRank, int totalRanks, void *global_base_address) {
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

  struct memkind_ops *my_memkind_ops_cache = (struct memkind_ops *)memkind_malloc(MEMKIND_DEFAULT, sizeof(struct memkind_ops));
  memcpy(my_memkind_ops_cache, &MEMKIND_PMEM_OPS, sizeof(struct memkind_ops));
  my_memkind_ops_cache->mmap = my_pmem_mmap;

  struct distmem_ops *localheap_vtable_cache = (struct distmem_ops *)memkind_malloc(MEMKIND_DEFAULT, sizeof(struct distmem_ops));
  localheap_vtable_cache->dist_malloc = NULL;
  localheap_vtable_cache->dist_create = distmem_arena_create;
  localheap_vtable_cache->memkind_operations = my_memkind_ops_cache;
  localheap_vtable_cache->dist_determine_distribution = NULL;

  distmem_create(localheap_vtable_cache, "localheap_cache", &INTERNAL_LOCALCACHE_KIND);

  size_t jemk_chunksize_exponent;
  size_t s = sizeof(jemk_chunksize_exponent);
  jemk_mallctl("opt.lg_chunk", &jemk_chunksize_exponent, &s, NULL, 0);

  int jemk_chunksize = (int)pow(2.0, jemk_chunksize_exponent);

  unsigned long start_addresses[totalRanks];
  if (myRank == MASTER_RANK) {
    for (i = 0; i < totalRanks; i++) {
      if (i == 0) {
        start_addresses[i] = (unsigned long)global_base_address;
      } else {
        start_addresses[i] = start_addresses[i - 1] + LOCAL_HEAP_SIZE;
      }
      start_addresses[i] = roundup(start_addresses[i], jemk_chunksize);
    }
  }
  MPI_Bcast(start_addresses, totalRanks, MPI_UNSIGNED_LONG, MASTER_RANK, MPI_COMM_WORLD);

  unsigned long local_cache_start_address = roundup(start_addresses[myRank] + (LOCAL_HEAP_SIZE - LOCAL_CACHE_SIZE), jemk_chunksize);

  struct memkind_pmem *priv = (struct memkind_pmem *)INTERNAL_LOCALCACHE_KIND->priv;
  priv->fd = 0;
  priv->addr = (void *)local_cache_start_address;
  priv->max_size = LOCAL_HEAP_SIZE - (local_cache_start_address - 1 - start_addresses[myRank]);
  priv->offset = 0;

  priv = (struct memkind_pmem *)LOCALHEAP_KIND->priv;
  priv->fd = 0;
  priv->addr = (void *)start_addresses[myRank];
  priv->max_size = local_cache_start_address - 1 - start_addresses[myRank];
  priv->offset = 0;

  mlock(priv->addr, LOCAL_HEAP_SIZE);

  for (i = 0; i < totalRanks; i++) {
    gvi_directory_registerMemory((void *)start_addresses[i], LOCAL_HEAP_SIZE, i);
  }
  MPI_Win win;
  MPI_Win_create(priv->addr, LOCAL_HEAP_SIZE, 1, MPI_INFO_NULL, MPI_COMM_WORLD, &win);
  gvi_cache_registerLocalHeap(win, global_base_address, start_addresses, totalRanks);
  return priv->addr;
}

/**
 * Called by memkind and returns the mmapped region
 */
static void *my_pmem_mmap(struct memkind *kind, void *addr, size_t size) { return ((struct memkind_pmem *)kind->priv)->addr; }
