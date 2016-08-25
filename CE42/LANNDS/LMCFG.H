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

#ifndef LMCFG_H
#define LMCFG_H



/******************************************************************************/
/* Configurable constants                                                     */
/******************************************************************************/

#define DRIVER_MAJOR_VERSION        1
#define DRIVER_MINOR_VERSION        00
#define DRIVER_VERSION_STRING       "1.00"
#define NDIS_VERSION_STR            "NDIS 5.0 Miniport Driver"
#define DRIVER_COMPANYNAME_STR      "Infineon Technologies AG"
#define DRIVER_FILE_NAME            "LANNDS"
#define DRIVER_PRODUCTNAME_STR      "Infineon 851X Series Adapters"
#define DRIVER_FILE_DESCRIPTION     DRIVER_PRODUCTNAME_STR NDIS_VERSION_STR
#define DRIVER_COPYRIGHT_STRING     "Copyright (C) Infineon Technologies AG. 2004-2005"

#define MAX_TX_PACKET_DESCRIPTORS           40
#define MAX_RX_PACKET_DESCRIPTORS           20 // mod - 06/03/05,05/26/05,bolo; orignal: 20
#define MIN_RX_PACKET_DESCRIPTORS           3

// #define NEW_VERSION              // There are two section should be marked 
									// for customer who use older driver (Like MSC)

#define SUPPORT_MEDIASENSE			// Enable this flag to support mediasense feature, add - 07/20/05,bolo for MSC

#endif /* LMCFG_H */

