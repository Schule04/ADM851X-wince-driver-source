/******************************************************************************/
/* The information contained in this file is confidential and proprietary to  */
/* ZNK Corporation.  No part of this file may be reproduced or distributed,   */
/* in any form or by any means for any purpose, without the express written   */
/* permission of ZNK Corporation.                                             */
/*                                                                            */
/* (c) COPYRIGHT 1998 ZNK Corporation, ALL RIGHTS RESERVED.                   */
/*                                                                            */
/*                                                                            */
/* Module Description:                                                        */
/*                                                                            */
/*                                                                            */
/* History:                                                                   */
/*    12/03/98 Hav Khauv        Initial version.                              */
/******************************************************************************/

#ifndef LM_H
#define LM_H

#ifdef NDISCE_MINIPORT
#include <windows.h>
#include "usbdi.h"
#endif
#include "types.h"
#include "debug.h"
#include "list.h"
#include "lmcfg.h"
#include "lmstatus.h"
#include "bits.h"

/******************************************************************************/
/* Some defined constants for NWAY link ability                               */
/******************************************************************************/
// ++ add - 08/25/05,bolo
#define NWAY_COMMAND_REG                0
#define NWAY_STATUS_REG                 1
#define NWAY_ADVERTISE_REG              4
#define NWAY_LINKABL_REG                5
#define NWAY_SPEED_REG                  0x12
#define NWAY_CMD_VAL                    0x3000
#define NWAY_FD_OFF                     0x0ffbf
#define NWAY_AN_RESET                   0x8000
#define NWAY_SEL_100M                   0x2000
#define NWAY_AN_RESTART                 0x0200
#define NWAY_AN_OK                      0x0020
#define NWAY_LINK_OK                    4      // add - 06/03/1998
#define NWAY_AUTO_RESTART               0x3200 // add - 06/02/1998
#define NWAY_SEL_10M                    0x0000 // add - 06/02/1998
#define NWAY_10T_DOWN                   0x0800 // add - 06/02/1998
#define NWAY_AN_FD                      0x0100 // add - 06/02/1998
#define NWAY_LINKABL_SELECTOR           1      // add - 06/02/1998
#define NWAY_ALL_SPEED                  0x1e1  // add - 06/02/1998
#define NWAY_FORCE_100MbFD              0x101  // add - 06/02/1998
#define NWAY_FORCE_HD                   0xa1   // add - 06/02/1998
#define NWAY_FORCE_100Mb                0x81   // add - 06/02/1998
#define NWAY_FORCE_10MbFD               0x41   // add - 06/02/1998
#define NWAY_FORCE_10Mb                 0x21   // add - 06/02/1998
// --

//#define   WORDAROUND                    1
//#define   NS_HPNA                       1
#define   ETH_LENGTH_OF_ADDRESS         6
#define   AMD901                        0x6B93
#define   NS851A                        0x5C20
#define   NS851B                        0x5C21
#define   NS851C                        0x5C22

/******************************************************************************/
/* Some defined constants                                                     */
/******************************************************************************/
#define LM_DevID                        0x07a6
#define LM_VenID                        0x0986


#define LM_DEVICE_EXTENSION_INFO_SIZE           140

#define MIN_ETHERNET_PACKET_SIZE                60
#define MAX_ETHERNET_PACKET_SIZE                1514
#define MAX_PACKET_BUFFER_SIZE                  0x600
#define ETHERNET_PACKET_HEADER_SIZE             14
#define MAX_MULTICAST_ADDRESS                   64
#define MAX_WAKEUP_PATTERN                      3

#define RECEIVE_MASK_ACCEPT_MULTICAST           BIT_0
#define RECEIVE_MASK_ACCEPT_BROADCAST           BIT_1
#define RECEIVE_MASK_PROMISCUOUS_MODE           BIT_2
#define RECEIVE_MASK_ACCEPT_ERR_PACKETS         BIT_3
#define RECEIVE_MASK_ACCEPT_MULTI_PROM          BIT_4
#define RECEIVE_MASK_ACCEPT_ALL_MAC_FRAMES      BIT_5

/* requested media type*/
#define MEDIA_TYPE_AUTO_DETECTION           0x00
#define MEDIA_TYPE_UTP_10MBPS               0x01
#define MEDIA_TYPE_UTP_10MBPS_FULL_DUPLEX   0x02
#define MEDIA_TYPE_UTP_100MBPS              0x03
#define MEDIA_TYPE_UTP_100MBPS_FULL_DUPLEX  0x04

/* requested selected media*/
#define SELECT_MEDIA_AUTO_DETECTION         0x00
#define SELECT_MEDIA_ETHERNET               0x01
#define SELECT_MEDIA_HOMELAN                0x02
#define SELECT_MEDIA_MAC_MII                0x03 // add - 08/19/05,bolo

#define ETHERNET                            0x01
#define HPNA                                0x02
#define MACMII                              0x03 // add - 08/19/05,bolo
#define ETHER_GPIO                          0x26
#define HPNA_GPIO                           0x34
#define NONE_GPIO                           0x24
#define PII_ETHER                           0x02
#define PII_HPNA                            0x06
#define PII_MACMII                          0x06 // 0x06, add - 06/30/05,bolo for ASUS
#define MACMII_PHYID                        0x01 // add - 08/25/05,bolo

#define LINK_SPEED_UNKNOWN                      0
#define LINK_SPEED_10MBPS                       1
#define LINK_SPEED_100MBPS                      2

#define LINK_DUPLEX_UNKNOWN                     0
#define LINK_DUPLEX_HALF                        1
#define LINK_DUPLEX_FULL                        2

#define PIPE_TYPE_IN                            0
#define PIPE_TYPE_OUT                           1

#define DEVICE_STATE_D0                         0
#define DEVICE_STATE_D1                         1
#define DEVICE_STATE_D2                         2
#define DEVICE_STATE_D3                         3

//Ethernet Control 0 Reg.
/* 0x0 */
#define Tx_en                                   BIT_7
#define Rx_en                                   BIT_6
#define Rx_Flowctl_en                           BIT_5
#define Wakeon_en                               BIT_4
#define Rxstatus_append                         BIT_3
#define Stop_back_off                           BIT_2
#define rx_multicast_all                        BIT_1
#define rx_crc_sent                             BIT_0

//Ethernet Control 1 Reg.
/* 0x1 */
#define DELAY_HOME                              BIT_6
#define Full_Duplex                             BIT_5
#define _100Mode                                BIT_4  //0:10M  1:100M
#define Reset_Mac                               BIT_3
#define Home_Lan                                BIT_2  //1:Mome LAN
// 0x02
#define Promiscuous                             BIT_2


//ACPI
/* 0x78 */
#define MGCPKT_EN                               BIT_7
#define LINK_EN                                 BIT_6
#define WAKEUPFRAME0_EN                         BIT_5
#define WAKEUPFRAME1_EN                         BIT_4
#define WAKEUPFRAME2_EN                         BIT_3
#define CRC16TYPE                               BIT_2

/* 0x7A */

#define RX_MGCPKT                               BIT_7
#define LINK_WAKE                               BIT_6
#define RX_WAKEUPFRAME                          BIT_5
#define LINK_STS								 BIT_0
/******************************************************************************/
/* Default CRC POLY                                                           */
/******************************************************************************/
#define POLY                                    0x04C11DB6L

/******************************************************************************/
/* Packet descriptor.                                                         */
/******************************************************************************/

struct _LM_DEVICE_INFO_BLOCK;

typedef struct
{
    LL_LIST_ENTRY                    Link;

    /* OS-Specific packet buffer structure */
    PVOID                            pUmPacket;

    struct _LM_DEVICE_INFO_BLOCK *   pDevice;
    LM_STATUS                        PacketStatus;
    DWORD                            PacketSize;
#ifndef NDISCE_MINIPORT
    PVOID                            pUrb;
#else
    USB_TRANSFER                     hTransfer;
#endif
    BYTE *                           Buffer;
} LM_PACKET_DESC, *PLM_PACKET_DESC;



/******************************************************************************/
/* Device structure.                                                          */
/******************************************************************************/

typedef struct _LM_DEVICE_INFO_BLOCK {
    LL_LIST_ENTRY Link;

    /* Configurable parameters. */
    DWORD MaxTxPacketDesc;
    DWORD MaxRxPacketDesc;
    DWORD RequestedMediaType;
    DWORD RequestedSelectMedia;
    DWORD RequestedMagicPacket;
    DWORD RequestedLinkWakeup;
    DWORD RequestedFlowControl;
    DWORD RequestedFlowControlTx;
    DWORD RequestedFlowControlRx;
    /* Free packet descriptors. */
    LL_LIST_CONTAINER TxPacketFreeList;
    LL_LIST_CONTAINER TxPacketReadyList;
    LL_LIST_CONTAINER TxPacketActiveList;
    LL_LIST_CONTAINER TxPacketTransmittedList;
    /* Free packet descriptors. */
    LL_LIST_CONTAINER RxPacketFreeList;
    LL_LIST_CONTAINER RxPacketReceivedList;

    /* Counters */
    DWORD RxPacketCount;
    DWORD RxFragListErrors;
    DWORD RxLostPackets;
    DWORD RxCrcErrors;
    DWORD RxAlignErrors;
    DWORD RxInvalidSizeErrors;

    DWORD TxPacketCount;
    DWORD TxPacketErrors;
    DWORD TxUnderrunCount;
    DWORD TxCollisionCount;
    DWORD TxDefferedCount;

    /* Miscellaneous info */
    BYTE NodeAddress[6];
    DWORD LinkStatus;
    DWORD ReceiveMask;
    DWORD LinkSpeed;
    DWORD LinkDuplex;
    DWORD PowerStatus;
    BYTE  PhyAddr;
    BYTE  HomePhy;       //0: AMD AM79C901  1:NS DP83851A 2:NS DP83851B
    /* Adapter status */
    LM_STATUS AdapterStatus;

    /* Adapter Info */
    BYTE PermanentNodeAddress[8];
    WORD VendorId;
    WORD DeviceId;

    /* This is really LM_DEVICE_EXTENSION_INFO. */
    BYTE pLmDeviceExtension[LM_DEVICE_EXTENSION_INFO_SIZE];
    BYTE FixFlag;
    BYTE Ctrl0Data;
    BYTE Ctrl1Data;
    BYTE FlowCtrlRx;
    BYTE FlowCtrlTx;
    BYTE Testpin;



        // for asyn get-set register
        PVOID pUrb;
        PVOID pUrbGet;

        // for running time detect media
        WORD  PartnerMedia;
        WORD  WorkState;
        BYTE  SetState;
        BYTE  GetState;
        BYTE  OrgMode;
    BYTE  CurrMode;
    BYTE  OldCtrl1Data;
    BYTE  EthernetDisconnect;
         /**************************************************************************/
    /* List of Multicast Address                                              */
    /**************************************************************************/
     BYTE PrivateMulticastBuffer[MAX_MULTICAST_ADDRESS][ETH_LENGTH_OF_ADDRESS];
     WORD NumberOfMcAddress;


        // for wakeup event
        union {
                BYTE PrivateWakeupBuffer[168];
                struct {
                        DWORD   rev0;
                        DWORD   rev1;
                        DWORD   MaskCount;
                        DWORD   Offset;
                        DWORD   Length;
                }WakeupStru;
        };

        BYTE WakeCtrlReg;
        BYTE WakeupMask[3][16];
        BYTE WakeupOffset[3][2];
#if WORDAROUND
        // for workaround
        BYTE    NeedCheckDA;
        WORD    Reset1;
        WORD    Reset2;
        BYTE    NeedReset;
#endif

		BYTE	ChipType;
#ifdef NDISCE_MINIPORT
        DWORD   TxIssueCount;
        DWORD   RxIssueCount;
        DWORD   TxCompletedCount;
        DWORD   RxCompletedCount;
#endif
//		BYTE	ResetAdpterFlag;	// v1.09 jack add 2003_0411 for power off and suspend

} LM_DEVICE_INFO_BLOCK, *PLM_DEVICE_INFO_BLOCK;



/******************************************************************************/
/* Function prototypes. */
/******************************************************************************/

LM_STATUS LM_SendPacket(PLM_DEVICE_INFO_BLOCK pDevice, PLM_PACKET_DESC pPacket);
LM_STATUS LM_InitializeAdapter(PLM_DEVICE_INFO_BLOCK pDevice);
LM_STATUS LM_ReInitializeAdapter( PLM_DEVICE_INFO_BLOCK pDevice ); // ~RV
LM_STATUS LM_Shutdown(PLM_DEVICE_INFO_BLOCK pDevice);
LM_STATUS LM_ResetAdapter(PLM_DEVICE_INFO_BLOCK pDevice);
LM_STATUS LM_SetReceiveMask(PLM_DEVICE_INFO_BLOCK pDevice, DWORD ReceiveMask);
LM_STATUS LM_QueueRxPacket(PLM_DEVICE_INFO_BLOCK pDevice,
    PLM_PACKET_DESC pPacket);

VOID LM_CheckMedia(PLM_DEVICE_INFO_BLOCK pDevice);
VOID LM_RemoveWakeupPattern(PLM_DEVICE_INFO_BLOCK pDevice);
VOID LM_AddWakeupPattern(PLM_DEVICE_INFO_BLOCK pDevice);
// lic add it for 770ED
//VOID HookAddDevice(PDRIVER_OBJECT DriverObject);
LM_STATUS
LM_SetRegisterAsyn(IN PLM_DEVICE_INFO_BLOCK pDevice, IN USHORT  Count,
    IN USHORT  Index, IN UCHAR *pOutRegisterdata);
LM_STATUS
LM_GetRegisterAsyn(IN PLM_DEVICE_INFO_BLOCK pDevice, IN USHORT  Count,
    IN USHORT  Index);
LM_STATUS LM_AbortPipe( PLM_DEVICE_INFO_BLOCK pDevice);
LM_STATUS LM_ChangeMcAddress(IN PLM_DEVICE_INFO_BLOCK pDevice,IN WORD AddressCount);
LM_STATUS
Wait_SetState(IN PLM_DEVICE_INFO_BLOCK pDevice, IN WORD waittime);
LM_STATUS LM_ResetMACAsyn(IN PLM_DEVICE_INFO_BLOCK pDevice);
VOID LM_ClearWakeUpStatus(IN PLM_DEVICE_INFO_BLOCK pDevice);
VOID LM_ReleaseResources(PLM_DEVICE_INFO_BLOCK pDevice);
USHORT LM_GetLinkStatus(PLM_DEVICE_INFO_BLOCK pDevice);
#endif /* LM_H */
