#include "Schedule.h"

LOG_MODULE_REGISTER(schedule, LOG_LEVEL_DBG);

// Getting Time and setting it RTC
static const struct device *rtc_dev = DEVICE_DT_GET(DT_NODELABEL(rtc));

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
    int current_time = get_seconds_today_from_rtc();
    
    struct TemperatureSchedule *best_today = NULL;
    struct TemperatureSchedule *earliest_tomorrow = NULL;

    for (int id = 0; id < STRUCT_ROOM_COUNT; id++) {
        struct Room *room = get_room_by_id(id);

        if (room->no_sched == 0) continue;

        for (int i = 0; i < room->no_sched; i++) {
            struct TemperatureSchedule *current_s = &room->list_of_schedules[i];

            // Track the earliest overall (in case we need to roll over to tomorrow)
            if (earliest_tomorrow == NULL || current_s->time < earliest_tomorrow->time) {
                earliest_tomorrow = current_s;
            }

            // Track the next one for today
            if (current_s->time > current_time) {
                if (best_today == NULL || current_s->time < best_today->time) {
                    best_today = current_s;
                }
            }
        }
    }

    return (best_today != NULL) ? best_today : earliest_tomorrow;
}

uint32_t calculate_time_for_schedule(struct TemperatureSchedule* schedule) {
    
    int current_time = get_seconds_today_from_rtc();
    int diff = schedule->time - current_time;
    LOG_DBG("%d - %d = %d", schedule->time, current_time, diff);

    if (diff <= 0) diff += 86400; // Wrap around logic

    return (uint32_t)diff;
}

int addNewSchedule(struct TemperatureSchedule schedule) {
    struct Room *room = schedule.parent_room;
    if (room->no_sched >= MAX_SCHEDULES_PER_ROOM) return -ENOSPC;
    room->list_of_schedules[room->no_sched] = schedule;
    room->no_sched++;
    LOG_DBG("Temperature in room %d is %d", room->room_id, room->list_of_schedules[room->no_sched - 1].temperature);
    LOG_DBG("Adding new schedule and notifing(nr of sched: %d), time %d", room->no_sched, room->list_of_schedules[room->no_sched - 1].time);
    k_sem_give(&new_data_sem);
    
    return 0;
}

int removeSchedule(struct TemperatureSchedule schedule) {

    for (int i = 0; i < STRUCT_ROOM_COUNT; i++) {
        struct Room *room = schedule.parent_room;

        if (room->no_sched == 0) return 0;
        struct TemperatureSchedule *list = room->list_of_schedules;

        int index = -1;

        for (int i = 0; i < room->no_sched; i++) {
            if (list[i].time == schedule.time && 
                list[i].temperature == schedule.temperature) {
                index = i;
                break;
            }
        }

        if (index == -1) {
            return -ENOENT;
        }
        
        for (int i = index; i < room->no_sched - 1; i++) {
            list[i] = list[i + 1];
        }

        room->no_sched--;

        k_sem_give(&new_data_sem);
    }

    return 0;
}
