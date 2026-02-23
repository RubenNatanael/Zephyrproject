#include <unistd.h>
#include "config.h"

struct k_sem new_data_sem;

struct TemperatureSchedule {
    struct Room *room;
    int time; // minutes from midnight
    int temperature; // scale  *100
};

struct TemperatureSchedule list[MAX_SCHEDULES];
uint8_t number_of_elements = 0;

void thread() {
    while(1) {
        TemperatureSchedule* next = get_next_schedule()
        uint32_t sleep_ms = calculate_next_event_time(next);
        
        int ret = k_sem_take(&new_data_sem, K_MSEC(sleep_ms));

        if (ret == -EAGAIN) {
            next->room->desired_temperature = next->temperature;
        } 
        // If ret == 0: Just restart the loop to recalculate with new data
    }
}

TemperatureSchedule* get_next_schedule() {
    if (number_of_elements == 0) return K_FOREVER;

    int current_time = get_seconds_today_from_rtc();
    int next_idx = -1;
    int min_day_idx = 0; // Smallest time overall (for tomorrow)

    for (int i = 0; i < number_of_elements; i++) {
        if (list[i].time < list[min_day_idx].time) {
            min_day_idx = i;
        }

        if (list[i].time > current_time) {
            if (next_idx == -1 || list[i].time < list[next_idx].time) {
                next_idx = i;
            }
        }
    }

    // If no future event today, use the earliest event of the next day
    uint8_t currentEventIndex = (next_idx != -1) ? next_idx : min_day_idx;

    return &list[currentEventIndex];
}

uint32_t calculate_time_for_schedule(TemperatureSchedule schedule) {
    
    int current_time = get_seconds_today_from_rtc();
    int diff = schedule->time - current_time;

    if (diff <= 0) diff += 1440; // Wrap around logic

    return (uint32_t)diff * 60 * 1000;
}

int addNewSchedule(struct TemperatureSchedule schedule) {
    if (number_of_elements >= MAX_SCHEDULES) return -ENOSPC;

    list[number_of_elements] = schedule;
    number_of_elements++;
    
    k_sem_give(&new_data_sem);
    
    return 0;
}

int removeSchedule(struct TemperatureSchedule *target) {
    int index = -1;

    for (int i = 0; i < number_of_elements; i++) {
        if (list[i].time == target->time && 
            list[i].temperature == target->temperature) {
            index = i;
            break;
        }
    }

    if (index == -1) {
        return -ENOENT;
    }
    
    for (int i = index; i < number_of_elements - 1; i++) {
        list[i] = list[i + 1];
    }

    number_of_elements--;
    memset(&list[number_of_elements], 0, sizeof(struct TemperatureSchedule));

    k_sem_give(&new_data_sem);

    return 0;
}

// ################
// ################
CONFIG_RTC=y


#include <zephyr/net/sntp.h>
#include <zephyr/net/socket.h>
#include <time.h>
#include <zephyr/drivers/rtc.h>

    
static uint64_t get_net_time(void) {
    struct sntp_time sntp_time;
    int rv;

    const char *server = "216.239.35.0"; // time.google.com

    LOG_INF("Sending SNTP request...");
    
    rv = sntp_query(server, SYS_FOREVER_MS, &sntp_time);
    if (rv < 0) {
        LOG_ERR("SNTP lookup failed: %d", rv);
        return;
    }

    LOG_INF("Seconds since 1 Jan 1970: %llu", sntp_time.seconds);
    return sntp_time.seconds;
}

static const struct device *rtc_dev = DEVICE_DT_GET(DT_NODELABEL(rtc));

static void sync_rtc_with_network() {

    if (!device_is_ready(rtc_dev)) {
        printk("RTC device not ready\n");
        return;
    }
    uint64_t sntp_seconds = get_net_time();
    if (sntp_seconds == 0) return;

    struct tm time_ptr;
    time_t local_time_val = (time_t)sntp_seconds + UTC_PLUS_2;    

    gmtime_r(&time_val, &time_ptr);

    // Set the Hardware RTC
    int ret = rtc_set_time(rtc_dev, &time_ptr);
    if (ret < 0) {
        LOG_ERR("Failed to set RTC: %d\n", ret);
    } else {
        LOG_INF("RTC Synchronized!\n");
    }
}

uint32_t get_seconds_today_from_rtc(void) {
    struct tm now;
    
    // Read current time from hardware
    rtc_get_time(rtc_dev, &now);

    // Calculate seconds since midnight
    return (now.tm_hour * 3600) + (now.tm_min * 60) + now.tm_sec;
}