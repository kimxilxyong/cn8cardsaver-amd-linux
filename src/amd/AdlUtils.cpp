/* XMRig
 * Copyright 2010      Jeff Garzik <jgarzik@pobox.com>
 * Copyright 2012-2014 pooler      <pooler@litecoinpool.org>
 * Copyright 2014      Lucas Jones <https://github.com/lucasjones>
 * Copyright 2014-2016 Wolf9466    <https://github.com/OhGodAPet>
 * Copyright 2016      Jay D Dee   <jayddee246@gmail.com>
 * Copyright 2017-2018 XMR-Stak    <https://github.com/fireice-uk>, <https://github.com/psychocrypt>
 * Copyright 2018      SChernykh   <https://github.com/SChernykh>
 * Copyright 2016-2018 XMRig       <https://github.com/xmrig>, <support@xmrig.com>
 *
 *   This program is free software: you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation, either version 3 of the License, or
 *   (at your option) any later version.
 *
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 *   GNU General Public License for more details.
 *
 *   You should have received a copy of the GNU General Public License
 *   along with this program. If not, see <http://www.gnu.org/licenses/>.
 */

#include <uv.h>
#include <cmath>
#include <thread>

#include <CL/cl_ext.h>

#include "amd/OclCache.h"
#include "amd/OclError.h"
#include "amd/OclLib.h"
#include "amd/AdlUtils.h"

//#include "amd/OclGPU.h"
#include "common/log/Log.h"
#include "workers/Workers.h"
#include "3rdparty\ADL\adl_sdk.h"
#include "3rdparty\ADL\adl_defines.h"
#include "3rdparty\ADL\adl_structures.h"

 // Definitions of the used function pointers.Add more if you use other ADL APIs.
typedef int(*ADL2_MAIN_CONTROL_CREATE)(ADL_MAIN_MALLOC_CALLBACK, int, ADL_CONTEXT_HANDLE*);
typedef int(*ADL2_MAIN_CONTROL_DESTROY)(ADL_CONTEXT_HANDLE);
typedef int(*ADL2_OVERDRIVEN_TEMPERATURE_GET) (ADL_CONTEXT_HANDLE, int, int, int*);


ADL2_MAIN_CONTROL_CREATE                        ADL2_Main_Control_Create = nullptr;
ADL2_MAIN_CONTROL_DESTROY                       ADL2_Main_Control_Destroy = nullptr;
ADL2_OVERDRIVEN_TEMPERATURE_GET					ADL2_OverdriveN_Temperature_Get = nullptr;


// Memory allocation function
void* __stdcall ADL_Main_Memory_Alloc(int iSize)
{
	void* lpBuffer = malloc(iSize);
	return lpBuffer;
}

// Optional Memory de-allocation function
void __stdcall ADL_Main_Memory_Free(void* lpBuffer)
{
	if (NULL != lpBuffer)
	{
		free(lpBuffer);
		lpBuffer = NULL;
	}
}

int AdlUtils::InitADL(ADL_CONTEXT_HANDLE *context)
{
#ifdef __linux__
	void *hDLL;             // Handle to .so library
#else
	HINSTANCE hDLL;         // Handle to DLL
#endif

#ifdef __linux__
	hDLL = dlopen("libatiadlxx.so", RTLD_LAZY | RTLD_GLOBAL);
#else
	hDLL = LoadLibrary(TEXT("atiadlxx.dll"));
	if (hDLL == NULL)
		// A 32 bit calling application on 64 bit OS will fail to LoadLIbrary.
		// Try to load the 32 bit library (atiadlxy.dll) instead
		hDLL = LoadLibrary(TEXT("atiadlxy.dll"));
#endif

	if (NULL == hDLL)
	{
		fprintf(stderr, "ADL library not found! Please install atiadlxx.dll and atiadlxy.dll.\n");
		return ADL_ERR;
	}

	ADL2_Main_Control_Create = (ADL2_MAIN_CONTROL_CREATE)GetProcAddress(hDLL, "ADL2_Main_Control_Create");
	ADL2_Main_Control_Destroy = (ADL2_MAIN_CONTROL_DESTROY)GetProcAddress(hDLL, "ADL2_Main_Control_Destroy");
	ADL2_OverdriveN_Temperature_Get = (ADL2_OVERDRIVEN_TEMPERATURE_GET)GetProcAddress(hDLL, "ADL2_OverdriveN_Temperature_Get");

	if (NULL == ADL2_Main_Control_Create ||
		NULL == ADL2_Main_Control_Destroy ||
		NULL == ADL2_OverdriveN_Temperature_Get )
	{
		fprintf(stderr, "ADL APIs are missing!\n");
		return ADL_ERR;
	}

	return ADL2_Main_Control_Create(ADL_Main_Memory_Alloc, 1, context);
}


int AdlUtils::Temperature(ADL_CONTEXT_HANDLE context, int deviceIdx)
{	
	int temp;
	if (ADL_OK != ADL2_OverdriveN_Temperature_Get(context, deviceIdx, 1, &temp)) {
		LOG_ERR("Failed to get ADL2_OverdriveN_Temperature_Get");
	}

	return temp / 1000;
}

int  AdlUtils::DoCooling(ADL_CONTEXT_HANDLE context, int deviceIdx, CoolingContext *cool)
{
	const int StartSleepFactor = 10;

	cool->Temp = AdlUtils::Temperature(context, deviceIdx);
	if (cool->Temp > Workers::maxtemp()) {
		if (!cool->NeedsCooling) {
			cool->SleepFactor = StartSleepFactor;
			LOG_INFO("Card %u temperature %u is over %i, reduced mining, sleep time %i", deviceIdx, cool->Temp, Workers::maxtemp(), cool->SleepFactor);
		}
		cool->NeedsCooling = true;
	}
	if (cool->NeedsCooling) {
		if (cool->Temp < Workers::maxtemp() - Workers::falloff()) {
			LOG_INFO("Card %u temperature %i is below %i, do full mining, sleeptime was %u", deviceIdx, cool->Temp, Workers::maxtemp() - Workers::falloff(), cool->SleepFactor);
			cool->LastTemp = cool->Temp;
			cool->NeedsCooling = false;
			cool->SleepFactor = StartSleepFactor;
		}
		if (cool->LastTemp <= cool->Temp) {
			cool->SleepFactor = cool->SleepFactor * 2;
			//LOG_INFO("Card %u Temperature %i iSleepFactor %i LastTemp %i NeedCooling %i ", deviceIdx, temp, cool->SleepFactor, cool->LastTemp, cool->NeedCooling);
		}
		cool->LastTemp = cool->Temp;
	}
	if (cool->NeedsCooling) {
		int iReduceMining = 10;
		//LOG_INFO("Card %u Temperature %i iReduceMining %i iSleepFactor %i LastTemp %i NeedCooling %i ", deviceIdx, temp, iReduceMining, cool->SleepFactor, cool->LastTemp, cool->NeedCooling);

		do {
			std::this_thread::sleep_for(std::chrono::milliseconds(cool->SleepFactor));
			iReduceMining = iReduceMining - 1;
		} while (iReduceMining > 0);
	}
	return ADL_OK;
}

int  AdlUtils::ReleaseADL(ADL_CONTEXT_HANDLE context)
{
	return ADL2_Main_Control_Destroy(context);
}