/*
 * dshot.c
 *
 *  Created on: Apr. 22, 2020
 *      Author: Alka
 */

#include "dshot.h"
#include "IO.h"
#include "common.h"
#include "functions.h"
#include "sounds.h"
#include "targets.h"
#if DRONECAN_SUPPORT
#include "DroneCAN/DroneCAN.h"
#endif

int dpulse[16] = { 0 };

const char gcr_encode_table[16] = {
    0b11001, 0b11011, 0b10010, 0b10011, 0b11101, 0b10101, 0b10110, 0b10111,
    0b11010, 0b01001, 0b01010, 0b01011, 0b11110, 0b01101, 0b01110, 0b01111
};

char EDT_ARM_ENABLE = 0;
char EDT_ARMED = 0;
int shift_amount = 0;
uint32_t gcrnumber;
extern int zero_crosses;
extern char send_telemetry;
extern uint8_t max_duty_cycle_change;
int dshot_full_number;
extern char play_tone_flag;
extern char send_esc_info_flag;
uint8_t command_count = 0;
uint8_t last_command = 0;
volatile uint8_t coast_release_count = 0;
volatile uint8_t dshot_arm_zero_count = 0;
uint8_t high_pin_count = 0;
uint32_t gcr[37] = { 0 };
uint16_t dshot_frametime;
uint16_t dshot_goodcounts;
uint16_t dshot_badcounts;
char dshot_extended_telemetry = 0;
uint16_t send_extended_dshot = 0;
uint16_t processtime = 0;
uint16_t halfpulsetime = 0;

uint8_t programming_mode;
uint16_t position;
uint8_t  new_byte;

void rejectDshotCapture()
{
    // A DMA transfer error is not a DShot frame. In particular, never let a stale
    // command-16 buffer advance or complete the release authentication sequence.
    dshotcommand = 0;
    command_count = 0;
    coast_release_count = 0;
    coast_release_request = 0;
    dshot_arm_zero_count = 0;
    compute_dshot_flag = 0;
    programming_mode = 0;
}

void computeDshotDMA()
{
    dshot_frametime = dma_buffer[31] - dma_buffer[0];
    halfpulsetime = dshot_frametime >> 5;
    if ((dshot_frametime > dshot_frametime_low) && (dshot_frametime < dshot_frametime_high)) {
        for (int i = 0; i < 16; i++) {
            // note that dma_buffer[] is uint32_t, we cast the difference to uint16_t to handle
            // timer wrap correctly
            const uint16_t pdiff = dma_buffer[(i << 1) + 1] - dma_buffer[(i << 1)];
            dpulse[i] = (pdiff > halfpulsetime);
        }
        uint8_t calcCRC = ((dpulse[0] ^ dpulse[4] ^ dpulse[8]) << 3 | (dpulse[1] ^ dpulse[5] ^ dpulse[9]) << 2 | (dpulse[2] ^ dpulse[6] ^ dpulse[10]) << 1 | (dpulse[3] ^ dpulse[7] ^ dpulse[11]));
        uint8_t checkCRC = (dpulse[12] << 3 | dpulse[13] << 2 | dpulse[14] << 1 | dpulse[15]);

        if (!armed) {
            if (dshot_telemetry == 0) {
                if (getInputPinState()) { // if the pin is high for 100 checks between
                                          // signal pulses its inverted
                    high_pin_count++;
                    if (high_pin_count > 100) {
                        dshot_telemetry = 1;
                    }
                }
            }
        }
        if (dshot_telemetry) {
            checkCRC = ~checkCRC + 16;
        }

        int tocheck = (dpulse[0] << 10 | dpulse[1] << 9 | dpulse[2] << 8 | dpulse[3] << 7 | dpulse[4] << 6 | dpulse[5] << 5 | dpulse[6] << 4 | dpulse[7] << 3 | dpulse[8] << 2 | dpulse[9] << 1 | dpulse[10]);

        if (calcCRC == checkCRC) {
            signaltimeout = 0;
            dshot_goodcounts++;
            if (dpulse[11] == 1) {
                send_telemetry = 1;
            }
            // Arming must be backed by actual valid wire-zero frames. Command frames also
            // map to adjusted_input == 0, so the legacy zero_input_count alone is ambiguous.
            // Frames ignored by a coast latch never contribute to a later re-arm.
            if (!coast_request && (tocheck == 0)) {
                if (dshot_arm_zero_count < 31) {
                    dshot_arm_zero_count++;
                }
            } else {
                dshot_arm_zero_count = 0;
            }
            if (!armed) {
                // Both normal and bidirectional DShot must qualify arming here.
                // The telemetry branches in transfercomplete() return before its
                // servo zero counter; after release that counter would stay zero.
                zero_input_count = dshot_arm_zero_count;
            }
            if (coast_request) {
                // Coast is a safety latch. Normal zero/throttle/command traffic must not be
                // able to re-energize a windmilling motor. Only six valid release frames are
                // accepted; everything else is ignored and breaks the release run.
                newinput = 0;
                dshotcommand = 0;
                command_count = 0;
                if (tocheck == DSHOT_CMD_COAST_RELEASE) {
                    if (coast_release_count < 6) {
                        coast_release_count++;
                    }
                    if (coast_release_count >= 6) {
                        coast_release_count = 0;
                        coast_release_request = 1;
                        last_dshot_command = DSHOT_CMD_COAST_RELEASE;
                    }
                } else {
                    coast_release_count = 0;
                    if (tocheck == DSHOT_CMD_COAST) {
                        // A renewed coast overrides a release that main has not
                        // committed yet, even if all six 16s were already decoded.
                        coast_release_request = 0;
                    }
                }
                return;
            }

            if (programming_mode > 0) {
                // Coast is a safety override even if command 36 left the decoder in
                // EEPROM programming mode. The first 15 aborts programming and then
                // participates in the normal six-frame coast confirmation. Release is
                // still a complete no-op when no coast latch owns it.
                if (tocheck == DSHOT_CMD_COAST) {
                    programming_mode = 0;
                } else if (tocheck == DSHOT_CMD_COAST_RELEASE) {
                    return;
                } else {
                    if (programming_mode == 1) { // begin programming mode
                        position = tocheck; // eepromBuffer position
                        programming_mode = 2;
                        return;
                    }
                    if (programming_mode == 2) {
                        new_byte = tocheck; // new value of setting
                        programming_mode = 3;
                        return;
                    }
                    if (programming_mode == 3) {
                        if (tocheck == 37) { // commit; command 12 saves it permanently
                            eepromBuffer.buffer[position] = new_byte;
                            programming_mode = 0;
                        }
                    }
                    return; // do not process ordinary DShot while programming
                }
            }

            if (tocheck > 47) {
                // any throttle-range frame breaks a command run (also while EDT is not armed yet,
                // otherwise one command frame plus five throttle frames would dispatch it)
                dshotcommand = 0;
                command_count = 0;
                if (EDT_ARMED) {
                    // BLHeli/Bluejay controllers commonly use wire value 48 as their armed
                    // 1D stop value. Preserve raw 0 as the only arming value, and preserve
                    // AM32's intentional value-48 neutral semantics in bidirectional mode.
                    if ((tocheck == 48) && !eepromBuffer.bi_direction && armed) {
                        newinput = 0;
                    } else {
                        newinput = tocheck;
                    }
                }
                return;
            }

            if ((tocheck <= 47) && (tocheck > 0)) {
                if (tocheck != last_command) { // a different command breaks the consecutive run, dispatched or not
                    last_command = tocheck;
                    command_count = 0;
                }
#if !defined(BRUSHED_MODE) && !defined(GIMBAL_MODE)
                if ((tocheck == DSHOT_CMD_COAST) ||
                    (tocheck == DSHOT_CMD_COAST_RELEASE)) {
                    // Both custom state transitions are non-braking. COAST holds the prior
                    // input until its sixth confirmation, including during sine startup.
                    // RELEASE is a complete no-op unless the held-coast decoder above owns it.
                } else {
                    newinput = 0;
                }
#else
                if (tocheck != DSHOT_CMD_COAST_RELEASE) {
                    newinput = 0;
                }
#endif
                dshotcommand = tocheck;
            }
            if (tocheck == 0) {
                if (EDT_ARM_ENABLE == 1) {
                    EDT_ARMED = 0;
                }
                dshotcommand = 0; // a zero frame breaks a command run even when DroneCAN owns the throttle
                command_count = 0;
#if DRONECAN_SUPPORT
                if (DroneCAN_active()) {
                    // allow DroneCAN to override DShot input
                    return;
                }
#endif
                newinput = 0;
            }

#if !defined(BRUSHED_MODE) && !defined(GIMBAL_MODE)
#if DRONECAN_SUPPORT
            const uint8_t coast_cmd = (dshotcommand == DSHOT_CMD_COAST) && !DroneCAN_active();
#else
            const uint8_t coast_cmd = (dshotcommand == DSHOT_CMD_COAST);
#endif
#else
            const uint8_t coast_cmd = 0;
#endif
            const uint8_t normal_cmd = (dshotcommand != DSHOT_CMD_COAST) &&
                (dshotcommand != DSHOT_CMD_COAST_RELEASE);
            // Normal commands are only dispatched while armed and stopped. Coast is the
            // safety exception: it is accepted while running and while disarmed, including
            // during reset recovery before the ordinary arming sequence has completed.
            if ((dshotcommand > 0) &&
                (coast_cmd || (armed && normal_cmd && (running == 0)))) {
                if (dshotcommand < 5) { // beacons
                    command_count = 6; // go on right away
                }
                command_count++;
                if (command_count >= 6) {
                    command_count = 0;
                    switch (dshotcommand) { // todo

                    case 1:
                        play_tone_flag = 1;
                        break;
                    case 2:
                        play_tone_flag = 2;
                        break;
                    case 3:
                        play_tone_flag = 3;
                        break;
                    case 4:
                        play_tone_flag = 4;
                        break;
                    case 5:
                        play_tone_flag = 5;
                        break;
                    case 6:
                        send_esc_info_flag = 1;
                        break;
                    case 7:
                        eepromBuffer.dir_reversed = 0;
                        forward = 1 - eepromBuffer.dir_reversed;
                        //	play_tone_flag = 1;
                        break;
                    case 8:
                        eepromBuffer.dir_reversed = 1;
                        forward = 1 - eepromBuffer.dir_reversed;
                        //	play_tone_flag = 2;
                        break;
                    case 9:
                        eepromBuffer.bi_direction = 0;
                        break;
                    case 10:
                        eepromBuffer.bi_direction = 1;
                        break;
                    case 12:
                        saveEEpromSettings();
                        play_tone_flag = 1 + eepromBuffer.dir_reversed;
                        //	NVIC_SystemReset();
                        break;
                    case 13:
                        dshot_extended_telemetry = 1;
                        send_extended_dshot = 0b111000000000;
                        if (EDT_ARM_ENABLE == 1) {
                            EDT_ARMED = 1;
                        }
                        break;
                    case 14:
                        dshot_extended_telemetry = 0;
                        send_extended_dshot = 0b111011111111;
                        //	make_dshot_package();
                        break;
                    case 20:
                        forward = 1 - eepromBuffer.dir_reversed;
                        break;
                    case 21:
                        forward = eepromBuffer.dir_reversed;
                        break;
#if !defined(BRUSHED_MODE) && !defined(GIMBAL_MODE)
                    case DSHOT_CMD_COAST:
                        coast_release_count = 0;
                        coast_release_request = 0;
                        // Ordinary DShot decode runs in the V203 DMA ISR. Set the latch and
                        // open the bridge immediately, but defer the larger controller-state
                        // transaction to main so interrupted main-loop state cannot overwrite it.
                        requestCoast();
                        break;
#endif
                    case 36:
                        programming_mode = 1;
              //          armed = 0;           // disarm when entering programming mode
                        break;
                    }
                    last_dshot_command = dshotcommand;
                    dshotcommand = 0;
                }
            }
        } else {
            dshot_badcounts++;
            programming_mode = 0;
        }
    }
}

void make_dshot_package(uint16_t com_time)
{
    if (send_extended_dshot > 0) {
        dshot_full_number = send_extended_dshot;
        send_extended_dshot = 0;
    } else {
        if (!running) {
            com_time = 65535;
        }
        //	calculate shift amount for data in format eee mmm mmm mmm, first 1 found
        // in first seven bits of data determines shift amount
        // this allows for a range of up to 65408 microseconds which would be
        // shifted 0b111 (eee) or 7 times.
        for (int i = 15; i >= 9; i--) {
            if (com_time >> i == 1) {
                shift_amount = i + 1 - 9;
                break;
            } else {
                shift_amount = 0;
            }
        }
        // shift the commutation time to allow for expanded range and put shift
        // amount in first three bits
        dshot_full_number = ((shift_amount << 9) | (com_time >> shift_amount));
    }
    // calculate checksum
    uint16_t csum = 0;
    uint16_t csum_data = dshot_full_number;
    for (int i = 0; i < 3; i++) {
        csum ^= csum_data; // xor data by nibbles
        csum_data >>= 4;
    }
    csum = ~csum; // invert it
    csum &= 0xf;

    dshot_full_number = (dshot_full_number << 4) | csum; // put checksum at the end of 12 bit dshot number

    // GCR RLL encode 16 to 20 bit

    gcrnumber = gcr_encode_table[(dshot_full_number >> 12)]
            << 15 // first set of four digits
        | gcr_encode_table[(((1 << 4) - 1) & (dshot_full_number >> 8))]
            << 10 // 2nd set of 4 digits
        | gcr_encode_table[(((1 << 4) - 1) & (dshot_full_number >> 4))]
            << 5 // 3rd set of four digits
        | gcr_encode_table[(((1 << 4) - 1) & (dshot_full_number >> 0))]; // last four digits
// GCR RLL encode 20 to 21bit output
#if defined(MCU_F051) || defined(MCU_F031) || defined(MCU_CH32V203)
    gcr[1 + buffer_padding] = 64;
    for (int i = 19; i >= 0; i--) { // each digit in gcrnumber
        gcr[buffer_padding + 20 - i + 1] = ((((gcrnumber & 1 << i)) >> i) ^ (gcr[buffer_padding + 20 - i] >> 6))
            << 6; // exclusive ored with number before it multiplied by 64 to match
                  // output timer.
    }
    gcr[buffer_padding] = 0;
#else
    gcr[1 + buffer_padding] = 128;
    for (int i = 19; i >= 0; i--) { // each digit in gcrnumber
        gcr[buffer_padding + 20 - i + 1] = ((((gcrnumber & 1 << i)) >> i) ^ (gcr[buffer_padding + 20 - i] >> 7))
            << 7; // exclusive ored with number before it multiplied by 64 to match
                  // output timer.
    }
    gcr[buffer_padding] = 0;
#endif
}
