#include "LEDs.h"
#include "board.h"
#include "stm32g0xx.h"
#include "kl_libG070.h"
#include "color.h"

#if 1 // ============================= Variables ===============================
static const Timer_t ITmr{LEDS_TIM}; // Latch generator

#define LEDS_TIM_SHIFT      4
#define LEDS_TIM_PULSE_LEN  1

// Counting on both fronts => x2 pulses
#define LEDS_TIM_TOP        ((LED_IC_CNT * 16UL) * 2UL - 1UL)

static void LedsDmaIrqHandler(void*p, uint32_t flags);

static const I2S_t DispSpi{LEDS_SPI};
const DMA_t DispDma{LEDS_DMA_TX_CHNL, LEDS_DMA_TX_REQID, LedsDmaIrqHandler};

// Ordinal DMA mode: irq
#define LEDS_DMA_MODE (\
        DMA_PRIORITY_VERYHIGH | \
        DMA_MSIZE_16_BIT | \
        DMA_PSIZE_16_BIT | \
        DMA_MEM_INC |       \
        DMA_DIR_MEM2PER |  \
        DMA_TCIE )

__attribute__((aligned))
static uint8_t BrtBuf[LED_CHNL_CNT] = { 0 };

uint8_t CurrentBrt = 255;
static volatile bool NewPicIsReady = false;
#endif

#if 1 // ========================= RawBuf ======================================
#define LED_RAWBUF_SZ      (TIMESLOT_TOP * LED_IC_CNT * 2) // Size in bytes
#define LED_RAWBUF_CNT16   (LED_RAWBUF_SZ / 2) // Count of Word16

struct RawBuf_t {
    uint16_t Buf1[LED_RAWBUF_CNT16], Buf2[LED_RAWBUF_CNT16], *ReadBuf;
    void Reset() {
        ReadBuf = Buf1;
        for(uint32_t i=0; i<LED_RAWBUF_CNT16; i++) {
            Buf1[i] = 0;
            Buf2[i] = 0;
        }
    }
    uint16_t* GetWriteBuf() { return (ReadBuf == Buf1)? Buf2 : Buf1; }
    void SwitchReadBuf() { ReadBuf = (ReadBuf == Buf1)? Buf2 : Buf1; }
} RawBuf;
#endif

void LedsDmaIrqHandler(void*p, uint32_t flags) {
//    DBG_HI();
    DispDma.Disable();
    if(NewPicIsReady) {
        RawBuf.SwitchReadBuf();
        NewPicIsReady = false;
//        DBG_LO();
    }
    // Prepare DMA
    DispDma.SetMemoryAddr(RawBuf.ReadBuf);
    DispDma.SetTransferDataCnt(LED_RAWBUF_CNT16);
    // Wait transmission to complete (4 words in FIFO after last DMA transaction)
    while(LEDS_SPI->SR & SPI_SR_FTLVL);
    while(LEDS_SPI->SR & SPI_SR_BSY);
    ITmr.SetCounter(0); // Reset counter to return it back to sync, in case it is lost somehow
    DispDma.Enable(); // Start transmission
//    DBG_LO();
}

namespace ILeds {

void SetBrightness(uint8_t Brt);

void Init() {
#if 1 // ==== SPI & DMA ====
    RawBuf.Reset();
    // SPI as I2S to allow 24MHz SPI
    PinSetupAlterFunc(LED_CLK, psHigh);
    PinSetupAlterFunc(LED_DATA, psHigh);
    DispSpi.SetupI2SMasterTx16Bit();
    DispSpi.SetPrescaler(2);
    DispSpi.EnableTxDma();
    // DMA
    DispDma.Init(&LEDS_SPI->DR, RawBuf.ReadBuf, LEDS_DMA_MODE, LED_RAWBUF_CNT16);
    DispDma.Enable();
#endif

#if 1 // ==== Latch generator ====
    ITmr.Init();
    // Input1 as clock input
    PinSetupAlterFunc(LATCH_TMR_IN, psVeryHigh);
    // CC1 channel is configured as input, IC1 is mapped on TI1
    ITmr.SetupInput1(0b01, Timer_t::pscDiv1, risefallBoth);
    ITmr.SelectSlaveMode(smExternal); // External clock mode 1
    ITmr.SetTriggerInput(tiTI1FP1);
    // Output3 is OE
    PinSetupAlterFunc(OUT_EN_PIN, psLow);
    ITmr.SetupOutput2(0b0110); // PWM
    ITmr.EnableCCOutput2();
    // Output4 as latch output
    PinSetupAlterFunc(LATCH_PIN, psLow);
    ITmr.SetTopValue(LEDS_TIM_TOP);
    // Combined PWM mode 2: OC4REFC is the logical AND between OC4REF and OC3REF
    ITmr.SetupOutput3(0b1100);
    ITmr.SetupOutput4(0b1101);
    ITmr.EnableCCOutput4();
    ITmr.SetCCR4(LEDS_TIM_TOP - LEDS_TIM_SHIFT); // When set Hi
    ITmr.SetCCR3(LEDS_TIM_TOP - LEDS_TIM_SHIFT + LEDS_TIM_PULSE_LEN); // When set Lo
    ITmr.Enable();
#endif
    DispSpi.Enable();
    SetBrightness(255);
}

void SetBrightness(uint8_t Brt) {
    CurrentBrt = Brt;
    ITmr.SetCCR2(4); // XXX
}

static uint32_t Real2NLedTbl[LED_CNT] = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9};

void ShowPic(Color_t *PClr) {
    NewPicIsReady = false;
//    DBG_HI();
    // Place pic to BrtBuf
    uint8_t *p = BrtBuf;
    for(uint32_t i=0; i<LED_CNT; i++) {
        uint32_t RealN = Real2NLedTbl[i];
        *p++ = PClr[RealN].B;
        *p++ = PClr[RealN].R;
        *p++ = PClr[RealN].G;
    }
    // Where to write to and read from
    uint16_t *pDst = RawBuf.GetWriteBuf() + (LED_IC_CNT - 1);
    uint16_t w;
    // Process other brightness values
    for(uint32_t brt=1; brt<=TIMESLOT_TOP; brt++) {
        uint8_t *PSrc = BrtBuf;
        for(uint32_t N=0; N<LED_IC_CNT; N++) { // Iterate ICs
            w = 0;
            if(*PSrc++ >= brt) w |= (1<< 0);
            if(*PSrc++ >= brt) w |= (1<< 1);
            if(*PSrc++ >= brt) w |= (1<< 2);
            if(*PSrc++ >= brt) w |= (1<< 3);
            if(*PSrc++ >= brt) w |= (1<< 4);
            if(*PSrc++ >= brt) w |= (1<< 5);
            if(*PSrc++ >= brt) w |= (1<< 6);
            if(*PSrc++ >= brt) w |= (1<< 7);
            if(*PSrc++ >= brt) w |= (1<< 8);
            if(*PSrc++ >= brt) w |= (1<< 9);
            if(*PSrc++ >= brt) w |= (1<<10);
            if(*PSrc++ >= brt) w |= (1<<11);
            if(*PSrc++ >= brt) w |= (1<<12);
            if(*PSrc++ >= brt) w |= (1<<13);
            if(*PSrc++ >= brt) w |= (1<<14);
            if(*PSrc++ >= brt) w |= (1<<15);
//            w ^= 0xFFFF;
            *pDst-- = w;
        } // for N
        pDst += (1 + LED_WORD_CNT + (LED_WORD_CNT - 1)); // Eliminate last --, move to next part, move to last element of next part
    } // brt
    // Say it is ready
    NewPicIsReady = true;
//    DBG_LO();
}

} // Namespace

void LedsAllSeq_t::Init() {
    ILeds::Init();
}

void LedsAllSeq_t::SetColor(Color_t AColor) {
    for(uint32_t i=0; i<LED_CNT; i++) IPic[i] = AColor;
    ILeds::ShowPic(IPic);
}
