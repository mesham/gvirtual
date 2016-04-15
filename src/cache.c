/*
 * cache.c
 *
 *  Created on: 15 Apr 2016
 *      Author: nick
 */

#include <memkind.h>
#include "cache.h"
#include "gvirtual.h"
#include <string.h>
#include <stdlib.h>
#include <stddef.h>
#include <stdio.h>

struct cached_item {
  unsigned long global_address, local_address;
  size_t number_elements;
  struct cached_item* next, *prev;
};

memkind_t INTERNAL_LOCALCACHE_KIND;

MPI_Win localheap_window;
unsigned long localheaps_start, localheaps_end, *processStartAddresses;
struct cached_item* cachedRoot = NULL;

static struct cached_item* findCachedItemByGlobalAddress(unsigned long);

void gvi_cache_registerLocalHeap(MPI_Win mpi_window, void* global_base_address, unsigned long* startAddresses, int numberRanks) {
  localheap_window = mpi_window;
  localheaps_start = (unsigned long)global_base_address;
  localheaps_end = localheaps_start + (numberRanks * LOCAL_HEAP_SIZE) - 1;
  processStartAddresses = (unsigned long*)memkind_malloc(MEMKIND_DEFAULT, sizeof(unsigned long) * numberRanks);
  memcpy(processStartAddresses, startAddresses, sizeof(unsigned long) * numberRanks);
}

void* gvi_cache_retrieveData(void* address, size_t elements, int homeNode) {
  unsigned long translatedAddress = (unsigned long)address;
  if (translatedAddress <= localheaps_end) {
    // Local heap
    MPI_Aint displacement = translatedAddress - processStartAddresses[homeNode];
    MPI_Win_lock(MPI_LOCK_EXCLUSIVE, homeNode, 0, localheap_window);
    // check return value, if 0 then handle clearing the cache out of released elements
    void* cachedData = memkind_malloc(INTERNAL_LOCALCACHE_KIND, elements);
    MPI_Get(cachedData, elements, MPI_BYTE, homeNode, displacement, elements, MPI_BYTE, localheap_window);
    MPI_Win_unlock(homeNode, localheap_window);
    struct cached_item* new_cached_item = (struct cached_item*)memkind_malloc(MEMKIND_DEFAULT, sizeof(struct cached_item));
    new_cached_item->global_address = translatedAddress;
    new_cached_item->local_address = (unsigned long)cachedData;
    new_cached_item->number_elements = elements;
    new_cached_item->next = cachedRoot;
    new_cached_item->prev = NULL;
    cachedRoot = new_cached_item;
    return cachedData;
  } else {
    // Need to implement dynamic - use a dynamic window with attached memory for this (makes translation of displacement more complex
    // to handle)
  }
  return NULL;
}

void gvi_cache_commitData(void* global_address, int homeNode) {
  // Size/number of elements should be retrieved from an internal list here
  unsigned long translatedAddress = (unsigned long)global_address;
  struct cached_item* existingCachedInfo = findCachedItemByGlobalAddress((unsigned long)global_address);
  if (existingCachedInfo == NULL) {
    fprintf(stderr, "Can not find cached item");
    abort();
  }
  if (translatedAddress <= localheaps_end) {
    // Local heap
    MPI_Aint displacement = translatedAddress - processStartAddresses[homeNode];
    MPI_Win_lock(MPI_LOCK_EXCLUSIVE, homeNode, 0, localheap_window);
    MPI_Put((void*)existingCachedInfo->local_address, existingCachedInfo->number_elements, MPI_BYTE, homeNode, displacement,
            existingCachedInfo->number_elements, MPI_BYTE, localheap_window);
    MPI_Win_unlock(homeNode, localheap_window);
  } else {
    // Need to implement dynamic
  }
}

static struct cached_item* findCachedItemByGlobalAddress(unsigned long global_address) {
  struct cached_item* head = cachedRoot;
  while (head != NULL) {
    if (head->global_address == global_address) return head;
    head = head->next;
  }
  return NULL;
}

// implement release, but hold the data to avoid a re-copy (can clear out on demand)
