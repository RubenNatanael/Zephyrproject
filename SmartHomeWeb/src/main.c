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

#include <zephyr/sys/sys_heap.h>

extern struct sys_heap _system_heap; // The default system heap

void check_memory(void) {
    struct sys_memory_stats stats;
    sys_heap_runtime_stats_get(&_system_heap, &stats);

    printk("Heap - Free: %zu | Allocated: %zu | Max: %zu\n", 
            stats.free_bytes, stats.allocated_bytes, stats.max_allocated_bytes);
}

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

            if (new_state != rooms[i]->light_gpio_value) {
                // In case is a PWM event light needs special value so I calculated it here
                new_state = rooms[i]->light_pwm->period * percentage_ / 100;
                register_new_event(rooms[i], new_state, true);
                rooms[i]->light_gpio_value = new_state;
            }
        }

        k_msleep(SLEEP_TIME_MS);
    }
}

void listening_tmp_events_thread(void) {

    struct Room **rooms = get_all_rooms();

    while (1) {
        int percentage_ = 50;

        for (int i = 0; i < STRUCT_ROOM_COUNT; i++) {

            uint32_t temp_value = 0;
            uint32_t hum_value = 0;
            read_temp_and_hum(rooms[i], &temp_value, &hum_value);

            if (temp_value != rooms[i]->temp_sensor_value ||
                hum_value != rooms[i]->hum_sensor_value) {
                register_new_temp_hum_event(rooms[i], temp_value, hum_value, true);
                rooms[i]->temp_sensor_value = temp_value;
                rooms[i]->hum_sensor_value = hum_value;
            }
            if (rooms[i]->desired_temperature > rooms[i]->temp_sensor_value - rooms[i]->offset_desired_temperature / 100) {
                register_new_event(rooms[i], 1, false);
                rooms[i]->heat_relay_state = true;
            } else if (rooms[i]->desired_temperature < rooms[i]->temp_sensor_value + rooms[i]->offset_desired_temperature / 100) {
                register_new_event(rooms[i], 0, false);
                rooms[i]->heat_relay_state = false;
            }
        }

        k_sleep(K_SECONDS(10));
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

        check_memory();
        ret = gpio_pin_toggle_dt(get_led_by_id(ROOM_LED_POWER));
        if (ret < 0) {
            return -1;
        }
        k_sleep(K_SECONDS(2));
    }

    return 0;
    
}

// --- Thread definitions ---
K_THREAD_DEFINE(listening_id, STACKSIZE, listening_switch_events_thread, NULL, NULL, NULL,
                PRIORITY, 0, 0);
K_THREAD_DEFINE(execut_id, STACKSIZE, execut_events_thread, NULL, NULL, NULL,
                PRIORITY, 0, 0);
K_THREAD_DEFINE(listening_tmp_id, STACKSIZE, listening_tmp_events_thread, NULL, NULL, NULL,
                PRIORITY, 0, 0);