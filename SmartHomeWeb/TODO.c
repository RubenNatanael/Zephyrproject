
struct k_sem new_data_sem;

struct TemperatureSchedule {
    struct Room *room;
    int time; // minutes from midnight
    int temperature; // scale  *100
}
#define MAX_SCHEDULES 15
struct TemperatureSchedule list[MAX_SCHEDULES];
uint8_t number_of_elements = 0;
uint8_t currentEventIndex = 0;
int current_time; // ?????

void thread() {
    while(1) {
        uint32_t sleep_ms = calculate_next_event_time();
        
        int ret = k_sem_take(&new_data_sem, K_MSEC(sleep_ms));

        if (ret == -EAGAIN) {
            struct TemperatureSchedule *s = &list[currentEventIndex];
            if (s->room) {
                s->room->desired_temperature = s->temperature;
            }
        } 
        // If ret == 0: Just restart the loop to recalculate with new data
    }
}

uint32_t calculate_next_event_time() {
    if (number_of_elements == 0) return K_FOREVER;

    int next_idx = -1;
    int min_day_idx = 0; // Smallest time overall (for tomorrow)

    for (int i = 0; i < number_of_elements; i++) {
        // Track the absolute earliest event in the list (in case we need to wrap to tomorrow)
        if (list[i].time < list[min_day_idx].time) {
            min_day_idx = i;
        }

        // Find the smallest time that is GREATER than current_time
        if (list[i].time > current_time) {
            if (next_idx == -1 || list[i].time < list[next_idx].time) {
                next_idx = i;
            }
        }
    }

    // If no future event today, use the earliest event of the next day
    currentEventIndex = (next_idx != -1) ? next_idx : min_day_idx;

    struct TemperatureSchedule *next = &list[currentEventIndex];
    int diff = next->time - current_time;

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