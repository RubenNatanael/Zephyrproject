#include <zephyr/kernel.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/drivers/pwm.h>

#define SLEEP_TIME_MS 200
#define LED0_NODE DT_ALIAS(led0)
#define LED1_NODE DT_ALIAS(led1)
#define LED2_NODE DT_ALIAS(led2)


static const struct gpio_dt_spec power_led = GPIO_DT_SPEC_GET(LED0_NODE, gpios);
static const struct gpio_dt_spec power_led_info = GPIO_DT_SPEC_GET(LED1_NODE, gpios);
static const struct gpio_dt_spec power_led_error = GPIO_DT_SPEC_GET(LED2_NODE, gpios);

static const struct pwm_dt_spec lr_pwdled = PWM_DT_SPEC_GET(DT_ALIAS(pwmlivingroom));
static const struct pwm_dt_spec kr_pwdled = PWM_DT_SPEC_GET(DT_ALIAS(pwmkitchen));

static const struct gpio_dt_spec lr_switch = GPIO_DT_SPEC_GET_OR(DT_ALIAS(switchlivingroom), gpios, {0});
static const struct gpio_dt_spec kr_switch = GPIO_DT_SPEC_GET_OR(DT_ALIAS(switchkitchen), gpios, {0});

static uint32_t light_lr = 0;
static uint32_t light_kt = 0;


uint32_t getSwitchValue(struct gpio_dt_spec *sw);
void log_msg(const char* fmt, ...);

int main(void)
{
    printk("boot\n");
    int ret;
    bool led_state = true;

    if (!gpio_is_ready_dt(&power_led)) {
        return 0;
    }

    ret = gpio_pin_configure_dt(&power_led, GPIO_OUTPUT_ACTIVE);
    if (ret < 0) {
        log_msg("Error while configuring the led\n");
        return -1;
    }

    if (!gpio_is_ready_dt(&power_led_info)) {
        return 0;
    }

    ret = gpio_pin_configure_dt(&power_led_info, GPIO_OUTPUT_ACTIVE);
    if (ret < 0) {
        log_msg("Error while configuring the led\n");
        return -1;
    }

    if (!gpio_is_ready_dt(&power_led_error)) {
        return 0;
    }

    ret = gpio_pin_configure_dt(&power_led_error, GPIO_OUTPUT_ACTIVE);
    if (ret < 0) {
        log_msg("Error while configuring the led\n");
        return -1;
    }


    ret = gpio_pin_toggle_dt(&power_led);
    k_msleep(500);
    ret = gpio_pin_toggle_dt(&power_led);
    k_msleep(500);
    ret = gpio_pin_toggle_dt(&power_led);
    k_msleep(500);
    ret = gpio_pin_toggle_dt(&power_led);
    k_msleep(500);
    ret = gpio_pin_toggle_dt(&power_led);
    k_msleep(500);

    /*
    Led settup
    */
   if (!device_is_ready(lr_pwdled.dev)) {
        log_msg("LivingRoom led not ready\n");
        return 0;
    }
    if (!device_is_ready(kr_pwdled.dev)) {
        log_msg("Kitchen led not ready\n");
        return 0;
    }
	log_msg("Initialization light done.\n");

    /*
    Switch settup
    */
   if (!gpio_is_ready_dt(&lr_switch)) {
        printk("The Livin switch pin GPIO port is not ready.\n");
        return;
    }
    ret = gpio_pin_configure_dt(&lr_switch, GPIO_INPUT | lr_switch.dt_flags);
    if (ret != 0) {
        printk("Configuring Living GPIO pin failed: %d\n", ret);
        return;
    }
    if (!gpio_is_ready_dt(&kr_switch)) {
        printk("The Kit switch pin GPIO port is not ready.\n");
        return;
    }
    ret = gpio_pin_configure_dt(&kr_switch, GPIO_INPUT | kr_switch.dt_flags);
    if (ret != 0) {
        printk("Configuring kit GPIO pin failed: %d\n", ret);
        return;
    }

	log_msg("Initialization and configuration switch done.\n");

    int i = 0;

    while (1) {

        // ON LED code
        ret = gpio_pin_toggle_dt(&power_led);
        if (ret < 0) {
            return -1;
        }
        led_state = !led_state;
        log_msg("State of LED changed to: %s\n", led_state ? "ON" : "OFF");
        // ON LED code end
        if (i % 2 == 0)
            ret = gpio_pin_toggle_dt(&power_led_info);
        if (i++ % 4 == 0)
            ret = gpio_pin_toggle_dt(&power_led_error);


        uint32_t light_procentage_lr = getSwitchValue(&lr_switch);
        uint32_t light_procentage_kt = getSwitchValue(&kr_switch);
        pwm_set_dt(&lr_pwdled, lr_pwdled.period, light_procentage_lr);
        pwm_set_dt(&kr_pwdled, kr_pwdled.period, light_procentage_kt);



        k_msleep(SLEEP_TIME_MS);
    }

    return 0;
    
}

uint32_t getSwitchValue(struct gpio_dt_spec *sw) {
    uint16_t procentage = 50; // getOnlineState(); // request procentage of light from app or local,
                              // now the value is hardcoded to 50%
    if (gpio_pin_get_dt(sw)) {
        return lr_pwdled.period * procentage / 100;
    } else {
        return 0;
    }
}

void log_msg(const char* fmt, ...) {
    /*
    Use this function to log also in a file or a circular buffer or a serial terminal/device,...
    */
    va_list args;
    va_start(args, fmt);
    vprintk(fmt, args);
    va_end(args);
}
