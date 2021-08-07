/*
 * Sequences.h
 *
 *  Created on: 09 ���. 2015 �.
 *      Author: Kreyl
 */

#pragma once

#include "ChunkTypes.h"

#if 1 // ============================ LED RGB ==================================
#define LOW_BRTNESS     4
#define SHOWTIME        270
#define PAUSETIME       720
#define SHORTPAUSETIME  72

const LedRGBChunk_t lsqStart[] = {
        {csSetup, 180, clRed},
        {csSetup, 180, clBlack},
        {csSetup, 180, clGreen},
        {csSetup, 180, clBlack},
        {csSetup, 180, clBlue},
        {csSetup, 180, clBlack},
        {csSetup, 0, {1,0,0}},
        {csEnd},
//        {csGoto, 0},
};

const LedRGBChunk_t lsqHeartBeat[] = {
        {csSetup, 0, {18,0,0}},
        {csSetup, 540, {1,0,0}},
        {csEnd},
};

const LedRGBChunk_t lsqFailure[] = {
        {csSetup, 0, clRed},
        {csWait, 99},
        {csSetup, 0, clBlack},
        {csWait, 99},
        {csSetup, 0, clRed},
        {csWait, 99},
        {csSetup, 0, clBlack},
        {csWait, 99},
        {csSetup, 0, clRed},
        {csWait, 99},
        {csSetup, 0, clBlack},
        {csEnd}
};

#endif
