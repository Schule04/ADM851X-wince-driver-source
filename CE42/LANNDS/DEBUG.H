/******************************************************************************/
/* Module Description: DEBUG.H                                                */
/******************************************************************************/
#ifndef DEBUG_H
#define DEBUG_H

//#define UM_DEBUG_PC		1
//#define MM_DEBUG_PC		1
//#define LM_DEBUG_PC		1
//#define USBM_DEBUG_PC 	1
//#define LANNDS_DUBG_PC	1
//#define TX_DEBUG_PC		1
//#define RX_DEBUG_PC		1

//#define MM_DEBUG_CE	1
//#define LM_DEBUG_CE	1
//#define LANNDS_DEBUG 	1

#define DEBCEINIT   // add - 09/09/05,bolo for the initial debug messages shown on the console

// Show the debug messages in LM.C, MM.C and UM.C; add - 09/09/05,bolo
// #define DEBCE    // mark - 06/09/05,06/01/05,bolo for the debug messages shown on the console 
// #define DEBTXERR // add - 07/27/05, bolo for the TX error debug messages shown on the console 
#define DEBERR      // unmark - 07/25/05,add - 06/22/05,bolo for the exception debug messages shown on the console 

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

