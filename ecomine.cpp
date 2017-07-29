/**
 * Source code of ecomine.exe
 * Please note this tool requires a special nvapi SDK not included in this project.
 */

#include "stdafx.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "nvapi.h"

// Init NVAPIs
bool initializeApi(NvPhysicalGpuHandle hGpu[], NvU32 *pCount)
{
	NvU32               physicalGpuCount    = 0;
	NvU32               tccGpuCount         = 0;
	NvAPI_Status        ret                 = NVAPI_ERROR;
	NvPhysicalGpuHandle physicalGpuHandles[NVAPI_MAX_PHYSICAL_GPUS];
	NvPhysicalGpuHandle tccGpuHandles[NVAPI_MAX_PHYSICAL_GPUS];

	// Init to default
	*pCount = 0;

	if (NvAPI_Initialize() != NVAPI_OK)
	{
		return false;
	}

	ret = NvAPI_EnumPhysicalGPUs(physicalGpuHandles, &physicalGpuCount);
	if(ret == NVAPI_OK)
	{
		for(NvU32 i = 0; i < physicalGpuCount; i++)
		{
			hGpu[*pCount] = physicalGpuHandles[i];
			(*pCount)++;
		}
	}

	ret = NvAPI_EnumTCCPhysicalGPUs(tccGpuHandles, &tccGpuCount);
	if(ret == NVAPI_OK)
	{
		for(NvU32 i = 0; i < tccGpuCount; i++)
		{
			hGpu[*pCount] = tccGpuHandles[i];
			(*pCount)++;
		}
	}

	if(*pCount < 1)
	{
		return false;
	}

	return true;
}

void modify_gclk(NvPhysicalGpuHandle hGpu, NvS32 delta)
{
	NvAPI_Status                status;
	NV_GPU_PERF_PSTATES20_INFO  ps20Info = { 0 };

	ps20Info.version = NV_GPU_PERF_PSTATES20_INFO_VER1;

	// Specify only data required for given parameter.
	ps20Info.numPstates      = 1;
	ps20Info.numClocks       = 1;
	ps20Info.numBaseVoltages = 0;
	ps20Info.pstates[0].pstateId = NVAPI_GPU_PERF_PSTATE_P0;
	ps20Info.pstates[0].clocks[0].domainId = NVAPI_GPU_PUBLIC_CLOCK_GRAPHICS;
	ps20Info.pstates[0].clocks[0].freqDelta_kHz.value = delta;

	status = NvAPI_GPU_SetPstates20(hGpu, &ps20Info);
	if (status == NVAPI_OK)
	{
		printf("GRAPHIC CLK offset applied successfully\n");
	}
	else
	{
		printf("API call failed with error %d\n", status);
	}
}

void modify_mclk(NvPhysicalGpuHandle hGpu, NvS32 delta)
{
	NvAPI_Status status;
	NV_GPU_PERF_PSTATES20_INFO  ps20Info = { 0 };

	ps20Info.version = NV_GPU_PERF_PSTATES20_INFO_VER1;

	// Specify only data required for given parameter.
	ps20Info.numPstates      = 1;
	ps20Info.numClocks       = 1;
	ps20Info.numBaseVoltages = 0;
	ps20Info.pstates[0].pstateId = NVAPI_GPU_PERF_PSTATE_P0;
	ps20Info.pstates[0].clocks[0].domainId = NVAPI_GPU_PUBLIC_CLOCK_MEMORY;
	ps20Info.pstates[0].clocks[0].freqDelta_kHz.value = delta;

	status = NvAPI_GPU_SetPstates20(hGpu, &ps20Info);
	if (status == NVAPI_OK)
	{
		printf("MEMORY CLK offset applied successfully\n");
	}
	else
	{
		printf("API call failed with error %d\n", status);
	}
}

void setPowerCap(NvPhysicalGpuHandle hGpu, NV_GPU_CLIENT_POWER_POLICIES_POLICY_ID policyId, float power)
{
	NvAPI_Status                        status;
	NV_GPU_CLIENT_POWER_POLICIES_INFO   polInfo = { 0 };
	NV_GPU_CLIENT_POWER_POLICIES_STATUS polStat = { 0 };
	NvU32                               index;

	// Check if power policy is supported on this GPU
	polInfo.version = NV_GPU_CLIENT_POWER_POLICIES_INFO_VER;
	status = NvAPI_GPU_ClientPowerPoliciesGetInfo(hGpu, &polInfo);
	if (status != NVAPI_OK)
	{
		printf("API call failed with error %d\n", status);
		return;
	}
	if (polInfo.bSupported == 0)
	{
		printf("Power policies are not supported!\n");
		return;
	}
	for (index = 0; index < polInfo.numPolicies; index++)
	{
		if (polInfo.policies[index].policyId == policyId)
		{
			break;
		}
	}
	if (index == polInfo.numPolicies)
	{
		printf("Requested power policy is not supported!\n");
		return;
	}
	if ((((NvU32)(power * 1000.0)) > polInfo.policies[index].powerLimitMax.mp) ||
		(((NvU32)(power * 1000.0)) < polInfo.policies[index].powerLimitMin.mp))
	{
		printf("Requested power policy limit exceeds max allowed value!\n");
		return;
	}

	polStat.version     = NV_GPU_CLIENT_POWER_POLICIES_STATUS_VER;
	polStat.numPolicies = 1;
	polStat.policies[0].policyId = policyId;
	polStat.policies[0].powerLimit.bMilliWattValid = 0;
	polStat.policies[0].powerLimit.mp = (NvU32)(power * 1000.0);
	status = NvAPI_GPU_ClientPowerPoliciesSetStatus(hGpu, &polStat);
	if (status != NVAPI_OK)
	{
		printf("API call failed with error %d\n", status);
		return;
	}
	printf("Requested policy limit applied successfully\n");
}

/*!
 * Main Function - entry point for perfdebug.
 */
int
main
(
	int   argc,
	char *argv[]
)
{
	NvPhysicalGpuHandle hGpu[NVAPI_MAX_PHYSICAL_GPUS];
	NvU32 gpuCount   = 0;
	NvU32 gpu        = 0;
	NvU32 i          = 0;
	float powerDelta = 60.0;
	NvS32 freqDelta  = 100000;

	// Initialize NVAPI.
	if (!initializeApi(hGpu, &gpuCount))
	{
		printf("\nNvAPI initialization failed\n");
		return 0;
	}

	// For ALL connected GPUs
	for (gpu = 0; gpu < gpuCount; gpu++)
	{
		// Reset the counter
		i = 1;

		// Set TGP
		if (i < (NvU32)argc)
		{
			sscanf_s(argv[i], "%f", &powerDelta);
		}
		printf("\nSetting power cap = %.2f %%\n", powerDelta);
		setPowerCap(hGpu[gpu], NV_GPU_CLIENT_POWER_POLICIES_POLICY_TOTAL_GPU_POWER, powerDelta);
		i++;

		// Offset Graphics clocks
		if (i < (NvU32)argc)
		{
			sscanf_s(argv[i], "%u", &freqDelta);
		}
		printf("\nSetting Graphics clocks offset = %u kHz\n", freqDelta);
		modify_gclk(hGpu[gpu], freqDelta);
		i++;

		// Offset Memory clocks
		if (i < (NvU32)argc)
		{
			sscanf_s(argv[i], "%u", &freqDelta);
		}
		printf("\nSetting Memory clocks offset = %u kHz\n", freqDelta);
		modify_mclk(hGpu[gpu], freqDelta);
	}

	return 0;
}
