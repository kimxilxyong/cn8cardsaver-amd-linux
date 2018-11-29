


#ifndef __ADLUTILS_H__
#define __ADLUTILS_H__

#include <iostream>
#include <fstream>
#include <uv.h>

#include "3rdparty/ADL/adl_structures.h"

// Memory allocation function
//void* __stdcall ADL_Main_Memory_Alloc(int iSize);
// Optional Memory de-allocation function
//void __stdcall ADL_Main_Memory_Free(void* lpBuffer);

typedef struct _CoolingContext {
	int SleepFactor = 0;
	int LastTemp = 0;
	int CurrentTemp = 0;
	int CurrentFan = 0;
	bool NeedsCooling = false;
	bool FanIsAutomatic = false;
	bool IsFanControlEnabled = false;
	int pciBus = -1;
	int Card = -1;
	std::ifstream ifsTemp;
	std::ifstream ifsFan;
	//uv_mutex_t m_mutex;
} CoolingContext;

class AdlUtils
{
public:
	
	static int InitADL(ADL_CONTEXT_HANDLE *context, CoolingContext *cool);
	static int ReleaseADL(ADL_CONTEXT_HANDLE context, CoolingContext *cool);
	static int Temperature(ADL_CONTEXT_HANDLE context, cl_device_id DeviceId, int deviceIdx, CoolingContext *cool);
	static int TemperatureLinux(ADL_CONTEXT_HANDLE context, cl_device_id DeviceId, int deviceIdx, CoolingContext *cool);
	static int TemperatureWindows(ADL_CONTEXT_HANDLE context, cl_device_id DeviceId, int deviceIdx, CoolingContext *cool);
	static int SetFanPercent(ADL_CONTEXT_HANDLE context, CoolingContext *cool, int percent);
	static int SetFanPercentLinux(ADL_CONTEXT_HANDLE context, CoolingContext *cool, int percent);
	static int SetFanPercentWindows(ADL_CONTEXT_HANDLE context, CoolingContext *cool, int percent);
	static int DoCooling(ADL_CONTEXT_HANDLE context, cl_device_id DeviceID, int deviceIdx, int ThreadID, CoolingContext *cool);

};


#endif // __ADLUTILS_H__
