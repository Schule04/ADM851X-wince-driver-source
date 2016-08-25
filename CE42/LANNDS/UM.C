//Driver hisotry
// 1.09 modify by jack for slove power off/on and plug/unplug
// please search "2003_0411"

// 1.09 2003_0415  jackl mask 2003_0415 Ignore receive error packet
// Please "2003_0415"

//******************************************************************************
// Module Description:   UM.C
//******************************************************************************
#include "um.h"
#include "mm.h"

//#define UM_DEBUG_PC	1
//******************************************************************************
// Constants
//******************************************************************************
#define DRIVER_NDIS_MAJOR_VERSION   4
#define DRIVER_NDIS_MINOR_VERSION   0

//******************************************************************************
// Global variables
//******************************************************************************
#ifdef NDISCE_MINIPORT
PNDIS_MINIPORT_BLOCK    pMiniportBlock;
NDIS_HANDLE             NdisWrapper;
BOOL                    NdisDriverLoaded = FALSE;
PLM_DEVICE_INFO_BLOCK   pDeviceForHeader = NULL;
DWORD                   ReceiveMaskForHeader = 0;

#ifdef SUPPORT_MEDIASENSE // add - 07/20/05,bolo for MSC
NDIS_MINIPORT_TIMER 	LinkStatusTimer;
#endif

extern BOOL             Unplugged;
#else
extern VOID LM_HookAddDevice( PDRIVER_OBJECT );
#endif
gDuringInitialize = FALSE;


//******************************************************************************
// Function prototypes.
//******************************************************************************

NTSTATUS DriverEntry( IN  PDRIVER_OBJECT    DriverObject,
                      IN  PUNICODE_STRING   RegistryPath );
#pragma NDIS_INIT_FUNCTION(DriverEntry)

NDIS_STATUS UM_Initialize( OUT PNDIS_STATUS  OpenErrorStatus,
                           OUT PUINT         SelectedMediumIndex,
                           IN  PNDIS_MEDIUM  MediumArray,
                           IN  UINT          MediumArraySize,
                           IN  NDIS_HANDLE   MiniportAdapterHandle,
                           IN  NDIS_HANDLE   ConfigurationHandle );
#pragma NDIS_PAGEABLE_FUNCTION(UM_Initialize)

STATIC VOID UM_FreeResources( PUM_DEVICE_INFO_BLOCK pUmDevice );
#pragma NDIS_PAGEABLE_FUNCTION(UM_FreeResources)

VOID UM_Halt( IN  PVOID MiniportAdapterContext );
#pragma NDIS_PAGEABLE_FUNCTION(UM_Halt)

#ifdef SUPPORT_MEDIASENSE // add - 07/20/05,bolo for MSC
VOID UM_CheckLinkStatus( IN  PVOID   SystemSpecific1,
                           IN  PVOID   FunctionContext,
                           IN  PVOID   SystemSpecific2,
                           IN  PVOID   SystemSpecific3 );
#endif

VOID UM_IndicateRxPackets( IN  PVOID   SystemSpecific1,
                           IN  PVOID   FunctionContext,
                           IN  PVOID   SystemSpecific2,
                           IN  PVOID   SystemSpecific3 );

//******************************************************************************
// Description:
//    Frees up the resources in the resource list.
//
// Returns:
//    None.
//******************************************************************************
STATIC VOID UM_FreeResources( PUM_DEVICE_INFO_BLOCK pUmDevice )
{
    PUM_RESOURCE_DESC pResource;
        int             i, NumOfFree;
    PLM_PACKET_DESC         pLmPacket;
    PUM_RX_PACKET_DESC      pUmRxPacket;

        NumOfFree = pUmDevice->ResourceNumber;                  //lic (6.3.99)
    #ifdef UM_DEBUG_PC
      DbgMessage(INFORM, (ADMTEXT("*** UM_FreeResources ***\n")));
      DbgMessage(INFORM, (ADMTEXT("Number of free = %d\n"), NumOfFree));
    #endif

    for(i=0;i<NumOfFree;i++) {
    	     #ifdef UM_DEBUG_PC
                DbgMessage(FATAL, (ADMTEXT("Pop a Resource from List, i = %d\n"), i));
             #endif
        pResource = (PUM_RESOURCE_DESC) LL_PopHead(&pUmDevice->ResourceList);
             #ifdef UM_DEBUG_PC
                DbgMessage(FATAL, (ADMTEXT("Resourceptr=%u\n"),pResource));
             #endif
        if(pResource == NULL) {
            break;
        } // if

        switch(pResource->ResourceType) {
            case RESOURCE_TYPE_UNASSIGNED:
                #ifdef UM_DEBUG_PC
                  DbgMessage(FATAL, (ADMTEXT("Unassigned resource type\n")));
                #endif
                //DbgBreak();
                break;

            case RESOURCE_TYPE_MEMORY:
                #ifdef UM_DEBUG_PC
                  DbgMessage(INFORM, (ADMTEXT("Freeing memory\n")));
                #endif
/*
#ifdef NDISCE_MINIPORT
                UnlockPages( pResource->Memory.pMemoryBlock, pResource->Memory.BlockSize );
#endif
*/
                NdisFreeMemory(pResource->Memory.pMemoryBlock,
                    pResource->Memory.BlockSize, 0);

                #ifdef UM_DEBUG_PC
                  DbgMessage(INFORM, (ADMTEXT("Freeing memory done\n")));
                #endif
                break;

            case RESOURCE_TYPE_IO_SPACE:
                #ifdef UM_DEBUG_PC
                  DbgMessage(INFORM, (ADMTEXT("Freeing IO Space\n")));
                #endif
                NdisMDeregisterIoPortRange(pResource->IoSpace.MiniportHandle,
                    pResource->IoSpace.IoBase,
                    pResource->IoSpace.IoSpaceSize,
                    (PVOID) pResource->IoSpace.MappedIoBase);
                break;

            case RESOURCE_TYPE_INTERRUPT:
                #ifdef UM_DEBUG_PC
                  DbgMessage(INFORM, (ADMTEXT("Freeing interrupt\n")));
                #endif
                NdisMDeregisterInterrupt(&pResource->NdisInterrupt);
                break;

            case RESOURCE_TYPE_MAP_REGISTERS:
                #ifdef UM_DEBUG_PC
                  DbgMessage(INFORM, (ADMTEXT("Freeing map registers\n")));
                #endif
                NdisMFreeMapRegisters(pUmDevice->MiniportHandle);
                break;

            case RESOURCE_TYPE_PACKET_POOL:
                #ifdef UM_DEBUG_PC
                  DbgMessage(INFORM, (ADMTEXT("Freeing packet pool\n")));
                #endif
				pLmPacket = (PLM_PACKET_DESC) LL_GetHead(&pUmDevice->LmDevice.RxPacketFreeList);
				while (pLmPacket)
				{
			        pUmRxPacket = pLmPacket->pUmPacket;
					NdisFreePacket(pUmRxPacket->pNdisPacket);
			        pLmPacket = (PLM_PACKET_DESC) LL_NextEntry(&pLmPacket->Link);
				}
                NdisFreePacketPool(pResource->NdisPacketPool);
                break;

            case RESOURCE_TYPE_BUFFER_POOL:
                #ifdef UM_DEBUG_PC
                  DbgMessage(INFORM, (ADMTEXT("Freeing buffer pool\n")));
                #endif
				pLmPacket = (PLM_PACKET_DESC) LL_GetHead(&pUmDevice->LmDevice.RxPacketFreeList);
				while (pLmPacket)
				{
			        pUmRxPacket = pLmPacket->pUmPacket;
					NdisFreeBuffer(pUmRxPacket->pNdisBuffer);
			        pLmPacket = (PLM_PACKET_DESC) LL_NextEntry(&pLmPacket->Link);
				}
                NdisFreeBufferPool(pResource->NdisBufferPool);
                break;

            default:
                #ifdef UM_DEBUG_PC
                  DbgMessage(FATAL, (ADMTEXT("Unknown resource descriptor\n")));
                #endif
                break;
        } // switch
    } // for(; ;)

//	// Free the pUmDevice itself
//    NdisFreeMemory(pUmDevice, sizeof(UM_DEVICE_INFO_BLOCK), 0);

        #ifdef UM_DEBUG_PC
         DbgMessage(INFORM, (ADMTEXT("*** UM_FreeResources End ***\n")));
        #endif
} // UM_FreeResources

//******************************************************************************
// Description:
//    This routine allocates a resource descriptor and add it to the resource
//    list.
//
// Returns:
//    PUM_RESOURCE_DESC
//******************************************************************************
PUM_RESOURCE_DESC UM_GetResourceDesc( PUM_DEVICE_INFO_BLOCK pUmDevice )
{
    NDIS_PHYSICAL_ADDRESS   MaxAddress = NDIS_PHYSICAL_ADDRESS_CONST(-1, -1);
    PUM_RESOURCE_DESC       pResource;
    NDIS_STATUS             Status;
#ifndef NDISCE_MINIPORT
    static ULONG            Tag = 'KH00';
#endif
    ULONG                   MemoryBlockSize;
    ULONG                   Temp;

    #ifdef UM_DEBUG_PC
      DbgMessage(VERBOSE, (ADMTEXT("*** UM_GetResourceDesc ***\n")));
    #endif

    if(LL_Empty(&pUmDevice->FreeResourceDescriptors))
    {
        MemoryBlockSize = sizeof(UM_RESOURCE_DESC) * 15;

        // allocate memory for 15 descriptors
#ifndef NDISCE_MINIPORT
        DbgMessage(VERBOSE, (ADMTEXT("Tag = %s\n"), (PCHAR) &Tag));
        Status = NdisAllocateMemoryWithTag(&pResource, MemoryBlockSize, Tag);
        Tag++;
#else
        Status = NdisAllocateMemory( &pResource, MemoryBlockSize, 0, MaxAddress );
#endif


        if(Status != NDIS_STATUS_SUCCESS)
        {
            #ifdef UM_DEBUG_PC
              DbgMessage(FATAL, (ADMTEXT("Allocate memory failed\n")));
            #endif
            return NULL;
        } // if
        NdisZeroMemory(pResource, MemoryBlockSize);

        // add descriptors to the free descriptor list.
        for(Temp = 0; Temp < 15; Temp++)
        {
            LL_PushTail(&pUmDevice->FreeResourceDescriptors, &pResource[Temp].Link);
        } // for

        // get a free descriptor and put it in the resource list.
        pResource = (PUM_RESOURCE_DESC)LL_PopHead(&pUmDevice->FreeResourceDescriptors);

        // initialize the descriptors for the memory we just allocated.
/*
#ifdef NDISCE_MINIPORT
        LockPages( pResource, MemoryBlockSize, NULL, LOCKFLAG_WRITE );
#endif
*/
        pResource->ResourceType = RESOURCE_TYPE_MEMORY;
        pResource->Memory.BlockSize = MemoryBlockSize;
        pResource->Memory.pMemoryBlock = (PUCHAR) pResource;

        // add the descriptor to the list.
        LL_PushHead(&pUmDevice->ResourceList, &pResource->Link);
        pUmDevice->ResourceNumber++;
    } // if

    // get a free descriptor and put it in the resource list.
    pResource = (PUM_RESOURCE_DESC)LL_PopHead(&pUmDevice->FreeResourceDescriptors);
    LL_PushHead(&pUmDevice->ResourceList, &pResource->Link);
    pUmDevice->ResourceNumber++;                    // lic (6.3.99) let it add one

    return pResource;
} // UM_GetResourceDesc

//******************************************************************************
// Description:
//    Shuts down the h/w.
//
// Returns:
//    None.
//******************************************************************************
VOID UM_Shutdown( IN PVOID MiniportAdapterContext )
{
    PUM_DEVICE_INFO_BLOCK   pUmDevice;
    BOOLEAN                 Cancelled;


	#ifdef UM_DEBUG_PC
	DbgMessage(INFORM, (ADMTEXT("*** UM_Shutdown Start ***\n")));
	#endif
    #ifdef UM_DEBUG_CE
      printf("*** UM_Shutdown Start ***\n");
    #endif

    pUmDevice = (PUM_DEVICE_INFO_BLOCK) MiniportAdapterContext;

//    if( !pUmDevice->IndicateRxTimer.MiniportTimerFunction )  // change by Y.M 01/04/02
    if( pUmDevice->IndicateRxTimer.MiniportTimerFunction )
    {
        NdisMCancelTimer(&pUmDevice->IndicateRxTimer, &Cancelled);
        if( Cancelled )
            NdisZeroMemory( (LPVOID)&pUmDevice->IndicateRxTimer, sizeof( NDIS_MINIPORT_TIMER ) );
    }

#ifdef SUPPORT_MEDIASENSE // add - 07/20/05,bolo for MSC
	// 2004/08/24, ccwong, add MediaSense support
    NdisMCancelTimer(&LinkStatusTimer, &Cancelled);
	if( !Cancelled ){
		Sleep(UM_CHECK_LINK_INTERVAL + 1000);
        NdisMCancelTimer(&LinkStatusTimer, &Cancelled);
	}
	// End of ccwong
#endif

    LM_Shutdown(&pUmDevice->LmDevice);

    //MM_InitWait( 5000000 );

} // UM_Shutdown

//******************************************************************************
// Description:
//    This is the Initialize routine.    The system calls this routine from
//    driver entry to add support for a particular adapter.  This routine
//    extracts configuration information from the configuration data base and
//    registers the adapter with NDIS.
//
// Returns:
//    NDIS_STATUS_SUCCESS - Adapter was successfully added.
//    NDIS_STATUS_FAILURE - Adapter was not added, also MAC deregistered.
//    NDIS_STATUS_UNSUPPORTED_MEDIA - Adapter was not added.
//    NDIS_STATUS_RESOURCES - Adapter was not added due to no resources.
//******************************************************************************
NDIS_STATUS UM_Initialize( OUT PNDIS_STATUS   OpenErrorStatus,
                           OUT PUINT          SelectedMediumIndex,
                           IN  PNDIS_MEDIUM   MediumArray,
                           IN  UINT           MediumArraySize,
                           IN  NDIS_HANDLE    MiniportAdapterHandle,
                           IN  NDIS_HANDLE    ConfigurationHandle )
{
    NDIS_PHYSICAL_ADDRESS   MaxAddress = NDIS_PHYSICAL_ADDRESS_CONST(-1, -1);
    NDIS_STATUS             Status;
    PUM_DEVICE_INFO_BLOCK   pUmDevice;
    PLM_DEVICE_INFO_BLOCK   pDevice;
    PUM_RESOURCE_DESC       pResource;
    UINT                    MediaType;

	BOOLEAN	Cancelled;
	gDuringInitialize = TRUE;


#ifdef NDISCE_MINIPORT
    pMiniportBlock = (PNDIS_MINIPORT_BLOCK)MiniportAdapterHandle;
#endif

 	#ifdef UM_DEBUG_PC
 	DbgMessage(INFORM, (ADMTEXT("*** UM_Initialize Start ***\n")));
 	#endif

    #ifdef UM_DEBUG_CE
      printf("*** UM_Initialize Start ***\n");
    #endif
    // Search for 802.3 media type
    MediaType = 0;
    for(; ;)
    {
        if(MediaType == MediumArraySize)
        {
            #ifdef UM_DEBUG_PC
              DbgMessage(FATAL, (ADMTEXT("Unsupported media type\n")));
            #endif
			Status = NDIS_STATUS_UNSUPPORTED_MEDIA;
			goto lFREE_RESOURCES;
        }
        if(MediumArray[MediaType] == NdisMedium802_3)
        {
            *SelectedMediumIndex = MediaType;
            break;
        }
        MediaType++;
    } // for


    // allocate memory for the logical device and its descriptor.
#ifndef NDISCE_MINIPORT
    Status = NdisAllocateMemoryWithTag(&pUmDevice, sizeof(UM_DEVICE_INFO_BLOCK), 'KHaa');
#else
        Status = NdisAllocateMemory( &pUmDevice, sizeof(UM_DEVICE_INFO_BLOCK), 0, MaxAddress );
#endif

    if(Status != NDIS_STATUS_SUCCESS)
    {
        #ifdef UM_DEBUG_PC
          DbgMessage(FATAL, (ADMTEXT("Cannot Allocate memory for device structure\n")));
        #endif
		goto lFREE_RESOURCES;
    } // if
    NdisZeroMemory(pUmDevice, sizeof(UM_DEVICE_INFO_BLOCK));
    pUmDevice->InitCompleted = FALSE;
    // setup pointer to device info blocks.
    pDevice = &pUmDevice->LmDevice;

    // This flag is used to prevent status indication during initialization. NDIS will crash.
    pDevice->AdapterStatus = LM_STATUS_UNKNOWN;

    // initialize the descriptor
    pResource = UM_GetResourceDesc(pUmDevice);


/*
#ifdef NDISCE_MINIPORT
    LockPages( pUmDevice, sizeof(UM_DEVICE_INFO_BLOCK), NULL, LOCKFLAG_WRITE );
#endif
*/
    pResource->ResourceType = RESOURCE_TYPE_MEMORY;
    pResource->Memory.BlockSize = sizeof(UM_DEVICE_INFO_BLOCK);
    pResource->Memory.pMemoryBlock = (PUCHAR) pUmDevice;

    // save NDIS handles
    pUmDevice->MiniportHandle = MiniportAdapterHandle;

    pUmDevice->ConfigHandle = ConfigurationHandle;

    // Set the attributes for NDIS.
    NdisMSetAttributesEx( MiniportAdapterHandle,
                          (NDIS_HANDLE) pUmDevice,
                          0,
                          NDIS_ATTRIBUTE_IGNORE_PACKET_TIMEOUT |
                           NDIS_ATTRIBUTE_IGNORE_REQUEST_TIMEOUT |
#ifndef NDISCE_MINIPORT
                            NDIS_ATTRIBUTE_DESERIALIZE |
#endif
                             NDIS_ATTRIBUTE_INTERMEDIATE_DRIVER,
                          0);


    NdisAllocateSpinLock(&pUmDevice->TransmitQSpinLock);
    NdisAllocateSpinLock(&pUmDevice->ReceiveQSpinLock);
    //NdisAllocateSpinLock(&pUmDevice->IrpSpinLock);


    // Initialize the adapter
    Status = LM_InitializeAdapter(pDevice);


    if(Status != LM_STATUS_SUCCESS)
    {

		Status = NDIS_STATUS_RESOURCES;
		goto lFREE_RESOURCES;
    } // if

    // Register our shutdown handler.
    NdisMRegisterAdapterShutdownHandler(pUmDevice->MiniportHandle, pUmDevice, UM_Shutdown);


    // Set up a timer to poll if any data received
    NdisMInitializeTimer(&pUmDevice->IndicateRxTimer, pUmDevice->MiniportHandle, UM_IndicateRxPackets, pUmDevice);


#ifdef NDISCE_MINIPORT
    //NdisMSetPeriodicTimer(&pUmDevice->IndicateRxTimer, 10);
    /* mark - 06/06/05,bolo
    NdisMSetTimer(&pUmDevice->IndicateRxTimer, 10);
    */

#endif



    pUmDevice->TxDescLeft = MAX_TX_PACKET_DESCRIPTORS;
    Status = LM_ResetAdapter(pDevice);
    if( Status != LM_STATUS_SUCCESS){

		Status = NDIS_STATUS_RESOURCES;
		goto lFREE_RESOURCES;
	}

    pUmDevice->InitCompleted = TRUE;

#ifdef NDISCE_MINIPORT
    NdisDriverLoaded = TRUE;
    pDeviceForHeader = pDevice;
#endif

#ifdef SUPPORT_MEDIASENSE // add - 07/20/05,bolo for MSC
	// 2004/08/24, ccwong, add MediaSense support
	// Set up a timer to poll link status
	NdisMInitializeTimer(&LinkStatusTimer, MiniportAdapterHandle, UM_CheckLinkStatus, pUmDevice);
	NdisMSetPeriodicTimer(&LinkStatusTimer, UM_CHECK_LINK_INTERVAL);
	// End of ccwong
#endif

	//Finally Succeed
	Status = NDIS_STATUS_SUCCESS;

lFREE_RESOURCES:
	if( Status != NDIS_STATUS_SUCCESS ){

//		Status = NDIS_STATUS_FAILURE;

		// Stop the timerTo prevent Exception
	    if( pUmDevice->IndicateRxTimer.MiniportTimerFunction )
	    {
	        NdisMCancelTimer(&pUmDevice->IndicateRxTimer, &Cancelled);
			if( !Cancelled ){
				Sleep(100);
		        NdisMCancelTimer(&pUmDevice->IndicateRxTimer, &Cancelled);
			}
		}

        UM_FreeResources(pUmDevice);
	}

	gDuringInitialize = FALSE;
	return Status;

} // UM_Initialize

extern DWORD UnPlugNotify( void );

//******************************************************************************
// Description:
//    This routine is called when the OS wants to unload the driver.  This
//    deallocates all the resources allocated in UM_Initialize.
//
// Returns:
//    None.
//******************************************************************************
VOID  UM_Halt( IN PVOID MiniportAdapterContext )
{
    PUM_DEVICE_INFO_BLOCK   pUmDevice;


    #ifdef UM_DEBUG_CE
      printf("*** UM_Halt Start ***\n");
    #endif

    pUmDevice = (PUM_DEVICE_INFO_BLOCK) MiniportAdapterContext;

    // Stop everything first.
    UM_Shutdown(MiniportAdapterContext);

    // Deregister the shutdown routine.
    NdisMDeregisterAdapterShutdownHandler(pUmDevice->MiniportHandle);

    NdisFreeSpinLock(&pUmDevice->TransmitQSpinLock);
    NdisFreeSpinLock(&pUmDevice->ReceiveQSpinLock);

    //MM_InitWait( 1000000 );
      MM_InitWait( 100000 ); //jackl modify 05/31/2001
    // Free all the resources that have been allocated.
    UM_FreeResources(pUmDevice);

	NdisTerminateWrapper( NdisWrapper, NULL );

} // UM_Halt

//******************************************************************************
// Description:
//    This routine instructs the driver to issue a hardware reset to the
//    network adapter.  The driver also reset its software state.
//
// Returns:
//    NDIS_STATUS_SUCCESS
//******************************************************************************
NDIS_STATUS UM_Reset( OUT PBOOLEAN      AddressingReset,
                      IN  NDIS_HANDLE   MiniportAdapterContext )
{
    PUM_DEVICE_INFO_BLOCK    pUmDevice;
    LM_STATUS	Status; // add - 07/20/05,bolo

    #ifdef UM_DEBUG_PC
    DbgMessage(INFORM, (ADMTEXT("*** UM_Reset Start ***\n")));
    #endif
    #ifdef UM_DEBUG_CE
      printf("*** UM_Reset  ***\n");
    #endif

    pUmDevice = (PUM_DEVICE_INFO_BLOCK) MiniportAdapterContext;
    *AddressingReset = FALSE;

    // v1.09 jackl modify 2003_0411 for suspend and power off control
//    if ( pUmDevice->LmDevice.ResetAdpterFlag == 0 )
		if (!gDuringInitialize && !Unplugged )
    		Status = LM_ResetAdapter(&pUmDevice->LmDevice); // mod - 07/20/05,bolo

    // reset error counters
    pUmDevice->LmDevice.RxPacketCount           = 0;
    pUmDevice->LmDevice.RxFragListErrors        = 0;
    pUmDevice->LmDevice.RxLostPackets           = 0;
    pUmDevice->LmDevice.RxCrcErrors             = 0;
    pUmDevice->LmDevice.RxAlignErrors           = 0;
    pUmDevice->LmDevice.RxInvalidSizeErrors     = 0;

    pUmDevice->LmDevice.TxPacketCount           = 0;
    pUmDevice->LmDevice.TxPacketErrors          = 0;
    pUmDevice->LmDevice.TxUnderrunCount         = 0;
    pUmDevice->LmDevice.TxCollisionCount        = 0;
    pUmDevice->LmDevice.TxDefferedCount         = 0;

    //DbgMessage(INFORM, (ADMTEXT("*** UM_Reset End ***\n")));

#ifdef SUPPORT_MEDIASENSE // add - 07/20/05,bolo for MSC
    return NDIS_STATUS_SUCCESS;
#else
	if( Status == LM_STATUS_SUCCESS ){
		pUmDevice->LmDevice.LinkStatus = LM_STATUS_LINK_ACTIVE;
		return NDIS_STATUS_SUCCESS;
	}else{
		pUmDevice->LmDevice.LinkStatus = LM_STATUS_LINK_DOWN;
		return NDIS_STATUS_HARD_ERRORS;
	}
#endif
} // UM_Reset

#ifdef SUPPORT_MEDIASENSE // add - 07/20/05,bolo for MSC
// 2004/08/24, ccwong, add MediaSense support
//******************************************************************************
// Description:
//    Timer Function to check link status.
//
// Returns:
//    None.
//******************************************************************************
VOID UM_CheckLinkStatus( IN  PVOID   SystemSpecific1,
                           IN  PVOID   FunctionContext,
                           IN  PVOID   SystemSpecific2,
                           IN  PVOID   SystemSpecific3 )
{
    PUM_DEVICE_INFO_BLOCK   pUmDevice;
    PLM_DEVICE_INFO_BLOCK   pDevice;
	USHORT					 regDat;
	static int check_number = 0;

    pUmDevice = (PUM_DEVICE_INFO_BLOCK) FunctionContext;
    pDevice = &pUmDevice->LmDevice;

    if(pDevice->AdapterStatus != LM_STATUS_ADAPTER_OPEN) // add - 08/25/05,bolo
    {
        return;
    }

	regDat = LM_GetLinkStatus(pDevice);
	if( regDat & LINK_STS )
	{
		if( pDevice->LinkStatus != LM_STATUS_LINK_ACTIVE )
		{
            /* mark - 08/26/05,bolo
			LM_ReInitializeAdapter(pDevice);	// ccwong, 2004/09/13
            */
			pDevice->LinkStatus = LM_STATUS_LINK_ACTIVE;
			MM_IndicateStatus(pDevice,LM_STATUS_LINK_ACTIVE);
            #ifdef DEBCEINIT // add - 08/26/05,bolo
            // unmark - 09/05/05,bolo
            NKDbgPrintfW(TEXT("UM_CheckLinkStatus: LinkStatus=%d\n\r"),pDevice->LinkStatus);
            #endif

		}
	}
	else
	{
		if( pDevice->LinkStatus != LM_STATUS_LINK_DOWN )
		{
			pDevice->LinkStatus = LM_STATUS_LINK_DOWN;
			MM_IndicateStatus(pDevice,LM_STATUS_LINK_DOWN);
		} 
        else // add - 08/26/05,bolo
        {
			LM_ReInitializeAdapter(pDevice);
        }
	}
}
// End of ccwong
#endif

//******************************************************************************
// Description:
//    Timer Function to indicate received packets.
//    1. Pop a packet from head of pDevice->RxPacketReceivedList
//    2. Push the packet to tail of pDevice->RxPacketFreeList
//    3. Indicate received packets, one packet once
//    4. After indicated all packets, if there is not any packet issued for Bulk IN transfer, issue one
//
// Returns:
//    None.
//******************************************************************************
VOID UM_IndicateRxPackets( IN  PVOID   SystemSpecific1,
                           IN  PVOID   FunctionContext,
                           IN  PVOID   SystemSpecific2,
                           IN  PVOID   SystemSpecific3 )
{
    PUM_DEVICE_INFO_BLOCK   pUmDevice;
    PLM_DEVICE_INFO_BLOCK   pDevice;
    PUM_RX_PACKET_DESC      pUmPacket;
    PLM_PACKET_DESC         pPacket;
    WORD i=0;
    //NDIS_STATUS             Status;


    #ifdef RX_DEBUG_CE
     printf("*** UM_IndicateRxPackets start***\n");
    #endif
    #ifdef DEBCE // add - 05/31/05,bolo
    // mark - 09/05/05,bolo
    // printf("++UIRP\n"); // add - 05/26/06,bolo
    NKDbgPrintfW(TEXT("++UIRP\n\r")); // add - 09/05/05,bolo
    #endif
  //  printf("*** UM_IndicateRxPackets start***\n");
    #ifdef RX_DEBUG_PC
      DbgMessage(VERBOSE, (ADMTEXT("UM_IndicateRxPackets Start\n")));
    #endif
    pUmDevice = (PUM_DEVICE_INFO_BLOCK) FunctionContext;

    pDevice = &pUmDevice->LmDevice;
    NdisAcquireSpinLock(&pUmDevice->ReceiveQSpinLock);
    if( Unplugged )
    {

        if( LL_Empty(&pDevice->RxPacketReceivedList) )
        {
            //DbgMessage(VERBOSE, (ADMTEXT("%x h Rx Free packets, %x h Received packets\n"), LL_GetSize(&pDevice->RxPacketFreeList), LL_GetSize(&pDevice->RxPacketReceivedList)));
            NdisReleaseSpinLock(&pUmDevice->ReceiveQSpinLock);
            return;
        }
    }

    while(LL_Empty(&pDevice->RxPacketReceivedList) == FALSE)
    {
        pPacket = (PLM_PACKET_DESC) LL_PopHead(&pDevice->RxPacketReceivedList);
        LL_PushTail(&pDevice->RxPacketFreeList, &pPacket->Link);
        if( !Unplugged )
        {
            #ifdef UM_DEBUG_PC
//              DbgMessage(VERBOSE, (ADMTEXT("Received packet = 0x%x\n"), pPacket));
            #endif

            pUmPacket = (PUM_RX_PACKET_DESC) pPacket->pUmPacket;

            #if 0
            #if UM_DEBUG_PC
            DbgMessage(VERBOSE, (ADMTEXT("*** UM_IndicateRxPackets Start ***\n")));
    		DbgMessage(VERBOSE, (ADMTEXT("Rx PacketSize=%x\n"),pPacket->PacketSize));
		    DbgMessage(VERBOSE, (ADMTEXT("===============================================\n")));
		    for (i=0;i<(pPacket->PacketSize);i++)
		    {
		      if ( ((i%16) == 0 )  && ( i != 0 ) )
		        DbgMessage(VERBOSE, (ADMTEXT("\n")));

		      DbgMessage(VERBOSE, (ADMTEXT("%x "),pPacket->Buffer[i]));
		    }
		    #endif
		    #endif

            #ifdef DEBCE // add - 06/03/05,bolo
            // mark - 09/05/05,bolo
            // printf("UIRP: pkt=0x%x,len=%d\n",pPacket,pPacket->PacketSize);
            NKDbgPrintfW(TEXT("UIRP: pkt=0x%x,len=%d\n\r"),pPacket,pPacket->PacketSize); // add - 09/05/05,bolo
            #endif

            NdisAdjustBufferLength(pUmPacket->pNdisBuffer, pPacket->PacketSize);
            NDIS_SET_PACKET_STATUS(pUmPacket->pNdisPacket, NDIS_STATUS_RESOURCES);
            #ifdef UM_DEBUG_PC
//              DbgMessage(VERBOSE, (ADMTEXT("%x h Rx Free packets, %x h Received packets\n"), LL_GetSize(&pDevice->RxPacketFreeList), LL_GetSize(&pDevice->RxPacketReceivedList)));
            #endif
            NdisReleaseSpinLock(&pUmDevice->ReceiveQSpinLock);
            NdisMIndicateReceivePacket(pUmDevice->MiniportHandle, &pUmPacket->pNdisPacket, 1);

            //Status = NDIS_GET_PACKET_STATUS( pUmPacket->pNdisPacket );   // Fred
            //DbgMessage(VERBOSE, (ADMTEXT("Indicated Packet Status = %08x h\n"), Status));

            NdisAcquireSpinLock(&pUmDevice->ReceiveQSpinLock);

        }
    } // while

    if( Unplugged)
    {
        #ifdef UM_DEBUG_CE
          printf("%x h Rx Free packets, %x h Received packets\n", LL_GetSize(&pDevice->RxPacketFreeList), LL_GetSize(&pDevice->RxPacketReceivedList));
        #endif
        NdisReleaseSpinLock(&pUmDevice->ReceiveQSpinLock);
        return;
    }

    if( LL_GetSize(&pDevice->RxPacketFreeList) >= pDevice->MaxRxPacketDesc )
    {
        #ifdef UM_DEBUG_PC
          DbgMessage(VERBOSE, (ADMTEXT("pDevice->RxPacketFreeList is full, there is not any queued-packet\n")));
        #endif 
        #ifdef DEBCE // add - 06/09/05,bolo
        // mark - 09/05/05,bolo
        // printf("Rx free full !, RxFree=%d, RxPkt=%d\n", LL_GetSize(&pDevice->RxPacketFreeList), LL_GetSize(&pDevice->RxPacketReceivedList)); // add - 06/03/05,bolo
        NKDbgPrintfW(TEXT("Rx free full !, RxFree=%d, RxPkt=%d\n\r"),LL_GetSize(&pDevice->RxPacketFreeList), LL_GetSize(&pDevice->RxPacketReceivedList)); // add - 09/05/05,bolo
        #endif
        pPacket = (PLM_PACKET_DESC) LL_PopHead(&pDevice->RxPacketFreeList);
        if (pPacket)
        {
            // Issue another Bulk IN transfer to wait Rx data with new packet buffer            
            // DbgMessage(VERBOSE, (TEXT("UIRP: LQRP\n"))); // mark - 06/22/05,06/03/05,bolo
            if( LM_STATUS_SUCCESS != LM_QueueRxPacket(pDevice, pPacket) )
            {
                // DbgMessage(VERBOSE, (TEXT("UIRP: LQRP Failed\n"))); // mark - 06/22/05,06/03/05,bolo
                #ifdef DEBERR // add - 07/27/05,bolo
                    // mark - 09/05/05,bolo
                	// printf("UIRP: LQRP Failed !, RxFree=%d, RxPkt=%d\n", LL_GetSize(&pDevice->RxPacketFreeList), LL_GetSize(&pDevice->RxPacketReceivedList)); // mod - 06/22/05,06/03/05,bolo
                    NKDbgPrintfW(TEXT("UIRP: LQRP Failed !, RxFree=%d, RxPkt=%d\n\r"),LL_GetSize(&pDevice->RxPacketFreeList), LL_GetSize(&pDevice->RxPacketReceivedList)); // add - 09/05/05,bolo
                #endif
                #ifdef UM_DEBUG_PC
                  DbgMessage(VERBOSE, (ADMTEXT("LM_QueueRxPacket fail, push Rx Pack to Free list\n")));
                #endif
                LL_PushTail(&pDevice->RxPacketFreeList, &pPacket->Link);
            }

        }
    } // if

    #ifdef UM_DEBUG_PC
      //DbgMessage(VERBOSE, (ADMTEXT("Free Rx list %d\n"), LL_GetSize(&pDevice->RxPacketFreeList)));
      //DbgMessage(VERBOSE, (ADMTEXT("%x h Rx Free packets, %x h Received packets\n"), LL_GetSize(&pDevice->RxPacketFreeList), LL_GetSize(&pDevice->RxPacketReceivedList)));
    #endif
    NdisReleaseSpinLock(&pUmDevice->ReceiveQSpinLock);
    //NdisMSetPeriodicTimer(&pUmDevice->IndicateRxTimer, 10);
    //NdisMSetTimer(&pUmDevice->IndicateRxTimer, 10);

    #ifdef UM_DEBUG_CE
      printf("*** UM_IndicateRxPackets end**\n");
    #endif

    #ifdef DEBCE // add - 05/31/05,bolo
    // mark - 09/05/05,bolo
    // printf("--UIRP\n"); // add - 05/26/05,bolo
    NKDbgPrintfW(TEXT("--UIRP\n\r")); // add - 09/05/05,bolo
    #endif

    #ifdef RX_DEBUG_PC
      DbgMessage(VERBOSE, (ADMTEXT("** UM_IndicateRxPackets End ** \n")));
    #endif
	return;
} // UM_IndicateRxPackets

//******************************************************************************
// Description:
//    Sends a packet on the wire or queue it up for transmission.
//
// Returns:
//    NDIS_STATUS_SUCCESS
//    NDIS_STATUS_RESOURCES
//******************************************************************************
NDIS_STATUS UM_SendPacket( IN NDIS_HANDLE    MiniportAdapterContext,
                           IN PNDIS_PACKET   pNdisPacket,
                           IN UINT           SendFlags )
{
    PUM_DEVICE_INFO_BLOCK   pUmDevice;
    PLM_PACKET_DESC         pPacket;
    PUM_TX_PACKET_DESC      pUmPacket;
    LM_STATUS               LmStatus;


#ifdef NDISCE_MINIPORT
    if( Unplugged )
    {
		return NDIS_STATUS_FAILURE;		//Chg to
     }
#endif

    #ifdef TX_DEBUG_CE
      printf("*** UM_SendPacket Start ***\n");
    #endif

    pUmDevice = (PUM_DEVICE_INFO_BLOCK) MiniportAdapterContext;
    #ifdef TX_DEBUG_PC
      DbgMessage(VERBOSE, (ADMTEXT("UM_SendPacket Start\n")));
      DbgMessage(VERBOSE, (ADMTEXT("Free Tx Resource Left %d\n"), pUmDevice->TxDescLeft));
    #endif
//  printf("[%d]", pUmDevice->TxDescLeft);
    // Get a free tx packet descriptor.
    //_asm { cli };   //Fred
    NdisAcquireSpinLock(&pUmDevice->TransmitQSpinLock);
    if (pUmDevice->TxDescLeft <= 1)
    {
        pUmDevice->OutOfSendResources = TRUE;
        NdisReleaseSpinLock(&pUmDevice->TransmitQSpinLock);
        #ifdef DEBERR // add - 07/27/05,bolo
            // mark - 09/05/05,bolo
        	// printf("One free txpkt !, TxDescLeft=%d\n",pUmDevice->TxDescLeft); // add - 06/03/05,bolo
            NKDbgPrintfW(TEXT("One free txpkt !, TxDescLeft=%d\n\r"),pUmDevice->TxDescLeft); // add - 09/05/05,bolo
        #endif
        // DbgMessage(VERBOSE, (TEXT("One free txpkt !\n"))); // mark - 06/22/05,06/03/05,bolo
        //_asm { sti };   //Fred
        return NDIS_STATUS_RESOURCES;
    }
    pPacket = (PLM_PACKET_DESC)LL_PopHead(&pUmDevice->LmDevice.TxPacketFreeList);
    pUmDevice->TxDescLeft--;
    NdisReleaseSpinLock(&pUmDevice->TransmitQSpinLock);
    //_asm { sti };   //Fred
    if(pPacket == NULL)
    {
        pUmDevice->OutOfSendResources = TRUE;
        pUmDevice->TxDescLeft++;
        #ifdef DEBERR // add - 07/27/05,bolo
            // mark - 09/05/05,bolo
        	// printf("No free pkt !, TxDescLeft=%d\n",pUmDevice->TxDescLeft); // add - 06/03/05,bolo
            NKDbgPrintfW(TEXT("No free pkt !, TxDescLeft=%d\n\r"),pUmDevice->TxDescLeft); // add - 09/05/05,bolo
        #endif
        // DbgMessage(VERBOSE, (TEXT("No free pkt !\n"))); // mark - 06/22/05,06/03/05,bolo
        return NDIS_STATUS_RESOURCES;
    } // if

    pUmPacket = (PUM_TX_PACKET_DESC) pPacket->pUmPacket;
    pUmPacket->pNdisPacket = pNdisPacket;


    //NdisAcquireSpinLock(&pUmDevice->IrpSpinLock);
    LmStatus = LM_SendPacket(&pUmDevice->LmDevice, pPacket);
    //NdisReleaseSpinLock(&pUmDevice->IrpSpinLock);
    if(LmStatus != LM_STATUS_SUCCESS)
    {
        #ifdef UM_DEBUG_PC
          DbgMessage(VERBOSE, (ADMTEXT("LM_SendPacket 1 fail in UM_SendPacket\n")));
        #endif
        #ifdef DEBERR // add - 07/27/05,bolo
            // mark - 09/05/05,bolo
        	// printf("Send Fail !, TxDescLeft=%d\n",pUmDevice->TxDescLeft); // add - 06/03/05,bolo
            NKDbgPrintfW(TEXT("Send Fail !, TxDescLeft=%d\n\r"),pUmDevice->TxDescLeft); // add - 09/05/05,bolo
        #endif
        // DbgMessage(VERBOSE, (TEXT("Send Fail !\n"))); // mark - 06/22/05,06/03/05,bolo
        NdisAcquireSpinLock(&pUmDevice->TransmitQSpinLock);
        LL_PushHead(&pUmDevice->LmDevice.TxPacketFreeList, &pPacket->Link);
        pUmDevice->TxDescLeft++;
        NdisReleaseSpinLock(&pUmDevice->TransmitQSpinLock);
        return NDIS_STATUS_FAILURE;
    } // if

    #ifdef TX_DEBUG_PC
      DbgMessage(VERBOSE, (ADMTEXT("PACKET LEN=%04x\n"),pPacket->PacketSize));
    #endif

    // Baushg Workaround chipset bug, 1999.01.21
    // Send a zero size packet to device to fix the bug
    if ( pUmDevice->LmDevice.FixFlag == 1)
    {
        // Get a free tx packet descriptor.
        //_asm { cli };   //Fred
        #ifdef DEBTXERR // mod - 07/27/05,add - 05/31/05,bolo
        // mark - 09/05/05,bolo
        // printf("Send null pkt !, TxDescLeft=%d\n",pUmDevice->TxDescLeft); // add - 06/03/05,bolo
        NKDbgPrintfW(TEXT("Send null pkt !, TxDescLeft=%d\n\r"),pUmDevice->TxDescLeft); // add - 09/05/05,bolo
        #endif
        // DbgMessage(VERBOSE, (TEXT("Send null pkt !\n"))); // mark - 06/22/05,06/03/05,bolo
        NdisAcquireSpinLock(&pUmDevice->TransmitQSpinLock);
        pPacket = (PLM_PACKET_DESC)
        LL_PopHead(&pUmDevice->LmDevice.TxPacketFreeList);
        pUmDevice->TxDescLeft--;
        NdisReleaseSpinLock(&pUmDevice->TransmitQSpinLock);
        //_asm { sti };   //Fred
        if(pPacket == NULL)
        {
            pUmDevice->OutOfSendResources = TRUE;
            pUmDevice->TxDescLeft++;
            return NDIS_STATUS_RESOURCES;
        } // if
        //NdisAcquireSpinLock(&pUmDevice->IrpSpinLock);
        LmStatus = LM_SendPacket(&pUmDevice->LmDevice, pPacket);
        //NdisReleaseSpinLock(&pUmDevice->IrpSpinLock);
        if(LmStatus != LM_STATUS_SUCCESS)
        {
        	#ifdef DEBERR // add - 07/27/05,bolo
            // mark - 09/05/05,bolo
            // printf("Send null pkt fail !, TxDescLeft=%d\n",pUmDevice->TxDescLeft); // add - 06/03/05,bolo
            NKDbgPrintfW(TEXT("Send null pkt fail !, TxDescLeft=%d\n\r"),pUmDevice->TxDescLeft); // add - 09/05/05,bolo
          #endif
            // DbgMessage(VERBOSE, (TEXT("Send null pkt fail !\n"))); // mark - 06/22/05,06/03/05,bolo
            #ifdef UM_DEBUG_PC
              DbgMessage(VERBOSE, (ADMTEXT("LM_SendPacket 2 fail in UM_SendPacket\n")));
            #endif
        }
    }
    //-----------------------------------------end, 1999.01.21

    //DbgMessage(FATAL,(ADMTEXT("Indicate Send Complete\n")));
    //NdisMSendComplete(pUmDevice->MiniportHandle,pUmPacket->pNdisPacket,NDIS_STATUS_SUCCESS);
	NdisMSendComplete(pUmDevice->MiniportHandle,pNdisPacket,NDIS_STATUS_SUCCESS);

    #ifdef TX_DEBUG_CE
      printf("*** UM_SendPacket End ***\n");
    #endif

    #ifdef TX_DEBUG_PC
    DbgMessage(VERBOSE, (ADMTEXT(" **UM_SendPacket End ** \n")));
    #endif

	return NDIS_STATUS_PENDING;

    //return LM_STATUS_SUCCESS;
} // UM_SendPacket

//******************************************************************************
// Description:
//    This routine copies the receive buffer into an NDIS_PACKET.
//
// Parameters:
//    MiniportAdapterContext - Context registered with the wrapper, really
//              a pointer to the adapter.
//    MiniportReceiveContext - The context value passed by the driver on its
//              call to NdisMEthIndicateReceive.  The driver can use this value
//              to determine which packet, on which adapter, is being received.
//    ByteOffset - An unsigned integer specifying the offset within the
//              received packet at which the copy is to begin.  If the entire
//              packet is to be copied, ByteOffset must be zero.
//    BytesToTransfer - An unsigned integer specifying the number of bytes
//              to copy.  It is legal to transfer zero bytes; this has no
//              effect.  If the sum of ByteOffset and BytesToTransfer is
//              greater than the size of the received packet, then the
//              remainder of the packet (starting from ByteOffset) is
//              transferred, and the trailing portion of the receive buffer is
//              not modified.
//    Packet - A pointer to a descriptor for the packet storage into which
//              the MAC is to copy the received packet.
//    BytesTransfered - A pointer to an unsigned integer.  The MAC writes
//              the actual number of bytes transferred into this location.
//              This value is not valid if the return status is STATUS_PENDING.
//
// Returns:
//    NDIS_STATUS_SUCCESS
//******************************************************************************
NDIS_STATUS UM_TransferData( OUT PNDIS_PACKET   Packet,
                             OUT PUINT          BytesTransferred,
                             IN  NDIS_HANDLE    MiniportAdapterContext,
                             IN  NDIS_HANDLE    MiniportReceiveContext,
                             IN  UINT           ByteOffset,
                             IN  UINT           BytesToTransfer )
{
    PNDIS_BUFFER pNdisBuffer;
    PUCHAR pDataBuffer;
    PUCHAR pDestBuffer;
    UINT BytesToCopy;
    UINT BufferLength;

    pDataBuffer = (PUCHAR) MiniportReceiveContext + ByteOffset +
        ETHERNET_PACKET_HEADER_SIZE;
    *BytesTransferred = 0;

    DbgMessage(FATAL, (ADMTEXT("*** UM_TransferData ***\n")));
    //DbgBreak();
    NdisQueryPacket(Packet, NULL, NULL, &pNdisBuffer, NULL);

    while(BytesToTransfer) {
        NdisQueryBuffer(pNdisBuffer, &pDestBuffer, &BufferLength);

        BytesToCopy = BytesToTransfer < BufferLength ? BytesToTransfer:
            BufferLength;

        NdisMoveMemory(pDestBuffer, pDataBuffer, BytesToCopy);
        pDataBuffer += BytesToCopy;
        BytesToTransfer -= BytesToCopy;
        *BytesTransferred += BytesToCopy;

        NdisGetNextBuffer(pNdisBuffer, &pNdisBuffer);
        if(pNdisBuffer == NULL) {
          NdisFreeBuffer(pNdisBuffer);
            break;
        }
    } // while

#if DBG
    if(BytesToTransfer) {
        DbgMessage(FATAL, (ADMTEXT("Transferred data error\n")));
//      MM_Break();
    }
#endif

    return NDIS_STATUS_SUCCESS;
} // UM_TransferData

//******************************************************************************
// Description:
//    This routine handles OID get operations.
//
// Returns:
//    NDIS_STATUS_SUCCESS
//    NDIS_STATUS_INVALID_LENGTH
//    NDIS_STATUS_INVALID_OID
//******************************************************************************
NDIS_STATUS UM_QueryInformation( IN  PNDIS_HANDLE   MiniportAdapterContext,
                                 IN  NDIS_OID       Oid,
                                 IN  PVOID          InformationBuffer,
                                 IN  ULONG          InformationBufferLength,
                                 OUT PULONG         BytesWritten,
                                 OUT PULONG         BytesNeeded )
{
    static const NDIS_OID SupportedOids[] =
    {
        OID_GEN_SUPPORTED_LIST,
        OID_GEN_HARDWARE_STATUS,
        OID_GEN_MEDIA_SUPPORTED,
        OID_GEN_MEDIA_IN_USE,
        OID_GEN_MAXIMUM_LOOKAHEAD,
        OID_GEN_MAXIMUM_FRAME_SIZE,
        OID_GEN_MAXIMUM_TOTAL_SIZE,
        OID_GEN_MAC_OPTIONS,
        OID_GEN_PROTOCOL_OPTIONS,
        OID_GEN_LINK_SPEED,
        OID_GEN_TRANSMIT_BUFFER_SPACE,
        OID_GEN_RECEIVE_BUFFER_SPACE,
        OID_GEN_TRANSMIT_BLOCK_SIZE,
        OID_GEN_RECEIVE_BLOCK_SIZE,
        OID_GEN_VENDOR_ID,
        OID_GEN_VENDOR_DESCRIPTION,
        OID_GEN_DRIVER_VERSION,
        OID_GEN_CURRENT_PACKET_FILTER,
        OID_GEN_CURRENT_LOOKAHEAD,
        OID_GEN_XMIT_OK,
        OID_GEN_RCV_OK,
        OID_GEN_XMIT_ERROR,
        OID_GEN_RCV_ERROR,
        OID_GEN_RCV_NO_BUFFER,
        OID_GEN_RCV_CRC_ERROR,
        OID_GEN_MEDIA_CONNECT_STATUS,
        OID_GEN_VENDOR_DRIVER_VERSION,
        OID_GEN_MAXIMUM_SEND_PACKETS,
        OID_802_3_PERMANENT_ADDRESS,
        OID_802_3_CURRENT_ADDRESS,
        OID_802_3_MULTICAST_LIST,
        OID_802_3_MAXIMUM_LIST_SIZE,
        OID_802_3_RCV_ERROR_ALIGNMENT,
        OID_802_3_XMIT_ONE_COLLISION,
        OID_802_3_XMIT_MORE_COLLISIONS,
        OID_802_3_XMIT_DEFERRED,
        OID_802_3_XMIT_MAX_COLLISIONS,
        OID_802_3_RCV_OVERRUN,
        OID_802_3_XMIT_UNDERRUN,
#ifndef NDISCE_MINIPORT
        //Power Management stuff
        OID_PNP_SET_POWER,
        OID_PNP_QUERY_POWER,
        OID_PNP_ADD_WAKE_UP_PATTERN,
        OID_PNP_REMOVE_WAKE_UP_PATTERN,
        OID_PNP_ENABLE_WAKE_UP,
#endif
    }; // SupportedOids

    PUM_DEVICE_INFO_BLOCK   pUmDevice;
    PLM_DEVICE_INFO_BLOCK   pLmDevice;
    UCHAR                   TempBuffer[256];
    NDIS_STATUS             Status;
    PUCHAR                  pBuffer;
    ULONG                   BufferSize;
#ifndef NDISCE_MINIPORT
    NDIS_PNP_CAPABILITIES   Power_Management_Capabilities;
#endif


    #ifdef UM_DEBUG_CE
      //printf("*** UM_QueryInformation *** OID = 0x%x\n", Oid);
    #endif

    pUmDevice = (PUM_DEVICE_INFO_BLOCK) MiniportAdapterContext;
    pLmDevice = (PLM_DEVICE_INFO_BLOCK) MiniportAdapterContext;
    pBuffer = TempBuffer;

    Status = NDIS_STATUS_SUCCESS;
    BufferSize = sizeof(ULONG);

    switch(Oid)
    {
#ifndef NDISCE_MINIPORT
        case OID_PNP_CAPABILITIES:
            #ifdef UM_DEBUG_PC
              DbgMessage(INFORM, (ADMTEXT("OID_PNP_CAPABILITIES\n")));
            #endif
            Power_Management_Capabilities.WakeUpCapabilities.MinMagicPacketWakeUp = NdisDeviceStateD3;
            Power_Management_Capabilities.WakeUpCapabilities.MinPatternWakeUp     = NdisDeviceStateD3;
            Power_Management_Capabilities.WakeUpCapabilities.MinLinkChangeWakeUp  = NdisDeviceStateD3;
            pBuffer = (PUCHAR) &Power_Management_Capabilities;
            BufferSize = sizeof(NDIS_PNP_CAPABILITIES);
            break;

        case OID_PNP_QUERY_POWER:
            #ifdef UM_DEBUG_PC
              DbgMessage(INFORM, (ADMTEXT("OID_PNP_QUERY_POWER\n")));
            #endif
            *((NDIS_DEVICE_POWER_STATE * )pBuffer) = NdisDeviceStateD0;
            BufferSize = sizeof(NDIS_DEVICE_POWER_STATE);
            break;

        case OID_TCP_TASK_OFFLOAD:
        case OID_TCP_TASK_IPSEC_ADD_SA:
        case OID_TCP_TASK_IPSEC_DELETE_SA:
        //case OID_TCP_TASK_IPSEC_QUERY_SPI:
            #ifdef UM_DEBUG_PC
              DbgMessage(INFORM, (ADMTEXT("OID_TCP_TASK\n")));
            #endif
            Status = NDIS_STATUS_NOT_SUPPORTED;
            break;

        case OID_GEN_SUPPORTED_GUIDS:
            #ifdef UM_DEBUG_PC
              DbgMessage(INFORM, (ADMTEXT("OID_GEN_SUPPORTED_GUIDS\n")));
            #endif
            Status = NDIS_STATUS_NOT_SUPPORTED;
            break;
#endif

        case OID_GEN_SUPPORTED_LIST:
            #ifdef UM_DEBUG_PC
              DbgMessage(VERBOSE, (ADMTEXT("OID_GEN_SUPPORTED_LIST\n")));
            #endif
            pBuffer = (PUCHAR) SupportedOids;
            BufferSize = sizeof(SupportedOids);
            break;

        case OID_GEN_HARDWARE_STATUS:
            #ifdef UM_DEBUG_PC
              DbgMessage(VERBOSE, (ADMTEXT("OID_GEN_HARDWARE_STATUS\n")));
            #endif
            *((NDIS_HARDWARE_STATUS *) pBuffer) = NdisHardwareStatusReady;
            break;

        case OID_GEN_MEDIA_SUPPORTED:
            #ifdef UM_DEBUG_PC
              DbgMessage(VERBOSE, (ADMTEXT("OID_GEN_MEDIA_SUPPORTED\n")));
            #endif
            *((PNDIS_MEDIUM) pBuffer) = NdisMedium802_3;
            BufferSize = sizeof(NDIS_MEDIUM);
            break;

        case OID_GEN_MEDIA_IN_USE:
            #ifdef UM_DEBUG_PC
              DbgMessage(VERBOSE, (ADMTEXT("OID_GEN_MEDIA_IN_USE\n")));
            #endif
            *((PNDIS_MEDIUM) pBuffer) = NdisMedium802_3;
            BufferSize = sizeof(NDIS_MEDIUM);
            break;

        case OID_GEN_MAXIMUM_LOOKAHEAD:
            #ifdef UM_DEBUG_PC
              DbgMessage(VERBOSE, (ADMTEXT("OID_GEN_MAXIMUM_LOOKAHEAD\n")));
            #endif
            *((ULONG *) pBuffer) = 1500;
            break;

        case OID_GEN_CURRENT_LOOKAHEAD:
            #ifdef UM_DEBUG_PC
              DbgMessage(VERBOSE, (ADMTEXT("OID_GEN_CURRENT_LOOKAHEAD\n")));
            #endif
            *((ULONG *) pBuffer) = 1500;
            break;

        case OID_GEN_MAXIMUM_FRAME_SIZE:
            #ifdef UM_DEBUG_PC
              DbgMessage(VERBOSE, (ADMTEXT("OID_GEN_MAXIMUM_FRAME_SIZE\n")));
            #endif
            *((ULONG *) pBuffer) = 1500;
            break;

        case OID_GEN_LINK_SPEED:
            #ifdef UM_DEBUG_PC
              DbgMessage(VERBOSE, (ADMTEXT("OID_GEN_LINK_SPEED\n")));
            #endif
            *((ULONG *) pBuffer) = 100000;
            if(pUmDevice->LmDevice.LinkSpeed == LINK_SPEED_100MBPS)
                *((ULONG *) pBuffer) = 1000000;
            break;

        case OID_GEN_TRANSMIT_BUFFER_SPACE:
            #ifdef UM_DEBUG_PC
              DbgMessage(VERBOSE, (ADMTEXT("OID_GEN_TRANSMIT_BUFFER_SPACE\n")));
            #endif
            *((ULONG *) pBuffer) = MAX_ETHERNET_PACKET_SIZE *
            pUmDevice->LmDevice.MaxTxPacketDesc;
            break;

        case OID_GEN_RECEIVE_BUFFER_SPACE:
            #ifdef UM_DEBUG_PC
              DbgMessage(VERBOSE, (ADMTEXT("OID_GEN_RECEIVE_BUFFER_SPACE\n")));
            #endif
            *((ULONG *) pBuffer) = MAX_ETHERNET_PACKET_SIZE *
            pUmDevice->LmDevice.MaxRxPacketDesc;
            break;

        case OID_GEN_TRANSMIT_BLOCK_SIZE:
            #ifdef UM_DEBUG_PC
              DbgMessage(VERBOSE, (ADMTEXT("OID_GEN_TRANSMIT_BLOCK_SIZE\n")));
            #endif
            *((ULONG *) pBuffer) = MAX_ETHERNET_PACKET_SIZE;
            break;

        case OID_GEN_RECEIVE_BLOCK_SIZE:
            #ifdef UM_DEBUG_PC
              DbgMessage(VERBOSE, (ADMTEXT("OID_GEN_RECEIVE_BLOCK_SIZE\n")));
            #endif
            *((ULONG *) pBuffer) = MAX_ETHERNET_PACKET_SIZE;
            break;

        case OID_GEN_VENDOR_ID:
            #ifdef UM_DEBUG_PC
              DbgMessage(VERBOSE, (ADMTEXT("OID_GEN_VENDOR_ID\n")));
            #endif
            pBuffer = pUmDevice->LmDevice.PermanentNodeAddress;
            break;

        case OID_802_3_PERMANENT_ADDRESS:
            #ifdef UM_DEBUG_PC
              DbgMessage(VERBOSE, (ADMTEXT("OID_802_3_PERMANENT_ADDRESS\n")));
            #endif
            pBuffer = pUmDevice->LmDevice.PermanentNodeAddress;
            BufferSize = 6;
            break;

        case OID_GEN_VENDOR_DESCRIPTION:
            #ifdef UM_DEBUG_PC
              DbgMessage(VERBOSE, (ADMTEXT("OID_GEN_VENDOR_DESCRIPTION\n")));
            #endif
            pBuffer = DRIVER_FILE_DESCRIPTION;
            BufferSize = sizeof(DRIVER_FILE_DESCRIPTION);
            break;

        case OID_GEN_VENDOR_DRIVER_VERSION:
            #ifdef UM_DEBUG_PC
              DbgMessage(VERBOSE, (ADMTEXT("OID_GEN_VENDOR_DRIVER_VERSION\n")));
            #endif
            *((USHORT *) pBuffer) = (DRIVER_MAJOR_VERSION << 8) | DRIVER_MINOR_VERSION;
            BufferSize = sizeof(USHORT);
            break;

        case OID_GEN_CURRENT_PACKET_FILTER:
            #ifdef UM_DEBUG_PC
              DbgMessage(VERBOSE, (ADMTEXT("OID_GEN_CURRENT_PACKET_FILTER\n")));
            #endif

            //*((ULONG *) pBuffer) = pUmDevice->FilterMask;
            break;

        case OID_GEN_DRIVER_VERSION:
            #ifdef UM_DEBUG_PC
              DbgMessage(VERBOSE, (ADMTEXT("OID_GEN_DRIVER_VERSION\n")));
            #endif
            *((USHORT *) pBuffer) = (DRIVER_NDIS_MAJOR_VERSION << 8) | DRIVER_NDIS_MINOR_VERSION;
            BufferSize = sizeof(USHORT);
            break;

        case OID_GEN_MAXIMUM_TOTAL_SIZE:
            #ifdef UM_DEBUG_PC
              DbgMessage(VERBOSE, (ADMTEXT("OID_GEN_MAXIMUM_TOTAL_SIZE\n")));
            #endif
            *((ULONG *) pBuffer) = MAX_ETHERNET_PACKET_SIZE;
            break;

        case OID_GEN_MAXIMUM_SEND_PACKETS:
            #ifdef UM_DEBUG_PC
              DbgMessage(VERBOSE, (ADMTEXT("OID_GEN_MAXIMUM_SEND_PACKETS\n")));
            #endif
            *((ULONG *) pBuffer) = pUmDevice->LmDevice.MaxTxPacketDesc;
            break;

        case OID_GEN_MAC_OPTIONS:
            #ifdef UM_DEBUG_PC
              DbgMessage(VERBOSE, (ADMTEXT("OID_GEN_MAC_OPTIONS\n")));
            #endif
            *((ULONG *) pBuffer) = NDIS_MAC_OPTION_RECEIVE_SERIALIZED |
                                   NDIS_MAC_OPTION_TRANSFERS_NOT_PEND |
                                   NDIS_MAC_OPTION_NO_LOOPBACK |
                                   NDIS_MAC_OPTION_FULL_DUPLEX |
                                   NDIS_MAC_OPTION_COPY_LOOKAHEAD_DATA;
            break;

        case OID_GEN_PROTOCOL_OPTIONS:
            #ifdef UM_DEBUG_PC
              DbgMessage(VERBOSE, (ADMTEXT("OID_GEN_PROTOCOL_OPTIONS\n")));
            #endif
            *((ULONG *) pBuffer) = 0;
            break;

        case OID_GEN_MEDIA_CONNECT_STATUS:
            #ifdef UM_DEBUG_PC
              DbgMessage(VERBOSE, (ADMTEXT("OID_GEN_MEDIA_CONNECT_STATUS\n")));
            #endif

#ifdef SUPPORT_MEDIASENSE // add - 07/20/05,bolo for MSC
            *((ULONG *) pBuffer) = NdisMediaStateConnected;
            if(pUmDevice->LmDevice.LinkStatus == LM_STATUS_LINK_DOWN)
            *((ULONG *) pBuffer) = NdisMediaStateDisconnected;
#else            
			if(pUmDevice->LmDevice.LinkStatus == LM_STATUS_LINK_ACTIVE){
				*((ULONG *) pBuffer) = NdisMediaStateConnected;
			}else{
				*((ULONG *) pBuffer) = NdisMediaStateDisconnected;
			}
#endif
            break;

        case OID_GEN_XMIT_OK:
            #ifdef UM_DEBUG_PC
              DbgMessage(VERBOSE, (ADMTEXT("OID_GEN_XMIT_OK\n")));
            #endif
            *((ULONG *) pBuffer) = pUmDevice->LmDevice.TxPacketCount;
            break;

        case OID_GEN_RCV_OK:
            #ifdef UM_DEBUG_PC
              DbgMessage(VERBOSE, (ADMTEXT("OID_GEN_RCV_OK\n")));
            #endif
            *((ULONG *) pBuffer) = pUmDevice->LmDevice.RxPacketCount;
            break;

        case OID_GEN_XMIT_ERROR:
            #ifdef UM_DEBUG_PC
              DbgMessage(VERBOSE, (ADMTEXT("OID_GEN_XMIT_ERROR\n")));
            #endif
            *((ULONG *) pBuffer) = pUmDevice->LmDevice.TxPacketErrors;
            break;

        case OID_GEN_RCV_CRC_ERROR:
            #ifdef UM_DEBUG_PC
              DbgMessage(VERBOSE, (ADMTEXT("OID_GEN_RCV_CRC_ERROR\n")));
            #endif
            *((ULONG *) pBuffer) = pUmDevice->LmDevice.RxCrcErrors;
            break;

        case OID_GEN_RCV_ERROR:
            #ifdef UM_DEBUG_PC
              DbgMessage(VERBOSE, (ADMTEXT("OID_GEN_RCV_ERROR\n")));
            #endif
            *((ULONG *) pBuffer) = pUmDevice->LmDevice.RxCrcErrors;
            break;

        case OID_GEN_RCV_NO_BUFFER:
            #ifdef UM_DEBUG_PC
              DbgMessage(VERBOSE, (ADMTEXT("OID_GEN_RCV_NO_BUFFER\n")));
            #endif
            *((ULONG *) pBuffer) = 0;
            break;

        case OID_802_3_CURRENT_ADDRESS:
            #ifdef UM_DEBUG_PC
              DbgMessage(VERBOSE, (ADMTEXT("OID_802_3_CURRENT_ADDRESS\n")));
            #endif
            pBuffer = pUmDevice->LmDevice.NodeAddress;
            BufferSize = 6;
            break;

        case OID_802_3_MAXIMUM_LIST_SIZE:
            #ifdef UM_DEBUG_PC
              DbgMessage(VERBOSE, (ADMTEXT("OID_802_3_MAXIMUM_LIST_SIZE\n")));
            #endif
            *((ULONG *) pBuffer) = 32;
            break;

        case OID_802_3_RCV_ERROR_ALIGNMENT:
            #ifdef UM_DEBUG_PC
              DbgMessage(VERBOSE, (ADMTEXT("OID_802_3_RCV_ERROR_ALIGNMENT\n")));
            #endif
            *((ULONG *) pBuffer) = pUmDevice->LmDevice.RxAlignErrors;
            break;

        case OID_802_3_XMIT_ONE_COLLISION:
            #ifdef UM_DEBUG_PC
              DbgMessage(VERBOSE, (ADMTEXT("OID_802_3_XMIT_MORE_COLLISIONS\n")));
            #endif
            *((ULONG *) pBuffer) = pUmDevice->LmDevice.TxCollisionCount;
            break;

        case OID_802_3_XMIT_MORE_COLLISIONS:
            #ifdef UM_DEBUG_PC
              DbgMessage(VERBOSE, (ADMTEXT("OID_802_3_XMIT_MORE_COLLISIONS\n")));
            #endif
            *((ULONG *) pBuffer) = 0;
            break;

        case OID_802_3_XMIT_DEFERRED:
            #ifdef UM_DEBUG_PC
              DbgMessage(VERBOSE, (ADMTEXT("OID_802_3_XMIT_DEFERRED\n")));
            #endif
            *((ULONG *) pBuffer) = pUmDevice->LmDevice.TxDefferedCount;
            break;

        case OID_802_3_XMIT_MAX_COLLISIONS:
            #ifdef UM_DEBUG_PC
              DbgMessage(VERBOSE, (ADMTEXT("OID_802_3_XMIT_MAX_COLLISIONS\n")));
            #endif
            *((ULONG *) pBuffer) = 0;
            break;

        case OID_802_3_RCV_OVERRUN:
             #ifdef UM_DEBUG_PC
              DbgMessage(VERBOSE, (ADMTEXT("OID_802_3_RCV_OVERRUN\n")));
             #endif
            *((ULONG *) pBuffer) = 0;
            break;

        case OID_802_3_XMIT_UNDERRUN:
            #ifdef UM_DEBUG_PC
              DbgMessage(VERBOSE, (ADMTEXT("OID_802_3_XMIT_UNDERRUN\n")));
            #endif
            *((ULONG *) pBuffer) = pUmDevice->LmDevice.TxUnderrunCount;
            break;

        default:
            Status = NDIS_STATUS_INVALID_OID;
            #ifdef UM_DEBUG_PC
              DbgMessage(FATAL, (ADMTEXT("Oid not supported\n")));
            #endif
            break;
    } // switch

    if(Status == NDIS_STATUS_SUCCESS)
    {
        if(BufferSize > InformationBufferLength)
        {
            *BytesNeeded = BufferSize - InformationBufferLength;
            Status = NDIS_STATUS_INVALID_LENGTH;
        }
        else
        {
            NdisMoveMemory(InformationBuffer, pBuffer, BufferSize);
            *BytesWritten = BufferSize;
        } // if
    } // if

    return Status;
} // UM_QueryInformation

//******************************************************************************
// Description:
//    This routine handles OID set operations.
//
// Returns:
//    NDIS_STATUS_SUCCESS
//    NDIS_STATUS_INVALID_LENGTH
//    NDIS_STATUS_INVALID_OID
// IRQL :
//    DISPATCH_LEVEL
//******************************************************************************
NDIS_STATUS UM_SetInformation( IN  NDIS_HANDLE   MiniportAdapterContext,
                               IN  NDIS_OID      Oid,
                               IN  PVOID         InformationBuffer,
                               IN  ULONG         InformationBufferLength,
                               OUT PULONG        BytesRead,
                               OUT PULONG        BytesNeeded )
{
    PUM_DEVICE_INFO_BLOCK   pUmDevice;
    NDIS_STATUS             Status;

    #ifdef UM_DEBUG_PC
      DbgMessage(VERBOSE, (ADMTEXT("*** UM_SetInformation ***\n")));
      //DbgMessage(VERBOSE, (ADMTEXT("OID = 0x%x\n"), Oid));
    #endif

    #ifdef UM_DEBUG_CE
      //printf("*** UM_SetInformation ***\n");
    #endif

    pUmDevice = (PUM_DEVICE_INFO_BLOCK) MiniportAdapterContext;
    Status = NDIS_STATUS_SUCCESS;

    switch(Oid)
    {
        case OID_GEN_CURRENT_PACKET_FILTER:
        {
            //static ULONG   HCK[] = { 0x6f72500a, 0x6d617267, 0x2064656d, 0x48207962, 0x4b207661, 0x76756168, 0x0000000a };
            #if 0
            ULONG          ReceiveMask;

            #ifdef UM_DEBUG_PC
              DbgMessage(INFORM, (ADMTEXT("OID_GEN_CURRENT_PACKET_FILTER\n")));
            #endif
            if(InformationBufferLength != 4)
            {
                #ifdef UM_DEBUG_PC
                  DbgMessage(FATAL, (ADMTEXT("Inalid buffer length for OID_GEN_CURRENT_PACKET_FILTER : %d\n"), InformationBufferLength));
                #endif
                Status = NDIS_STATUS_INVALID_LENGTH;
                *BytesNeeded = 4;
                break;
            } // if

            pUmDevice->FilterMask = *((PULONG) InformationBuffer);
            #ifdef UM_DEBUG_PC
              DbgMessage(VERBOSE, (ADMTEXT("FilterMask : 0x%x \n"), pUmDevice->FilterMask));
            #endif
            if(pUmDevice->FilterMask & ( NDIS_PACKET_TYPE_ALL_FUNCTIONAL | NDIS_PACKET_TYPE_ALL_LOCAL |
                                         NDIS_PACKET_TYPE_FUNCTIONAL     | NDIS_PACKET_TYPE_GROUP     |
                                         NDIS_PACKET_TYPE_MAC_FRAME      | NDIS_PACKET_TYPE_SMT       |
                                         NDIS_PACKET_TYPE_SOURCE_ROUTING ))
            {
                #ifdef UM_DEBUG_PC
                  DbgMessage(INFORM, (ADMTEXT("Packet filter not supported\n")));
                #endif
                Status = NDIS_STATUS_NOT_SUPPORTED;
                break;
            } // if

            ReceiveMask = 0;
            if(pUmDevice->FilterMask & NDIS_PACKET_TYPE_ALL_MULTICAST)
                ReceiveMask |= RECEIVE_MASK_ACCEPT_MULTI_PROM;
            if(pUmDevice->FilterMask & NDIS_PACKET_TYPE_BROADCAST)
                ReceiveMask |= RECEIVE_MASK_ACCEPT_BROADCAST;
            if(pUmDevice->FilterMask & NDIS_PACKET_TYPE_MULTICAST)
                ReceiveMask |= RECEIVE_MASK_ACCEPT_MULTICAST;
            if(pUmDevice->FilterMask & NDIS_PACKET_TYPE_PROMISCUOUS)
                ReceiveMask |= RECEIVE_MASK_PROMISCUOUS_MODE | RECEIVE_MASK_ACCEPT_BROADCAST | RECEIVE_MASK_ACCEPT_MULTI_PROM;

            #ifdef UM_DEBUG_PC
              DbgMessage(VERBOSE, (ADMTEXT("ReceiveMask : 0x%x \n"), ReceiveMask));
            #endif

#ifdef NDISCE_MINIPORT
            ReceiveMaskForHeader = ReceiveMask;
            if( !Unplugged )
#endif
                LM_SetReceiveMask(&pUmDevice->LmDevice, ReceiveMask);

            *BytesRead = 4;
            #endif
            break;
        } // case OID_GEN_CURRENT_PACKET_FILTER

        case OID_GEN_CURRENT_LOOKAHEAD:
            #ifdef UM_DEBUG_PC
              DbgMessage(INFORM, (ADMTEXT("OID_GEN_CURRENT_LOOKAHEAD\n")));
            #endif
            // we always indicate the whole packet, so no need to record look ahead size.
            *BytesRead = 4;
            break;

        case OID_802_3_MULTICAST_LIST:
            #ifdef UM_DEBUG_PC
              DbgMessage(INFORM, (ADMTEXT("OID_802_3_MULTICAST_LIST\n")));
            #endif
            //Status = NDIS_STATUS_NOT_SUPPORTED;
            #if 0
            if(InformationBufferLength % ETH_LENGTH_OF_ADDRESS)
            {
                #ifdef UM_DEBUG_PC
                  DbgMessage(FATAL, (ADMTEXT("Invalid buffer length for OID_802_3_MULTICAST_LIST: %d\n"), InformationBufferLength));
                #endif
                Status = NDIS_STATUS_INVALID_LENGTH;
                *BytesNeeded = ETH_LENGTH_OF_ADDRESS;
                break;
            } // if
            *BytesRead = InformationBufferLength;
            #endif
            break;

#ifndef NDISCE_MINIPORT
        case OID_PNP_SET_POWER:
              DbgMessage(INFORM, (ADMTEXT("OID_PNP_SET_POWER\n")));

            // Get ready, because our device may soon be changed to another power state
            LM_ClearWakeUpStatus(&pUmDevice->LmDevice);   // Do nothing
            Status = NDIS_STATUS_NOT_SUPPORTED;
            break;

       case OID_PNP_ADD_WAKE_UP_PATTERN:
            DbgMessage(INFORM, (ADMTEXT("OID_PNP_ADD_WAKE_UP_PATTERN\n")));
            DbgMessage(VERBOSE, (ADMTEXT("Add wakeup pattern = 0x%x, Length = %d\n"),InformationBuffer, InformationBufferLength));
            NdisMoveMemory(pUmDevice->LmDevice.PrivateWakeupBuffer, InformationBuffer, InformationBufferLength);
            LM_AddWakeupPattern(&pUmDevice->LmDevice);
            *BytesRead = InformationBufferLength;
            *BytesNeeded = 0;
            break;

        case OID_PNP_REMOVE_WAKE_UP_PATTERN:
            DbgMessage(INFORM, (ADMTEXT("OID_PNP_REMOVE_WAKE_UP_PATTERN\n")));
            DbgMessage(VERBOSE, (ADMTEXT("Remove wakeup pattern = 0x%x, Length = %d\n"),InformationBuffer, InformationBufferLength));
            NdisMoveMemory(pUmDevice->LmDevice.PrivateWakeupBuffer, InformationBuffer, InformationBufferLength);
            LM_RemoveWakeupPattern(&pUmDevice->LmDevice);
            *BytesRead = InformationBufferLength;
            *BytesNeeded = 0;
            break;

        case OID_PNP_ENABLE_WAKE_UP:

            DbgMessage(INFORM, (ADMTEXT("OID_PNP_ENABLE_WAKE_UP\n")));
            *BytesRead = InformationBufferLength;
            *BytesNeeded = 0;
            break;
#endif
        default:
            Status = NDIS_STATUS_INVALID_OID;
            #ifdef UM_DEBUG_PC
              DbgMessage(FATAL, (ADMTEXT("Unknown Oid\n")));
            #endif
            break;
    } // switch
    #ifdef UM_DEBUG_PC
	DbgMessage(VERBOSE, (ADMTEXT("*** UM_SetInformation end***\n")));
	#endif
    return Status;
} // UM_SetInformation

//******************************************************************************
//******************************************************************************
BOOLEAN UM_CheckForHang( IN  NDIS_HANDLE MiniportAdapterContext )
{
    PUM_DEVICE_INFO_BLOCK   pUmDevice;
    PLM_DEVICE_INFO_BLOCK   pDevice;

    #ifdef UM_DEBUG_PC
     DbgMessage(FATAL, (ADMTEXT("*** UM_CheckForHang ***\n")));
    #endif

    #ifdef UM_DEBUG_CE
      printf("*** UM_CheckForHang ***\n");
    #endif

    pUmDevice = (PUM_DEVICE_INFO_BLOCK) MiniportAdapterContext;
    pDevice = &pUmDevice->LmDevice;
    LM_CheckMedia(&pUmDevice->LmDevice);

    // Always be false
    return FALSE;
} // UM_CheckForHang
//******************************************************************************
// Description:
//    This routine is called when NDIS decides to return packets that were
//    indicated with NdisMIndicateReceivePacket.
//
// Parameters:
//    IN NDIS_HANDLE MiniportAdapterContext     Pointer to DEVICE_INFO_BLOCK
//    IN PNDIS_PACKET Packet
//
// Returns:
//    None.
//******************************************************************************
VOID UM_ReturnPacket( IN  NDIS_HANDLE    MiniportAdapterContext,
                      IN  PNDIS_PACKET   Packet )
{
    //PUM_DEVICE_INFO_BLOCK   pUmDevice;
    //PLM_PACKET_DESC         pLmPacket;
    //PNDIS_PACKET_RESERVED   pReserved;

    #ifdef UM_DEBUG_PC
      //DbgMessage(VERBOSE, (ADMTEXT("*** UM_ReturnPacket ***\n")));
    #endif
    //pUmDevice = (PUM_DEVICE_INFO_BLOCK) MiniportAdapterContext;
    //pReserved = (PNDIS_PACKET_RESERVED) Packet->MiniportReserved;
    //pLmPacket = (PLM_PACKET_DESC) pReserved->LmContext;
    //ListPushTail(&pUmDevice->LmDevice.RxPacketFreeList, &pLmPacket->Link);
} // UM_ReturnPacket

//******************************************************************************
// Description:
//    This is the entry point of the driver. It initializes the characteristics
//    structure and calls NdisInitializeWrapper() and NdisRegisterMac().
//
// Returns:
//    NDIS_STATUS_SUCCESS
//    NDIS_STATUS_FAILURE
//******************************************************************************
NTSTATUS DriverEntry( IN PDRIVER_OBJECT    DriverObject,
                      IN PUNICODE_STRING   RegistryPath )
{
    NDIS_STATUS                     Status;
#ifndef NDISCE_MINIPORT
    NDIS_HANDLE                     NdisWrapper;
#endif
    NDIS_MINIPORT_CHARACTERISTICS   MacCharacteristics;


    #ifdef UM_DEBUG_PC
    DbgMessage(VERBOSE, (ADMTEXT("*** DriverEntry ***\n")));
    #endif
    #ifdef UM_DEBUG_CE
     printf("*** UM DriverEntry ***\n");
    #endif

    //DbgBreak();

    // Initialize the wrapper.
    NdisMInitializeWrapper(&NdisWrapper, DriverObject, RegistryPath, NULL);

    // zero out the MacCharacteristics structure.
    NdisZeroMemory(&MacCharacteristics, sizeof(MacCharacteristics));

    // Initialize the miniport characteristics for the call to NdisMRegisterMiniport.
    MacCharacteristics.MajorNdisVersion        = DRIVER_NDIS_MAJOR_VERSION;
    MacCharacteristics.MinorNdisVersion        = DRIVER_NDIS_MINOR_VERSION;
    MacCharacteristics.HaltHandler             = UM_Halt;
    MacCharacteristics.InitializeHandler       = UM_Initialize;
    MacCharacteristics.QueryInformationHandler = UM_QueryInformation;
    MacCharacteristics.SetInformationHandler   = UM_SetInformation;
    MacCharacteristics.ResetHandler            = UM_Reset;
    MacCharacteristics.SendHandler             = UM_SendPacket;
    //MacCharacteristics.TransferDataHandler     = UM_TransferData;
    //MacCharacteristics.CheckForHangHandler     = UM_CheckForHang; // havhav
    MacCharacteristics.ReturnPacketHandler     = UM_ReturnPacket;

    Status = NdisMRegisterMiniport(NdisWrapper, &MacCharacteristics, sizeof(MacCharacteristics));
    if(Status != NDIS_STATUS_SUCCESS)
    {
        #ifdef UM_DEBUG_PC
          DbgMessage(FATAL, (ADMTEXT("NdisMRegisterMiniport fail status = 0x%x\n"), Status));
        #endif
		NdisTerminateWrapper( NdisWrapper, NULL );
        return NDIS_STATUS_FAILURE;
    }

    //PsGetVersion(NULL,NULL,NULL,NULL);
#ifndef NDISCE_MINIPORT
    LM_HookAddDevice(DriverObject);
#endif

    return NDIS_STATUS_SUCCESS;
} // DriverEntry
