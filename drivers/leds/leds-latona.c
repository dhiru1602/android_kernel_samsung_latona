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

#include <plat/mux.h>

#define DRIVER_NAME "LatonaLedDriver"

static void latona_led_set(struct led_classdev *led_cdev,
			   enum led_brightness value)
{
	if (value == LED_OFF) {
		gpio_set_value(OMAP_GPIO_LED_EN1, 0);
		gpio_set_value(OMAP_GPIO_LED_EN2, 0);
	} else {
		gpio_set_value(OMAP_GPIO_LED_EN1, 1);
		gpio_set_value(OMAP_GPIO_LED_EN2, 1);
	}
}

static struct led_classdev latona_led = {
	.name		= "button-backlight",
	.brightness_set	= latona_led_set,
	.brightness	= LED_OFF,
};

static int latona_led_probe(struct platform_device *pdev)
{
	return led_classdev_register(&pdev->dev, &latona_led);
}

static int __devexit latona_led_remove(struct platform_device *pdev)
{
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
