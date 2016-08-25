//******************************************************************************
// The information contained in this file is confidential and proprietary to
// ZNK Corporation.  No part of this file may be reproduced or distributed, in
// any form or by any means for any purpose, without the express written
// permission of ZNK Corporation.
//
// (c) COPYRIGHT 1998 ZNK Corporation, ALL RIGHTS RESERVED.
//
//
// Module Description:
//
//
// History:
//    12/03/98 Hav Khauv        Initial version.
//******************************************************************************

#ifndef MM_H
#define MM_H

/* mark - 09/09/05,bolo
#include "types.h"
#include "lmstatus.h"
*/
#include "lm.h"

//******************************************************************************
// Function prototypes.
//******************************************************************************

LM_STATUS MM_AllocateMemory(PLM_DEVICE_INFO_BLOCK pDevice, PVOID *pMemoryBlock,
    DWORD BlockSize);
LM_STATUS MM_GetConfig(PLM_DEVICE_INFO_BLOCK pDevice);

#ifndef NDISCE_MINIPORT
VOID MM_GetDeviceObjects(PLM_DEVICE_INFO_BLOCK pDevice,
    PVOID *pPhysicalDeviceObject, PVOID *pFunctionalDeviceObject,
    PVOID *pNextDeviceObject);
#endif

LM_STATUS MM_AllocateUmPackets(PLM_DEVICE_INFO_BLOCK pDevice);
LM_STATUS MM_AllocateUmPackets1(PLM_DEVICE_INFO_BLOCK pDevice);

LM_STATUS MM_SetupTxPacket(PLM_DEVICE_INFO_BLOCK pDevice,
    PLM_PACKET_DESC pPacket);

LM_STATUS MM_IndicateTxPacket(PLM_DEVICE_INFO_BLOCK pDevice,
    PLM_PACKET_DESC pPacket);

LM_STATUS MM_IndicateRxPacket(PLM_DEVICE_INFO_BLOCK pDevice,
    PLM_PACKET_DESC pPacket);

LM_STATUS MM_IndicateStatus(PLM_DEVICE_INFO_BLOCK pDevice,
    LM_STATUS Status);
//VOID MM_Wait(UINT);
VOID MM_InitWait(ULONG);
VOID MM_FreeResources(PLM_DEVICE_INFO_BLOCK pDevice);

#endif // MM_H
