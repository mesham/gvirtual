/*
 * directory.h
 *
 *  Created on: 30 Mar 2016
 *      Author: nick
 */

#ifndef DIRECTORY_H_
#define DIRECTORY_H_

void registerMemory(void*, size_t, int);
void registerMemoryStartEnd(void*, void*, int, unsigned long);
void removeMemoryByAddress(void*);
void removeAllMemoriesByUUID(unsigned long);
int getHomeNode(void*);

#endif /* DIRECTORY_H_ */
