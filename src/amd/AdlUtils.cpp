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
#include <iostream>
#include <fstream>
#include <dlfcn.h>
#include <string>
#include <unistd.h>
#include <sys/types.h>

#include <CL/cl_ext.h>

#include "amd/OclCache.h"
#include "amd/OclError.h"
#include "amd/OclLib.h"
#include "amd/AdlUtils.h"

//#include "amd/OclGPU.h"
#include "common/log/Log.h"
#include "workers/Workers.h"
#include "3rdparty/ADL/adl_sdk.h"
#include "3rdparty/ADL/adl_defines.h"
#include "3rdparty/ADL/adl_structures.h"

 // Definitions of the used function pointers.Add more if you use other ADL APIs.
typedef int(*ADL2_MAIN_CONTROL_CREATE)(ADL_MAIN_MALLOC_CALLBACK, int, ADL_CONTEXT_HANDLE*);
typedef int(*ADL2_MAIN_CONTROL_DESTROY)(ADL_CONTEXT_HANDLE);
typedef int(*ADL2_OVERDRIVEN_TEMPERATURE_GET) (ADL_CONTEXT_HANDLE, int, int, int*);
typedef int(*ADL2_ADAPTER_NUMBEROFADAPTERS_GET)(ADL_CONTEXT_HANDLE, int*);
typedef int(*ADL2_ADAPTER_ADAPTERINFO_GET)(ADL_CONTEXT_HANDLE context, LPAdapterInfo lpInfo, int iInputSize);

static uv_mutex_t m_mutex;


ADL2_MAIN_CONTROL_CREATE                        ADL2_Main_Control_Create = nullptr;
ADL2_MAIN_CONTROL_DESTROY                       ADL2_Main_Control_Destroy = nullptr;
ADL2_OVERDRIVEN_TEMPERATURE_GET					ADL2_OverdriveN_Temperature_Get = nullptr;
ADL2_ADAPTER_NUMBEROFADAPTERS_GET				ADL2_Adapter_NumberOfAdapters_Get = nullptr;
ADL2_ADAPTER_ADAPTERINFO_GET					ADL2_Adapter_AdapterInfo_Get = nullptr;


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

#if defined (__linux__)
// equivalent functions in linux
void * GetProcAddress( void * pLibrary, const char * name)
{
    return dlsym( pLibrary, name);
}


const char *getUserName()
{
  uid_t uid = geteuid();
  struct passwd *pw = getpwuid(uid);
  if (pw)
  {
    return pw->pw_name;
  }

  return "";
}

#endif

int AdlUtils::InitADL(ADL_CONTEXT_HANDLE *context, CoolingContext *cool)
{
#ifdef __linux__
	void *hDLL = NULL;             // Handle to .so library
#else
	HINSTANCE hDLL;         // Handle to DLL
#endif

#ifdef __linux__
	//hDLL = dlopen("./libatiadlxx.so", RTLD_LAZY | RTLD_GLOBAL);

	//string fn("TEST");
	//fn = "/sys/module/amdgpu/drivers/pci:amdgpu/0000:" << cool->pciBus << ":00.0/hwmon/hwmon" << cool->Card << "/temp1_input";
	char filenameBuf[100];
	snprintf(filenameBuf, 100, "/sys/module/amdgpu/drivers/pci:amdgpu/0000:%02x:00.0/hwmon/hwmon%i/temp1_input", cool->pciBus, cool->Card);
	//LOG_INFO("OPEN std::ifstream %s", filenameBuf);
	cool->ifsTemp.open (filenameBuf, std::ifstream::in);
	if (!cool->ifsTemp.is_open()) {
		LOG_ERR("Failed to open %s", filenameBuf);
		return ADL_ERR;
	}



	snprintf(filenameBuf, 100, "/sys/module/amdgpu/drivers/pci:amdgpu/0000:%02x:00.0/hwmon/hwmon%i/pwm1", cool->pciBus, cool->Card);
	//LOG_INFO("OPEN std::ifstream %s", filenameBuf);
	cool->ifsFan.open (filenameBuf, std::ifstream::in);
	if (!cool->ifsFan.is_open()) {
		LOG_ERR("Failed to open %s", filenameBuf);

		// Check for root
		uid_t uid = geteuid();
		if (uid != 0) {
			LOG_ERR("UserID %i has no priviledge to open fan control, needs to be run as root, fan control disabled!", uid);
			cool->IsFanControlEnabled = false;
		}

		return ADL_ERR;
	}
	cool->IsFanControlEnabled = true;

	
#else	

	hDLL = LoadLibrary("atiadlxx.dll");
	if (hDLL == NULL)
		// A 32 bit calling application on 64 bit OS will fail to LoadLIbrary.
		// Try to load the 32 bit library (atiadlxy.dll) instead
		hDLL = LoadLibrary("atiadlxy.dll");
#endif

#ifndef __linux__
	if (NULL == hDLL)
	{
		//fprintf(stderr, "ADL library not found! Please install atiadlxx.dll(64 bit) or atiadlxy(32 bit).dll.\n");
		LOG_ERR("ADL library not found! %s", dlerror());
		
		return ADL_ERR;
	}

	ADL2_Main_Control_Create = (ADL2_MAIN_CONTROL_CREATE)GetProcAddress(hDLL, "ADL2_Main_Control_Create");
	ADL2_Main_Control_Destroy = (ADL2_MAIN_CONTROL_DESTROY)GetProcAddress(hDLL, "ADL2_Main_Control_Destroy");
	ADL2_OverdriveN_Temperature_Get = (ADL2_OVERDRIVEN_TEMPERATURE_GET)GetProcAddress(hDLL, "ADL2_OverdriveN_Temperature_Get");
	ADL2_Adapter_NumberOfAdapters_Get = (ADL2_ADAPTER_NUMBEROFADAPTERS_GET)GetProcAddress(hDLL, "ADL2_Adapter_NumberOfAdapters_Get");
	ADL2_Adapter_AdapterInfo_Get = (ADL2_ADAPTER_ADAPTERINFO_GET)GetProcAddress(hDLL, "ADL2_Adapter_AdapterInfo_Get");

	if (NULL == ADL2_Main_Control_Create ||
		NULL == ADL2_Main_Control_Destroy ||
		NULL == ADL2_OverdriveN_Temperature_Get ||
		NULL == ADL2_Adapter_NumberOfAdapters_Get ||
		NULL == ADL2_Adapter_AdapterInfo_Get)
	{
		LOG_ERR("ADL APIs are missing! %s", dlerror());
		return ADL_ERR;
	}

	return ADL2_Main_Control_Create(ADL_Main_Memory_Alloc, 1, context);
#else
   return ADL_OK;
#endif
}

int  AdlUtils::ReleaseADL(ADL_CONTEXT_HANDLE context, CoolingContext *cool)
{
#ifdef __linux__
	if (!cool->FanIsAutomatic) {
		// Set back to automatic fan control
		cool->CurrentFan = 0;
		AdlUtils::SetFanPercent(context, cool, cool->CurrentFan);
	}
	if (cool->ifsTemp.is_open()) {
		cool->ifsTemp.close();
	}
	if (cool->ifsFan.is_open()) {
		cool->ifsFan.close();
	}
#else	
	return ADL2_Main_Control_Destroy(context);
#endif	
}

int AdlUtils::SetFanPercent(ADL_CONTEXT_HANDLE context, CoolingContext *cool, int percent)
{
	
	if (!cool->IsFanControlEnabled) {
		return ADL_ERR;
	}
	
	if (percent < 0) percent = 0;
	if (percent > 100) percent = 100;

#ifdef __linux__
	return SetFanPercentLinux(context, cool, percent);
#else
	return SetFanPercentWindowsw(context, cool, percent);
#endif	
}

int AdlUtils::SetFanPercentLinux(ADL_CONTEXT_HANDLE context, CoolingContext *cool, int percent)
{
	int result;
	char filenameBuf[100];
	std::ofstream ifsFanControl;
	std::ofstream ifsFanSpeed;

	snprintf(filenameBuf, 100, "/sys/module/amdgpu/drivers/pci:amdgpu/0000:%02x:00.0/hwmon/hwmon%i/pwm1_enable", cool->pciBus, cool->Card );
	//LOG_INFO("OPEN std::ifstream %s", filenameBuf);
	ifsFanControl.open (filenameBuf);
	if (!ifsFanControl.is_open()) {
		LOG_ERR("Failed to open %s", filenameBuf);

		// Check for root
		uid_t uid = geteuid();
		if (uid != 0) {
			LOG_ERR("UserID %i has no priviledge to open fan control, needs to be run as root, fan control disabled!", uid);
			cool->IsFanControlEnabled = false;
		}
		return ADL_ERR;
	}
	cool->IsFanControlEnabled = true;

	if (percent == 0) {
		char *automatic = "2";
		ifsFanControl << automatic;
		ifsFanControl.close();
		cool->FanIsAutomatic = true;
		result = ADL_OK;
	}
	else {
		char *manual = "1";
		ifsFanControl << manual;
		ifsFanControl.close();
		cool->FanIsAutomatic = false;

		snprintf(filenameBuf, 100, "/sys/module/amdgpu/drivers/pci:amdgpu/0000:%02x:00.0/hwmon/hwmon%i/pwm1", cool->pciBus, cool->Card );
		//LOG_INFO("OPEN std::ifstream %s", filenameBuf);
		ifsFanSpeed.open (filenameBuf);
		if (!ifsFanSpeed.is_open()) {
			LOG_ERR("Failed to open %s", filenameBuf);
			return ADL_ERR;
		}

		int speed = (percent * 255) / 100;
		ifsFanSpeed << speed;
		ifsFanSpeed.close();
		result = ADL_OK;
	}

	return result;
}

int AdlUtils::SetFanPercentWindows(ADL_CONTEXT_HANDLE context, CoolingContext *cool, int percent)
{
	return ADL_OK;
}

int AdlUtils::TemperatureLinux(ADL_CONTEXT_HANDLE context, cl_device_id DeviceId, int deviceIdx, CoolingContext *cool)
{
	int result;
	cl_device_topology_amd topology;

	cl_int status = OclLib::getDeviceInfo(DeviceId, CL_DEVICE_TOPOLOGY_AMD,
		sizeof(cl_device_topology_amd), &topology, NULL);

	if (status != CL_SUCCESS) {
		// Handle error
		//LOG_ERR("Failed to get clGetDeviceInfo %u", status);
		return ADL_ERR;
	}

	if (topology.raw.type == CL_DEVICE_TOPOLOGY_TYPE_PCIE_AMD) {
		//LOG_DEBUG("****************** m_ctx->deviceIdx %u INFO: Topology: PCI[ B#%u D#%u F#%u ]", deviceIdx, (int)topology.pcie.bus, (int)topology.pcie.device, (int)topology.pcie.function);		
	}

	if (cool->pciBus != (int)topology.pcie.bus) {
		//LOG_WARN("PCI Bus mismatch: cool->pciBus != topology.pcie.bus");
	}
	
	int temp = 0;
	int fan = 0;
	try
	{
		if (cool->ifsTemp.is_open()) {
			//LOG_INFO("Filestream is open.");
			cool->ifsTemp.seekg (0, std::ifstream::beg);
			cool->ifsTemp >> temp;
			result = ADL_OK;
		}
		//LOG_INFO("Temp is %i", temp);
		if (cool->ifsFan.is_open()) {
			//LOG_INFO("Filestream is open.");
			cool->ifsFan.seekg (0, std::ifstream::beg);
			cool->ifsFan >> fan;
			result = ADL_OK;
		}

	}
	catch(const std::exception& ex) {
		LOG_ERR("Exception reading temp: %s", ex.what());
		result = ADL_ERR;
	}

	cool->CurrentTemp = temp / 1000;
	cool->CurrentFan = (fan * 100) / 255;

	//LOG_INFO("FINAL std::ifstream temp %i", temp);
			

				//if (topology.pcie.bus == infos[i].iBusNumber) {
			
				//LOG_DEBUG("***** Card %u temp %u adapterindex %u, adlbus %u oclbus %u", i, temp, infos[i].iAdapterIndex, infos[i].iBusNumber, topology.pcie.bus);
			
	
	return result;
}

int AdlUtils::TemperatureWindows(ADL_CONTEXT_HANDLE context, cl_device_id DeviceId, int deviceIdx, CoolingContext *cool)
{
	cl_device_topology_amd topology;

	cl_int status = OclLib::getDeviceInfo(DeviceId, CL_DEVICE_TOPOLOGY_AMD,
		sizeof(cl_device_topology_amd), &topology, NULL);

	if (status != CL_SUCCESS) {
		// Handle error
		LOG_ERR("Failed to get clGetDeviceInfo %u", status);
	}

	if (topology.raw.type == CL_DEVICE_TOPOLOGY_TYPE_PCIE_AMD) {
		LOG_DEBUG("****************** m_ctx->deviceIdx %u INFO: Topology: PCI[ B#%u D#%u F#%u ]", deviceIdx, (int)topology.pcie.bus, (int)topology.pcie.device, (int)topology.pcie.function);
	}
	int temp = 0;
	int iNumberAdapters = 0;
	if (ADL_OK == ADL2_Adapter_NumberOfAdapters_Get(context, &iNumberAdapters)) {

		AdapterInfo* infos = new AdapterInfo[iNumberAdapters];
		if (ADL_OK == ADL2_Adapter_AdapterInfo_Get(context, infos, sizeof(AdapterInfo)*iNumberAdapters)) {

			int iCandidateIndex;
			int iCandidateCount = 0;
			bool found = false;

			for (int i = 0; i < iNumberAdapters; i++) {

				if (topology.pcie.bus == infos[i].iBusNumber) {
					iCandidateIndex = i;

					if (iCandidateCount == deviceIdx) {
						if (ADL_OK != ADL2_OverdriveN_Temperature_Get(context, i, 1, &temp)) {
							LOG_ERR("Failed to get ADL2_OverdriveN_Temperature_Get");
						}

						found = true;
						break;
					}
					else {
						iCandidateCount++;
					}
				}

				LOG_DEBUG("***** Card %u temp %u adapterindex %u, adlbus %u oclbus %u", i, temp, infos[i].iAdapterIndex, infos[i].iBusNumber, topology.pcie.bus);
			}
		}
	}
    cool->CurrentTemp = temp / 1000;
	cool->CurrentFan = -1;
	return ADL_OK;
}

int AdlUtils::Temperature(ADL_CONTEXT_HANDLE context, cl_device_id DeviceId, int deviceIdx, CoolingContext *cool)
{
#ifdef __linux__
	return TemperatureLinux(context, DeviceId, deviceIdx, cool);
#else
	return TemperatureWindows(context, DeviceId, deviceIdx, cool);
#endif
}


int  AdlUtils::DoCooling(ADL_CONTEXT_HANDLE context, cl_device_id DeviceID, int deviceIdx, int ThreadID, CoolingContext *cool)
{
	const int StartSleepFactor = 500;
    const float IncreaseSleepFactor = 1.5;
	const int FanFactor = 5;
    const int FanAutoDefault = 50;

	//LOG_INFO("AdlUtils::Temperature(context, DeviceID, deviceIdx) %i", deviceIdx);
	
	if (AdlUtils::Temperature(context, DeviceID, deviceIdx, cool) != ADL_OK) {
		return ADL_ERR;
	}
	if (cool->CurrentTemp > Workers::maxtemp()) {
		if (!cool->NeedsCooling) {
			cool->SleepFactor = StartSleepFactor;
			LOG_INFO( YELLOW("Card %u Thread %i Temperature %u is over %i, reduced mining, Sleeptime %i"), deviceIdx, ThreadID, cool->CurrentTemp, Workers::maxtemp(), cool->SleepFactor);
		}
		cool->NeedsCooling = true;
	}
	if (cool->NeedsCooling) {
		if (cool->CurrentTemp < Workers::maxtemp() - Workers::falloff()) {
			LOG_INFO( YELLOW("Card %u Thread %i Temperature %i is below %i, do full mining, Sleeptime was %u"), deviceIdx, ThreadID, cool->CurrentTemp, Workers::maxtemp() - Workers::falloff(), cool->SleepFactor);
			//cool->LastTemp = cool->CurrentTemp;
			cool->NeedsCooling = false;
			cool->SleepFactor = StartSleepFactor;

			// Decrease fan speed
			if (cool->CurrentFan > 0)
				cool->CurrentFan = cool->CurrentFan - FanFactor;
			AdlUtils::SetFanPercent(context, cool, cool->CurrentFan);
			

		}
		if (cool->LastTemp < cool->CurrentTemp) {
			cool->SleepFactor = cool->SleepFactor * IncreaseSleepFactor;
			if (cool->SleepFactor > 10000) {
				cool->SleepFactor = 10000;
			}
			//LOG_INFO("Card %u Temperature %i iSleepFactor %i LastTemp %i NeedCooling %i ", deviceIdx, temp, cool->SleepFactor, cool->LastTemp, cool->NeedCooling);
		}
		
	}
	if (cool->NeedsCooling) {
		int iReduceMining = 10;

		// Increase fan speed
		if (cool->CurrentFan < 100)
			cool->CurrentFan = cool->CurrentFan + (FanFactor*3);
		AdlUtils::SetFanPercent(context, cool, cool->CurrentFan);

		//LOG_INFO("Card %u Temperature %i iReduceMining %i iSleepFactor %i LastTemp %i NeedCooling %i ", deviceIdx, temp, iReduceMining, cool->SleepFactor, cool->LastTemp, cool->NeedCooling);

		do {
			std::this_thread::sleep_for(std::chrono::milliseconds(cool->SleepFactor));
			iReduceMining = iReduceMining - 1;
		} while ((iReduceMining > 0) && (Workers::sequence() > 0));
	}
	else {
		// Decrease fan speed if temp keeps dropping
        if (cool->LastTemp > cool->CurrentTemp) {
			if (!cool->FanIsAutomatic) {
				if (cool->CurrentFan > FanAutoDefault) {
					cool->CurrentFan = cool->CurrentFan - FanFactor;
					AdlUtils::SetFanPercent(context, cool, cool->CurrentFan);
				}
				else {
					if (cool->CurrentFan < FanAutoDefault) {
						// Set back to automatic fan control
						cool->CurrentFan = 0;
						AdlUtils::SetFanPercent(context, cool, cool->CurrentFan);
					}	
				}
			}
		}
	}
	cool->LastTemp = cool->CurrentTemp;
	return ADL_OK;
}