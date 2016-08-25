/*++
Module Name:   8511NDS.c
Abstract:      ADMtek ADM8511(with AN986) NDIS driver
--*/
#include <windows.h>
#include <ndis.h>
#include "usbdi.h"
#include "lm.h"

// #define LANNDS_DEBUG 	1 // unmark - 07/25/05,bolo

#ifdef DEBUG

// 1. Define Defining Debug Zones
#define DBG_ERROR      1
#define DBG_WARN       2
#define DBG_FUNCTION   4
#define DBG_INIT       8
#define DBG_INTR       16
#define DBG_RCV        32
#define DBG_XMIT       64

// 2. Declaring a DBGPARAM Structure
DBGPARAM dpCurSettings =
{
   TEXT("LANNDS"),
   { TEXT("Errors"),     TEXT("Warnings"),  TEXT("Functions"), TEXT("Init"),
     TEXT("Interrupts"), TEXT("Receives"),  TEXT("Transmits"), TEXT("Undefined"),
     TEXT("Undefined"),  TEXT("Undefined"), TEXT("Undefined"), TEXT("Undefined"),
     TEXT("Undefined"),  TEXT("Undefined"), TEXT("Undefined"), TEXT("Undefined") },
   DBG_ERROR | DBG_WARN | DBG_FUNCTION | DBG_INIT | DBG_INTR | DBG_RCV | DBG_XMIT
};
#endif  // DEBUG

typedef void (* LPUsbDataExchange)( USB_HANDLE * , LPCUSB_FUNCS * );


extern PNDIS_MINIPORT_BLOCK    pMiniportBlock;
extern BOOL                    NdisDriverLoaded;
extern PLM_DEVICE_INFO_BLOCK   pDeviceForHeader;
extern DWORD                   ReceiveMaskForHeader;

USB_HANDLE     hUSB;
LPCUSB_FUNCS   pUsbFunctions;
BOOL           Unplugged = FALSE;
extern	HANDLE resetEvent;
extern HANDLE th;

BOOL __stdcall DllEntry( HANDLE hDLL, DWORD dwReason, LPVOID lpReserved )
{
	HINSTANCE            hInst;
  LPUsbDataExchange    pUsbDataEx;

  switch (dwReason)
  {
  	case DLL_PROCESS_ATTACH:
    	DEBUGREGISTER(hDLL);

      hInst = LoadLibrary( L"LANUSB.DLL" );
      pUsbDataEx = (LPUsbDataExchange)GetProcAddress(hInst, L"UsbDataExchange");
      (*pUsbDataEx)( &hUSB, &pUsbFunctions );
      FreeLibrary( hInst );

			#ifdef LANNDS_DEBUG
				DbgMessage(VERBOSE, (TEXT("*** LANUSB_PROCESS_ATTACH ***\n"))); // add - 07/25/05,bolo
            #endif

            #ifdef DEBCEINIT // mod - 09/09/05,add - 07/28/05,bolo
                NKDbgPrintfW(TEXT("DllEntry: DLL_PROCESS_ATTACH (LANNDS)!\n"));
			#endif
			break;

		case DLL_PROCESS_DETACH:
			break;
			
		default:
			break;
	}
	return TRUE;
}

void NdsDataExchange( DWORD * pMiniport )
{
    *pMiniport = (DWORD)pMiniportBlock;
}

DWORD UnPlugNotify( void )
{
    Unplugged = TRUE;
    LM_AbortPipe( pDeviceForHeader );
	if( resetEvent != INVALID_HANDLE_VALUE ){
		SetEvent(resetEvent);
		//We must exit here anyway, so no INFINITE.
      	WaitForSingleObject( th, 1000);
      	CloseHandle(th);
		th = NULL;
	}
    return (DWORD)pMiniportBlock;    // add by JACKL
}

/*
BOOL AttachNotify( void )
{
   HINSTANCE               hInst;
   LPUsbDataExchange       pUsbDataEx;
   LM_STATUS               Status;


    if( NdisDriverLoaded && pDeviceForHeader )
    {
         #ifdef LANNDS_DEBUG
           printf("###### Be notified that Device is Plugged.\n");
         #endif
         
         Unplugged = FALSE;
         hInst = LoadLibrary( L"LANUSB.DLL" );
         pUsbDataEx = (LPUsbDataExchange)GetProcAddress(hInst, L"UsbDataExchange");
         (*pUsbDataEx)( &hUSB, &pUsbFunctions );
         FreeLibrary( hInst );
		
		 // for wince suspend
         Status = LM_InitializeAdapter( pDeviceForHeader );
         Status = LM_ResetAdapter( pDeviceForHeader );
         Status = LM_SetReceiveMask( pDeviceForHeader, ReceiveMaskForHeader );
     
    return NdisDriverLoaded;
}
*/
