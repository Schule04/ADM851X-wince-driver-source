/******************************************************************************/
/* Module Description: LM.C                                                   */
/******************************************************************************/
#include <windows.h>
#include <ndis.h>
#include "usbdi.h"
#include "lm.h"
#include "mm.h"
#include "um.h"
#include "usbm.h"

extern USB_HANDLE     hUSB;
extern BOOL           NdisDriverLoaded;
extern BOOL           Unplugged;
WORD DebugCount;
HANDLE th = NULL;

//#define LM_DEBUG_CE	1
//#define LM_DEBUG_PC	1
/******************************************************************************/
/* LM_DEVICE_EXTENSION_INFO                                                   */
/******************************************************************************/
typedef struct
{
#ifndef NDISCE_MINIPORT
    PDEVICE_OBJECT                  PhysicalDeviceObject;
    PDEVICE_OBJECT                  FunctionalDeviceObject;
    PDEVICE_OBJECT                  NextDeviceObject;
    CHAR                            NextDeviceStackSize;
    USB_DEVICE_DESCRIPTOR           UsbDeviceDescriptor;
    PUSB_CONFIGURATION_DESCRIPTOR   pUsbConfigurationDesc;
    USBD_CONFIGURATION_HANDLE       UsbdConfigurationHandle;
    PUSBD_INTERFACE_INFORMATION     pUsbdInterface;
#else
    LPUSB_DEVICE                    pUsbDeviceInfo;
    LPCUSB_INTERFACE                pUsbInterface0;
    USB_PIPE                        hPipeBulkIN;
    USB_PIPE                        hPipeBulkOUT;
    //USB_PIPE                        hPipeInterrupt;
#endif
    WORD                            VendorId;
    WORD                            ProductId;
    UCHAR                           InRegisterData[20];
    UCHAR                           OutRegisterData[20];
    UCHAR                           OutRegisterData1[20];
    UCHAR                           OutRegisterData2[20];
} LM_DEVICE_EXTENSION_INFO, * PLM_DEVICE_EXTENSION_INFO;

/******************************************************************************/
/* Function prototypes                                                        */
/******************************************************************************/
STATIC LM_STATUS LM_SetMediaType(PLM_DEVICE_INFO_BLOCK pDevice);
STATIC LM_STATUS LM_AutoSelectMedia(PLM_DEVICE_INFO_BLOCK pDevice);
#ifndef NDISCE_MINIPORT
STATIC LM_STATUS LM_CallUsbd(PLM_DEVICE_INFO_BLOCK pDevice, PURB pUrb);
STATIC LM_STATUS LM_CallUsbdIOCTL(PLM_DEVICE_INFO_BLOCK pDevice, ULONG IOCTL_COMMAND);
STATIC LM_STATUS LM_ConfigureUsb(PLM_DEVICE_INFO_BLOCK pDevice);
STATIC NTSTATUS LM_ReceiveComplete(PDEVICE_OBJECT DeviceObject, PIRP pIrp, PVOID Context);
STATIC NTSTATUS LM_TransmitComplete(PDEVICE_OBJECT DeviceObject, PIRP pIrp, PVOID Context);
#else
STATIC LM_STATUS LM_ConfigureUsbCE(PLM_DEVICE_INFO_BLOCK pDevice);
DWORD CALLBACK LM_ReceiveCompleteCE( LPVOID lpvNotifyParameter );
DWORD CALLBACK LM_TransmitCompleteCE( LPVOID lpvNotifyParameter );
#endif
STATIC LM_STATUS LM_SetPowerState(PLM_DEVICE_INFO_BLOCK pDevice, DWORD PowerState);
STATIC LM_STATUS LM_SetRegister(PLM_DEVICE_INFO_BLOCK pDevice, USHORT  Count, USHORT  Index);
STATIC LM_STATUS LM_GetRegister(PLM_DEVICE_INFO_BLOCK pDevice, USHORT  Count, USHORT  Index);

STATIC LM_STATUS LM_ProcessSROMData(PLM_DEVICE_INFO_BLOCK pDevice);
STATIC USHORT    LM_ReadROMWord(PLM_DEVICE_INFO_BLOCK pDevice, UCHAR index);
STATIC LM_STATUS LM_WriteROMWord(PLM_DEVICE_INFO_BLOCK pDevice, UCHAR index, UCHAR lowbyte, UCHAR highbyte);
/******************************************************************************/
void DumpHex(PUCHAR p, DWORD dwlen)
{
        int i,j;
        int len = dwlen;
#if 0
        for(i = 0;i<len/16; i++){
                for(j = 0;j<16;j++){
                        NKDbgPrintfW(TEXT("%0.2X "),p[i*16 + j]);
                }
                NKDbgPrintfW(TEXT(" |"));
                for(j = 0;j<16;j++){
                        NKDbgPrintfW(TEXT("%c"),((p[i*16 + j]<0x20)|| (p[i*16+j]&0x80))?'.':p[i*16+j]);
                }
                NKDbgPrintfW(TEXT("|\r\n"));
        }
        for(j = 0;j<len%16;j++){
                NKDbgPrintfW(TEXT("%0.2X "),p[i*16 + j]);
        }
        for(j=0; j < 16 -(len%16);j++){
                        NKDbgPrintfW(TEXT("   "),p[i*16 + j]);
        }
        NKDbgPrintfW(TEXT(" |"));
        for(j = 0;j<len%16;j++){
                        NKDbgPrintfW(TEXT("%c"),((p[i*16 + j]<0x20)||(p[i*16+j]&0x80))?'.':p[i*16+j]);
        }
        for(j=0; j < 16 -(len%16);j++){
                        NKDbgPrintfW(TEXT(" "),p[i*16 + j]);
        }
        NKDbgPrintfW(TEXT("|\r\n"));
#else
        for(i = 0;i<len/16; i++){
                for(j = 0;j<16;j++){
                        printf("%0.2X ",p[i*16 + j]);
                }
                printf(" |");
                for(j = 0;j<16;j++){
                        printf("%c",((p[i*16 + j]<0x20)|| (p[i*16+j]&0x80))?'.':p[i*16+j]);
                }
                printf("|\n");
        }
        for(j = 0;j<len%16;j++){
                printf("%0.2X ",p[i*16 + j]);
        }
        for(j=0; j < 16 -(len%16);j++){
                        printf("   ",p[i*16 + j]);
        }
        printf(" |");
        for(j = 0;j<len%16;j++){
                        printf("%c",((p[i*16 + j]<0x20)||(p[i*16+j]&0x80))?'.':p[i*16+j]);
        }
        for(j=0; j < 16 -(len%16);j++){
                        printf(" ",p[i*16 + j]);
        }
        printf("|\n");
#endif
}

void tkdump(PBYTE p, DWORD len)
{
//      int i;
        printf("tkdump %X, len = %x[%x] %x\n",p,len, *(USHORT UNALIGNED*)(p+len-4), *(USHORT UNALIGNED*)(p+len-2));
        if(len >= 12){
                if(     p[0] == 0xff &&
                        p[1] == 0xff &&
                        p[2] == 0xff &&
                        p[3] == 0xff &&
                        p[4] == 0xff &&
                        p[5] == 0xff )printf("broadcast from %X.%X.%X.%X.%X.%X\n",p[6],p[7],p[8],p[9],p[10],p[11]);

        }
        if(len >= 0x58){
                if(     p[0xc] == 0x08 &&
                        p[0xd] == 0x00 ){
                        printf("IP\n");
                        if(     p[0x22] == 0x00 && p[0x24] == 0x00 &&
                                p[0x23] == 0x89 && p[0x25] == 0x89 ){
                                printf("NetBIOS-ns:[%c%c%c%c%c%c%c%c%c%c%c%c%c%c%c%c]\n",
(((p[0x37] - 'A')<<4)&0xf0 | ((p[0x38] - 'A'))&0x0f),
(((p[0x39] - 'A')<<4)&0xf0 | ((p[0x3a] - 'A'))&0x0f),
(((p[0x3b] - 'A')<<4)&0xf0 | ((p[0x3c] - 'A'))&0x0f),
(((p[0x3d] - 'A')<<4)&0xf0 | ((p[0x3e] - 'A'))&0x0f),
(((p[0x3f] - 'A')<<4)&0xf0 | ((p[0x40] - 'A'))&0x0f),
(((p[0x41] - 'A')<<4)&0xf0 | ((p[0x42] - 'A'))&0x0f),
(((p[0x43] - 'A')<<4)&0xf0 | ((p[0x44] - 'A'))&0x0f),
(((p[0x45] - 'A')<<4)&0xf0 | ((p[0x46] - 'A'))&0x0f),

(((p[0x47] - 'A')<<4)&0xf0 | ((p[0x48] - 'A'))&0x0f),
(((p[0x49] - 'A')<<4)&0xf0 | ((p[0x4a] - 'A'))&0x0f),
(((p[0x4b] - 'A')<<4)&0xf0 | ((p[0x4c] - 'A'))&0x0f),
(((p[0x4d] - 'A')<<4)&0xf0 | ((p[0x4e] - 'A'))&0x0f),
(((p[0x4f] - 'A')<<4)&0xf0 | ((p[0x50] - 'A'))&0x0f),
(((p[0x51] - 'A')<<4)&0xf0 | ((p[0x52] - 'A'))&0x0f),
(((p[0x53] - 'A')<<4)&0xf0 | ((p[0x54] - 'A'))&0x0f),
(((p[0x55] - 'A')<<4)&0xf0 | ((p[0x56] - 'A'))&0x0f)
);
                        }
                        if(     p[0x22] == 0x00 && p[0x24] == 0x00 &&
                                p[0x23] == 0x8a && p[0x25] == 0x8a ){
                                printf("NetBIOS-dgm:[%c%c%c%c%c%c%c%c%c%c%c%c%c%c%c%c]\n",
(((p[0x39] - 'A')<<4)&0xf0 | ((p[0x3a] - 'A'))&0x0f),
(((p[0x3b] - 'A')<<4)&0xf0 | ((p[0x3c] - 'A'))&0x0f),
(((p[0x3d] - 'A')<<4)&0xf0 | ((p[0x3e] - 'A'))&0x0f),
(((p[0x3f] - 'A')<<4)&0xf0 | ((p[0x40] - 'A'))&0x0f),
(((p[0x41] - 'A')<<4)&0xf0 | ((p[0x42] - 'A'))&0x0f),
(((p[0x43] - 'A')<<4)&0xf0 | ((p[0x44] - 'A'))&0x0f),
(((p[0x45] - 'A')<<4)&0xf0 | ((p[0x46] - 'A'))&0x0f),

(((p[0x47] - 'A')<<4)&0xf0 | ((p[0x48] - 'A'))&0x0f),
(((p[0x49] - 'A')<<4)&0xf0 | ((p[0x4a] - 'A'))&0x0f),
(((p[0x4b] - 'A')<<4)&0xf0 | ((p[0x4c] - 'A'))&0x0f),
(((p[0x4d] - 'A')<<4)&0xf0 | ((p[0x4e] - 'A'))&0x0f),
(((p[0x4f] - 'A')<<4)&0xf0 | ((p[0x50] - 'A'))&0x0f),
(((p[0x51] - 'A')<<4)&0xf0 | ((p[0x52] - 'A'))&0x0f),
(((p[0x53] - 'A')<<4)&0xf0 | ((p[0x54] - 'A'))&0x0f),
(((p[0x55] - 'A')<<4)&0xf0 | ((p[0x56] - 'A'))&0x0f),
(((p[0x57] - 'A')<<4)&0xf0 | ((p[0x58] - 'A'))&0x0f)

);
                        }

                }

        }
//      DumpHex(p,len);
}

LM_STATUS LM_WriteMii( PLM_DEVICE_INFO_BLOCK   pDevice,
                  BYTE                    PhyAddr,
                  BYTE                    PhyRegsIndex,
                  WORD                    Data )
{
    PLM_DEVICE_EXTENSION_INFO pExtension;
    LM_STATUS Status;
    WORD Count;

    #ifdef LM_DEBUG_PC
      DbgMessage(VERBOSE, (ADMTEXT("*** LM_WriteMii : PhyAddr=%02x, PhyRegsIndex=%02x, Data=%04x\n"), PhyAddr, PhyRegsIndex, Data));
    #endif
//  printf("LM_Write MII\n");
    pExtension = (PLM_DEVICE_EXTENSION_INFO) pDevice->pLmDeviceExtension;
    Count = 0;
    pExtension->OutRegisterData[0]=PhyAddr;
    pExtension->OutRegisterData[1]=Data & 0xff;
    pExtension->OutRegisterData[2]=(Data & 0xff00) >> 8;
    pExtension->OutRegisterData[3]=PhyRegsIndex | 0x20;
    Status = LM_SetRegister (pDevice, 4, 0x25 );
	if( Status != LM_STATUS_SUCCESS){
		goto lExit;
	}
    do
    {
        Status = LM_GetRegister (pDevice, 2, 0x28 );
	if( Status != LM_STATUS_SUCCESS){
		goto lExit;
	}
        MM_InitWait(100);
        Count ++;
        if ( Count >300)
            pExtension->InRegisterData[0] |= 0x80;   //force done to avoid infinity loop
    } while (!(pExtension->InRegisterData[0] & 0x80));
//    pExtension->OutRegisterData[0]=0;
//    pExtension->OutRegisterData[1]=0;
//    Status = LM_SetRegister (pDevice, 2, 0x28);

	Status = LM_STATUS_SUCCESS;

lExit:
	return Status;
} /* LM_WriteMii */

/******************************************************************************/
/******************************************************************************/
LM_STATUS LM_ReadMii( PLM_DEVICE_INFO_BLOCK   pDevice,
                 BYTE                    PhyAddr,
                 BYTE                    PhyRegsIndex,
                 WORD *                  pData )
{
    PLM_DEVICE_EXTENSION_INFO   pExtension;
    WORD                        TempData=0,Count;
    LM_STATUS                   Status;

    #ifdef LM_DEBUG_PC
      DbgMessage(VERBOSE, (ADMTEXT("LM_ReadMii : PhyAddr=%02x, PhyRegsIndex=%02x\n"), PhyAddr, PhyRegsIndex));
    #endif
    pExtension = (PLM_DEVICE_EXTENSION_INFO) pDevice->pLmDeviceExtension;
    Count = 0;
    pExtension->OutRegisterData[0]=PhyAddr;
    pExtension->OutRegisterData[1]= 0;
    pExtension->OutRegisterData[2]= 0;
    pExtension->OutRegisterData[3]= PhyRegsIndex | 0x40;
    Status = LM_SetRegister (pDevice, 4, 0x25  );
	if( Status != LM_STATUS_SUCCESS){
		goto lExit;
	}
    do
    {
        Status = LM_GetRegister (pDevice, 4, 0x25 );
		if( Status != LM_STATUS_SUCCESS){
			goto lExit;
		}
        MM_InitWait(100);
        Count ++;
        if ( Count >300)
            pExtension->InRegisterData[3] |= 0x80;   //force done to avoid infinity loop
    } while (!(pExtension->InRegisterData[3] & 0x80));

    TempData = (WORD) pExtension->InRegisterData[2];
    TempData = (TempData << 8)+(WORD) pExtension->InRegisterData[1];

    #ifdef LM_DEBUG_PC
      DbgMessage(VERBOSE, (ADMTEXT("Data=%04X\n"), TempData));
    #endif
    *pData = TempData;

	Status = LM_STATUS_SUCCESS;

lExit:
	return Status;
} /* LM_ReadMii */

/******************************************************************************/
/* Description:                                                               */
/*    Gets and sets up USB configurations.                                    */
/*                                                                            */
/* Returns:                                                                   */
/*    LM_STATUS_SUCCESS                                                       */
/*    LM_STATUS_FAILURE                                                       */
/******************************************************************************/
#ifndef NDISCE_MINIPORT
STATIC LM_STATUS LM_ConfigureUsb( PLM_DEVICE_INFO_BLOCK pDevice )
{
    PLM_DEVICE_EXTENSION_INFO         pExtension;
    PUSB_DEVICE_DESCRIPTOR            pUsbDeviceDesc;
    USB_CONFIGURATION_DESCRIPTOR      UsbConfigurationDesc;
    PUSB_CONFIGURATION_DESCRIPTOR     pUsbConfigurationDesc;
    PUSB_INTERFACE_DESCRIPTOR         pUsbInterfaceDescriptor;
    PUSB_STRING_DESCRIPTOR            pUsbStringDesc;
    WCHAR                             SerialStringBuffer[16];
    LM_STATUS                         Status;
    ULONG                             j;
    WCHAR                             Digit;
    PUSBD_INTERFACE_INFORMATION       pUsbdInterface;
    USBD_INTERFACE_LIST_ENTRY         UsbdInterfaceList[2];
    URB                               Urb;
    PURB                              pUrb;

    #ifdef LM_DEBUG_PC
      DbgMessage(INFORM, (ADMTEXT("*** LM_ConfigureUsb Start ***\n")));
    #endif

    #ifdef LM_DEBUG_CE
      printf("*** LM_ConfigureUsb Start ***\n");
    #endif

    pExtension = (PLM_DEVICE_EXTENSION_INFO) pDevice->pLmDeviceExtension;

    /* Get device objects. */
    MM_GetDeviceObjects( pDevice,
                         &pExtension->PhysicalDeviceObject,
                         &pExtension->FunctionalDeviceObject,
                         &pExtension->NextDeviceObject );
    #ifdef LM_DEBUG_PC
      DbgMessage(INFORM, (ADMTEXT("PhysicalDeviceObject = 0x%x\n"), pExtension->PhysicalDeviceObject));
      DbgMessage(INFORM, (ADMTEXT("FunctionalDeviceObject = 0x%x\n"), pExtension->FunctionalDeviceObject));
      DbgMessage(INFORM, (ADMTEXT("NextDeviceObject = 0x%x\n"), pExtension->NextDeviceObject));
    #endif

    pExtension->NextDeviceStackSize = (CHAR)pExtension->NextDeviceObject->StackSize + 1;

    /* Build a Get Device Descriptor request. */
    pUsbDeviceDesc = &pExtension->UsbDeviceDescriptor;
    UsbBuildGetDescriptorRequest( &Urb,
                                  sizeof(struct _URB_CONTROL_DESCRIPTOR_REQUEST),
                                  USB_DEVICE_DESCRIPTOR_TYPE,
                                  0,
                                  0,
                                  pUsbDeviceDesc,
                                  NULL,
                                  sizeof(USB_DEVICE_DESCRIPTOR),
                                  NULL );
    Status = LM_CallUsbd(pDevice, &Urb);
    if(Status != LM_STATUS_SUCCESS)   return LM_STATUS_FAILURE;

    DbgMessage(INFORM, (ADMTEXT("Device Descriptor :\n")));
    DbgMessage(INFORM, (ADMTEXT("bLength    %d\n"),     pUsbDeviceDesc->bLength));
    DbgMessage(INFORM, (ADMTEXT("bDescType  %d\n"),     pUsbDeviceDesc->bDescriptorType));
    DbgMessage(INFORM, (ADMTEXT("bcdUSB     0x%04x\n"), pUsbDeviceDesc->bcdUSB));
    DbgMessage(INFORM, (ADMTEXT("bClass     %d\n"),     pUsbDeviceDesc->bDeviceClass));
    DbgMessage(INFORM, (ADMTEXT("bSubClass  %d\n"),     pUsbDeviceDesc->bDeviceSubClass));
    DbgMessage(INFORM, (ADMTEXT("bProtocol  %d\n"),     pUsbDeviceDesc->bDeviceProtocol));
    DbgMessage(INFORM, (ADMTEXT("bPcktSize0 %d\n"),     pUsbDeviceDesc->bMaxPacketSize0));
    DbgMessage(INFORM, (ADMTEXT("idVendor   0x%04x\n"), pUsbDeviceDesc->idVendor));
    DbgMessage(INFORM, (ADMTEXT("idProduct  0x%04x\n"), pUsbDeviceDesc->idProduct));
    DbgMessage(INFORM, (ADMTEXT("bcdDevice  0x%04x\n"), pUsbDeviceDesc->bcdDevice));
    DbgMessage(INFORM, (ADMTEXT("iManuf     %d\n"),     pUsbDeviceDesc->iManufacturer));
    DbgMessage(INFORM, (ADMTEXT("iProduct   %d\n"),     pUsbDeviceDesc->iProduct));
    DbgMessage(INFORM, (ADMTEXT("iSerial    %d\n"),     pUsbDeviceDesc->iSerialNumber));
    DbgMessage(INFORM, (ADMTEXT("bNumCfg    %d\n"),     pUsbDeviceDesc->bNumConfigurations));

    /* Save device information. */
    pExtension->VendorId = pUsbDeviceDesc->idVendor;
    pExtension->ProductId = pUsbDeviceDesc->idProduct;

    if((pExtension->VendorId == 0) && (pExtension->ProductId == 0) && (pUsbDeviceDesc->bLength ==0))
    {
        // Reset USB port LIC add it (6.9.99)770ED
        //MM_InitWait(10000000);
        LM_CallUsbdIOCTL(pDevice, IOCTL_INTERNAL_USB_CYCLE_PORT);   // simulate a plug/unplug
        //------------------
        return LM_STATUS_FAILURE;
    }

    /* When USB_CONFIGURATION_DESCRIPTOR_TYPE is specified for DescriptorType */
    /* in a call to UsbBuildGetDescriptorRequest(), all interface, endpoint, */
    /* class-specific, and vendor-specific descriptors for the configuration */
    /* also are retrieved.  The caller must allocate a buffer large enough to */
    /* hold all of this information or the data is truncated without error. */
    /* Therefore we pass in a short buffer to get the real size and then */
    /* allocate a buffer with the correct size. */

    /* Allocating memory for all the configuration descriptors. */
    Status = MM_AllocateMemory(pDevice, &pUsbConfigurationDesc, 0xff);
    if(Status != LM_STATUS_SUCCESS)   return LM_STATUS_FAILURE;

    pExtension->pUsbConfigurationDesc = pUsbConfigurationDesc;

    /* Now get all the descriptors. */
    UsbBuildGetDescriptorRequest( &Urb,
                                  sizeof(struct _URB_CONTROL_DESCRIPTOR_REQUEST),
                                  USB_CONFIGURATION_DESCRIPTOR_TYPE,
                                  0,
                                  0,
                                  pUsbConfigurationDesc,
                                  NULL,
                                  0xff,
                                  NULL );
    Status = LM_CallUsbd(pDevice, &Urb);
    if(Status != LM_STATUS_SUCCESS)   return LM_STATUS_FAILURE;

    /* USBD_ParseConfigurationDescriptorEx searches a given configuration */
    /* descriptor and returns a pointer to an interface that matches the */
    /* given search criteria. We only support one interface on this device. */
    pUsbInterfaceDescriptor = USBD_ParseConfigurationDescriptorEx( pUsbConfigurationDesc,
                                                                   pUsbConfigurationDesc,
                                                                   -1,
                                                                   -1,
                                                                   -1,
                                                                   -1,
                                                                   -1 );
    if(pUsbInterfaceDescriptor == NULL)
    {
        DbgMessage(FATAL, (ADMTEXT("USBD_ParseConfigurationDescriptorEx : Can't found any interface\n")));
        return LM_STATUS_FAILURE;
    } /* if */

    /* Setup the interface list to pass to USBD_CreateConfigurationRequestEx. */
    UsbdInterfaceList[0].InterfaceDescriptor = pUsbInterfaceDescriptor;
    UsbdInterfaceList[0].Interface = NULL;
    UsbdInterfaceList[1].InterfaceDescriptor = NULL;
    UsbdInterfaceList[1].Interface = NULL;

    /* Create a configuration request URB. */
    pUrb = USBD_CreateConfigurationRequestEx( pUsbConfigurationDesc, UsbdInterfaceList );
    if(pUrb == NULL)
    {
        DbgMessage(FATAL, (ADMTEXT("USBD_CreateConfigurationRequestEx : Cannot get a configuration URB\n")));
        return LM_STATUS_FAILURE;
    } /* if */

    /* Get the pointer to the USBD interface information. */
    pUsbdInterface = UsbdInterfaceList[0].Interface;

    if(!pUsbdInterface)
    {
        DbgMessage(FATAL, (ADMTEXT("Can't find any interface in Interface Descriptor\n")));
        return LM_STATUS_FAILURE;
    } /* if */
    if(pUsbdInterface->NumberOfPipes != 3)
    {
        DbgMessage(FATAL, (ADMTEXT("Can't find 3 pipes in Interface Descripter\n")));
        return LM_STATUS_FAILURE;
    } /* if */

    /* Configure maximum transfer size of each pipe. */
    pUsbdInterface->Pipes[0].MaximumTransferSize = MAX_PACKET_BUFFER_SIZE;   // 0x600, Ethernet Packet Size
    pUsbdInterface->Pipes[0].PipeFlags = 0;
    pUsbdInterface->Pipes[1].MaximumTransferSize = MAX_PACKET_BUFFER_SIZE;   // 0x600
    pUsbdInterface->Pipes[1].PipeFlags = 0;

    /* Build a select configuration request. */
    UsbBuildSelectConfigurationRequest( pUrb,
                                        GET_SELECT_CONFIGURATION_REQUEST_SIZE(1, 3),
                                        pUsbConfigurationDesc );
    Status = LM_CallUsbd(pDevice, pUrb);
    if(Status != LM_STATUS_SUCCESS)
    {
        ExFreePool(pUrb);
        return LM_STATUS_FAILURE;
    } /* if */

    if ( (pUsbdInterface->Pipes[0].PipeType != 2) |
         (pUsbdInterface->Pipes[1].PipeType != 2) |
         (pUsbdInterface->Pipes[2].PipeType != 3) )
    {
       ExFreePool(pUrb);
       return LM_STATUS_FAILURE;
    }

    pExtension->UsbdConfigurationHandle = pUrb->UrbSelectConfiguration.ConfigurationHandle;   // not used
    Status = MM_AllocateMemory(pDevice, &pExtension->pUsbdInterface, pUsbdInterface->Length);
    if(Status != LM_STATUS_SUCCESS)
    {
        ExFreePool(pUrb);
        return LM_STATUS_FAILURE;
    } /* if */

    /* save a copy of the interface information returned. */
    RtlCopyMemory(pExtension->pUsbdInterface, pUsbdInterface, pUsbdInterface->Length);
    ExFreePool(pUrb);

    UsbBuildFeatureRequest( &Urb,
                            URB_FUNCTION_CLEAR_FEATURE_TO_DEVICE,   //Clear Feature
                            0x1,                                    //Disable remote wake up
                            0,                                      //index =0
                            NULL );
    Status = LM_CallUsbd(pDevice, &Urb);
    if(Status != LM_STATUS_SUCCESS)   return LM_STATUS_FAILURE;

    UsbBuildGetStatusRequest( &Urb,
                              URB_FUNCTION_GET_STATUS_FROM_DEVICE,   //Get Status from Device
                              0,                                     //index
                              pExtension->InRegisterData,            //Data Buffer
                              NULL,                                  //index =0
                              NULL );
    Status = LM_CallUsbd(pDevice, &Urb);
    if(Status != LM_STATUS_SUCCESS)   return LM_STATUS_FAILURE;

    DbgMessage(FATAL,(ADMTEXT("Device Status : [0]=%02x, [1]=%02x \n"),pExtension->InRegisterData[0],pExtension->InRegisterData[1]));
    DbgMessage(INFORM, (ADMTEXT("*** LM_ConfigureUsb End ***\n")));
    return LM_STATUS_SUCCESS;
} /* LM_ConfigureUsb */
#else

STATIC LM_STATUS LM_ConfigureUsbCE( PLM_DEVICE_INFO_BLOCK pDevice )
{
    PLM_DEVICE_EXTENSION_INFO         pExtension;
    LPUSB_DEVICE                      pUsbDeviceInfo;
    LPCUSB_INTERFACE                  pUsbInterface;
    LM_STATUS                         result;


    #ifdef LM_DEBUG_PC
      DbgMessage(INFORM, (ADMTEXT("*** LM_ConfigureUsbCE Start ***\n")));
    #endif

    #ifdef LM_DEBUG_CE
    printf("*** LM_ConfigureUsbCE Start ***\n");
    #endif

    pExtension = (PLM_DEVICE_EXTENSION_INFO) pDevice->pLmDeviceExtension;
    result = USBM_GetDeviceInfo( hUSB, &pUsbDeviceInfo );
    if(result != LM_STATUS_SUCCESS){ goto lExit;}
    pExtension->pUsbDeviceInfo = pUsbDeviceInfo;
    pExtension->VendorId = pUsbDeviceInfo->Descriptor.idVendor;
    pExtension->ProductId = pUsbDeviceInfo->Descriptor.idProduct;

    result = USBM_FindInterface( pUsbDeviceInfo, 0, 0, &pUsbInterface );
    if(result != LM_STATUS_SUCCESS){ goto lExit;}
    if( pUsbInterface->Descriptor.bNumEndpoints != 3 ){
		result = LM_STATUS_FAILURE;
    	goto lExit;
    }
    pExtension->pUsbInterface0 = pUsbInterface;

    result = USBM_ClearFeature( hUSB, USB_SEND_TO_DEVICE, USB_FEATURE_REMOTE_WAKEUP, 0 );
    if(result != LM_STATUS_SUCCESS){  goto lExit;}
    result = USBM_OpenPipe( hUSB, &(pUsbInterface->lpEndpoints[0].Descriptor), &(pExtension->hPipeBulkIN) );
    if(result != LM_STATUS_SUCCESS){  goto lExit;}
    result = USBM_OpenPipe( hUSB, &(pUsbInterface->lpEndpoints[1].Descriptor), &(pExtension->hPipeBulkOUT) );
    if(result != LM_STATUS_SUCCESS){  goto lExit;}
    //result = USBM_OpenPipe( hUSB, &(pUsbInterface->lpEndpoints[2].Descriptor), &(pExtension->hPipeInterrupt) );
    //if(result != LM_STATUS_SUCCESS)   return result;

    pDevice->ChipType = 0;   //adm8511 986

    #ifdef LM_DEBUG_CE
    	printf("bcdDevice = %x \n",pUsbDeviceInfo->Descriptor.bcdDevice);
    #endif

	if(pUsbDeviceInfo->Descriptor.bcdDevice >= 0x200 )
	{
		pDevice->ChipType = 1;  //adm8513
	}

    #ifdef LM_DEBUG_PC
      DbgMessage(INFORM, (ADMTEXT("*** LM_ConfigureUsbCE End ***\n")));
    #endif

lExit:
//	if (result != LM_STATUS_SUCCESS)
//		MM_FreeResources(pDevice);
    return result;
}
#endif

/******************************************************************************/
/* Description:                                                               */
/*    Read SROM data in word                                                  */
/*                                                                            */
/* Returns:                                                                   */
/*    LM_STATUS_SUCCESS                                                       */
/*    LM_STATUS_FAILURE                                                       */
/******************************************************************************/
STATIC USHORT LM_ReadROMWord( PLM_DEVICE_INFO_BLOCK   pDevice,
                              UCHAR                   index )
{

    PLM_DEVICE_EXTENSION_INFO   pExtension;
    USHORT                      i;
    USHORT                      Data;
    LM_STATUS                   Status;

    pExtension = (PLM_DEVICE_EXTENSION_INFO) pDevice->pLmDeviceExtension;

    pExtension->OutRegisterData[0]=index;   // Register Index : 0x20
    pExtension->OutRegisterData[1]=0x00;    // Register Index : 0x21
    pExtension->OutRegisterData[2]=0x00;    // Register Index : 0x22
    pExtension->OutRegisterData[3]=0x02;    // Register Index : 0x23
    Status = LM_SetRegister (pDevice, 4, 0x20 );

    for (i=0;i<100;i++)
    {
         MM_InitWait(500);
         Status = LM_GetRegister (pDevice, 2, 0x23 );
         if ((pExtension->InRegisterData[0]) & 0x04)
            break;
    }

    Status = LM_GetRegister (pDevice, 3, 0x21 );
    Data = (pExtension->InRegisterData[0])+(pExtension->InRegisterData[1]<<8);

    return Data;
}

/******************************************************************************/
/* Description:                                                               */
/*    Write SROM data in word                                                 */
/*                                                                            */
/* Returns:                                                                   */
/*    LM_STATUS_SUCCESS                                                       */
/*    LM_STATUS_FAILURE                                                       */
/******************************************************************************/
STATIC LM_STATUS LM_WriteROMWord( PLM_DEVICE_INFO_BLOCK pDevice,
                                  UCHAR index,
                                  UCHAR lowbyte,
                                  UCHAR highbyte )
{
    PLM_DEVICE_EXTENSION_INFO pExtension;
    USHORT      i;
    LM_STATUS   Status;

    pExtension = (PLM_DEVICE_EXTENSION_INFO) pDevice->pLmDeviceExtension;

    pExtension->OutRegisterData[0]=index;
    pExtension->OutRegisterData[1]=lowbyte;
    pExtension->OutRegisterData[2]=highbyte;
    pExtension->OutRegisterData[3]=0x01;
    Status = LM_SetRegister (pDevice, 4, 0x20 );

    for (i=0;i<100;i++) {
         MM_InitWait(500);
         Status = LM_GetRegister (pDevice, 2, 0x23 );
         if ((pExtension->InRegisterData[0]) & 0x04) {
            break;
         }
    }
    return LM_STATUS_SUCCESS;
}

/******************************************************************************/
/* Description:                                                               */
/*    Gets the SROM data from 93LC46                                          */
/*    Read Node Address from SROM                                             */
/* Returns:                                                                   */
/*    LM_STATUS_SUCCESS                                                       */
/*    LM_STATUS_FAILURE                                                       */
/******************************************************************************/
STATIC LM_STATUS LM_ProcessSROMData( PLM_DEVICE_INFO_BLOCK pDevice )
{
    PLM_DEVICE_EXTENSION_INFO   pExtension;
    USHORT                      Data;
    UCHAR                       i/*, j*/;

    UCHAR                       temp_data [0x80];

    #ifdef LM_DEBUG_CE
      printf("** LM_ProcessSROMData ** \n");
    #endif

    pExtension = (PLM_DEVICE_EXTENSION_INFO) pDevice->pLmDeviceExtension;


    for ( i=0; i<0x10; i++ )
    {
        Data = LM_ReadROMWord (pDevice, i);
        temp_data[2*i]= (UCHAR) (Data & 0xff);
        temp_data[2*i+1]= (UCHAR) (Data >> 8);
    }

    #ifdef LM_DEBUG_PC
      DbgMessage(VERBOSE, (ADMTEXT("SROM Data\n")));
    #endif
    for (i=0x0;i<0x20;i++)
    {
        #ifdef LM_DEBUG_PC
          DbgMessage(VERBOSE, (ADMTEXT("[%x] %02x\n"), i, temp_data[i]));
        #endif
    }

    // Fill permanent node address
    #ifdef LM_DEBUG_CE
     printf("Node ID:");
    #endif
    for (i=0;i<6;i++)
    {
          pDevice->PermanentNodeAddress[i] =temp_data[i];
          #ifdef LM_DEBUG_CE
           printf("%x=",temp_data[i]);
          #endif
    }
    #ifdef LM_DEBUG_CE
     printf("\n");
    #endif
    return LM_STATUS_SUCCESS;
}

/******************************************************************************/
/* Description:                                                               */
/*    This routine check media type                                           */
/*                                                                            */
/* Returns:                                                                   */
/*    NONE                                                                    */
/******************************************************************************/
VOID LM_CheckMedia( PLM_DEVICE_INFO_BLOCK pDevice )
{
    PLM_DEVICE_EXTENSION_INFO   pExtension;

    #ifdef LM_DEBUG_PC
      DbgMessage(INFORM, (ADMTEXT("*** LM_CheckMedia ***\n")));
    #endif
    if(pDevice->AdapterStatus != LM_STATUS_ADAPTER_OPEN)
        return ;

    pExtension = (PLM_DEVICE_EXTENSION_INFO) pDevice->pLmDeviceExtension;
    if (pDevice->SetState == 0)
    {
        pDevice->SetState = 1;
        if (pDevice->CurrMode == ETHERNET)
        {
            pExtension->OutRegisterData2[0]=  0x1;
            pExtension->OutRegisterData2[3]= 0x45;   // 0x41 ???
        }
        else
        {
            pExtension->OutRegisterData2[0]= 0x2;
            pExtension->OutRegisterData2[3]=0x41;
        }
        pDevice->WorkState = 0x12;
        LM_SetRegisterAsyn(pDevice, 4, 0x25, pExtension->OutRegisterData2);
    }
} /* LM_CheckReg */

PLM_PACKET_DESC           m_pPacket;
PLM_DEVICE_INFO_BLOCK m_pDevice;
HANDLE resetEvent=INVALID_HANDLE_VALUE;

int recvThread(PLM_DEVICE_INFO_BLOCK unUsed)
{
PLM_PACKET_DESC           pPacket;
PLM_DEVICE_INFO_BLOCK pDevice;
   //   NKDbgPrintfW(TEXT("recvThread Start\r\n"));

        #ifdef LM_DEBUG_CE
          printf("recvThread Start\n");
	    #endif

        while(!Unplugged){
        WaitForSingleObject(resetEvent,INFINITE);
		if (Unplugged)
			break;
        pPacket=m_pPacket;
        pDevice=m_pDevice;

        #ifdef LM_DEBUG_CE
          printf("pPacket->PacketSize= %d\n",pPacket->PacketSize);
        #endif
 
        #ifdef DEBCE // mod - 06/06/05,06/03/05,bolo
        // mark - 09/05/05,bolo
        // printf("RTHD: pkt=0x%x,len=%d,RxFree=%d,RxPkt=%d\n",pPacket,pPacket->PacketSize,LL_GetSize(&pDevice->RxPacketFreeList),LL_GetSize(&pDevice->RxPacketReceivedList)); // add - 05/26/05,bolo
        NKDbgPrintfW(TEXT("RTHD: pkt=0x%x,len=%d,RxFree=%d,RxPkt=%d\n\r"),pPacket,pPacket->PacketSize,LL_GetSize(&pDevice->RxPacketFreeList),LL_GetSize(&pDevice->RxPacketReceivedList)); // add - 09/05/05,bolo   
        #endif

        if(pPacket->PacketSize){
        // Set timer to wait for indication routine & issue another Bulk transfer
//              NKDbgPrintfW(TEXT("recvThread: call MM_IndicateRxPacket\r\n"));
                #ifdef LM_DEBUG_CE
                  printf("recvThread: call MM_IndicateRxPacket\n");
                #endif     
                #ifdef DEBCE // add - 06/03/05,bolo           
                // mark - 09/05/05,bolo
                // printf("RTHD: MIRP\n"); // add - 05/26/05,bolo
                NKDbgPrintfW(TEXT("RTHD: MIRP\n\r")); // add - 09/05/05,bolo   
                #endif
                MM_IndicateRxPacket(pDevice, pPacket);
//              NKDbgPrintfW(TEXT("recvThread: return from MM_IndicateRxPacket\r\n"));
        }else{
        // Issue another Bulk IN transfer to wait Rx data with original packet buffer
//              NKDbgPrintfW(TEXT("recvThread: call LM_QueueRxPacket\r\n"));
                //MM_InitWait(1000);  //jackl add 11.21.2001 advoid UM_QueryInformation and LM_QueueRxPacket happen
                #ifdef LM_DEBUG_CE
                  printf("recvThread: call LM_QueueRxPacket\n");
                #endif
                // DbgMessage(VERBOSE, (TEXT("recvThread: LQRP !\n"))); // mark - 06/22/05,06/09/05,bolo
                #ifdef DEBCE // add - 06/09/05,bolo  
                // mark - 09/05/05,bolo
                // printf("RTHD: LQRP !, RxFree=%d, RxPkt=%d\n", LL_GetSize(&pDevice->RxPacketFreeList), LL_GetSize(&pDevice->RxPacketReceivedList)); // add - 06/03/05,bolo
                NKDbgPrintfW(TEXT("RTHD: LQRP !, RxFree=%d, RxPkt=%d\n\r"), LL_GetSize(&pDevice->RxPacketFreeList), LL_GetSize(&pDevice->RxPacketReceivedList)); // add - 09/05/05,bolo 
                #endif
                /* mark - 06/01/05,bolo
                LM_QueueRxPacket(pDevice, pPacket);
                */

                if(LM_STATUS_SUCCESS != LM_QueueRxPacket(pDevice, pPacket)) { // add - 06/01/05,bolo

                    // DbgMessage(VERBOSE, (TEXT("recvThread: LQRP Failed !\n"))); // mark - 06/22/05,06/03/05,bolo

                    #ifdef DEBERR // add - 06/22/05,bolo
                        // mark - 09/05/05,bolo
                    	// printf("RTHD: LQRP Failed !, RxFree=%d, RxPkt=%d\n", LL_GetSize(&pDevice->RxPacketFreeList), LL_GetSize(&pDevice->RxPacketReceivedList)); // add - 06/03/05,bolo
	                    NKDbgPrintfW(TEXT("RTHD: LQRP Failed !, RxFree=%d, RxPkt=%d\n\r"), LL_GetSize(&pDevice->RxPacketFreeList), LL_GetSize(&pDevice->RxPacketReceivedList)); // add - 09/05/05,bolo
                    #endif

                    LL_PushTail(&pDevice->RxPacketFreeList, &pPacket->Link);
                }        

//      NKDbgPrintfW(TEXT("recvThread: return from LM_QueueRxPacket\r\n"));
        }
        }
        if(resetEvent!=INVALID_HANDLE_VALUE)CloseHandle(resetEvent);
        resetEvent = INVALID_HANDLE_VALUE;
   //   NKDbgPrintfW(TEXT("recvThread: detect Unplugged and exit\r\n"));
   //   printf("recvThread: detect Unplugged and exit\n");
        return 0;
}
int createRecvThread(void)
{
        DWORD thdID;
        if(resetEvent == INVALID_HANDLE_VALUE)resetEvent = CreateEvent(NULL, FALSE, FALSE, NULL);
        th = CreateThread(NULL,0, (LPTHREAD_START_ROUTINE)recvThread,NULL, 0, &thdID);
        return (th!=NULL);
}

/*************************************************************************************************/
/* Description:                                                                                  */
/*    This routine sets up receive/transmit buffer descriptions queues.                          */
/*    1. Configure USB device                                                                    */
/*    2. Read Node Address from EEPROM                                                           */
/*    3. Read Configuration from Registry & get node address if specified in Registry            */
/*    4. Set Node Address                                                                        */
/*    5. Set Multicast Address : FF FF FF FF FF FF FF FF                                         */
/*    6. Reset PHY, GPIO0_o : 0 -> 1                                                             */
/*    7. Select Media Type, Ethernet or HomePHY                                                  */
/*    8. Set Ethernet control_0 = 0x19 & Ethernet control_0 = 0                                  */
/*    9. Allocate memory for Tx/Rx packet descriptors, URBs & buffers                            */
/*   10. Set up Tx/Rx packet descriptors                                                         */
/*   11. Allocate UM packet descriptors associated with LM packet descriptors                    */
/*        & Allocate Rx Ndis packet and buffer descriptors                                       */
/*   12. Set the receive mask with RECEIVE_MASK_ACCEPT_MULTICAST | RECEIVE_MASK_ACCEPT_BROADCAST */
/*   13. Set link status with LM_STATUS_LINK_ACTIVE                                              */
/*   14. Set Ethernet control_1                                                                  */
/*                                                                                               */
/* Returns:                                                                                      */
/*    LM_STATUS_SUCCESS                                                                          */
/*    LM_STATUS_FAILURE                                                                          */
/*************************************************************************************************/
LM_STATUS LM_InitializeAdapter( PLM_DEVICE_INFO_BLOCK pDevice )
{
    PLM_DEVICE_EXTENSION_INFO   pExtension;
    PLM_PACKET_DESC             pPacket;
    LM_STATUS                   Status;
#ifndef NDISCE_MINIPORT
    URB                         Urb;
    PURB                        pUrb;
#endif
    BYTE *                      pMemory;
    ULONG                       Count;
    USHORT                      i, ctrl1data=0;
    WORD   DATA1,DATA2;


    #ifdef LM_DEBUG_PC
      DbgMessage(INFORM, (ADMTEXT("*** LM_InitializeAdapter Start ***\n")));
    #endif

    #ifdef LM_DEBUG_CE
     printf("*** LM_InitializeAdapter Start ***\n");
    #endif

    pExtension = (PLM_DEVICE_EXTENSION_INFO) pDevice->pLmDeviceExtension;

    /* set up default values */
    pDevice->MaxTxPacketDesc = MAX_TX_PACKET_DESCRIPTORS;   // 20
    pDevice->MaxRxPacketDesc = MAX_RX_PACKET_DESCRIPTORS;   // 20

    /* Configure USB device */
#ifndef NDISCE_MINIPORT
    Status = LM_ConfigureUsb(pDevice);   // Set Configuration
#else
    Status = LM_ConfigureUsbCE(pDevice);
#endif

    if(Status != LM_STATUS_SUCCESS){
		Status = LM_STATUS_FAILURE;
  		goto lExit;
	}


    //DbgBreak();
    pExtension->OutRegisterData[0]=0x08;
    pExtension->OutRegisterData[1]=0x00;
    Status = LM_SetRegister (pDevice, 2, 0x01 );
	if( Status != LM_STATUS_SUCCESS ){
		goto lExit;
	}
    MM_InitWait(1000);

    pDevice->Testpin=0xc0;

    //** jackl add start 05/30/2001 kick eeprom*****
    pExtension->OutRegisterData[0]=0x20;
    pExtension->OutRegisterData[1]=0x00;
    Status = LM_SetRegister (pDevice, 2, 0x2 );
	if( Status != LM_STATUS_SUCCESS ){
		goto lExit;
	}
    pExtension->OutRegisterData[0]=0x00;
    pExtension->OutRegisterData[1]=0x00;
    Status = LM_SetRegister (pDevice, 2, 0x2 );
    //**** jackl add end ******************
	if( Status != LM_STATUS_SUCCESS ){
		goto lExit;
	}

    Status = LM_GetRegister (pDevice, 6, 0x10 );   // Index 0x10 : Ethernet ID registers
    if(Status != LM_STATUS_SUCCESS){
		Status = LM_STATUS_FAILURE;
		goto lExit;
	}

//  DbgMessage(FATAL,(ADMTEXT("Node Address from Register = %02x %02x %02x %02x %02x %02x\n"),pExtension->InRegisterData[0],pExtension->InRegisterData[1],pExtension->InRegisterData[2],pExtension->InRegisterData[3],pExtension->InRegisterData[4],pExtension-

    // Read Node Address from EEPROM
    //Status = LM_ProcessSROMData(pDevice);     //jackl mask 05/30/2001
    //if(Status != LM_STATUS_SUCCESS)   return LM_STATUS_FAILURE;  //jackl mask 05/30/2001

    for (i=0;i<6;i++)    						  //jackl add 05/30/2001 for reduce device initiation time
     pDevice->PermanentNodeAddress[i]=pExtension->InRegisterData[i];      //jackl add 05/30/2001

    for( i=0; i<6; i++ )   { pDevice->NodeAddress[i] = pDevice->PermanentNodeAddress[i]; }

    #ifdef LM_DEBUG_PC
      DbgMessage(INFORM,(ADMTEXT("Node Address from EEPROM: %02x %02x %02x %02x %02x %02x\n"),pDevice->NodeAddress[0],pDevice->NodeAddress[1],pDevice->NodeAddress[2], pDevice->NodeAddress[3],pDevice->NodeAddress[4], pDevice->NodeAddress[5]));
    #endif

    #ifdef LM_DEBUG_CE
      printf("Node Address from EEPROM: %02x %02x %02x %02x %02x %02x\n",pDevice->NodeAddress[0],pDevice->NodeAddress[1],pDevice->NodeAddress[2], pDevice->NodeAddress[3],pDevice->NodeAddress[4], pDevice->NodeAddress[5]);
    #endif
    
    #ifdef DEBCEINIT // add - 08/19/05,bolo
        // unmark - 09/05/05,bolo
	    NKDbgPrintfW(TEXT("LM_InitializeAdapter: Node Address from EEPROM: %02x %02x %02x %02x %02x %02x\n\r"),pDevice->NodeAddress[0],pDevice->NodeAddress[1],pDevice->NodeAddress[2], pDevice->NodeAddress[3],pDevice->NodeAddress[4], pDevice->NodeAddress[5]);
    #endif

#ifdef NDISCE_MINIPORT   
    // Read Configuration from Registry & get node address if specified in Registry
    MM_GetConfig(pDevice);
#else 
    // Read Configuration from Registry & get node address if specified in Registry
    Status = MM_GetConfig(pDevice);
    if(Status != LM_STATUS_SUCCESS)   return LM_STATUS_FAILURE;
#endif

    // Set Node Address
    for( i=0; i<6; i++)   { pExtension->OutRegisterData[i] = pDevice->NodeAddress[i]; }
    Status = LM_SetRegister (pDevice, 6, 0x10 );
    if(Status != LM_STATUS_SUCCESS){
		Status = LM_STATUS_FAILURE;
		goto lExit;
	}

    // Set Multicast Address : FF FF FF FF FF FF FF FF
    for (i=0;i<8;i++)   { pExtension->OutRegisterData[i] = 0xff; }
    Status = LM_SetRegister (pDevice, 8, 0x08 );
    if(Status != LM_STATUS_SUCCESS){
		Status = LM_STATUS_FAILURE;
		goto lExit;
	}

    // Reset PHY, GPIO0_o : 0 -> 1
    pExtension->OutRegisterData[0]=0x24;         // 0x24
    pExtension->OutRegisterData[1]=0x34;
    Status = LM_SetRegister (pDevice, 2, 0x7e );
	if( Status != LM_STATUS_SUCCESS ){
		goto lExit;
	}
    for (i=0;i<100;i++)   { MM_InitWait(1000); } //jackl mask 05/30/2001
    //MM_InitWait(1000); //jackl add 05/30/2001 for modify device initiation time
    pExtension->OutRegisterData[0]=0x26;   // 0x26
    pExtension->OutRegisterData[1]=0x34;
    Status = LM_SetRegister (pDevice, 2, 0x7e );
	if( Status != LM_STATUS_SUCCESS ){
		goto lExit;
	}
    MM_InitWait(1000);
    // enable Internal PHY
    //pExtension->OutRegisterData[0]=0x02;
    //pExtension->OutRegisterData[1]=0x00;
    //Status = LM_SetRegister (pDevice, 2, 0x7B);
    pExtension->OutRegisterData[0]=0x01;
    pExtension->OutRegisterData[1]=0x00;
    Status = LM_SetRegister (pDevice, 2, 0x7B );
	if( Status != LM_STATUS_SUCCESS ){
		goto lExit;
	}
    MM_InitWait(10000);
    pExtension->OutRegisterData[0]=0x02;
    pExtension->OutRegisterData[1]=0x00;
    Status = LM_SetRegister (pDevice, 2, 0x7B );
	if( Status != LM_STATUS_SUCCESS ){
		goto lExit;
	}
    MM_InitWait(10000);

//  printf("Enable Internal PHY\n");

//    LM_WriteMii(pDevice, HPNA, 0x19, 0x7F05);
    if(pDevice->RequestedSelectMedia == SELECT_MEDIA_AUTO_DETECTION)
    {
//   printf(" Check Media Type \n");
    #ifdef DEBCEINIT // add - 08/19/05,bolo
        // unmark - 09/05/05,bolo
	    NKDbgPrintfW(TEXT("LM_InitializeAdapter: Media AutoDect, SelectMedia=%d\n\r"),pDevice->RequestedSelectMedia);
    #endif

	    Status = LM_WriteMii(pDevice,ETHERNET,0x4,0x01E1);    //reset PHY
		if( Status != LM_STATUS_SUCCESS ){
			goto lExit;
		}
        MM_InitWait(1000);
	    Status =  LM_ReadMii(pDevice,ETHERNET,0x4,&DATA1);
	    if( Status != LM_STATUS_SUCCESS ){
		    goto lExit;
	    }

        DATA1 &= 0x1F;
	    Status = LM_ReadMii(pDevice,HPNA,0x3,&DATA2);
	    if( Status != LM_STATUS_SUCCESS ){
		    goto lExit;
	    }
        DATA2 &= 0xFFF0;
        if ( (DATA2 == 0x6B90 ) | (DATA2 == 0x5C20))
        {
            DATA2 = DATA2 ;
        } else
        {
            DATA2 = 0xFFFF ; // cannot Find AMD | NS honme PHY
        }

//      printf(" Data 1= [%X] Data 2=[%X]\n",DATA1,DATA2);
//      DbgMessage(FATAL,("DATA1=[%x] DATA2=[%X]\n",DATA1,DATA2));
        if ( (DATA1 != 0x1) & (DATA2 != 0xFFFF))
        {
//          DbgMessage(FATAL,("Can not Find Ethernet\n"));
            pDevice->RequestedSelectMedia = SELECT_MEDIA_HOMELAN;
        #ifdef DEBCEINIT // add - 08/19/05,bolo
            // unmark - 09/05/05,bolo
	        NKDbgPrintfW(TEXT("LM_InitializeAdapter: Can not find Ethernet, SelectMedia=%d\n\r"),pDevice->RequestedSelectMedia);
        #endif

        }
        if ( (DATA1 == 0x1) & (DATA2 == 0xFFFF))
        {
//          DbgMessage(FATAL,("Can not Find HOMELAN(NS|AMD)\n"));
            pDevice->RequestedSelectMedia = SELECT_MEDIA_ETHERNET;
        #ifdef DEBCEINIT // add - 08/19/05,bolo
            // unmark - 09/05/05,bolo
	        NKDbgPrintfW(TEXT("LM_InitializeAdapter: Can not find Ethernet, SelectMedia=%d\n\r"),pDevice->RequestedSelectMedia);
        #endif
        }
    }

	Status = LM_AutoSelectMedia(pDevice);
	if( Status != LM_STATUS_SUCCESS ){
        #ifdef DEBCEINIT // add - 08/25/05,bolo
        // unmark - 09/05/05,bolo
        NKDbgPrintfW(TEXT("LM_InitializeAdapter: failed to select Media, SelectMedia=%d\n\r"),pDevice->RequestedSelectMedia);
        #endif
		goto lExit;
	}

    // Read Register 1 of PHY 1 for nothing ???
    pExtension->OutRegisterData[0]=0x01;
    pExtension->OutRegisterData[1]=0x00;
    pExtension->OutRegisterData[2]=0x00;
    pExtension->OutRegisterData[3]=0x41;
    Status = LM_SetRegister (pDevice, 4, 0x25 );
	if( Status != LM_STATUS_SUCCESS ){
		goto lExit;
	}
    MM_InitWait(2000);
    Status = LM_GetRegister (pDevice, 3, 0x26 );
	if( Status != LM_STATUS_SUCCESS ){
		goto lExit;
	}

    // Set Ethernet control_0 = 0x19 & Ethernet control_0 = 0
    pExtension->OutRegisterData[0]=0x19;
    pExtension->OutRegisterData[1]=0x00;
    Status = LM_SetRegister (pDevice, 2, 0 );
	if( Status != LM_STATUS_SUCCESS ){
		goto lExit;
	}
    MM_InitWait(1000);
    Status = LM_GetRegister (pDevice, 2, 0 );
	if( Status != LM_STATUS_SUCCESS ){
		goto lExit;
	}
    pDevice->Ctrl0Data = pExtension->InRegisterData[0];

    #ifdef LM_DEBUG_PC
      DbgMessage(INFORM, (ADMTEXT("Ethernet control_0 = %02x, Ethernet control_1 = %02x\n"), pExtension->InRegisterData[0], pExtension->InRegisterData[1]));
    #endif
    // Get wake-up status for nothing ???
    Status = LM_GetRegister (pDevice, 2, 0x7A );
	if( Status != LM_STATUS_SUCCESS ){
		goto lExit;
	}
    MM_InitWait(1000);

    createRecvThread();

  if( !NdisDriverLoaded )
  {
    // Allocate memory for Tx/Rx packet descriptors, URBs & buffers
    Status=MM_AllocateMemory( pDevice,
                              &pMemory,
                              ( sizeof(LM_PACKET_DESC) +
#ifndef NDISCE_MINIPORT
                                sizeof(URB) +
#endif
                                MAX_PACKET_BUFFER_SIZE ) * (pDevice->MaxTxPacketDesc + pDevice->MaxRxPacketDesc) );        // (_+_+0x600)*(0x20+0x20)
    if(Status != LM_STATUS_SUCCESS){
		goto lExit;
	}

    pPacket = (PLM_PACKET_DESC) pMemory;
    pMemory += sizeof(LM_PACKET_DESC) * (pDevice->MaxRxPacketDesc + pDevice->MaxTxPacketDesc);
#ifndef NDISCE_MINIPORT
    pUrb = (PURB) pMemory;
    pMemory += sizeof(URB) * (pDevice->MaxRxPacketDesc + pDevice->MaxTxPacketDesc);
#endif

//  createRecvThread();
    /* setup tx packet descriptors */
    for(Count = 0; Count < pDevice->MaxTxPacketDesc; Count++)
    {
        LL_PushTail(&pDevice->TxPacketFreeList, &pPacket->Link);

        pPacket->Buffer = pMemory;
#ifndef NDISCE_MINIPORT
        pPacket->pUrb = pUrb;
#endif
        pPacket->pDevice = pDevice;

        pPacket++;
#ifndef NDISCE_MINIPORT
        pUrb++;
#endif
        pMemory += MAX_PACKET_BUFFER_SIZE;
    } /* for */

    /* setup Rx packet descriptors. */
    for(Count = 0; Count < pDevice->MaxRxPacketDesc; Count++)
    {
        LL_PushHead(&pDevice->RxPacketFreeList, &pPacket->Link);

        pPacket->Buffer = pMemory;
#ifndef NDISCE_MINIPORT
        pPacket->pUrb = pUrb;
#endif
        pPacket->pDevice = pDevice;

        pPacket++;
#ifndef NDISCE_MINIPORT
        pUrb++;
#endif
        pMemory += MAX_PACKET_BUFFER_SIZE;
    } /* for */

    // Allocate UM packet descriptors associated with LM packet descriptors
    //  & Allocate Rx Ndis packet and buffer descriptors
    Status = MM_AllocateUmPackets(pDevice);
    if(Status != LM_STATUS_SUCCESS){
		goto lExit;
    }
    } // if( !NdisDriverLoaded )

    // Set the receive mask
    pDevice->ReceiveMask = RECEIVE_MASK_ACCEPT_MULTICAST | RECEIVE_MASK_ACCEPT_BROADCAST;

    // Set link status
#ifdef SUPPORT_MEDIASENSE  // add - 07/20/05,bolo for MSC
	if (LM_GetLinkStatus(pDevice) & LINK_STS)
#endif

        pDevice->LinkStatus = LM_STATUS_LINK_ACTIVE;

#ifdef SUPPORT_MEDIASENSE  // add - 07/20/05,bolo for MSC    
	else
        pDevice->LinkStatus = LM_STATUS_LINK_DOWN;
#endif

    /* mark - 08/25/05,bolo
    // Set Ethernet control_1
    pDevice->Ctrl1Data =0;
    if(pDevice->LinkDuplex == LINK_SPEED_100MBPS)
        pDevice->Ctrl1Data |= _100Mode;             // 0x10
    if(pDevice->LinkDuplex == LINK_DUPLEX_FULL)
        pDevice->Ctrl1Data |= Full_Duplex;          // 0x20
    pExtension->OutRegisterData[0]= pDevice->OldCtrl1Data = pDevice->Ctrl1Data; // | Home_Lan ;
    pExtension->OutRegisterData[1]=0x00;
    Status = LM_SetRegister (pDevice, 2, 0x01 );
	if( Status != LM_STATUS_SUCCESS ){
		goto lExit;
	}

    MM_InitWait(1000); //1000
    */
//    pDevice->ResetAdpterFlag = 0;

    #ifdef LM_DEBUG_PC
      DbgMessage(INFORM, (ADMTEXT("*** LM_InitializeAdapter End ***\n")));
    #endif

	Status = LM_STATUS_SUCCESS;

    #ifdef DEBCEINIT // add - 08/25/05,bolo
    // unmark - 09/05/05,bolo
    NKDbgPrintfW(TEXT("LM_InitializeAdapter: failed to select Media, SelectMedia=%d\n\r"),pDevice->RequestedSelectMedia);
    #endif

lExit:
	if (Status != LM_STATUS_SUCCESS)
		MM_FreeResources(pDevice);
	return Status;
} /* LM_InitializeAdapter */

#ifdef SUPPORT_MEDIASENSE // add - 07/20/05,bolo for MSC
// ccwong, 2004/09/13
LM_STATUS LM_ReInitializeAdapter( PLM_DEVICE_INFO_BLOCK pDevice )
{
	LM_STATUS	Status;

    #ifdef DEBCEINIT // add - 08/25/05,bolo
    // unmark - 09/05/05,bolo
    NKDbgPrintfW(TEXT("LM_ReInitializeAdapter: LinkStatus=%d\n\r"),pDevice->LinkStatus);
    #endif

	Status = LM_ResetAdapter(pDevice);
	if( Status == LM_STATUS_SUCCESS )
		return LM_AutoSelectMedia(pDevice);
	else
		return Status;
}
// end ccwong
#endif


/******************************************************************************/
/* Description:                                                               */
/*    This function gets called whenever the driver is unloaded.              */
//    1. Set Feature of Enable remote wakeup
//    2. Abort & Reset IN/OUT pipes
/*                                                                            */
/* Returns:                                                                   */
/*    LM_STATUS_SUCCESS                                                       */
/******************************************************************************/
LM_STATUS LM_Shutdown( PLM_DEVICE_INFO_BLOCK pDevice )
{
    //abort endpoint pipe
    PLM_DEVICE_EXTENSION_INFO   pExtension;
    LM_STATUS                   Status;
    USHORT                      i;
#ifndef NDISCE_MINIPORT
    URB                         Urb;
    PURB                        pUrb;
#endif


    #ifdef LM_DEBUG_CE
    printf("*** LM_Shutdown Start ***\n");
    #endif

    #ifdef LM_DEBUG_PC
      DbgMessage(INFORM, (ADMTEXT("*** LM_Shutdown Start ***\n")));
    #endif
    pDevice->AdapterStatus = LM_STATUS_ADAPTER_CLOSED;
    pExtension = (PLM_DEVICE_EXTENSION_INFO) pDevice->pLmDeviceExtension;


#ifndef NDISCE_MINIPORT
    UsbBuildFeatureRequest( &Urb,
                            URB_FUNCTION_SET_FEATURE_TO_DEVICE,   //set feature DEVICE
                            0x1,                                  //enable remote wake up
                            0,                                    //index =0
                            NULL );

    Status = LM_CallUsbd(pDevice, &Urb);

#endif
    Status = LM_AbortPipe(pDevice);
    for ( i=0; i<100; i++ )
      MM_InitWait(1000);
    #ifdef LM_DEBUG_PC
      DbgMessage(INFORM, (ADMTEXT("*** LM_Shutdown End ***\n")));
    #endif
    return LM_STATUS_SUCCESS;
} /* LM_Shutdown */

/******************************************************************************/
/* Description:                                                               */
/*    This function reinitializes the adapter.                                */
/*    1. Reset MAC                                                            */
/*    2. Pop a Rx packet                                                      */
/*    3. Enblae PHY loopback                                                  */
/*    4. Set wakeup control                                                   */
/*    5. Set adapter status with LM_STATUS_ADAPTER_OPEN                       */
/*    6. Set Rx flow control                                                  */
/*    7. Enable Ethernet Tx, Rx                                               */
/*    8. Set Feature of DEVICE_REMOTE_WAKEUP to Device                        */
/*    9. Issue a Bulk IN transfer to queue Rx packet                             */
/*                                                                            */
/* Returns:                                                                   */
/*    LM_STATUS_SUCCESS                                                       */
/******************************************************************************/
LM_STATUS LM_ResetAdapter( PLM_DEVICE_INFO_BLOCK pDevice )
{
    PLM_PACKET_DESC             pPacket;
    PLM_DEVICE_EXTENSION_INFO   pExtension;
    LM_STATUS                   Status;
#ifndef NDISCE_MINIPORT
    URB                         Urb;
#endif
    WORD                        Value16; // add - 08/25/05,bolo

    #ifdef LM_DEBUG_PC
      DbgMessage(INFORM, (ADMTEXT("*** LM_ResetAdapter Start ***\n")));
    #endif


    #ifdef LM_DEBUG_CE
    printf("*** LM_ResetAdapter Start ***\n");
    #endif
//    pDevice->ResetAdpterFlag = 1; // v1.09 jackl add 2003_0411 for suspend and power off control
    pExtension = (PLM_DEVICE_EXTENSION_INFO) pDevice->pLmDeviceExtension;

    //reset MAC
    #if 0
    pExtension->OutRegisterData[0]=0x08;
    pExtension->OutRegisterData[1]=0x00;
    Status = LM_SetRegister (pDevice, 2, 0x01);
    MM_InitWait(1000);
    #endif
    pPacket = (PLM_PACKET_DESC) LL_PopHead(&pDevice->RxPacketFreeList);

    #ifdef LM_DEBUG_PC
      DbgMessage(FATAL, (ADMTEXT("Pop a Rx Packet Descriptor\n")));
    #endif

    if(pPacket == NULL)
    {
        #ifdef LM_DEBUG_PC
          DbgMessage(FATAL, (ADMTEXT("No packet available in pDevice->RxPacketFreeList\n")));
        #endif
        return LM_STATUS_FAILURE;
    } /* if */

    // Set bit 13 of Register 0 of PHY 2 to select High Speed
    /* mark - 08/25/05,bolo
    Status = LM_WriteMii(pDevice, 2, 0, 0x2000);
	if( Status != LM_STATUS_SUCCESS ){
		goto lExit;
	}
    */

    if (pDevice->CurrMode == ETHERNET) // mod - 08/19/05,bolo
    {
        #ifdef LM_DEBUG_PC
        DbgMessage(INFORM, (ADMTEXT("Current mode is ETHERNET\n")));
        #endif
        #ifdef DEBCEINIT // add - 08/25/05,bolo
        // unmark - 09/05/05,bolo
	    NKDbgPrintfW(TEXT("LM_ResetAdapter: Current mode is ETHERNET\n\r"));
        #endif
        // According to select from registry, choose a media type of Ethernet
		Status =  LM_SetMediaType(pDevice);
		if( Status != LM_STATUS_SUCCESS ){
			goto lExit;
		}

        // Set Ethernet control_1
        pDevice->Ctrl1Data =0;
        if(pDevice->LinkDuplex == LINK_SPEED_100MBPS)
            pDevice->Ctrl1Data |= _100Mode;             // 0x10

        if(pDevice->LinkDuplex == LINK_DUPLEX_FULL)
            pDevice->Ctrl1Data |= Full_Duplex;          // 0x20

        pExtension->OutRegisterData[0]= pDevice->OldCtrl1Data = pDevice->Ctrl1Data; // | Home_Lan ;
        pExtension->OutRegisterData[1]=0x00;

        Status = LM_SetRegister (pDevice, 2, 0x01 );
	    if( Status != LM_STATUS_SUCCESS ){
    	    goto lExit;
	    }

        /* mark - 08/26/05,bolo
        // add - 08/25/05,bolo; isolate another one
        pExtension->OutRegisterData[0]=HPNA;
        pExtension->OutRegisterData[1]=0x0;
        pExtension->OutRegisterData[2]=0x04;
        pExtension->OutRegisterData[3]=0x20;
        Status = LM_SetRegister (pDevice, 4, 0x25);
	    if( Status != LM_STATUS_SUCCESS ){
    	    goto lExit;
	    }
        */

        // Set bit 14 of Register 0 of PHY 1 to Enable Loopback
        /* mark - 08/25/05,bolo
		Status =  LM_WriteMii(pDevice, 2, 0, 0x0400);
		if( Status != LM_STATUS_SUCCESS ){
			goto lExit;
		}
        */
    }
    else
    {
        // ++ add - 08/25/05,bolo
        if (pDevice->CurrMode == MACMII) {

            pDevice->Ctrl1Data =0;

            if (pDevice->LinkDuplex == LINK_SPEED_100MBPS)
                pDevice->Ctrl1Data |= _100Mode;

            if (pDevice->LinkDuplex == LINK_DUPLEX_FULL)
                pDevice->Ctrl1Data |= Full_Duplex;

            pExtension->OutRegisterData[0] = pDevice->OldCtrl1Data = pDevice->Ctrl1Data; // | Home_Lan ;
            pExtension->OutRegisterData[1] = 0x00;

#ifdef DEBCEINIT
            // unmark - 09/05/05,bolo
            NKDbgPrintfW(TEXT("LM_ResetAdapter: MACMII: Ctrl-1=%02x, Ctrl-2=%02x\n\r"),pExtension->OutRegisterData[0],pExtension->OutRegisterData[1]); // mod - 08/26/05,bolo
#endif

            Status = LM_SetRegister (pDevice, 2, 0x01);
    	    if( Status != LM_STATUS_SUCCESS ){
		        goto lExit;
	        }

            Status = LM_ReadMii(pDevice, pDevice->PhyAddr, 1, &Value16); // add - 08/25/05,bolo
			if( Status != LM_STATUS_SUCCESS )
            { 
                goto lExit;
            }

#ifdef DEBCEINIT  
            // unmark - 09/05/05,bolo
            NKDbgPrintfW(TEXT("LM_ResetAdapter: MACMII: Ctrl-1=%02x, Ctrl-2=%02x\n\r"),pExtension->OutRegisterData[0],pExtension->OutRegisterData[1]); // mod - 08/26/05,bolo
#endif

	    } else { // --
            #ifdef LM_DEBUG_PC
            DbgMessage(INFORM, (ADMTEXT("Current mode is not ETHERNET\n")));
            #endif
            #ifdef DEBCEINIT // add - 08/25/05,bolo
            // unmark - 09/05/05,bolo
	        NKDbgPrintfW(TEXT("LM_ResetAdapter: Current mode is HomeLAN\n\r"));
            #endif
            // Set bit 14 of Register 0 of PHY 2 to Enable Loopback
		    Status =  LM_WriteMii(pDevice, 1, 0, 0x0400);
		    if( Status != LM_STATUS_SUCCESS ){
			    goto lExit;
		    }
        }
    }

    // Set  wake-up control register according registry setting
    if(pDevice->RequestedMagicPacket == 0)
        pDevice->WakeCtrlReg = CRC16TYPE | MGCPKT_EN;   // 0x04 | 0x80
    else
        pDevice->WakeCtrlReg = CRC16TYPE;
    if(pDevice->RequestedLinkWakeup == 0)
        pDevice->WakeCtrlReg |= LINK_EN;                // 0x40
    pExtension->OutRegisterData[0]=pDevice->WakeCtrlReg;
    Status = LM_SetRegister (pDevice, 2, 0x78 );
	if( Status != LM_STATUS_SUCCESS ){
		goto lExit;
	}


    // Set adapter status with LM_STATUS_ADAPTER_OPEN
    pDevice->AdapterStatus = LM_STATUS_ADAPTER_OPEN;   // 0x000c

    // Set Rx flow control (when PAUSE frame will be transmited by HW) with packet number & FIFO limit
    if (pDevice->RequestedFlowControl == 0)   //flow_control enable
    {
        pDevice->Ctrl0Data = (pDevice->Ctrl0Data) | Rx_Flowctl_en;   //0x20
        pDevice->FlowCtrlRx=pDevice->FlowCtrlTx=0;
        if(pDevice->RequestedFlowControlTx != 0)
            pDevice->FlowCtrlTx = 1<<(pDevice->RequestedFlowControlTx) | 1;   // Packet Number limit
        if(pDevice->RequestedFlowControlRx != 0)
            pDevice->FlowCtrlRx = 1<<(pDevice->RequestedFlowControlRx) | 1;   // FIFO limit
        pExtension->OutRegisterData[0]=pDevice->FlowCtrlTx;
        pExtension->OutRegisterData[1]=pDevice->FlowCtrlRx;
        Status = LM_SetRegister (pDevice, 2, 0x1A );
		if( Status != LM_STATUS_SUCCESS ){
			goto lExit;
		}

    }

    // Enable Ethernet Tx, Rx & Rx flow control if need
    pDevice->Ctrl0Data = (pDevice->Ctrl0Data) | (Tx_en + Rx_en);   // 0x80 + 0x40
    pExtension->OutRegisterData[0]=pDevice->Ctrl0Data;
    pExtension->OutRegisterData[1]=pDevice->Ctrl1Data;
#ifdef DEBCEINIT // add - 08/25/05,bolo
    // unmark - 09/05/05,bolo
    NKDbgPrintfW(TEXT("LM_ResetAdapter: Ctrl-0=%02x, Ctrl-1=%02x\n\r"),pExtension->OutRegisterData[0],pExtension->OutRegisterData[1]);
#endif

    Status = LM_SetRegister (pDevice, 2, 0 );
	if( Status != LM_STATUS_SUCCESS ){
		goto lExit;
	}

	Status = LM_GetRegister (pDevice, 0x02, 0x80 );
	if( Status != LM_STATUS_SUCCESS ){
		goto lExit;
	}
	pExtension->OutRegisterData[0]=0xc0;
    pExtension->OutRegisterData[1]=pExtension->InRegisterData[1];
    Status = LM_SetRegister (pDevice, 0x02, 0x80 );
	if( Status != LM_STATUS_SUCCESS ){
		goto lExit;
	}


	pExtension->OutRegisterData[0]=0xff;
    pExtension->OutRegisterData[1]=0x01;
    Status = LM_SetRegister (pDevice, 0x02, 0x83 );
	if( Status != LM_STATUS_SUCCESS ){
		goto lExit;
	}


    // Set Feature of DEVICE_REMOTE_WAKEUP to Device
#ifndef NDISCE_MINIPORT
    UsbBuildFeatureRequest( &Urb,
                            URB_FUNCTION_SET_FEATURE_TO_DEVICE,   // set feature DEVICE
                            0x1,                                  // feature selector of enable remote wake up
                            0,                                    // device index I think
                            NULL );
    Status = LM_CallUsbd(pDevice, &Urb);
#else
    Status = USBM_SetFeature( hUSB, USB_SEND_TO_DEVICE, USB_FEATURE_REMOTE_WAKEUP, 0 );

#endif
    if(Status != LM_STATUS_SUCCESS){
		goto lExit;
	}

    // Issue a Bulk IN transfer to queue Rx packet
//    LM_QueueRxPacket(pDevice, pPacket);
m_pDevice = pDevice;
m_pPacket = pPacket;
SetEvent(resetEvent);

     //LM_GetRegister (pDevice, 0x02, 0x7f);
    //pExtension->OutRegisterData[0]=0xc0;
    //pExtension->OutRegisterData[1]=pExtension->InRegisterData[1];
    //Status = LM_SetRegister (pDevice, 0x02, 0x80);

    #ifdef LM_DEBUG_PC
      DbgMessage(INFORM, (ADMTEXT("*** LM_ResetAdapter End ***\n")));
    #endif
//    pDevice->ResetAdpterFlag = 0; // v1.09 jackl add 2003_0411 for suspend and power off control
	Status = LM_STATUS_SUCCESS;

    #ifdef DEBCEINIT // add - 08/25/05,bolo
    // unmark - 09/05/05,bolo
    NKDbgPrintfW(TEXT("LM_ResetAdapter: Ctrl-0=%02x, Ctrl-1=%02x\n\r"),pExtension->OutRegisterData[0],pExtension->OutRegisterData[1]);
    #endif

lExit:
//	if (Status != LM_STATUS_SUCCESS)
//		MM_FreeResources(pDevice);

#ifdef DEBCEINIT // mod - 08/25/05,add - 08/19/05,bolo
    
    LM_GetRegister (pDevice, 1, 0x0);
    // unmark - 09/05/05,bolo
    NKDbgPrintfW(TEXT("LM_ResetAdapter: ETherCtrl-0=0x%x\n\r"),pExtension->InRegisterData[0]);

    LM_GetRegister (pDevice, 1, 0x1);
    // unmark - 09/05/05,bolo
    NKDbgPrintfW(TEXT("LM_ResetAdapter: ETherCtrl-1=0x%x\n\r"),pExtension->InRegisterData[0]);

    LM_GetRegister (pDevice, 1, 0x2);
    // unmark - 09/05/05,bolo
    NKDbgPrintfW(TEXT("LM_ResetAdapter: ETherCtrl-2=0x%x\n\r"),pExtension->InRegisterData[0]);

    LM_GetRegister (pDevice, 1, 0x3);
    // unmark - 09/05/05,bolo
    NKDbgPrintfW(TEXT("LM_ResetAdapter: Reserved(0x03)=0x%x\n\r"),pExtension->InRegisterData[0]);
    
    LM_GetRegister (pDevice, 1, 0x1A);
    // unmark - 09/05/05,bolo
    NKDbgPrintfW(TEXT("LM_ResetAdapter: RxPktNumFlow=0x%x\n\r"),pExtension->InRegisterData[0]);   

    LM_GetRegister (pDevice, 1, 0x1B);
    // unmark - 09/05/05,bolo
    NKDbgPrintfW(TEXT("LM_ResetAdapter: OcpRxFifoFlow=0x%x\n\r"),pExtension->InRegisterData[0]);

    LM_GetRegister (pDevice, 1, 0x1C);
    // unmark - 09/05/05,bolo
    NKDbgPrintfW(TEXT("LM_ResetAdapter: EP1Ctrl=0x%x\n\r"),pExtension->InRegisterData[0]);

    LM_GetRegister (pDevice, 1, 0x1D);
    // unmark - 09/05/05,bolo
    NKDbgPrintfW(TEXT("LM_ResetAdapter: Reserved(0x1D)=0x%x\n\r"),pExtension->InRegisterData[0]);

    LM_GetRegister (pDevice, 1, 0x2A);
    // unmark - 09/05/05,bolo
    NKDbgPrintfW(TEXT("LM_ResetAdapter: USBStatus=0x%x\n\r"),pExtension->InRegisterData[0]);

    LM_GetRegister (pDevice, 1, 0x2B);
    // unmark - 09/05/05,bolo
    NKDbgPrintfW(TEXT("LM_ResetAdapter: USBStatus=0x%x\n\r"),pExtension->InRegisterData[0]);

    LM_GetRegister (pDevice, 1, 0x2C);
    // unmark - 09/05/05,bolo
    NKDbgPrintfW(TEXT("LM_ResetAdapter: TxStatus-2=0x%x\n\r"),pExtension->InRegisterData[0]);

    LM_GetRegister (pDevice, 1, 0x2D);
    // unmark - 09/05/05,bolo
    NKDbgPrintfW(TEXT("LM_ResetAdapter: RxStatus=0x%x\n\r"),pExtension->InRegisterData[0]);

    LM_GetRegister (pDevice, 1, 0x2E);
    // unmark - 09/05/05,bolo
    NKDbgPrintfW(TEXT("LM_ResetAdapter: RxLostPktHigh=0x%x\n\r"),pExtension->InRegisterData[0]);

    LM_GetRegister (pDevice, 1, 0x2F);
    // unmark - 09/05/05,bolo
    NKDbgPrintfW(TEXT("LM_ResetAdapter: RxLostPktLow=0x%x\n\r"),pExtension->InRegisterData[0]);
     
    LM_GetRegister (pDevice, 1, 0x7B);
    // unmark - 09/05/05,bolo
    NKDbgPrintfW(TEXT("LM_ResetAdapter: IntPHYCtrl=0x%x\n\r"),pExtension->InRegisterData[0]);

    LM_GetRegister (pDevice, 1, 0x7C);
    // unmark - 09/05/05,bolo
    NKDbgPrintfW(TEXT("LM_ResetAdapter: GPIO[5:4]=0x%x\n\r"),pExtension->InRegisterData[0]);

    LM_GetRegister (pDevice, 1, 0x7E);
    // unmark - 09/05/05,bolo
    NKDbgPrintfW(TEXT("LM_ResetAdapter: GPIO[1:0]=0x%x\n\r"),pExtension->InRegisterData[0]);

    LM_GetRegister (pDevice, 1, 0x7F);
    // unmark - 09/05/05,bolo
    NKDbgPrintfW(TEXT("LM_ResetAdapter: GPIO[3:2]=0x%x\n\r"),pExtension->InRegisterData[0]);

    LM_GetRegister (pDevice, 1, 0x81);
    // unmark - 09/05/05,bolo
    NKDbgPrintfW(TEXT("LM_ResetAdapter: MII test_mode=0x%x\n\r"),pExtension->InRegisterData[0]);  

#endif /* DEBCEINIT */    
    
	return Status;
} /* LM_ResetAdapter */

/******************************************************************************/
/* Description:                                                               */
/*    This routine detects/sets link speed and media type of Ethernet         */
/*                                                                            */
/* Parameters:                                                                */
/*    PLM_DEVICE_INFO_BLOCK pDevice                                           */
/*                                                                            */
/* Return:                                                                    */
/*    LM_STATUS_SUCCESS.                                                      */
/******************************************************************************/
STATIC LM_STATUS LM_SetMediaType( PLM_DEVICE_INFO_BLOCK pDevice )
{
    WORD    Value16, Temp, i;
    DWORD   Count = 0;

	LM_STATUS	Status;

    #ifdef LM_DEBUG_PC
      DbgMessage(INFORM, (ADMTEXT("*** LM_SetMediaType Start ***\n")));
    #endif

    #ifdef LM_DEBUG_CE
    printf("*** LM_SetMediaType Start ***\n");
    #endif

    /* set link status */
#ifdef SUPPORT_MEDIASENSE // add - 07/20/05,bolo for MSC
	if (LM_GetLinkStatus(pDevice) & LINK_STS)
#endif
        pDevice->LinkStatus = LM_STATUS_LINK_ACTIVE;

#ifdef SUPPORT_MEDIASENSE // add - 07/20/05,bolo for MSC
	else
        pDevice->LinkStatus = LM_STATUS_LINK_DOWN;
#endif

    /* determine media type settings */
    //pDevice->MediaType = MEDIA_TYPE_AUTO_DETECTION;
    pDevice->LinkSpeed = LINK_SPEED_UNKNOWN;
    pDevice->LinkDuplex = LINK_DUPLEX_UNKNOWN;

    #ifdef LM_DEBUG_PC
      DbgMessage(FATAL, (ADMTEXT("Media Type = %d\n"),pDevice->RequestedMediaType));
    #endif
    
    #ifdef DEBCEINIT // add - 08/19/05,bolo
        // unmark - 09/05/05,bolo
	    NKDbgPrintfW(TEXT("LM_SetMediaType: RequestedMediaType=%d\n\r"),pDevice->RequestedMediaType);
    #endif

    switch(pDevice->RequestedMediaType)
    {
        case MEDIA_TYPE_UTP_10MBPS:
            #ifdef LM_DEBUG_PC
              DbgMessage(INFORM, (ADMTEXT("Media Type from Registry : 10Mbps UTP\n")));
            #endif
            pDevice->LinkSpeed = LINK_SPEED_10MBPS;
            pDevice->LinkDuplex = LINK_DUPLEX_HALF;
            Temp=0x20;
            break;

        case MEDIA_TYPE_UTP_10MBPS_FULL_DUPLEX:
            #ifdef LM_DEBUG_PC
              DbgMessage(INFORM, (ADMTEXT("Media Type from Registry : 10Mbps UTP Full Duplex\n")));
            #endif
            pDevice->LinkSpeed = LINK_SPEED_10MBPS;
            pDevice->LinkDuplex = LINK_DUPLEX_FULL;
            Temp=0x40;
            break;

        case MEDIA_TYPE_UTP_100MBPS:
            #ifdef LM_DEBUG_PC
              DbgMessage(INFORM, (ADMTEXT("Media Type from Registry : 100Mbps UTP\n")));
            #endif
            pDevice->LinkSpeed = LINK_SPEED_100MBPS;
            pDevice->LinkDuplex = LINK_DUPLEX_HALF;
            Temp=0x80;
            break;

        case MEDIA_TYPE_UTP_100MBPS_FULL_DUPLEX:
            #ifdef LM_DEBUG_PC
              DbgMessage(INFORM, (ADMTEXT("Media Type from Registry : 100Mbps UTP Full Duplex\n")));
            #endif
            pDevice->LinkSpeed = LINK_SPEED_100MBPS;
            pDevice->LinkDuplex = LINK_DUPLEX_FULL;
            Temp=0x100;
            break;

        default:
            #ifdef LM_DEBUG_PC
              DbgMessage(INFORM, (ADMTEXT("Unknown Media Type from Registry\n")));
            #endif
			//if invalid value is set, MEDIA_TYPE_AUTO_DETECTION should be applied.
			pDevice->RequestedMediaType = MEDIA_TYPE_AUTO_DETECTION;
            Temp=0;
            break;
    }  /* switch */


    if (pDevice->RequestedMediaType == MEDIA_TYPE_AUTO_DETECTION)
    {
        // wait for auto-negotiate completed
        for (i=0; i<=250; i++)
        {
			Status =  LM_ReadMii(pDevice, pDevice->PhyAddr, 1, &Temp);
			if( Status != LM_STATUS_SUCCESS ){
				goto lExit;
			}
            #ifdef LM_DEBUG_PC
              DbgMessage(VERBOSE, (ADMTEXT(" PHY %d Register 1 (Status) = %04x\n"), pDevice->PhyAddr, Temp));
            #endif
            if ( Temp & 0x20)   break;   // auto-negotiation completed
            MM_InitWait(1000);
        } // for
		Status =  LM_ReadMii(pDevice,pDevice->PhyAddr,5,&Value16);
		if( Status != LM_STATUS_SUCCESS ){
			goto lExit;
		}
        #ifdef LM_DEBUG_PC
          DbgMessage(INFORM, (ADMTEXT(" PHY %d Register 5 (Link Partner Ability)= %04x\n"), pDevice->PhyAddr, Value16));
        #endif
        // lic add this for mediatype
        pDevice->PartnerMedia = Value16;

        Value16 >>= 5;
        if ( Value16 & 8)
        {
            #ifdef LM_DEBUG_PC
              DbgMessage(INFORM, (ADMTEXT("Auto-negotiation completed & select 100Mbps FULL UTP\n")));
            #endif
            pDevice->LinkSpeed = LINK_SPEED_100MBPS;
            pDevice->LinkDuplex = LINK_DUPLEX_FULL;
        }
        else
        {
            if ( Value16 & 4)
            {
                #ifdef LM_DEBUG_PC
                  DbgMessage(INFORM, (ADMTEXT("Auto-negotiation completed & select 100Mbps UTP\n")));
                #endif
                pDevice->LinkSpeed = LINK_SPEED_100MBPS;
                pDevice->LinkDuplex = LINK_DUPLEX_HALF;
            }
            else
            {
                if ( Value16 & 2)
                {
                    #ifdef LM_DEBUG_PC
                      DbgMessage(INFORM, (ADMTEXT("Auto-negotiation completed & select 10Mbps FULL UTP\n")));
                    #endif
                    pDevice->LinkSpeed = LINK_SPEED_10MBPS;
                    pDevice->LinkDuplex = LINK_DUPLEX_FULL;
                }
                else
                {
                    #ifdef LM_DEBUG_PC
                      DbgMessage(INFORM, (ADMTEXT("Auto-negotiation completed & select 10Mbps HALF UTP\n")));
                    #endif
                    pDevice->LinkSpeed = LINK_SPEED_10MBPS;
                    pDevice->LinkDuplex = LINK_DUPLEX_HALF;
                }//if ( Value16 & 2)
            } //( Value16 & 4)
        } //if ( Value16 & 8)
    }
    else //if (pDevice->RequestedMediaType == MEDIA_TYPE_AUTO_DETECTION)
    {
        // lic I modify here and above
		Status =  LM_ReadMii(pDevice,pDevice->PhyAddr,4,&Value16);
		if( Status != LM_STATUS_SUCCESS ){
			goto lExit;
		}
        #ifdef LM_DEBUG_PC
          DbgMessage(INFORM, (ADMTEXT(" PHY %d Register 4 (Advertisement)= %04x \n"), pDevice->PhyAddr, Value16));
        #endif

        // Set manual selected media type to PHY 2 Register 4
        Value16 = (Value16 & 0xFE1F) | Temp;
        #ifdef LM_DEBUG_PC
          DbgMessage(INFORM, (ADMTEXT("Write %04x to PHY %d Register 4 (Advertisement)\n"), Value16, pDevice->PhyAddr));
        #endif
		Status =  LM_WriteMii(pDevice,pDevice->PhyAddr, 4, Value16);
		if( Status != LM_STATUS_SUCCESS ){
			goto lExit;
		}
		// Restart Auto-Negotiation
		Status =  LM_WriteMii(pDevice,pDevice->PhyAddr, 0, 0x1200);
		if( Status != LM_STATUS_SUCCESS ){
			goto lExit;
		}
        MM_InitWait(30000);
    }
    #ifdef LM_DEBUG_PC
      DbgMessage(INFORM, (ADMTEXT("*** LM_SetMediaType End ***\n")));
    #endif
	Status = LM_STATUS_SUCCESS;
lExit:
//	if (Status != LM_STATUS_SUCCESS)
//		MM_FreeResources(pDevice);
	return Status;
} /* LM_SetMediaType */

/******************************************************************************/
/* Description:                                                               */
/*    This routine select what media we use (Ethernet/HomeLan)                */
/*                                                                            */
/* Parameters:                                                                */
/*    PLM_DEVICE_INFO_BLOCK pDevice                                           */
/*                                                                            */
/* Return:                                                                    */
/*    LM_STATUS_SUCCESS.                                                      */
/******************************************************************************/
STATIC LM_STATUS
LM_AutoSelectMedia(
PLM_DEVICE_INFO_BLOCK pDevice) {
    PLM_DEVICE_EXTENSION_INFO pExtension;
    WORD Value16,PHYID;     //PHYID=HOMEPHY
    LM_STATUS Status;
    int             i;
    BYTE Temp, Temp1;


    //#ifdef LM_DEBUG_PC
    //	DbgMessage(INFORM, (ADMTEXT("LM_AutoSelectMedia\n")));
    //#endif

    #ifdef LM_DEBUG_CE
      printf("*** LM_AutoSelectMedia Start ***\n");
    #endif

    pExtension = (PLM_DEVICE_EXTENSION_INFO) pDevice->pLmDeviceExtension;

    // Set link status
#ifdef SUPPORT_MEDIASENSE // add - 07/20/05,bolo for MSC
	if (LM_GetLinkStatus(pDevice) & LINK_STS)
#endif

        pDevice->LinkStatus = LM_STATUS_LINK_ACTIVE;

#ifdef SUPPORT_MEDIASENSE // add - 07/20/05,bolo for MSC
	else
        pDevice->LinkStatus = LM_STATUS_LINK_DOWN;
#endif

    /* determine media type settings */

//  DbgMessage(FATAL, ("Select Media = %d \n",pDevice->RequestedSelectMedia));


// check HOME PHY TYPE
    #ifdef DEBCEINIT // add - 08/19/05,bolo
        // unmark - 09/05/05,bolo
        NKDbgPrintfW(TEXT("LM_AutoSelectMedia: RequestedSelectMedia=%d\n\r"),pDevice->RequestedSelectMedia);
    #endif

    if( (pDevice->RequestedSelectMedia == SELECT_MEDIA_AUTO_DETECTION) || // mod - 08/19/05,bolo; org: "|", should change as "||"
        (pDevice->RequestedSelectMedia == SELECT_MEDIA_HOMELAN))
    {
//      DbgMessage(INFORM,("Define Home Lan Type ==>"));
        pDevice->HomePhy = 0xFF;   //unknow
        Status = LM_ReadMii(pDevice, HPNA, 3, &PHYID);
		if( Status != LM_STATUS_SUCCESS ){
			goto lExit;
		}
//      DbgMessage(INFORM,("PHY =3 index3 = %X \n",PHYID));
        if ( PHYID == AMD901 )
        {
            pDevice->HomePhy = 0x0;   //AMD AM79c901v A3

//          DbgMessage(INFORM,("AMD\n"));
        }
        if ( PHYID == NS851A )
        {
            pDevice->HomePhy = 0x1;   //NS DP83851 A
//          DbgMessage(INFORM,("NS_A\n"));
            Status = LM_WriteMii(pDevice, HPNA, 0, 0x2000);
		    if( Status != LM_STATUS_SUCCESS ){ goto lExit; }

            Status = LM_WriteMii(pDevice, HPNA, 0x19, 0x7F07);
		    if( Status != LM_STATUS_SUCCESS ){ goto lExit; }

        }
        if ( PHYID == NS851B )
        {
            pDevice->HomePhy = 0x2;   //NS DP83851 B
//          DbgMessage(INFORM,("NS_B\n"));
            Status = LM_WriteMii(pDevice, HPNA, 0, 0x2000);
		    if( Status != LM_STATUS_SUCCESS ){ goto lExit; }

            Status = LM_WriteMii(pDevice, HPNA, 0x19, 0x7F05);
		    if( Status != LM_STATUS_SUCCESS ){ goto lExit; }
        }
        if ( PHYID == NS851C )
        {
            pDevice->HomePhy = 0x3;   //NS DP83851 C
//          DbgMessage(INFORM,("NS_C\n"));
            Status = LM_WriteMii(pDevice, HPNA, 0, 0x2000);
		    if( Status != LM_STATUS_SUCCESS ){ goto lExit; }

            Status = LM_WriteMii(pDevice, HPNA, 0x19, 0x7F02);
	    	if( Status != LM_STATUS_SUCCESS ){ goto lExit; }
//work around for NS_C
            Status = LM_WriteMii(pDevice, HPNA, 0x1F, 0x1);
		    if( Status != LM_STATUS_SUCCESS ){ goto lExit; }

            Status = LM_WriteMii(pDevice, HPNA, 0x7, 0x08);
	    	if( Status != LM_STATUS_SUCCESS ){ goto lExit; }

            Status = LM_WriteMii(pDevice, HPNA, 0x6, 0x38);
    		if( Status != LM_STATUS_SUCCESS ){ goto lExit; }

            Status = LM_WriteMii(pDevice, HPNA, 0x1F, 0x0);
	    	if( Status != LM_STATUS_SUCCESS ){ goto lExit; }
        }
    }

    if (pDevice->RequestedSelectMedia == SELECT_MEDIA_AUTO_DETECTION)
    {
 // force to Ehernet first
        // LIC add this for checkforhang autodetectmedia
        // because Ethernet will be the first priority
        pDevice->CurrMode = ETHERNET;
        pDevice->OrgMode  = ETHERNET;
        //----------------------------------------------
        for (i=0;i<10;i++)
        MM_InitWait(10000);

        pDevice->PhyAddr=ETHERNET;
        Status = LM_ReadMii(pDevice, pDevice->PhyAddr, 2, &Value16);
		if( Status != LM_STATUS_SUCCESS ){ goto lExit; }
//      DbgMessage(INFORM,("PHY =1 index2 = %X ",Value16));
        if ( (Value16 == 0xFFFF) | (Value16 == 0) )
        {
            Value16 = 0;
        } else {
            Status = LM_ReadMii(pDevice, pDevice->PhyAddr, 1, &Value16);
			if( Status != LM_STATUS_SUCCESS ){ goto lExit; }
            Status = LM_ReadMii(pDevice, pDevice->PhyAddr, 1, &Value16);
			if( Status != LM_STATUS_SUCCESS ){ goto lExit; }
//          DbgMessage(INFORM,("PHY =1 index1 = %X \n",Value16));
        }
        if ( Value16 & 0x4 )
        {
            pExtension->OutRegisterData[0]=ETHER_GPIO;
            pExtension->OutRegisterData[1]=pDevice->Testpin;
            pExtension->OutRegisterData[2]=PII_ETHER;

            Status = LM_SetRegister (pDevice, 3, 0x7F );
			if( Status != LM_STATUS_SUCCESS ){ goto lExit; }

        } else {
            pDevice->PhyAddr=HPNA;
            Status = LM_ReadMii(pDevice, pDevice->PhyAddr, 3, &Value16);
			if( Status != LM_STATUS_SUCCESS ){ goto lExit; }
//          DbgMessage(INFORM,("PHY =1 index2 = %X ",Value16));
            if ( (Value16 == 0xFFFF) | (Value16 == 0) )
            {
                Value16 = 0;

            } else {
                Status = LM_ReadMii(pDevice, pDevice->PhyAddr, 1, &Value16);
			    if( Status != LM_STATUS_SUCCESS ){ goto lExit; }

                Status = LM_ReadMii(pDevice, pDevice->PhyAddr, 1, &Value16);
	    		if( Status != LM_STATUS_SUCCESS ){ goto lExit; }
//              DbgMessage(INFORM,("PHY =1 index1 = %X \n",Value16));
           }
           if ( Value16 & 0x4)
           {
//              DbgMessage(INFORM, ("Find Home Lan\n"));
                // LIC add this for checkforhang autodetectmedia
                pDevice->CurrMode = HPNA;
                pDevice->OrgMode  = HPNA;
                pDevice->LinkSpeed = LINK_SPEED_10MBPS;
                pDevice->LinkDuplex = LINK_DUPLEX_HALF;
                pExtension->OutRegisterData[0]=HPNA_GPIO;
                pExtension->OutRegisterData[1]=pDevice->Testpin;
                pExtension->OutRegisterData[2]=PII_HPNA;
                Status = LM_SetRegister (pDevice, 3, 0x7F );
				if( Status != LM_STATUS_SUCCESS ){ goto lExit; }

            } else {
//              DbgMessage(INFORM, ("None Above, Force Ethernet\n"));
//              pExtension->OutRegisterData[0]=ETHER_GPIO;
//              pExtension->OutRegisterData[1]=0;
//              Status = LM_SetRegister (pDevice, 2, 0x7F);
                pDevice->PhyAddr=1;

            }
        }
    }else{
        switch(pDevice->RequestedSelectMedia) {
        case SELECT_MEDIA_ETHERNET:
//          DbgMessage(INFORM, ("Forcing Ethernet\n"));

#ifdef DEBCEINIT // add - 08/19/05,bolo    
    // unmark - 09/05/05,bolo
    NKDbgPrintfW(TEXT("LM_AutoSelectMedia: Forcing Ethernet !\n\r"));
#endif
            pDevice->PhyAddr=ETHERNET;
            Temp = ETHER_GPIO; //GPIO2 = 1  GPIO3=0
            Temp1 = PII_ETHER; //Tri-state MII
            pDevice->CurrMode = ETHERNET;
            pDevice->OrgMode  = ETHERNET;
            break;

        case SELECT_MEDIA_HOMELAN:
//          DbgMessage(INFORM, ("Forcing Home Lan\n"));

#ifdef DEBCEINIT // add - 08/19/05,bolo   
    // unmark - 09/05/05,bolo
    NKDbgPrintfW(TEXT("LM_AutoSelectMedia: Forcing Home LAN !\n\r"));
#endif
            pDevice->LinkSpeed = LINK_SPEED_10MBPS;
            pDevice->LinkDuplex = LINK_DUPLEX_HALF;
            pDevice->PhyAddr=2;
            Temp = HPNA_GPIO; //GPIO2 = 0  GPIO3=1
            Temp1 = PII_HPNA; //enable external MII
            pDevice->CurrMode = HPNA;
            pDevice->OrgMode  = HPNA;
            break;

        case SELECT_MEDIA_MAC_MII: // add - 08/19/05,bolo
//          DbgMessage(INFORM, ("Forcing MAC MII\n"));

#ifdef DEBCEINIT // add - 08/19/05,bolo    
    // unmark - 09/05/05,bolo
    NKDbgPrintfW(TEXT("LM_AutoSelectMedia: Forcing MAC MII !\n\r"));
#endif
            pDevice->LinkSpeed = LINK_SPEED_100MBPS;
            pDevice->LinkDuplex = LINK_DUPLEX_FULL;
            pDevice->PhyAddr = MACMII_PHYID; // mod - 08/25/05,bolo
            Temp = NONE_GPIO; // No GPIO setting
            Temp1 = PII_MACMII; //enable external MAC MII mode = 0x86
            pDevice->CurrMode = MACMII;
            pDevice->OrgMode  = MACMII;
            break;

        default:
//          DbgMessage(INFORM, ("None of Above\n"));

#ifdef DEBCEINIT // add - 08/19/05,bolo    
    // unmark - 09/05/05,bolo
    NKDbgPrintfW(TEXT("LM_AutoSelectMedia: Forcing Default Ethernet !\n\r"));
#endif
            pDevice->LinkSpeed = LINK_SPEED_100MBPS;
            pDevice->LinkDuplex = LINK_DUPLEX_HALF;
            pDevice->PhyAddr=1;
            Temp = ETHER_GPIO; //GPIO2 = 1  GPIO3=0
            Temp1 = PII_ETHER;
            pDevice->CurrMode = ETHERNET;
            pDevice->OrgMode  = ETHERNET;
            break;
        }  /* switch */
        pExtension->OutRegisterData[0]=Temp;
        pExtension->OutRegisterData[1]=pDevice->Testpin;
        pExtension->OutRegisterData[2]=Temp1;
        Status = LM_SetRegister (pDevice, 3, 0x7F );

        if (pDevice->CurrMode != MACMII) { // add - 08/25/05,bolo; avoid the bug for MAC MII mode

    		if( Status != LM_STATUS_SUCCESS )
            {
                #ifdef DEBCEINIT // add - 08/25/05,bolo   
                // unmark - 09/05/05,bolo
                NKDbgPrintfW(TEXT("LM_AutoSelectMedia: Set failed on GPIO & TestMode, Status=%d\n\r"),Status);
                #endif
                goto lExit;
            }
        }
    }

	Status = LM_STATUS_SUCCESS;

    #ifdef DEBCEINIT // add - 08/25/05,bolo
    // unmark - 09/05/05,bolo
	NKDbgPrintfW(TEXT("LM_AutoSelectMedia: O.K., LinkStatus=%d\n\r"),pDevice->LinkStatus);
    #endif


lExit:
//	if (Status != LM_STATUS_SUCCESS)
//		MM_FreeResources(pDevice);
	return Status;
} /* LM_AutoSelectMedia */
/******************************************************************************/
/* Description:                                                               */
/*    Issue a Bulk IN transfer to wait Rx data.                               */
/*    Queue a receive buffer for an incoming packet.                          */
//    1. Build a URB for Bulk IN transfer with the input packet
//    2. Allocate an IRP
//    3. Get the next IRP stack location and initialize it
//    4. Set a completion routine : LM_ReceiveComplete
//    5. Pass the IRP to the next device
/*                                                                            */
/* Returns:                                                                   */
/*    LM_STATUS_SUCCESS                                                       */
/*    LM_STATUS_FAILURE                                                       */
/******************************************************************************/
LM_STATUS LM_QueueRxPacket( PLM_DEVICE_INFO_BLOCK   pDevice,
                            PLM_PACKET_DESC         pPacket )
{
    PLM_DEVICE_EXTENSION_INFO   pExtension;
#ifndef NDISCE_MINIPORT
    PIO_STACK_LOCATION          pNextStack;
    PIRP                        pIrp;
    NTSTATUS                    NtStatus;
#else
    LM_STATUS                   Status;
#endif

    #ifdef LM_DEBUG_PC
      //DbgMessage(INFORM, (ADMTEXT("*** LM_QueueRxPacket Start ***\n")));
    #endif
 //    printf("*** LM_QueueRxPacket Start ***\n");
    #ifdef DEBCE // add - 05/31/05,bolo
    // mark - 09/05/05,bolo
    // printf("LQRP: pkt=0x%x,len=%d\n",pPacket,pPacket->PacketSize); // add - 05/26/06,bolo
    NKDbgPrintfW(TEXT("LQRP: pkt=0x%x,len=%d\n\r"),pPacket,pPacket->PacketSize); // add - 09/05/05,bolo 
    #endif
    #ifdef LM_DEBUG_CE
      printf("*** LM_QueueRxPacket Start ***\n");
    #endif
 //   printf("*** LM_QueueRxPacket Start ***\n");
    pExtension = (PLM_DEVICE_EXTENSION_INFO) pDevice->pLmDeviceExtension;
//printf("LM Q Rx Packet[%08lX] Rx Left [%d]\n",pPacket,LL_GetSize(&pDevice->RxPacketFreeList));
    // Build a Bulk IN transfer with the input packet
#ifndef NDISCE_MINIPORT
    UsbBuildInterruptOrBulkTransferRequest( (PURB) pPacket->pUrb,
                                            sizeof(struct _URB_BULK_OR_INTERRUPT_TRANSFER),
                                            pExtension->pUsbdInterface->Pipes[PIPE_TYPE_IN].PipeHandle,   // pipe 0 for Bulk IN
                                            pPacket->Buffer,
                                            NULL,
                                            MAX_PACKET_BUFFER_SIZE,                                       // 0x600
                                            USBD_TRANSFER_DIRECTION_IN | USBD_SHORT_TRANSFER_OK,
                                            NULL );
    #ifdef LM_DEBUG_PC
      DbgMessage(VERBOSE, (ADMTEXT("Bulk IN pipe handle =%08lx\n"),pExtension->pUsbdInterface->Pipes[PIPE_TYPE_IN].PipeHandle));
    #endif

    /* Allocate an IRP. */
    pIrp = IoAllocateIrp(pExtension->NextDeviceStackSize, FALSE);
    if(pIrp == NULL)
    {
        #ifdef LM_DEBUG_PC
          DbgMessage(FATAL, (ADMTEXT("IoAllocateIrp fail\n")));
        #endif
        return LM_STATUS_FAILURE;
    } /* if */

    // Get the next IRP stack location and initialize it
    pNextStack = IoGetNextIrpStackLocation(pIrp);
    pNextStack->MajorFunction = IRP_MJ_INTERNAL_DEVICE_CONTROL;
    pNextStack->Parameters.Others.Argument1 = pPacket->pUrb;
    pNextStack->Parameters.DeviceIoControl.IoControlCode = IOCTL_INTERNAL_USB_SUBMIT_URB;

    // Different from LM_CallUsbd, set a completion routine
    IoSetCompletionRoutine(pIrp, LM_ReceiveComplete, pPacket, TRUE, TRUE, TRUE);

    // Pass the IRP to the next device
    NtStatus = IoCallDriver(pExtension->NextDeviceObject, pIrp);

    if(NtStatus != STATUS_PENDING)
    {
        #ifdef LM_DEBUG_PC
          DbgMessage(FATAL, (ADMTEXT("IoCallDriver Fail Status = 0x%x\n"), NtStatus));
        #endif
        IoFreeIrp(pIrp);
        return LM_STATUS_FAILURE;
    } /* if */
#else
    //LockPages( pPacket->Buffer, MAX_PACKET_BUFFER_SIZE, NULL, 0 );
#ifdef NEW_VERSION
    Status = USBM_IssueBulkTransfer( pExtension->hPipeBulkIN,
                                     LM_ReceiveCompleteCE,
                                     pPacket,
                                     USB_NO_WAIT | USB_IN_TRANSFER | USB_SHORT_TRANSFER_OK,
                                     MAX_PACKET_BUFFER_SIZE,
                                     pPacket->Buffer,
                                     0,
                                     &pPacket->hTransfer );
		if( Status != LM_STATUS_SUCCESS ) {
			#ifdef DEBERR // add - 07/27/05,bolo
                // mark - 09/05/05,bolo
      	        // printf("LQRP: Issue Rx Bulk Transfer fail (NO WAIT)!\n");
                NKDbgPrintfW(TEXT("LQRP: Issue Rx Bulk Transfer fail (NO WAIT)!\n\r")); // add - 09/05/05,bolo
			#endif
    	return LM_STATUS_FAILURE;
		}
#else
    Status = USBM_IssueBulkTransfer( pExtension->hPipeBulkIN,
                                     NULL,
                                     NULL,
                                     USB_IN_TRANSFER | USB_SHORT_TRANSFER_OK,
                                     MAX_PACKET_BUFFER_SIZE,
                                     pPacket->Buffer,
                                     0,
                                     &pPacket->hTransfer );
    if( Status != LM_STATUS_SUCCESS ){
			#ifdef LM_DEBUG_PC
				NKDbgPrintfW(TEXT("USBM_IssueBulkTransfer fails in LM_QueueRxPacket\r\n"));
  		    #endif
			#ifdef DEBERR // add - 07/27/05,bolo
                // mark - 09/05/05,bolo
              	// printf("LQRP: Issue Rx Bulk Transfer fail !\n"); // mod - 07/27/05,add - 05/26/06,bolo
                NKDbgPrintfW(TEXT("LQRP: Issue Rx Bulk Transfer fail !\n\r")); // add - 09/05/05,bolo
			#endif
    	return LM_STATUS_FAILURE;
		}
Status = LM_ReceiveCompleteCE(pPacket);
    if( Status != 0 ){
//NKDbgPrintfW(TEXT("LM_ReceiveCompleteCE fails in LM_QueueRxPacket\r\n"));
			#ifdef DEBERR // add - 07/27/05,bolo
                // mark - 09/05/05,bolo
    		    // printf("LQRP: LM_ReceiveCompleteCE fail !\n"); // mod - 07/27/05,add - 05/26/06,bolo
                NKDbgPrintfW(TEXT("LQRP: LM_ReceiveCompleteCE fail !\n\r")); // add - 09/05/05,bolo
			#endif
    /* mark - 06/01/05,bolo
    LL_PushTail(&pDevice->RxPacketFreeList, &pPacket->Link);
    */
//printf("LM_ReceiveCompleteCE fails in LM_QueueRxPacket\n");
   return LM_STATUS_FAILURE;
}
#endif
    pDevice->RxIssueCount ++;
    //DbgMessage(INFORM, (ADMTEXT(" Rx Issue Count = %d\n"), pDevice->RxIssueCount));
  //printf(" Rx Issue Count = %d\n", pDevice->RxIssueCount);
#endif

//    DbgMessage(INFORM, (ADMTEXT("*** LM_QueueRxPacket End ***\n");
    #ifdef LM_DEBUG_CE
      #if 0
      printf("Rx PacketSize=%x\n",pPacket->PacketSize);
	  printf("===============================================\n");
	  for (DebugCount=0;DebugCount<(pPacket->PacketSize);DebugCount++)
	  {
	    if ( ((DebugCount%16) == 0 )  && ( DebugCount != 0 ) )
	      printf("\n");
	    printf("%x ",pPacket->Buffer[DebugCount]);
	  }
	  #endif
    #endif

    return LM_STATUS_SUCCESS;
} /* LM_QueueRxPacket */

/******************************************************************************/
/* Description:                                                               */
/*    This routine puts a packet on the wire if there is a transmit DMA       */
/*    descriptor available; otherwise the packet is queued for later          */
/*    transmission.  If the second argue is NULL, this routine will put       */
/*    the queued packet on the wire if possible.                              */
/*                                                                            */
/* Return:                                                                    */
/*    LM_STATUS_FAILURE                                                       */
/*    LM_STATUS_SUCCESS                                                       */
/******************************************************************************/
LM_STATUS LM_SendPacket( PLM_DEVICE_INFO_BLOCK   pDevice,
                         PLM_PACKET_DESC         pPacket )
{
    PLM_DEVICE_EXTENSION_INFO   pExtension;
    int i=0; // mark - 06/07/05,bolo,Size;
#ifndef NDISCE_MINIPORT
    PIO_STACK_LOCATION          pNextStack;
    PIRP                        pIrp, pIrp1;
    NTSTATUS                    NtStatus;
    PLM_PACKET_DESC             TempPacket;
#else
//    USB_TRANSFER                hTransfer;  delete by Y.M 01/05/07
    LM_STATUS                   Status;
//  WORD                        EpStatus;
#endif

    #ifdef LM_DEBUG_CE
      DbgMessage(VERBOSE, (ADMTEXT("*** LM_SendPacket Start ***\n")));
    #endif
//    printf("LM Start Send Packet \n");
    if(pDevice->AdapterStatus != LM_STATUS_ADAPTER_OPEN)
    {
        #ifdef LM_DEBUG_CE
        	printf("Adapter is not opened\n");
        #endif

        #ifdef LM_DEBUG_PC
          DbgMessage(VERBOSE, (ADMTEXT("Adapter is not opened\n")));
        #endif
        if( Unplugged )
            return LM_STATUS_FAILURE;
        else
            pDevice->AdapterStatus = LM_STATUS_ADAPTER_OPEN;
    }

    pExtension = (PLM_DEVICE_EXTENSION_INFO) pDevice->pLmDeviceExtension;
    if ( pDevice->FixFlag != 1)
    {
        #ifdef LM_DEBUG_PC
          DbgMessage(VERBOSE, (ADMTEXT("pDevice->FixFlag = 0 ...\n")));
        #endif

//        printf("1");
        // copy pNdisBuffer data to our buffer
        MM_SetupTxPacket(pDevice, pPacket);
        //printf("2");

        // Baushg Workaround chipset bug, 1999.01.21
        // AN986 has a problem while transmit Ethernet frame size is equal to 64*N,
        // where N=1,2,3...., at this time, AN986 will wait another USB packet (NULL
        // packet) to send the Ethernet frame.
        // An easy way to workaround this problem is to let the Ethernet frame size
        // not equal to 64*N.  But this packet will be treat as length error in the
        // network.  Hope we can solve this problem at second cut NIC.

        if ( pDevice->ChipType == 0 )  //jackl2002_1104
        {

	    /*
	        if ((pPacket->PacketSize % 64)==0)

	        {
	            #ifdef LM_DEBUG_CE
	            	printf("Bug : 64N+2\n");
	            #endif

	            pPacket->PacketSize+=2;
	   //       pDevice->FixFlag =1;
	            #ifdef LM_DEBUG_PC
	              DbgMessage(INFORM, (ADMTEXT("Bug : 64N+2\n")));
	            #endif
	            //printf("$$$$$64N+2$$$$$\n");
	        }
	     */

            pDevice->FixFlag = 0;
        }
        //-----------------------------------------end, 1999.01.21

        if ((pPacket->PacketSize % 64)==0)
        {
            pDevice->FixFlag = 1;
            
            // DbgMessage(VERBOSE, (TEXT("Tx packet size is 64*N!\n"))); // mark - 06/22/05,06/03/05,bolo

            #ifdef DEBTXERR // mod - 07/27/05,add - 06/22/05,bolo
                // mark - 09/05/05,bolo
            	// printf("Tx packet size is 64*N!\n"); // unmark - 06/03/05,bolo
                NKDbgPrintfW(TEXT("Tx packet size is 64*N!\n\r")); // add - 09/05/05,bolo                
            #endif
	    }

        /* Set minimum ethernet packet size. */
        if(pPacket->PacketSize < MIN_ETHERNET_PACKET_SIZE)
            pPacket->PacketSize = MIN_ETHERNET_PACKET_SIZE;
    }
    else
    {
        // Baushg Workaround chipset bug, 1999.01.21
    	// Send a zero size packet to device to fix the bug
        #ifdef LM_DEBUG_PC
          DbgMessage(VERBOSE, (ADMTEXT("pDevice->FixFlag == 1\n")));
        #endif

        // DbgMessage(VERBOSE, (TEXT("Set tx zero packet !\n"))); // mark - 06/22/05,06/03/05,bolo
        #ifdef DEBTXERR // mod - 07/27/05,add - 06/22/05,bolo
            // mark - 09/05/05,bolo
        	// printf("Set tx zero packet !, PktSize=%d\n",pPacket->PacketSize); // add - 06/03/05,bolo
            NKDbgPrintfW(TEXT("Set tx zero packet !, PktSize=%d\n\r"),pPacket->PacketSize); // add - 09/05/05,bolo   
        #endif

        pDevice->FixFlag = 0;
        pPacket->PacketSize = 0;
    }
    //-----------------------------------------end, 1999.01.21
 //   printf("T[%04d] %02x %02x\n",pPacket->PacketSize,pPacket->Buffer[0x34],pPacket->Buffer[0x35]);
    //jackl2002_1104 add for 8513  length < 60
    if( pPacket->Buffer[0] <60 && pPacket->Buffer[1]== 0 )
    {
    	pPacket->Buffer[0]=60;
	}

    #ifdef LM_DEBUG_CE
      printf("Tx PacketSize=%x\n",pPacket->PacketSize);
      printf("pacet[0]%x packet[1]%x \n ",pPacket->Buffer[0], pPacket->Buffer[1]);
	  printf("===============================================\n");
	  for (DebugCount=0;DebugCount<(pPacket->PacketSize);DebugCount++)
	  {
	   if ( ((DebugCount%16) == 0 ) && ( DebugCount != 0 ) )
	      printf("\n");
	   printf("%x ",pPacket->Buffer[DebugCount]);
      }
	  printf("\n");
	#endif

    /* Build an URB. */
#ifndef NDISCE_MINIPORT
    UsbBuildInterruptOrBulkTransferRequest( (PURB) pPacket->pUrb,
                                            sizeof(struct _URB_BULK_OR_INTERRUPT_TRANSFER),
                                            pExtension->pUsbdInterface->Pipes[PIPE_TYPE_OUT].PipeHandle,
                                            pPacket->Buffer,
                                            NULL,
                                            pPacket->PacketSize,
                                            0,
                                            NULL );
    pIrp = IoAllocateIrp(pExtension->NextDeviceStackSize, FALSE);
    if(pIrp == NULL)
    {
        #ifdef LM_DEBUG_PC
          DbgMessage(FATAL, (ADMTEXT("IoAllocateIrp fail in LM_SendPacket\n")));
        #endif
        return LM_STATUS_FAILURE;
    } /* if */

    /* Initialize the lower layered driver's stack area. */
    pNextStack = IoGetNextIrpStackLocation(pIrp);
    pNextStack->MajorFunction = IRP_MJ_INTERNAL_DEVICE_CONTROL;
    pNextStack->Parameters.Others.Argument1 = pPacket->pUrb;
    pNextStack->Parameters.DeviceIoControl.IoControlCode = IOCTL_INTERNAL_USB_SUBMIT_URB;

    /* Set up a completion routine. */
    IoSetCompletionRoutine( pIrp, LM_TransmitComplete, pPacket, TRUE, TRUE, TRUE );

    /* Pass the IRP to the next device. */
    NtStatus = IoCallDriver(pExtension->NextDeviceObject, pIrp);
    if(NtStatus != STATUS_PENDING || pDevice->AdapterStatus != LM_STATUS_ADAPTER_OPEN)
    {
        #ifdef LM_DEBUG_PC
          DbgMessage(FATAL, (ADMTEXT("IoCallDriver fail in LM_SendPacket: 0x%x\n"), NtStatus));
        #endif
        return LM_STATUS_FAILURE;
    } /* if */
#else

#ifdef NEW_VERSION // mod - 06/07/05,bolo; original value: #if  1
    // Size = pPacket->PacketSize; mark - 06/07/05,bolo
//  printf("(!%d=",pPacket->PacketSize);
//    printf(" TX PKT=");
//    for (i=2;i<Size;i++)
//      printf("%02X ",pPacket->Buffer[i]);

//    printf("&&&\n");
    Status = USBM_IssueBulkTransfer( pExtension->hPipeBulkOUT,
                                     LM_TransmitCompleteCE,
                                     pPacket,
                                     USB_NO_WAIT | USB_OUT_TRANSFER | USB_SHORT_TRANSFER_OK,
                                     pPacket->PacketSize,
                                     pPacket->Buffer,
                                     0,
//                                     &hTransfer );              change by Y.M 01/04/13
                                     &pPacket->hTransfer );


    if( Status != LM_STATUS_SUCCESS )
    {
        pPacket->hTransfer = NULL;
        #ifdef LM_DEBUG_CE
        printf("Issue Tx Bulk Transfer fail\n");
        #endif
        #ifdef LM_DEBUG_PC
          DbgMessage(VERBOSE, (ADMTEXT("Issue Tx Bulk Transfer fail\n")));
        #endif
        // DbgMessage(VERBOSE, (TEXT("Issue Tx Bulk Transfer fail\n"))); // mark - 07/27/05,add - 06/03/05,bolo
        #ifdef DEBERR // add - 07/27/05,bolo
            // mark - 09/05/05,bolo
        	// printf("Issue Tx Bulk Transfer fail (NO WAIT)!\n"); // add - 06/03/05,bolo
            NKDbgPrintfW(TEXT("Issue Tx Bulk Transfer fail (NO WAIT)!\n\r")); // add - 09/05/05,bolo
		#endif
        return LM_STATUS_FAILURE;
    }

#else
    Status = USBM_IssueBulkTransfer( pExtension->hPipeBulkOUT,
                                     NULL,
                                     NULL,
                                     USB_OUT_TRANSFER |  USB_SHORT_TRANSFER_OK,
                                     pPacket->PacketSize,
                                     pPacket->Buffer,
                                     0,
                                     // &hTransfer ); mark - 06/07/05,bolo
                                     &pPacket->hTransfer );
    if( Status != LM_STATUS_SUCCESS )
    {
        pPacket->hTransfer = NULL;
        #ifdef LM_DEBUG_CE
        printf("Issue Tx Bulk Transfer fail\n");
        #endif
        #ifdef LM_DEBUG_PC
        DbgMessage(VERBOSE, (ADMTEXT("Issue Tx Bulk Transfer fail\n")));
        #endif
        #ifdef DEBERR // add - 07/27/05,bolo
            // mark - 09/05/05,bolo
        	// printf("Issue Tx Bulk Transfer fail !\n");
            NKDbgPrintfW(TEXT("Issue Tx Bulk Transfer fail !\n\r")); // add - 09/05/05,bolo
		#endif
        return LM_STATUS_FAILURE;
    }
Status = LM_TransmitCompleteCE(pPacket);

    if( Status != 0 )
    {
        pPacket->hTransfer = NULL;
        #ifdef DEBERR // add - 07/27/05,bolo
            // mark - 09/05/05,bolo
        	// printf("LM_TransmitCompleteCE fail\n");
            NKDbgPrintfW(TEXT("LM_TransmitCompleteCE fail\n\r")); // add - 09/05/05,bolo
        #endif
        // DbgMessage(VERBOSE, (ADMTEXT("LM_TransmitCompleteCE fail\n"))); // mark - 07/27/05,bolo
        return LM_STATUS_FAILURE;
    }
#endif

//    pPacket->hTransfer = hTransfer;    delete by Y.M 01/05/07
    pDevice->TxIssueCount ++;

#endif
    #ifdef LM_DEBUG_PC
      DbgMessage(VERBOSE, (ADMTEXT(" Tx Issue Count = %d\n"), pDevice->TxIssueCount));
      DbgMessage(VERBOSE, (ADMTEXT("*** LM_SendPacket End ***\n")));
    #endif

    //printf("Lm Start Send End \n");
    return LM_STATUS_SUCCESS;
} /* LM_SendPacket */

/******************************************************************************/
/* Description:                                                               */
/*    This routine sets the receive control register according to ReceiveMask */
/*                                                                            */
/* Returns:                                                                   */
/*    LM_STATUS_SUCCESS                                                       */
/******************************************************************************/
LM_STATUS LM_SetReceiveMask( PLM_DEVICE_INFO_BLOCK   pDevice,
                             DWORD                   ReceiveMask )
{
    PLM_DEVICE_EXTENSION_INFO   pExtension;
    USHORT                      temp_data0;
#ifndef NDISCE_MINIPORT
    LM_STATUS                   Status;
    KIRQL                       oldirq;
#endif
	LM_STATUS Status;


    #ifdef LM_DEBUG_PC
      DbgMessage(INFORM, (ADMTEXT("*** LM_SetReceiveMask ***\n")));
    #endif

    pExtension = (PLM_DEVICE_EXTENSION_INFO) pDevice->pLmDeviceExtension;

    temp_data0 = 0;

    // Set rx_multicast_all if required
    if (ReceiveMask & RECEIVE_MASK_ACCEPT_MULTI_PROM)
        pDevice->Ctrl0Data |= rx_multicast_all;   // 0x02
    else
        pDevice->Ctrl0Data &= ~rx_multicast_all;

    // Set promiscuous if required
    if (ReceiveMask & RECEIVE_MASK_PROMISCUOUS_MODE)
        temp_data0 = Promiscuous;   // 0x04, receive any packet

#ifndef NDISCE_MINIPORT
    oldirq = KeGetCurrentIrql();
    KeLowerIrql(PASSIVE_LEVEL);
#endif
    pExtension->OutRegisterData[0]=pDevice->Ctrl0Data;
    pExtension->OutRegisterData[1]=pDevice->Ctrl1Data;
    pExtension->OutRegisterData[2]=(UCHAR)temp_data0;
    Status = LM_SetRegister (pDevice, 3, 0x0 );

#ifndef NDISCE_MINIPORT
    KeRaiseIrql(oldirq,&oldirq);
#endif

	return Status;
} /* LM_SetReceiveMask */

/******************************************************************************/
/* Description:                                                               */
/*    Passes an URB to the USBD class driver.                                 */
/*                                                                            */
/* Returns:                                                                   */
/*    LM_STATUS_SUCCESS                                                       */
/******************************************************************************/
#ifndef NDISCE_MINIPORT
STATIC LM_STATUS LM_CallUsbd( PLM_DEVICE_INFO_BLOCK   pDevice,
                              PURB                    pUrb )
{
    PLM_DEVICE_EXTENSION_INFO   pExtension;
    PIRP                        pIrp;
    KEVENT                      KeEvent;
    IO_STATUS_BLOCK             IoStatusBlock;
    PIO_STACK_LOCATION          pNextStack;
    NTSTATUS                    NtStatus, status;
    LARGE_INTEGER               dueTime;

    #ifdef LM_DEBUG_PC
      DbgMessage(VERBOSE, (ADMTEXT("*** LM_CallUsbd ***\n")));
    #endif

    #ifdef LM_DEBUG_CE
    printf("*** LM_CallUsbd Start ***\n");
    #endif

    pExtension = (PLM_DEVICE_EXTENSION_INFO) pDevice->pLmDeviceExtension;

    /* issue a synchronous request. */
    KeInitializeEvent(&KeEvent, NotificationEvent, FALSE);

    /* Build an Irp to pass to the lower layered driver. */
    pIrp = IoBuildDeviceIoControlRequest( IOCTL_INTERNAL_USB_SUBMIT_URB,
                                          pExtension->NextDeviceObject,
                                          NULL,
                                          0,
                                          NULL,
                                          0,
                                          TRUE,
                                          &KeEvent,
                                          &IoStatusBlock );

    /* Call the class driver to perform the operation.  If the returned */
    /* status is PENDING, wait for the request to complete. */
    pNextStack = IoGetNextIrpStackLocation(pIrp);
    if(pNextStack == NULL)
    {
        #ifdef LM_DEBUG_PC
          DbgMessage(FATAL, (ADMTEXT("IoGetNextIrpStackLocation Fail\n")));
        #endif
        return LM_STATUS_FAILURE;
    } /* if */

    /* pass the URB to the USB driver stack. */
    pNextStack->Parameters.Others.Argument1 = pUrb;

    /* Call the lower layered driver. */
    NtStatus = IoCallDriver(pExtension->NextDeviceObject, pIrp);
    if(NtStatus == STATUS_PENDING)
    {
        // modify by LIC 6.8.99-----------------------------------
        dueTime.QuadPart = -10000 * 5000;
        status = KeWaitForSingleObject(&KeEvent, Suspended, KernelMode, FALSE, &dueTime);
        if(status == STATUS_TIMEOUT)
        {
            IoCancelIrp(pIrp);
            NtStatus = STATUS_IO_TIMEOUT;
            return NtStatus;
        }
        //--------------------------------------------------------
    } /* if */

    return LM_STATUS_SUCCESS;
} /* LM_CallUsbd */

//-------
STATIC LM_STATUS LM_CallUsbdIOCTL( PLM_DEVICE_INFO_BLOCK   pDevice,
                                   ULONG                   IOCTL_COMMAND )
{
    PLM_DEVICE_EXTENSION_INFO   pExtension;
    PIRP                        pIrp;
    KEVENT                      KeEvent;
    IO_STATUS_BLOCK             IoStatusBlock;
    PIO_STACK_LOCATION          pNextStack;
    NTSTATUS                    NtStatus, status;
    LARGE_INTEGER               dueTime;

    #ifdef LM_DEBUG_PC
      DbgMessage(VERBOSE, (ADMTEXT("*** LM_CallUsbdIOCTL ***\n")));
    #endif

    #ifdef LM_DEBUG_CE
    printf("*** LM_CallUsbdIOCTL Start ***\n");
    #endif

    pExtension = (PLM_DEVICE_EXTENSION_INFO) pDevice->pLmDeviceExtension;

    /* issue a synchronous request. */
    KeInitializeEvent(&KeEvent, NotificationEvent, TRUE);

    /* Build an Irp to pass to the lower layered driver. */
    pIrp = IoBuildDeviceIoControlRequest( IOCTL_COMMAND,
                                          pExtension->NextDeviceObject,
                                          NULL,
                                          0,
                                          NULL,
                                          0,
                                          TRUE,
                                          &KeEvent,
                                          &IoStatusBlock );

    /* Call the class driver to perform the operation.  If the returned */
    /* status is PENDING, wait for the request to complete. */
    pNextStack = IoGetNextIrpStackLocation(pIrp);
    if(pNextStack == NULL)
    {
        #ifdef LM_DEBUG_PC
          DbgMessage(FATAL, (ADMTEXT("IoGetNextIrpStackLocation Fail\n")));
        #endif
        return LM_STATUS_FAILURE;
    } /* if */

    /* pass the URB to the USB driver stack. */
    //pNextStack->Parameters.Others.Argument1 = NULL;
    //DbgBreak();

    // delay        // for 770ED
    dueTime.QuadPart = 10000000;
    KeDelayExecutionThread(KernelMode, FALSE, &dueTime);

    /* Call the lower layered driver. */
    NtStatus = IoCallDriver(pExtension->NextDeviceObject, pIrp);
    if(NtStatus == STATUS_PENDING)
    {
        // modify by LIC 6.8.99----------------------------
        dueTime.QuadPart = -10000 * 5000;
        //printf("%d\n",dueTime.QuadPart);
        status = KeWaitForSingleObject(&KeEvent, Suspended, KernelMode, FALSE, &dueTime);
        if(status == STATUS_TIMEOUT)
        {
            IoCancelIrp(pIrp);
            NtStatus = STATUS_IO_TIMEOUT;
            return NtStatus;
        }
        //-------------------------------------------------
    } /* if */
    return LM_STATUS_SUCCESS;
} /* LM_CallUsbdIOCTL */
#endif

/******************************************************************************/
/* Description:                                                               */
/*    Received packet Completion Routine                                      */
//    1. Check length(packet size) & status appended to packet data by HW
//    2. Free IRP
//    3. If there is no error, call MM_IndicateRxPacket to Set timer to wait for indication routine & issue another Bulk transfer
//    4. Else call LM_QueueRxPacket to Issue another Bulk IN transfer to wait Rx data with original packet buffer
/*                                                                            */
/* Returns:                                                                   */
/*    STATUS_MORE_PROCESSING_REQUIRED;                                        */
/******************************************************************************/
#ifndef NDISCE_MINIPORT
STATIC NTSTATUS LM_ReceiveComplete( PDEVICE_OBJECT   DeviceObject,
                                    PIRP             pIrp,
                                    PVOID            Context )
{
    PLM_DEVICE_INFO_BLOCK                      pDevice;
    PLM_PACKET_DESC                            pPacket;
    struct _URB_BULK_OR_INTERRUPT_TRANSFER *   pBulkUrb;
    USHORT                                     length, status;
    ULONG                                      offset;
#if WORDAROUND
    WORD                                       i;
#endif

    #ifdef LM_DEBUG_PC
      DbgMessage(VERBOSE, (ADMTEXT("*** LM_ReceiveComplete Start ***\n")));
    #endif

    pPacket = (PLM_PACKET_DESC) Context;
    pDevice = pPacket->pDevice;
    if(pDevice->AdapterStatus != LM_STATUS_ADAPTER_OPEN)
    {
        IoCancelIrp(pIrp);
        return LM_STATUS_FAILURE;
    } /* if */

    pPacket->PacketSize = 0;
    pPacket->PacketStatus = LM_STATUS_FAILURE;

    if(pIrp->IoStatus.Status == STATUS_SUCCESS)
    {
        pBulkUrb = &((PURB) pPacket->pUrb)->UrbBulkOrInterruptTransfer;

        if(pBulkUrb->TransferBufferLength)
        {
            offset=(pBulkUrb->TransferBufferLength)-4;           // see spec. page 10
            length = *((USHORT *) (pPacket->Buffer+offset));
            status = *((USHORT *) (pPacket->Buffer+offset+2));
            length = (offset& 0xFFF)-4;

            // Check if it is an error packet,long_packet,runt_packet,crc_error and dribbling
            if (status & 0x1e)
            {
                pPacket->PacketSize = 0;
                if (status & 0x2)
                    pDevice->RxInvalidSizeErrors ++;
                if (status & 0x4)
                    pDevice->RxInvalidSizeErrors ++;
                if (status & 0x8)
                    pDevice->RxCrcErrors ++;
                if (status & 0x10)
                    pDevice->RxFragListErrors ++;
                #ifdef LM_DEBUG_PC
                  DbgMessage(FATAL, (ADMTEXT("Rx Packet Error = 0x%x\n"), status));
                #endif
            }
            else
            {
                pPacket->PacketSize = length;
                pDevice->RxPacketCount ++;
            }
        } /* if */
        pPacket->PacketStatus = LM_STATUS_SUCCESS;
    } // if(pIrp->IoStatus.Status == STATUS_SUCCESS)
    else
    {
        pDevice->AdapterStatus = LM_STATUS_ADAPTER_REMOVED;
        #ifdef LM_DEBUG_PC
          DbgMessage(FATAL, (ADMTEXT("Receive completion fail status = 0x%x, Bulk Transfer buffer = 0x%x, Packet buffer = 0x%x\n"), pIrp->IoStatus.Status, pBulkUrb->TransferBuffer, pPacket->Buffer));
        #endif
    } /* if */

    IoFreeIrp(pIrp);

    /* Indicate rx complete status. */
    if(pPacket->PacketStatus == LM_STATUS_SUCCESS)
    {
        if(pPacket->PacketSize)
            // Set timer to wait for indication routine & issue another Bulk transfer
            MM_IndicateRxPacket(pDevice, pPacket);
        else
            // Issue another Bulk IN transfer to wait Rx data with original packet buffer
            //printf("2");
            LM_QueueRxPacket(pDevice, pPacket);
    } /* if */

    #ifdef LM_DEBUG_PC
      DbgMessage(VERBOSE, (ADMTEXT("*** LM_ReceiveComplete End ***\n")));
    #endif
    return STATUS_MORE_PROCESSING_REQUIRED;
} /* LM_ReceiveComplete */

#else

DWORD CALLBACK LM_ReceiveCompleteCE( LPVOID lpvNotifyParameter )
{
    PLM_DEVICE_INFO_BLOCK                      pDevice;
    PLM_PACKET_DESC                            pPacket;
    DWORD                                      ByteTransfered, Error;
    USHORT                                     length, status;
    ULONG                                      offset;

    #ifdef RX_DEBUG_PC
      DbgMessage(VERBOSE, (ADMTEXT("*** LM_ReceiveCompleteCE Start ***\n")));
    #endif


//    printf("*** LM_ReceiveCompleteCE Start ***\n");

    #ifdef RX_DEBUG_CE
    printf("*** LM_ReceiveCompleteCE Start ***\n");
    #endif

    pPacket = (PLM_PACKET_DESC) lpvNotifyParameter;
//NKDbgPrintfW(TEXT("Callback:pPacket = %X:%X\r\n"),pPacket,pPacket->hTransfer);

    #ifdef DEBCE // add - 06/06/05,bolo
    // mark - 09/05/05,bolo
    // printf("LRCCE: pkt=0x%x,len=%d\n",pPacket,pPacket->PacketSize);
    NKDbgPrintfW(TEXT("LRCCE: pkt=0x%x,len=%d\n\r"),pPacket,pPacket->PacketSize); // add - 09/05/05,bolo 
    #endif

    pDevice = pPacket->pDevice;
    pDevice->RxCompletedCount ++;
    #ifdef LM_DEBUG_PC
      //DbgMessage(VERBOSE, (ADMTEXT(" Rx Completed Count = %d\n"), pDevice->RxCompletedCount));
    #endif

    pPacket->PacketSize = 0;
    pPacket->PacketStatus = LM_STATUS_FAILURE;

    //UnlockPages( pPacket->Buffer, MAX_PACKET_BUFFER_SIZE );
/*
    if( pDevice->AdapterStatus != LM_STATUS_ADAPTER_OPEN )
    {
NKDbgPrintfW(TEXT("AdapterStatus != LM_STATUS_ADAPTER_OPEN\r\n"));
        USBM_CloseTransfer( pPacket->hTransfer );
        return LM_STATUS_FAILURE;
    }
*/
    if( LM_STATUS_FAILURE == USBM_GetTransferStatus( pPacket->hTransfer, &ByteTransfered, &Error ) )
    {
            #ifdef LM_DEBUG_PC
              DbgMessage(INFORM, (ADMTEXT("Receive not completed or completed with error, push Packet to free list\n")));
            #endif

        		#ifdef DEBERR // add - 07/27/05,bolo
            	// DbgMessage(VERBOSE, (TEXT("LRCCE: rx incomplete !\n"))); // mark - 07/27/05,add - 06/03/05,bolo
                // mark - 09/05/05,bolo
            	// printf("LRCCE: rx incomplete !, RxFree=%d, RxPkt=%d\n", LL_GetSize(&pDevice->RxPacketFreeList), LL_GetSize(&pDevice->RxPacketReceivedList)); // add - 06/03/05,bolo
                NKDbgPrintfW(TEXT("LRCCE: rx incomplete !, RxFree=%d, RxPkt=%d\n\r"), LL_GetSize(&pDevice->RxPacketFreeList), LL_GetSize(&pDevice->RxPacketReceivedList)); // add - 09/05/05,bolo
            #endif
            /* mark - 06/01/05,bolo
            NdisAcquireSpinLock(&((PUM_DEVICE_INFO_BLOCK)pDevice)->ReceiveQSpinLock);
            LL_PushTail(&pDevice->RxPacketFreeList, &pPacket->Link);
            */

            #ifdef LM_DEBUG_PC
              //DbgMessage(VERBOSE, (ADMTEXT("%x h Rx Free packets, %x h Received packets\n"), LL_GetSize(&pDevice->RxPacketFreeList), LL_GetSize(&pDevice->RxPacketReceivedList)));
            #endif
            //printf("%x h Rx Free packets, %x h Received packets\n",LL_GetSize(&pDevice->RxPacketFreeList), LL_GetSize(&pDevice->RxPacketReceivedList));

            /* mark - 06/01/05,bolo
            NdisReleaseSpinLock(&((PUM_DEVICE_INFO_BLOCK)pDevice)->ReceiveQSpinLock);
            */

			//v1.09 jackl mask 2003_0415 Ignore receive error packet
            //Unplugged = TRUE;
            //pDevice->AdapterStatus = LM_STATUS_ADAPTER_REMOVED;

            //USBM_CloseTransfer( pPacket->hTransfer );
            return LM_STATUS_FAILURE;
        //}
    }

    if( ByteTransfered )
    {
      //  printf("R[%04d] \n",ByteTransfered);
        offset = ByteTransfered - 4;           // see spec. page 10

		//jackl2002_1104 add for 8513
		if ( pDevice->ChipType == 0 )
		{
        	//chip is 8511 & 986
        	length = *((USHORT UNALIGNED *) (pPacket->Buffer + offset));
        	status = *((USHORT UNALIGNED *) (pPacket->Buffer + offset + 2));
        }
        else
        {
        	//chip is 8513
        	length = *((USHORT UNALIGNED *) (pPacket->Buffer ));
        	length = length - 8;
        	status = *((USHORT UNALIGNED *) (pPacket->Buffer + length + 6));
        }

   #ifdef LM_DEBUG_PC
    // if(length != ByteTransfered)
    //   DbgMessage(INFORM,(TEXT("LM_ReceiveCompleteCE:length != ByteTransfered\n")));
   #endif
//        length = (USHORT) (offset & 0xFFF) - 4;

		//jackl2002_1104 add for 8513
		if ( pDevice->ChipType == 0 )
		{
        	//chip is 8511 986
        	length = (USHORT) (offset & 0xFFF);
        }
        else
        {
        	//chip is 8513
        	length = (USHORT) (length & 0xFFF);
        }

 //     printf("Length=%d \n",length);
//tkdump(pPacket->Buffer, ByteTransfered);
#if 0
        printf(" Byte Transfer %04X \n",ByteTransfered);
        printf(" Recv Data = %02X ", *((USHORT UNALIGNED *) (pPacket->Buffer)));
        printf("  %02X ", *((USHORT UNALIGNED *) (pPacket->Buffer+2)));
        printf("  %02X ", *((USHORT UNALIGNED *) (pPacket->Buffer+4)));
        printf("  %02X ", *((USHORT UNALIGNED *) (pPacket->Buffer+6)));
        printf("  %02X ", *((USHORT UNALIGNED *) (pPacket->Buffer+8)));
        printf("  %02X ", *((USHORT UNALIGNED *) (pPacket->Buffer+10)));
        printf("  %02X \n", *((USHORT UNALIGNED *) (pPacket->Buffer+12)));
#endif
        // Check if it is an error packet,long_packet,runt_packet,crc_error and dribbling
        if (status & 0x1e)
        {
            pPacket->PacketSize = 0;
            if (status & 0x2)    pDevice->RxInvalidSizeErrors ++;
            if (status & 0x4)    pDevice->RxInvalidSizeErrors ++;
            if (status & 0x8)    pDevice->RxCrcErrors ++;
            if (status & 0x10)   pDevice->RxFragListErrors ++;
            //printf("Rx Packet Error=0x%x\n",status);
            #ifdef DEBERR // add - 07/27/05,bolo
            	// DbgMessage(VERBOSE, (TEXT("LRCCE: rx packet error !\n"))); // mark - 07/27/05,add - 06/03/05,bolo
                // mark - 09/05/05,bolo
            	// printf("LRCCE: rx packet error !, RxFree=%d, RxPkt=%d\n", LL_GetSize(&pDevice->RxPacketFreeList), LL_GetSize(&pDevice->RxPacketReceivedList)); // add - 06/03/05,bolo
                NKDbgPrintfW(TEXT("LRCCE: rx packet error !, RxFree=%d, RxPkt=%d\n\r"), LL_GetSize(&pDevice->RxPacketFreeList), LL_GetSize(&pDevice->RxPacketReceivedList)); // add - 09/05/05,bolo
            #endif
            #ifdef LM_DEBUG_PC
              DbgMessage(FATAL, (ADMTEXT("Rx Packet Error = 0x%x\n"), status));
            #endif
        }
        else
        {
            pPacket->PacketSize = length;
            pDevice->RxPacketCount ++;
        }
    }
#if 0
if(pPacket->PacketSize == 0){
NKDbgPrintfW(TEXT("no recursing\r\n"));
            NdisAcquireSpinLock(&((PUM_DEVICE_INFO_BLOCK)pDevice)->ReceiveQSpinLock);
            LL_PushTail(&pDevice->RxPacketFreeList, &pPacket->Link);
            NdisReleaseSpinLock(&((PUM_DEVICE_INFO_BLOCK)pDevice)->ReceiveQSpinLock);

            return LM_STATUS_FAILURE;
}
#endif

    pPacket->PacketStatus = LM_STATUS_SUCCESS;

#ifdef NEW_VERSION
    if(pPacket->PacketSize)
        // Set timer to wait for indication routine & issue another Bulk transfer
        MM_IndicateRxPacket(pDevice, pPacket);
    else {
        // Issue another Bulk IN transfer to wait Rx data with original packet buffer
        /* mark - 06/01/05,bolo
        LM_QueueRxPacket(pDevice, pPacket);
        */
        if(LM_STATUS_SUCCESS != LM_QueueRxPacket(pDevice, pPacket)) { // add - 06/01/05,bolo
            LL_PushTail(&pDevice->RxPacketFreeList, &pPacket->Link);
        }
    }
        
    USBM_CloseTransfer( pPacket->hTransfer );

#else
    USBM_CloseTransfer( pPacket->hTransfer );

    #ifdef DEBCE // add - 06/06/05,bolo
    // mark - 09/05/05,bolo
    // printf("LRCCE: USBMOK pkt=0x%x, len=%d\n",pPacket,pPacket->PacketSize);
    NKDbgPrintfW(TEXT("LRCCE: USBMOK pkt=0x%x, len=%d\n\r"),pPacket,pPacket->PacketSize); // add - 09/05/05,bolo 
    #endif

    m_pDevice = pDevice;
    m_pPacket = pPacket;

    SetEvent(resetEvent);
#endif
    #ifdef RX_LM_DEBUG_PC
      DbgMessage(INFORM, (ADMTEXT("*** LM_ReceiveCompleteCE End ***\n")));
    #endif
//    printf("*** LM_ReceiveCompleteCE End ***\n");
    return 0;
}
#endif

/******************************************************************************/
/* Description:                                                               */
/*    Transmit completion routine.                                            */
/*                                                                            */
/* Returns:                                                                   */
/*    STATUS_MORE_PROCESSING_REQUIRED;                                        */
/******************************************************************************/
#ifndef NDISCE_MINIPORT
STATIC NTSTATUS LM_TransmitComplete( PDEVICE_OBJECT   DeviceObject,
                                     PIRP             pIrp,
                                     PVOID            Context )
{
    PLM_DEVICE_INFO_BLOCK   pDevice;
    PLM_PACKET_DESC         pPacket;
    PIO_STACK_LOCATION      pNextStack;
    PIRP                    pIrp1;
    NTSTATUS                NtStatus;

    #ifdef LM_DEBUG_PC
      DbgMessage(VERBOSE, (ADMTEXT("*** LM_TransmitComplete ***\n")));
    #endif
    pPacket = (PLM_PACKET_DESC) Context;

    #ifdef LM_DEBUG_PC
      if(pPacket == NULL)
        DbgMessage(FATAL, (ADMTEXT("pPacket = NULL\n")));
    #endif

    pDevice = pPacket->pDevice;

    #ifdef LM_DEBUG_PC
      if(pDevice == NULL)
        DbgMessage(FATAL, (ADMTEXT("pDevice = NULL\n")));
      if(pIrp == NULL)
        DbgMessage(FATAL, (ADMTEXT("Invalid IRP\n")));
    #endif

    if(pIrp->IoStatus.Status == STATUS_SUCCESS)
    {
        pPacket->PacketStatus = LM_STATUS_SUCCESS;
        pDevice->TxPacketCount ++;
    }
    else
    {
        pPacket->PacketStatus = LM_STATUS_FAILURE;
        pDevice->AdapterStatus = LM_STATUS_ADAPTER_REMOVED;

        #ifdef LM_DEBUG_PC
          DbgMessage(FATAL, (ADMTEXT("LM_TransmitComplete fail status = 0x%x\n"), pIrp->IoStatus.Status));
        #endif
        IoCancelIrp(pIrp);
    } /* if */
    IoFreeIrp(pIrp);

    /* Indicate tx complete status. */
    MM_IndicateTxPacket(pDevice, pPacket);

    return STATUS_MORE_PROCESSING_REQUIRED;
} /* LM_TransmitComplete */

#else
DWORD CALLBACK LM_TransmitCompleteCE( LPVOID lpvNotifyParameter )
{
    PLM_DEVICE_INFO_BLOCK                      pDevice;
    PLM_PACKET_DESC                            pPacket;
//  DWORD                                      ByteTransfered;
    DWORD                                      Error;

    #ifdef TX_DEBUG_PC
      DbgMessage(INFORM, (ADMTEXT("*** LM_TransmitCompleteCE Start ***\n")));
    #endif

    #ifdef TX_DEBUG_CE
      printf("*** LM_TransmitCompleteCE Start ***\n");
    #endif

    //printf("Tx Complete Start\n");
    pPacket = (PLM_PACKET_DESC) lpvNotifyParameter;
    pDevice = pPacket->pDevice;
    pDevice->TxCompletedCount ++;

    #ifdef LM_DEBUG_PC
      DbgMessage(VERBOSE, (ADMTEXT(" Tx Completed Count = %d\n"), pDevice->TxCompletedCount));
    #endif

 // UnlockPages( pPacket->Buffer, pPacket->PacketSize );
//  if( pPacket->hTransfer == NULL )
    if( 0 )
    {
        //printf("1");
        #ifdef LM_DEBUG_PC
          DbgMessage(VERBOSE, (ADMTEXT("Error : Transfer handle is NULL\n")));
        #endif

        NdisAcquireSpinLock(&((PUM_DEVICE_INFO_BLOCK)pDevice)->TransmitQSpinLock);
        LL_PushTail(&pDevice->TxPacketFreeList, &pPacket->Link);
        ((PUM_DEVICE_INFO_BLOCK)pDevice)->TxDescLeft++;
        #ifdef LM_DEBUG_PC
          DbgMessage(VERBOSE, (ADMTEXT("%x h Tx Free packets\n"), LL_GetSize(&pDevice->TxPacketFreeList)));
        #endif
        NdisReleaseSpinLock(&((PUM_DEVICE_INFO_BLOCK)pDevice)->TransmitQSpinLock);

        //printf("[end]\n");
        return LM_STATUS_FAILURE;
    }
//  if( LM_STATUS_FAILURE == USBM_GetTransferStatus( pPacket->hTransfer, &ByteTransfered, &Error ) )
  if( 0 )
    {
//  printf(" &%d)",ByteTransfered);
        //printf("2");
        if( Error )
        {
            #ifdef LM_DEBUG_PC
              DbgMessage(VERBOSE, (ADMTEXT("Transmit completed with error, push Packet to free list\n")));
            #endif
            NdisAcquireSpinLock(&((PUM_DEVICE_INFO_BLOCK)pDevice)->TransmitQSpinLock);
            LL_PushTail(&pDevice->TxPacketFreeList, &pPacket->Link);
            ((PUM_DEVICE_INFO_BLOCK)pDevice)->TxDescLeft++;

            #ifdef LM_DEBUG_PC
            DbgMessage(VERBOSE, (ADMTEXT("%x h Tx Free packets\n"), LL_GetSize(&pDevice->TxPacketFreeList)));
            #endif
            NdisReleaseSpinLock(&((PUM_DEVICE_INFO_BLOCK)pDevice)->TransmitQSpinLock);

            pPacket->PacketStatus = LM_STATUS_FAILURE;
            pDevice->AdapterStatus = LM_STATUS_ADAPTER_REMOVED;

            USBM_CloseTransfer( pPacket->hTransfer );
        //printf("[end]\n");
            return LM_STATUS_FAILURE;
        }
        //printf("4");
    }

    pPacket->PacketStatus = LM_STATUS_SUCCESS;
    pDevice->TxPacketCount ++;

    /* Indicate tx complete status. */
        //printf("5");
    MM_IndicateTxPacket(pDevice, pPacket);
        //printf("6");

    USBM_CloseTransfer( pPacket->hTransfer );
    //printf("Lm Transmit Complete End \n");
    #ifdef TX_DEBUG_PC
      DbgMessage(INFORM, (ADMTEXT("*** LM_TransmitCompleteCE End ***\n")));
    #endif
    return 0;
}
#endif

/******************************************************************************/
/* Description:                                                               */
/*    Get the MAC registers                                                   */
/*                                                                            */
/* Returns:                                                                   */
/*    STATUS_MORE_PROCESSING_REQUIRED;                                        */
/******************************************************************************/
STATIC LM_STATUS LM_GetRegister( IN  PLM_DEVICE_INFO_BLOCK   pDevice,
                                 IN  USHORT                  Count,    // Length to get
                                 IN  USHORT                  Index)	   // Register Index
{
    PLM_DEVICE_EXTENSION_INFO   pExtension;
    LM_STATUS                   Status;
#ifndef NDISCE_MINIPORT
    URB                         urb;
#else
    USB_DEVICE_REQUEST          UsbDeviceReq;
#endif

    pExtension = (PLM_DEVICE_EXTENSION_INFO) pDevice->pLmDeviceExtension;
#ifndef NDISCE_MINIPORT
    UsbBuildVendorRequest( &urb,
                           URB_FUNCTION_VENDOR_DEVICE,
                           sizeof(struct _URB_CONTROL_VENDOR_OR_CLASS_REQUEST),
                           (USBD_TRANSFER_DIRECTION_IN),
                           0,
                           0xF0,
                           0,
                           Index,
                           pExtension->InRegisterData,   // Data Buffer
                           NULL,
                           Count,
                           NULL );
    Status = LM_CallUsbd(pDevice, &urb);
#else
    UsbDeviceReq.bmRequestType = USB_REQUEST_DEVICE_TO_HOST | USB_REQUEST_VENDOR;   // 0xc0
    UsbDeviceReq.bRequest      = 0xF0;
    UsbDeviceReq.wValue        = 0;
    UsbDeviceReq.wIndex        = Index;
    UsbDeviceReq.wLength       = Count;

    Status = USBM_IssueVendorTransfer( hUSB,USB_IN_TRANSFER,&UsbDeviceReq,pExtension->InRegisterData,0 );
#endif
    if(Status != LM_STATUS_SUCCESS)
    {
		Status = LM_STATUS_FAILURE;
    }


    return Status;
}

/******************************************************************************/
/* Description:                                                               */
/*    Set the MAC registers                                                   */
/*                                                                            */
/* Returns:                                                                   */
/*    STATUS_MORE_PROCESSING_REQUIRED;                                        */
/******************************************************************************/
STATIC LM_STATUS LM_SetRegister( IN  PLM_DEVICE_INFO_BLOCK   pDevice,
                                 IN  USHORT                  Count,
                                 IN  USHORT                  Index)
{
    PLM_DEVICE_EXTENSION_INFO   pExtension;
    LM_STATUS                   Status;
#ifndef NDISCE_MINIPORT
    URB                         urb;
#else
    USB_DEVICE_REQUEST          UsbDeviceReq;
#endif
    USHORT                      value;

    pExtension = (PLM_DEVICE_EXTENSION_INFO) pDevice->pLmDeviceExtension;
    if(Count==1)   value = pExtension->OutRegisterData[0];
    else           value = 0;
#ifndef NDISCE_MINIPORT
    UsbBuildVendorRequest( &urb,
                           URB_FUNCTION_VENDOR_DEVICE,
                           sizeof(struct _URB_CONTROL_VENDOR_OR_CLASS_REQUEST),
                           0,
                           0,
                           0xF1,
                           value,
                           Index,
                           pExtension->OutRegisterData,
                           NULL,
                           Count,
                           NULL );
    Status = LM_CallUsbd(pDevice, &urb);
#else
    UsbDeviceReq.bmRequestType = USB_REQUEST_HOST_TO_DEVICE | USB_REQUEST_VENDOR;   // 0x40
    UsbDeviceReq.bRequest      = 0xF1;
    UsbDeviceReq.wValue        = value;
    UsbDeviceReq.wIndex        = Index;
    UsbDeviceReq.wLength       = Count;


    Status = USBM_IssueVendorTransfer( hUSB,USB_OUT_TRANSFER,&UsbDeviceReq,pExtension->OutRegisterData,0 );
#endif
    if(Status != LM_STATUS_SUCCESS){
     //   NKDbgPrintfW(TEXT("USBM_IssueVendorTransfer fails in LM_SetRegister\r\n"));
		Status = Status;
	}
    return Status;
}


/******************************************************************************/
/* Description:                                                               */
/*    This routine to add Wake up Pattern                                     */
/*                                                                            */
/* Parameters:                                                                */
/*    PLM_DEVICE_INFO_BLOCK pDevice                                           */
/*                                                                            */
/* Return:                                                                    */
/*    VOID                                                                    */
/******************************************************************************/
void LM_AddWakeupPattern( PLM_DEVICE_INFO_BLOCK pDevice )
{
    DWORD                       MaskCount, Offset, Length;
    BYTE                        MaskVal, Temp, i, j, Data;
    BYTE                        WakeupMask[16];
    WORD                        WakeupCRC;
    BYTE                        Index, BufIndex;
    PLM_DEVICE_EXTENSION_INFO   pExtension;
    LM_STATUS                   Status;
#ifndef NDISCE_MINIPORT
    KIRQL                       oldirq;
#endif
    unsigned short              CRC=-1;
    unsigned short              crc_table[256] =
    {
        0x0000, 0x1021, 0x2042, 0x3063, 0x4084, 0x50A5, 0x60C6, 0x70E7,
        0x8108, 0x9129, 0xA14A, 0xB16B, 0xC18C, 0xD1AD, 0xE1CE, 0xF1EF,
        0x1231, 0x0210, 0x3273, 0x2252, 0x52B5, 0x4294, 0x72F7, 0x62D6,
        0x9339, 0x8318, 0xB37B, 0xA35A, 0xD3BD, 0xC39C, 0xF3FF, 0xE3DE,
        0x2462, 0x3443, 0x0420, 0x1401, 0x64E6, 0x74C7, 0x44A4, 0x5485,
        0xA56A, 0xB54B, 0x8528, 0x9509, 0xE5EE, 0xF5CF, 0xC5AC, 0xD58D,
        0x3653, 0x2672, 0x1611, 0x0630, 0x76D7, 0x66F6, 0x5695, 0x46B4,
        0xB75B, 0xA77A, 0x9719, 0x8738, 0xF7DF, 0xE7FE, 0xD79D, 0xC7BC,
        0x48C4, 0x58E5, 0x6886, 0x78A7, 0x0840, 0x1861, 0x2802, 0x3823,
        0xC9CC, 0xD9ED, 0xE98E, 0xF9AF, 0x8948, 0x9969, 0xA90A, 0xB92B,
        0x5AF5, 0x4AD4, 0x7AB7, 0x6A96, 0x1A71, 0x0A50, 0x3A33, 0x2A12,
        0xDBFD, 0xCBDC, 0xFBBF, 0xEB9E, 0x9B79, 0x8B58, 0xBB3B, 0xAB1A,
        0x6CA6, 0x7C87, 0x4CE4, 0x5CC5, 0x2C22, 0x3C03, 0x0C60, 0x1C41,
        0xEDAE, 0xFD8F, 0xCDEC, 0xDDCD, 0xAD2A, 0xBD0B, 0x8D68, 0x9D49,
        0x7E97, 0x6EB6, 0x5ED5, 0x4EF4, 0x3E13, 0x2E32, 0x1E51, 0x0E70,
        0xFF9F, 0xEFBE, 0xDFDD, 0xCFFC, 0xBF1B, 0xAF3A, 0x9F59, 0x8F78,
        0x9188, 0x81A9, 0xB1CA, 0xA1EB, 0xD10C, 0xC12D, 0xF14E, 0xE16F,
        0x1080, 0x00A1, 0x30C2, 0x20E3, 0x5004, 0x4025, 0x7046, 0x6067,
        0x83B9, 0x9398, 0xA3FB, 0xB3DA, 0xC33D, 0xD31C, 0xE37F, 0xF35E,
        0x02B1, 0x1290, 0x22F3, 0x32D2, 0x4235, 0x5214, 0x6277, 0x7256,
        0xB5EA, 0xA5CB, 0x95A8, 0x8589, 0xF56E, 0xE54F, 0xD52C, 0xC50D,
        0x34E2, 0x24C3, 0x14A0, 0x0481, 0x7466, 0x6447, 0x5424, 0x4405,
        0xA7DB, 0xB7FA, 0x8799, 0x97B8, 0xE75F, 0xF77E, 0xC71D, 0xD73C,
        0x26D3, 0x36F2, 0x0691, 0x16B0, 0x6657, 0x7676, 0x4615, 0x5634,
        0xD94C, 0xC96D, 0xF90E, 0xE92F, 0x99C8, 0x89E9, 0xB98A, 0xA9AB,
        0x5844, 0x4865, 0x7806, 0x6827, 0x18C0, 0x08E1, 0x3882, 0x28A3,
        0xCB7D, 0xDB5C, 0xEB3F, 0xFB1E, 0x8BF9, 0x9BD8, 0xABBB, 0xBB9A,
        0x4A75, 0x5A54, 0x6A37, 0x7A16, 0x0AF1, 0x1AD0, 0x2AB3, 0x3A92,
        0xFD2E, 0xED0F, 0xDD6C, 0xCD4D, 0xBDAA, 0xAD8B, 0x9DE8, 0x8DC9,
        0x7C26, 0x6C07, 0x5C64, 0x4C45, 0x3CA2, 0x2C83, 0x1CE0, 0x0CC1,
        0xEF1F, 0xFF3E, 0xCF5D, 0xDF7C, 0xAF9B, 0xBFBA, 0x8FD9, 0x9FF8,
        0x6E17, 0x7E36, 0x4E55, 0x5E74, 0x2E93, 0x3EB2, 0x0ED1, 0x1EF0
    };

    #ifdef LM_DEBUG_PC
      DbgMessage(VERBOSE, (ADMTEXT("*** LM_AddWakeupPattern ***\n")));
    #endif

    pExtension = (PLM_DEVICE_EXTENSION_INFO) pDevice->pLmDeviceExtension;
    MaskCount = pDevice->WakeupStru.MaskCount;   // MaskSize
    Offset = pDevice->WakeupStru.Offset;         // PatternOffset
    Length = pDevice->WakeupStru.Length;         // PatternSize

    // clear WakeupMask buffer
    for (i=0; i<16; i++)
        WakeupMask[i] = 0;

    // Caculate CRC of mask-bit bytes of wakeup pattern
    Temp =0;
    for (i=0; i<MaskCount; i++)
    {
        MaskVal=pDevice->PrivateWakeupBuffer[i+24];
        WakeupMask[i]=MaskVal;
        for (j=0; j<8; j++)
        {
            if ( MaskVal & 0x1)
            {
                Data=pDevice->PrivateWakeupBuffer[Offset+Temp];
                CRC = (CRC << 8) ^ crc_table[(CRC >> 8) ^ Data];   // CRC16 ???
            }
            MaskVal >>=1;
            Temp++;
        } //end for j
    } //end for i
    CRC=~CRC;
    WakeupCRC = CRC;

    // select which one is empty
    if ( !(pDevice->WakeCtrlReg & WAKEUPFRAME0_EN))   // bit 5 of wake-up control registry
    {
        BufIndex = 0;
        Index = 0x30;
        pDevice->WakeCtrlReg |= WAKEUPFRAME0_EN;
    }
    else
    {
        if ( !(pDevice->WakeCtrlReg & WAKEUPFRAME1_EN))
        {
            BufIndex = 1;
            Index = 0x48;
            pDevice->WakeCtrlReg |= WAKEUPFRAME1_EN;  // bit 5 of wake-up control registry
        }
        else
        {
            BufIndex = 2;
            Index = 0x60;
            pDevice->WakeCtrlReg |= WAKEUPFRAME2_EN;  // bit 5 of wake-up control registry
        }
    }

    // Set wakeup pattern to register
#ifndef NDISCE_MINIPORT
    oldirq = KeGetCurrentIrql();
    KeLowerIrql(PASSIVE_LEVEL);
#endif
    for (i=0;i<16;i++)
    {
        pDevice->WakeupMask[BufIndex][i] = pExtension->OutRegisterData[i] = WakeupMask[i];
    }
    pExtension->OutRegisterData[i]=0;
    pExtension->OutRegisterData[i+1]=(BYTE)(WakeupCRC & 0xFF);
    pExtension->OutRegisterData[i+2]=(BYTE)((WakeupCRC & 0xFF00)>>8);
    LM_SetRegister (pDevice, 19, Index );

    // Set wakeup control register
    pExtension->OutRegisterData[0]=pDevice->WakeCtrlReg;
    Status = LM_SetRegister (pDevice, 2, 0x78 );
#ifndef NDISCE_MINIPORT
    KeRaiseIrql(oldirq,&oldirq);
#endif
} /* LM_AddWakeupPattern */

/******************************************************************************/
/* Description:                                                               */
/*    This routine to Remove up Pattern                                       */
/*                                                                            */
/* Parameters:                                                                */
/*    PLM_DEVICE_INFO_BLOCK pDevice                                           */
/*                                                                            */
/* Return:                                                                    */
/*    VOID                                                                    */
/******************************************************************************/

void LM_RemoveWakeupPattern( PLM_DEVICE_INFO_BLOCK pDevice )
{
    DWORD                       MaskCount;
    PLM_DEVICE_EXTENSION_INFO   pExtension;
    BYTE                        i, EnableFlag=0;
#ifndef NDISCE_MINIPORT
    KIRQL                       oldirq;
#endif

    #ifdef LM_DEBUG_PC
      DbgMessage(VERBOSE, (ADMTEXT("*** LM_RemoveWakeupPattern ***\n")));
    #endif
    pExtension = (PLM_DEVICE_EXTENSION_INFO) pDevice->pLmDeviceExtension;

    MaskCount=pDevice->PrivateWakeupBuffer[8];

    for (i=0; i<MaskCount; i++)
    {
        if (pDevice->PrivateWakeupBuffer[i+24] != pDevice->WakeupMask[0][i])
            break;
        if (i == MaskCount-1)
        {
            #ifdef LM_DEBUG_PC
              DbgMessage(VERBOSE, (ADMTEXT("Remove Pattern 0\n")));
            #endif
            pDevice->WakeCtrlReg &= ~ WAKEUPFRAME0_EN;
        }
    }
    for (i=0;i<MaskCount;i++)
    {
        if (pDevice->PrivateWakeupBuffer[i+24] != pDevice->WakeupMask[1][i])
            break;
        if (i == MaskCount-1)
        {
            #ifdef LM_DEBUG_PC
              DbgMessage(VERBOSE, (ADMTEXT("Remove Pattern 1\n")));
            #endif
            pDevice->WakeCtrlReg &= ~ WAKEUPFRAME1_EN;
        }
    }
    for (i=0;i<MaskCount;i++)
    {
        if (pDevice->PrivateWakeupBuffer[i+24] != pDevice->WakeupMask[2][i])
            break;
        if (i == MaskCount-1)
        {
            #ifdef LM_DEBUG_PC
              DbgMessage(VERBOSE, (ADMTEXT("Remove Pattern 2\n")));
            #endif
            pDevice->WakeCtrlReg &= ~ WAKEUPFRAME2_EN;
        }
    }

#ifndef NDISCE_MINIPORT
    oldirq = KeGetCurrentIrql();
    KeLowerIrql(PASSIVE_LEVEL);
#endif
    pExtension->OutRegisterData[0]= pDevice->WakeCtrlReg;
    pExtension->OutRegisterData[1]= 0;
    LM_SetRegister (pDevice, 2, 0x78 );
#ifndef NDISCE_MINIPORT
    KeRaiseIrql(oldirq,&oldirq);
#endif
}     /* LM_RemoveWakeupPattern */

#ifndef NDISCE_MINIPORT
//---------------------------------------------------------------------------
// Function Prototype
//---------------------------------------------------------------------------
extern NTSTATUS MyAddDevice( IN PDRIVER_OBJECT   DriverObject,
                             IN PDEVICE_OBJECT   PhysicalDeviceObject );
extern NTSTATUS USB_Dispatch( IN PDEVICE_OBJECT   DeviceObject,
                              IN PIRP             Irp );

//---------------------------------------------------------------------------
// GLOBAL Variables
//---------------------------------------------------------------------------
PDRIVER_ADD_DEVICE   NdisAddDevice;
PDEVICE_OBJECT       PDO;
PDRIVER_DISPATCH     NdisPnpPower;

//--------------------------------------------------------------------------
// This function will save the AddDevice function pointer that NDIS has set
// up in the Driver Object and replace it with a pointer to our own
// AddDevice function pointer.
//--------------------------------------------------------------------------

extern VOID LM_HookAddDevice(PDRIVER_OBJECT DriverObject)
{
    //  --------------------------------------------------------------
    //  Save the pointer to NDIS'S AddDevice routine then hook our own
    //  ------------------------------------------------------------
    NdisAddDevice = DriverObject->DriverExtension->AddDevice;
    DriverObject->DriverExtension->AddDevice = MyAddDevice;
    NdisPnpPower = DriverObject->MajorFunction[IRP_MJ_PNP];
    DriverObject->MajorFunction[IRP_MJ_PNP] = USB_Dispatch;

    return;
}

//----------------------------------------------------------------------------
// This function is used to replace the AddDevice function the NDIS set up
// in the Driver Object.  The PDO is passed as a parameter to this function
// and we save it in a global variable until we are called at our miniport
// initialize function where we save the PDO in our context.
// After saving the PDO, we call the NDIS AddDevice function which we have
// saved away.
//-----------------------------------------------------------------------------
extern NTSTATUS MyAddDevice( IN PDRIVER_OBJECT DriverObject,
                             IN PDEVICE_OBJECT PhysicalDeviceObject )
{
   NTSTATUS   status;

   PDO = PhysicalDeviceObject;
   status = (*NdisAddDevice)(DriverObject, PhysicalDeviceObject);

   return status;
}

static UCHAR DeviceStarted = 0;

extern NTSTATUS USB_Dispatch( IN PDEVICE_OBJECT   DeviceObject,
                              IN PIRP             Irp )
{
    PIO_STACK_LOCATION   irpStack, nextStack;

    irpStack = IoGetCurrentIrpStackLocation (Irp);

    // Get a pointer to the device extension
    #ifdef LM_DEBUG_PC
      DbgMessage(VERBOSE, (ADMTEXT("*** USB_Dispatch ***\n")));
      DbgMessage(VERBOSE, (ADMTEXT("MajorFunction = %04x, MinorFunction = %04x\n"), irpStack->MajorFunction, irpStack->MinorFunction));
    #endif

    if(irpStack->MajorFunction == IRP_MJ_PNP_POWER)
    {
        switch (irpStack->MinorFunction)
        {
            case IRP_MN_START_DEVICE:
                if ( DeviceStarted == 0 )
                {
                    (*NdisPnpPower)(DeviceObject, Irp);
                    DeviceStarted = 1;
                    return STATUS_SUCCESS;
                }
                else
                {
                    nextStack = IoGetNextIrpStackLocation(Irp);
                    RtlCopyMemory(nextStack, irpStack, sizeof(IO_STACK_LOCATION));
                    IoCallDriver(PDO,Irp);
                    return STATUS_SUCCESS;
                }

            //case IRP_MN_QUERY_STOP_DEVICE:
            case IRP_MN_QUERY_CAPABILITIES:
            case IRP_MN_QUERY_ID:
            case IRP_MN_QUERY_PNP_DEVICE_STATE:
                nextStack = IoGetNextIrpStackLocation(Irp);
                RtlCopyMemory(nextStack, irpStack, sizeof(IO_STACK_LOCATION));
                IoCallDriver(PDO,Irp);
                break;

            default:
                (*NdisPnpPower)(DeviceObject, Irp);
                break;
        } //end switch
    } // end if PNP_POWER
}
#endif //NDISCE_MINIPORT

//***********************************************************
// LM_AbortPipe
//    Theis routine abort IN and OUT pipe.
//    1. Abort Out pipe
//    2. Reset Out pipe
//    3. Abort In pipe
//    4. Reset In pipe
//***********************************************************
LM_STATUS LM_AbortPipe( PLM_DEVICE_INFO_BLOCK   pDevice )
{
    PLM_DEVICE_EXTENSION_INFO   pExtension;
#ifndef NDISCE_MINIPORT
    PURB                        pUrb;
    LM_STATUS                   Status;
#endif

    #ifdef LM_DEBUG_PC
      DbgMessage(VERBOSE, (ADMTEXT("*** LM_AbortPipe ***\n")));
    #endif

    #ifdef LM_DEBUG_CE
    printf("*** LM_AbortPipe Start ***\n");
    #endif
    pExtension = (PLM_DEVICE_EXTENSION_INFO) pDevice->pLmDeviceExtension;




#ifndef NDISCE_MINIPORT
    DbgMessage(VERBOSE, (ADMTEXT("PIPE IN handle =%08lx, PIPE OUT handle =%08lx\n"), pExtension->pUsbdInterface->Pipes[PIPE_TYPE_IN].PipeHandle, pExtension->pUsbdInterface->Pipes[PIPE_TYPE_OUT].PipeHandle));
    pUrb = ExAllocatePool(NonPagedPool, sizeof( struct _URB_PIPE_REQUEST));
    pUrb->UrbHeader.Length = (USHORT)sizeof(struct _URB_PIPE_REQUEST);
    pUrb->UrbHeader.Function = URB_FUNCTION_ABORT_PIPE;
    pUrb->UrbPipeRequest.PipeHandle = pExtension->pUsbdInterface->Pipes[PIPE_TYPE_OUT].PipeHandle ;
    Status = LM_CallUsbd(pDevice, pUrb);
    if(Status != LM_STATUS_SUCCESS)
    {
        ExFreePool(pUrb);
        return LM_STATUS_FAILURE;
    } /* if */

    MM_InitWait(10000);
    //MM_InitWait(3000); //jackl modify 05/31/2001
    pUrb->UrbHeader.Length = (USHORT)sizeof(struct _URB_PIPE_REQUEST);
    pUrb->UrbHeader.Function = URB_FUNCTION_RESET_PIPE;
    pUrb->UrbPipeRequest.PipeHandle = pExtension->pUsbdInterface->Pipes[PIPE_TYPE_OUT].PipeHandle ;
    Status = LM_CallUsbd(pDevice, pUrb);
    if(Status != LM_STATUS_SUCCESS)
    {
        ExFreePool(pUrb);
        return LM_STATUS_FAILURE;
    } /* if */

    pUrb->UrbHeader.Length = (USHORT)sizeof(struct _URB_PIPE_REQUEST);
    pUrb->UrbHeader.Function = URB_FUNCTION_ABORT_PIPE;
    pUrb->UrbPipeRequest.PipeHandle = pExtension->pUsbdInterface->Pipes[PIPE_TYPE_IN].PipeHandle ;
    Status = LM_CallUsbd(pDevice, pUrb);
    if(Status != LM_STATUS_SUCCESS)
    {
        ExFreePool(pUrb);
        return LM_STATUS_FAILURE;
    } /* if */

    MM_InitWait(10000);
    //MM_InitWait(3000); //jackl modify 05/31/2001
    pUrb->UrbHeader.Length = (USHORT)sizeof(struct _URB_PIPE_REQUEST);
    pUrb->UrbHeader.Function = URB_FUNCTION_RESET_PIPE;
    pUrb->UrbPipeRequest.PipeHandle = pExtension->pUsbdInterface->Pipes[PIPE_TYPE_IN].PipeHandle ;

    Status = LM_CallUsbd(pDevice, pUrb);
    if(Status != LM_STATUS_SUCCESS)
    {
        ExFreePool(pUrb);
        return LM_STATUS_FAILURE;
    } /* if */

    ExFreePool(pUrb);

#else
    //USBM_AbortPipeTransfers( pExtension->hPipeBulkIN );
    //USBM_AbortPipeTransfers( pExtension->hPipeBulkOUT );
    //USBM_AbortPipeTransfers( pExtension->hPipeInterrupt );

	if( !pDevice ){
		//In Case, initialization hasn't completed successfully.
	    return  LM_STATUS_SUCCESS;
	}

    USBM_ClosePipe( pExtension->hPipeBulkIN );
    USBM_ClosePipe( pExtension->hPipeBulkOUT );
    //USBM_ClosePipe( pExtension->hPipeInterrupt );
#endif

    return  LM_STATUS_SUCCESS;
} // LM_AbortPipe

//*****************************************************************************
/* Description:                                                               */
/*    Completion Routine of LM_SetRegisterAsyn                                */
//*****************************************************************************
#ifndef NDISCE_MINIPORT
LM_SetRegisterComplete( PDEVICE_OBJECT   DeviceObject,
                        PIRP             pIrp,
                        PVOID            Context )
{
    PLM_DEVICE_INFO_BLOCK       pDevice;
    PLM_DEVICE_EXTENSION_INFO   pExtension;

    DbgMessage(FATAL, (ADMTEXT("*** LM_SetRegisterComplete ***\n")));
    pDevice = (PLM_DEVICE_INFO_BLOCK) Context;
    pExtension = (PLM_DEVICE_EXTENSION_INFO) pDevice->pLmDeviceExtension;
    DbgMessage(FATAL, (ADMTEXT("WorkState = %x\n"), pDevice->WorkState));
    ExFreePool(pDevice->pUrb);
    IoFreeIrp(pIrp);

    switch (pDevice->WorkState)
    {
        case 0x12:
            pDevice->WorkState = 0x13;
            LM_GetRegisterAsyn(pDevice, 2, 0x26);
            break;
        case 0x20:     //LM_SetupEthernet
            pDevice->SetState =0;
            break;
        case 0x100:    //Isolate HPNA & connect ETHERNET
            pDevice->WorkState = 0x101;
            pExtension->OutRegisterData2[0]=ETHERNET;
            pExtension->OutRegisterData2[1]=0x0;
            pExtension->OutRegisterData2[2]=0x30;
            pExtension->OutRegisterData2[3]=0x20;
            LM_SetRegisterAsyn (pDevice, 4, 0x25, pExtension->OutRegisterData2);
            break;
        case 0x101:
            pDevice->WorkState = 0x102;
            pExtension->OutRegisterData2[0]=HPNA;
            pExtension->OutRegisterData2[1]=0x0;
            pExtension->OutRegisterData2[2]=0x00;
            pExtension->OutRegisterData2[3]=0x04;
            LM_SetRegisterAsyn (pDevice, 4, 0x25, pExtension->OutRegisterData2);
            break;
        case 0x102:             // the above belong to change for hang
            pDevice->SetState = 0;
            break;
        case 0x200:            //Isolate ETHERNET & HPNA
            pDevice->WorkState = 0x201;
            pExtension->OutRegisterData2[0]=ETHERNET;
            pExtension->OutRegisterData2[1]=0x0;
            pExtension->OutRegisterData2[2]=0x34;
            pExtension->OutRegisterData2[3]=0x20;
            LM_SetRegisterAsyn (pDevice, 4, 0x25, pExtension->OutRegisterData2);
            break;
        case 0x201:
            pDevice->WorkState = 0x202;
            pExtension->OutRegisterData2[0]=HPNA;
            pExtension->OutRegisterData2[1]=0x0;
            pExtension->OutRegisterData2[2]=0x04;
            pExtension->OutRegisterData2[3]=0x20;
            LM_SetRegisterAsyn (pDevice, 4, 0x25, pExtension->OutRegisterData2);
            break;
        case 0x202:             // the above belong to change for hang
            pDevice->SetState = 0;
            break;
        case 0x300:          //Isolate ETHERNET & connect HPNA
            pDevice->WorkState = 0x301;
            // Write PHY 1 Register 0 to Isolate PHY from MII Asynchronously
            pExtension->OutRegisterData2[0]=ETHERNET;
            pExtension->OutRegisterData2[1]=0x0;
            pExtension->OutRegisterData2[2]=0x34;
            pExtension->OutRegisterData2[3]=0x20;
            LM_SetRegisterAsyn (pDevice, 4, 0x25, pExtension->OutRegisterData2);
            break;
        case 0x301:
            pDevice->WorkState = 0x302;
            // Write PHY 2 Register 0 to Reset HomePHY Asynchronously
            pExtension->OutRegisterData2[0]=HPNA;
            pExtension->OutRegisterData2[1]=0x00;
            pExtension->OutRegisterData2[2]=0xA0;
            pExtension->OutRegisterData2[3]=0x20;
            LM_SetRegisterAsyn (pDevice, 4, 0x25, pExtension->OutRegisterData2);
            break;
        case 0x302:             // the above belong to change for hang
            // Completed from pDevice->WorkState = 0x300 to 0x302
            pDevice->SetState = 0;
            break;
        case 0x400:         //Isolate ETHERNET & HPNA
            pDevice->WorkState = 0x401;
            pExtension->OutRegisterData2[0]=ETHERNET;
            pExtension->OutRegisterData2[1]=0x00;
            pExtension->OutRegisterData2[2]=0x34;
            pExtension->OutRegisterData2[3]=0x20;
            LM_SetRegisterAsyn (pDevice, 4, 0x25, pExtension->OutRegisterData2);
            break;
        case 0x401:
            pDevice->WorkState = 0x402;
            pExtension->OutRegisterData2[0]=HPNA;
            pExtension->OutRegisterData2[1]=0x00;
            pExtension->OutRegisterData2[2]=0x04;
            pExtension->OutRegisterData2[3]=0x20;
            LM_SetRegisterAsyn (pDevice, 4, 0x25, pExtension->OutRegisterData2);
            break;
        case 0x402:             // the above belong to change for hang
            pDevice->SetState = 0;
            break;
    }
    return STATUS_MORE_PROCESSING_REQUIRED;
}
#endif

/*
//***********************************************************
// LM_SetupEthernet
//
//***********************************************************
LM_SetupEthernet( IN PLM_DEVICE_INFO_BLOCK   pDevice,
                     WORD                    Value16 )
{
        PLM_DEVICE_EXTENSION_INFO pExtension;

        pExtension = (PLM_DEVICE_EXTENSION_INFO) pDevice->pLmDeviceExtension;

        DbgMessage(FATAL, (ADMTEXT("Re Setup Ethernet parameter\n")));
        DbgMessage(FATAL, (ADMTEXT("Old=[%X] New=[%X] \n"),pDevice->PartnerMedia,Value16));
        pDevice->PartnerMedia = Value16;
  //    DbgBreak();
        pDevice->Ctrl1Data &= ~(_100Mode | Full_Duplex | Home_Lan);
        if ( Value16 & 0x100)
        {  //detect 100 FULL Duplex
          DbgMessage(FATAL, (ADMTEXT("==>Dtecet 100 FULL mode\n")));
          pDevice->LinkSpeed = LINK_SPEED_100MBPS;
          pDevice->LinkDuplex = LINK_DUPLEX_FULL;
          pDevice->Ctrl1Data |= _100Mode | Full_Duplex;
        } else
        {
          if ( Value16 & 0x80)
          { //detect 100 HALF duplex
           DbgMessage(FATAL, (ADMTEXT("==>Dtecet 100 HALF mode\n")));
           pDevice->LinkSpeed = LINK_SPEED_100MBPS;
           pDevice->LinkDuplex = LINK_DUPLEX_HALF;
           pDevice->Ctrl1Data |= _100Mode;
          } else
          {
            if ( Value16 & 0x40)
            { //detect 10 FULL duplex
             DbgMessage(FATAL, (ADMTEXT("==>Dtecet 10 FULL mode\n")));
             pDevice->LinkSpeed = LINK_SPEED_10MBPS;
             pDevice->LinkDuplex = LINK_DUPLEX_FULL;
             pDevice->Ctrl1Data |=  Full_Duplex;
            } else
            { //detect 10 HALF dulpex
             DbgMessage(FATAL, (ADMTEXT("==>Dtecet 10 HALF mode\n")));
             pDevice->LinkSpeed = LINK_SPEED_10MBPS;
             pDevice->LinkDuplex = LINK_DUPLEX_HALF;
            }
          }
        }
   //   DbgBreak();
        pDevice->WorkState = 0x20;
        LM_SetRegisterAsyn(pDevice, 2, 0x1, pExtension->OutRegisterData2);
}
*/

//***********************************************************
// LM_GetRegisterComplete
//***********************************************************
#ifndef NDISCE_MINIPORT
LM_GetRegisterComplete( PDEVICE_OBJECT   DeviceObject,
                        PIRP             pIrp,
                        PVOID            Context )
{
    PLM_DEVICE_INFO_BLOCK       pDevice;
    PLM_DEVICE_EXTENSION_INFO   pExtension;
    WORD                        Value16;

    DbgMessage(FATAL, (ADMTEXT("*** LM_GetRegisterComplete ***\n")));
    pDevice = (PLM_DEVICE_INFO_BLOCK) Context;
    pExtension = (PLM_DEVICE_EXTENSION_INFO) pDevice->pLmDeviceExtension;
    ExFreePool(pDevice->pUrbGet);
    IoFreeIrp(pIrp);

    Value16 = pExtension->InRegisterData[1];
    Value16 <<= 8;
    Value16 |= pExtension->InRegisterData[0];
    DbgMessage(FATAL, (ADMTEXT("CurrMode = %2x\n"), pDevice->CurrMode));
    DbgMessage(FATAL, (ADMTEXT("Value16 = %4x\n"), Value16));

    switch (pDevice->WorkState)
    {
        case    0x13:           // CheckForHang
            if ( pDevice->CurrMode == ETHERNET )
            {
                if ( Value16 & 0x1E0 )
                {
                    //link on
                    if (pDevice->LinkStatus == LM_STATUS_LINK_DOWN)
                    {
                        if ( pDevice->RequestedSelectMedia == SELECT_MEDIA_AUTO_DETECTION )
                        {
                            pDevice->WorkState = 0x100;
                            pExtension->OutRegisterData2[0]=NONE_GPIO;
                            pExtension->OutRegisterData2[1]=pDevice->Testpin;
                            LM_SetRegisterAsyn (pDevice, 2, 0x7F, pExtension->OutRegisterData2);
                            pDevice->CurrMode = ETHERNET;
                        }
                        else
                        {
                            pDevice->SetState = 0;
                        }
					    // Set link status
#ifdef SUPPORT_MEDIASENSE // add - 07/20/05,bolo for MSC
						if (LM_GetLinkStatus(pDevice) & LINK_STS)
#endif
					        pDevice->LinkStatus = LM_STATUS_LINK_ACTIVE;

#ifdef SUPPORT_MEDIASENSE // add - 07/20/05,bolo for MSC
						else
					        pDevice->LinkStatus = LM_STATUS_LINK_DOWN;
#endif
                        MM_IndicateStatus(pDevice,pDevice->LinkStatus);
                    }
                    else
                    {
                        DbgMessage(FATAL, (ADMTEXT(" OLD =[%X] New=[%X]\n"),pDevice->PartnerMedia, Value16));
                        if ( Value16 == pDevice->PartnerMedia)
                        {
                            pDevice->SetState = 0;     //don't change
                        }
                        else
                        {
                            if (pDevice->RequestedMediaType == MEDIA_TYPE_AUTO_DETECTION)
                            {
                                LM_SetupEthernet(pDevice,Value16);
                            }
                            else
                            {
                                pDevice->SetState = 0;     //don't change
                            }
                        }
                    }
                }
                else
                {
                    if (pDevice->LinkStatus == LM_STATUS_LINK_ACTIVE)
                    {
                        if ( pDevice->RequestedSelectMedia == SELECT_MEDIA_AUTO_DETECTION )
                        {
                            pDevice->WorkState = 0x200;
                            pExtension->OutRegisterData2[0]=ETHER_GPIO;
                            pExtension->OutRegisterData2[1]=pDevice->Testpin;
                            LM_SetRegisterAsyn (pDevice, 2, 0x7F, pExtension->OutRegisterData2);
                            pDevice->CurrMode = ETHERNET;
                        }
                        else
                        {
                            pDevice->SetState = 0;
                        }
					    // Set link status
#ifdef SUPPORT_MEDIASENSE // add - 07/20/05,bolo for MSC
						if (LM_GetLinkStatus(pDevice) & LINK_STS)
					        pDevice->LinkStatus = LM_STATUS_LINK_ACTIVE;
						else
#endif
					        pDevice->LinkStatus = LM_STATUS_LINK_DOWN;

                        MM_IndicateStatus(pDevice,pDevice->LinkStatus);
                    }
                    else
                    {
                        if ( pDevice->RequestedSelectMedia == SELECT_MEDIA_AUTO_DETECTION )
                        {
                            pDevice->CurrMode = HPNA;
                        }
                        pDevice->SetState = 0;
                    }
                }
            }
            else
            {
                // current mode is HPNA
                if ( Value16 & 0x4 ) //link check
                {
                    //link on
                    if (pDevice->LinkStatus == LM_STATUS_LINK_DOWN)
                    {
                    if ( pDevice->RequestedSelectMedia == SELECT_MEDIA_AUTO_DETECTION )
                    {
                        pDevice->WorkState = 0x300;
                        pExtension->OutRegisterData2[0]=HPNA_GPIO;
                        pExtension->OutRegisterData2[1]=pDevice->Testpin;
                        LM_SetRegisterAsyn (pDevice, 2, 0x7F, pExtension->OutRegisterData2);
                        pDevice->CurrMode = HPNA;
                    }
                    else
                    {
                        pDevice->SetState = 0;
                    }
				    // Set link status
#ifdef SUPPORT_MEDIASENSE // add - 07/20/05,bolo for MSC
					if (LM_GetLinkStatus(pDevice) & LINK_STS)
#endif

				        pDevice->LinkStatus = LM_STATUS_LINK_ACTIVE;

#ifdef SUPPORT_MEDIASENSE // add - 07/20/05,bolo for MSC
					else
				        pDevice->LinkStatus = LM_STATUS_LINK_DOWN;
#endif
                    MM_IndicateStatus(pDevice,pDevice->LinkStatus);
                }
                else
                {
                    pDevice->SetState = 0;
                }
            }
            else
            {
                //link off
                if (pDevice->LinkStatus == LM_STATUS_LINK_ACTIVE)
                {
                    if ( pDevice->RequestedSelectMedia == SELECT_MEDIA_AUTO_DETECTION )
                    {
                        pDevice->WorkState = 0x400;
                        pExtension->OutRegisterData2[0]=HPNA_GPIO;
                        pExtension->OutRegisterData2[1]=pDevice->Testpin;
                        LM_SetRegisterAsyn (pDevice, 2, 0x7F, pExtension->OutRegisterData2);
                        pDevice->CurrMode = ETHERNET;
                    }
                    else
                    {
                        pDevice->SetState = 0;
                    }
				    // Set link status
					if (LM_GetLinkStatus(pDevice) & LINK_STS)
				        pDevice->LinkStatus = LM_STATUS_LINK_ACTIVE;
					else
				        pDevice->LinkStatus = LM_STATUS_LINK_DOWN;
                    MM_IndicateStatus(pDevice,pDevice->LinkStatus);
                }
                else
                {
                    if ( pDevice->RequestedSelectMedia == SELECT_MEDIA_AUTO_DETECTION )
                    {
                        pDevice->CurrMode = ETHERNET;
                        pDevice->SetState = 0;
                        LM_CheckMedia(pDevice);
                    }
                    else
                    {
                        pDevice->SetState = 0;
                    }
                }
            }   //end link check
        }
        break;
    }
    return STATUS_MORE_PROCESSING_REQUIRED;
}
#endif

//*****************************************************************************
/* Description:                                                               */
/*    Set Register Asynchronously with completion routine                     */
//*****************************************************************************
LM_STATUS LM_SetRegisterAsyn( IN PLM_DEVICE_INFO_BLOCK   pDevice,
                              IN USHORT                  Count,
                              IN USHORT                  Index,
                              IN UCHAR *                 pOutRegisterData )
{
    PLM_DEVICE_EXTENSION_INFO   pExtension;
#ifndef NDISCE_MINIPORT
    PURB                        pUrb;
    PIO_STACK_LOCATION          pNextStack;
    PIRP                        pIrp;
    KIRQL                       irql;
    IO_STATUS_BLOCK             IoStatusBlock;
#endif
    NTSTATUS                    NtStatus = 0;
    USHORT                      value=0;

    DbgMessage(FATAL, (ADMTEXT("*** LM_SetRegisterAsyn ***\n")));
    pExtension = (PLM_DEVICE_EXTENSION_INFO) pDevice->pLmDeviceExtension;

    DbgMessage(INFORM, (ADMTEXT("LM_SetRegisterAsyn : Count = %2x; Index = %2x; Set Data => [0]=%2x, [1]=%2x, [2]=%2x, [3]=%2x\n"),
                        Count, Index, pOutRegisterData[0], pOutRegisterData[1], pOutRegisterData[2], pOutRegisterData[3]));

    //NDISDprAcquireSpinLock(&pExtension->SetRegQSpinLock);
#ifndef NDISCE_MINIPORT
    pDevice->pUrb = pUrb = ExAllocatePool(NonPagedPool, sizeof(struct _URB_CONTROL_VENDOR_OR_CLASS_REQUEST));
    UsbBuildVendorRequest( (PURB)pDevice->pUrb,
                           URB_FUNCTION_VENDOR_DEVICE,
                           sizeof(struct _URB_CONTROL_VENDOR_OR_CLASS_REQUEST),
                           0,
                           0,
                           0xF1,
                           value,
                           Index,
                           pOutRegisterData,
                           NULL,
                           Count,
                           NULL );
    pIrp = IoAllocateIrp(pExtension->NextDeviceStackSize, FALSE);
    if(pIrp == NULL)
    {
        DbgMessage(FATAL, (ADMTEXT("IoAllocateIrp fail\n")));
        return LM_STATUS_FAILURE;
    }

    pNextStack = IoGetNextIrpStackLocation(pIrp);
    pNextStack->MajorFunction = IRP_MJ_INTERNAL_DEVICE_CONTROL;
    pNextStack->Parameters.Others.Argument1 = (PURB)pDevice->pUrb;
    pNextStack->Parameters.DeviceIoControl.IoControlCode = IOCTL_INTERNAL_USB_SUBMIT_URB;
/*
    irql = KeGetCurrentIrql();
    if (irql > PASSIVE_LEVEL)
    {
        DbgPrint("\nCall KeLowerIrql!\n");
        KeLowerIrql(PASSIVE_LEVEL);
    }
*/
    /* Set up a completion routine. */
    IoSetCompletionRoutine( pIrp, LM_SetRegisterComplete, pDevice, TRUE, TRUE, TRUE );

    /* Pass the IRP to the next device. */
    NtStatus = IoCallDriver(pExtension->NextDeviceObject, pIrp);
#endif

    return NtStatus;
}

//***********************************************************
// LM_SetRegisterAsyn
//***********************************************************
LM_STATUS LM_GetRegisterAsyn( IN PLM_DEVICE_INFO_BLOCK   pDevice,
                              IN USHORT                  Count,
                              IN USHORT                  Index )
{
    PLM_DEVICE_EXTENSION_INFO   pExtension;
#ifndef NDISCE_MINIPORT
    PURB                        Urb;
    PIO_STACK_LOCATION          pNextStack;
    PIRP                        pIrp;
    KIRQL                       irql;
#endif
    NTSTATUS                    NtStatus = 0;
    USHORT                      value=0;

    DbgMessage(INFORM, (ADMTEXT("LM_GetRegisterAsyn Count = %2X, Index = %2X\n"), Count, Index));

    pExtension = (PLM_DEVICE_EXTENSION_INFO) pDevice->pLmDeviceExtension;
#ifndef NDISCE_MINIPORT
    pDevice->pUrbGet = pUrb = ExAllocatePool(NonPagedPool, sizeof(struct _URB_CONTROL_VENDOR_OR_CLASS_REQUEST));
    UsbBuildVendorRequest( (PURB)pDevice->pUrbGet,
                           URB_FUNCTION_VENDOR_DEVICE,
                           sizeof(struct _URB_CONTROL_VENDOR_OR_CLASS_REQUEST),
                           (USBD_TRANSFER_DIRECTION_IN),
                           0,
                           0xF0,
                           0,
                           Index,
                           pExtension->InRegisterData,
                           NULL,
                           Count,
                           NULL);
    pIrp = IoAllocateIrp(pExtension->NextDeviceStackSize, FALSE);

    if(pIrp == NULL)
    {
        DbgMessage(FATAL, (ADMTEXT("Cannot allocate an IRP in LM_SetRegisterAsyn\n")));
        return LM_STATUS_FAILURE;
    }

    pNextStack = IoGetNextIrpStackLocation(pIrp);
    pNextStack->MajorFunction = IRP_MJ_INTERNAL_DEVICE_CONTROL;
    pNextStack->Parameters.Others.Argument1 = (PURB)pDevice->pUrbGet;
    pNextStack->Parameters.DeviceIoControl.IoControlCode = IOCTL_INTERNAL_USB_SUBMIT_URB;

    /* Set up a completion routine. */
    IoSetCompletionRoutine(pIrp,LM_GetRegisterComplete,pDevice,TRUE,TRUE,TRUE);
    NtStatus = IoCallDriver(pExtension->NextDeviceObject, pIrp);
#endif

    return(NtStatus);
}

//******************************************************************************
// Description:
//              This routine wait for 3 == 0
//
// Entry:
//              pDevice
//              waittime: how many msec
//******************************************************************************
LM_STATUS Wait_SetState( IN PLM_DEVICE_INFO_BLOCK   pDevice,
                         IN WORD                    waittime )
{
    WORD    i;
    for (i=0; i<waittime; i++)
    {
        if (pDevice->SetState == 0)
            break;
        MM_InitWait(1000);
    }
    if (i == waittime)
        return(LM_STATUS_FAILURE);
    else
        return(LM_STATUS_SUCCESS);
}

//******************************************************************************
// Description:
//    This routine is changed Multicast Address Table
//
// Parameters:
//    IN LM_DEVICE_INFO_BLOCK pDevice
//    IN WORD MC No.
//
// Returns:
//    LM_STATUS
//******************************************************************************
LM_STATUS LM_ChangeMcAddress( IN PLM_DEVICE_INFO_BLOCK   pDevice,
                              IN WORD                    AddressCount)
{
   int     i,j;
   DWORD   Crc= 0xffffffff;
   DWORD   FlippedCRC=0,CSR27=0,CSR28=0,R_Flag=1/*,TempData*/;
   DWORD   Msb;
   WORD    BytesLength=0x7E;
   BYTE    CurrentByte;
   WORD    Bit,MyCrc=0;
   WORD    Temp=0;
   PLM_DEVICE_EXTENSION_INFO pExtension;


   DbgMessage(VERBOSE, (ADMTEXT("*** UM_ChangeMCAddress ***\n")));
        pExtension = (PLM_DEVICE_EXTENSION_INFO) pDevice->pLmDeviceExtension;
//   MM_WritePort32(pDevice->pAdapter, COMET_MULTICAST_ADDR1,CSR27);
//   MM_WritePort32(pDevice->pAdapter, COMET_MULTICAST_ADDR2,CSR28);

   for (j=0;(j<AddressCount) && (j<MAX_MULTICAST_ADDRESS);j++)
  {
   Crc = 0xFFFFFFFF;
   for ( i=0;i<ETH_LENGTH_OF_ADDRESS;i++)
   {
    CurrentByte = pDevice->PrivateMulticastBuffer[j][i];
        DbgMessage(VERBOSE, (ADMTEXT("%2X\n"),pDevice->PrivateMulticastBuffer[j][i]));
    for(Bit=0;Bit<8;Bit++){
       Msb = Crc >>31;
       Crc <<=1;
     if(Msb^( CurrentByte&1)){
        Crc^=POLY;
        Crc|=0x00000001;
       }   /* end if */
        CurrentByte >>=1;
      }  /* end for Bit */
    }  /* end i */
        DbgMessage(VERBOSE, (ADMTEXT("******\n")));
     for (i=0;i<32;i++)
     {
       FlippedCRC <<=1;
       Bit = (WORD)Crc &1;
       Crc >>=1;
       FlippedCRC +=Bit;
     }
   Crc = FlippedCRC ^ 0xFFFFFFFF;
   Crc &= 0x3F;
   //DbgMessage(FATAL, (ADMTEXT("mc [%d]=%.2X %.2X %.2X %.2X %.2X %.2X\n"),j,
   //pDevice->PrivateMulticastBuffer[j][0],
   //pDevice->PrivateMulticastBuffer[j][1],
   //pDevice->PrivateMulticastBuffer[j][2],
   //pDevice->PrivateMulticastBuffer[j][3],
   //pDevice->PrivateMulticastBuffer[j][4],
   //pDevice->PrivateMulticastBuffer[j][5]));
   //DbgMessage(FATAL, (ADMTEXT(" CRC = %X \n"),Crc));
   R_Flag = 1;
   if (Crc < 32 )
   {
    R_Flag = R_Flag << Crc ;
    CSR27 |= R_Flag ;
   } else
   {
    Crc -= 32;
    R_Flag = R_Flag << Crc ;
    CSR28 |= R_Flag ;
   }
  } /* end j */

   //DbgMessage(FATAL, (ADMTEXT(" ADDR1 = %X \n"),CSR27));
   //DbgMessage(FATAL, (ADMTEXT(" ADDR2 = %X \n"),CSR28));
//   MM_WritePort32(pDevice->pAdapter, COMET_MULTICAST_ADDR1,CSR27);
//   MM_WritePort32(pDevice->pAdapter, COMET_MULTICAST_ADDR2,CSR28);
//   MM_WritePort32(pDevice->pAdapter, COMET_NETWORK_ACCESS, TempData);

        pExtension->OutRegisterData1[0]=(BYTE) CSR27;
        pExtension->OutRegisterData1[1]=(BYTE) CSR27>>8;
        pExtension->OutRegisterData1[2]=(BYTE) CSR27>>16;
        pExtension->OutRegisterData1[3]=(BYTE) CSR27>>24;

        pExtension->OutRegisterData1[4]=(BYTE) CSR28;
        pExtension->OutRegisterData1[5]=(BYTE) CSR28>>8;
        pExtension->OutRegisterData1[6]=(BYTE) CSR28>>16;
        pExtension->OutRegisterData1[7]=(BYTE) CSR28>>24;
        if (Wait_SetState(pDevice, 1000) == LM_STATUS_SUCCESS)
        {
                pDevice->WorkState = 0x50;
                pDevice->SetState = 1;
                DbgMessage(VERBOSE, (ADMTEXT("UM_ChangeMCAddress return\n")));
                LM_SetRegisterAsyn (pDevice, 8, 0x8, pExtension->OutRegisterData1);
                return(Wait_SetState(pDevice, 1000));
        }else{
                return( LM_STATUS_FAILURE);
        }
} // UM_ChangeMcAddress

// Disable Tx/Rx & reset PHY Asynchronously
LM_STATUS LM_ResetMACAsyn( IN PLM_DEVICE_INFO_BLOCK pDevice )
{
    PLM_DEVICE_EXTENSION_INFO   pExtension;

    pExtension = (PLM_DEVICE_EXTENSION_INFO) pDevice->pLmDeviceExtension;
    DbgMessage(VERBOSE, (ADMTEXT("*** LM_ResetMACAsyn ***\n")));
    if (Wait_SetState(pDevice, 2000) == LM_STATUS_SUCCESS)
    {
        // 1. Set Ethernet control_0 & Ethernet control_1 to Disable Tx/Rx Asynchronously (WorkState = 0x300)
        // 2. Write PHY 1 Register 0 to Isolate PHY from MII Asynchronously (WorkState = 0x301)
        // 3. Write PHY 2 Register 0 to Reset HomePHY Asynchronously (WorkState = 0x302)
        pDevice->WorkState = 0x300;
        pDevice->SetState = 1;
        pExtension->OutRegisterData1[0] = ( pDevice->Ctrl0Data & 0x3F );   // Disable tx_en & rx_en
        pExtension->OutRegisterData1[1] = pDevice->Ctrl1Data;
        LM_SetRegisterAsyn (pDevice, 2, 0, pExtension->OutRegisterData1);

        // Reset MAC Asynchronously
        //pExtension->OutRegisterData1[0] = pDevice->Ctrl1Data | 0x08;
        //pExtension->OutRegisterData1[1] = 0;
        //LM_SetRegisterAsyn (pDevice, 2, 0x1, pExtension->OutRegisterData1);

        // Wait WorkState 0x302 to completed
        Wait_SetState(pDevice, 300);
        return(LM_STATUS_SUCCESS);
    }
    else
        return(LM_STATUS_FAILURE);
}

//******************************************************************************
// Description:
//    This routine is Clear WakeUp Status
//
// Parameters:
//    IN LM_DEVICE_INFO_BLOCK pDevice
//
// Returns:
//    void
//******************************************************************************

VOID LM_ClearWakeUpStatus( IN PLM_DEVICE_INFO_BLOCK pDevice )
{
#ifndef NDISCE_MINIPORT
    KIRQL                       oldirq;
#endif
    PLM_DEVICE_EXTENSION_INFO   pExtension;

    DbgMessage(INFORM, (ADMTEXT("*** LM_ClearWakeUpStatus ***\n")));
    pExtension = (PLM_DEVICE_EXTENSION_INFO) pDevice->pLmDeviceExtension;
#ifndef NDISCE_MINIPORT
    oldirq = KeGetCurrentIrql();
    KeLowerIrql(PASSIVE_LEVEL);
#endif
    LM_GetRegister (pDevice, 2, 0x7A );
    DbgMessage(INFORM, (ADMTEXT("Wake Up Status = %02X\n"), pExtension->InRegisterData[0]));
#ifndef NDISCE_MINIPORT
    KeRaiseIrql(oldirq,&oldirq);
#endif
}	//LM_ClearWakeUpStatus
/*
Void LM_ReleaseResources( PLM_DEVICE_INFO_BLOCK   pDevice )
{

}
*/

// 2004/08/24, ccwong, add MediaSense support
//******************************************************************************
// Description:
//    This routine Query Wakeup register status
//
// Parameters:
//    IN LM_DEVICE_INFO_BLOCK pDevice
//
// Returns:
//    Value of Wakeup register
//******************************************************************************
USHORT LM_GetLinkStatus(PLM_DEVICE_INFO_BLOCK pDevice)
{
#ifndef NDISCE_MINIPORT
    KIRQL                       oldirq;
#endif
    PLM_DEVICE_EXTENSION_INFO   pExtension;
    LM_STATUS                   Status;     // ++ add - 08/25/05,bolo
    WORD                        Value16;    // --

    pExtension = (PLM_DEVICE_EXTENSION_INFO) pDevice->pLmDeviceExtension;
#ifndef NDISCE_MINIPORT
    oldirq = KeGetCurrentIrql();
    KeLowerIrql(PASSIVE_LEVEL);
#endif
    // ++ add - 08/25/05,bolo
    if (pDevice->CurrMode == MACMII) {

        Status = LM_ReadMii(pDevice, pDevice->PhyAddr, 1, &Value16); // add - 08/25/05,bolo
	    if( Status != LM_STATUS_SUCCESS )
        { 
            return 0;
        }

        Value16 = (Value16 & 0x04) >> 2;
        return Value16;

    } else { // --

        LM_GetRegister (pDevice, 2, 0x7a );   // Index 0x7a : wake up status registers
    }
#ifndef NDISCE_MINIPORT
    KeRaiseIrql(oldirq,&oldirq);
#endif
    return pExtension->InRegisterData[0];
}
