/*
 * TCBlock.h
 *
 *  Created on: May 1, 2015
 *      Author: timothy
 */

#ifndef TCBLOCK_H_
#define TCBLOCK_H_

extern "C" {
#include <VirtualMachine.h>
#include <Machine.h>
#include <stdint.h>
#include <unistd.h>

class TCBlock{
public:
	TVMThreadID myTid;

	TVMThreadPriority tPriority;
	TVMThreadState tState;
	TVMMemorySize tMemorySize;
	uint8_t* tStackBase;
	TVMThreadEntry tEntryFunction;
	void* tEntryParam;
	TVMTick tTicks;
	SMachineContext tContext;
	int result;
	void* sharedMemAddress;
	TCBlock();
	virtual ~TCBlock();
};
}
#endif /* TCBLOCK_H_ */
