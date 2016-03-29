/*
 * localheap.c
 *
 *  Created on: 29 Mar 2016
 *      Author: nick
 */

#include "localheap.h"
#include <memkind.h>
#include <memkind/internal/memkind_pmem.h>
#include <sys/param.h>
#include <sys/mman.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>

#define LOCAL_HEAP_SIZE 4194304

memkind_t LOCALHEAP_KIND;
static void *my_pmem_mmap(struct memkind *, void *, size_t);

struct memkind_ops MEMKIND_MY_OPS = {
    .create = memkind_pmem_create,
    .destroy = memkind_pmem_destroy,
    .malloc = memkind_arena_malloc,
    .calloc = memkind_arena_calloc,
    .posix_memalign = memkind_arena_posix_memalign,
    .realloc = memkind_arena_realloc,
    .free = memkind_default_free,
    .mmap = my_pmem_mmap,
    .get_mmap_flags = memkind_pmem_get_mmap_flags,
    .get_arena = memkind_thread_get_arena,
    .get_size = memkind_pmem_get_size, };

static struct distmem_ops LOCALHEAP_MEMORY_VTABLE = {
    .dist_malloc = NULL,
    .dist_create = distmem_arena_create,
    .memkind_operations = &MEMKIND_MY_OPS};

void initialise_local_heap_space() {
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
}

static void *my_pmem_mmap(struct memkind *kind, void *addr, size_t size) {
  struct memkind_pmem *priv = kind->priv;
  void *tr = priv->addr + priv->offset;
  priv->offset += size;
  void *returnAddress =
      mmap(tr, size, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANON, -1, 0);
  if (returnAddress == MAP_FAILED) {
    fprintf(stderr, "MMap call failed for local heap allocation\n");
  }

  return returnAddress;
}
