/*
 * simple.c
 *
 *  Created on: 29 Mar 2016
 *      Author: nick
 */

#include "mpi.h"
#include "gvirtual.h"
#include "directory.h"
#include "distributedheap.h"
#include "distmem_mpi.h"
#include <stdio.h>

int main(int argc, char* argv[]) {
  MPI_Init(&argc, &argv);

  initialiseGlobalVirtualAddressSpace();

  int myrank, size;
  MPI_Comm_rank(MPI_COMM_WORLD, &myrank);
  MPI_Comm_size(MPI_COMM_WORLD, &size);

  // First test will allocate some integer array memory in the local heap, ensure it can be written to and read from and then display
  // the array address as well as the read value
  int* data = (int*)memkind_malloc(LOCALHEAP_KIND, sizeof(int) * 10);
  int i, my_sum = 0;
  for (i = 0; i < 10; i++) {
    data[i] = (10 - i) * myrank;
  }
  for (i = 0; i < 10; i++) {
    my_sum += data[i];
  }
  printf("[%d] Int array allocated to local heap at address: 0x%lx, sum of values is %d\n", myrank, (unsigned long)data, my_sum);

  // The second test runs on rank 0 only, where it will pick an address which is in the middle of the local heap for each separate node
  // and displays this address along with the home directory (that node)
  if (myrank == 0) {
    struct global_address_space_descriptor gvm_description = getGlobalVirtualAddressSpaceDescription();
    for (i = 0; i < size; i++) {
      void* address = (void*)gvm_description.globalAddressSpaceStart + (i * LOCAL_HEAP_SIZE + LOCAL_HEAP_SIZE / 2);
      printf("Local heap address 0x%lx is on home node %d\n", (unsigned long)address, getHomeNode(address));
    }
  }

  // The third test will allocate some distributed memory, each rank will display the address that their local bit starts at and then
  // rank 0 will display the home node for each index of the array
  int* dist_data = (int*)distmem_mpi_malloc(DISTRIBUTEDHEAP_CONTIGUOUS_KIND, sizeof(int), 10, MPI_COMM_WORLD);
  printf("[%d] My bit of the distributed array starts at address: 0x%lx\n", myrank, (unsigned long)dist_data);
  if (myrank == 0) {
    int i;
    for (i = 0; i < 10; i++) {
      printf("Distributed element number %d, home node is %d\n", i, getHomeNode(&dist_data[i]));
    }
  }
  memkind_free(DISTRIBUTEDHEAP_CONTIGUOUS_KIND, dist_data);
  memkind_free(LOCALHEAP_KIND, data);
  memkind_finalize();
  MPI_Finalize();
  return 0;
}
