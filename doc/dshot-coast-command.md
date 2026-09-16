# ORBT DShot coast protocol

This document defines the custom coast protocol in `ORBT_V203` firmware 2.19.
It is a safety-oriented, latched bridge-open command. It is not standard AM32.

## Why DShot zero is not coast

With the normal AM32 control path, zero throttle while the rotor is still tracked
continues the commutation state at zero duty. In complementary-PWM configurations
that is an electrical braking/freewheel state, not the same thing as opening every
gate. The EEPROM `brake_on_stop` setting only selects the idle behavior after AM32
has declared the motor stopped; it does not turn a running zero-throttle transition
into a coast. On this ORBT build it is additionally inhibited until the ESC has
actually accepted a drive command since the most recent arming. A reset or coast
release followed only by raw-zero traffic therefore cannot short a windmilling motor.

The coast command calls `allOff()` atomically and prevents the commutation code from
calling `comStep()`. This means no MOSFET conduction is commanded. It does **not**
guarantee zero current or zero torque: the six MOSFET body diodes still form a
three-phase rectifier, so sufficient motor BEMF can feed the DC bus and brake the
rotor. Bus overvoltage must be checked on the actual motor, speed, and supply.

## Capability check

The deployed system must positively identify both of these before allowing command
15 or 16:

- firmware name: `ORBT_V203`
- firmware version: `2.19` or a later version that explicitly preserves this contract

Refuse the custom commands if the identity cannot be established or does not match.
This check is currently **out of band**: the shipped ORBT target does not enable its
USART serial-telemetry output, and bidirectional DShot returns eRPM/EDT rather than
the firmware name. DShot command 6 therefore cannot provide the identity to this
client on the one-wire DShot link. Use a provisioned ESC-image/driver capability or
verify the zero-padded `ORBT_V203` image identifier while flashing. Do not infer
support merely because command 15 is accepted; stock AM32 does not provide this latch.

## Wire contract

| Wire value | Meaning |
| --- | --- |
| `0` | Mandatory arming value after boot-latch release; after actual drive, normal AM32 stop/brake behavior |
| `15` | `DSHOT_CMD_COAST`: enter and hold bridge-open coast |
| `16` | `DSHOT_CMD_COAST_RELEASE`: explicit release of the coast or power-on latch |
| `48` | After arming in 1D mode, a stop alias; it is **not** coast |
| `49..2047` | 1D drive range |

Bidirectional/3D interpretation remains unchanged: the two directional endpoints,
48 and 1048, remain non-driving after the sine-start mapping, and 49/1049 are their
first drive values. The 1D value-48 stop alias is not applied in 3D.

### Entering coast

1. Send command 15 until at least six consecutive valid decoded command values have
   reached the ESC. A missing or CRC-invalid frame does not count. A different valid
   value or an input-DMA transfer error breaks the run.
2. During the first five confirmations the ESC retains the previous input, including
   during sine startup. The bridge opens on dispatch of the sixth valid command.
   There is no coast acknowledgement, so six transmitted packets are not proof that
   six packets decoded successfully.
3. Continuing to send 15 after entry is harmless, but it is not required. Once
   entered, coast remains latched if the input stream disappears.

Command 15 is accepted while armed, running, stopped, or disarmed. On the V203,
both unidirectional and bidirectional DShot decode occur in the DMA interrupt. The sixth value
atomically sets the request, preloads each GPIO gate latch to its OFF level, disconnects
all phase pins from the timer, and then writes zero PWM compare. Every bridge-energizing
primitive is interrupt-excluded and rejects a pending/active coast or a disarmed ORBT,
so code interrupted by that frame cannot re-energize a phase when it resumes. Main
then finalizes the controller/PID state at the next loop boundary.

If command 36 previously entered EEPROM programming mode, the first valid 15 aborts
programming and becomes coast confirmation one. Programming mode can never consume or
delay the safety override.

### While coast is latched

- All ordinary DShot zero, throttle, beacon, direction, mode, save, and programming
  values are ignored. They are not queued.
- Only command 16 participates in release confirmation. Any other valid decoded value
  resets that confirmation run. A renewed command 15 also cancels a fully confirmed
  release if main has not committed it yet.
- The bridge stays open. The comparator may passively track zero crossings while they
  remain detectable, only to provide best-effort eRPM telemetry.
- eRPM zero means **speed unknown / tracking lost**, not confirmed mechanical rest.
  It must never be the sole condition for reversing or restarting.
- If valid DShot disappears for 0.5 seconds, passive tracking is stopped and the
  bridge remains open. The coast-specific timeout does not reboot or clear the latch,
  and it clears any incomplete release confirmation.

### Releasing coast

Release is deliberately separate from throttle. The ORBT application also starts in
the coast-latched state after every reset or power cycle, so this same release sequence
is required before the first arming sequence after boot:

Command 16 is a no-op outside an already latched coast; it does not apply zero
throttle or initiate braking.

1. Establish with an external system-level condition that the rotor is safe to start.
   Examples are a sufficiently conservative elapsed coast time validated for the
   mechanism, a separate position/speed sensor, or a mechanical stop. The ESC cannot
   establish this from open-loop BEMF tracking.
2. Send six consecutive valid command-16 values. A different valid value or input-DMA
   transfer error breaks the run. A transfer error before main commits the release may
   also cancel an already decoded sixth value; this deliberately fails latched.
3. The sixth value queues a release. At the next main-loop transaction, the ESC
   rechecks that the request has not been canceled, clears the latch, zeros every
   duty/controller accumulator, marks BEMF tracking invalid, disarms the ESC, and
   still leaves every gate off. The release command itself never brakes or drives.
4. Perform a fresh raw-zero arming sequence: at least 31 valid wire-zero packets,
   followed by the full one-second arm interval while further valid wire-zero packets
   keep every inter-packet gap at or below 50 ms. A command, throttle value, or longer
   gap resets the arm progress. Only after arming completes may the client apply
   same-direction throttle through the normal cold-start path. Do not reverse a
   windmilling rotor.

Unarmed DShot throttle remains non-driving and cannot enter the servo-PWM calibration
tone path. As a final backstop, every ORBT phase-energizing primitive rejects execution
while `armed == 0`.

The active latch is RAM state, but ORBT deliberately initializes it asserted rather
than cleared. The application puts all gate pins at their explicit OFF levels before
enabling TIM1 and suppresses both the phase-driving startup melody and automatic
arming confirmation tones. After every reset, the controller must re-establish the
out-of-band image capability, establish that restart is safe, send six accepted 16s,
and then perform the raw-zero arming sequence. Bootloader, reset-latch, and external
gate-driver behavior remain bench requirements.

## DShot value 48 compatibility

Upstream AM32 deliberately treats 48 as the first throttle value. Some existing
controllers use the BLHeli/Bluejay convention in which 48 represents armed zero.
For this firmware, raw 0 is still required to arm; once armed and in 1D mode, raw 48
is mapped to normal stop. This prevents a controller stuck at 48 from automatically
arming after an ESC reset.

The next value, 49, is still the first drive value. With the shipped
`startup_power = 150` and complementary PWM, that can select approximately 600/2000
startup duty and 450/2000 minimum running duty before voltage compensation. The 48
alias prevents unintended motion at logical zero; it does not make the low-throttle
curve gentle. Validate value 49 separately.

## Failure behavior

| Event | Required firmware behavior |
| --- | --- |
| Fewer than six accepted command-15 values | Coast not latched; a running motor retains its previous throttle during accepted 15 confirmations |
| Ordinary frame while latched | Ignored; bridge remains open |
| DShot link loss while latched | Bridge remains open; tracking stops; incomplete release run clears; no coast-timeout reset |
| Tracking lost | eRPM becomes zero/unknown; bridge remains open |
| Fewer than six accepted command-16 values | Latch remains active and bridge remains open |
| Sixth accepted command-16 value | Latch clears, state is invalidated, ESC disarms, bridge remains open |
| Command 16 outside coast | No input or bridge-state change |
| Input-DMA transfer error | Capture is discarded and rearmed without decode; entry/release/programming confirmation state resets; a pending release may be canceled, so the ESC fails latched |
| MCU hardware reset or power cycle | ORBT starts coast-latched; application boot/arming tones stay silent, gate pins are made OFF before TIM1 output enable, and six accepted 16s are required before raw-zero arming |
| Raw zeros after reset/release | Cannot release the latch; after release they may arm, but `brake_on_stop` remains inhibited until an actual drive command has occurred |

## Bench validation required before deployment

Firmware build success is not proof of electrical safety. Validate on a guarded rig,
initially without a hazardous propeller or mechanical load.

1. Use isolated or differential probes for every switching phase measurement. Do not
   connect a ground-referenced oscilloscope clip to a motor phase.
2. Monitor all six gate commands, the DShot line, DC-bus voltage, and bus/phase current.
   Use a battery or a supply plus a verified sink/clamp; many laboratory supplies
   cannot absorb regenerated energy.
3. At several speeds, confirm the last commanded gate edge occurs only after the sixth
   accepted 15 and no commanded edge occurs for the entire coast.
4. Inject missing, bad-CRC, and interleaved frames. Confirm the entry/release counters
   behave as specified and that an incomplete release never enables a gate.
   Also send command 16 before coast and after an incomplete entry; it must not change
   throttle or create a braking edge. Inject a Channel-5 input-DMA transfer error,
   including TC+TE together and immediately after a sixth 16; confirm the buffer is not
   decoded and the ESC remains latched.
5. While latched, send 0, 48, 49, full throttle, beacons, direction changes, and mode
   commands. Confirm all are ignored.
6. Remove DShot for longer than two seconds while coasting. Confirm all gates remain
   off, the MCU does not enter a reset loop, and restored ordinary traffic cannot
   release the latch.
7. Verify eRPM across the useful speed range and deliberately force tracking loss.
   Treat the resulting zero only as unknown.
8. Send five release values and verify the bridge remains open. Send the sixth and
   verify the bridge stays open and throttle is rejected until a fresh raw-zero arming
   interval completes. Insert 60 ms gaps and nonzero packets during arming and confirm
   the full one-second progress restarts. With `brake_on_stop` enabled, confirm the
   raw-zero stream still leaves all gates off until a real drive command has occurred.
   Send stable high DShot throttle while still disarmed and confirm it cannot start a
   servo-calibration tone or change any gate mode.
   Measure cold-start current after conservative coast intervals.
9. Test raw 48 before arming, after arming in 1D, and in 3D. In 3D, also confirm 1048
   remains non-driving. Test the first directional drive values 49/1049 separately at
   minimum and maximum battery voltage.
10. Measure DC-bus rise and diode current during the fastest coast. Repeat with the
    real motor Kv, inertia, battery, wiring inductance, and worst-case supply state.
11. Reset and brownout the MCU while the rotor windmills. Scope the external bootloader,
    application handoff, GPIO latch levels, gate-driver inputs, raw-zero re-arm, and
    watchdog reset. Confirm no automatic startup/arming tone or gate pulse occurs,
    ordinary zeros/throttle cannot release the boot latch, and six accepted 16s are
    required before raw-zero re-arming. Confirm the client does not resume throttle
    automatically.
12. While armed and stopped, enter command-36 EEPROM programming mode and then send six
    valid 15s. Confirm the first 15 aborts programming, the sixth opens the bridge, and
    no programming payload or commit can delay coast entry.

## Implementation map

- `Inc/dshot.h`: custom command values and shared latch requests.
- `Src/dshot.c`: consecutive-frame confirmation, release-only decoder state, and the
  armed 1D value-48 stop alias; programming abort and DMA-error rejection are part of
  the safety override.
- `Src/signal.c`: DShot capture processing and the servo-only calibration-entry guard.
  V203 bidirectional capture is decoded synchronously after starting response DMA
  from the separate telemetry buffer; it is never left for main to decode after a
  later capture or transfer error. Both DShot modes count valid arming zeros in the
  decoder, including after release.
- `Src/main.c`: atomic bridge-open entry, controller-state reset, continuous-zero
  re-arming, power-on safety latch, post-drive brake qualification, passive tracking
  guard, coast-specific signal-loss behavior, pre-TIM1 gate-off boot ordering, and
  silent ORBT startup/arming.
- `Mcu/v203/Src/phaseouts.c`: interrupt-excluded bridge transitions that reject a
  pending or active coast latch, preventing stale interrupted code from restoring a
  drive/brake configuration.
- `Mcu/v203/Src/IO.c`: state-preserving interrupt critical sections around the shared
  GPIO configuration register used by DShot PA0 and phase-C PA7.
- `Mcu/v203/Src/ch32v20x_it.c`: transfer-error-first DMA handling that discards and
  rearms a failed capture without decoding stale command data.
- `Inc/version.h`: capability-bearing firmware version 2.19.
- `Inc/interrupt.h`: nested interrupt save/restore with compiler memory barriers
  on WCH. The bridge and coast transactions use these helpers.

## Automated review checks

Run `python3 tests/run_coast_tests.py` on a host with GCC, or
`wsl -e python3 tests/run_coast_tests.py` from PowerShell. The tests compile the
actual decoder, capture handler, coast transactions, arming branch, and V203 phase
primitives with mocked peripherals. They cover six-frame confirmation, every
ordinary wire value while latched, CRC rejection, DMA TC+TE errors, release
cancellation races, programming override, bidirectional re-arming, arming gaps,
the 1D stop alias, and all bridge drive/brake guards. They do not simulate the
motor, measure interrupt latency, or prove physical gate levels.

The release-cancellation and bidirectional re-arming regressions both failed
against the initial working-tree implementation and pass after the review fixes.
See [standalone build notes](ch32v203-standalone-build.md) for the byte-identical
voltage-compensation baseline and the firmware build command.

The saved MounRiver `Download` launch target points at the external bootloader image,
not the newly linked application image. Verify the selected HEX explicitly before
flashing.
