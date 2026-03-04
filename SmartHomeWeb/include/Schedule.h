#ifndef SCHEDULE_H
#define SCHEDULE_H

#include "Room.h"

#include <time.h>
#include <zephyr/drivers/rtc.h>
#include <zephyr/net/sntp.h>
#include <zephyr/net/socket.h>

extern struct k_sem new_data_sem;

void sync_rtc_with_network();

uint32_t get_seconds_today_from_rtc(void);

// Schedule

struct TemperatureSchedule* get_next_schedule();

uint32_t calculate_time_for_schedule(struct TemperatureSchedule* schedule);

int addNewSchedule(struct TemperatureSchedule schedule);

int removeSchedule(struct TemperatureSchedule schedule);

uint32_t to_seconds_today(struct rtc_time time);

#endif
