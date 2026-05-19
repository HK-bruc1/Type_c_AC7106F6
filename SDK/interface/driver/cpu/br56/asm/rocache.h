#ifndef __Q32DSP_DCACHE__
#define __Q32DSP_DCACHE__

//*********************************************************************************//
// Module name : rocache.h                                                         //
// Description : q32DSP dcache control head file                                   //
// By Designer : zequan_liu                                                        //
// Dat changed :                                                                   //
//*********************************************************************************//

//  ------  ------   ------
//  | c0 |  | c2 |   | pp |
//  ------  ------   ------
//    |_______|________|
//            |
//        ---------
//        |  L1i  |
//        ---------
//            |
//        ---------
//        | flash |
//        ---------

#define INCLUDE_DCU_RPT      0
#define INCLUDE_DCU_EMU      0

//------------------------------------------------------//
// dcache function
//------------------------------------------------------//
#define u32 unsigned int

void DcuEnable(void);
void DcuDisable(void);
void DcuInitial(void);
void DcuWaitIdle(void);
void DcuSetWayNum(u32 way);

void DcuFlushinvAll(void);
void DcuFlushinvRegion(u32 *beg, u32 len);            // note len!=0
void DcuUnlockAll(void);
void DcuUnlockRegion(u32 *beg, u32 len);              // note len!=0
void DcuPfetchRegion(u32 *beg, u32 len);              // note len!=0
void DcuLockRegion(u32 *beg, u32 len);                // note len!=0

void DcuReportEnable(void);
void DcuReportDisable(void);
void DcuReportPrintf(void);
void DcuReportClear(void);

void DcuEmuEnable(void);
void DcuEmuDisable(void);
void DcuEmuMessage(void);

//*********************************************************************************//
//                                                                                 //
//                               end of this module                                //
//                                                                                 //
//*********************************************************************************//
#undef u32

#endif

