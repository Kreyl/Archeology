#include "board.h"
#include "kl_libG070.h"
#include "ch.h"
#include "hal.h"
#include "uartG070.h"
#include "color.h"
#include "LEDs.h"
#include "shell.h"
#include "MsgQ.h"
#include "radio_lvl1.h"
#include "Sequences.h"

#if 1 // ======================== Variables & prototypes =======================
EvtMsgQ_t<EvtMsg_t, MAIN_EVT_Q_LEN> EvtQMain;
void OnCmd(Shell_t *PShell);
static const UartParams_t CmdUartParams(115200, CMD_UART_PARAMS);
CmdUart_t Uart{&CmdUartParams};
void ClockInit();
void ITask();

static TmrKL_t TmrOneSecond {TIME_MS2I(999), evtIdEverySecond, tktPeriodic};
static uint32_t RxTimeout = 0;
static uint8_t OldChnl = 0xFF; // non valid value

static LedRGBChunk_t lsq[] = {
        {csSetup, 0, clBlack},
        {csSetup, 99, clBlue},
        {csEnd},
};

LedsAllSeq_t Leds;
#endif

int main(void) {
    ClockInit();
//     Start Watchdog. Will reset in main thread by periodic 1 sec events.
//    Iwdg::InitAndStart(4500);
//    Iwdg::DisableInDebug();

    // === Init OS ===
    halInit();
    chSysInit();
    EvtQMain.Init();

    // ==== Init hardware ====
    PinSetupOut(DBG_PIN, omPushPull);
    Uart.Init();
    Printf("\r%S %S\r", APP_NAME, XSTRINGIFY(BUILD_TIME));

    // Led Pwr
    PinSetupOut(LED_PWR_CTRL, omPushPull, psLow);
    PinSetHi(LED_PWR_CTRL);
    // Leds
    Leds.Init();
    Leds.StartOrRestart(lsqStart);

    // ==== Radio ====
    if(Radio.Init() == retvOk) Leds.StartOrRestart(lsqStart);
    else Leds.StartOrRestart(lsqFailure);
    chThdSleepMilliseconds(1008);

    TmrOneSecond.StartOrRestart();
    // Main cycle
    ITask();
}

__attribute__((__noreturn__))
void ITask() {
    while(true) {
        EvtMsg_t Msg = EvtQMain.Fetch(TIME_INFINITE);
        switch(Msg.ID) {
            case evtIdEverySecond:
                Iwdg::Reload();
                RxTimeout++;
                if(RxTimeout > 4 and !Leds.IsOff()) {
                    Leds.StartOrRestart(lsqFadeout);
                }
                break;

            case evtIdRadioCmd: {
                RxTimeout = 0;
                // Show if color has changed
                if(OldChnl != Msg.ValueID) {
                    OldChnl = Msg.ValueID;
                    Color_t Clr;
                    Clr.DWord32 = (uint32_t)Msg.Value;
                    lsq[1].Color = Clr;
                    Leds.StartOrRestart(lsq);
                }
            } break;

            case evtIdShellCmd:
                while(((CmdUart_t*)Msg.Ptr)->GetRcvdCmd() == retvOk) OnCmd((Shell_t*)((CmdUart_t*)Msg.Ptr));
                break;

            default: Printf("Unhandled Msg %u\r", Msg.ID); break;
        } // Switch
    } // while true
}

#if 1 // ================= Command processing ====================
void OnCmd(Shell_t *PShell) {
    Cmd_t *PCmd = &PShell->Cmd;
    __attribute__((unused)) int32_t dw32 = 0;  // May be unused in some configurations
//    Printf("%S%S\r", PCmd->IString, PCmd->Remainer? PCmd->Remainer : " empty");
    // Handle command
    if(PCmd->NameIs("Ping")) PShell->Ok();
    else if(PCmd->NameIs("Version")) PShell->Print("%S %S\r", APP_NAME, XSTRINGIFY(BUILD_TIME));


    else PShell->CmdUnknown();
}
#endif

void ClockInit() {
    uint32_t tmpreg;
    // Reset everything
    RCC->AHBRSTR = 0xFFFFFFFF;
    RCC->AHBRSTR = 0;
    (void)RCC->AHBRSTR;
    RCC->APBRSTR1 = 0xFFFFFFFF;
    RCC->APBRSTR1 = 0;
    (void)RCC->APBRSTR1;
    RCC->APBRSTR2 = 0xFFFFFFFF;
    RCC->APBRSTR2 = 0;
    (void)RCC->APBRSTR2;

    // Enable syscfg and pwr
    Rcc::EnableAPB2Clk(RCC_APBENR2_SYSCFGEN);
    Rcc::EnableAPB1Clk(RCC_APBENR1_PWREN);
    // Set Flash latency
    tmpreg = FLASH->ACR;
    tmpreg &= ~FLASH_ACR_LATENCY;
    tmpreg |= 0b010UL; // Flash latency: two wait states
    tmpreg |= FLASH_ACR_ICEN | FLASH_ACR_PRFTEN; // Enable instruction cache and prefetch
    FLASH->ACR = tmpreg;

    // === PLL ===
    // PLL dividers and src: R = /3, P = /2, N = 12, M = /1, PLLSRC = HSI = 16MHz
    RCC->PLLCFGR = (0b010UL << 29) | (0b00001UL << 17) | (12UL << 8) | (0b000UL << 4) | (0b10UL << 0);
    RCC->CR |= RCC_CR_PLLON; // Enable PLL
    RCC->PLLCFGR |= RCC_PLLCFGR_PLLREN; // Enable R output
    RCC->PLLCFGR |= RCC_PLLCFGR_PLLPEN; // Enable P output

    // I2S1 clock src is PLL P output
    RCC->CCIPR = (RCC->CCIPR & ~RCC_CCIPR_I2S1SEL) | (0b01UL << 14);

    // Wait until PLL started
    while(!(RCC->CR & RCC_CR_PLLRDY));
    // Switch to PLL
    tmpreg = RCC->CFGR & ~RCC_CFGR_SW;
    tmpreg |= 0b010UL; // PLLR is sys clk
    RCC->CFGR = tmpreg;
    // Wait until done
    while((RCC->CFGR & RCC_CFGR_SWS) != (0b010UL << 3));
    // Enable DMA clock
    Rcc::EnableAHBClk(RCC_AHBENR_DMA1EN);
    // ADC clock is SYSCLK: ADCSEL = 0
    RCC->CCIPR &= ~RCC_CCIPR_ADCSEL;
    AHBFreqHz = 64000000;
    APBFreqHz = 64000000;
}
