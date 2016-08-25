/******************************************************************************/
/* Module Description: DEBUG.H                                                */
/******************************************************************************/
#ifndef DEBUG_H
#define DEBUG_H

#define DEBCEINIT // add - 09/09/05,bolo for the initial debug messages shown on the console 

/******************************************************************************/
/* Debug macros                                                               */
/******************************************************************************/
#define VERBOSE                     0x03
#define INFORM                      0x02
#define FATAL                       0x01

#define DBGLEVEL                    INFORM
#define RXDBG                       0
#define TXDBG                       0

#ifdef NDISCE_MINIPORT
#define ADMDEBUG   DEBUG
#else
#define ADMDEBUG   DBG
#endif

#if ADMDEBUG //DBG
   #ifdef NDISCE_MINIPORT
      #define ADMTEXT                     TEXT
      #define DbgMessage(LEVEL, MESSAGE)  DEBUGMSG(TRUE, MESSAGE)
      #define DbgBreak()                  DebugBreak()
   #else
      #define ADMTEXT(MESSAGE)            MESSAGE
      #define DbgMessage(LEVEL, MESSAGE)  DbgPrint MESSAGE
      #define DbgBreak()                  _asm int 3
   #endif
   #define STATIC

#else /* DBG */
   #ifdef NDISCE_MINIPORT
      #define ADMTEXT                        TEXT
      #define DbgMessage(LEVEL, MESSAGE)     RETAILMSG(TRUE, MESSAGE)
      #define STATIC                         static
      #define DbgBreak()
   #else
      #define DbgMessage(LEVEL, MESSAGE)
      #define STATIC                         static
      #define DbgBreak()
   #endif

#endif /* DBG */

#endif /* DEBUG_H */

// add - 08/26/05,bolo
#define LAN_MEDIA_SELECT            1   // 0:AutoSense, 1:Ethernet, 2:HomeLAN, 3:MAC-MII
