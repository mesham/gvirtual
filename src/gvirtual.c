/*
 * gvirtual.c
 *
 *  Created on: 29 Mar 2016
 *      Author: nick
 */

#include "gvirtual.h"
#include "distmem_mpi.h"
#include "directory.h"
#include <mpi.h>
#include <stddef.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdio.h>
#include <limits.h>
#include <sys/types.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <sys/param.h>
#include <sys/mman.h>
#include <memkind.h>
#include <memkind/internal/memkind_pmem.h>

#define BUFFER_MAX 1024
#define SIZE_MEM_STRIDES 1024
#define VIRTUAL_ADDRESS_SEARCH_MEMORY_SIZE 4 * 1024 * 1024

static int myRank, totalRanks;
void *globalAddressSpaceStart, *globalAddressSpaceEnd, *localHeapGlobalAddressStart, *distributedMemoryHeapGlobalAddressStart;

struct memory_allocation_item {
  unsigned long start, end;
  struct memory_allocation_item *next;
};

static unsigned long determineVirtualAddressSpace();
static int getLocalMemoryMap(unsigned long **);
static struct memory_allocation_item *appendInfoOrModifyMemoryList(struct memory_allocation_item *, unsigned long, unsigned long);
static struct memory_allocation_item *processAllMemorySpacesIntoAllocationTree(unsigned long **, int *);
static struct memory_allocation_item *part(struct memory_allocation_item *, struct memory_allocation_item *,
                                           struct memory_allocation_item **);
static void quickSort(struct memory_allocation_item *, struct memory_allocation_item *);
static void sortMemory(struct memory_allocation_item *);
static void combineContiguousChunks(struct memory_allocation_item *);
static unsigned long getStartOfGlobalVirtualAddressSpace(unsigned long **, int *);
static void *my_pmem_mmap(struct memkind *, void *, size_t);
static void generateMemkindKind();
static void deleteMemkindKind();

memkind_t MEMKIND_MEMLOOKUP;

void initialise_global_virtual_address_space() {
  generateMemkindKind();
  MPI_Comm_rank(MPI_COMM_WORLD, &myRank);
  MPI_Comm_size(MPI_COMM_WORLD, &totalRanks);
  unsigned long startofAddressSpace = determineVirtualAddressSpace();
  globalAddressSpaceStart = mmap((void *)startofAddressSpace, GLOBAL_ADDRESS_SPACE_SIZE, PROT_READ | PROT_WRITE,
                                 MAP_ANON | MAP_PRIVATE | MAP_NORESERVE | MAP_FIXED, -1, 0);
  deleteMemkindKind();
  if (globalAddressSpaceStart == MAP_FAILED) {
    fprintf(stderr, "Global virtual address space reservation error, error is '%s'\n", strerror(errno));
    abort();
  }
  globalAddressSpaceEnd = globalAddressSpaceStart + GLOBAL_ADDRESS_SPACE_SIZE - 1;
  distmem_mpi_init();
  localHeapGlobalAddressStart = initialise_local_heap_space(myRank, totalRanks, globalAddressSpaceStart);
  distributedMemoryHeapGlobalAddressStart = localHeapGlobalAddressStart + (totalRanks * LOCAL_HEAP_SIZE);
  initialise_distributed_heap(distributedMemoryHeapGlobalAddressStart);
}

static void deleteMemkindKind() {
  memkind_arena_destroy(MEMKIND_MEMLOOKUP);
  struct memkind_pmem *priv = MEMKIND_MEMLOOKUP->priv;
  munmap(priv->addr, priv->max_size);
}

static void generateMemkindKind() {
  struct memkind_ops *my_memkind_ops = (struct memkind_ops *)malloc(sizeof(struct memkind_ops));
  memcpy(my_memkind_ops, &MEMKIND_PMEM_OPS, sizeof(struct memkind_ops));
  my_memkind_ops->mmap = my_pmem_mmap;

  int err = memkind_create(my_memkind_ops, "memorymapper", &MEMKIND_MEMLOOKUP);
  if (err) {
    char err_msg[1024];
    memkind_error_message(err, err_msg, 1024);
    fprintf(stderr, "%s", err_msg);
  }

  struct memkind_pmem *priv = MEMKIND_MEMLOOKUP->priv;
  priv->fd = 0;
  priv->max_size = VIRTUAL_ADDRESS_SEARCH_MEMORY_SIZE;
  priv->addr = mmap(0, priv->max_size, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANON, -1, 0);
  priv->offset = 0;
}

static void *my_pmem_mmap(struct memkind *kind, void *addr, size_t size) { return ((struct memkind_pmem *)kind->priv)->addr; }

static unsigned long determineVirtualAddressSpace() {
  unsigned long *memoryDetails;
  int numberOfMemoryEntries = getLocalMemoryMap(&memoryDetails);
  unsigned long startOfGlobalAddressSpace;
  if (myRank == 0) {
    int i, elements;
    MPI_Status status;
    unsigned long **memorySpaces = (unsigned long **)memkind_malloc(MEMKIND_MEMLOOKUP, sizeof(unsigned long *) * totalRanks);
    int *numberEntries = (int *)memkind_malloc(MEMKIND_MEMLOOKUP, sizeof(int) * totalRanks);
    memorySpaces[0] = memoryDetails;
    numberEntries[0] = numberOfMemoryEntries;
    for (i = 1; i < totalRanks; i++) {
      MPI_Probe(i, 0, MPI_COMM_WORLD, &status);
      MPI_Get_elements(&status, MPI_UNSIGNED_LONG, &elements);
      memorySpaces[i] = (unsigned long *)memkind_malloc(MEMKIND_MEMLOOKUP, sizeof(unsigned long) * elements);
      numberEntries[i] = elements / 2;
      MPI_Recv(memorySpaces[i], elements, MPI_UNSIGNED_LONG, i, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
    }
    startOfGlobalAddressSpace = getStartOfGlobalVirtualAddressSpace(memorySpaces, numberEntries);
    for (i = 1; i < totalRanks; i++) {
      memkind_free(MEMKIND_MEMLOOKUP, memorySpaces[i]);
    }
    memkind_free(MEMKIND_MEMLOOKUP, memorySpaces);
    memkind_free(MEMKIND_MEMLOOKUP, numberEntries);
    MPI_Bcast(&startOfGlobalAddressSpace, 1, MPI_UNSIGNED_LONG, 0, MPI_COMM_WORLD);
  } else {
    MPI_Send(memoryDetails, numberOfMemoryEntries * 2, MPI_UNSIGNED_LONG, 0, 0, MPI_COMM_WORLD);
    MPI_Bcast(&startOfGlobalAddressSpace, 1, MPI_UNSIGNED_LONG, 0, MPI_COMM_WORLD);
  }
  if (startOfGlobalAddressSpace == -1) {
    fprintf(stderr, "Can not find a chunk large enough for global virtual address space");
    abort();
  }
  return startOfGlobalAddressSpace;
}

static unsigned long getStartOfGlobalVirtualAddressSpace(unsigned long **memorySpaces, int *entriesPerProcess) {
  struct memory_allocation_item *memoryAllocations = processAllMemorySpacesIntoAllocationTree(memorySpaces, entriesPerProcess);
  struct memory_allocation_item *root = memoryAllocations, *tofree;
  unsigned long allocation = -1;
  while (root != NULL) {
    if (root->next != NULL)
      if (root->next != NULL && root->next->start - root->end > GLOBAL_ADDRESS_SPACE_SIZE) {
        return roundup(root->end + 1, 4096);
      }
    tofree = root;
    root = root->next;
    memkind_free(MEMKIND_MEMLOOKUP, tofree);
  }
  return allocation;
}

static struct memory_allocation_item *processAllMemorySpacesIntoAllocationTree(unsigned long **memorySpaces, int *entriesPerProcess) {
  struct memory_allocation_item *head = NULL;
  int i, j;
  for (i = 0; i < totalRanks; i++) {
    for (j = 0; j < entriesPerProcess[i]; j++) {
      head = appendInfoOrModifyMemoryList(head, memorySpaces[i][j * 2], memorySpaces[i][(j * 2) + 1]);
    }
  }
  sortMemory(head);
  combineContiguousChunks(head);
  return head;
}

static void combineContiguousChunks(struct memory_allocation_item *head) {
  struct memory_allocation_item *root = head, *prevnode = NULL;
  unsigned long prevend = 0;
  while (root != NULL) {
    if (prevnode != NULL) {
      if (root->start == prevend) {
        prevnode->next = root->next;
        prevnode->end = root->end;
        memkind_free(MEMKIND_MEMLOOKUP, root);
        root = prevnode;
      }
    }
    prevend = root->end;
    prevnode = root;
    root = root->next;
  }
}

static struct memory_allocation_item *appendInfoOrModifyMemoryList(struct memory_allocation_item *head, unsigned long startAddress,
                                                                   unsigned long endAddress) {
  struct memory_allocation_item *root = head;
  while (root != NULL) {
    if (root->start <= startAddress && root->end >= endAddress) {
      return head;
    }
    root = root->next;
  }
  struct memory_allocation_item *newItem =
      (struct memory_allocation_item *)memkind_malloc(MEMKIND_MEMLOOKUP, sizeof(struct memory_allocation_item));
  newItem->start = startAddress;
  newItem->end = endAddress;
  newItem->next = head;
  return newItem;
}

static void sortMemory(struct memory_allocation_item *head) {
  struct memory_allocation_item *tail = head;
  while (tail != NULL && tail->next != NULL) tail = tail->next;
  quickSort(head, tail);
}

static struct memory_allocation_item *part(struct memory_allocation_item *head, struct memory_allocation_item *tail,
                                           struct memory_allocation_item **newtail) {
  if (head == NULL || tail == NULL) return NULL;
  unsigned long pt = tail->start;
  unsigned long tempStart, tempEnd;
  struct memory_allocation_item *init = head;
  while (head != tail) {
    if (head->start < pt) {
      *newtail = init;
      tempStart = head->start;
      tempEnd = head->end;
      head->start = init->start;
      head->end = init->end;
      init->start = tempStart;
      init->end = tempEnd;
      init = init->next;
    }
    head = head->next;
  }
  tempStart = tail->start;
  tempEnd = tail->end;
  tail->start = init->start;
  tail->end = init->end;
  init->start = tempStart;
  init->end = tempEnd;
  return init;
}

static void quickSort(struct memory_allocation_item *head, struct memory_allocation_item *tail) {
  if (tail != NULL && tail != head && tail->next != head) {
    struct memory_allocation_item *newlast = NULL;
    struct memory_allocation_item *sec = part(head, tail, &newlast);
    quickSort(head, newlast);
    quickSort(sec->next, tail);
  }
}

static int getLocalMemoryMap(unsigned long **memoryDetails) {
  char fname[PATH_MAX];
  sprintf(fname, "/proc/%ld/maps", (long)getpid());
  FILE *f = fopen(fname, "r");
  unsigned long *tempDetails;
  tempDetails = (unsigned long *)memkind_malloc(MEMKIND_MEMLOOKUP, sizeof(unsigned long) * SIZE_MEM_STRIDES * 2);
  int currentIndex = 0, tempSize = SIZE_MEM_STRIDES;
  if (!f) {
    fprintf(stderr, "Error opening file '%s', error is '%s'\n", fname, strerror(errno));
    abort();
  }
  char buf[BUFFER_MAX];
  while (!feof(f)) {

    if (fgets(buf, sizeof(buf), f) == 0) break;
    sscanf(buf, "%lx-%lx", &tempDetails[currentIndex * 2], &tempDetails[(currentIndex * 2) + 1]);
    currentIndex++;
    if (currentIndex >= tempSize) {
      tempSize += SIZE_MEM_STRIDES;
      tempDetails = (unsigned long *)memkind_realloc(MEMKIND_MEMLOOKUP, tempDetails, sizeof(unsigned long) * tempSize * 2);
    }
  }
  fclose(f);
  *memoryDetails = (unsigned long *)memkind_malloc(MEMKIND_MEMLOOKUP, sizeof(unsigned long) * currentIndex * 2);
  memcpy(*memoryDetails, tempDetails, sizeof(unsigned long) * currentIndex * 2);
  memkind_free(MEMKIND_MEMLOOKUP, tempDetails);
  return currentIndex;
}
