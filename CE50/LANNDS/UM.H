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

#ifndef UM_H
#define UM_H

#include <ndis.h>

/* mark - 09/09/05,bolo
#include "debug.h"
#include "list.h"
#include "lmcfg.h"
*/
#include "lm.h"

//******************************************************************************
// UM_DEVICE_INFO_BLOCK
//******************************************************************************

typedef struct {
    // OS independent device structure.
    LM_DEVICE_INFO_BLOCK LmDevice;  // must be the first member of the struct

    // NDIS handles
    NDIS_HANDLE MiniportHandle;
    NDIS_HANDLE ConfigHandle;

    // filter mask
    ULONG FilterMask;

    BOOLEAN OutOfSendResources;

    // Spinlock for the transmit queues.
    NDIS_SPIN_LOCK TransmitQSpinLock;

    // Spinlock for the receive queues.
    NDIS_SPIN_LOCK ReceiveQSpinLock;

    NDIS_SPIN_LOCK IrpSpinLock;
    // Timer for indicating packet completion.
    NDIS_MINIPORT_TIMER IndicateRxTimer;

    // List of resources that are allocated by the driver.  A resource is
    // maintained by a resource descriptor, UM_RESOURCE_DESC.
    LL_LIST_CONTAINER ResourceList;

    // List of free resource descriptors.
    LL_LIST_CONTAINER FreeResourceDescriptors;

        // Resource Number
    int             ResourceNumber;         // lic (6.3.99)

    int             TxDescLeft;

    BOOLEAN         InitCompleted;

} UM_DEVICE_INFO_BLOCK, *PUM_DEVICE_INFO_BLOCK;



//******************************************************************************
// Receive packet descriptor.
//******************************************************************************

typedef struct {
    PNDIS_PACKET pNdisPacket;
    PNDIS_BUFFER pNdisBuffer;
    NDIS_PHYSICAL_ADDRESS pReceiveBufferPhy;
    PUCHAR pReceiveBufferVirt;
} UM_RX_PACKET_DESC, *PUM_RX_PACKET_DESC;



//******************************************************************************
// Send packet descriptor.
//******************************************************************************

typedef struct {
    PNDIS_PACKET pNdisPacket;
} UM_TX_PACKET_DESC, *PUM_TX_PACKET_DESC;



//******************************************************************************
// Ndis packet reserved area.
//******************************************************************************

typedef struct {
    PVOID LmContext;
} NDIS_PACKET_RESERVED, *PNDIS_PACKET_RESERVED;



//******************************************************************************
// Resource descriptors.  Resource descriptors are used to keep track of
// allocated resources.  These resources are freed when the driver unloads.
//******************************************************************************

typedef enum {
    RESOURCE_TYPE_UNASSIGNED    = 0,
    RESOURCE_TYPE_MEMORY        = 1,
    RESOURCE_TYPE_SHARED_MEMORY = 2,
    RESOURCE_TYPE_IO_SPACE      = 3,
    RESOURCE_TYPE_INTERRUPT     = 4,
    RESOURCE_TYPE_MAP_REGISTERS = 5,
    RESOURCE_TYPE_PACKET_POOL   = 6,
    RESOURCE_TYPE_BUFFER_POOL   = 7
} UM_RESOURCE_TYPE;

typedef struct {
    LL_LIST_ENTRY Link;
    UM_RESOURCE_TYPE ResourceType;

    union {
        struct {
            ULONG BlockSize;
            PUCHAR pMemoryBlock;
        } Memory;

        struct {
            ULONG BlockSize;
            PUCHAR pMemoryBlockVirt;
            NDIS_PHYSICAL_ADDRESS pMemoryBlockPhy;
            BOOLEAN Cached;
        } SharedMemory;

        struct {
            NDIS_HANDLE MiniportHandle;
            ULONG IoSpaceSize;
            ULONG IoBase;
            ULONG MappedIoBase;
        } IoSpace;

        struct {
            NDIS_MINIPORT_INTERRUPT NdisInterrupt;
        };

        struct {
            NDIS_HANDLE NdisPacketPool;
        };

        struct {
            NDIS_HANDLE NdisBufferPool;
        };
    }; // union
} UM_RESOURCE_DESC, *PUM_RESOURCE_DESC;



//******************************************************************************
// Function prototypes
//******************************************************************************

PUM_RESOURCE_DESC UM_GetResourceDesc(PUM_DEVICE_INFO_BLOCK pUmDevice);

VOID UM_IndicateRxPackets( IN  PVOID   SystemSpecific1,
                           IN  PVOID   FunctionContext,
                           IN  PVOID   SystemSpecific2,
                           IN  PVOID   SystemSpecific3 );

//VOID UM_FreeResources( PUM_DEVICE_INFO_BLOCK pUmDevice );

#define UM_CHECK_LINK_INTERVAL			1000

#endif // UM_H
