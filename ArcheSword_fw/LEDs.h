#pragma once

#include <inttypes.h>
#include "color.h"
#include "ChunkTypes.h"
#include "board.h"

//namespace Leds {
//
//void Init();
//void SetBrightness(uint8_t Brt);
//
//// Buf of N pixels
//void ShowPic(Color_t *PClr);
//}

class LedsAllSeq_t : public BaseSequencer_t<LedRGBChunk_t> {
protected:
    Color_t ICurrColor;
    Color_t IPic[LED_CNT];
    void ISwitchOff() {
        SetColor(clBlack);
        ICurrColor = clBlack;
    }
    SequencerLoopTask_t ISetup() {
        if(ICurrColor != IPCurrentChunk->Color) {
            if(IPCurrentChunk->Value == 0) {     // If smooth time is zero,
                SetColor(IPCurrentChunk->Color); // set color now,
                ICurrColor = IPCurrentChunk->Color;
                IPCurrentChunk++;                // and goto next chunk
            }
            else {
                ICurrColor.Adjust(IPCurrentChunk->Color);
                SetColor(ICurrColor);
                // Check if completed now
                if(ICurrColor == IPCurrentChunk->Color) IPCurrentChunk++;
                else { // Not completed
                    // Calculate time to next adjustment
                    uint32_t Delay = ICurrColor.DelayToNextAdj(IPCurrentChunk->Color, IPCurrentChunk->Value);
                    SetupDelay(Delay);
                    return sltBreak;
                } // Not completed
            } // if time > 256
        } // if color is different
        else IPCurrentChunk++; // Color is the same, goto next chunk
        return sltProceed;
    }
public:
    LedsAllSeq_t() : BaseSequencer_t() {}
    bool IsOff() { return (ICurrColor == clBlack) and IsIdle(); }
    void Init();
    void SetColor(Color_t AColor);
};
