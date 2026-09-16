/*
 * phaseouts.c
 *
 *  Created on: Apr 22, 2020
 *      Author: Alka
 *      Modified by TempersLee June,21 2024
 */
#include "phaseouts.h"

#include "functions.h"
#include "targets.h"
#include "common.h"
#include "interrupt.h"

extern char prop_brake_active;
extern char armed;
extern volatile uint8_t coast_request;
extern volatile uint8_t coast_active;

// DShot is decoded in the DMA ISR on V203. A coast request must either arrive before
// a bridge transition (and make it a no-op) or after the complete transition (and
// open the bridge last). Keeping IRQs masked across each top-level energizing primitive
// closes the otherwise unsafe "ISR opens, interrupted writer resumes" window. ORBT
// also rejects every energizing primitive while disarmed; allOff remains unconditional.
static uint8_t beginBridgeDrive(uint32_t* interrupt_state)
{
    *interrupt_state = saveAndDisableInterrupts();
    if (coast_request || coast_active
#ifdef ORBT_ESC_V203
        || !armed
#endif
    ) {
        restoreInterrupts(*interrupt_state);
        return 0;
    }
    return 1;
}

static void endBridgeDrive(uint32_t interrupt_state)
{
    restoreInterrupts(interrupt_state);
}

#ifndef PWM_ENABLE_BRIDGE

#ifdef USE_INVERTED_LOW
#pragma message("using inverted low side output")
#define LOW_BITREG_ON BCR
#define LOW_BITREG_OFF BSHR
#else
#define LOW_BITREG_ON BSHR
#define LOW_BITREG_OFF BCR
#endif

#ifdef USE_INVERTED_HIGH
#pragma message("using inverted high side output")
#define HIGH_BITREG_OFF BSHR
#else
#define HIGH_BITREG_OFF BCR
#endif

/*
 * T1CH1---PA8       CH
 * T1CH2---PA9       BH
 * T1CH3---PA10      AH
 * T1CH1N--PA7       CL
 * T1CH2N--PB0       BL
 * T1CH3N--PB1       AL
 * */


void proportionalBrake()
{
    uint32_t interrupt_state;
    if (!beginBridgeDrive(&interrupt_state)) {
        return;
    }
    // turn all HIGH channels off for ABC
    PHASE_A_GPIO_PORT_HIGH->CFGHR &= ~(0xf<<8); PHASE_A_GPIO_PORT_HIGH->CFGHR |= (0x3<<8);
    PHASE_A_GPIO_PORT_HIGH->HIGH_BITREG_OFF = PHASE_A_GPIO_HIGH;

    PHASE_B_GPIO_PORT_HIGH->CFGHR &= ~(0xf<<4); PHASE_B_GPIO_PORT_HIGH->CFGHR |= (0x3<<4);
    PHASE_B_GPIO_PORT_HIGH->HIGH_BITREG_OFF = PHASE_B_GPIO_HIGH;

    PHASE_C_GPIO_PORT_HIGH->CFGHR &= ~(0xf<<0); PHASE_C_GPIO_PORT_HIGH->CFGHR |= (0x3<<0);
    PHASE_C_GPIO_PORT_HIGH->HIGH_BITREG_OFF = PHASE_C_GPIO_HIGH;


    // set low channel to PWM, duty cycle will now control braking
    PHASE_A_GPIO_PORT_LOW->CFGLR  &= ~(0xf<<4); PHASE_A_GPIO_PORT_LOW->CFGLR|= (0xb<<4);
    PHASE_B_GPIO_PORT_LOW->CFGLR  &= ~(0xf<<0); PHASE_B_GPIO_PORT_LOW->CFGLR|= (0xb<<0);
    PHASE_C_GPIO_PORT_LOW->CFGLR  &= ~(0xf<<28); PHASE_C_GPIO_PORT_LOW->CFGLR|= (0xb<<28);
    endBridgeDrive(interrupt_state);
}



static void phaseBPWM()
{
    if(!eepromBuffer.comp_pwm)
    {  // for future
        PHASE_B_GPIO_PORT_LOW->CFGLR  &= ~(0xf<<0); PHASE_B_GPIO_PORT_LOW->CFGLR|= (0x3<<0);
        PHASE_B_GPIO_PORT_LOW->LOW_BITREG_OFF = PHASE_B_GPIO_LOW; //low close
    }
    else
    {
        PHASE_B_GPIO_PORT_LOW->CFGLR  &= ~(0xf<<0); PHASE_B_GPIO_PORT_LOW->CFGLR|= (0xb<<0); //low pwm
    }
    PHASE_B_GPIO_PORT_HIGH->CFGHR &= ~(0xf<<4); PHASE_B_GPIO_PORT_HIGH->CFGHR |= (0xb<<4);   //high pwm

}
static void phaseBFLOAT()
{
    // Preload each GPIO output latch with the gate-OFF level before disconnecting
    // the timer alternate function. Otherwise a stale OUTDR bit can pulse a gate at
    // the mode switch.
    PHASE_B_GPIO_PORT_LOW->LOW_BITREG_OFF = PHASE_B_GPIO_LOW;
    PHASE_B_GPIO_PORT_LOW->CFGLR  &= ~(0xf<<0); PHASE_B_GPIO_PORT_LOW->CFGLR|= (0x3<<0);

    PHASE_B_GPIO_PORT_HIGH->HIGH_BITREG_OFF = PHASE_B_GPIO_HIGH;
    PHASE_B_GPIO_PORT_HIGH->CFGHR &= ~(0xf<<4); PHASE_B_GPIO_PORT_HIGH->CFGHR |= (0x3<<4);

}
static void phaseBLOW()
{
    // low mosfet on
    PHASE_B_GPIO_PORT_LOW->CFGLR  &= ~(0xf<<0); PHASE_B_GPIO_PORT_LOW->CFGLR|= (0x3<<0);
    PHASE_B_GPIO_PORT_LOW->LOW_BITREG_ON = PHASE_B_GPIO_LOW;

    // high close
    PHASE_B_GPIO_PORT_HIGH->CFGHR &= ~(0xf<<4); PHASE_B_GPIO_PORT_HIGH->CFGHR |= (0x3<<4);
    PHASE_B_GPIO_PORT_HIGH->HIGH_BITREG_OFF = PHASE_B_GPIO_HIGH;
}

//////////////////////////////PHASE
/// 2//////////////////////////////////////////////////

static void phaseCPWM()
{
    if (!eepromBuffer.comp_pwm)
    {
        PHASE_C_GPIO_PORT_LOW->CFGLR  &= ~(0xf<<28); PHASE_C_GPIO_PORT_LOW->CFGLR |= (0x3<<28);
        PHASE_C_GPIO_PORT_LOW->LOW_BITREG_OFF = PHASE_C_GPIO_LOW;
    }
    else
    {
        PHASE_C_GPIO_PORT_LOW->CFGLR  &= ~(0xf<<28); PHASE_C_GPIO_PORT_LOW->CFGLR |= (0xb<<28);
    }
    PHASE_C_GPIO_PORT_HIGH->CFGHR &= ~(0xf<<0); PHASE_C_GPIO_PORT_HIGH->CFGHR |= (0xb<<0);
}

static void phaseCFLOAT()
{
    // floating
    PHASE_C_GPIO_PORT_LOW->LOW_BITREG_OFF = PHASE_C_GPIO_LOW;
    PHASE_C_GPIO_PORT_LOW->CFGLR  &= ~(0xf<<28); PHASE_C_GPIO_PORT_LOW->CFGLR |= (0x3<<28);

    PHASE_C_GPIO_PORT_HIGH->HIGH_BITREG_OFF = PHASE_C_GPIO_HIGH;
    PHASE_C_GPIO_PORT_HIGH->CFGHR &= ~(0xf<<0); PHASE_C_GPIO_PORT_HIGH->CFGHR |= (0x3<<0);
}

static void phaseCLOW()
{
    PHASE_C_GPIO_PORT_LOW->CFGLR  &= ~(0xf<<28); PHASE_C_GPIO_PORT_LOW->CFGLR |= (0x3<<28);
    PHASE_C_GPIO_PORT_LOW->LOW_BITREG_ON = PHASE_C_GPIO_LOW; //low on

    PHASE_C_GPIO_PORT_HIGH->CFGHR &= ~(0xf<<0); PHASE_C_GPIO_PORT_HIGH->CFGHR |= (0x3<<0);
    PHASE_C_GPIO_PORT_HIGH->HIGH_BITREG_OFF = PHASE_C_GPIO_HIGH; //high close
}

///////////////////////////////////////////////PHASE 3
////////////////////////////////////////////////////

static void phaseAPWM()
{
    if (!eepromBuffer.comp_pwm)
    {
        PHASE_A_GPIO_PORT_LOW->CFGLR  &= ~(0xf<<4); PHASE_A_GPIO_PORT_LOW->CFGLR|= (0x3<<4);
        PHASE_A_GPIO_PORT_LOW->LOW_BITREG_OFF = PHASE_A_GPIO_LOW; //low close
    }
    else
    {
        PHASE_A_GPIO_PORT_LOW->CFGLR  &= ~(0xf<<4); PHASE_A_GPIO_PORT_LOW->CFGLR|= (0xb<<4);//low pwm
    }
    PHASE_A_GPIO_PORT_HIGH->CFGHR &= ~(0xf<<8); PHASE_A_GPIO_PORT_HIGH->CFGHR |= (0xb<<8); //high pwm
}

static void phaseAFLOAT()
{
    PHASE_A_GPIO_PORT_LOW->LOW_BITREG_OFF = PHASE_A_GPIO_LOW;
    PHASE_A_GPIO_PORT_LOW->CFGLR  &= ~(0xf<<4); PHASE_A_GPIO_PORT_LOW->CFGLR|= (0x3<<4);

    PHASE_A_GPIO_PORT_HIGH->HIGH_BITREG_OFF = PHASE_A_GPIO_HIGH;
    PHASE_A_GPIO_PORT_HIGH->CFGHR &= ~(0xf<<8); PHASE_A_GPIO_PORT_HIGH->CFGHR |= (0x3<<8);
}

static void phaseALOW()
{
    PHASE_A_GPIO_PORT_LOW->CFGLR  &= ~(0xf<<4); PHASE_A_GPIO_PORT_LOW->CFGLR|= (0x3<<4);
    PHASE_A_GPIO_PORT_LOW->LOW_BITREG_ON = PHASE_A_GPIO_LOW;   // low on

    PHASE_A_GPIO_PORT_HIGH->CFGHR &= ~(0xf<<8); PHASE_A_GPIO_PORT_HIGH->CFGHR |= (0x3<<8);
    PHASE_A_GPIO_PORT_HIGH->HIGH_BITREG_OFF = PHASE_A_GPIO_HIGH; //high close
}

#else

//////////////////////////////////PHASE 1//////////////////////
static void phaseBPWM()
{
    if (!eepromBuffer.comp_pwm)
    {
        // for future
        // gpio_mode_QUICK(PHASE_B_GPIO_PORT_LOW, GPIO_MODE_OUTPUT,
        // GPIO_PULL_NONE, PHASE_B_GPIO_LOW);
        // PHASE_B_GPIO_PORT_LOW->LOW_BITREG_OFF = PHASE_B_GPIO_LOW;
    }
    else
    {
        PHASE_B_GPIO_PORT_LOW->CFGLR  &= ~(0xf<<0); PHASE_B_GPIO_PORT_LOW->CFGLR|= (0x3<<0);
        PHASE_B_GPIO_PORT_LOW->BSHR = PHASE_B_GPIO_LOW; //low on
    }
    PHASE_B_GPIO_PORT_HIGH->CFGHR &= ~(0xf<<4); PHASE_B_GPIO_PORT_HIGH->CFGHR |= (0xb<<4);   //high pwm
}

static void phaseBFLOAT()
{
    PHASE_B_GPIO_PORT_LOW->LOW_BITREG_OFF = PHASE_B_GPIO_LOW;
    PHASE_B_GPIO_PORT_LOW->CFGLR  &= ~(0xf<<0); PHASE_B_GPIO_PORT_LOW->CFGLR|= (0x3<<0);

    PHASE_B_GPIO_PORT_HIGH->HIGH_BITREG_OFF = PHASE_B_GPIO_HIGH;
    PHASE_B_GPIO_PORT_HIGH->CFGHR &= ~(0xf<<4); PHASE_B_GPIO_PORT_HIGH->CFGHR |= (0x3<<4);
}

static void phaseBLOW()
{
    // low mosfet on
    PHASE_B_GPIO_PORT_LOW->CFGLR  &= ~(0xf<<0); PHASE_B_GPIO_PORT_LOW->CFGLR|= (0x3<<0);
    PHASE_B_GPIO_PORT_LOW->LOW_BITREG_ON = PHASE_B_GPIO_LOW;

    // high close
    PHASE_B_GPIO_PORT_HIGH->CFGHR &= ~(0xf<<4); PHASE_B_GPIO_PORT_HIGH->CFGHR |= (0x3<<4);
    PHASE_B_GPIO_PORT_HIGH->HIGH_BITREG_OFF = PHASE_B_GPIO_HIGH;
}

//////////////////////////////PHASE
/// 2//////////////////////////////////////////////////

static void phaseCPWM()
{
    if (!eepromBuffer.comp_pwm)
    {
        //	gpio_mode_QUICK(PHASE_C_GPIO_PORT_LOW, GPIO_MODE_OUTPUT,
        // GPIO_PULL_NONE,
        // PHASE_C_GPIO_LOW); PHASE_C_GPIO_PORT_LOW->LOW_BITREG_OFF =
        // PHASE_C_GPIO_LOW;
    }
    else
    {
        PHASE_C_GPIO_PORT_LOW->CFGLR  &= ~(0xf<<28); PHASE_C_GPIO_PORT_LOW->CFGLR |= (0x3<<28);
        PHASE_C_GPIO_PORT_LOW->BSHR = PHASE_C_GPIO_LOW;
    }
    PHASE_C_GPIO_PORT_HIGH->CFGHR &= ~(0xf<<0); PHASE_C_GPIO_PORT_HIGH->CFGHR |= (0xb<<0);
}

static void phaseCFLOAT()
{
    // floating
    PHASE_C_GPIO_PORT_LOW->LOW_BITREG_OFF = PHASE_C_GPIO_LOW;
    PHASE_C_GPIO_PORT_LOW->CFGLR  &= ~(0xf<<28); PHASE_C_GPIO_PORT_LOW->CFGLR |= (0x3<<28);

    PHASE_C_GPIO_PORT_HIGH->HIGH_BITREG_OFF = PHASE_C_GPIO_HIGH;
    PHASE_C_GPIO_PORT_HIGH->CFGHR &= ~(0xf<<0); PHASE_C_GPIO_PORT_HIGH->CFGHR |= (0x3<<0);
}

static void phaseCLOW()
{
    PHASE_C_GPIO_PORT_LOW->CFGLR  &= ~(0xf<<28); PHASE_C_GPIO_PORT_LOW->CFGLR |= (0x3<<28);
    PHASE_C_GPIO_PORT_LOW->LOW_BITREG_ON = PHASE_C_GPIO_LOW; //low on

    PHASE_C_GPIO_PORT_HIGH->CFGHR &= ~(0xf<<0); PHASE_C_GPIO_PORT_HIGH->CFGHR |= (0x3<<0);
    PHASE_C_GPIO_PORT_HIGH->HIGH_BITREG_OFF = PHASE_C_GPIO_HIGH; //high close
}

///////////////////////////////////////////////PHASE 3
////////////////////////////////////////////////////

static void phaseAPWM()
{
    if (!eepromBuffer.comp_pwm)
    {
        //	gpio_mode_QUICK(PHASE_A_GPIO_PORT_LOW, GPIO_MODE_OUTPUT,
        // GPIO_PULL_NONE,
        // PHASE_A_GPIO_LOW); PHASE_A_GPIO_PORT_LOW->LOW_BITREG_OFF =
        // PHASE_A_GPIO_LOW;
    }
    else
    {
        PHASE_A_GPIO_PORT_LOW->CFGLR  &= ~(0xf<<4); PHASE_A_GPIO_PORT_LOW->CFGLR|= (0x3<<4);
        PHASE_A_GPIO_PORT_LOW->BSHR = PHASE_A_GPIO_LOW; //low on
    }
    PHASE_A_GPIO_PORT_HIGH->CFGHR &= ~(0xf<<8); PHASE_A_GPIO_PORT_HIGH->CFGHR |= (0xb<<8); //high pwm
}

static void phaseAFLOAT()
{
    PHASE_A_GPIO_PORT_LOW->LOW_BITREG_OFF = PHASE_A_GPIO_LOW;
    PHASE_A_GPIO_PORT_LOW->CFGLR  &= ~(0xf<<4); PHASE_A_GPIO_PORT_LOW->CFGLR|= (0x3<<4);

    PHASE_A_GPIO_PORT_HIGH->HIGH_BITREG_OFF = PHASE_A_GPIO_HIGH;
    PHASE_A_GPIO_PORT_HIGH->CFGHR &= ~(0xf<<8); PHASE_A_GPIO_PORT_HIGH->CFGHR |= (0x3<<8);
}

static void phaseALOW()
{
    PHASE_A_GPIO_PORT_LOW->CFGLR  &= ~(0xf<<4); PHASE_A_GPIO_PORT_LOW->CFGLR|= (0x3<<4);
    PHASE_A_GPIO_PORT_LOW->LOW_BITREG_ON = PHASE_A_GPIO_LOW;   // low on

    PHASE_A_GPIO_PORT_HIGH->CFGHR &= ~(0xf<<8); PHASE_A_GPIO_PORT_HIGH->CFGHR |= (0x3<<8);
    PHASE_A_GPIO_PORT_HIGH->HIGH_BITREG_OFF = PHASE_A_GPIO_HIGH; //high close
}

#endif


void allOff()
{
    // PA7 (phase C low) shares GPIOA->CFGLR with the PA0 DShot pin. Keep the
    // three-phase OFF transition atomic so neither side can restore a stale register
    // snapshot after interrupting the other. Unlike drive primitives, OFF is never
    // rejected by the coast latch.
    uint32_t interrupt_state = saveAndDisableInterrupts();
    phaseAFLOAT();
    phaseBFLOAT();
    phaseCFLOAT();
    restoreInterrupts(interrupt_state);
}

void comStep(int newStep)
{
    uint32_t interrupt_state;
    if (!beginBridgeDrive(&interrupt_state)) {
        return;
    }
    switch (newStep) {
    case 1: // A-B
        phaseCFLOAT();
        phaseBLOW();
        phaseAPWM();
        break;

    case 2: // C-B
        phaseAFLOAT();
        phaseBLOW();
        phaseCPWM();
        break;

    case 3: // C-A
        phaseBFLOAT();
        phaseALOW();
        phaseCPWM();
        break;

    case 4: // B-A
        phaseCFLOAT();
        phaseALOW();
        phaseBPWM();
        break;

    case 5: // B-C
        phaseAFLOAT();
        phaseCLOW();
        phaseBPWM();
        break;

    case 6: // A-C
        phaseBFLOAT();
        phaseCLOW();
        phaseAPWM();
        break;
    }
    endBridgeDrive(interrupt_state);
}

void fullBrake()
{ // full braking shorting all low sides
    uint32_t interrupt_state;
    if (!beginBridgeDrive(&interrupt_state)) {
        return;
    }
    phaseALOW();
    phaseBLOW();
    phaseCLOW();
    endBridgeDrive(interrupt_state);
}

void allpwm()
{ // for stepper_sine
    uint32_t interrupt_state;
    if (!beginBridgeDrive(&interrupt_state)) {
        return;
    }
    phaseAPWM();
    phaseBPWM();
    phaseCPWM();
    endBridgeDrive(interrupt_state);
}

void twoChannelForward()
{
    uint32_t interrupt_state;
    if (!beginBridgeDrive(&interrupt_state)) {
        return;
    }
    phaseAPWM();
    phaseBLOW();
    phaseCPWM();
    endBridgeDrive(interrupt_state);
}

void twoChannelReverse()
{
    uint32_t interrupt_state;
    if (!beginBridgeDrive(&interrupt_state)) {
        return;
    }
    phaseALOW();
    phaseBPWM();
    phaseCLOW();
    endBridgeDrive(interrupt_state);
}
