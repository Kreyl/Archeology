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

#define SMOOTH          90

const LedRGBChunk_t lsqStart[] = {
        {csSetup, SMOOTH, clRed},
        {csSetup, SMOOTH, clBlack},
        {csSetup, SMOOTH, clGreen},
        {csSetup, SMOOTH, clBlack},
        {csSetup, SMOOTH, clBlue},
        {csSetup, SMOOTH, clBlack},
//        {csSetup, 0, {0,1,1}},
        {csEnd},
//        {csGoto, 0},
};

const LedRGBChunk_t lsqHeartBeat[] = {
        {csSetup, 0, {0,18,18}},
        {csSetup, 540, {0,1,1}},
        {csEnd},
};

const LedRGBChunk_t lsqFadeout[] = {
        {csSetup, 540, clBlack},
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
