#include <stdlib.h>
#include <VirtualMachine.h>
#include <unistd.h>
#include <stdio.h>
#include <iostream>
#include <exception>
#include <Machine.h>
#include <queue>
#include <list>
#include <TCBlock.h>
#include <vector>
#include <map>
#include <cstring>
#include <fcntl.h>
#include <string>

//create list of references to thread control blocks, don't create a list of TCBs
extern "C"{

//used to debug program. Set 1 to show print messages. Set 0 to turn off print messages.
#define COUT if (0) std::cout
#define COUTproj3 if (0) std::cout
#define COUTproj4 if (1) std::cout

struct memPool{
	TVMMemorySize memPoolSize;
	TVMMemoryPoolID memPoolID;
	uint8_t* base;
	TVMMemorySize freeSpace;
	std::map<uint8_t*,TVMMemorySize> freeMem;
	std::map<uint8_t*,TVMMemorySize> allocatedMem;
};

struct Cluster {
	int location;
	std::vector<uint8_t*> data;    //resize with clustersize
	bool dirtyBit;
};

std::vector<uint16_t> FATTable;

struct DirectoryEntry{
	uint16_t firstCluster;
	SVMDirectoryEntry vmDirectoryEntry;
};

//global buffer
uint8_t* tempBuffer;

//global entry vector
std::vector<DirectoryEntry*> directoryEntries;


//want a global memory pool vector
std::vector<memPool*> allMemPools;



TVMStatus VMThreadState(TVMThreadID thread, TVMThreadStateRef state);
void idle(void *param);
void skeletonEntry(void *param);
void schedule();
TVMStatus VMThreadTerminate(TVMThreadID thread);
void insertWaitQueue(TCBlock* thread);
void popWaitQueue(void* sharedMemoryAddress);

//file.o functions
void MyFileCallback(void *param, int result);
TVMStatus VMFileOpen(const char *filename, int flags, int mode, int *filedescriptor);
TVMStatus VMFileClose(int filedescriptor);
TVMStatus VMFileRead(int filedescriptor, void *data, int *length);
TVMStatus VMFileWrite(int filedescriptor, void *data, int *length);
TVMStatus VMFileSeek(int filedescriptor, int offset, int whence, int *newoffset);
TVMStatus VMFilePrint(int filedescriptor, const char *format, ...);

//VMDirectory functions
TVMStatus VMDirectoryOpen(const char *dirname, int *dirdescriptor);
TVMStatus VMDirectoryClose(int dirdescriptor);
TVMStatus VMDirectoryRead(int dirdescriptor, SVMDirectoryEntryRef dirent);
TVMStatus VMDirectoryRewind(int dirdescriptor);
TVMStatus VMDirectoryCurrent(char *abspath);
TVMStatus VMDirectoryChange(const char *path);
TVMStatus VMDirectoryCreate(const char *dirname);
TVMStatus VMDirectoryUnlink(const char *path);

//memory.c functions
TVMStatus VMMemoryPoolCreate(void *base, TVMMemorySize size, TVMMemoryPoolIDRef memory);
TVMStatus VMMemoryPoolDelete(TVMMemoryPoolID memory);
TVMStatus VMMemoryPoolQuery(TVMMemoryPoolID memory, TVMMemorySizeRef bytesleft);
TVMStatus VMMemoryPoolAllocate(TVMMemoryPoolID memory, TVMMemorySize size, void **pointer);
TVMStatus VMMemoryPoolDeallocate(TVMMemoryPoolID memory, void *pointer);


volatile TVMTick timeCounter = 0;
TVMMainEntry VMLoadModule(const char *module);
std::list<TCBlock*> high, normal, low;             //priority lists
std::vector<TCBlock*> vmThreads;				   //all system threads
std::list<TCBlock*> waitingThreads;					//list of waiting threads
TCBlock* currentThread;
std::list<TCBlock*> idleThreads;
TCBlock* idleThread = new TCBlock();
int counter = 1;                   //counter to debug alarm callback
std::list<TCBlock*> waitHigh, waitNormal, waitLow;             //priority lists
extern const TVMMemoryPoolID VM_MEMORY_POOL_ID_SYSTEM = 0;
int fatFileDescriptor;

void readSector(int i, uint8_t* tempBuffer){
	TMachineSignalState sigstate;
	MachineSuspendSignals(&sigstate);

	MachineFileSeek(fatFileDescriptor, 512*i, 0, MyFileCallback, currentThread);
	currentThread->tState = VM_THREAD_STATE_WAITING;
	MachineResumeSignals(&sigstate);
	schedule();

	MachineFileRead(fatFileDescriptor, tempBuffer, 512, MyFileCallback, currentThread);
	currentThread->tState = VM_THREAD_STATE_WAITING;
	MachineResumeSignals(&sigstate);
	schedule();

}

void readCluster(uint16_t clusterLow, uint16_t firstDataSector, Cluster* cluster){

	//reads in sectors of clusters
	//look at fattable to find which cluster to look at next
	//stop at 0xFFF8

	while (clusterLow < 0xFFF8){
		TMachineSignalState sigstate;
		MachineSuspendSignals(&sigstate);

		MachineFileSeek(fatFileDescriptor, 512*(firstDataSector + (clusterLow-2)*2) , 0, MyFileCallback, currentThread);
		currentThread->tState = VM_THREAD_STATE_WAITING;
		MachineResumeSignals(&sigstate);
		schedule();
		MachineFileRead(fatFileDescriptor, tempBuffer, 512, MyFileCallback, currentThread);
		currentThread->tState = VM_THREAD_STATE_WAITING;
		MachineResumeSignals(&sigstate);
		schedule();
		cluster->data.push_back(tempBuffer);

		//read in second sector of cluster
		MachineFileSeek(fatFileDescriptor, 512*(firstDataSector + (clusterLow + 1 -2)*2) , 0, MyFileCallback, currentThread);
		currentThread->tState = VM_THREAD_STATE_WAITING;
		MachineResumeSignals(&sigstate);
		schedule();
		MachineFileRead(fatFileDescriptor, tempBuffer, 512, MyFileCallback, currentThread);
		currentThread->tState = VM_THREAD_STATE_WAITING;
		MachineResumeSignals(&sigstate);
		schedule();
		cluster->data.push_back(tempBuffer);

		uint16_t nextCluster = FATTable[clusterLow];
		clusterLow = nextCluster;
	}

}



TVMStatus VMDirectoryCurrent(char *abspath){

	//have a global currentDirectory string
	//do a lil more
	//strcpy(abspath, currentDirectory.c_str());
	return VM_STATUS_SUCCESS;
}

TVMStatus VMDirectoryOpen(const char *dirname, int *dirdescriptor){

	//set fd to 3 or higher
	return VM_STATUS_SUCCESS;
}

TVMStatus VMDirectoryChange(const char *path){

	//if(path == '.' || './' || '/')
	//     return success
	//else return failure
	return VM_STATUS_SUCCESS;
}

TVMStatus VMDirectoryRewind(int dirdescriptor){

	//set offset to 0
	return VM_STATUS_SUCCESS;
}

TVMStatus VMDirectoryRead(int dirdescriptor, SVMDirectoryEntryRef dirent){
	//read in directory entries
	//increase class/struct offset by 1
	//iff offset > entries in the directory, then return failure
	return VM_STATUS_SUCCESS;
}

TVMStatus VMMemoryPoolCreate(void *base, TVMMemorySize size, TVMMemoryPoolIDRef memory){

	TMachineSignalState sigState;
	MachineSuspendSignals(&sigState);


	COUTproj3 << "entering VMMemoryPoolCreate" << std::endl;
	if(size == 0 || base == NULL){
			MachineResumeSignals(&sigState);
			return VM_STATUS_ERROR_INVALID_PARAMETER;
	}
	COUTproj3 << "Memory size: " <<  size << "allMemPools size " << allMemPools.size() << std::endl;
	memPool* memPools = new memPool();
	memPools->base = (uint8_t*)base;
	memPools->memPoolSize = size;
	memPools->freeSpace = size;
	memPools->memPoolID = allMemPools.size();
	memPools->freeMem.insert(std::pair<uint8_t*, TVMMemorySize>((uint8_t*)base,size));
	*memory = allMemPools.size();
	allMemPools.push_back(memPools);
	COUTproj3 << "VMMemoryPoolCreate success " << size << std::endl;

	MachineResumeSignals(&sigState);
	return VM_STATUS_SUCCESS;
}

TVMStatus VMMemoryPoolAllocate(TVMMemoryPoolID memory, TVMMemorySize size, void **pointer){

	TMachineSignalState sigState;
	MachineSuspendSignals(&sigState);
	COUTproj3 << "entering VMMemoryPoolAllocate" << std::endl;
	if(memory < 0 || memory > allMemPools.size() - 1 || size == 0 || pointer == NULL){
		MachineResumeSignals(&sigState);
		return VM_STATUS_ERROR_INVALID_PARAMETER;
	}


	TVMMemorySize memSize = ((size + 63)/64) * 64;
	COUTproj3 << "allMemPools size: " << allMemPools.size() <<  std::endl;
	COUTproj3 << "freeSpace: " << allMemPools[memory]->freeSpace <<  std::endl;
	COUTproj3 << "memSize: " << memSize <<  std::endl;
	if (allMemPools[memory]->freeSpace < memSize){
		COUTproj3 << "insufficient resources allocate" << std::endl;
		MachineResumeSignals(&sigState);
		return VM_STATUS_ERROR_INSUFFICIENT_RESOURCES;
	}
	for(std::map<uint8_t*,TVMMemorySize>::iterator  it = allMemPools[memory]->freeMem.begin(); it != allMemPools[memory]->freeMem.end();) {
	    if (it->second >= memSize){
	    	*pointer = it->first;
	    	allMemPools[memory]->freeSpace -= memSize;
	    	allMemPools[memory]->allocatedMem.insert(std::pair<uint8_t*, TVMMemorySize>(it->first, memSize));

	    	//if there is a remainder of the freeMem then insert the element with updated base and length
	    	//regardless, erase the piece you just released.
	    	if(it->second - memSize > 0){
	    		allMemPools[memory]->freeMem.insert(std::pair<uint8_t*, TVMMemorySize>(it->first + memSize,it->second - memSize));
	    	}
	    	allMemPools[memory]->freeMem.erase(it);
	    	COUTproj3 << "VMMemoryPoolAllocate Success" << std::endl;
	    	MachineResumeSignals(&sigState);
	    	return VM_STATUS_SUCCESS;
	    }
	    else{
	    	it++;
	    }
	}
	COUTproj3 << "failed allocate" << std::endl;
	MachineResumeSignals(&sigState);
	return VM_STATUS_ERROR_INSUFFICIENT_RESOURCES;
}

TVMStatus VMMemoryPoolDeallocate(TVMMemoryPoolID memory, void *pointer){
	TMachineSignalState sigState;
	MachineSuspendSignals(&sigState);

	COUTproj3 << "entering VMMemoryPoolDeallocate" << std::endl;

	if(memory < 0 || memory > allMemPools.size() - 1 || pointer == NULL){
		MachineResumeSignals(&sigState);
		COUTproj3 << "VMMemoryPoolDeallocate Invalid Parameter Error1" << std::endl;
		return VM_STATUS_ERROR_INVALID_PARAMETER;
	}

	if(allMemPools[memory]->allocatedMem.count((uint8_t*)pointer) == 1){

		COUTproj3 << "allMemPools size: " << allMemPools.size() <<  std::endl;
		COUTproj3 << "freeSpace: " << allMemPools[memory]->freeSpace <<  std::endl;
		COUTproj3 << "deallocated memory length: " << allMemPools[memory]->allocatedMem[(uint8_t*)pointer] <<  std::endl;

		allMemPools[memory]->freeMem.insert(std::pair<uint8_t*, TVMMemorySize>((uint8_t*)pointer, allMemPools[memory]->allocatedMem[(uint8_t*)pointer]));
		allMemPools[memory]->freeSpace += allMemPools[memory]->allocatedMem[(uint8_t*)pointer];
		allMemPools[memory]->allocatedMem.erase((uint8_t*)pointer);

	}
	else{
		MachineResumeSignals(&sigState);
		COUTproj3 << "VMMemoryPoolDeallocate Invalid Parameter Error2" << std::endl;
		return VM_STATUS_ERROR_INVALID_PARAMETER;
	}

	std::map<uint8_t*,TVMMemorySize>::iterator previous = allMemPools[memory]->freeMem.begin();
	std::map<uint8_t*,TVMMemorySize>::iterator current = previous;
	current++;

	while(current != allMemPools[memory]->freeMem.end()){
		if(previous->first + previous->second == current->first){
			previous->second = previous->second + current->second;
			allMemPools[memory]->freeMem.erase(current++);                //this might cause a bug???
		}
		else
			previous = current;
			current++;
	}
	MachineResumeSignals(&sigState);
	COUTproj3 << "VMMemoryPoolDeallocate Success" << std::endl;

	return VM_STATUS_SUCCESS;

}

TVMStatus VMMemoryPoolQuery(TVMMemoryPoolID memory, TVMMemorySizeRef bytesleft){

	TMachineSignalState sigState;
	MachineSuspendSignals(&sigState);
	if(memory < 0 || memory > allMemPools.size() - 1 || bytesleft == NULL){
			MachineResumeSignals(&sigState);
			return VM_STATUS_ERROR_INVALID_PARAMETER;
	}

	*bytesleft = allMemPools[memory]->freeSpace;
	MachineResumeSignals(&sigState);
	return VM_STATUS_SUCCESS;
}

TVMStatus VMMemoryPoolDelete(TVMMemoryPoolID memory){
	COUTproj3 << "Entering VMMemoryPoolDelete" << std::endl;
	TMachineSignalState sigState;
	MachineSuspendSignals(&sigState);
	if(memory < 0 || memory > allMemPools.size() - 1){
		COUTproj3 << "VMMemoryPoolDelete Invalid Perameter Error" << std::endl;
		MachineResumeSignals(&sigState);
		return VM_STATUS_ERROR_INVALID_PARAMETER;
	}
	else if (allMemPools[memory]->freeSpace != allMemPools[memory]->memPoolSize){
		COUTproj3 << "VMMemoryPoolDelete Invalid State Error; freespace: " << allMemPools[memory]->freeSpace << " memPoolSize: " <<
				allMemPools[memory]->memPoolSize << std::endl;
		MachineResumeSignals(&sigState);
		return VM_STATUS_ERROR_INVALID_STATE;
	}

	allMemPools[memory] = NULL;
	MachineResumeSignals(&sigState);
	return VM_STATUS_SUCCESS;
}

//called in VMFileWrite and VMFileRead
void popWaitQueue(void* sharedMemoryAddress){
	TCBlock* tempPriorityThread = new TCBlock();
	if(waitHigh.size() > 0){
		tempPriorityThread = waitHigh.front();
		tempPriorityThread->sharedMemAddress = sharedMemoryAddress;
		waitHigh.pop_front();
	}
	else if(waitNormal.size() > 0){
		tempPriorityThread = waitNormal.front();
		tempPriorityThread->sharedMemAddress = sharedMemoryAddress;
		waitNormal.pop_front();
	}
	else if(waitLow.size() > 0){
		tempPriorityThread = waitLow.front();
		tempPriorityThread->sharedMemAddress = sharedMemoryAddress;
		waitLow.pop_front();
	}
}

void MyFileCallback(void *param, int result){

	TMachineSignalState sigState;
	MachineSuspendSignals(&sigState);

	TCBlock* thread = (TCBlock*)param;
    thread->tState = VM_THREAD_STATE_READY;

    COUT << "entering myFileCallback" << std::endl;

   	if (thread->tPriority == VM_THREAD_PRIORITY_HIGH){
		high.push_back(thread);
	}
	else if (thread->tPriority == VM_THREAD_PRIORITY_NORMAL){
		normal.push_back(thread);
		COUT << "Entering vmThreadActive normal priority size: " << normal.size() <<std::endl;
	}
	else if  (thread->tPriority == VM_THREAD_PRIORITY_LOW){
		low.push_back(thread);
	}
	else if (thread->tPriority == 0){
		idleThreads.push_back(thread);
	}

   	//std::cout << "Result: " << result << std::endl;
   	COUTproj3 << "MyFileCallback Result: " <<  result <<  std::endl;
   	thread->result = result;

   	if(thread->tPriority > currentThread->tPriority){
   		schedule();
   	}


}

//Can be used as time slicing for each thread depending on tick amount, so we call schedule before we return
void AlarmCallBack(void *param){
	COUT << "Entering AlarmCallBack " << counter << std::endl;
	counter++;
	TMachineSignalState sigState;
	MachineSuspendSignals(&sigState);

	std::list<TCBlock*>::iterator iterator;
	iterator = waitingThreads.begin();
	//Checks through waiting list of TCBlock* and decrements tick for each by 1. If tick is zero then push to priority queue.
	//if thread reaches 0 ticks, then push to priority queue and remove thread from waiting queue
	while (iterator != waitingThreads.end()){
		TCBlock* i = *iterator;
		i->tTicks--;
		if(i->tTicks == 0){
			i->tState = VM_THREAD_STATE_READY;
			COUT << "inside iterator loop" << std::endl;
			if (i->tPriority == VM_THREAD_PRIORITY_HIGH){
				high.push_back(i);
			}
			else if (i->tPriority == VM_THREAD_PRIORITY_NORMAL){
				normal.push_back(i);
			}
			else if (i->tPriority == VM_THREAD_PRIORITY_LOW){
				low.push_back(i);
			}
			else if (i->tPriority == 0){
				idleThreads.push_back(i);
			}
			COUT << "inside itterator loop2" << std::endl;
			//schedule();
			iterator = waitingThreads.erase(iterator);
		}
		else{
			iterator++;
		}
	}
	COUT << "inside outter loop" << std::endl;
	schedule();
	MachineResumeSignals(&sigState);
}

TVMStatus VMStart(int tickms, TVMMemorySize heapsize, int machinetickms, TVMMemorySize sharedsize, const char *mount, int argc, char *argv[]){
	COUT << "Entering VMStart" << std::endl;
	void* sharedMemoryBase = MachineInitialize(machinetickms, sharedsize);

	//create System Memory Pool
	uint8_t* mainBase = new uint8_t[heapsize];
	unsigned int* systemPoolID = new unsigned int();
	VMMemoryPoolCreate(mainBase, heapsize , systemPoolID);

	//create Shared Memory Pool
	TVMMemorySize share = ((sharedsize+4095)/4096)*4096;
	unsigned int* sharedMemoryPoolID = new unsigned int();
	VMMemoryPoolCreate((uint8_t*)sharedMemoryBase, share, sharedMemoryPoolID);

	//create idle thread
	idleThread->myTid = vmThreads.size();
	idleThread->tPriority = 0;
	idleThread->tState = VM_THREAD_STATE_DEAD;
	idleThread->tTicks = 0;
	vmThreads.push_back(idleThread);
	if (VM_STATUS_SUCCESS != VMMemoryPoolAllocate(VM_MEMORY_POOL_ID_SYSTEM, 0x100000, (void**)&(idleThread->tStackBase))){
		COUT << "YOU FUCKED UP" << std::endl;
	}
	MachineContextCreate(&idleThread->tContext, idle, 0, idleThread->tStackBase, 0x100000);
	idleThreads.push_back(idleThread);
	COUT << "Created idleThread" << std::endl;

	//create main thread
	TCBlock* mainThread = new TCBlock();
	mainThread->myTid = vmThreads.size();
	mainThread->tPriority = VM_THREAD_PRIORITY_NORMAL;
	mainThread->tState = VM_THREAD_STATE_RUNNING;
	mainThread->tTicks = 0;
	vmThreads.push_back(mainThread);
	currentThread = mainThread;
	COUT << "Created mainThread" << std::endl;

	//mount the fat
	tempBuffer = NULL;
	VMMemoryPoolAllocate(*sharedMemoryPoolID, 512, (void**)&tempBuffer);
	MachineFileOpen(mount, O_RDWR, 0644, MyFileCallback, currentThread);
	currentThread->tState = VM_THREAD_STATE_WAITING;
	schedule();
	//std::cout << "file descriptor: " << currentThread->result<< std::endl;
	fatFileDescriptor = currentThread->result;
	MachineFileRead(currentThread->result, tempBuffer, 512, MyFileCallback, currentThread);
	currentThread->tState = VM_THREAD_STATE_WAITING;
	schedule();


	uint16_t BPB_BytsPerSec = tempBuffer[11] + (((uint16_t)tempBuffer[11 + 1])<<8);
	uint16_t BPB_SecPerClus = tempBuffer[13];
	uint16_t BPB_RsvdSecCnt = tempBuffer[14] + (((uint16_t)tempBuffer[14 + 1])<<8);
	uint16_t BPB_NumFATs  = tempBuffer[16];
	uint16_t BPB_RootEntCnt =  tempBuffer[17] + (((uint16_t)tempBuffer[17 + 1])<<8);
	uint16_t BPB_TotSec16 = tempBuffer[19] + (((uint16_t)tempBuffer[19 + 1])<<8);
	uint16_t BPB_Media =  tempBuffer[21];
	uint16_t BPB_FATSz16 = tempBuffer[22] + (((uint16_t)tempBuffer[22 + 1])<<8);
	uint16_t BPB_SecPerTrk =  tempBuffer[24] + (((uint16_t)tempBuffer[24 + 1])<<8);
	uint16_t BPB_NumHeads = tempBuffer[26] + (((uint16_t)tempBuffer[26 + 1])<<8);
	uint16_t BS_DrvNum = tempBuffer[36];
	uint16_t BS_Reserved1 = tempBuffer[37];
	uint16_t BS_BootSig = tempBuffer[38];
	unsigned int BPB_TotSec32 = tempBuffer[32] + (((unsigned int)tempBuffer[32 + 1])<<8) + (((unsigned int)tempBuffer[32 + 2])<<16) + (((unsigned int)tempBuffer[32 + 3])<<24);

	uint16_t FirstRootSector = BPB_RsvdSecCnt + BPB_NumFATs * BPB_FATSz16;
	uint16_t RootDirectorySectors = (BPB_RootEntCnt * 32) / 512;
	uint16_t FirstDataSector = FirstRootSector + RootDirectorySectors;
	unsigned int ClusterCount = (BPB_TotSec32 - (unsigned int)FirstDataSector) / (unsigned int)BPB_SecPerClus;

	COUTproj4 << "BPB_BytsPerSec: " << BPB_BytsPerSec << std::endl;
	COUTproj4 << "BPB_SecPerClus: " << BPB_SecPerClus << std::endl;
	COUTproj4 << "BPB_RsvdSecCnt: " << BPB_RsvdSecCnt << std::endl;
	COUTproj4 << "BPB_NumFATs: " << BPB_NumFATs<< std::endl;
	COUTproj4 << "BPB_RootEntCnt: " << BPB_RootEntCnt << std::endl;
	COUTproj4 << "BPB_TotSec16: " << BPB_TotSec16 << std::endl;
	COUTproj4 << "BPB_Media: " << BPB_Media << std::endl;
	COUTproj4 << "BPB_FATSz16: " << BPB_FATSz16 << std::endl;
	COUTproj4 << "BPB_SecPerTrk: " << BPB_SecPerTrk << std::endl;
	COUTproj4 << "BPB_NumHeads: " << BPB_NumHeads << std::endl;
	COUTproj4 << "BS_DrvNum: " << BS_DrvNum << std::endl;
	COUTproj4 << "BPB_TotSec32: " << BPB_TotSec32 << std::endl;
	COUTproj4 << "BS_Reserved1: " << BS_Reserved1 << std::endl;
	COUTproj4 << "BS_BootSig: " << BS_BootSig << std::endl;
	COUTproj4 << "FirstRootSector: " << FirstRootSector << std::endl;
	COUTproj4 << "RootDirectorySectors: " << RootDirectorySectors << std::endl;
	COUTproj4 << "FirstDataSector: " << FirstDataSector << std::endl;
	COUTproj4 << "ClusterCount: " << ClusterCount << std::endl;

	//Read in the primary FAT

	for(int i = 1; i <= BPB_FATSz16; i++){
		readSector(i,tempBuffer);
		uint16_t *Words = (uint16_t *)tempBuffer;
		for(int i = 0; i <=255; i++){
			FATTable.push_back(Words[i]);
		}
	}

	//WEIRDEST BUG EVER
	for (int i = 0; i <= 255; i++){
		std::cout << std::hex <<  FATTable[i] << " " << std::dec;
		//std::cout << hex <<  FATTable[i] << std::endl;
	}

	/*
	COUTproj4 << "FirstRootSector  " <<FirstRootSector <<  std::endl;
	COUTproj4 << "RootDirectorSector  " <<RootDirectorySectors <<  std::endl;
	*/

	//Read in ROOT
	int count = 0;
	int outerCount = 0;
	for(int i = FirstRootSector; i < FirstRootSector + RootDirectorySectors ; i++){

		readSector(i,tempBuffer);
		outerCount++;
		for(int j = 0; j <(512/32); j++){
			uint8_t charWords[32] = {};
			//memcpy(charWords, tempBuffer + 32*(uint8_t)j,32);
			memcpy(charWords, tempBuffer + (32*j),32);
			count++;
			if (charWords[11] == (VM_FILE_SYSTEM_ATTR_READ_ONLY |  VM_FILE_SYSTEM_ATTR_HIDDEN |  VM_FILE_SYSTEM_ATTR_SYSTEM | VM_FILE_SYSTEM_ATTR_VOLUME_ID)){
				//just do nothing and skip the entry
			}
			else if (charWords[0] == 0x00){
				j = 100;
			}
			else{
				DirectoryEntry* newEntry = new DirectoryEntry();
				newEntry->firstCluster = charWords[26] + ((uint16_t)charWords[27]<<8);
				newEntry->vmDirectoryEntry.DAttributes = charWords[11];
				newEntry->vmDirectoryEntry.DSize = charWords[28] + ((unsigned int)charWords[29]<<8) + ((unsigned int)charWords[30]<<16) + ((unsigned int)charWords[31]<<24) ;
				memcpy(newEntry->vmDirectoryEntry.DShortFileName, charWords, 11);

				uint16_t date = charWords[16] + ((uint16_t)charWords[17]<<8);
				newEntry->vmDirectoryEntry.DCreate.DDay = date & 0x1F;
				newEntry->vmDirectoryEntry.DCreate.DMonth = (date >> 5) & 0xF;
				newEntry->vmDirectoryEntry.DCreate.DYear = (date >> 9) + 1980;
				newEntry->vmDirectoryEntry.DCreate.DHundredth = charWords[13] % 100;
				uint16_t time = charWords[14] + ((uint16_t)charWords[15]<<8);
				newEntry->vmDirectoryEntry.DCreate.DHour = (time >> 11);
				newEntry->vmDirectoryEntry.DCreate.DMinute = (time >> 5) & 0x3F;
				newEntry->vmDirectoryEntry.DCreate.DSecond = ((time & 0x1F)<<1) + charWords[13]/100;

				date = charWords[16] + ((uint16_t)charWords[17]<<8);
				newEntry->vmDirectoryEntry.DModify.DDay = date & 0x1F;
				newEntry->vmDirectoryEntry.DModify.DMonth = (date >> 5) & 0xF;
				newEntry->vmDirectoryEntry.DModify.DYear = (date >> 9) + 1980;
				time = charWords[14] + ((uint16_t)charWords[15]<<8);
				newEntry->vmDirectoryEntry.DModify.DHour = (time >> 11);
				newEntry->vmDirectoryEntry.DModify.DMinute = (time >> 5) & 0x3F;
				newEntry->vmDirectoryEntry.DModify.DSecond = (time & 0x1F)<<1;

				date = charWords[16] + ((uint16_t)charWords[17]<<8);
				newEntry->vmDirectoryEntry.DAccess.DDay = date & 0x1F;
				newEntry->vmDirectoryEntry.DAccess.DMonth = (date >> 5) & 0xF;
				newEntry->vmDirectoryEntry.DAccess.DYear = (date >> 9) + 1980;

				directoryEntries.push_back(newEntry);
			}

		}
	}

	/*
	COUTproj4 << "FirstRootSector  " <<FirstRootSector <<  std::endl;
	COUTproj4 << "RootDirectorSector  " <<RootDirectorySectors <<  std::endl;
	COUTproj4 << "outerCount:  " <<outerCount <<  std::endl;
	COUTproj4 << "count:  " <<count <<  std::endl;
	COUTproj4 << "DirectoryEntries vector size: " << directoryEntries.size() <<  std::endl;
	 */

	//PRINTS OUT THE ROOT TABLE
	for (int i = 0; i < directoryEntries.size(); i++){

		fprintf(stdout, "Entry: %s\n", directoryEntries[i]->vmDirectoryEntry.DShortFileName);

			fprintf(stdout, "	First Cluster: %u\n", directoryEntries[i]->firstCluster);
			fprintf(stdout, "	Size:  %u\n " , directoryEntries[i]->vmDirectoryEntry.DSize);
			fprintf(stdout, "	Create Date/Time: %u/%u/%u %u:%u:%u \n" ,(unsigned int)directoryEntries[i]->vmDirectoryEntry.DCreate.DMonth,
					(unsigned int)directoryEntries[i]->vmDirectoryEntry.DCreate.DDay,
					(unsigned int)directoryEntries[i]->vmDirectoryEntry.DCreate.DYear,
					(unsigned int)directoryEntries[i]->vmDirectoryEntry.DCreate.DHour,
					(unsigned int)directoryEntries[i]->vmDirectoryEntry.DCreate.DMinute,
					(unsigned int)directoryEntries[i]->vmDirectoryEntry.DCreate.DSecond);
			fprintf(stdout, "	Access Date: %u/%u/%u \n" ,(unsigned int)directoryEntries[i]->vmDirectoryEntry.DAccess.DMonth,
								(unsigned int)directoryEntries[i]->vmDirectoryEntry.DAccess.DDay,
								(unsigned int)directoryEntries[i]->vmDirectoryEntry.DAccess.DYear);
			fprintf(stdout, "	Write Date/Time: %u/%u/%u %u:%u:%u \n" ,(unsigned int)directoryEntries[i]->vmDirectoryEntry.DModify.DMonth,
								(unsigned int)directoryEntries[i]->vmDirectoryEntry.DModify.DDay,
								(unsigned int)directoryEntries[i]->vmDirectoryEntry.DModify.DYear,
								(unsigned int)directoryEntries[i]->vmDirectoryEntry.DModify.DHour,
								(unsigned int)directoryEntries[i]->vmDirectoryEntry.DModify.DMinute,
								(unsigned int)directoryEntries[i]->vmDirectoryEntry.DModify.DSecond);

	}



	TVMMainEntry myEntry;	
	myEntry = VMLoadModule(argv[0]);

	useconds_t tickMicroSeconds = tickms * 1000;
	MachineEnableSignals();
	MachineRequestAlarm(tickMicroSeconds, AlarmCallBack, 0);

	if (myEntry != 0){	
		(*myEntry)(argc, argv);
		return VM_STATUS_SUCCESS;
	}
	else{
		return VM_STATUS_FAILURE;
	} 
}

void insertWaitQueue(TCBlock* thread){
	if (thread->tPriority == VM_THREAD_PRIORITY_HIGH){
		waitHigh.push_back(thread);
	}
	else if (thread->tPriority == VM_THREAD_PRIORITY_NORMAL){
		waitNormal.push_back(thread);
	}
	else if  (thread->tPriority == VM_THREAD_PRIORITY_LOW){
		waitLow.push_back(thread);
	}
}

TVMStatus VMFileWrite(int filedescriptor, void *data, int *length){	

	//PROJECT4 IN PROGRESS

	//if fd >= 3
	//very similar to VMFILEREAD
	//The difference is that you are writing this time
	//obtain a new cluster if there is no more space is the sector. To do list just look at last cluster chain.
	//update offset, modify date, access date

	COUTproj3 << "entering VMFileWrite" << std::endl;
	TMachineSignalState sigState;
	MachineSuspendSignals(&sigState);

	if(data == NULL || length == NULL){
		return VM_STATUS_ERROR_INVALID_PARAMETER;
	}
	int bitsLeft = *length;
	TVMMemorySize blockSize = 512;
	int bytesTransferred = 0;

	while(bitsLeft > 0){
		if(bitsLeft < 512){
			if (VM_STATUS_SUCCESS != VMMemoryPoolAllocate(1, bitsLeft , &(currentThread->sharedMemAddress))){
				currentThread->tState = VM_THREAD_STATE_WAITING;
				insertWaitQueue(currentThread);
				MachineResumeSignals(&sigState);
				schedule();
				//when it returns from schedule, you are guaranteed to have the shared memory
			}
			COUT << "in VMFileWrite after the allocate" << std::endl;
			memcpy(currentThread->sharedMemAddress, data, bitsLeft);
			COUT << "in VMFileWrite after the allocate11" << std::endl;

			MachineFileWrite(filedescriptor, currentThread->sharedMemAddress, bitsLeft , MyFileCallback, currentThread);
			currentThread->tState = VM_THREAD_STATE_WAITING;
			COUT << "in VMFileWrite after the allocate111" << std::endl;
			MachineResumeSignals(&sigState);
			schedule();
			bitsLeft -= blockSize;
			data = (uint8_t*)data + bitsLeft;

			if(waitHigh.size() > 0 || waitNormal.size() > 0 || waitLow.size() > 0){
				popWaitQueue(currentThread->sharedMemAddress);
			}
			else{
				VMMemoryPoolDeallocate(1,currentThread->sharedMemAddress);
			}
		}
		else{
			if (VM_STATUS_SUCCESS != VMMemoryPoolAllocate(1, blockSize , &(currentThread->sharedMemAddress))){
				currentThread->tState = VM_THREAD_STATE_WAITING;
				insertWaitQueue(currentThread);
				MachineResumeSignals(&sigState);
				schedule();
				//when it returns from schedule, you are guaranteed to have the shared memory
			}


			memcpy(currentThread->sharedMemAddress, data, blockSize);
			MachineFileWrite(filedescriptor, currentThread->sharedMemAddress, blockSize , MyFileCallback, currentThread);
			currentThread->tState = VM_THREAD_STATE_WAITING;

			MachineResumeSignals(&sigState);
			schedule();
			bitsLeft -= blockSize;
			data = (uint8_t*)data + blockSize;

			if(waitHigh.size() > 0 || waitNormal.size() > 0 || waitLow.size() > 0){
				popWaitQueue(currentThread->sharedMemAddress);
			}
			else{
				VMMemoryPoolDeallocate(1,currentThread->sharedMemAddress);
			}
		}

		bytesTransferred += currentThread->result;
		COUTproj3 << "in VMFileWrite everything, bitsLeft: " << bitsLeft << std::endl;
	}


	if (currentThread->result < 0){
		COUTproj3 << "VMFileWrite Status Failure" << std::endl;
		MachineResumeSignals(&sigState);
		return VM_STATUS_FAILURE;
	}
	else{
		COUTproj3 << "VMFileWrite Success" << std::endl;
		*length = bytesTransferred;
		//*length = currentThread->result;
		MachineResumeSignals(&sigState);
		return VM_STATUS_SUCCESS;
	}
}

TVMStatus VMFileOpen(const char *filename, int flags, int mode, int *filedescriptor){

	//PROJECT4
	//create file class/struct for file functions
	//set FD >= 3

	TMachineSignalState sigState;
	MachineSuspendSignals(&sigState);

	if(filename == NULL || filedescriptor == NULL){
		return VM_STATUS_ERROR_INVALID_PARAMETER;
	}

	MachineFileOpen(filename, flags, mode, MyFileCallback, currentThread);
	currentThread->tState = VM_THREAD_STATE_WAITING;
	MachineResumeSignals(&sigState);
	schedule();

	COUT << "in VMFileOpen after the schedule" << std::endl;

	if (currentThread->result < 0){
		return VM_STATUS_FAILURE;
	}
	else{
		*filedescriptor = currentThread->result;
		return VM_STATUS_SUCCESS;
	}

}

TVMStatus VMFileClose(int filedescriptor){

	//PROJECT 4
	//set filecontainer[fd] = null

	TMachineSignalState sigState;
	MachineSuspendSignals(&sigState);


	MachineFileClose(filedescriptor, MyFileCallback, currentThread);
	currentThread->tState = VM_THREAD_STATE_WAITING;
	MachineResumeSignals(&sigState);
	schedule();

	COUT << "in VMFileClose after the schedule" << std::endl;

	if (currentThread->result < 0){
		return VM_STATUS_FAILURE;
	}
	else{

		return VM_STATUS_SUCCESS;
	}

}

TVMStatus VMFileRead(int filedescriptor, void *data, int *length){
	COUTproj3 << "entering VMFileRead" << std::endl;
	TMachineSignalState sigState;
	MachineSuspendSignals(&sigState);

	//PROJECT4 In progress

	//if fd is >= 3
	//	if fd.flag == rdwr or rdonly, then continue. Else return a failure
	//have some helper function to read in the cluster chain
	//pay careful attention to param length which determines how much you should read in
	//pay attention to file offset
	//update file's access date
	//update offset
	//else if fd < 3, then do project 3 stuff

	if(data == NULL || length == NULL){
			return VM_STATUS_ERROR_INVALID_PARAMETER;
	}
	int bitsLeft = *length;
	TVMMemorySize blockSize = 512;

	int bytesTransferred = 0;
	while(bitsLeft > 0){

		if(bitsLeft < 512){
			if (VM_STATUS_SUCCESS != VMMemoryPoolAllocate(1, bitsLeft , &(currentThread->sharedMemAddress))){
				currentThread->tState = VM_THREAD_STATE_WAITING;
				insertWaitQueue(currentThread);
				MachineResumeSignals(&sigState);
				schedule();
			}
			MachineFileRead(filedescriptor, currentThread->sharedMemAddress , bitsLeft , MyFileCallback, currentThread);
			currentThread->tState = VM_THREAD_STATE_WAITING;
			MachineResumeSignals(&sigState);
			schedule();

			memcpy(data, currentThread->sharedMemAddress, bitsLeft);

			data = (uint8_t*)data + bitsLeft;
			bitsLeft -= blockSize;

			VMMemoryPoolDeallocate(1, currentThread->sharedMemAddress);

			if(waitHigh.size() > 0 || waitNormal.size() > 0 || waitLow.size() > 0){
				popWaitQueue(currentThread->sharedMemAddress);
			}
			else{
				VMMemoryPoolDeallocate(1,currentThread->sharedMemAddress);
			}
		}
		else{
			if (VM_STATUS_SUCCESS != VMMemoryPoolAllocate(1, blockSize , &(currentThread->sharedMemAddress))){
				currentThread->tState = VM_THREAD_STATE_WAITING;
				insertWaitQueue(currentThread);
				MachineResumeSignals(&sigState);
				schedule();
			}

			MachineFileRead(filedescriptor, currentThread->sharedMemAddress , blockSize , MyFileCallback, currentThread);
			currentThread->tState = VM_THREAD_STATE_WAITING;
			MachineResumeSignals(&sigState);
			schedule();
			bitsLeft -= blockSize;
			memcpy(data, currentThread->sharedMemAddress, blockSize);

			data = (uint8_t*)data + blockSize;
			VMMemoryPoolDeallocate(1, currentThread->sharedMemAddress);

			if(waitHigh.size() > 0 || waitNormal.size() > 0 || waitLow.size() > 0){
				popWaitQueue(currentThread->sharedMemAddress);
			}
			else{
				VMMemoryPoolDeallocate(1,currentThread->sharedMemAddress);
			}
		}

		bytesTransferred += currentThread->result;
	}

	COUT << "in VMFileRead after the schedule" << std::endl;
	if (currentThread->result < 0){
		MachineResumeSignals(&sigState);
		return VM_STATUS_FAILURE;
	}
	else{

		*length = bytesTransferred;
		MachineResumeSignals(&sigState);
		return VM_STATUS_SUCCESS;
	}
}

TVMStatus VMFileSeek(int filedescriptor, int offset, int whence, int *newoffset){

	TMachineSignalState sigState;
	MachineSuspendSignals(&sigState);

	MachineFileSeek(filedescriptor, offset, whence, MyFileCallback, currentThread);
	currentThread->tState = VM_THREAD_STATE_WAITING;
	MachineResumeSignals(&sigState);
	schedule();

	COUT << "in VMFileSeek after the schedule" << std::endl;

	if (currentThread->result < 0){
		return VM_STATUS_FAILURE;
	}
	else{
		*newoffset = currentThread->result;
		return VM_STATUS_SUCCESS;
	}
}



//pushes back thread into sleeping queue, then schedules
TVMStatus VMThreadSleep(TVMTick tick){

	COUT << "Entering vmThreadSleep" << std::endl;
	TMachineSignalState sigState;
	MachineSuspendSignals(&sigState);

		if(tick == VM_TIMEOUT_INFINITE){
			MachineResumeSignals(&sigState);
			return VM_STATUS_ERROR_INVALID_PARAMETER;
		}
		else if(tick == VM_TIMEOUT_IMMEDIATE ){
			//Call priorityScheduler here
			currentThread->tState = VM_THREAD_STATE_READY;
			schedule();
			MachineResumeSignals(&sigState);
			return VM_STATUS_SUCCESS;
			//current process yeild its remainder of its processing quantum to the next ready process of higher priority
		}
		else{
			currentThread->tTicks = tick;
			currentThread->tState =  VM_THREAD_STATE_WAITING;
			waitingThreads.push_back(currentThread);
			schedule();
			MachineResumeSignals(&sigState);
			return VM_STATUS_SUCCESS;
		}


}

TVMStatus VMThreadID(TVMThreadIDRef threadref){

	TMachineSignalState sigState;
	MachineSuspendSignals(&sigState);

	if(threadref == NULL){
		MachineResumeSignals(&sigState);
		return VM_STATUS_ERROR_INVALID_PARAMETER;
	}
	else{
		*threadref = currentThread->myTid;
		MachineResumeSignals(&sigState);
		return VM_STATUS_SUCCESS;
	}
}

//intialized TCBlock* object, tid, push into vmThreads
TVMStatus VMThreadCreate(TVMThreadEntry entry, void *param,
TVMMemorySize memsize, TVMThreadPriority prio, TVMThreadIDRef tid){

	COUT << "Entering vmThreadCreate" << std::endl;

	TMachineSignalState sigState;
	MachineSuspendSignals(&sigState);

	if (entry == NULL || tid == NULL){
		MachineResumeSignals(&sigState);
		return VM_STATUS_ERROR_INVALID_PARAMETER;
	}
	else{
		TMachineSignalState sigState;
		MachineSuspendSignals(&sigState);

		//initialize TCBlock
		TCBlock* newThread = new TCBlock();
		newThread->myTid = vmThreads.size();
		newThread->tPriority = prio;
		newThread->tState = VM_THREAD_STATE_DEAD;
		newThread->tMemorySize = memsize - 1;
		newThread->result = 0;

		COUTproj3 << "in vmthreadcreate, before allocating" << std::endl;

		newThread->tStackBase = new uint8_t();
		VMMemoryPoolAllocate(VM_MEMORY_POOL_ID_SYSTEM, memsize, (void**)&(newThread->tStackBase));
		newThread->tEntryFunction = entry;
		newThread->tEntryParam = param;
		newThread->tTicks = 0;

		COUT << "vmthreads size " << vmThreads.size() << std::endl;
		COUT << "intialized tcblock, size of normal " << normal.size() << " " << newThread->tPriority << std::endl;

		*tid =  newThread->myTid;

		vmThreads.push_back(newThread);
		MachineResumeSignals(&sigState);
		return VM_STATUS_SUCCESS;
	}
}

//activates thread, push in queue based on priority, schedule if necessary
TVMStatus VMThreadActivate(TVMThreadID thread){
	COUT << "Entering vmThreadActivate" << std::endl;

	TMachineSignalState sigState;
	MachineSuspendSignals(&sigState);

	if(thread < 0 || thread >= vmThreads.size()){
		COUT << "thread out of bounds" << std::endl;
		MachineResumeSignals(&sigState);
		return VM_STATUS_ERROR_INVALID_ID;
	}
	else if (vmThreads[thread]->tState != VM_THREAD_STATE_DEAD){
		COUT << "VMThreadsActive error invalid state" << std::endl;
		MachineResumeSignals(&sigState);
		return VM_STATUS_ERROR_INVALID_STATE;
	}
	else{
		COUT << "VMThreadsActive else block" << std::endl;
		TCBlock* aThread = vmThreads[thread];
		COUT << "VMThreadsActive else block10" << std::endl;
		COUT << "VMThreadsActive else context" << &aThread->tContext << std::endl;
		COUT << "VMThreadsActive else block10" <<  aThread << std::endl;
		COUT << "VMThreadsActive else block10" << aThread->tStackBase << std::endl;
		COUT << "VMThreadsActive else block10" << aThread->tMemorySize << std::endl;
		MachineContextCreate(&aThread->tContext, skeletonEntry, aThread, aThread->tStackBase, aThread->tMemorySize);
		COUT << "VMThreadsActive else block" << std::endl;
		aThread->tState = VM_THREAD_STATE_READY;
		COUT << "VMThreadsActive else block1" << std::endl;
		if (aThread->tPriority == VM_THREAD_PRIORITY_HIGH){
			COUT << "VMThreadsActive else block2" << std::endl;
			high.push_back(aThread);
		}
		else if (aThread->tPriority == VM_THREAD_PRIORITY_NORMAL){
			COUT << "VMThreadsActive else block2" << std::endl;
			normal.push_back(aThread);
			COUT << "Entering vmThreadActive normal priority size: " << normal.size() <<std::endl;
		}
		else if  (aThread->tPriority == VM_THREAD_PRIORITY_LOW){
			COUT << "VMThreadsActive else block3" << std::endl;
			low.push_back(aThread);
		}
		else if (aThread->tPriority == 0){
			COUT << "VMThreadsActive else block4" << std::endl;
			idleThreads.push_back(aThread);
		}
		COUT << "VMThreadsActive else block5" << std::endl;
		//checks if currentThread priority is less than aThread priority
		if(aThread->tPriority > currentThread->tPriority){
			schedule();
		}
		MachineResumeSignals(&sigState);
		COUT << "VMThreadsActive sucess" << std::endl;
		return VM_THREAD_STATE_READY;
	}
}

void schedule(){

	COUT << "Entering schedule" << std::endl;
	TCBlock* tempPriorityThread = new TCBlock();
	//if thread is running only check for priorities that are higher than it.

	//checks if priority queue is not empty and if activated thread is lower priority then the queue priority
	if(high.size() != 0){
		COUT << "something in high queue" << std::endl;
		if(currentThread->tPriority <= VM_THREAD_PRIORITY_HIGH){
			tempPriorityThread = high.front();
			high.pop_front();

			//prepares and sets states for the context switch
			tempPriorityThread->tState = VM_THREAD_STATE_RUNNING;
			TCBlock* tempOld = currentThread;
			currentThread = tempPriorityThread;
			COUT << "before context switch in schedule" << std::endl;
			MachineEnableSignals();
			if(tempOld != currentThread){
				//COUT << "new context: " << tempPriorityThread->myTid << std::endl;
				//COUT << "old context: " << tempOld->myTid << std::endl;
				MachineContextSwitch(&tempOld->tContext, &tempPriorityThread->tContext);
			}
		}
	}
	else if(normal.size() != 0){
		COUT << "something in normal queue" << std::endl;
		if(currentThread->tPriority <= VM_THREAD_PRIORITY_NORMAL){
			COUT << "in normal queue" << std::endl;
			tempPriorityThread = normal.front();
			normal.pop_front();

			//prepares and sets states for the context switch
			tempPriorityThread->tState = VM_THREAD_STATE_RUNNING;
			TCBlock* tempOld = currentThread;
			currentThread = tempPriorityThread;
			COUT << "before context switch in schedule" << std::endl;
			MachineEnableSignals();
			if(tempOld != currentThread){
				//COUT << "new context: " << tempPriorityThread->myTid << std::endl;
				//COUT << "old context: " << tempOld->myTid << std::endl;
				MachineContextSwitch(&tempOld->tContext, &tempPriorityThread->tContext);
			}
		}

	}
	else if(low.size() != 0){
		COUT << "something in low queue" << std::endl;
		if(currentThread->tPriority <= VM_THREAD_PRIORITY_LOW){
			tempPriorityThread = low.front();
			low.pop_front();

			//prepares and sets states for the context switch
			tempPriorityThread->tState = VM_THREAD_STATE_RUNNING;
			TCBlock* tempOld = currentThread;
			currentThread = tempPriorityThread;
			COUT << "before context switch in schedule" << std::endl;
			MachineEnableSignals();
			if(tempOld != currentThread){
				//COUT << "new context: " << tempPriorityThread->myTid << std::endl;
				//COUT << "old context: " << tempOld->myTid << std::endl;
				MachineContextSwitch(&tempOld->tContext, &tempPriorityThread->tContext);
			}
		}
	}
	else if(currentThread->tState == VM_THREAD_STATE_RUNNING){
		//this accounts for when a thread is running but there are nothing in the queues
		COUT << "in scheduler VM_THREAD_STATE_RUNNING condition block" << idleThreads.size() << std::endl;
		COUT << currentThread->myTid << std::endl;
	}
	else{
		COUT << "in idle thread, idle thread size " << idleThreads.size() << std::endl;
			tempPriorityThread = idleThreads.front();
			//COUT << "idle thread context: " << tempPriorityThread->tContext << std::endl;

			//prepares and sets states for the context switch
			tempPriorityThread->tState = VM_THREAD_STATE_RUNNING;
			TCBlock* tempOld = currentThread;
			currentThread = tempPriorityThread;
			COUT << "before context switch in schedule" << std::endl;
			MachineEnableSignals();
			if(tempOld != currentThread){
				//COUT << "new context: " << tempPriorityThread->myTid << std::endl;
				//COUT << "old context: " << tempOld->myTid << std::endl;
				MachineContextSwitch(&tempOld->tContext, &tempPriorityThread->tContext);
			}
	}
	COUT << "Entering schedule3" << std::endl;
}



//just goes in while loop until it gets interrupted
void idle(void *param){

	COUT << "entered idle function" << std::endl;
	MachineEnableSignals();
	while(1){
		//COUT << "in idle while" << std::endl;
	}
}

//set state to dead, keep global list of threads, and remove thread from all other queues
//sometimes the person might want to activate a thread from its dead state.
TVMStatus VMThreadTerminate(TVMThreadID thread){

	COUT << "Entering vmThreadTerminate" << std::endl;

	TMachineSignalState sigState;
	MachineSuspendSignals(&sigState);

	if(thread < 0 || thread >= vmThreads.size()){
		return VM_STATUS_ERROR_INVALID_ID;
	}
	else if (vmThreads[thread]->tState == VM_THREAD_STATE_DEAD){
		return VM_STATUS_ERROR_INVALID_STATE;
	}
	else{
		COUT << "Entering vmThreadTerminate status success block" << std::endl;

		waitingThreads.remove(vmThreads[thread]);
		high.remove(vmThreads[thread]);
		normal.remove(vmThreads[thread]);
		low.remove(vmThreads[thread]);

		vmThreads[thread]->tState =  VM_THREAD_STATE_DEAD;
		schedule();
		MachineResumeSignals(&sigState);
		return VM_STATUS_SUCCESS;
	}
}

//search and delete iff state == dead, Set location in vmThreads to NULL
TVMStatus VMThreadDelete(TVMThreadID thread){

	COUT << "Entering vmThreadDelete" << std::endl;

	TMachineSignalState sigState;
	MachineSuspendSignals(&sigState);

	if(thread < 0 || thread >= vmThreads.size()){
		MachineResumeSignals(&sigState);
		return VM_STATUS_ERROR_INVALID_ID;
	}
	else if (vmThreads[thread]->tState !=  VM_THREAD_STATE_DEAD){
		MachineResumeSignals(&sigState);
		return VM_STATUS_ERROR_INVALID_STATE;
	}
	else{
		vmThreads[thread] = NULL;
		MachineResumeSignals(&sigState);
		return VM_STATUS_SUCCESS;
	}

}

//Used to guarantee that a thread will terminate after it is done executing
void skeletonEntry(void *param){

	COUT << "Entering skeleton" << std::endl;

	MachineEnableSignals();
	TCBlock* thread = (TCBlock*)param;
	thread->tEntryFunction(thread->tEntryParam);
	VMThreadTerminate(thread->myTid);  // This will allow you to gain control back if the ActualThreadEntry returns
}



TVMStatus VMThreadState(TVMThreadID thread, TVMThreadStateRef state){

	TMachineSignalState sigState;
	MachineSuspendSignals(&sigState);
	if(thread < 0 || thread >= vmThreads.size()){
		MachineResumeSignals(&sigState);
		return VM_STATUS_ERROR_INVALID_ID;
	}
	else if(state == NULL){
		MachineResumeSignals(&sigState);
		return VM_STATUS_ERROR_INVALID_PARAMETER;
	}
	else{
		state = &vmThreads[thread]->tState;
		MachineResumeSignals(&sigState);
		return VM_STATUS_SUCCESS;
	}

}

}
