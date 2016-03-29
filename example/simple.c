/*
 * simple.c
 *
 *  Created on: 29 Mar 2016
 *      Author: nick
 */

#include "mpi.h"
#include "gvirtual.h"

int main(int argc, char* argv[]) {
  MPI_Init(&argc, &argv);
  initialise_global_virtual_address_space();
  int* data = (int*)memkind_malloc(LOCALHEAP_KIND, sizeof(int) * 10);
  int i;
  for (i = 0; i < 10; i++) {
    data[i] = 10 - i;
  }
  MPI_Finalize();
  return 0;
}
