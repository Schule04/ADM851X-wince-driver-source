/******************************************************************************/
/* Module Description   : USBM.C                                              */
/* Function Description : Interface module to USBDI                           */
/******************************************************************************/
#include "usbm.h"

extern LPCUSB_FUNCS   pUsbFunctions;

//#define USBM_DEBUG_PC 1
/*
 * Get Device Infomation
 */
USBM_STATUS USBM_GetDeviceInfo( IN  USB_HANDLE        hDevice,
                                OUT LPCUSB_DEVICE *   plpDeviceInfo )
{
    LPUSB_DEVICE           pUsbDevice;
    LPCUSB_CONFIGURATION   pUsbConfiguration;
    LPCUSB_INTERFACE       pUsbInterface;
    LPCUSB_ENDPOINT        pUsbEndpoint;
    WORD                   i, j, k;
    
    #ifdef USBM_DEBUG_PC
      DbgMessage(INFORM, (ADMTEXT("*** USBM_GetDeviceInfo ***\n")));
    #endif
    if (!pUsbFunctions)
    {
    return LM_STATUS_FAILURE;
    }
    *plpDeviceInfo = (*pUsbFunctions->lpGetDeviceInfo)( hDevice );
    if( !(*plpDeviceInfo) )
    {
        #ifdef USBM_DEBUG_PC
          DbgMessage(INFORM, (ADMTEXT("USBM_GetDeviceInfo fail\n")));
        #endif
        return LM_STATUS_FAILURE;
    }

    pUsbDevice = (LPUSB_DEVICE)*plpDeviceInfo;
    #ifdef USBM_DEBUG_PC
      DbgMessage(INFORM, (ADMTEXT("--------- Device Descriptor ---------\n")));
      DbgMessage(INFORM, (ADMTEXT("bLength             =   %02x h\n"), pUsbDevice->Descriptor.bLength));
      DbgMessage(INFORM, (ADMTEXT("bDescriptorType     =   %02x h\n"), pUsbDevice->Descriptor.bDescriptorType));
      DbgMessage(INFORM, (ADMTEXT("bcdUSB              = %04x h\n"), pUsbDevice->Descriptor.bcdUSB));
      DbgMessage(INFORM, (ADMTEXT("bDeviceClass        =   %02x h\n"), pUsbDevice->Descriptor.bDeviceClass));
      DbgMessage(INFORM, (ADMTEXT("bDeviceSubClass     =   %02x h\n"), pUsbDevice->Descriptor.bDeviceSubClass));
      DbgMessage(INFORM, (ADMTEXT("bDeviceProtocol     =   %02x h\n"), pUsbDevice->Descriptor.bDeviceProtocol));
      DbgMessage(INFORM, (ADMTEXT("bMaxPacketSize0     =   %02x h\n"), pUsbDevice->Descriptor.bMaxPacketSize0));
      DbgMessage(INFORM, (ADMTEXT("idVendor            = %04x h\n"), pUsbDevice->Descriptor.idVendor));
      DbgMessage(INFORM, (ADMTEXT("idProduct           = %04x h\n"), pUsbDevice->Descriptor.idProduct));
      DbgMessage(INFORM, (ADMTEXT("bcdDevice           = %04x h\n"), pUsbDevice->Descriptor.bcdDevice));
      DbgMessage(INFORM, (ADMTEXT("iManufacturer       =   %02x h\n"), pUsbDevice->Descriptor.iManufacturer));
      DbgMessage(INFORM, (ADMTEXT("iProduct            =   %02x h\n"), pUsbDevice->Descriptor.iProduct));
      DbgMessage(INFORM, (ADMTEXT("iSerialNumber       =   %02x h\n"), pUsbDevice->Descriptor.iSerialNumber));
      DbgMessage(INFORM, (ADMTEXT("bNumConfigurations  =   %02x h\n"), pUsbDevice->Descriptor.bNumConfigurations));
    #endif

    pUsbConfiguration = pUsbDevice->lpConfigs;
    for( i=0; i<pUsbDevice->Descriptor.bNumConfigurations; i++ )
    {
        #ifdef USBM_DEBUG_PC
          DbgMessage(INFORM, (ADMTEXT("------- Configuration %02xh Descriptor -------\n"), pUsbConfiguration[i].Descriptor.bConfigurationValue));
          DbgMessage(INFORM, (ADMTEXT("bLength             =   %02x h\n"), pUsbConfiguration[i].Descriptor.bLength));
          DbgMessage(INFORM, (ADMTEXT("bDescriptorType     =   %02x h\n"), pUsbConfiguration[i].Descriptor.bDescriptorType));
          DbgMessage(INFORM, (ADMTEXT("wTotalLength        = %04x h\n"), pUsbConfiguration[i].Descriptor.wTotalLength));
          DbgMessage(INFORM, (ADMTEXT("bNumInterfaces      =   %02x h\n"), pUsbConfiguration[i].Descriptor.bNumInterfaces));
          DbgMessage(INFORM, (ADMTEXT("bConfigurationValue =   %02x h\n"), pUsbConfiguration[i].Descriptor.bConfigurationValue));
          DbgMessage(INFORM, (ADMTEXT("iConfiguration      =   %02x h\n"), pUsbConfiguration[i].Descriptor.iConfiguration));
          DbgMessage(INFORM, (ADMTEXT("bmAttributes        =   %02x h\n"), pUsbConfiguration[i].Descriptor.bmAttributes));
          DbgMessage(INFORM, (ADMTEXT("MaxPower            =   %02x h\n"), pUsbConfiguration[i].Descriptor.MaxPower));
        #endif

        pUsbInterface = pUsbConfiguration[i].lpInterfaces;
        for( j=0; j<pUsbConfiguration->Descriptor.bNumInterfaces; j++ )
        {
            #ifdef USBM_DEBUG_PC
              DbgMessage(INFORM, (ADMTEXT("----- Interface %02xh Descriptor -----\n"), pUsbInterface[j].Descriptor.bInterfaceNumber));
              DbgMessage(INFORM, (ADMTEXT("bLength             =   %02x h\n"), pUsbInterface[j].Descriptor.bLength));
              DbgMessage(INFORM, (ADMTEXT("bDescriptorType     =   %02x h\n"), pUsbInterface[j].Descriptor.bDescriptorType));
              DbgMessage(INFORM, (ADMTEXT("bInterfaceNumber    =   %02x h\n"), pUsbInterface[j].Descriptor.bInterfaceNumber));
              DbgMessage(INFORM, (ADMTEXT("bAlternateSetting   =   %02x h\n"), pUsbInterface[j].Descriptor.bAlternateSetting));
              DbgMessage(INFORM, (ADMTEXT("bNumEndpoints       =   %02x h\n"), pUsbInterface[j].Descriptor.bNumEndpoints));
              DbgMessage(INFORM, (ADMTEXT("bInterfaceClass     =   %02x h\n"), pUsbInterface[j].Descriptor.bInterfaceClass));
              DbgMessage(INFORM, (ADMTEXT("bInterfaceSubClass  =   %02x h\n"), pUsbInterface[j].Descriptor.bInterfaceSubClass));
              DbgMessage(INFORM, (ADMTEXT("bInterfaceProtocol  =   %02x h\n"), pUsbInterface[j].Descriptor.bInterfaceProtocol));
              DbgMessage(INFORM, (ADMTEXT("iInterface          =   %02x h\n"), pUsbInterface[j].Descriptor.iInterface));
            #endif

            pUsbEndpoint = pUsbInterface[j].lpEndpoints;
            for( k=0; k<pUsbInterface->Descriptor.bNumEndpoints; k++ )
            {
                #ifdef USBM_DEBUG_PC
                  DbgMessage(INFORM, (ADMTEXT("--- Endpoint %02xh Descriptor ---\n"), pUsbEndpoint[k].Descriptor.bEndpointAddress));
                  DbgMessage(INFORM, (ADMTEXT("bLength             =   %02x h\n"), pUsbEndpoint[k].Descriptor.bLength));
                  DbgMessage(INFORM, (ADMTEXT("bDescriptorType     =   %02x h\n"), pUsbEndpoint[k].Descriptor.bDescriptorType));
                  DbgMessage(INFORM, (ADMTEXT("bEndpointAddress    =   %02x h\n"), pUsbEndpoint[k].Descriptor.bEndpointAddress));
                  DbgMessage(INFORM, (ADMTEXT("bmAttributes        =   %02x h\n"), pUsbEndpoint[k].Descriptor.bmAttributes));
                  DbgMessage(INFORM, (ADMTEXT("wMaxPacketSize      = %04x h\n"), pUsbEndpoint[k].Descriptor.wMaxPacketSize));
                  DbgMessage(INFORM, (ADMTEXT("bInterval           =   %02x h\n"), pUsbEndpoint[k].Descriptor.bInterval));
                #endif
            }
        }
    }
    return LM_STATUS_SUCCESS;
}

/*
 * Find specific Interface
 */
USBM_STATUS USBM_FindInterface( IN  LPCUSB_DEVICE        lpDeviceInfo,
                                IN  UCHAR                bInterfaceNumber,
                                IN  UCHAR                bAlternateSetting,
                                OUT LPCUSB_INTERFACE *   plpInterface )
{
    #ifdef USBM_DEBUG_PC
      DbgMessage(INFORM, (ADMTEXT("*** USBM_FindInterface, InterfaceNumber = %02xh, AlternateSetting = %02xh ***\n"), bInterfaceNumber, bAlternateSetting));
    #endif
    *plpInterface = NULL;
    *plpInterface = (*pUsbFunctions->lpFindInterface)( lpDeviceInfo, bInterfaceNumber, bAlternateSetting );
    if( !(*plpInterface) )
    {
        #ifdef USBM_DEBUG_PC
          DbgMessage(INFORM, (ADMTEXT("Can't find specified Interface\n")));
        #endif
        return LM_STATUS_FAILURE;
    }
    return LM_STATUS_SUCCESS;
}

/*
 * Get Descriptor synchronously
 */
USBM_STATUS USBM_GetDescriptor( IN      USB_HANDLE   hDevice,
                                IN      UCHAR        bType,
                                IN      WORD         wLength,
                                IN OUT  LPVOID       lpvBuffer )
{
    USB_TRANSFER   hTransfer;
    USBM_STATUS    status;

    #ifdef USBM_DEBUG_PC
      DbgMessage(INFORM, (ADMTEXT("*** USBM_GetDescriptor, bType = %02xh ***\n"), bType));
    #endif

    hTransfer = (*pUsbFunctions->lpGetDescriptor)( hDevice,NULL,NULL,0,bType,0,0,wLength,lpvBuffer );
    if( !hTransfer )
    {
        #ifdef USBM_DEBUG_PC
          DbgMessage(INFORM, (ADMTEXT("Issue Request fail\n")));
        #endif
        return LM_STATUS_FAILURE;
    }
    status = USBM_GetTransferStatus( hTransfer, NULL, NULL );
    (*pUsbFunctions->lpCloseTransfer)( hTransfer );
    return status;
}

/*
 * Clear Feature
 */
USBM_STATUS USBM_ClearFeature( IN  USB_HANDLE   hDevice,
                               IN  DWORD        dwFlags,
                               IN  WORD         wFeature,
                               IN  UCHAR        bIndex )
{
    USB_TRANSFER   hTransfer;
    USBM_STATUS    status;

    #ifdef USBM_DEBUG_PC
      DbgMessage(INFORM, (ADMTEXT("*** USBM_ClearFeature, dwFlags = %08xh, wFeature = %04xh, bIndex = %02xh ***\n"), dwFlags, wFeature, bIndex));
    #endif

    hTransfer = (*pUsbFunctions->lpClearFeature)( hDevice,NULL,NULL,dwFlags,wFeature,bIndex);
    if( !hTransfer )
    {
        #ifdef USBM_DEBUG_PC
          DbgMessage(INFORM, (ADMTEXT("Issue Request fail\n")));
        #endif
        return LM_STATUS_FAILURE;
    }
    status = USBM_GetTransferStatus( hTransfer, NULL, NULL );
    (*pUsbFunctions->lpCloseTransfer)( hTransfer );
    return status;
}

/*
 * Set Feature
 */
USBM_STATUS USBM_SetFeature( IN  USB_HANDLE   hDevice,
                             IN  DWORD        dwFlags,
                             IN  WORD         wFeature,
                             IN  UCHAR        bIndex )
{
    USB_TRANSFER   hTransfer;
    USBM_STATUS    status;

    #ifdef USBM_DEBUG_PC
      DbgMessage(INFORM, (ADMTEXT("*** USBM_SetFeature, dwFlags = %08xh, wFeature = %04xh, bIndex = %02xh ***\n"), dwFlags, wFeature, bIndex));
    #endif

    hTransfer = (*pUsbFunctions->lpSetFeature)( hDevice,NULL,NULL,dwFlags,wFeature,bIndex );
    if( !hTransfer )
    {
        #ifdef USBM_DEBUG_PC
          DbgMessage(INFORM, (ADMTEXT("Issue Request fail\n")));
        #endif
        return LM_STATUS_FAILURE;
    }
    status = USBM_GetTransferStatus( hTransfer, NULL, NULL );
    (*pUsbFunctions->lpCloseTransfer)( hTransfer );
    return status;
}

/*
 * Get Status
 */
USBM_STATUS USBM_GetStatus( IN     USB_HANDLE   hDevice,
                            IN     DWORD        dwFlags,
                            IN     UCHAR        bIndex,
                            IN OUT LPWORD       lpwStatus )
{
    USB_TRANSFER   hTransfer;
    USBM_STATUS    status;

    #ifdef USBM_DEBUG_PC
      DbgMessage(INFORM, (ADMTEXT("*** USBM_GetStatus, dwFlags = %08xh, bIndex = %02xh ***\n"), dwFlags,bIndex));
    #endif

    hTransfer = (*pUsbFunctions->lpGetStatus)( hDevice,NULL,NULL,dwFlags,bIndex,lpwStatus );
    if( !hTransfer )
    {
        #ifdef USBM_DEBUG_PC
          DbgMessage(INFORM, (ADMTEXT("Issue Request fail\n")));
        #endif
        return LM_STATUS_FAILURE;
    }
    status = USBM_GetTransferStatus( hTransfer, NULL, NULL );
    (*pUsbFunctions->lpCloseTransfer)( hTransfer );
    return status;
}

/*
 * Open Pipe
 */
USBM_STATUS USBM_OpenPipe( IN     USB_HANDLE                   hDevice,
                           IN     LPCUSB_ENDPOINT_DESCRIPTOR   lpEndpointDescriptor,
                           IN OUT USB_PIPE *                   phUsbPipe )
{
    #ifdef USBM_DEBUG_PC
      DbgMessage(INFORM, (ADMTEXT("*** USBM_OpenPipe, Endpoint %02xh ***\n"), lpEndpointDescriptor->bEndpointAddress));
    #endif

    *phUsbPipe = (*pUsbFunctions->lpOpenPipe)( hDevice, lpEndpointDescriptor );
    if( !(*phUsbPipe) )
    {
        #ifdef USBM_DEBUG_PC
          DbgMessage(INFORM, (ADMTEXT("USBM_OpenPipe fail\n")));
        #endif
        return LM_STATUS_FAILURE;
    }
    #ifdef USBM_DEBUG_PC
      DbgMessage(INFORM, (ADMTEXT("Pipe Handle = %08xh\n"), *phUsbPipe));
    #endif
    return LM_STATUS_SUCCESS;
}

/*
 * Abort Pipe Transfers
 */
USBM_STATUS USBM_AbortPipeTransfers( IN  USB_PIPE   hUsbPipe )
{
    BOOL   result;

    #ifdef USBM_DEBUG_PC
      DbgMessage(INFORM, (ADMTEXT("*** USBM_AbortPipeTransfers, Pipe Handle = %08xh ***\n"), hUsbPipe));
    #endif

    result = (*pUsbFunctions->lpAbortPipeTransfers)( hUsbPipe, 0 );
    if( !result )
    {
        #ifdef USBM_DEBUG_PC
          DbgMessage(INFORM, (ADMTEXT("USBM_AbortPipeTransfers fail\n")));
        #endif
        return LM_STATUS_FAILURE;
    }
    return LM_STATUS_SUCCESS;
}

/*
 * Close Pipe
 */
USBM_STATUS USBM_ClosePipe( IN  USB_PIPE   hUsbPipe )
{
    BOOL   result;

    #ifdef USBM_DEBUG_PC
      DbgMessage(INFORM, (ADMTEXT("*** USBM_ClosePipe, Pipe Handle = %08xh ***\n"), hUsbPipe));
    #endif

    result = (*pUsbFunctions->lpClosePipe)( hUsbPipe );
    if( !result )
    {
        #ifdef USBM_DEBUG_PC
          DbgMessage(INFORM, (ADMTEXT("USBM_ClosePipe fail\n")));
        #endif
        return LM_STATUS_FAILURE;
    }
    return LM_STATUS_SUCCESS;
}

/*
 * Issue Vendor Transfer
 */
USBM_STATUS USBM_IssueVendorTransfer( IN     USB_HANDLE              hDevice,
                                      IN     DWORD                   dwFlags,
                                      IN     LPCUSB_DEVICE_REQUEST   lpControlHeader,
                                      IN OUT LPVOID                  lpvBuffer,
                                      IN OUT ULONG                   uBufferPhysicalAddress )
{
    ULONG          PhysicalAdd;
    USB_TRANSFER   hTransfer;
    USBM_STATUS    status;

    #ifdef USBM_DEBUG_PC
      DbgMessage(INFORM, (ADMTEXT("*** USBM_IssueVendorTransfer ***\n")));
    #endif

    PhysicalAdd = uBufferPhysicalAddress;
    hTransfer = (*pUsbFunctions->lpIssueVendorTransfer)( hDevice,NULL,NULL,dwFlags,lpControlHeader,lpvBuffer,PhysicalAdd );
    if( !hTransfer )
    {
        #ifdef USBM_DEBUG_PC
          DbgMessage(INFORM, (ADMTEXT("Issue Request fail\n")));
        #endif
        return LM_STATUS_FAILURE;
    }
    status = USBM_GetTransferStatus( hTransfer, NULL, NULL );
    (*pUsbFunctions->lpCloseTransfer)( hTransfer );
    return status;
}

/*
 * Issue Bulk Transfer
 */
USBM_STATUS USBM_IssueBulkTransfer( IN     USB_PIPE                    hPipe,
                                    IN     LPTRANSFER_NOTIFY_ROUTINE   lpStartAddress,
                                    IN     LPVOID                      lpvNotifyParameter,
                                    IN     DWORD                       dwFlags,
                                    IN     DWORD                       dwBufferSize,
                                    IN OUT LPVOID                      lpvBuffer,
                                    IN OUT ULONG                       uBufferPhysicalAddress,
                                    IN OUT USB_TRANSFER *              phTransfer )
{
    ULONG   PhysicalAdd;

    #ifdef USBM_DEBUG_PC
      //DbgMessage(INFORM, (ADMTEXT("*** USBM_IssueBulkTransfer ***\n")));
    #endif
//  printf("R");

    PhysicalAdd = uBufferPhysicalAddress;
    *phTransfer = (*pUsbFunctions->lpIssueBulkTransfer)( hPipe,lpStartAddress,lpvNotifyParameter,dwFlags,dwBufferSize,lpvBuffer,PhysicalAdd );
    if( !(*phTransfer) )
    {
        #ifdef USBM_DEBUG_PC
          //DbgMessage(INFORM, (ADMTEXT("Issue Bulk Transfer fail, transfer handle is NULL.\n")));
        #endif
        return LM_STATUS_FAILURE;
    }
    #ifdef USBM_DEBUG_PC
      //DbgMessage(INFORM, (ADMTEXT("Bulk Transfer Handle = %08xh\n"), *phTransfer));
    #endif
    return LM_STATUS_SUCCESS;
}
/*
 * Issue Bulk Transfer
 */
USBM_STATUS USBM_IssueBulkTransfer1(IN     USB_PIPE                    hPipe,
                                    IN     LPTRANSFER_NOTIFY_ROUTINE   lpStartAddress,
                                    IN     LPVOID                      lpvNotifyParameter,
                                    IN     DWORD                       dwFlags,
                                    IN     DWORD                       dwBufferSize,
                                    IN OUT LPVOID                      lpvBuffer,
                                    IN OUT ULONG                       uBufferPhysicalAddress,
                                    IN OUT USB_TRANSFER *              phTransfer )
{
    ULONG   PhysicalAdd;

    #ifdef USBM_DEBUG_PC
      DbgMessage(INFORM, (ADMTEXT("*** USBM_IssueBulkTransfer ***\n")));
    #endif

    PhysicalAdd = uBufferPhysicalAddress; 
    *phTransfer = (*pUsbFunctions->lpIssueBulkTransfer)( hPipe,lpStartAddress,lpvNotifyParameter,dwFlags,dwBufferSize,lpvBuffer,PhysicalAdd );
 
    if( !(*phTransfer) )
    {
        #ifdef USBM_DEBUG_PC
          DbgMessage(INFORM, (ADMTEXT("Issue Bulk Transfer fail, transfer handle is NULL.\n")));
        #endif
   
        return LM_STATUS_FAILURE;
    }
    #ifdef USBM_DEBUG_PC
      DbgMessage(INFORM, (ADMTEXT("Bulk Transfer Handle = %08xh\n"), *phTransfer));
    #endif
 
    return LM_STATUS_SUCCESS;
}

/*
 * Get Transfer Status
 */
USBM_STATUS USBM_GetTransferStatus( USB_TRANSFER   hTransfer,
                                    LPDWORD        lpBytesTransferred,
                                    LPDWORD        lpError)
{
    BOOL      result;
    DWORD     dwTransfered, dwError;
    LPDWORD   lpdwBytesTransferred, lpdwError;

    if( !lpBytesTransferred )    lpdwBytesTransferred = &dwTransfered;
    else                         lpdwBytesTransferred = lpBytesTransferred;
    if( !lpError )    lpdwError = &dwError;
    else              lpdwError = lpError;

    if( !hTransfer )
    {
        #ifdef USBM_DEBUG_PC
          DbgMessage(INFORM, (ADMTEXT("Transfer handle is NULL, so can't get status\n")));
        #endif
        return LM_STATUS_FAILURE;
    }

    result = (*pUsbFunctions->lpIsTransferComplete)( hTransfer );
    if( result )
    {
        result = (*pUsbFunctions->lpGetTransferStatus)( hTransfer, lpdwBytesTransferred, lpdwError );
        if( (!result) || (*lpdwError != USB_NO_ERROR) )
        {
            #ifdef USBM_DEBUG_PC
              DbgMessage(INFORM, (ADMTEXT("Transfer %08xh Completed with error %08xh\n"), hTransfer, *lpdwError));
              
            #endif            
            return LM_STATUS_FAILURE;
        }
    }
    else
    {
        #ifdef USBM_DEBUG_PC
          DbgMessage(INFORM, (ADMTEXT("Transfer %08xh not completed yet\n"), hTransfer));
        #endif
        return LM_STATUS_FAILURE;
    }

    #ifdef USBM_DEBUG_PC
//      DbgMessage(INFORM, (ADMTEXT("Transfer %08xh completed success\n"), hTransfer));
    #endif
    return LM_STATUS_SUCCESS;
}

/*
 * Reset Pipe
 */
USBM_STATUS USBM_ResetPipe( IN  USB_PIPE   hUsbPipe )
{
    BOOL   result;

    #ifdef USBM_DEBUG_PC
      DbgMessage(INFORM, (ADMTEXT("*** USBM_ResetPipe, Pipe Handle = %08xh ***\n"), hUsbPipe));
    #endif

    result = (*pUsbFunctions->lpResetPipe)( hUsbPipe );
    if( !result )
    {
        #ifdef USBM_DEBUG_PC
          DbgMessage(INFORM, (ADMTEXT("USBM_ClosePipe fail\n")));
        #endif
        return LM_STATUS_FAILURE;
    }
    return LM_STATUS_SUCCESS;
}

/*
 * Close Transfer
 */
void USBM_CloseTransfer( IN  USB_TRANSFER    hTransfer )
{
    #ifdef USBM_DEBUG_PC
      //DbgMessage(INFORM, (ADMTEXT("*** USBM_CloseTransfer, Transfer Handle = %08xh ***\n"), hTransfer));
    #endif
    (*pUsbFunctions->lpCloseTransfer)( hTransfer );
}
