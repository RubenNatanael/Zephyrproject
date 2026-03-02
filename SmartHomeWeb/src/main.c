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
#include "config.h"
#include "Schedule.h"

#include <zephyr/sys/sys_heap.h>

extern struct sys_heap _system_heap; // The default system heap

void check_memory(void) {
    struct sys_memory_stats stats;
    sys_heap_runtime_stats_get(&_system_heap, &stats);

    printk("Heap - Free: %zu | Allocated: %zu | Max: %zu\n", 
            stats.free_bytes, stats.allocated_bytes, stats.max_allocated_bytes);
}

LOG_MODULE_REGISTER(main, LOG_LEVEL_DBG);

void listening_switch_events_thread(void) {

    struct Room **rooms = get_all_rooms();

    while (1) {
        int percentage_ = 90;

        for (int i = 0; i < STRUCT_ROOM_COUNT; i++) {

            // uint32_t new_state = gpio_pin_get_dt(rooms[i]->light_switch);

            // // In case is a PWM event light needs special value so I calculated it here (90% of brightness for ON state)
            // new_state = rooms[i]->light_pwm->period * percentage_ / 100;

            // process_light_control(rooms[i], new_state);
        }

        k_msleep(SLEEP_TIME_MS);
    }
}

void listening_tmp_events_thread(void) {

    struct Room **rooms = get_all_rooms();

    while (1) {

        for (int i = 0; i < STRUCT_ROOM_COUNT; i++) {

            uint32_t temp_scaled_value = 0;
            uint32_t hum_scaled_value = 0;
            if (rooms[i]->temp_dht11 != NULL) {
                int res = read_temp_and_hum_dht11(rooms[i], &temp_scaled_value, &hum_scaled_value);
                k_sleep(K_SECONDS(2));
                if (res < 0) {
                    LOG_ERR("Error reading DHT11 sensor for room %d", rooms[i]->room_id);
                    // Even if temperature is not registered, setpoint may be changed
                    // so we need to process it
                    process_temperature_control(rooms[i]);
                    continue;
                }

            } else {
                read_temp_and_hum(rooms[i], &temp_scaled_value, &hum_scaled_value);
            }

            if (temp_scaled_value != rooms[i]->temp_sensor_value ||
                hum_scaled_value != rooms[i]->hum_sensor_value) {
                register_new_event(rooms[i], temp_scaled_value, HEAT_EV, true);
                register_new_event(rooms[i], hum_scaled_value, HUM_EV, true);
                rooms[i]->temp_sensor_value = temp_scaled_value;
                rooms[i]->hum_sensor_value = hum_scaled_value;
            }
            
            process_temperature_control(rooms[i]);
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

void temperature_schedule_thread() {
    while(1) {
        struct TemperatureSchedule *next = get_next_schedule();
        if (next == NULL) {
            k_sem_take(&new_data_sem, K_FOREVER); // Sleep until a new schedule is added
            continue;
        }
        uint32_t sleep_ms = calculate_time_for_schedule(next);
        LOG_INF("Sleep sec more: %d", sleep_ms);
        
        int ret = k_sem_take(&new_data_sem, K_MSEC(sleep_ms));

        if (ret == -EAGAIN) {
            LOG_INF("Schedule, setting temperature to %d", next->temperature);
            next->room->desired_temperature = next->temperature;
            k_sleep(K_SECONDS(30)); // Sleep 30 sec to ensure function is not called again
            continue;
        }
        // If ret == 0: Just restart the loop to recalculate with new data
        LOG_INF("Semaphore signal"); // DEBUG
        k_sleep(K_SECONDS(1)); // DEBUG to be removed!
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
        return -1;
    }

    LOG_INF("Sync time");
    sync_rtc_with_network();

    LOG_INF("HTTP server started");
    while (1) {

        //check_memory();
        ret = gpio_pin_toggle_dt(get_led_by_id(ROOM_LED_POWER));
        if (ret < 0) {
            return -1;
        }
        // get_seconds_today_from_rtc();

        k_sleep(K_SECONDS(2));
    }

    return 0;
    
}

// --- Thread definitions ---
K_THREAD_DEFINE(listening_id, STACKSIZE, listening_switch_events_thread, NULL, NULL, NULL,
                PRIORITY_7, 0, 0);
K_THREAD_DEFINE(execut_id, STACKSIZE, execut_events_thread, NULL, NULL, NULL,
                PRIORITY_7, 0, 0);
K_THREAD_DEFINE(listening_tmp_id, STACKSIZE, listening_tmp_events_thread, NULL, NULL, NULL,
                PRIORITY_7, 0, 0);
// K_THREAD_DEFINE(executin_schedle_id, STACKSIZE, temperature_schedule_thread, NULL, NULL, NULL,
//                 PRIORITY_7, 0, 0);
