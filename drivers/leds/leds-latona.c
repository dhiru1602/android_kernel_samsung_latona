#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/leds.h>
#include <linux/device.h>
#include <linux/platform_device.h>
#include <linux/err.h>
#include <linux/workqueue.h>
#include <linux/i2c/twl.h>
#include <linux/slab.h>
#include <linux/miscdevice.h>
#include <plat/mux.h>
#include <mach/gpio.h>
#include <linux/input.h>

#define DRIVER_NAME "LatonaLedDriver"
//Touch Keys State
#define TOUCHKEY_ALL 0
#define TOUCHKEY_MENU 1
#define TOUCHKEY_BACK 2
#define TOUCHKEY_NONE 3

struct latona_led_data {
	struct led_classdev cdev;
	unsigned gpio1;
	unsigned gpio2;
	u8 new_level;
	u8 can_sleep1;
	u8 can_sleep2;
};

static int bln_state = 0;
static int pressing = 0;

static int bl_timeout = 1600;
static struct timer_list bl_timer;
static void bl_off(struct work_struct *bl_off_work);
static DECLARE_WORK(bl_off_work, bl_off);

void bl_timer_callback(unsigned long data)
{
	schedule_work(&bl_off_work);
}

static void bl_off(struct work_struct *bl_off_work)
{
	if (bln_state == 0 && pressing == 0) {
		gpio_set_value(OMAP_GPIO_LED_EN1, 0);
		gpio_set_value(OMAP_GPIO_LED_EN2, 0);
	}
}

static void bl_set_timeout(void)
{
	if (bl_timeout > 0) {
		mod_timer(&bl_timer, jiffies + msecs_to_jiffies(bl_timeout));
	}
}

static void trigger_touchkey_led(int event)
{
	if (bl_timeout != 0) {
		pressing = 0;
		switch (event) {
		case TOUCHKEY_ALL:
			gpio_set_value(OMAP_GPIO_LED_EN1, 1);
			gpio_set_value(OMAP_GPIO_LED_EN2, 1);
			bl_set_timeout();
			break;
		case TOUCHKEY_MENU:
		case TOUCHKEY_BACK:
			pressing = 1;
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

void suspend_touchkey_led(void)
{
	cancel_work_sync(&bl_off_work);
	del_timer(&bl_timer);
	if (bln_state == 0) {
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
	}else if (key_code == KEY_MENU) {
		if (value)
			trigger_touchkey_led(TOUCHKEY_MENU);
		else
			trigger_touchkey_led(TOUCHKEY_NONE);
	}else if (key_code == KEY_BACK) {
		if (value)
			trigger_touchkey_led(TOUCHKEY_BACK);
		else
			trigger_touchkey_led(TOUCHKEY_NONE);
	}
	return;
}

static void latona_led_set(struct led_classdev *led_cdev,
	enum led_brightness value)
{
	struct latona_led_data *led_dat =
		container_of(led_cdev, struct latona_led_data, cdev);

	if (value == LED_OFF) {
		gpio_set_value(led_dat->gpio1, 0);
		gpio_set_value(led_dat->gpio2, 0);
		bln_state = 0;
	} else {
		gpio_set_value(led_dat->gpio1, 1);
		gpio_set_value(led_dat->gpio2, 1);
		bln_state = 1;
	}
}

static ssize_t bl_timeout_read(struct device *dev, struct device_attribute *attr, char *buf)
{
	return sprintf(buf,"%d\n", bl_timeout);
}

static ssize_t bl_timeout_write(struct device *dev, struct device_attribute *attr, const char *buf, size_t size)
{
	sscanf(buf, "%d\n", &bl_timeout);
	return size;
}

static DEVICE_ATTR(bl_timeout, S_IRUGO | S_IWUGO, bl_timeout_read, bl_timeout_write);

static struct attribute *bl_led_attributes[] = {
	&dev_attr_bl_timeout.attr,
	NULL
};

static struct attribute_group bl_led_group = {
	.attrs = bl_led_attributes,
};

static struct miscdevice bl_led_device = {
	.minor = MISC_DYNAMIC_MINOR,
	.name = "notification",
};

static int latona_led_probe(struct platform_device *pdev)
{
	struct led_platform_data *pdata = pdev->dev.platform_data;
	struct led_info *cur_led;
	struct latona_led_data *leds_data, *led_dat;
	
	int i, ret = 0;

	pr_debug("%s\n", __func__);

	if (!pdata)
		return -EBUSY;

	leds_data = kzalloc(sizeof(struct latona_led_data) * pdata->num_leds,
				GFP_KERNEL);
	if (!leds_data)
		return -ENOMEM;

	for (i = 0; i < pdata->num_leds; i++) {
		cur_led = &pdata->leds[i];
		led_dat = &leds_data[i];
		led_dat->gpio1 = OMAP_GPIO_LED_EN1;
		led_dat->gpio2 = OMAP_GPIO_LED_EN2;
		led_dat->can_sleep1 = gpio_cansleep(OMAP_GPIO_LED_EN1);
		led_dat->can_sleep2 = gpio_cansleep(OMAP_GPIO_LED_EN2);
		gpio_direction_output(OMAP_GPIO_LED_EN1, 0);
		gpio_direction_output(OMAP_GPIO_LED_EN2, 0);

		led_dat->cdev.name = cur_led->name;
		led_dat->cdev.default_trigger = cur_led->default_trigger;
		led_dat->cdev.brightness_set = latona_led_set;
		led_dat->cdev.brightness = LED_OFF;

		ret = led_classdev_register(&pdev->dev, &led_dat->cdev);
		if (ret < 0) {
			goto err;
		}
	}

	platform_set_drvdata(pdev, leds_data);

	setup_timer(&bl_timer, bl_timer_callback, 0);

	if (misc_register(&bl_led_device))
		pr_err("%s: misc_register(%s) failed\n", __FUNCTION__, bl_led_device.name);
	else {
		if (sysfs_create_group(&bl_led_device.this_device->kobj, &bl_led_group) < 0)
			pr_err("%s: failed to create sysfs group for device %s\n", __func__, bl_led_device.name);
	}

	return 0;

err:
	if (i > 0) {
		for (i = i - 1; i >= 0; i--) {
			led_classdev_unregister(&leds_data[i].cdev);
		}
	}

	kfree(leds_data);

	return ret;
}

static int __devexit latona_led_remove(struct platform_device *pdev)
{
	int i;
	struct led_platform_data *pdata = pdev->dev.platform_data;
	struct latona_led_data *leds_data;

	leds_data = platform_get_drvdata(pdev);

	for (i = 0; i < pdata->num_leds; i++) {
		led_classdev_unregister(&leds_data[i].cdev);
	}	

	kfree(leds_data);

	return 0;
}

static void latona_led_shutdown(struct platform_device *pdev)
{
	gpio_direction_output(OMAP_GPIO_LED_EN1, 0);
	gpio_direction_output(OMAP_GPIO_LED_EN2, 0);
	del_timer(&bl_timer);
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
	pr_debug("%s", __func__);
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
