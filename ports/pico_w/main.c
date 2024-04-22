#include <stdio.h>
#include "btstack.h"
#include "pico/cyw43_arch.h"
#include "pico/stdlib.h"

#include "bluebomb_micro.h"

// Provided by generated l2cb.c file
extern uint32_t bluebomb_l2cb;

static btstack_timer_source_t heartbeat;

static void heartbeat_handler(struct btstack_timer_source *ts)
{
    // Invert the led
    static bool led_on = true;
    led_on = !led_on;
    cyw43_arch_gpio_put(CYW43_WL_GPIO_LED_PIN, led_on);

    uint32_t blink_speed;
    switch (bluebomb_get_stage()) {
        // Very slow idle blink
        case STAGE_INIT:
            blink_speed = 1000;
            break;
        // Fast blink while uploading payload
        case STAGE_UPLOAD_PAYLOAD:
            blink_speed = 50;
            break;
        // Medium blink during all other connected stages
        default:
            blink_speed = 100;
            break;
    }

    // Restart timer
    btstack_run_loop_set_timer(ts, blink_speed);
    btstack_run_loop_add_timer(ts);
}

int main()
{
    stdio_init_all();
    // add a small delay for usb serial to catch up
    // sleep_ms(1000 * 2);

    // initialize CYW43 driver architecture
    if (cyw43_arch_init()) {
        printf("failed to initialise cyw43_arch\n");
        return -1;
    }

    // set one-shot btstack timer
    heartbeat.process = &heartbeat_handler;
    btstack_run_loop_set_timer(&heartbeat, 1000);
    btstack_run_loop_add_timer(&heartbeat);

    bluebomb_setup(bluebomb_l2cb);

    // Enter run loop (forever)
    btstack_run_loop_execute();

    return 0;
}
