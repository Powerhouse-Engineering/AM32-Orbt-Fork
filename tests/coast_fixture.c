#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#define ORBT_ESC_V203
#define MCU_CH32V203
#define LOOP_FREQUENCY_HZ 20000
#define DISABLE_ARMING_TUNE
/* EEPROM */
/* DSHOT HEADER */
EEprom_t eepromBuffer;
typedef struct fastPID {
    int32_t error, integral, derivative, last_error, pid_output;
} fastPID;
fastPID speedPid, stallPid, currentPid;
char armed, dshot_telemetry, forward, send_telemetry, play_tone_flag, send_esc_info_flag;
char prop_brake_active, stepper_sine, old_routine, servoPwm, dshot, inputSet, out_put;
uint8_t last_dshot_command, running, dshotcommand, buffer_padding, compute_dshot_flag;
uint32_t commutation_interval, dma_buffer[64];
uint16_t newinput, input, adjusted_input, zero_input_count, signaltimeout, armed_timeout_count;
extern uint16_t dshot_frametime_low, dshot_frametime_high;
uint16_t duty_cycle_setpoint, duty_cycle, last_duty_cycle, adjusted_duty_cycle;
int32_t input_override, stall_protection_adjust, use_current_limit_adjust;
int zero_crosses, drive_seen_since_arm, cell_count, e_com_time;
int tenkhzcounter, ledcounter, one_khz_loop_counter, LOW_VOLTAGE_CUTOFF, battery_voltage;
int servo_high_threshold = 1900, servo_low_threshold = 1100, servo_neutral = 1500;
int ic_timer_prescaler, CPU_FREQUENCY_MHZ = 48;
volatile uint8_t coast_request, coast_active, coast_release_request;
uint32_t irq_state = 1;
int inject_error_on_disable, off_calls, compare_value;
struct { uint32_t INTFR, INTFCR; } dma_regs;
struct { uint32_t CFGR; } dma_channel;
struct { uint32_t CCER; } input_timer;
#define IC_TIMER_REGISTER (&input_timer)
#define DMA1 (&dma_regs)
#define INPUT_DMA_CHANNEL (&dma_channel)
#define DMA1_IT_GL5 1u
#define DMA1_IT_TC5 2u
#define DMA1_IT_HT5 4u
#define DMA1_IT_TE5 8u
#define CLEAR_BIT(reg, bits) ((reg) &= ~(bits))
int input_ready;
void rejectDshotCapture(void);
uint32_t __get_MSTATUS(void) { return irq_state; }
void __disable_irq(void) {
    if (inject_error_on_disable) {
        inject_error_on_disable = 0;
        rejectDshotCapture();
    }
    irq_state = 0;
}
void __set_MSTATUS(uint32_t state) { irq_state = state; }
uint32_t saveAndDisableInterrupts(void) {
    uint32_t state = irq_state;
    __disable_irq();
    return state;
}
void restoreInterrupts(uint32_t state) { irq_state = state; }
void allOff(void) { off_calls++; }
void maskPhaseInterrupts(void) {}
#define SET_DUTY_CYCLE_ALL(x) (compare_value = (x))
#define DISABLE_COM_TIMER_INT() ((void)0)
#define SET_INTERVAL_TIMER_COUNT(x) ((void)(x))
int getInputPinState(void) { return 0; }
void saveEEpromSettings(void) {}
void playDefaultTone(void) {}
void playChangedTone(void) {}
void playBeaconTune3(void) {}
void receiveDshotDma(void) { out_put = 0; }
void sendDshotDma(void) { out_put = 1; }
void detectInput(void);
int map(int x, int lo, int hi, int outlo, int outhi) {
    return outlo + (x - lo) * (outhi - outlo) / (hi - lo);
}
int getAbsDif(int a, int b) { return a > b ? a - b : b - a; }
/* PRODUCTION */

static void packet(unsigned value, int corrupt) {
    unsigned payload = (value << 1) | 1;
    unsigned crc = (payload ^ (payload >> 4) ^ (payload >> 8)) & 15;
    if (dshot_telemetry) crc ^= 15;
    unsigned frame = (payload << 4) | (crc ^ !!corrupt);
    for (unsigned i = 0; i < 16; ++i) {
        dma_buffer[2*i] = 100*i;
        dma_buffer[2*i+1] = 100*i + ((frame & (1u << (15-i))) ? 75 : 37);
    }
}
static void decode(unsigned value) { packet(value, 0); computeDshotDMA(); }
static void reset(void) {
    memset(&eepromBuffer, 0, sizeof(eepromBuffer));
    coast_request = coast_active = coast_release_request = 0;
    armed = running = 1;
    dshot = inputSet = 1;
    dshot_telemetry = servoPwm = 0;
    EDT_ARMED = 1;
    EDT_ARM_ENABLE = 0;
    command_count = last_command = coast_release_count = dshot_arm_zero_count = 0;
    programming_mode = zero_input_count = armed_timeout_count = 0;
    newinput = input = 700;
    adjusted_input = signaltimeout = cell_count = 0;
    dshot_frametime_low = 1400;
    dshot_frametime_high = 1700;
    irq_state = 1;
    inject_error_on_disable = off_calls = 0;
    average_count = 8;
}
static void enter(void) { for (int i = 0; i < 6; ++i) decode(15); coastMotor(); }
static void confirm_release(void) { for (int i = 0; i < 6; ++i) decode(16); }

int main(void) {
    setvbuf(stdout, NULL, _IONBF, 0);

    reset();
    for (int i = 0; i < 5; ++i) { decode(15); assert(!coast_request && newinput == 700); }
    decode(15);
    assert(coast_request && off_calls && compare_value == 0);
    coastMotor();
    for (unsigned i = 0; i < 2048; ++i) {
        if (i == 16) continue;
        decode(i);
        assert(coast_request && !coast_release_request && !newinput);
    }
    for (int i = 0; i < 5; ++i) decode(16);
    assert(!coast_release_request);
    decode(0);
    for (int i = 0; i < 5; ++i) decode(16);
    assert(!coast_release_request);
    decode(16);
    assert(coast_release_request);
    releaseCoast();
    assert(!coast_request && !armed && !running && !compare_value);
    puts("PASS: six-frame entry/release and exhaustive ignored ordinary values");

    reset();
    decode(15); decode(900);
    for (int i = 0; i < 5; ++i) decode(15);
    assert(!coast_request);
    rejectDshotCapture(); decode(15);
    assert(!coast_request);
    programming_mode = 1;
    command_count = 0;
    for (int i = 0; i < 6; ++i) decode(15);
    assert(coast_request && !programming_mode);
    puts("PASS: interrupted confirmation and programming override");

    reset(); enter(); confirm_release();
    // Main has observed release_request; the DMA ISR cancels it before IRQ masking.
    inject_error_on_disable = 1;
    releaseCoast();
    assert(coast_request && coast_active && !coast_release_request);
    puts("PASS: DMA cancellation between release check and critical section");

    reset(); enter(); confirm_release(); decode(15); releaseCoast();
    assert(coast_request && coast_active && !coast_release_request);
    puts("PASS: renewed coast supersedes an uncommitted release");

    reset(); enter(); confirm_release();
    unsigned good_before = dshot_goodcounts;
    packet(16, 0);
    DMA1->INTFR = DMA1_IT_TC5 | DMA1_IT_TE5;
    compute_dshot_flag = 1;
    DMA1_Channel5_IRQHandler();
    assert(dshot_goodcounts == good_before && !coast_release_request);
    assert(!coast_release_count && !compute_dshot_flag && !input_ready);
    assert(DMA1->INTFCR == DMA1_IT_GL5);
    puts("PASS: simultaneous DMA completion/error discards capture");

    reset(); enter();
    for (int i = 0; i < 5; ++i) decode(16);
    packet(16, 1); computeDshotDMA();
    assert(coast_request && !coast_release_request && coast_release_count == 5);
    decode(16);
    assert(coast_release_request);
    puts("PASS: invalid CRC does not count as a release confirmation");

    reset(); enter(); confirm_release(); releaseCoast();
    dshot_telemetry = 1;
    for (int i = 0; i < 31; ++i) {
        packet(0, 0);
        out_put = 0;
        transfercomplete();
        transfercomplete();
    }
    assert(dshot_arm_zero_count == 31 && zero_input_count >= 31);
    for (int i = 0; i <= LOOP_FREQUENCY_HZ; ++i) armTick();
    assert(armed);
    puts("PASS: bidirectional DShot zeros can re-arm after release");

    reset(); armed = running = 0;
    for (int i = 0; i < 31; ++i) decode(0);
    for (int i = 0; i < LOOP_FREQUENCY_HZ / 2; ++i) armTick();
    assert(!armed && armed_timeout_count > 0);
    decode(48); armTick();
    assert(!armed && !armed_timeout_count && !dshot_arm_zero_count);
    for (int i = 0; i < 31; ++i) decode(0);
    armTick();
    signaltimeout = LOOP_FREQUENCY_HZ / 20 + 1;
    armTick();
    assert(!armed && !armed_timeout_count && !dshot_arm_zero_count);
    puts("PASS: nonzero packets and link gaps restart arming");

    reset(); decode(48); assert(newinput == 0);
    decode(49); assert(newinput == 49);
    eepromBuffer.bi_direction = 1;
    decode(48); assert(newinput == 48);
    decode(1048); assert(newinput == 1048);
    puts("PASS: 1D stop alias preserves 3D wire endpoints");
    return 0;
}
