/*
 * gvirtual.c
 *
 *  Created on: 29 Mar 2016
 *      Author: nick
 */

#include "gvirtual.h"
#include "distmem_mpi.h"
#include "directory.h"
#include "cache.h"
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
#define MASTER_RANK 0

static int myRank, totalRanks;
struct global_address_space_descriptor address_space_descriptor;

struct memory_allocation_item {
  unsigned long start, end;
  struct memory_allocation_item *next;
};

static unsigned long determineVirtualAddressSpace();
static int getLocalMemoryMap(unsigned long **);
static struct memory_allocation_item *appendInfoOrModifyMemoryList(struct memory_allocation_item *, unsigned long, unsigned long);
static struct memory_allocation_item *processAllMemorySpacesIntoAllocationTree(unsigned long **, int *);
static struct memory_allocation_item *partitionMemoryList(struct memory_allocation_item *, struct memory_allocation_item *,
                                                          struct memory_allocation_item **);
static void quickSort(struct memory_allocation_item *, struct memory_allocation_item *);
static void sortMemory(struct memory_allocation_item *);
static void combineContiguousChunks(struct memory_allocation_item *);
static unsigned long getStartOfGlobalVirtualAddressSpace(unsigned long **, int *);
static void *my_pmem_mmap(struct memkind *, void *, size_t);
static void generateMemkindKind();
static void deleteMemkindKind();

memkind_t MEMKIND_MEMLOOKUP;

/**
 * Initialises the global virtual address space
 */
void gv_initialise() {
  generateMemkindKind();
  MPI_Comm_rank(MPI_COMM_WORLD, &myRank);
  MPI_Comm_size(MPI_COMM_WORLD, &totalRanks);
  unsigned long startofAddressSpace = determineVirtualAddressSpace();
  address_space_descriptor.globalAddressSpaceStart =
      mmap((void *)startofAddressSpace, GLOBAL_ADDRESS_SPACE_SIZE, PROT_READ | PROT_WRITE,
           MAP_ANON | MAP_PRIVATE | MAP_NORESERVE | MAP_FIXED, -1, 0);
  // deleteMemkindKind();
  if (address_space_descriptor.globalAddressSpaceStart == MAP_FAILED) {
    fprintf(stderr, "Global virtual address space reservation error, error is '%s'\n", strerror(errno));
    abort();
  }
  address_space_descriptor.globalAddressSpaceEnd = address_space_descriptor.globalAddressSpaceStart + GLOBAL_ADDRESS_SPACE_SIZE - 1;
  distmem_mpi_init();
  gvi_cache_init();
  address_space_descriptor.localHeapGlobalAddressStart =
      gvi_localHeap_initialise(myRank, totalRanks, address_space_descriptor.globalAddressSpaceStart);
  address_space_descriptor.distributedMemoryHeapGlobalAddressStart =
      address_space_descriptor.localHeapGlobalAddressStart + (totalRanks * LOCAL_HEAP_SIZE);
  gvi_distributedHeap_initialise(address_space_descriptor);
}

/**
 * Returns the descriptor of the global virtual address space
 */
struct global_address_space_descriptor gv_getAddressSpaceDescription() { return address_space_descriptor; }

int gv_getHomeNode(void *address) { return gvi_directory_getHomeNode(address); }

void *gv_acquireMutable(void *globalAddress, size_t elements) {
  return gvi_cache_retrieveData(globalAddress, elements, gvi_directory_getHomeNode(globalAddress));
}

void *gv_acquireConst(void *globalAddress, size_t elements) {
  return gvi_cache_retrieveData(globalAddress, elements, gvi_directory_getHomeNode(globalAddress));
}

void gv_commitKeepMutable(void *address) { gvi_cache_commitData(address, gvi_directory_getHomeNode(address)); }

void gv_commitMakeConst(void *address) { gvi_cache_commitData(address, gvi_directory_getHomeNode(address)); }

void gv_release(void *address) {}

/**
 * Deletes the node's process memory analysis memory kind and unmap the associated memory
 */
static void deleteMemkindKind() {
  memkind_arena_destroy(MEMKIND_MEMLOOKUP);
  struct memkind_pmem *priv = MEMKIND_MEMLOOKUP->priv;
  munmap(priv->addr, priv->max_size);
}

/**
 * Generates the kind that is associated with analysing the nodes's processes memory to find a hole large enough for the global virtual
 * address space. We don't want the memory to change whilst this is happening, hence reserve a space large enough via mmap here and
 * allocate all required data into there
 */
static void generateMemkindKind() {
  struct memkind_ops *my_memkind_ops = (struct memkind_ops *)memkind_malloc(MEMKIND_DEFAULT, sizeof(struct memkind_ops));
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

/**
 * Callback from memkind, returns the address of the mmapped region
 */
static void *my_pmem_mmap(struct memkind *kind, void *addr, size_t size) { return ((struct memkind_pmem *)kind->priv)->addr; }

/**
 * Determines the global virtual address space and returns the value of the start of this region which is large enough on each process
 * to hold the global virtual address space.
 * This will gather all the processes' memory spaces onto the master process, sort them according to address & combine similar and
 * overlapping spaces.
 */
static unsigned long determineVirtualAddressSpace() {
  unsigned long *memoryDetails;
  int numberOfMemoryEntries = getLocalMemoryMap(&memoryDetails);
  unsigned long startOfGlobalAddressSpace;
  if (myRank == MASTER_RANK) {
    int i, elements;
    MPI_Status status;
    unsigned long **memorySpaces = (unsigned long **)memkind_malloc(MEMKIND_MEMLOOKUP, sizeof(unsigned long *) * totalRanks);
    int *numberEntries = (int *)memkind_malloc(MEMKIND_MEMLOOKUP, sizeof(int) * totalRanks);
    for (i = 0; i < totalRanks; i++) {
      if (i != myRank) {
        MPI_Probe(i, 0, MPI_COMM_WORLD, &status);
        MPI_Get_elements(&status, MPI_UNSIGNED_LONG, &elements);
        memorySpaces[i] = (unsigned long *)memkind_malloc(MEMKIND_MEMLOOKUP, sizeof(unsigned long) * elements);
        numberEntries[i] = elements / 2;
        MPI_Recv(memorySpaces[i], elements, MPI_UNSIGNED_LONG, i, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
      } else {
        // Local for the master
        memorySpaces[i] = memoryDetails;
        numberEntries[i] = numberOfMemoryEntries;
      }
    }
    startOfGlobalAddressSpace = getStartOfGlobalVirtualAddressSpace(memorySpaces, numberEntries);
    for (i = 1; i < totalRanks; i++) {
      memkind_free(MEMKIND_MEMLOOKUP, memorySpaces[i]);
    }
    memkind_free(MEMKIND_MEMLOOKUP, memorySpaces);
    memkind_free(MEMKIND_MEMLOOKUP, numberEntries);
    MPI_Bcast(&startOfGlobalAddressSpace, 1, MPI_UNSIGNED_LONG, MASTER_RANK, MPI_COMM_WORLD);
  } else {
    MPI_Send(memoryDetails, numberOfMemoryEntries * 2, MPI_UNSIGNED_LONG, MASTER_RANK, 0, MPI_COMM_WORLD);
    MPI_Bcast(&startOfGlobalAddressSpace, 1, MPI_UNSIGNED_LONG, MASTER_RANK, MPI_COMM_WORLD);
  }
  if (startOfGlobalAddressSpace == -1) {
    fprintf(stderr, "Can not find a chunk large enough for global virtual address space");
    abort();
  }
  return startOfGlobalAddressSpace;
}

/**
 * Builds up a sorted, non-overlapping non-contiguous allocation list of memory for all processes and then identifies a hole large
 * enough for the global virtual address space to sit in. The starting address of this whole is returned
 */
static unsigned long getStartOfGlobalVirtualAddressSpace(unsigned long **memorySpaces, int *entriesPerProcess) {
  struct memory_allocation_item *memoryAllocations = processAllMemorySpacesIntoAllocationTree(memorySpaces, entriesPerProcess);
  struct memory_allocation_item *root = memoryAllocations, *tofree;
  unsigned long allocation = -1;
  while (root != NULL) {
    if (root->next != NULL && allocation == -1 && root->next != NULL && root->next->start > root->end &&
        root->next->start - root->end > GLOBAL_ADDRESS_SPACE_SIZE) {
      long page_size = sysconf(_SC_PAGESIZE);
      allocation = roundup(root->end + 1, page_size);
    }
    tofree = root;
    root = root->next;
    memkind_free(MEMKIND_MEMLOOKUP, tofree);
  }
  return allocation;
}

/**
 * Builds up an allocation tree of all existing processes memory, the entries are unique, sorted, non-contiguous and non-overlapping
 */
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

/**
 * Searches the allocation tree for any contiguous memory items and combines these together
 */
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

/**
 * Will either append a memory item onto the memory list, or returns an existing item if the provided start and end sits within that
 * existing node
 */
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

/**
 * Sorts memory in ascending order based upon the start address
 */
static void sortMemory(struct memory_allocation_item *head) {
  struct memory_allocation_item *tail = head;
  while (tail != NULL && tail->next != NULL) tail = tail->next;
  quickSort(head, tail);
}

/**
 * Partitions and sorts the memory list for Quicksort
 */
static struct memory_allocation_item *partitionMemoryList(struct memory_allocation_item *head, struct memory_allocation_item *tail,
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

/**
 * Quicksort for the sorting of memory list based upon the start address
 */
static void quickSort(struct memory_allocation_item *head, struct memory_allocation_item *tail) {
  if (tail != NULL && tail != head && tail->next != head) {
    struct memory_allocation_item *newlast = NULL;
    struct memory_allocation_item *sec = partitionMemoryList(head, tail, &newlast);
    quickSort(head, newlast);
    quickSort(sec->next, tail);
  }
}

/**
 * Reads the local proc entry for this process to build up an array of existing allocated memory which will later on be sent to the
 * master process
 */
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
