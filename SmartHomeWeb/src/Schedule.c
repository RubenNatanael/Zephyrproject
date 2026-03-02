#include "Schedule.h"

LOG_MODULE_REGISTER(schedule, LOG_LEVEL_DBG);

// Getting Time and setting it RTC
static const struct device *rtc_dev = DEVICE_DT_GET(DT_NODELABEL(rtc));

struct TemperatureSchedule list[MAX_SCHEDULES];
uint8_t number_of_elements = 0;

K_SEM_DEFINE(new_data_sem, 0, 1);

static uint64_t get_net_time(void) {
    struct sntp_time sntp_time;
    int rv;

    const char *server = "216.239.35.0"; // time.google.com

    LOG_INF("Sending SNTP request...");
    
    rv = sntp_simple(server, SYS_FOREVER_MS, &sntp_time);
    if (rv < 0) {
        LOG_ERR("SNTP lookup failed: %d", rv);
        return 0;
    }

    LOG_INF("Seconds since 1 Jan 1970: %llu", sntp_time.seconds);
    return sntp_time.seconds;
}

void sync_rtc_with_network() {

    if (!device_is_ready(rtc_dev)) {
        LOG_ERR("RTC device not ready\n");
        return;
    }
    uint64_t sntp_seconds = get_net_time();
	time_t local_time_val = (time_t)sntp_seconds + UTC_PLUS_2;
    struct tm temp_tm;
    gmtime_r(&local_time_val, &temp_tm);

    // Create the Zephyr-specific RTC structure
    struct rtc_time zephyr_rtc_time;

    // Map fields from struct tm to struct rtc_time
    zephyr_rtc_time.tm_sec  = temp_tm.tm_sec;
    zephyr_rtc_time.tm_min  = temp_tm.tm_min;
    zephyr_rtc_time.tm_hour = temp_tm.tm_hour;
    zephyr_rtc_time.tm_mday = temp_tm.tm_mday;
    zephyr_rtc_time.tm_mon  = temp_tm.tm_mon;
    // IMPORTANT: Zephyr RTC expects the real year, not "years since 1900"
    zephyr_rtc_time.tm_year = temp_tm.tm_year + 1900;
    
    // Optional fields (set to 0 or -1 if not used)
    zephyr_rtc_time.tm_wday = temp_tm.tm_wday;
    zephyr_rtc_time.tm_yday = temp_tm.tm_yday;
    zephyr_rtc_time.tm_isdst = -1; 
    zephyr_rtc_time.tm_nsec = 0;

    // Now call the function with the correct pointer type
    int ret = rtc_set_time(rtc_dev, &zephyr_rtc_time);
    
    if (ret < 0) {
        LOG_ERR("Failed to set RTC: %d", ret);
        return;
    }
    LOG_INF("Time is %d:%d", temp_tm.tm_hour, temp_tm.tm_min);
}

uint32_t get_seconds_today_from_rtc(void) {
    struct rtc_time now;
    
    // Read current time from hardware
    rtc_get_time(rtc_dev, &now);

    // Calculate seconds since midnight
    LOG_INF("Time is %d:%d,    %d", now.tm_hour, now.tm_min, to_seconds_today(now));
    return to_seconds_today(now);
}

uint32_t to_seconds_today(struct rtc_time time) {
    return (time.tm_hour * 3600) + (time.tm_min * 60) + time.tm_sec;
}

// Schedule

struct TemperatureSchedule* get_next_schedule() {
    if (number_of_elements == 0) return NULL;

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

uint32_t calculate_time_for_schedule(struct TemperatureSchedule* schedule) {
    
    int current_time = get_seconds_today_from_rtc();
    int diff = schedule->time - current_time;
    LOG_ERR("%d - %d = %d", schedule->time, current_time, diff);

    if (diff <= 0) diff += 86400; // Wrap around logic

    return (uint32_t)diff;
}

int addNewSchedule(struct TemperatureSchedule schedule) {
    if (number_of_elements >= MAX_SCHEDULES) return -ENOSPC;

    list[number_of_elements] = schedule;
    number_of_elements++;
    LOG_ERR("Adding new schedule and notifing(nr of sched: %d), time %d", number_of_elements, list[number_of_elements - 1].time);
    LOG_ERR("Temperature is room %d is %d", list[number_of_elements - 1].room->room_id, list[number_of_elements - 1].temperature);
    k_sem_give(&new_data_sem);
    
    return 0;
}

int removeSchedule(struct TemperatureSchedule target) {
    int index = -1;

    for (int i = 0; i < number_of_elements; i++) {
        if (list[i].time == target.time && 
            list[i].temperature == target.temperature) {
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
