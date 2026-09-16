#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#define ORBT_ESC_V203
typedef struct { uint32_t CFGLR, CFGHR, BSHR, BCR; } GPIO;
GPIO gpio_a, gpio_b;
#define PHASE_A_GPIO_PORT_HIGH (&gpio_a)
#define PHASE_B_GPIO_PORT_HIGH (&gpio_a)
#define PHASE_C_GPIO_PORT_HIGH (&gpio_a)
#define PHASE_A_GPIO_PORT_LOW (&gpio_b)
#define PHASE_B_GPIO_PORT_LOW (&gpio_b)
#define PHASE_C_GPIO_PORT_LOW (&gpio_a)
#define PHASE_A_GPIO_HIGH (1u << 10)
#define PHASE_B_GPIO_HIGH (1u << 9)
#define PHASE_C_GPIO_HIGH (1u << 8)
#define PHASE_A_GPIO_LOW (1u << 1)
#define PHASE_B_GPIO_LOW (1u << 0)
#define PHASE_C_GPIO_LOW (1u << 7)
struct { int comp_pwm; } eepromBuffer = {1};
char armed, prop_brake_active;
volatile uint8_t coast_request, coast_active;
uint32_t irq_state;
uint32_t saveAndDisableInterrupts(void) {
    uint32_t saved = irq_state;
    irq_state = 0;
    return saved;
}
void restoreInterrupts(uint32_t saved) { irq_state = saved; }
/* PRODUCTION */

int main(void) {
    for (unsigned saved = 0; saved <= 1; ++saved) {
        for (unsigned guard = 0; guard < 3; ++guard) {
            armed = guard != 0;
            coast_request = guard == 1;
            coast_active = guard == 2;
            irq_state = saved;
            memset(&gpio_a, 0x5a, sizeof(gpio_a));
            memset(&gpio_b, 0xa5, sizeof(gpio_b));
            GPIO before_a = gpio_a, before_b = gpio_b;
            for (int step = 1; step <= 6; ++step) comStep(step);
            fullBrake(); proportionalBrake(); allpwm();
            twoChannelForward(); twoChannelReverse();
            assert(!memcmp(&gpio_a, &before_a, sizeof(gpio_a)));
            assert(!memcmp(&gpio_b, &before_b, sizeof(gpio_b)));
            assert(irq_state == saved);
            allOff();
            assert((gpio_a.CFGHR & 0xfff) == 0x333);
            assert((gpio_a.CFGLR >> 28) == 3);
            assert((gpio_b.CFGLR & 0xff) == 0x33);
            assert(irq_state == saved);
        }
        armed = 1; coast_request = coast_active = 0; irq_state = saved;
        allpwm();
        assert((gpio_a.CFGHR & 0xfff) == 0xbbb);
        assert((gpio_a.CFGLR >> 28) == 0xb);
        assert((gpio_b.CFGLR & 0xff) == 0xbb);
        assert(irq_state == saved);
    }
    puts("PASS: every V203 drive/brake primitive rejects coast/disarm; OFF remains unconditional");
    puts("PASS: GPIO mode transitions preserve prior interrupt enable state");
    return 0;
}
