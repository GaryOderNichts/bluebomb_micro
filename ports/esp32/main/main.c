#include "btstack.h"
#include "btstack_port_esp32.h"
#include "btstack_stdio_esp32.h"
#include "hci_dump.h"
#include "hci_dump_embedded_stdout.h"
#include "driver/gpio.h"

#include "bluebomb_micro.h"

// warn about unsuitable sdkconfig
#include "sdkconfig.h"
#if !CONFIG_BT_ENABLED
#error "Bluetooth disabled - please set CONFIG_BT_ENABLED via menuconfig -> Component Config -> Bluetooth -> [x] Bluetooth"
#endif
#if !CONFIG_BT_CONTROLLER_ONLY
#error "Different Bluetooth Host stack selected - please set CONFIG_BT_CONTROLLER_ONLY via menuconfig -> Component Config -> Bluetooth -> Host -> Disabled"
#endif
#if ESP_IDF_VERSION_MAJOR >= 5
#if !CONFIG_BT_CONTROLLER_ENABLED
#error "Different Bluetooth Host stack selected - please set CONFIG_BT_CONTROLLER_ENABLED via menuconfig -> Component Config -> Bluetooth -> Controller -> Enabled"
#endif
#endif
#ifndef ENABLE_CLASSIC
#error "Classic mode needs to be enabled for bluebomb to work"
#endif

#define BLINK_ENABLE CONFIG_BLINK_ENABLE
#define BLINK_GPIO CONFIG_BLINK_GPIO

// Provided by generated l2cb.c file
extern uint32_t bluebomb_l2cb;

static btstack_timer_source_t heartbeat;

static void heartbeat_handler(struct btstack_timer_source *ts)
{
    // Invert the led
    static bool led_on = true;
    led_on = !led_on;

#if BLINK_ENABLE
    gpio_set_level(BLINK_GPIO, led_on);
#endif

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

int app_main(void)
{
#if BLINK_ENABLE
    // Set up the LED
    gpio_reset_pin(BLINK_GPIO);
    gpio_set_direction(BLINK_GPIO, GPIO_MODE_OUTPUT);
#endif

    // optional: enable packet logger
    // hci_dump_init(hci_dump_embedded_stdout_get_instance());

    // Enable buffered stdout
    btstack_stdio_init();

    // Configure BTstack for ESP32 VHCI Controller
    btstack_init();

    // set one-shot btstack timer
    heartbeat.process = &heartbeat_handler;
    btstack_run_loop_set_timer(&heartbeat, 1000);
    btstack_run_loop_add_timer(&heartbeat);

    // Setup bluebomb
    bluebomb_setup(bluebomb_l2cb);

    // Enter run loop (forever)
    btstack_run_loop_execute();

    return 0;
}
