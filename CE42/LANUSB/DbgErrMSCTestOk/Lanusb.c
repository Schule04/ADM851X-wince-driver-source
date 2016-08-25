/*++
Module Name:   8511USB.c
Abstract:      ADMtek ADM8511(backward compatiable AN986) USB client driver
--*/
//version v1.01 
//modify date 2001/02/02
#include <windows.h>
#include <ndis.h>
#include "usbdi.h"
#include "debug.h"

//#define LANUSB_DEBUG_PC	1
//#define IODATA			1
#define	ADMtekUSB		1
//#define	SMC2206			1

#ifdef IODATA
#define VENDOR_ID        0x04bb
#define PRODUCT_ID       0x0913
#define DRIVER_ID        L"IO-DATA_LANUSB_Driver"
#define Registory_key L"Drivers\\USB\\ClientDrivers\\IO-DATA_LANUSB_Driver"
#endif

#ifdef ADMtekUSB
#define VENDOR_ID        0x07a6
#define PRODUCT_ID       0x8511
#define DRIVER_ID        L"ADMTEK_LANUSB_Driver"
#define Registory_key L"Drivers\\USB\\ClientDrivers\\ADMTEK_LANUSB_Driver"
#endif

#ifdef SMC2206
#define VENDOR_ID        0x0423
#define PRODUCT_ID       0x0301
#define DRIVER_ID        L"SMC_LANUSB_Driver"
#define Registory_key L"Drivers\\USB\\ClientDrivers\\SMC_LANUSB_Driver"
#endif

#define NDISDRIVERNAME   L"NDIS.DLL"

HANDLE 		WinceHandle;

#ifdef DEBUG
#define DBG_ERROR      1
#define DBG_WARN       2
#define DBG_FUNCTION   4
#define DBG_INIT       8
#define DBG_INTR       16
#define DBG_RCV        32
#define DBG_XMIT       64
DBGPARAM dpCurSettings =
{
   TEXT("LANUSB"),
   { TEXT("Errors"),     TEXT("Warnings"),  TEXT("Functions"), TEXT("Init"),
     TEXT("Interrupts"), TEXT("Receives"),  TEXT("Transmits"), TEXT("Undefined"),
     TEXT("Undefined"),  TEXT("Undefined"), TEXT("Undefined"), TEXT("Undefined"),
     TEXT("Undefined"),  TEXT("Undefined"), TEXT("Undefined"), TEXT("Undefined") },
   DBG_ERROR | DBG_WARN | DBG_FUNCTION | DBG_INIT | DBG_INTR | DBG_RCV | DBG_XMIT
};
#endif  // DEBUG

typedef struct _REG_VALUE_DESCR
{
    LPWSTR val_name;
    DWORD  val_type;
    PBYTE  val_data;
} REG_VALUE_DESCR, * PREG_VALUE_DESCR;

// Values for [HKEY_LOCAL_MACHINE\Comm\8511NDS] and [HKEY_LOCAL_MACHINE\Comm\8511NDS1]
REG_VALUE_DESCR CommKeyValues[] =
{
   (TEXT("DisplayName")), REG_SZ, (PBYTE)(TEXT("USB/Ethernet Series Adapter")),
   (TEXT("Group")),       REG_SZ, (PBYTE)(TEXT("NDIS")),
   (TEXT("ImagePath")),   REG_SZ, (PBYTE)(TEXT("LANNDS.DLL")),
   NULL,                  0,      NULL
};

// Values for [HKEY_LOCAL_MACHINE\Comm\8511NDS\Linkage]
REG_VALUE_DESCR LinkageKeyValues[] =
{
   (TEXT("Route")), REG_MULTI_SZ, (PBYTE)(TEXT("LANNDS1\0")),
   NULL,            0,            NULL
};

// Values for [HKEY_LOCAL_MACHINE\Comm\8511NDS1\Parms]
REG_VALUE_DESCR ParmKeyValues[] =
{
   (TEXT("BusNumber")), REG_DWORD, (PBYTE)0,
   (TEXT("BusType")),   REG_DWORD, (PBYTE)(-1),
   NULL,                0,         NULL
};

// Values for [HKEY_LOCAL_MACHINE\Comm\Tcpip\Linkage]
REG_VALUE_DESCR BindTcpipKeyValues[] =
{
   (TEXT("Bind")), REG_MULTI_SZ, (PBYTE)(TEXT("LANNDS1\0")),
   NULL,           0,            NULL
};

// Values for [HKEY_LOCAL_MACHINE\Comm\8511NDS1\Parms\Tcpip]
REG_VALUE_DESCR ParmTcpipKeyValues[] =
{
   (TEXT("EnableDHCP")),       REG_DWORD, (PBYTE)1,
   (TEXT("DefaultGateway")),   REG_SZ,    (PBYTE)(TEXT("")),
   (TEXT("UseZeroBroadcast")), REG_DWORD, (PBYTE)0,
   (TEXT("IpAddress")),        REG_SZ,    (PBYTE)(TEXT("0.0.0.0")),
   (TEXT("Subnetmask")),       REG_SZ,    (PBYTE)(TEXT("0.0.0.0")),
   NULL,                       0,         NULL
};
REG_VALUE_DESCR UsbDllValues[] =
{
   (TEXT("Dll")),   REG_SZ,    (PBYTE)(TEXT("LANUSB.dll")),
   (TEXT("Prefix")),       REG_SZ,    (PBYTE)(TEXT("NDS")),
   NULL,                       0,         NULL
};

PREG_VALUE_DESCR Values[] =
{
    CommKeyValues,
    LinkageKeyValues,
    CommKeyValues,
    ParmKeyValues,
    UsbDllValues,
//  BindTcpipKeyValues,
//  ParmTcpipKeyValues
};

//const LPCWSTR Registory_key = {TEXT("Drivers\\USB\\ClientDrivers\\ADMtek_8511USB_Driver")};
//#define Registory_key L"Drivers\\USB\\ClientDrivers\\SMC_LANUSB_Driver"
LPWSTR KeyNames[] =
{
    (TEXT("Comm\\LANNDS")),
    (TEXT("Comm\\LANNDS\\Linkage")),
    (TEXT("Comm\\LANNDS1")),
    (TEXT("Comm\\LANNDS1\\Parms")),
    (Registory_key), //Jackl
//  (TEXT("Comm\\Tcpip\\Linkage")),
//  (TEXT("Comm\\8511NDS1\\Parms\\Tcpip"))
};


typedef DWORD (* LPNDS_INIT)( DWORD );
typedef BOOL (* LPNDS_DEINIT)( DWORD );

typedef void (* LPNdsDataExchange)( DWORD * );
//typedef void (* LPUnPlugNotify)( void );
typedef DWORD (* LPUnPlugNotify)( void ); //JACKL
typedef BOOL (* LPAttachNotify)( void );

void SetRegistry( void );
BOOL AddKeyValues( LPWSTR KeyName, PREG_VALUE_DESCR Vals );

const WCHAR gcszRegisterClientDriverId[]   = L"RegisterClientDriverID";
const WCHAR gcszRegisterClientSettings[]   = L"RegisterClientSettings";
const WCHAR gcszUnRegisterClientDriverId[] = L"UnRegisterClientDriverID";
const WCHAR gcszUnRegisterClientSettings[] = L"UnRegisterClientSettings";
const WCHAR gcszDriverId[]                 = DRIVER_ID;

//DWORD          Miniport;
//HANDLE         hRegisteredDevice;

USB_HANDLE     hUSB;
LPCUSB_FUNCS   pUsbFunctions;
BOOL           Unplugged;

DWORD          NdisContext;
BOOL           UserTcpipSetting;


#define		MPX_MUTEX_NAME	TEXT("LANUSB_Mutex")	//Mutex
HANDLE		g_hMutex = NULL;

BOOL MutexAlloc()
{
	BOOL bRet=FALSE;
	
	if( !g_hMutex ){
		g_hMutex = CreateMutex(NULL, FALSE, MPX_MUTEX_NAME);
		if( g_hMutex ){
			bRet=TRUE;
		}
	}
	return bRet;
}

void MutexFree()
{
	if( g_hMutex ){
		CloseHandle(g_hMutex);
		g_hMutex = NULL;
	}
}

void Lock( void ){
	DWORD dwResult;
	dwResult = WaitForSingleObject(g_hMutex, INFINITE);
	return;
}

void Unlock( void ){
	BOOL bRet;
	bRet = ReleaseMutex(g_hMutex);
	return;
}

BOOL DllEntry( HANDLE hDllHandle, DWORD dwReason, LPVOID lpreserved )
{
    switch (dwReason)
    {
        case DLL_PROCESS_ATTACH:
            DEBUGREGISTER((HINSTANCE)hDllHandle);
          	#ifdef LANUSB_DEBUG_PC
              DbgMessage(VERBOSE, (ADMTEXT("*** DLL_PROCESS_ATTACH ***\n")));
            #endif  
			MutexAlloc();
            break;
        case DLL_PROCESS_DETACH:
            #ifdef LANUSB_DEBUG_PC
              DbgMessage(VERBOSE, (ADMTEXT("*** DLL_PROCESS_DETACH ***\n")));
            #endif 
			MutexFree();
        default:
            break;
    }
    return TRUE ;
}

/*
 * @func   BOOL | USBInstallDriver | Install USB client driver.
 * @rdesc  Return TRUE if install succeeds, or FALSE if there is some error.
 * @comm   This function is called by USBD when an unrecognized device
 *         is attached to the USB and the user enters the client driver
 *         DLL name.  It should register a unique client id string with
 *         USBD, and set up any client driver settings.
 * @xref   <f USBUnInstallDriver>
 */
BOOL USBInstallDriver( LPCWSTR szDriverLibFile )  // @parm [IN] - Contains client driver DLL name
{
    BOOL        fRet  = FALSE;
    HINSTANCE   hInst = LoadLibrary(L"USBD.DLL");
	LPREGISTER_CLIENT_DRIVER_ID    pRegisterId       = NULL;
	LPREGISTER_CLIENT_SETTINGS     pRegisterSettings = NULL;
	LPUN_REGISTER_CLIENT_DRIVER_ID pUnRegisterId     = NULL;

    // This is the install routine for the mouse sample driver.  Typically, this
    // routine will never be invoked for the mouse driver, as most platforms will
    // hard code the mouse driver settings into the registry.
   #ifdef LANUSB_DEBUG_CE
     printf("USBInstallDriver \n");
   #endif
   #ifdef LANUSB_DEBUG_PC
     DbgMessage(VERBOSE, (ADMTEXT("*** USBInstallDriver ***\n")));
   #endif

    if(hInst)
    {
        pRegisterId         = (LPREGISTER_CLIENT_DRIVER_ID)   GetProcAddress(hInst, gcszRegisterClientDriverId);
        pRegisterSettings   = (LPREGISTER_CLIENT_SETTINGS)    GetProcAddress(hInst, gcszRegisterClientSettings);
        pUnRegisterId       = (LPUN_REGISTER_CLIENT_DRIVER_ID)GetProcAddress(hInst, gcszUnRegisterClientDriverId);

        if(pRegisterId && pRegisterSettings && pUnRegisterId)
        {
            USB_DRIVER_SETTINGS   DriverSettings;

            DriverSettings.dwCount = sizeof(DriverSettings);

            // Set up our DriverSettings struct to specify how we are to
            // be loaded.  Mice and keyboards can be identified as boot
            // devices within the HID interface class.
            DriverSettings.dwVendorId      = VENDOR_ID;
            DriverSettings.dwProductId     = PRODUCT_ID;
            DriverSettings.dwReleaseNumber = USB_NO_INFO;

            DriverSettings.dwDeviceClass    = USB_NO_INFO;
            DriverSettings.dwDeviceSubClass = USB_NO_INFO;
            DriverSettings.dwDeviceProtocol = USB_NO_INFO;

            DriverSettings.dwInterfaceClass    = USB_NO_INFO;
            DriverSettings.dwInterfaceSubClass = USB_NO_INFO;
            DriverSettings.dwInterfaceProtocol = USB_NO_INFO;

            fRet = (*pRegisterId)(gcszDriverId);

            if(fRet)
            {
                fRet = (*pRegisterSettings)(szDriverLibFile, gcszDriverId, NULL, &DriverSettings);

                if(!fRet)
                {
                    (*pUnRegisterId)(gcszDriverId);
                }
            }
        }
        FreeLibrary(hInst);
    }

    return fRet;
}

/*
 * @func   BOOL | USBUnInstallDriver | Uninstall USB client driver.
 * @rdesc  Return TRUE if install succeeds, or FALSE if there is some error.
 * @comm   This function can be called by a client driver to deregister itself
 *         with USBD.
 * @xref   <f USBInstallDriver>
 */
BOOL USBUnInstallDriver()
{
    BOOL        fRet  = FALSE;
    HINSTANCE   hInst = LoadLibrary(L"USBD.DLL");
    LPUN_REGISTER_CLIENT_DRIVER_ID pUnRegisterId       = NULL;
    LPUN_REGISTER_CLIENT_SETTINGS  pUnRegisterSettings = NULL;
	
	#ifdef LANUSB_DEBUG_PC
     DbgMessage(VERBOSE, (ADMTEXT("*** USBUnInstallDriver ***\n")));
    #endif

    if(hInst)
    {
        pUnRegisterId       = (LPUN_REGISTER_CLIENT_DRIVER_ID)GetProcAddress(hInst, gcszUnRegisterClientDriverId);
        pUnRegisterSettings = (LPUN_REGISTER_CLIENT_SETTINGS) GetProcAddress(hInst, gcszUnRegisterClientSettings);

        if(pUnRegisterSettings)
        {
            USB_DRIVER_SETTINGS DriverSettings;

            DriverSettings.dwCount = sizeof(DriverSettings);

            // This structure must be filled out the same as we registered with
            DriverSettings.dwVendorId      = VENDOR_ID;
            DriverSettings.dwProductId     = PRODUCT_ID;
            DriverSettings.dwReleaseNumber = USB_NO_INFO;

            DriverSettings.dwDeviceClass    = USB_NO_INFO;
            DriverSettings.dwDeviceSubClass = USB_NO_INFO;
            DriverSettings.dwDeviceProtocol = USB_NO_INFO;

            DriverSettings.dwInterfaceClass    = USB_NO_INFO;
            DriverSettings.dwInterfaceSubClass = USB_NO_INFO;
            DriverSettings.dwInterfaceProtocol = USB_NO_INFO;

            fRet = (*pUnRegisterSettings)(gcszDriverId, NULL, &DriverSettings);
        }

        if(pUnRegisterId)
        {
            BOOL fRetTemp = (*pUnRegisterId)(gcszDriverId);
            fRet = fRet ? fRetTemp : fRet;
        }
        FreeLibrary(hInst);
    }

    return fRet;
}

// Process notifications from USBD.  Currently, the only notification
// supported is a device removal.
BOOL USBDeviceNotifications( LPVOID      lpvNotifyParameter,
                             DWORD       dwCode,
                             LPDWORD *   dwInfo1,
                             LPDWORD *   dwInfo2,
                             LPDWORD *   dwInfo3,
                             LPDWORD *   dwInfo4  )
{
    HINSTANCE              hInst;
//    DWORD                  MiniportHandle;
//    LPUnPlugNotify         pNdsUnplugN;

    //LPNDS_DEINIT           pNds_Deinit;
    //DWORD                  i;
    //BOOL                   result;
    DWORD                  MiniportHandle;
    NDIS_STATUS ndisStat;
    LPUnPlugNotify         pNdsUnplugN;

	BOOL				   bRet;

	Lock();
    
    #ifdef LANUSB_DEBUG_PC
     DbgMessage(VERBOSE, (ADMTEXT("*** USBDeviceNotifications ***\n")));
    #endif
    switch(dwCode)
    {
        case USB_CLOSE_DEVICE:
            Unplugged = TRUE;	    
            (*pUsbFunctions->lpUnRegisterNotificationRoutine)( hUSB, USBDeviceNotifications, (LPVOID)NdisContext );


            hInst = LoadLibrary( L"LANNDS.DLL" );
            pNdsUnplugN = (LPUnPlugNotify)GetProcAddress(hInst, L"UnPlugNotify");
            //(*pNdsUnplugN)();
            
            MiniportHandle = (*pNdsUnplugN)();
            FreeLibrary( hInst );           
            NdisDeregisterAdapter(&ndisStat,L"LANNDS1");
            
            
            #if 0
            hInst = LoadLibrary( L"LANNDS.DLL" );
            pNdsUnplugN = (LPUnPlugNotify)GetProcAddress(hInst, L"UnPlugNotify");
            //(*pNdsUnplugN)();
            MiniportHandle = (*pNdsUnplugN)();
            FreeLibrary( hInst );
            
            //add by jackl 00/04/12
            hInst = LoadLibrary( L"NDIS.DLL" );
            pNds_Deinit = (LPNDS_DEINIT)GetProcAddress(hInst, L"NDS_Deinit");
            result = (*pNds_Deinit)( MiniportHandle );
            FreeLibrary( hInst );
            #endif
              //add end
/*
            hInst = LoadLibrary( L"NDIS.DLL" );
            pNds_Deinit = (LPNDS_DEINIT)GetProcAddress(hInst, L"NDS_Deinit");
            result = (*pNds_Deinit)( 0x4d15ba5e );
            if( !result )   // RETAILMSG(1,(TEXT("NDS_Deinit fail\n")));;
            FreeLibrary( hInst );

            //result = DeregisterDevice( hRegisteredDevice );

            for( i=0; i<( sizeof(KeyNames)/sizeof(LPWSTR) ); i++ )
            {
                if( (i == ( sizeof(KeyNames)/sizeof(LPWSTR) - 1 )) && UserTcpipSetting )
                     break;
                RegDeleteKey( HKEY_LOCAL_MACHINE, KeyNames[i] );
            }
*/
            NdisContext = 0;
            //hRegisteredDevice = NULL;
            hUSB = NULL;
            pUsbFunctions = NULL;
            DeactivateDevice(WinceHandle); //Jackl
			bRet = TRUE;
			break;

		default:
			bRet = FALSE;
			break;
    }
	
	Unlock();
	
	return bRet;
}

/*
 *  @func   BOOL | USBDeviceAttach | USB device attach routine.
 *  @rdesc  Return TRUE upon success, or FALSE if an error occurs.
 *  @comm   This function is called by USBD when a device is attached
 *          to the USB, and a matching registry key is found off the
 *          LoadClients registry key. The client  should determine whether
 *          the device may be controlled by this driver, and must load
 *          drivers for any uncontrolled interfaces.
 *  @xref   <f FindInterface> <f LoadGenericInterfaceDriver>
 */
BOOL USBDeviceAttach( USB_HANDLE               hDevice,         // @parm [IN] - USB device handle
                      LPCUSB_FUNCS             lpUsbFuncs,      // @parm [IN] - Pointer to USBDI function table
                      LPCUSB_INTERFACE         lpInterface,     // @parm [IN] - If client is being loaded as an interface
                                                                //              driver, contains a pointer to the USB_INTERFACE
                                                                //              structure that contains interface information.
                                                                //              If client is not loaded for a specific interface,
                                                                //              this parameter will be NULL, and the client
                                                                //              may get interface information through <f FindInterface>
                      LPCWSTR                  szUniqueDriverId,// @parm [IN] - Contains client driver id string
                      LPBOOL                   fAcceptControl,  // @parm [OUT]- Filled in with TRUE if we accept control
                                                                //              of the device, or FALSE if USBD should continue
                                                                //              to try to load client drivers.
                      LPCUSB_DRIVER_SETTINGS   lpDriverSettings,// @parm [IN] - Contains pointer to USB_DRIVER_SETTINGS
                                                                //              struct that indicates how we were loaded.
                      DWORD                    dwUnused )       // @parm [IN] - Reserved for use with future versions of USBD
{
//    HINSTANCE             hInst;
//    LPNDS_INIT            pNds_Init;
//    LPAttachNotify        pAttachN;
    //LPNdsDataExchange     pNdsDataEx;
    BOOL                  NdisDriverLoaded = FALSE; 
    NDIS_STATUS ndisStat = TRUE;   
    *fAcceptControl = FALSE;
    NdisContext = 0;
    //hRegisteredDevice = NULL;
    hUSB = NULL;
    pUsbFunctions = NULL;
    
    #ifdef LANUSB_DEBUG_PC
     DbgMessage(VERBOSE, (ADMTEXT("*** USBDeviceAttach ***\n")));
     DbgMessage(VERBOSE, (ADMTEXT("VID = 0x%x, PID = 0x%x\n",lpDriverSettings->dwVendorId, lpDriverSettings->dwProductId)));
    #endif
    
	Lock();
    
    //if( lpDriverSettings->dwVendorId == VENDOR_ID && lpDriverSettings->dwProductId == PRODUCT_ID )
    if( lpDriverSettings->dwVendorId == VENDOR_ID )
    {
        #ifdef LANUSB_DEBUG
         printf("USBDeviceAttach : VID = 0x%x, PID = 0x%x\n",lpDriverSettings->dwVendorId, lpDriverSettings->dwProductId);
        #endif
        
        Unplugged = FALSE;
        hUSB = hDevice;
        pUsbFunctions = lpUsbFuncs;

//        hInst = LoadLibrary( L"LANNDS.DLL" );
//        pAttachN = (LPAttachNotify)GetProcAddress(hInst, L"AttachNotify");
//        NdisDriverLoaded = (*pAttachN)();
//        FreeLibrary( hInst );
        WinceHandle = ActivateDevice(Registory_key,(DWORD)NULL);  //jackl

        if( !NdisDriverLoaded )
        {
            SetRegistry();       
            NdisRegisterAdapter(&ndisStat,L"LANNDS", L"LANNDS1");      

            NdisContext = 0;
        }
        //hRegisteredDevice = RegisterDevice( L"NDS", 5, NDISDRIVERNAME, (DWORD)0x4d15ba5e );
/*
        hInst = LoadLibrary( L"8511NDS.DLL" );
        pNdsDataEx = (LPNdsDataExchange)GetProcAddress(hInst, L"NdsDataExchange");
        (*pNdsDataEx)( &Miniport );
        FreeLibrary( hInst );
*/
        (*lpUsbFuncs->lpRegisterNotificationRoutine)(hDevice, USBDeviceNotifications, (LPVOID)NdisContext);
        *fAcceptControl = TRUE;
    }

	Unlock();
	
    return TRUE;
}

// Set all registry for NDIS
void SetRegistry( void )
{
    DWORD   i, DhcpEnable, Bytes;
    HKEY    hKey;
    DWORD   type;

    for (i = 0; i < (sizeof(KeyNames)/sizeof(LPWSTR)); i++)
    {
        if( i == (sizeof(KeyNames)/sizeof(LPWSTR) - 1) )
        {
            UserTcpipSetting = FALSE;
            if( ERROR_SUCCESS == RegOpenKeyEx( HKEY_LOCAL_MACHINE,KeyNames[i],0,0,&hKey ) )
            {
                type = REG_DWORD;
                if( ERROR_SUCCESS == RegQueryValueEx( hKey,L"EnableDHCP",NULL,&type,(LPBYTE)&DhcpEnable,&Bytes ) )
                {
                    if( !DhcpEnable )
                    {
                        UserTcpipSetting = TRUE;
                        RegCloseKey( hKey );
                        break;
                    }
                }
                RegCloseKey( hKey );
            }
        }
        AddKeyValues(KeyNames[i], Values[i]);
    }
}

BOOL AddKeyValues( LPWSTR             KeyName,
                   PREG_VALUE_DESCR   Vals )
{
    DWORD              Status;
    DWORD              dwDisp;
    HKEY               hKey;
    PREG_VALUE_DESCR   pValue;
    DWORD              ValLen;
    PBYTE              pVal;
    DWORD              dwVal;
    LPWSTR             pStr;

    Status = RegCreateKeyEx( HKEY_LOCAL_MACHINE,KeyName,0,0,0,0,0,&hKey,&dwDisp );
    if (Status != ERROR_SUCCESS)   return FALSE;

    pValue = Vals;
    while (pValue->val_name)
    {
        switch (pValue->val_type)
        {
            case REG_DWORD:
                pVal = (PBYTE)&dwVal;
                dwVal = (DWORD)pValue->val_data;
                ValLen = sizeof(DWORD);
                break;

            case REG_SZ:
                pVal = (PBYTE)pValue->val_data;
                ValLen = (wcslen((LPWSTR)pVal) + 1)*sizeof(WCHAR);
                break;

            case REG_MULTI_SZ:
                pStr = (LPWSTR)pValue->val_data;
                dwVal = 0;
                while (*pStr)
                {
                    dwDisp += wcslen(pStr) + 1;
                    dwVal += dwDisp;
                    pStr += dwDisp;
                }
                dwVal++;

                ValLen = dwVal*sizeof(WCHAR);
                pVal = pValue->val_data;
                break;
        }
        Status = RegSetValueEx( hKey,
                                pValue->val_name,
                                0,
                                pValue->val_type,
                                pVal,
                                ValLen );
        if (Status != ERROR_SUCCESS)
        {
            RegCloseKey(hKey);
            return FALSE;
        }
        pValue++;
    }
    RegCloseKey(hKey);
    return TRUE;
} // AddKeyValues

void UsbDataExchange( USB_HANDLE * phUsb, LPCUSB_FUNCS * ppUsbFs )
{
    *phUsb   = hUSB;
    *ppUsbFs = pUsbFunctions;
}

//** jackl add start 2001/02/27 ******
BOOL NDS_Close(DWORD hOpenContext)
{
	return FALSE;
}

BOOL NDS_Deinit(DWORD hDeviceContext)
{
	return TRUE;
}

DWORD NDS_Init(DWORD dwContext)
{
	return dwContext;
}

BOOL NDS_IOControl(DWORD hOpenContext,
              DWORD dwCode,
              PBYTE pBufIn,
              DWORD dwLenIn,
              PBYTE pBufOut,
              DWORD dwLenOut,
              PDWORD pdwActualOut )	
{
     return FALSE;
}

DWORD NDS_Open(DWORD hDeviceContext, DWORD AccessCode ,DWORD ShareMode )
{
     return hDeviceContext;
}

void NDS_PowerDown(DWORD hDeviceContext ,DWORD AccessCode ,DWORD ShareMode )
{
}

void NDS_PowerUp( DWORD hDeviceContext )
{
}

DWORD NDS_Read(DWORD hOpenContext, LPVOID pBuffer, DWORD Count )
{
	return -1;
}

DWORD NDS_Seek(DWORD hOpenContext, long Amount ,WORD Type )
{
	return -1;
}

DWORD NDS_Write(DWORD hOpenContext ,LPCVOID pSourceBytes ,DWORD NumberOfBytes)
{	
	return -1;
}	
//***** jackl add end *******************	

