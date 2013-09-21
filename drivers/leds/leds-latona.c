/*
 * Latona LED driver
 *
 * Copyright (C) 2013 The CyanogenMod Project
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#include <linux/leds.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/gpio.h>
#include <linux/input.h>
#include <linux/timer.h>

#include <plat/mux.h>

#define DRIVER_NAME "LatonaLedDriver"

#ifdef CONFIG_LEDS_LATONA_BACKLIGHT_TIMEOUT
//Touch Keys State
#define TOUCHKEY_ALL 0
#define TOUCHKEY_MENU 1
#define TOUCHKEY_BACK 2
#define TOUCHKEY_NONE 3

static int notification = 0;
static int bl_timeout = 1600;
static struct timer_list bl_timer;
static void bl_off(struct work_struct *bl_off_work);
static DECLARE_WORK(bl_off_work, bl_off);

static void bl_timer_callback(unsigned long data)
{
	schedule_work(&bl_off_work);
}

static void bl_off(struct work_struct *bl_off_work)
{
	gpio_set_value(OMAP_GPIO_LED_EN1, 0);
	gpio_set_value(OMAP_GPIO_LED_EN2, 0);
}

static void bl_set_timeout(void)
{
	if (bl_timeout > 0)
		mod_timer(&bl_timer, jiffies + msecs_to_jiffies(bl_timeout));
} 

static void trigger_touchkey_led(int event)
{
	if (bl_timeout != 0) {
		switch (event) {
		case TOUCHKEY_ALL:
			gpio_set_value(OMAP_GPIO_LED_EN1, 1);
			gpio_set_value(OMAP_GPIO_LED_EN2, 1);
			bl_set_timeout();
			break;
		case TOUCHKEY_MENU:
		case TOUCHKEY_BACK:
			// Keep LEDs on while pressing
			del_timer(&bl_timer);
			gpio_set_value(OMAP_GPIO_LED_EN1, 1);
			gpio_set_value(OMAP_GPIO_LED_EN2, 1);
			break;
		case TOUCHKEY_NONE:
			bl_set_timeout();
			break;
		default:
			pr_err("%s: unknown event: %d\n", __func__, event);
			break;
		}
	}
}

static void suspend_touchkey_led(void)
{
	// Make sure that bl_off() isn't be executed after this
	del_timer(&bl_timer);
	if (notification) {
		gpio_set_value(OMAP_GPIO_LED_EN1, 1);
		gpio_set_value(OMAP_GPIO_LED_EN2, 1);
	} else {
		gpio_set_value(OMAP_GPIO_LED_EN1, 0);
		gpio_set_value(OMAP_GPIO_LED_EN2, 0);
	}
}

void latona_leds_report_event(int key_code, int value)
{
	if (key_code == KEY_POWER) {
		if (value)
			trigger_touchkey_led(TOUCHKEY_ALL);
		else
			suspend_touchkey_led();
	} else if (key_code == KEY_MENU) {
		if (value)
			trigger_touchkey_led(TOUCHKEY_MENU);
		else
			trigger_touchkey_led(TOUCHKEY_NONE);
	} else if (key_code == KEY_BACK) {
		if (value)
			trigger_touchkey_led(TOUCHKEY_BACK);
		else
			trigger_touchkey_led(TOUCHKEY_NONE);
	}
	return;
}

static ssize_t bl_timeout_read(struct device *dev,
			       struct device_attribute *attr, char *buf)
{
	return sprintf(buf,"%d\n", bl_timeout);
}

static ssize_t bl_timeout_write(struct device *dev,
				struct device_attribute *attr,
				const char *buf, size_t size)
{
	sscanf(buf, "%d\n", &bl_timeout);
	if (bl_timeout == 0) {
		gpio_set_value(OMAP_GPIO_LED_EN1, 0);
		gpio_set_value(OMAP_GPIO_LED_EN2, 0);
	}
	return size;
}

static DEVICE_ATTR(bl_timeout, S_IRUGO | S_IWUSR | S_IWGRP,
		   bl_timeout_read, bl_timeout_write);
#endif

static void latona_led_set(struct led_classdev *led_cdev,
			   enum led_brightness value)
{
	/* 
	 * This will be used only for LED notifications.
	 * Set a flag to keep LEDs on even when the screen
	 * is turned off.
	 */
	if (value == LED_OFF) {
		gpio_set_value(OMAP_GPIO_LED_EN1, 0);
		gpio_set_value(OMAP_GPIO_LED_EN2, 0);
#ifdef CONFIG_LEDS_LATONA_BACKLIGHT_TIMEOUT
		notification = 0;
#endif
	} else {
		gpio_set_value(OMAP_GPIO_LED_EN1, 1);
		gpio_set_value(OMAP_GPIO_LED_EN2, 1);
#ifdef CONFIG_LEDS_LATONA_BACKLIGHT_TIMEOUT
		notification = 1;
#endif
	}
}

static struct led_classdev latona_led = {
	.name		= "button-backlight",
	.brightness_set	= latona_led_set,
	.brightness	= LED_OFF,
};

static int latona_led_probe(struct platform_device *pdev)
{
	int ret;

	ret = led_classdev_register(&pdev->dev, &latona_led);
	if (ret < 0) {
		pr_err("%s: Failed to register LED\n", __func__);
		return ret;
	}

#ifdef CONFIG_LEDS_LATONA_BACKLIGHT_TIMEOUT
	ret = device_create_file(latona_led.dev, &dev_attr_bl_timeout);
	if (ret < 0) {
		pr_err("%s: Failed to create device file %s\n",
				__func__, dev_attr_bl_timeout.attr.name);
		led_classdev_unregister(&latona_led);
		return ret;
	}
	setup_timer(&bl_timer, bl_timer_callback, 0);
#endif

	return ret;
}

static int __devexit latona_led_remove(struct platform_device *pdev)
{
#ifdef CONFIG_LEDS_LATONA_BACKLIGHT_TIMEOUT
	del_timer(&bl_timer);
#endif
	led_classdev_unregister(&latona_led);
	return 0;
}

static void latona_led_shutdown(struct platform_device *pdev)
{
	gpio_direction_output(OMAP_GPIO_LED_EN1, 0);
	gpio_direction_output(OMAP_GPIO_LED_EN2, 0);
}

static struct platform_driver latona_led_driver = {
	.probe		= latona_led_probe,
	.remove		= __devexit_p(latona_led_remove),
	.shutdown	= latona_led_shutdown,
	.driver		= {
		.name	= DRIVER_NAME,
		.owner	= THIS_MODULE,
	},
};

static int __init latona_led_init(void)
{
	return platform_driver_register(&latona_led_driver);
}

static void __exit latona_led_exit(void)
{
	platform_driver_unregister(&latona_led_driver);
}

module_init(latona_led_init);
module_exit(latona_led_exit);

MODULE_DESCRIPTION("LATONA LED driver");
MODULE_LICENSE("GPL");
