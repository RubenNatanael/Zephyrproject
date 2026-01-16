#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/drivers/pwm.h>
#include <zephyr/net/http/server.h>
#include <zephyr/net/http/service.h>
#include <zephyr/logging/log.h>
#include <stddef.h>
#include <string.h>

#include "Room.h"

LOG_MODULE_REGISTER(main, LOG_LEVEL_DBG);


#define SLEEP_TIME_MS 200
#define STACKSIZE 1024
#define PRIORITY 7

void listening_switch_events_thread(void) {

    struct Room **rooms = get_all_rooms();

    while (1) {
        int percentage_ = 50;

        for (int i = 0; i < STRUCT_ROOM_COUNT; i++) {

            bool new_state = gpio_pin_get_dt(rooms[i]->light_switch);

            if (new_state != rooms[i]->light_value) {
                // In case is a PWM event light needs special value so I calculated it here
                new_state = rooms[i]->light_pwm->period * percentage_ / 100;
                register_new_event(rooms[i], new_state);
            }
        }

        k_msleep(SLEEP_TIME_MS);
    }
}

void execut_events_thread(void) {
    while (1) {
        struct Event *registered_event = k_fifo_get(&events_fifo,
							   K_FOREVER);
        registered_event->action(
            registered_event->ctx,
            registered_event->value
        );

        k_free(registered_event);

        k_msleep(SLEEP_TIME_MS);
    }
}

int main(void)
{
    LOG_INF("Booting C++ Zephyr LightSwitch app");
    if (!room_device_init()) {
        LOG_ERR("Error while initializing the devices");
        return 0;
    }

    int ret = 0;
    ret = http_server_start();
    if (ret) {
        LOG_ERR("Server failed: %d", ret);
    }
    LOG_INF("HTTP server started");
    while (1) {

        ret = gpio_pin_toggle_dt(get_led_by_id(ROOM_LED_POWER));
        if (ret < 0) {
            return -1;
        }
        k_msleep(SLEEP_TIME_MS);
    }

    return 0;
    
}

// --- Thread definitions ---
K_THREAD_DEFINE(listening_id, STACKSIZE, listening_switch_events_thread, NULL, NULL, NULL,
                PRIORITY, 0, 0);
K_THREAD_DEFINE(execut_id, STACKSIZE, execut_events_thread, NULL, NULL, NULL,
                PRIORITY, 0, 0);
