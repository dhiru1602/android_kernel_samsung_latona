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

#define DRIVER_NAME "secLedDriver"

struct sec_led_data {
	struct led_classdev cdev;
	unsigned gpio1;
	unsigned gpio2;
//	struct work_struct work;
	u8 new_level;
	u8 can_sleep1;
	u8 can_sleep2;
//	u8 active_low;
//	int (*platform_gpio_blink_set)(unsigned gpio,
//			unsigned long *delay_on, unsigned long *delay_off);
};

static int led_state =0;
static int bln_state=0;

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
	if(bln_state==0)
	{
		gpio_set_value(OMAP_GPIO_LED_EN1, 0);
		gpio_set_value(OMAP_GPIO_LED_EN2, 0);
	}
	led_state = 0;
}

static void bl_set_timeout() {
	if (bl_timeout > 0) {
		mod_timer(&bl_timer, jiffies + msecs_to_jiffies(bl_timeout));
	}
}

void trigger_touchkey_led(int event)
{
	//event: 0-All Lights | 1-Menu Pressed | 2-Back Pressed	| 3- All Key Release
	if(bl_timeout!=0)
	{
		if((event==3)&&(led_state==0)) return; //Dont lightup if keys already turned off

		if((event==0)||(event==3))
		{
			gpio_set_value(OMAP_GPIO_LED_EN1, 1);
			gpio_set_value(OMAP_GPIO_LED_EN2, 1);
		}else if(event==1){
			gpio_set_value(OMAP_GPIO_LED_EN1, 0);
			gpio_set_value(OMAP_GPIO_LED_EN2, 1);
		}else if(event==2){
			gpio_set_value(OMAP_GPIO_LED_EN1, 1);
			gpio_set_value(OMAP_GPIO_LED_EN2, 0);
		}
		led_state = 1;
		bln_state = 0;
		bl_set_timeout();
	}
}

EXPORT_SYMBOL(trigger_touchkey_led);

void suspend_touchkey_led(void)
{
	if(bln_state==0)
	{
		gpio_set_value(OMAP_GPIO_LED_EN1, 0);
		gpio_set_value(OMAP_GPIO_LED_EN2, 0);
	}
	led_state = 0;
}

EXPORT_SYMBOL(suspend_touchkey_led);

static void sec_led_set(struct led_classdev *led_cdev,
	enum led_brightness value)
{
	struct sec_led_data *led_dat =
		container_of(led_cdev, struct sec_led_data, cdev);
    
	// printk("[LED] %s ::: value=%d , led_state=%d\n", __func__, value, bln_state);
	if ((value == LED_OFF)&&( bln_state==1))
	{
		if (led_state && bl_timer.expires < jiffies)
		{
			gpio_set_value(led_dat->gpio1, 0);
			gpio_set_value(led_dat->gpio2, 0);
			printk(KERN_DEBUG "[LED] %s ::: OFF \n", __func__);
		}
		bln_state = 0;
	}
	else if ((value != LED_OFF)&&(bln_state==0))
	{
		gpio_set_value(led_dat->gpio1, 1);
		gpio_set_value(led_dat->gpio2, 1);
		bln_state = 1;
		printk(KERN_DEBUG "[LED] %s ::: ON \n", __func__);
	}
	else
	{ 
		// printk("[LED] %s ::: same state ... \n", __func__);
		return ;
	}
}

static ssize_t bl_timeout_read(struct device *dev, struct device_attribute *attr, char *buf) {
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

static int sec_led_probe(struct platform_device *pdev)
{	

	struct led_platform_data *pdata = pdev->dev.platform_data;
	struct led_info *cur_led;
	struct sec_led_data *leds_data, *led_dat;
	
	int i, ret = 0;

	printk("[LED] %s +\n", __func__);

	if (!pdata)
		return -EBUSY;

	leds_data = kzalloc(sizeof(struct sec_led_data) * pdata->num_leds,
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
		led_dat->cdev.brightness_set = sec_led_set;
		led_dat->cdev.brightness = LED_OFF;

		ret = led_classdev_register(&pdev->dev, &led_dat->cdev);
		if (ret < 0) {
			goto err;
		}
	}

	platform_set_drvdata(pdev, leds_data);

	setup_timer(&bl_timer, bl_timer_callback, 0);

	if (misc_register(&bl_led_device))
		printk("%s misc_register(%s) failed\n", __FUNCTION__, bl_led_device.name);
	else {
		if (sysfs_create_group(&bl_led_device.this_device->kobj, &bl_led_group) < 0)
			pr_err("failed to create sysfs group for device %s\n", bl_led_device.name);
	}

	return 0;

err:
	if (i > 0) {
		for (i = i - 1; i >= 0; i--) {
			led_classdev_unregister(&leds_data[i].cdev);
//			cancel_work_sync(&leds_data[i].work);
		}
	}

	kfree(leds_data);

	return ret;
}

static int __devexit sec_led_remove(struct platform_device *pdev)
{
	int i;
//	struct gpio_led_platform_data *pdata = pdev->dev.platform_data;
//	struct gpio_led_data *leds_data;
	struct led_platform_data *pdata = pdev->dev.platform_data;
	struct sec_led_data *leds_data;

	leds_data = platform_get_drvdata(pdev);

	for (i = 0; i < pdata->num_leds; i++) {
		led_classdev_unregister(&leds_data[i].cdev);
//		cancel_work_sync(&leds_data[i].work);
	}	

	kfree(leds_data);

	return 0;
}

static int sec_led_shutdown(struct platform_device *pdev)
{
	gpio_direction_output(OMAP_GPIO_LED_EN1, 0);
	gpio_direction_output(OMAP_GPIO_LED_EN2, 0);
	del_timer(&bl_timer);

	return 0;
}

#if 0
static int sec_led_suspend(struct platform_device *dev, pm_message_t state)
{
	struct led_platform_data *pdata = dev->dev.platform_data;
	struct sec_led_data *leds_data;
	int i;
  
	printk("[LED] %s +\n", __func__);  
	printk("[LED] %s nuim_leds=%d \n", __func__, pdata->num_leds);
	for (i = 0; i < pdata->num_leds; i++) 
		led_classdev_suspend(&leds_data[i].cdev);

	return 0;
}

static int sec_led_resume(struct platform_device *dev)
{
	struct led_platform_data *pdata = dev->dev.platform_data;
	struct sec_led_data *leds_data;
	int i;

	printk("[LED] %s +\n", __func__);

	for (i = 0; i < pdata->num_leds; i++) 
		led_classdev_resume(&leds_data[i].cdev);

	return 0;
}
#endif

static struct platform_driver sec_led_driver = {
	.probe		= sec_led_probe,
	.remove		= __devexit_p(sec_led_remove),
	.shutdown	= sec_led_shutdown,
//	.suspend		= sec_led_suspend,
//	.resume		= sec_led_resume,
	.driver		= {
		.name	= DRIVER_NAME,
		.owner	= THIS_MODULE,
	},	
};

static int __init sec_led_init(void)
{
	printk("[LED] %s +\n", __func__);
	return platform_driver_register(&sec_led_driver);
}

static void __exit sec_led_exit(void)
{
	platform_driver_unregister(&sec_led_driver);
}

module_init(sec_led_init);
module_exit(sec_led_exit);

MODULE_DESCRIPTION("SAMSUNG LED driver");
MODULE_LICENSE("GPL");

