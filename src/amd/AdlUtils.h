


#ifndef __ADLUTILS_H__
#define __ADLUTILS_H__

#include "3rdparty\ADL\adl_structures.h"

// Memory allocation function
void* __stdcall ADL_Main_Memory_Alloc(int iSize);
// Optional Memory de-allocation function
void __stdcall ADL_Main_Memory_Free(void* lpBuffer);

typedef struct _CoolingContext {
	int SleepFactor = 0;
	int LastTemp = 0;
	int Temp = 0;
	bool NeedsCooling = false;
	int card = -1;
} CoolingContext;

class AdlUtils
{
public:

	//static GpuContext *m_ctx;
	
	static int InitADL(ADL_CONTEXT_HANDLE *context);
	static int ReleaseADL(ADL_CONTEXT_HANDLE context);
	static int Temperature(ADL_CONTEXT_HANDLE context, int deviceIdx);
	static int DoCooling(ADL_CONTEXT_HANDLE context, int deviceIdx, CoolingContext *cool);

};


#endif // __ADLUTILS_H__
