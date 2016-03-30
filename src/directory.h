/*
 * directory.h
 *
 *  Created on: 30 Mar 2016
 *      Author: nick
 */

#ifndef DIRECTORY_H_
#define DIRECTORY_H_

void registerLocalMemory(void*, void*);
void removeMemory(void*);
void* getGlobalAddress(void*);
void* getLocalAddress(void*);

#endif /* DIRECTORY_H_ */
