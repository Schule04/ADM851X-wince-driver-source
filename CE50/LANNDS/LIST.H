/******************************************************************************/
/* The information contained in this file is confidential and proprietary to  */
/* ZNK Corporation.  No part of this file may be reproduced or distributed,   */
/* in any form or by any means for any purpose, without the express written   */
/* permission of ZNK Corporation.                                             */
/*                                                                            */
/* (c) COPYRIGHT 1998 ZNK Corporation, ALL RIGHTS RESERVED.                   */
/*                                                                            */
/*                                                                            */
/* List Functions.                                                            */
/*      void       LL_InitList   (PLL_CONTAINER pList)                        */
/*      char       LL_Empty      (PLL_CONTAINER pList)                        */
/*      PLL_ENTRY  LL_GetHead    (PLL_CONTAINER pList)                        */
/*      PLL_ENTRY  LL_GetTail    (PLL_CONTAINER pList)                        */
/*      void       LL_PushHead   (PLL_CONTAINER pList, PLL_ENTRY pEntry)      */
/*      void       LL_PushTail   (PLL_CONTAINER pList, PLL_ENTRY pEntry)      */
/*      PLL_ENTRY  LL_PopHead    (PLL_CONTAINER pList)                        */
/*      PLL_ENTRY  LL_PopTail    (PLL_CONTAINER pList)                        */
/*      void       LL_RemoveEntry(PLL_ENTRY pEntry)                           */
/*      PLL_ENTRY  LL_NextEntry  (PLL_ENTRY pEntry)                           */
/*      PLL_ENTRY  LL_PrevEntry  (PLL_ENTRY pEntry)                           */
/*                                                                            */
/* History:                                                                   */
/*    12/03/98 Hav Khauv        Initial version.                              */
/******************************************************************************/
#ifndef LIST_H 
#define LIST_H

/* Doubly linked list structure used for linking entries together. */
typedef struct _LL_LIST_ENTRY {
    struct _LL_LIST_ENTRY *FLink;
    struct _LL_LIST_ENTRY *BLink;
} LL_LIST_ENTRY, *PLL_LIST_ENTRY;

/* List container */
typedef struct {
    LL_LIST_ENTRY   Link;
    unsigned long   EntryCount;
} LL_LIST_CONTAINER, *PLL_LIST_CONTAINER;

/******************************************************************************/
/* Description:                                                               */
/*    Initializes a list container.                                           */
/*                                                                            */
/* Returns:                                                                   */
/*    None.                                                                   */
/******************************************************************************/
__inline static void
LL_InitList(
PLL_LIST_CONTAINER pList) {
    pList->Link.FLink = pList->Link.BLink = (PLL_LIST_ENTRY) 0;
    pList->EntryCount = 0;
} /* LL_InitList */

/******************************************************************************/
/* Description:                                                               */
/*    Checks for an empty list.                                               */
/*                                                                            */
/* Returns:                                                                   */
/*    TRUE if the list container is empty.                                    */
/******************************************************************************/
__inline static char 
LL_Empty(
PLL_LIST_CONTAINER pList) {
    return(pList->Link.FLink == (PLL_LIST_ENTRY) 0);
} /* LL_Empty */

/******************************************************************************/
/* Description:                                                               */
/*    Retrieves the head entry without removing the entry from the list.      */
/*                                                                            */
/* Returns:                                                                   */
/*    Pointer to the head entry.                                              */
/******************************************************************************/
__inline static PLL_LIST_ENTRY 
LL_GetHead(
PLL_LIST_CONTAINER pList) {
    return pList->Link.FLink;
} /* LL_GetHead */

/******************************************************************************/
/* Description:                                                               */
/*    Retrieves the tail entry without removing it from the list.             */
/*                                                                            */
/* Returns:                                                                   */
/*    Pointer to the tail entry.                                              */
/******************************************************************************/
__inline static PLL_LIST_ENTRY 
LL_GetTail(
PLL_LIST_CONTAINER pList) {
    return pList->Link.BLink;
} /* LL_GetTail */

/******************************************************************************/
/* Description:                                                               */
/*    Adds an entry to the head of the list.                                  */
/*                                                                            */
/* Returns:                                                                   */
/*    None.                                                                   */
/******************************************************************************/
__inline static void
LL_PushHead(
PLL_LIST_CONTAINER pList, 
PLL_LIST_ENTRY pEntry) {
    pEntry->BLink = (PLL_LIST_ENTRY) 0;
    pEntry->FLink = pList->Link.FLink;

    if(pList->Link.FLink == (PLL_LIST_ENTRY) 0) {
        pList->Link.BLink = (PLL_LIST_ENTRY) pEntry;
    } else {
        pList->Link.FLink->BLink = pEntry;
    }

    pList->Link.FLink = (PLL_LIST_ENTRY) pEntry;

    pList->EntryCount++;
} /* LL_PushHead */

/******************************************************************************/
/* Description:                                                               */
/*    Adds an entry to the tail of the list.                                  */
/*                                                                            */
/* Returns:                                                                   */
/*    None.                                                                   */
/******************************************************************************/
__inline static void
LL_PushTail(
PLL_LIST_CONTAINER pList, 
PLL_LIST_ENTRY pEntry) {
    pEntry->BLink = pList->Link.BLink;

    if(pList->Link.BLink) {
        pList->Link.BLink->FLink = pEntry;
    } else {
        pList->Link.FLink = pEntry;
    }

    pList->Link.BLink = pEntry;
    pEntry->FLink = (PLL_LIST_ENTRY) 0;

    pList->EntryCount++;
} /* LL_PushTail */

/******************************************************************************/
/* Description:                                                               */
/*    Removes an entry from the head of the list.                             */
/*                                                                            */
/* Returns:                                                                   */
/*    The head entry.                                                         */
/******************************************************************************/
__inline static PLL_LIST_ENTRY 
LL_PopHead(
PLL_LIST_CONTAINER pList) {
    PLL_LIST_ENTRY pEntry;

    pEntry = pList->Link.FLink;
//DbgMessage(FATAL, (ADMTEXT("pEntry=%u\n"),pEntry));
    if(pEntry) {
        pList->Link.FLink = pEntry->FLink;
        if(pList->Link.FLink == (PLL_LIST_ENTRY) 0) {
            pList->Link.BLink = (PLL_LIST_ENTRY) 0;
        } else {
            pList->Link.FLink->BLink = (PLL_LIST_ENTRY) 0;
        }

        pList->EntryCount--;
    } /* if(pEntry) */
    
    return pEntry;
} /* LL_PopHead */

/******************************************************************************/
/* Description:                                                               */
/*    Removes an entry from the tail of the list.                             */
/*                                                                            */
/* Returns:                                                                   */
/*    Pointer to the tail entry.                                              */
/******************************************************************************/
__inline static PLL_LIST_ENTRY 
LL_PopTail(
PLL_LIST_CONTAINER pList) {
    PLL_LIST_ENTRY pEntry;

    pEntry = pList->Link.BLink;

    if(pEntry) {
        pList->Link.BLink = pEntry->BLink;
        if(pList->Link.BLink == (PLL_LIST_ENTRY) 0) {
            pList->Link.FLink = (PLL_LIST_ENTRY) 0;
        } else {
            pList->Link.BLink->FLink = (PLL_LIST_ENTRY) 0;
        }

        pList->EntryCount--;
    } /* if(pEntry) */
    
    return pEntry;
} /* LL_PopTail */

/******************************************************************************/
/* Description:                                                               */
/*    Advances to the next entry.                                             */
/*                                                                            */
/* Returns:                                                                   */
/*    Pointer to the next entry.                                              */
/******************************************************************************/
__inline static PLL_LIST_ENTRY
LL_NextEntry(
PLL_LIST_ENTRY pEntry) {
    return pEntry->FLink;
} /* LL_NextEntry */

/******************************************************************************/
/* Description:                                                               */
/*    Advances to the previous entry.                                         */
/*                                                                            */
/* Returns:                                                                   */
/*    Pointer to the previous entry.                                          */
/******************************************************************************/
__inline static PLL_LIST_ENTRY
LL_PrevEntry(
PLL_LIST_ENTRY pEntry) {
    return pEntry->BLink;
} /* LL_PrevEntry */

/******************************************************************************/
/* Description:                                                               */
/*    Gets the number of entries in the list.                                 */
/*                                                                            */
/* Returns:                                                                   */
/*    The number of entry in the list.                                        */
/******************************************************************************/
__inline static unsigned long 
LL_GetSize(
PLL_LIST_CONTAINER pList) {
    return pList->EntryCount;
} /* LL_GetSize */

/******************************************************************************/
/* Description:                                                               */
/*    Removes an entry from the list.                                         */
/*                                                                            */
/* Returns:                                                                   */
/*    None.                                                                   */
/******************************************************************************/
__inline static void
LL_RemoveEntry(
PLL_LIST_ENTRY pEntry) {
    if(pEntry->FLink) {
        pEntry->FLink->BLink = pEntry->BLink;
    } /* if */

    if(pEntry->BLink) {
        pEntry->BLink->FLink = pEntry->FLink;
    } /* if */
} /* LL_RemoveEntry */

#endif /* LIST_H */
