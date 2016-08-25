/******************************************************************************/
/* Module Description: USBM.H                                                 */
/******************************************************************************/
#ifndef USBM_H
#define USBM_H

#include <windows.h>
#include "usbdi.h"
#include "lmstatus.h"
#include "debug.h"

#define USBM_STATUS    LM_STATUS
#define PUSBM_STATUS   PLM_STATUS


/*
 * Function Prototypes
 */
USBM_STATUS USBM_GetDeviceInfo( IN  USB_HANDLE        hDevice,
                                OUT LPCUSB_DEVICE *   plpDeviceInfo );

USBM_STATUS USBM_FindInterface( IN  LPCUSB_DEVICE        lpDeviceInfo,
                                IN  UCHAR                bInterfaceNumber,
                                IN  UCHAR                bAlternateSetting,
                                OUT LPCUSB_INTERFACE *   plpInterface );

USBM_STATUS USBM_GetDescriptor( IN  USB_HANDLE   hDevice,
                                IN  UCHAR        bType,
                                IN  WORD         wLength,
                                IN  LPVOID       lpvBuffer );

USBM_STATUS USBM_ClearFeature( IN  USB_HANDLE   hDevice,
                               IN  DWORD        dwFlags,
                               IN  WORD         wFeature,
                               IN  UCHAR        bIndex );

USBM_STATUS USBM_SetFeature( IN  USB_HANDLE   hDevice,
                             IN  DWORD        dwFlags,
                             IN  WORD         wFeature,
                             IN  UCHAR        bIndex );

USBM_STATUS USBM_GetStatus( IN     USB_HANDLE   hDevice,
                            IN     DWORD        dwFlags,
                            IN     UCHAR        bIndex,
                            IN OUT LPWORD       lpwStatus );

USBM_STATUS USBM_OpenPipe( IN     USB_HANDLE                   hDevice,
                           IN     LPCUSB_ENDPOINT_DESCRIPTOR   lpEndpointDescriptor,
                           IN OUT USB_PIPE *                   phUsbPipe );

USBM_STATUS USBM_AbortPipeTransfers( IN  USB_PIPE   hUsbPipe );

USBM_STATUS USBM_ClosePipe( IN  USB_PIPE   hUsbPipe );

USBM_STATUS USBM_IssueVendorTransfer( IN     USB_HANDLE              hDevice,
                                      IN     DWORD                   dwFlags,
                                      IN     LPCUSB_DEVICE_REQUEST   lpControlHeader,
                                      IN OUT LPVOID                  lpvBuffer,
                                      IN OUT ULONG                   uBufferPhysicalAddress );

USBM_STATUS USBM_IssueBulkTransfer( IN     USB_PIPE                    hPipe,
                                    IN     LPTRANSFER_NOTIFY_ROUTINE   lpStartAddress,
                                    IN     LPVOID                      lpvNotifyParameter,
                                    IN     DWORD                       dwFlags,
                                    IN     DWORD                       dwBufferSize,
                                    IN OUT LPVOID                      lpvBuffer,
                                    IN OUT ULONG                       uBufferPhysicalAddress,
                                    IN OUT USB_TRANSFER *              phTransfer );

USBM_STATUS USBM_IssueBulkTransfer1( IN     USB_PIPE                    hPipe,
                                    IN     LPTRANSFER_NOTIFY_ROUTINE   lpStartAddress,
                                    IN     LPVOID                      lpvNotifyParameter,
                                    IN     DWORD                       dwFlags,
                                    IN     DWORD                       dwBufferSize,
                                    IN OUT LPVOID                      lpvBuffer,
                                    IN OUT ULONG                       uBufferPhysicalAddress,
                                    IN OUT USB_TRANSFER *              phTransfer );

USBM_STATUS USBM_GetTransferStatus( USB_TRANSFER   phTransfer,
                                    LPDWORD        lpdwBytesTransferred,
                                    LPDWORD        lpdwError);


USBM_STATUS USBM_ResetPipe( IN  USB_PIPE   hUsbPipe );

void USBM_CloseTransfer( IN  USB_TRANSFER    hTransfer );

#endif
