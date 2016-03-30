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
  initialise_global_virtual_address_space();
  int* data = (int*)memkind_malloc(LOCALHEAP_KIND, sizeof(int) * 10);
  void* globalAddress = getGlobalAddress(data);
  printf("Local heap data: Local=0x%x Global=0x%x\n", data, globalAddress);
  int* dist_data = (int*)distmem_mpi_malloc(DISTRIBUTEDHEAP_CONTIGUOUS_KIND, sizeof(int), 10, MPI_COMM_WORLD);
  void* dist_GlobalAddress = getGlobalAddress(dist_data);
  printf("Distributed heap data: Local=0x%x Global=0x%x\n", dist_data, dist_GlobalAddress);
  memkind_free(DISTRIBUTEDHEAP_CONTIGUOUS_KIND, dist_data);
  memkind_free(LOCALHEAP_KIND, data);
  memkind_finalize();
  MPI_Finalize();
  return 0;
}
