/*
 * directory.h
 *
 *  Created on: 30 Mar 2016
 *      Author: nick
 */

#ifndef DIRECTORY_H_
#define DIRECTORY_H_

void gvi_directory_registerMemory(void*, size_t, int);
void gvi_directory_registerMemoryStartEnd(void*, void*, int, unsigned long);
void gvi_directory_removeMemoryByAddress(void*);
void gvi_directory_removeAllMemoriesByUUID(unsigned long);
int gvi_directory_getHomeNode(void*);

#endif /* DIRECTORY_H_ */
