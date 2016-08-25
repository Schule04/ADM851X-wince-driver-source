//******************************************************************************
// Module Description: MM.C
//******************************************************************************
#include "um.h"
#include "mm.h"
#ifndef NDISCE_MINIPORT
#include <ntddk.h>
#else
extern BOOL             Unplugged;
#endif

//#define MM_DEBUG_CE	1
//#define MM_DEBUG_PC	1

//******************************************************************************
// Description:
//    This routine allocates a block of memory including its resource
//    descriptor.  The descriptor is used for keeping track of the memory
//    block so it can be freed later when the driver unloads.  The descriptor
//    occupies the top of the block and is linked to the tail of the
//    resource list.  This function has large over head, call it sparingly.
//
// Returns:
//    LM_STATUS_SUCCESS
//    LM_STATUS_FAILURE
//******************************************************************************
LM_STATUS MM_AllocateMemory( PLM_DEVICE_INFO_BLOCK   pDevice,
                             PVOID *                 pMemoryBlock,
                             DWORD                   BlockSize )
{
    NDIS_PHYSICAL_ADDRESS   MaxAddress = NDIS_PHYSICAL_ADDRESS_CONST(-1, -1);
    NDIS_STATUS             Status;
    PUM_DEVICE_INFO_BLOCK   pUmDevice;
    PUM_RESOURCE_DESC       pResource;
    static ULONG            Tag = 'KHba';

    #ifdef MM_DEBUG_PC
      DbgMessage(VERBOSE, (ADMTEXT("*** MM_AllocateMemory ***\n")));
    #endif
    #ifdef MM_DEBUG_CE
      printf("*** MM_AllocateMemory ***\n");
    #endif
    
    pUmDevice = (PUM_DEVICE_INFO_BLOCK) pDevice;

    // get a resource descriptor
    pResource = UM_GetResourceDesc(pUmDevice);

#ifndef NDISCE_MINIPORT
    // allocate a block of memory plus the descriptor.
    Status = NdisAllocateMemoryWithTag(pMemoryBlock, BlockSize, Tag);
    Tag++;
#else
    Status = NdisAllocateMemory( pMemoryBlock, BlockSize, 0, MaxAddress );
#endif

    if(Status != NDIS_STATUS_SUCCESS)
    {
        #ifdef MM_DEBUG_PC
          DbgMessage(FATAL, (ADMTEXT("NdisAllocateMemoryWithTag failed\n")));
        #endif
//        MM_FreeResources(pDevice);
        *pMemoryBlock = NULL;
        return LM_STATUS_FAILURE;
    } // if
    NdisZeroMemory(*pMemoryBlock, BlockSize);

    // initialize the descriptor
/*
#ifdef NDISCE_MINIPORT
    LockPages( *pMemoryBlock, BlockSize, NULL, LOCKFLAG_WRITE );
#endif
*/
    pResource->ResourceType = RESOURCE_TYPE_MEMORY;
    pResource->Memory.BlockSize = BlockSize;
    pResource->Memory.pMemoryBlock = *pMemoryBlock;

    return LM_STATUS_SUCCESS;
} // MM_AllocateMemory

//******************************************************************************
// Description:
//    This routine allocates UM packet descriptors for each LM packet descriptors.
//
// Returns:
//    LM_STATUS_SUCCESS
//    LM_STATUS_FAILURE
//******************************************************************************
LM_STATUS MM_AllocateUmPackets( PLM_DEVICE_INFO_BLOCK pDevice )
{
    PUM_DEVICE_INFO_BLOCK   pUmDevice;
    NDIS_STATUS             Status;
    PLM_PACKET_DESC         pLmPacket;
    PUM_RX_PACKET_DESC      pUmRxPacket;
    PUM_TX_PACKET_DESC      pUmTxPacket;
    PUM_RESOURCE_DESC       pResource;
    PNDIS_PACKET            pNdisPacket;
    PNDIS_BUFFER            pNdisBuffer;
    NDIS_HANDLE             NdisPacketPool;
    NDIS_HANDLE             NdisBufferPool;

    #ifdef MM_DEBUG_PC
      DbgMessage(INFORM, (ADMTEXT("*** MM_AllocateUmPackets ***\n")));
    #endif
    
    #ifdef MM_DEBUG_CE
      printf("*** MM_AllocateUmPackets ***\n");
    #endif
    
    pUmDevice = (PUM_DEVICE_INFO_BLOCK) pDevice;

    // Allocate memory for UmTxPacket descriptors
    Status = MM_AllocateMemory(pDevice, &pUmTxPacket, sizeof(UM_TX_PACKET_DESC) * pDevice->MaxTxPacketDesc);
    
    if(Status != LM_STATUS_SUCCESS)   return LM_STATUS_FAILURE;
    pLmPacket = (PLM_PACKET_DESC) LL_GetHead(&pDevice->TxPacketFreeList);
    while(pLmPacket)
    {
        pLmPacket->pUmPacket = pUmTxPacket;

        pUmTxPacket++;
        pLmPacket = (PLM_PACKET_DESC) LL_NextEntry(&pLmPacket->Link);
    } // while

    // Allocate NDIS Rx packet pool
    pResource = UM_GetResourceDesc(pUmDevice);    
    NdisAllocatePacketPool(&Status, &NdisPacketPool, pDevice->MaxRxPacketDesc, 0);

    if(Status != NDIS_STATUS_SUCCESS)
    {
        #ifdef MM_DEBUG_PC
          DbgMessage(FATAL, (ADMTEXT("NdisAllocatePacketPool fail\n")));
        #endif
//        MM_FreeResources(pDevice);
        return LM_STATUS_FAILURE;
    }
    pResource->ResourceType = RESOURCE_TYPE_PACKET_POOL;
    pResource->NdisPacketPool = NdisPacketPool;

    // Allocate NDIS Rx buffer pool
    pResource = UM_GetResourceDesc(pUmDevice);    
    NdisAllocateBufferPool(&Status, &NdisBufferPool, pDevice->MaxRxPacketDesc);
    
    if(Status != NDIS_STATUS_SUCCESS)
    {
        #ifdef MM_DEBUG_PC
          DbgMessage(FATAL, (ADMTEXT("NdisAllocateBufferPool fail\n")));
        #endif
//        MM_FreeResources(pDevice);
        return LM_STATUS_FAILURE;
    }
    pResource->ResourceType = RESOURCE_TYPE_BUFFER_POOL;
    pResource->NdisBufferPool = NdisBufferPool;

    // Allocate memory for UmRxPacket descriptors.
    Status = MM_AllocateMemory(pDevice, &pUmRxPacket, sizeof(UM_RX_PACKET_DESC) * pDevice->MaxRxPacketDesc);

    if(Status != LM_STATUS_SUCCESS)   return LM_STATUS_FAILURE;
    pLmPacket = (PLM_PACKET_DESC) LL_GetHead(&pDevice->RxPacketFreeList);
    while(pLmPacket)
    {
        // allocate an Rx NDIS packet descriptor from packet pool just allocated
        NdisAllocatePacket(&Status, &pNdisPacket, NdisPacketPool);

        // allocate an Rx NDIS buffer descriptor from buffer pool just allocated
        if ( pDevice->ChipType == 0 )
        {
        	// ADM8511 986
        	NdisAllocateBuffer(&Status, &pNdisBuffer, NdisBufferPool, pLmPacket->Buffer, MAX_ETHERNET_PACKET_SIZE);
        }
        else
        {
        	//ADM8513
        	NdisAllocateBuffer(&Status, &pNdisBuffer, NdisBufferPool, pLmPacket->Buffer+2, MAX_ETHERNET_PACKET_SIZE);
        }	

        // set up header size
        NDIS_SET_PACKET_HEADER_SIZE(pNdisPacket, ETHERNET_PACKET_HEADER_SIZE);   // 14, how to get this ??? Different with HomePNA ???
        // links buffer descriptor to the head of the buffer-descriptor chain attached to a packet descriptor
        NdisChainBufferAtFront(pNdisPacket, pNdisBuffer);

        // initialize UM Rx packet
        pUmRxPacket->pNdisPacket = pNdisPacket;
        pUmRxPacket->pNdisBuffer = pNdisBuffer;

        pLmPacket->pUmPacket = pUmRxPacket;

        // initialize NDIS packet reserved area
        //((PNDIS_PACKET_RESERVED) pNdisPacket->MiniportReserved)->LmContext = pLmPacket;
        pUmRxPacket++;
        pLmPacket = (PLM_PACKET_DESC) LL_NextEntry(&pLmPacket->Link);
    } // while

    return LM_STATUS_SUCCESS;
} // MM_AllocateUmPackets

//******************************************************************************
// Description:
//    Copies a list of NDIS_BUFFERs.
//
// Returns:
//    Number of bytes copied.
//******************************************************************************
STATIC _inline ULONG MM_DoubleCopy( PNDIS_BUFFER   pNdisBuffer,
                                    PUCHAR         pDestBuffer )
{
    ULONG    CopySize;
    PUCHAR   pSrcBuffer;
    ULONG    SrcBufferSize;

    CopySize = 0;
    while(pNdisBuffer)
    {
        NdisQueryBuffer(pNdisBuffer, &pSrcBuffer, &SrcBufferSize);
        NdisMoveMemory(pDestBuffer, pSrcBuffer, SrcBufferSize);
        pDestBuffer += SrcBufferSize;
        CopySize += SrcBufferSize;
        NdisGetNextBuffer(pNdisBuffer, &pNdisBuffer);
    } // while

    return CopySize;
} // MM_DoubleCopy

//******************************************************************************
//******************************************************************************
LM_STATUS MM_SetupTxPacket( PLM_DEVICE_INFO_BLOCK   pDevice,
                            PLM_PACKET_DESC         pPacket )
{
    PNDIS_BUFFER         pNdisBuffer;
    PUM_TX_PACKET_DESC   pUmPacket;
    UINT                 Size;
 //   int			i;	

    #ifdef MM_DEBUG_PC
      DbgMessage(VERBOSE, (ADMTEXT("*** MM_SetupTxPacket ***\n")));
    #endif
    
    #ifdef MM_DEBUG_CE
      printf("*** MM_SetupTxPacket ***\n");
    #endif
    pUmPacket = (PUM_TX_PACKET_DESC) pPacket->pUmPacket;

    // Query the corresponding pNdisBuffer of the pNdisPacket
    NdisQueryPacket(pUmPacket->pNdisPacket, NULL, NULL, &pNdisBuffer, &Size);

    // Copy all buffer data to our buffer
    MM_DoubleCopy(pNdisBuffer, pPacket->Buffer + 2);
//    printf("[%d] [%d]\n",Size,(Size %16));
    if ( (Size % 16) == 13 )
      Size++;                            //fix askey's problem
 //   printf("Send Packet= ");
 //   for (i=0;i<Size ;i++)
 //   	printf("%02X ",pPacket->Buffer[2+i]);
 //   printf("@@@\n");
    // Set total size including device-need header
    pPacket->PacketSize = Size + 2;

    // Set the actual data size in the device-need header of our buffer
    *((USHORT *) pPacket->Buffer) = Size;

    return LM_STATUS_SUCCESS;
} /* MM_SetupTxPacket */

#ifndef NDISCE_MINIPORT
//******************************************************************************
// Description:
//    Get pointers of the device object.
//
// Returns:
//    None.
//******************************************************************************
VOID MM_GetDeviceObjects( PLM_DEVICE_INFO_BLOCK   pDevice,
                          PVOID *                 pPhysicalDeviceObject,
                          PVOID *                 pFunctionalDeviceObject,
                          PVOID *                 pNextDeviceObject )
{
    PUM_DEVICE_INFO_BLOCK   pUmDevice;
    
    #ifdef MM_DEBUG_PC
      DbgMessage(INFORM, (ADMTEXT("MM_GetDeviceObjects : NdisMGetDeviceProperty\n")));
    #endif
    pUmDevice = (PUM_DEVICE_INFO_BLOCK) pDevice;
    NdisMGetDeviceProperty( pUmDevice->MiniportHandle,
                            (PDEVICE_OBJECT *) pPhysicalDeviceObject,
                            (PDEVICE_OBJECT *) pFunctionalDeviceObject,
                            (PDEVICE_OBJECT *) pNextDeviceObject,
                            NULL,
                            NULL );
} // MM_GetDeviceObjects
#endif

//******************************************************************************
// Description:
//    Set timer to wait for UM_IndicateRxPackets routine & issue another Bulk transfer
//    1. Push received packet into tail of pDevice->RxPacketReceivedList
//    2. Pop a new packet from head of pDevice->RxPacketFreeList
//    3. Issue another Bulk IN transfer to wait Rx data with new packet buffer
//    4. Set the Timer Object to wait UM_IndicateRxPackets to indecate Rx packet
//
// Returns:
//    LM_STATUS_SUCCESS
//******************************************************************************
LM_STATUS MM_IndicateRxPacket( PLM_DEVICE_INFO_BLOCK   pDevice,
                               PLM_PACKET_DESC         pPacket )
{
    PUM_DEVICE_INFO_BLOCK   pUmDevice; 
    
    #ifdef MM_DEBUG_PC
      //DbgMessage(VERBOSE, (ADMTEXT("*** MM_IndicateRxPacket ***\n")));
    #endif
    
    #ifdef MM_DEBUG_CE
      printf("*** MM_IndicateRxPacket ***\n");
    #endif
//    printf("*** MM_IndicateRxPacket ***\n");
    pUmDevice = (PUM_DEVICE_INFO_BLOCK) pDevice;

//#ifdef NDISCE_MINIPORT
//    if( Unplugged )    return LM_STATUS_FAILURE;
//#endif

    #ifdef DEBCE // add - 05/31/05,bolo
    // mark - 09/05/05,bolo
    // printf("MIRP: rx pkt=0x%x\n",pPacket); // add - 05/26/06,bolo     
    NKDbgPrintfW(TEXT("MIRP: rx pkt=0x%x\n\r"),pPacket); // add - 09/05/05,bolo
    #endif

    NdisAcquireSpinLock(&pUmDevice->ReceiveQSpinLock);
    //_asm { cli};   // clear Interrupt flag
    LL_PushTail(&pDevice->RxPacketReceivedList, &pPacket->Link);

    UM_IndicateRxPackets(NULL, (PVOID)pUmDevice, NULL, NULL); // add - 06/06/05,bolo

    pPacket = (PLM_PACKET_DESC) LL_PopHead(&pDevice->RxPacketFreeList);
    //_asm { sti};   // set Interrupt flag
    #ifdef MM_DEBUG_PC
//      DbgMessage(VERBOSE, (ADMTEXT("%x h Rx Free packets, %x h Received packets\n"), LL_GetSize(&pDevice->RxPacketFreeList), LL_GetSize(&pDevice->RxPacketReceivedList)));
    #endif
    NdisReleaseSpinLock(&pUmDevice->ReceiveQSpinLock);
    #ifdef MM_DEBUG_PC
      //DbgMessage(FATAL, (ADMTEXT("Packet = 0x%x\n"), pPacket));
    #endif

#ifndef NDISCE_MINIPORT // unmark - 06/06/05,06/01/05,bolo
    // Set the Timer Object to wait UM_IndicateRxPackets to indecate Rx packet
    NdisMSetTimer(&pUmDevice->IndicateRxTimer, 0);
#endif
        
    if(pPacket)
    {
        // Issue another Bulk IN transfer to wait Rx data with new packet buffer  
        #ifdef DEBCE // add - 05/31/05,bolo
        // mark - 09/05/05,bolo
        // printf("MIRP: LQRP, pkt=0x%x,len=%d\n",pPacket,pPacket->PacketSize); // add - 05/26/06,bolo 
        NKDbgPrintfW(TEXT("MIRP: LQRP, pkt=0x%x,len=%d\n\r"),pPacket,pPacket->PacketSize); // add - 09/05/05,bolo    
        #endif
        /* mark - 06/01/05,bolo
        LM_QueueRxPacket(pDevice, pPacket);
        */
        if(LM_STATUS_SUCCESS != LM_QueueRxPacket(pDevice, pPacket)) { // add - 06/01/05,bolo
            #ifdef DEBERR // add - 09/05/05,bolo
            // DbgMessage(VERBOSE, (TEXT("MIRP: LQRP Failed !\n"))); // mark - 06/22/05,06/03/05,bolo
            // mark - 09/05/05,bolo
            // printf("MIRP: LQRP Failed !, RxFree=%d, RxPkt=%d\n", LL_GetSize(&pDevice->RxPacketFreeList), LL_GetSize(&pDevice->RxPacketReceivedList)); // add - 06/03/05,bolo
            NKDbgPrintfW(TEXT("MIRP: LQRP Failed !, RxFree=%d, RxPkt=%d\n\r"),LL_GetSize(&pDevice->RxPacketFreeList), LL_GetSize(&pDevice->RxPacketReceivedList)); // add - 09/05/05,bolo    
            #endif
            LL_PushTail(&pDevice->RxPacketFreeList, &pPacket->Link);
        }
        
    } // if
    else
    {
        #ifdef MM_DEBUG_PC
          DbgMessage(FATAL, (ADMTEXT("Pop a Packet fail from pDevice->RxPacketFreeList\n")));
        #endif

        #ifdef DEBERR // add - 09/05/05,bolo
        // DbgMessage(VERBOSE, (TEXT("MIRP: No free packet!"))); // add - 06/03/05,bolo
        // mark - 09/05/05,bolo
        // printf("MIRP: No free packet !, RxFree=%d, RxPkt=%d\n", LL_GetSize(&pDevice->RxPacketFreeList), LL_GetSize(&pDevice->RxPacketReceivedList)); // add - 06/03/05,bolo
        NKDbgPrintfW(TEXT("MIRP: No free packet !, RxFree=%d, RxPkt=%d\n\r"),LL_GetSize(&pDevice->RxPacketFreeList), LL_GetSize(&pDevice->RxPacketReceivedList)); // add - 09/05/05,bolo    
        #endif
        // printf("MIRP: NOPKT\n"); // add - 05/26/06,bolo
    }
//    printf("*** Set Timer ***\n");

    /* mark - 06/06/05,bolo
    NdisMSetTimer(&pUmDevice->IndicateRxTimer, 1);
    */
    return LM_STATUS_SUCCESS;
} // MM_IndicateRxPacket

//******************************************************************************
// Description:
//    This routine indicates all the transmitted packets.
//
// Returns:
//    LM_STATUS_SUCCESS
//******************************************************************************
LM_STATUS MM_IndicateTxPacket( PLM_DEVICE_INFO_BLOCK   pDevice,
                               PLM_PACKET_DESC         pPacket )
{
    PUM_DEVICE_INFO_BLOCK   pUmDevice;

    #ifdef MM_DEBUG_PC
      DbgMessage(VERBOSE, (ADMTEXT("*** MM_IndicateTxPacket ***\n")));
    #endif
    
    #ifdef MM_DEBUG_CE
      printf("*** MM_IndicateTxPacket ***\n");
    #endif
    
    pUmDevice = (PUM_DEVICE_INFO_BLOCK) pDevice;


    // Put the packet descriptor back in the free list.
    NdisAcquireSpinLock(&pUmDevice->TransmitQSpinLock);
    LL_PushTail(&pDevice->TxPacketFreeList, &pPacket->Link);
    pUmDevice->TxDescLeft ++;
    NdisReleaseSpinLock(&pUmDevice->TransmitQSpinLock);
#ifdef NDISCE_MINIPORT
    if( Unplugged )    return LM_STATUS_FAILURE;
#endif

    if(pUmDevice->OutOfSendResources)
    {
        #ifdef MM_DEBUG_PC
          DbgMessage(FATAL,(ADMTEXT("Free Tx Resource Left = %d\n"), pUmDevice->TxDescLeft));
        #endif

        #ifdef DEBERR // add - 09/05/05,bolo
        // mark - 09/05/05,bolo
        // printf("MITP: Free TxResource=%d\n", pUmDevice->TxDescLeft); // add - 06/03/05,bolo
        NKDbgPrintfW(TEXT("MITP: Free TxResource=%d\n\r"),pUmDevice->TxDescLeft); // add - 09/05/05,bolo    
        #endif

        if ( pUmDevice->TxDescLeft ==  MAX_TX_PACKET_DESCRIPTORS )
        {
            NdisMSendResourcesAvailable(pUmDevice->MiniportHandle);
            pUmDevice->OutOfSendResources = FALSE;
        }
    } // if

    return LM_STATUS_SUCCESS;
} // MM_IndicateTxPacket

//******************************************************************************
// Description:
//    Indicates link status to the OS.
//
// Parameters:
//    PLM_DEVICE_INFO_BLOCK pDevice
//    LM_STATUS Status
//
// Parameters:
//    LM_STATUS_SUCCESS
//******************************************************************************
LM_STATUS MM_IndicateStatus( PLM_DEVICE_INFO_BLOCK   pDevice,
                             LM_STATUS               Status )
{
    PUM_DEVICE_INFO_BLOCK pUmDevice;

    #ifdef MM_DEBUG_PC
      DbgMessage(INFORM, (ADMTEXT("*** MM_IndicateLinkStatus ***\n")));
    #endif

    pUmDevice = (PUM_DEVICE_INFO_BLOCK) pDevice;

    if(Status == LM_STATUS_LINK_DOWN) {

       if ( pUmDevice->InitCompleted == TRUE) {
        NdisMIndicateStatus(pUmDevice->MiniportHandle,
            NDIS_STATUS_MEDIA_DISCONNECT, NULL, 0);
        }
    } else {
       if ( pUmDevice->InitCompleted == TRUE) {
        NdisMIndicateStatus(pUmDevice->MiniportHandle,
            NDIS_STATUS_MEDIA_CONNECT, NULL, 0);
        }
           }  // end if
    NdisMIndicateStatusComplete(pUmDevice->MiniportHandle);
    return LM_STATUS_SUCCESS;
} // LM_IndicateStatus

#ifdef NDISCE_MINIPORT
//2004.08.31 add by Fujutsu ---->>>
//******************************************************************************
// Descriptions:
//    This routine reads configuration values from the registry.
//    On Windows CE, we only read "MediaType".
//
// Returns:
//    LM_STATUS_SUCCESS
//    LM_STATUS_FAILURE
//******************************************************************************
LM_STATUS MM_GetConfig( PLM_DEVICE_INFO_BLOCK pDevice )
{
    PUM_DEVICE_INFO_BLOCK           pUmDevice;
    NDIS_STATUS                     Status;
    NDIS_HANDLE                     ConfigHandle = NULL;
    PNDIS_CONFIGURATION_PARAMETER	ConfigValue;
	NDIS_STRING						nsMediaType = NDIS_STRING_CONST("MediaType");
	NDIS_STRING						nsMediaSel  = NDIS_STRING_CONST("MediaSel"); // add - 08/19/05,bolo
    LM_STATUS						lmRet;
	
    pUmDevice = (PUM_DEVICE_INFO_BLOCK) pDevice;

	NdisOpenConfiguration(&Status, &ConfigHandle, pUmDevice->ConfigHandle);
    if(Status != NDIS_STATUS_SUCCESS){
        #ifdef MM_DEBUG_CE
          DbgMessage(FATAL, (ADMTEXT("NdisOpenConfiguration failed %p\r\n"), ConfigHandle));
        #endif
		goto lExit;
    }
	
    NdisReadConfiguration( &Status, &ConfigValue, ConfigHandle, &nsMediaType, NdisParameterInteger );
	
lExit:
	if(	Status == NDIS_STATUS_SUCCESS ){
		//a value will be checked in LM_SetMediaType(), just set as it is,
		pDevice->RequestedMediaType = ConfigValue->ParameterData.IntegerData;
		lmRet = LM_STATUS_SUCCESS;
	}else{
		//Do nothing!
		//Invalid value will be changed into MEDIA_TYPE_AUTO_DETECTION in LM_SetMediaType().
		lmRet = LM_STATUS_FAILURE;
	}

    // add - 08/19/05,bolo
    NdisReadConfiguration( &Status, &ConfigValue, ConfigHandle, &nsMediaSel, NdisParameterInteger );

	if(	Status == NDIS_STATUS_SUCCESS ){
		//a value will be checked in LM_AutoSelectMedia(), just set as it is,
		pDevice->RequestedSelectMedia = ConfigValue->ParameterData.IntegerData;
		lmRet = LM_STATUS_SUCCESS;
	}else{
		//Do nothing!
		//Invalid value will be changed into MEDIA_TYPE_AUTO_DETECTION in LM_SetMediaType().
		lmRet = LM_STATUS_FAILURE;
	}

	#ifdef MM_DEBUG_CE
	DbgMessage(FATAL, (ADMTEXT("RequestedMediaType is %d\r\n"), pDevice->RequestedMediaType ));
	#endif

    #ifdef DEBCEINIT // add - 08/19/05,bolo
        // unmark - 09/05/05,bolo
	    NKDbgPrintfW(TEXT("MM_GetConfig: RequestedMediaType=%d,RequestedSelectMedia=%d\n\r"),pDevice->RequestedMediaType, pDevice->RequestedSelectMedia);
    #endif

    if(ConfigHandle){
	    NdisCloseConfiguration(ConfigHandle);
	}
    return lmRet;
} // MM_GetConfig
// 2004.08.31 add <<<---
#else
//******************************************************************************
// Descriptions:
//    This routine reads configuration values from the registry.
//
// Returns:
//    LM_STATUS_SUCCESS
//    LM_STATUS_FAILURE
//******************************************************************************
LM_STATUS MM_GetConfig( PLM_DEVICE_INFO_BLOCK pDevice )
{
    typedef struct
    {
        NDIS_STRING   Name;       // registry item name
        PULONG        pValue;     // location to store the value
    } REGISTRY_ENTRY, *PREGISTRY_ENTRY;

    REGISTRY_ENTRY RegistryEntries[] =
    {
        { NDIS_STRING_CONST("TxPacketDesc"),    &pDevice->MaxTxPacketDesc },
        { NDIS_STRING_CONST("RxPacketDesc"),    &pDevice->MaxRxPacketDesc },
        { NDIS_STRING_CONST("MediaType"),       &pDevice->RequestedMediaType },
        { NDIS_STRING_CONST("MediaSel"),        &pDevice->RequestedSelectMedia },
        { NDIS_STRING_CONST("MagicPkt"),        &pDevice->RequestedMagicPacket },
        { NDIS_STRING_CONST("LinkWakeup"),      &pDevice->RequestedLinkWakeup },
        { NDIS_STRING_CONST("FlowControl"),     &pDevice->RequestedFlowControl},
        { NDIS_STRING_CONST("FlowControl_Tx"),  &pDevice->RequestedFlowControlTx},
        { NDIS_STRING_CONST("FlowControl_Rx"),  &pDevice->RequestedFlowControlRx},
        { NDIS_STRING_CONST(""),                NULL } // must be the last item
    };

    CHAR * RegistryEntryString[] =
    {
        "TxPacketDesc",
        "RxPacketDesc",
        "MediaType",
        "MediaSel",
        "MagicPkt",
        "LinkWakeup",
        "FlowControl",
        "FlowControl_Tx",
        "FlowControl_Rx",
        ""
    };

    NDIS_STATUS                     Status;
    NDIS_HANDLE                     ConfigHandle;
    PNDIS_CONFIGURATION_PARAMETER   ConfigValue;
    PREGISTRY_ENTRY                 pEntry;
    CHAR *                          pString;
    PUM_DEVICE_INFO_BLOCK           pUmDevice;
    PUCHAR                          NetAddress;
    ULONG                           AddrLength;

    #ifdef MM_DEBUG_PC
      DbgMessage(INFORM, (ADMTEXT("*** MM_GetConfig ***\n")));
    #endif
    pUmDevice = (PUM_DEVICE_INFO_BLOCK) pDevice;

    // Open the configuration info found in the registry.
    NdisOpenConfiguration(&Status, &ConfigHandle, pUmDevice->ConfigHandle);
    if(Status != NDIS_STATUS_SUCCESS)
    {
        #ifdef MM_DEBUG_PC
          DbgMessage(FATAL, (ADMTEXT("NdisOpenConfiguration failed\n")));
        #endif
        return LM_STATUS_FAILURE;
    } // if

    // Read registry entries
    pString = RegistryEntryString[0];
    pEntry = RegistryEntries;
    while(pEntry->pValue)
    {
        NdisReadConfiguration( &Status,
                               &ConfigValue,
                               ConfigHandle,
                               &pEntry->Name,
                               NdisParameterInteger );
        if(Status == NDIS_STATUS_SUCCESS)
        {
            *pEntry->pValue = ConfigValue->ParameterData.IntegerData;
            #ifdef MM_DEBUG_PC
              DbgMessage(INFORM, (ADMTEXT("%s = 0x%x\n"), pString, *pEntry->pValue));
            #endif
        }
        else
        {
            #ifdef MM_DEBUG_PC
              DbgMessage(INFORM, (ADMTEXT("Default %s = 0x%x\n"), RegistryEntryString, *pEntry->pValue));
            #endif
        }
        pEntry++;
        pString++;
    } // while

    // Read net address from registry.
    NdisReadNetworkAddress(&Status, &NetAddress, &AddrLength, ConfigHandle);
    if(Status == NDIS_STATUS_SUCCESS && AddrLength == ETH_LENGTH_OF_ADDRESS)   // 6
    {
        NdisMoveMemory(pDevice->NodeAddress, NetAddress, 6);
        #ifdef MM_DEBUG_PC
          DbgMessage(INFORM, (ADMTEXT("Node Address from Registry: %02x %02x %02x %02x %02x %02x\n"),
                            pDevice->NodeAddress[0], pDevice->NodeAddress[1],
                             pDevice->NodeAddress[2], pDevice->NodeAddress[3],
                              pDevice->NodeAddress[4], pDevice->NodeAddress[5]));
       #endif
    }
    else
    {
        #ifdef MM_DEBUG_PC
          DbgMessage(INFORM, (ADMTEXT("Cannot read network address from registry\n")));
        #endif

    // We are done with the Registry.
    NdisCloseConfiguration(ConfigHandle);
    return LM_STATUS_SUCCESS;
} // MM_GetConfig
#endif

//VOID MM_Wait(ULONG usec) {
//        NdisStallExecution(usec);
//}

VOID MM_InitWait(ULONG usec)
{
    NdisMSleep(usec);
}
VOID MM_FreeResources(PLM_DEVICE_INFO_BLOCK   pDevice)
{
    PUM_DEVICE_INFO_BLOCK   pUmDevice;
    PUM_RESOURCE_DESC pResource;
    int             i, NumOfFree;
    PLM_PACKET_DESC         pLmPacket;
    PUM_RX_PACKET_DESC      pUmRxPacket;

    pUmDevice = (PUM_DEVICE_INFO_BLOCK) pDevice;
    // get a resource descriptor
    NumOfFree = pUmDevice->ResourceNumber;                  //lic (6.3.99)
    
    #ifdef MM_DEBUG_PC
      DbgMessage(INFORM, (ADMTEXT("*** UM_FreeResources ***\n")));
      DbgMessage(INFORM, (ADMTEXT("Number of free = %d\n"), NumOfFree));
    #endif
    
    #ifdef MM_DEBUG_CE
      printf("*****  MM_FessResources *******\n");
    #endif 
   

    for(i=0;i<NumOfFree;i++) {
    	        #ifdef MM_DEBUG_PC
                  DbgMessage(FATAL, (ADMTEXT("Pop a Resource from List, i = %d\n"), i));
                #endif
        pResource = (PUM_RESOURCE_DESC) LL_PopHead(&pUmDevice->ResourceList);
                #ifdef MM_DEBUG_PC
                  DbgMessage(FATAL, (ADMTEXT("Resourceptr=%u\n"),pResource));
                #endif
        if(pResource == NULL) {
            break;
        } // if

        switch(pResource->ResourceType) {
            case RESOURCE_TYPE_UNASSIGNED:
                #ifdef MM_DEBUG_PC
                  DbgMessage(FATAL, (ADMTEXT("Unassigned resource type\n")));
                #endif              
                break;

            case RESOURCE_TYPE_MEMORY:
                #ifdef MM_DEBUG_PC
                  DbgMessage(INFORM, (ADMTEXT("Freeing memory\n")));               
                #endif
/*
#ifdef NDISCE_MINIPORT
                UnlockPages( pResource->Memory.pMemoryBlock, pResource->Memory.BlockSize );
#endif
*/
                #ifdef MM_DEBUG_CE
                 printf("[%08lx]\n",pResource->Memory.pMemoryBlock);
                #endif
                NdisFreeMemory(pResource->Memory.pMemoryBlock,
                    pResource->Memory.BlockSize, 0);
                #ifdef MM_DEBUG_PC
                   DbgMessage(INFORM, (ADMTEXT("Freeing memory done\n")));
                #endif    
                break;

            case RESOURCE_TYPE_IO_SPACE:
                #ifdef MM_DEBUG_PC
                  DbgMessage(INFORM, (ADMTEXT("Freeing IO Space\n")));
                #endif
                               
                NdisMDeregisterIoPortRange(pResource->IoSpace.MiniportHandle,
                    pResource->IoSpace.IoBase,
                    pResource->IoSpace.IoSpaceSize,
                    (PVOID) pResource->IoSpace.MappedIoBase);
                break;

            case RESOURCE_TYPE_INTERRUPT:
                #ifdef MM_DEBUG_PC
                  DbgMessage(INFORM, (ADMTEXT("Freeing interrupt\n")));
                #endif
               
                NdisMDeregisterInterrupt(&pResource->NdisInterrupt);
                break;

            case RESOURCE_TYPE_MAP_REGISTERS:
                #ifdef MM_DEBUG_PC
                  DbgMessage(INFORM, (ADMTEXT("Freeing map registers\n")));               
                #endif
                NdisMFreeMapRegisters(pUmDevice->MiniportHandle);
                break;

            case RESOURCE_TYPE_PACKET_POOL:
                #ifdef MM_DEBUG_PC
                  DbgMessage(INFORM, (ADMTEXT("Freeing packet pool\n")));
                #endif
				pLmPacket = (PLM_PACKET_DESC) LL_GetHead(&pDevice->RxPacketFreeList);
				while (pLmPacket)
				{
			        pUmRxPacket = pLmPacket->pUmPacket;
					NdisFreePacket(pUmRxPacket->pNdisPacket);
			        pLmPacket = (PLM_PACKET_DESC) LL_NextEntry(&pLmPacket->Link);
				}
                NdisFreePacketPool(pResource->NdisPacketPool);
                break;

            case RESOURCE_TYPE_BUFFER_POOL:
                #ifdef MM_DEBUG_PC
                  DbgMessage(INFORM, (ADMTEXT("Freeing buffer pool\n")));
                #endif               
				pLmPacket = (PLM_PACKET_DESC) LL_GetHead(&pDevice->RxPacketFreeList);
				while (pLmPacket)
				{
			        pUmRxPacket = pLmPacket->pUmPacket;
					NdisFreeBuffer(pUmRxPacket->pNdisBuffer);
			        pLmPacket = (PLM_PACKET_DESC) LL_NextEntry(&pLmPacket->Link);
				}
                NdisFreeBufferPool(pResource->NdisBufferPool);
                break;
                           
            default:
                #ifdef MM_DEBUG_PC
                  DbgMessage(FATAL, (ADMTEXT("Unknown resource descriptor\n")));
                #endif                
                break;
        } // switch
    } // for(; ;)
        #ifdef MM_DEBUG_PC
          DbgMessage(INFORM, (ADMTEXT("*** UM_FreeResources End ***\n")));
        #endif
        
        #ifdef MM_DEBUG_CE
         printf("*** MM_FreeResources End ***\n");
        #endif
}

