#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/drivers/pwm.h>
#include <stddef.h>
#include <string.h>

#include "Room.h"

#define SLEEP_TIME_MS 200
#define STACKSIZE 1024
#define PRIORITY 7

void log_msg(const char* fmt, ...) {
    va_list args;
    va_start(args, fmt);
    vprintk(fmt, args);
    va_end(args);
}

void listening_events_thread(void) {

    struct Room **rooms = get_all_rooms();

    while (1) {
        int percentage_ = 50;

        for (int i = 0; i < STRUCT_ROOM_COUNT; i++) {

            bool new_state = gpio_pin_get_dt(rooms[i]->light_switch);

            struct Event *new_event = k_malloc(sizeof(struct Event));
            if (!new_event) {
                printk("Unable to allocate memory for event\n");
                return;
            }

            /* Register light events */
            if (rooms[i]->light_gpio != NULL) {
                new_event->action = gpio_event_action;
                new_event->ctx = (void *)rooms[i]->light_gpio;
                new_event->value = new_state;
            }
            if (rooms[i]->light_pwm != NULL) {
                // PWM light needs special value so I calculated it here
                new_state = rooms[i]->light_pwm->period * percentage_ / 100;
                new_event->action = pwm_event_action;
                new_event->ctx = (void *)rooms[i]->light_pwm;
                new_event->value = new_state;
            }

            // TODO register heat events

            k_fifo_put(&events_fifo, new_event);
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
    printk("Booting C++ Zephyr LightSwitch app\n");
    if (!room_device_init()) {
        printk("Error while initializing the devices\n");
        return 0;
    }

    int ret = 0;
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
K_THREAD_DEFINE(listening_id, STACKSIZE, listening_events_thread, NULL, NULL, NULL,
                PRIORITY, 0, 0);
K_THREAD_DEFINE(execut_id, STACKSIZE, execut_events_thread, NULL, NULL, NULL,
                PRIORITY, 0, 0);
