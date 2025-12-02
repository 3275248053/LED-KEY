/*
 * 示例：按键切换三种 LED 模式（使用线程）
 *
 * 根据原理图：
 * LED1  - PF7
 * LED2  - PF8
 * LED3  - PE3
 * LED4  - PE2
 *
 * KEY1(Wakeup) - PA0  （上拉，按下接地）：普通流水灯模式
 * KEY2(Tamper) - PC13 （上拉，按下接地）：二进制计数点亮模式
 * KEY3(User)   - PB14 （上拉，按下接地）：关闭（全灭）
 */

#include <rtthread.h>
#include <rtdevice.h>
#include <board.h>

/* LED 引脚定义 */
#define LED1_PIN    GET_PIN(F, 7)
#define LED2_PIN    GET_PIN(F, 8)
#define LED3_PIN    GET_PIN(E, 3)
#define LED4_PIN    GET_PIN(E, 2)

/* 按键引脚定义（上拉输入，按下为低电平） */
#define KEY1_PIN    GET_PIN(A, 0)
#define KEY2_PIN    GET_PIN(C, 13)
#define KEY3_PIN    GET_PIN(B, 14)

/* 模式定义 */
enum
{
    LED_MODE_NONE = 0,      /* 全灭 */
    LED_MODE_FLOW,          /* 普通流水灯 */
    LED_MODE_BINARY,        /* 二进制计数显示 */
};

static volatile rt_uint8_t g_led_mode = LED_MODE_NONE;

/* 方便统一操作的 LED 引脚数组 */
static const rt_base_t led_pins[4] = { LED1_PIN, LED2_PIN, LED3_PIN, LED4_PIN };

/* 将所有 LED 关断 */
static void leds_all_off(void)
{
    for (int i = 0; i < 4; i++)
    {
        rt_pin_write(led_pins[i], PIN_LOW);
    }
}

/* 模式 1：普通流水灯 */
static void led_mode_flow_step(void)
{
    static int index = 0;

    leds_all_off();
    rt_pin_write(led_pins[index], PIN_HIGH);
    rt_thread_mdelay(150);

    index = (index + 1) % 4;
}

/* 模式 2：二进制计数显示 */
static void led_mode_binary_step(void)
{
    static rt_uint8_t value = 0;

    for (int i = 0; i < 4; i++)
    {
        if (value & (1 << i))
            rt_pin_write(led_pins[i], PIN_HIGH);
        else
            rt_pin_write(led_pins[i], PIN_LOW);
    }

    value++;
    rt_thread_mdelay(200);
}

/* LED 线程：根据当前模式刷灯 */
static void led_thread_entry(void *parameter)
{
    while (1)
    {
        rt_uint8_t mode = g_led_mode;

        switch (mode)
        {
        case LED_MODE_FLOW:
            led_mode_flow_step();
            break;
        case LED_MODE_BINARY:
            led_mode_binary_step();
            break;
        default:
            leds_all_off();
            rt_thread_mdelay(50);
            break;
        }
    }
}

/* 按键扫描线程：三个按键分别切换不同模式 */
static void key_thread_entry(void *parameter)
{
    int key1_last = 1, key2_last = 1, key3_last = 1;

    while (1)
    {
        int k1 = rt_pin_read(KEY1_PIN);
        int k2 = rt_pin_read(KEY2_PIN);
        int k3 = rt_pin_read(KEY3_PIN);

        /* 简单下降沿检测 + 软消抖 */
        if (key1_last == 1 && k1 == 0)
        {
            rt_thread_mdelay(20);
            if (rt_pin_read(KEY1_PIN) == 0)
            {
                g_led_mode = LED_MODE_FLOW;
                rt_kprintf("Mode: 普通流水灯\r\n");
            }
        }

        if (key2_last == 1 && k2 == 0)
        {
            rt_thread_mdelay(20);
            if (rt_pin_read(KEY2_PIN) == 0)
            {
                g_led_mode = LED_MODE_BINARY;
                rt_kprintf("Mode: 二进制计数\r\n");
            }
        }

        if (key3_last == 1 && k3 == 0)
        {
            rt_thread_mdelay(20);
            if (rt_pin_read(KEY3_PIN) == 0)
            {
                g_led_mode = LED_MODE_NONE;
                rt_kprintf("Mode: 全灭\r\n");
            }
        }

        key1_last = k1;
        key2_last = k2;
        key3_last = k3;

        rt_thread_mdelay(10);
    }
}

int main(void)
{
    /* 配置 LED 引脚为输出模式，默认熄灭 */
    rt_pin_mode(LED1_PIN, PIN_MODE_OUTPUT);
    rt_pin_mode(LED2_PIN, PIN_MODE_OUTPUT);
    rt_pin_mode(LED3_PIN, PIN_MODE_OUTPUT);
    rt_pin_mode(LED4_PIN, PIN_MODE_OUTPUT);

    leds_all_off();

    /* 配置按键引脚为输入模式（上拉） */
    rt_pin_mode(KEY1_PIN, PIN_MODE_INPUT_PULLUP);
    rt_pin_mode(KEY2_PIN, PIN_MODE_INPUT_PULLUP);
    rt_pin_mode(KEY3_PIN, PIN_MODE_INPUT_PULLUP);

    /* 创建 LED 线程 */
    rt_thread_t led_tid = rt_thread_create("led",
                                           led_thread_entry,
                                           RT_NULL,
                                           1024,
                                           5,
                                           10);
    if (led_tid != RT_NULL)
    {
        rt_thread_startup(led_tid);
    }

    /* 创建按键扫描线程 */
    rt_thread_t key_tid = rt_thread_create("key",
                                           key_thread_entry,
                                           RT_NULL,
                                           1024,
                                           6,
                                           10);
    if (key_tid != RT_NULL)
    {
        rt_thread_startup(key_tid);
    }

    return RT_EOK;
}

