/*
 * dshot.h
 *
 *  Created on: Apr. 22, 2020
 *      Author: Alka
 */

#include "main.h"

#ifndef INC_DSHOT_H_
#define INC_DSHOT_H_

void computeDshotDMA(void);
void rejectDshotCapture(void);
void make_dshot_package(uint16_t com_time);

extern void playInputTune(void);
extern void playInputTune2(void);
extern void playBeaconTune3(void);
extern void saveEEpromSettings(void);

extern char dshot_telemetry;
extern char armed;
extern char dir_reversed;
extern char buffer_divider;
extern uint8_t last_dshot_command;
extern uint32_t commutation_interval;

// Custom commands in the unassigned 15-19 block of the DShot command space. COAST latches
// the bridge in its all-gates-off state after 6 valid frames. While latched, every ordinary
// command and throttle frame is ignored; only 6 valid COAST_RELEASE frames can leave it.
// The release is an explicit assertion by the controller that the rotor is safe to restart.
// Passive BEMF tracking may continue while it is detectable, but telemetry 0 only means that
// tracking was lost; it is not proof that the rotor has physically stopped. An open bridge
// commands no MOSFET conduction, although body-diode conduction remains physically possible.
#define DSHOT_CMD_COAST 15
#define DSHOT_CMD_COAST_RELEASE 16
extern volatile uint8_t coast_request;
extern volatile uint8_t coast_release_request;
extern volatile uint8_t coast_release_count;
extern volatile uint8_t dshot_arm_zero_count;
void requestCoast(void);
void coastMotor(void);

// int e_com_time;

#endif /* INC_DSHOT_H_ */
