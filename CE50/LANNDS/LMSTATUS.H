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

#ifndef STATUS_H
#define STATUS_H



/******************************************************************************/
/* General status codes.                                                      */
/******************************************************************************/

typedef enum {
    LM_STATUS_UNKNOWN                   = 0x0000,
    LM_STATUS_SUCCESS                   = 0x0001,
    LM_STATUS_FAILURE                   = 0x0002,
    LM_STATUS_INTERRUPT_ACTIVE          = 0x0003,
    LM_STATUS_INTERRUPT_NOT_ACTIVE      = 0x0004,
    LM_STATUS_LINK_ACTIVE               = 0x0005,
    LM_STATUS_LINK_DOWN                 = 0x0006,
    LM_STATUS_TOO_MANY_FRAGMENTS        = 0x0007,
    LM_STATUS_TRANSMIT_ABORTED          = 0x0008,
    LM_STATUS_TRANSMIT_ERROR            = 0x0009,
    LM_STATUS_RECEIVE_ERROR             = 0x000a,
    LM_STATUS_INVALID_PACKET_SIZE       = 0x000b,
    LM_STATUS_ADAPTER_OPEN              = 0x000c,
    LM_STATUS_ADAPTER_INITIALIZED       = 0x000d,
    LM_STATUS_ADAPTER_CLOSED            = 0x000e,
    LM_STATUS_ADAPTER_FAILED            = 0x000f,
    LM_STATUS_ADAPTER_NOT_INITIALIZED   = 0x0010,
    LM_STATUS_ADAPTER_IO_CONFLICT       = 0x0011,
    LM_STATUS_ADAPTER_REMOVED           = 0x0012,
    LM_STATUS_ADAPTER_INSERTED          = 0x0013,
    LM_STATUS_OUT_OF_RESOURCE           = 0x0014,
    LM_STATUS_PENDING                   = 0x0015,
} LM_STATUS, *PLM_STATUS;



#endif // STATUS_H

