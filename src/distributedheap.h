/*
 * distributedheap.h
 *
 *  Created on: 30 Mar 2016
 *      Author: nick
 */

#ifndef DISTRIBUTEDHEAP_H_
#define DISTRIBUTEDHEAP_H_

// The distributed block structure that is communicated from the master to other processes
struct global_distributed_block {
  unsigned long startAddress, endAddress;
  int owner_pid;
};

void gvi_distributedHeap_initialise();

#endif /* DISTRIBUTEDHEAP_H_ */
