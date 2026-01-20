#include "Room.h"

LOG_MODULE_REGISTER(room, LOG_LEVEL_DBG);

K_FIFO_DEFINE(events_fifo);
K_FIFO_DEFINE(web_events_fifo);

#define LED0_NODE DT_ALIAS(led0)
#define LED1_NODE DT_ALIAS(led1)
#define LED2_NODE DT_ALIAS(led2)

static const struct gpio_dt_spec leds[ROOM_LED_COUNT] = {
    GPIO_DT_SPEC_GET(LED0_NODE, gpios),
    GPIO_DT_SPEC_GET(LED1_NODE, gpios),
    GPIO_DT_SPEC_GET(LED2_NODE, gpios),
};

#define DHT0_ALIAS DT_ALIAS(dht0)
#define DHT1_ALIAS DT_ALIAS(dht1)

static const struct device *const dht_devices[] = {
    DEVICE_DT_GET(DHT0_ALIAS),
    DEVICE_DT_GET(DHT1_ALIAS),
};

static const struct pwm_dt_spec lr_pwdled = PWM_DT_SPEC_GET(DT_ALIAS(pwmlivingroom));
static const struct pwm_dt_spec kr_pwdled = PWM_DT_SPEC_GET(DT_ALIAS(pwmkitchen));

static const struct gpio_dt_spec lr_gpio_switch = GPIO_DT_SPEC_GET_OR(DT_ALIAS(switchlivingroom), gpios, {0});
static const struct gpio_dt_spec kr_gpio_switch = GPIO_DT_SPEC_GET_OR(DT_ALIAS(switchkitchen), gpios, {0});

static const struct gpio_dt_spec lr_gpio_relay_tmp = GPIO_DT_SPEC_GET_OR(DT_ALIAS(/*TODO*/), gpios, {0});
static const struct gpio_dt_spec kr_gpio_relay_tmp = GPIO_DT_SPEC_GET_OR(DT_ALIAS(/*TODO*/), gpios, {0});

/* RTIO devices for reading temperature/humidity channels */
SENSOR_DT_READ_IODEV(dht_iodev0,
                     DHT0_ALIAS,
                     { SENSOR_CHAN_AMBIENT_TEMP, 0 },
                     { SENSOR_CHAN_HUMIDITY, 0 });

SENSOR_DT_READ_IODEV(dht_iodev1,
                     DHT1_ALIAS,
                     { SENSOR_CHAN_AMBIENT_TEMP, 0 },
                     { SENSOR_CHAN_HUMIDITY, 0 });

RTIO_DEFINE(dht_ctx, 1, 1);


static struct Room lr_room  = { 
    .room_id = LIVINROOM_ROOM,
    .room_name = "Living Room",
    .light_switch = &lr_gpio_switch, 
    .light_gpio = &leds[ROOM_LED_ERROR], 
    .light_pwm = NULL, 
    .light_gpio_value = 0,
    .dht_devices = dht_devices[0],
    .dht_iodevs = &dht_iodev0,
    .temp_sensor_value = 22,
    .hum_sensor_value = 22,
    .desired_temperature = 22,
    .heat_relay = &lr_gpio_relay_tmp,
    .heat_relay_state = false,
    .offset_desired_temperature = 50
};
static struct Room kr_room = { 
    .room_id = KITCHEN_ROOM,
    .room_name = "Kitchen",
    .light_switch = &kr_gpio_switch, 
    .light_gpio = NULL, 
    .light_pwm = &kr_pwdled, 
    .light_gpio_value = 0,
    .dht_devices = dht_devices[1],
    .dht_iodevs = &dht_iodev1,
    .temp_sensor_value = 22,
    .hum_sensor_value = 22,
    .desired_temperature = 22,
    .heat_relay = &kr_gpio_relay_tmp,
    .heat_relay_state = false,
    .offset_desired_temperature = 50
};

static struct Room *rooms[STRUCT_ROOM_COUNT] = { 
    &lr_room, 
    &kr_room 
};

bool room_device_init() {
    int ret = 0;

    /* Leds init */
    for (int i = 0; i < ROOM_LED_COUNT; i++) {
        if (!gpio_is_ready_dt(&leds[i])) return false;
        gpio_pin_configure_dt(&leds[i], GPIO_OUTPUT_ACTIVE);
    }

    /* PWM signal for leds init */
    static const struct pwm_dt_spec pwd_lights[] = { lr_pwdled, kr_pwdled };
    for (size_t i = 0; i < ARRAY_SIZE(pwd_lights); i++) {

        if (!pwm_is_ready_dt(&pwd_lights[i])){
            LOG_ERR("PWM device not ready");
            return false;
        }
    }

    /* INPUT GPIO init */
    const struct gpio_dt_spec *input_gpio[] = { &lr_gpio_switch, &kr_gpio_switch};
    int number_of_switches = sizeof(input_gpio) / sizeof(struct gpio_dt_spec*);
    for (int i = 0; i < number_of_switches; i++) {
        if (!gpio_is_ready_dt(input_gpio[i])) return 0;
        ret = gpio_pin_configure_dt(input_gpio[i], GPIO_INPUT | input_gpio[i]->dt_flags);
        if (ret != 0) {
            LOG_ERR("Configuring OUTPUT GPIO pin failed: %d", ret);
            return false;
        }
    }

    /* OUTPUT GPIO init */
    // const struct gpio_dt_spec *output_gpio[] = { &lr_gpio_relay_tmp, &kr_gpio_relay_tmp};
    // int number_of_out = sizeof(output_gpio) / sizeof(struct gpio_dt_spec*);
    // for (int i = 0; i < number_of_out; i++) {
    //     if (!gpio_is_ready_dt(output_gpio[i])) return 0;
    //     ret = gpio_pin_configure_dt(output_gpio[i], GPIO_OUTPUT | output_gpio[i]->dt_flags);
    //     if (ret != 0) {
    //         LOG_ERR("Configuring OUTPUT GPIO pin failed: %d", ret);
    //         return false;
    //     }
    // }

    /* Temp sensor init */
    for (size_t i = 0; i < ARRAY_SIZE(dht_devices); i++) {
		if (!device_is_ready(dht_devices[i])) {
			printk("sensor: device %s not ready.\n", dht_devices[i]->name);
			return 0;
		}
	}

	LOG_INF("Initialization and configuration switch done.");
    return true;
}
 
void gpio_event_action(void *ctx, uint16_t value)
{
    const struct gpio_dt_spec *gpio = ctx;
    gpio_pin_set(gpio->port, gpio->pin, value);
}

void pwm_event_action(void *ctx, uint16_t value)
{
    const struct pwm_dt_spec *pwm = ctx;
    pwm_set_dt(pwm, pwm->period, value);
}

struct Room** get_all_rooms() {
    return rooms;
}

struct Room* get_room_by_id(int id) {
    return rooms[id];
}

const struct gpio_dt_spec* get_led_by_id(int id) {
    return &leds[id];
}

bool register_new_event(struct Room *room, uint16_t new_value, bool is_for_web_event) {
    struct Event *new_event = k_malloc(sizeof(struct Event));
    struct WebEvent *new_web_event = NULL;

    if (!new_event) {
        LOG_ERR("Unable to allocate memory for event");
        return false;
    }

    if (is_for_web_event) {
        new_web_event = k_malloc(sizeof(struct WebEvent));
        if (!new_web_event) {
            LOG_ERR("Unable to allocate memory for web event(LIGHT)");
            k_free(new_event);
            return false;
        }
    }


    /* Register light events */
    if (room->light_gpio != NULL) {
        new_event->action = gpio_event_action;
        new_event->ctx = (void *)room->light_gpio;
        new_event->value = new_value ? 1 : 0;
    }
    if (room->light_pwm != NULL) {
        new_event->action = pwm_event_action;
        new_event->ctx = (void *)room->light_pwm;
        new_event->value = new_value;
    }
    if (!is_for_web_event) {
        k_free(new_web_event);
        new_web_event = NULL;
    } else {
        new_web_event->room_id = room->room_id;
        new_web_event->value_type = LIGHT_EV;
        new_web_event->value = new_value ? 1 : 0;
    }

    k_fifo_put(&events_fifo, new_event);
    if (is_for_web_event) {
        k_fifo_put(&web_events_fifo, new_web_event);
    }
    return true;
}

bool register_new_temp_hum_event(struct Room *room, uint32_t temp_value, uint32_t hum_value, bool is_for_web_event) {
    if (!is_for_web_event) {
        return true;
    }
    
    struct WebEvent *new_web_event_temp = k_malloc(sizeof(struct WebEvent));
    if (!new_web_event_temp) {
        LOG_ERR("Unable to allocate memory for web event(TEMP)");
        return false;
    }
    
    struct WebEvent *new_web_event_hum = k_malloc(sizeof(struct WebEvent));
    if (!new_web_event_hum) {
        LOG_ERR("Unable to allocate memory for web event(HUM)");
        k_free(new_web_event_temp);
        return false;
    }

    new_web_event_temp->room_id = room->room_id;
    new_web_event_temp->value_type = HEAT_EV;
    new_web_event_temp->value = temp_value;

    new_web_event_hum->room_id = room->room_id;
    new_web_event_hum->value_type = HUM_EV;
    new_web_event_hum->value = hum_value;

    k_fifo_put(&web_events_fifo, new_web_event_temp);
    k_fifo_put(&web_events_fifo, new_web_event_hum);
    
    return true;
}

// For testing purposes only, simulating temperature and humidity readings
static int x = 4;
int read_temp_and_hum(struct Room *room, uint32_t* temp_fit, uint32_t* hum_fit) {

    *temp_fit = 20 + x -1;
    *hum_fit = 20  + x;
    x++;
    return 0;

    int rc;

    struct device *dev = (struct device *) room->dht_devices;

    uint8_t buf[128];

    rc = sensor_read(room->dht_iodevs, &dht_ctx, buf, 128);

    if (rc != 0) {
        LOG_WRN("%s: sensor_read() failed: %d\n", dev->name, rc);
        return rc;
    }

    const struct sensor_decoder_api *decoder;

    rc = sensor_get_decoder(dev, &decoder);

    if (rc != 0) {
        LOG_WRN("%s: sensor_get_decode() failed: %d\n", dev->name, rc);
        return rc;
    }

    *temp_fit = 0;
    struct sensor_q31_data temp_data = {0};

    decoder->decode(buf,
            (struct sensor_chan_spec) {SENSOR_CHAN_AMBIENT_TEMP, 0},
            temp_fit, 1, &temp_data);

    *hum_fit = 0;
    struct sensor_q31_data hum_data = {0};

    decoder->decode(buf,
            (struct sensor_chan_spec) {SENSOR_CHAN_HUMIDITY, 0},
            hum_fit, 1, &hum_data);
    return 0;
}


