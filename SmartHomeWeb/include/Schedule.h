#ifndef SCHEDULE_H
#define SCHEDULE_H

#include <time.h>
#include <zephyr/drivers/rtc.h>
#include <zephyr/net/sntp.h>
#include <zephyr/net/socket.h>

extern struct k_sem new_data_sem;

struct Room;

enum SCHED_TYPE {
    SETPOINT_SCH,
    LIGHT_SCH
};

struct Schedule {
    int time; // seconds from midnight
    int value;
    enum SCHED_TYPE type;
    struct Room *parent_room;
};

void sync_rtc_with_network();

uint32_t get_seconds_today_from_rtc(void);

// Schedule

struct Schedule* get_next_schedule();

uint32_t calculate_time_for_schedule(struct Schedule* schedule);

int addNewSchedule(struct Schedule schedule);

int removeSchedule(struct Schedule schedule);

uint32_t to_seconds_today(struct rtc_time time);

void execute_schedule(struct Schedule *schedule);

#endif
