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

static int myRank, totalRanks;
void *localAddressBase, *virtualAddressBase;

memkind_t LOCALHEAP_KIND;
static void *my_pmem_mmap(struct memkind *, void *, size_t);
static void *localheap_malloc(struct memkind *, size_t);
static void localheap_free(struct memkind *, void *);
static void *generateGlobalVirtualAddress(void *);

struct memkind_ops MEMKIND_MY_OPS = {.create = memkind_pmem_create,
                                     .destroy = memkind_pmem_destroy,
                                     .malloc = localheap_malloc,
                                     .calloc = memkind_arena_calloc,
                                     .posix_memalign = memkind_arena_posix_memalign,
                                     .realloc = memkind_arena_realloc,
                                     .free = localheap_free,
                                     .mmap = my_pmem_mmap,
                                     .get_mmap_flags = memkind_pmem_get_mmap_flags,
                                     .get_arena = memkind_thread_get_arena,
                                     .get_size = memkind_pmem_get_size, };

static struct distmem_ops LOCALHEAP_MEMORY_VTABLE = {
    .dist_malloc = NULL, .dist_create = distmem_arena_create, .memkind_operations = &MEMKIND_MY_OPS};

void initialise_local_heap_space(int myRanka, int totalRanksa) {
  myRank = myRanka;
  totalRanks = totalRanksa;
  distmem_create(&LOCALHEAP_MEMORY_VTABLE, "localheap", &LOCALHEAP_KIND);
  size_t Chunksize = LOCAL_HEAP_SIZE;
  void *addr = malloc(Chunksize);
  size_t s = sizeof(Chunksize);
  jemk_mallctl("opt.lg_chunk", &Chunksize, &s, NULL, 0);
  void *aligned_addr = (void *)roundup((uintptr_t)addr, Chunksize);
  struct memkind_pmem *priv = LOCALHEAP_KIND->priv;
  priv->fd = 0;
  priv->addr = addr;
  priv->max_size = roundup(s, Chunksize);
  priv->offset = (uintptr_t)aligned_addr - (uintptr_t)addr;

  virtualAddressBase = (void *)GLOBAL_HEAP_BASE_ADDRESS + (myRank * LOCAL_HEAP_SIZE);
  int i;
  for (i = 0; i < totalRanks; i++) {
    if (i != myRank) {
      registerRemoteMemory((void *)GLOBAL_HEAP_BASE_ADDRESS + (i * LOCAL_HEAP_SIZE), LOCAL_HEAP_SIZE, i);
    }
  }
}

static void *my_pmem_mmap(struct memkind *kind, void *addr, size_t size) {
  struct memkind_pmem *priv = kind->priv;
  void *tr = priv->addr + priv->offset;
  priv->offset += size;
  void *returnAddress = mmap(tr, size, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANON, -1, 0);
  if (returnAddress == MAP_FAILED) {
    fprintf(stderr, "MMap call failed for local heap allocation\n");
  }
  localAddressBase = returnAddress;
  return returnAddress;
}

static void *localheap_malloc(struct memkind *kind, size_t size) {
  void *localAddress = memkind_arena_malloc(kind, size);
  registerLocalMemory(generateGlobalVirtualAddress(localAddress), localAddress, size, myRank);
  return localAddress;
}

static void localheap_free(struct memkind *kind, void *ptr) {
  memkind_default_free(kind, ptr);
  removeMemoryByLocalAddress(ptr);
}

static void *generateGlobalVirtualAddress(void *localAddress) { return virtualAddressBase + (localAddress - localAddressBase); }
