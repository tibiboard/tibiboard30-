#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/init.h>
#include <zmk/battery.h>

/* XIAO BLEのLED定義 */
static const struct gpio_dt_spec led_red   = GPIO_DT_SPEC_GET(DT_ALIAS(led0), gpios);
static const struct gpio_dt_spec led_green = GPIO_DT_SPEC_GET(DT_ALIAS(led1), gpios);
static const struct gpio_dt_spec led_blue  = GPIO_DT_SPEC_GET(DT_ALIAS(led2), gpios);

/* 表示時間 (5秒) */
#define LED_SHOW_TIME 5000

static struct k_work_delayable led_off_work;
static struct k_work_delayable boot_show_work;
static struct k_work_delayable rainbow_work;

static uint8_t rainbow_step = 0;

/* LEDをすべて消す関数 (新しいAPIに変更) */
static void all_leds_off(void) {
    if (gpio_is_ready_dt(&led_red))   gpio_pin_set_dt(&led_red, 0);
    if (gpio_is_ready_dt(&led_green)) gpio_pin_set_dt(&led_green, 0);
    if (gpio_is_ready_dt(&led_blue))  gpio_pin_set_dt(&led_blue, 0);
}

/* 7色アニメーションを処理する関数 */
static void rainbow_handler(struct k_work *work) {
    all_leds_off();
    
    switch (rainbow_step % 7) {
        case 0: gpio_pin_set_dt(&led_red, 1); break;
        case 1: gpio_pin_set_dt(&led_red, 1); gpio_pin_set_dt(&led_green, 1); break;
        case 2: gpio_pin_set_dt(&led_green, 1); break;
        case 3: gpio_pin_set_dt(&led_green, 1); gpio_pin_set_dt(&led_blue, 1); break;
        case 4: gpio_pin_set_dt(&led_blue, 1); break;
        case 5: gpio_pin_set_dt(&led_red, 1); gpio_pin_set_dt(&led_blue, 1); break;
        case 6: gpio_pin_set_dt(&led_red, 1); gpio_pin_set_dt(&led_green, 1); gpio_pin_set_dt(&led_blue, 1); break;
    }
    
    rainbow_step++;
    k_work_schedule(&rainbow_work, K_MSEC(200));
}

static void led_off_handler(struct k_work *work) {
    k_work_cancel_delayable(&rainbow_work);
    all_leds_off();
}

/* メイン処理：バッテリー残量に応じて色を変える */
static void show_battery_level(void) {
    uint8_t level = zmk_battery_state_of_charge();
    
    k_work_cancel_delayable(&led_off_work);
    k_work_cancel_delayable(&rainbow_work);
    all_leds_off();

    /* 万が一バッテリーレベルが0（まだ読み取れていない）場合の安全策 */
    if (level == 0) {
        /* 白く光らせて「準備中」をアピール */
        gpio_pin_set_dt(&led_red, 1);
        gpio_pin_set_dt(&led_green, 1);
        gpio_pin_set_dt(&led_blue, 1);
    } else if (level >= 75) {
        rainbow_step = 0;
        k_work_schedule(&rainbow_work, K_NO_WAIT);
    } else if (level >= 50) {
        gpio_pin_set_dt(&led_blue, 1);
    } else if (level >= 25) {
        gpio_pin_set_dt(&led_red, 1);
        gpio_pin_set_dt(&led_green, 1);
    } else {
        gpio_pin_set_dt(&led_red, 1);
    }

    k_work_schedule(&led_off_work, K_MSEC(LED_SHOW_TIME));
}

static void boot_show_handler(struct k_work *work) {
    show_battery_level();
}

static int battery_led_init(void) {
    /* 最新のZephyrの推奨APIに変更 */
    if (!gpio_is_ready_dt(&led_red)) return -ENODEV;
    
    gpio_pin_configure_dt(&led_red, GPIO_OUTPUT_INACTIVE);
    gpio_pin_configure_dt(&led_green, GPIO_OUTPUT_INACTIVE);
    gpio_pin_configure_dt(&led_blue, GPIO_OUTPUT_INACTIVE);

    all_leds_off();
    
    k_work_init_delayable(&led_off_work, led_off_handler);
    k_work_init_delayable(&boot_show_work, boot_show_handler);
    k_work_init_delayable(&rainbow_work, rainbow_handler);

    /* ★バッテリー読み取りの準備が確実に終わるように3秒（3000ms）に延長★ */
    k_work_schedule(&boot_show_work, K_MSEC(3000));

    return 0;
}

SYS_INIT(battery_led_init, APPLICATION, CONFIG_APPLICATION_INIT_PRIORITY);
