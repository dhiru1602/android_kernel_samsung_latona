/*
 * ZEUS BUTTON KEY DRIVER - HOME / POWER / HEADSET KEYS
 *
 * Dheeraj CVR (dhiru1602) <cvr.dheeraj@gmail.com>
 * 
 * This file is subject to the terms and conditions of the GNU General
 * Public License. See the file "COPYING" in the main directory of this
 * archive for more details.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */


#include <linux/module.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/errno.h>
#include <linux/interrupt.h>
#include <linux/platform_device.h>
#include <linux/input.h>
#include <linux/jiffies.h>
#include <linux/timer.h>
#include <plat/gpio.h>
#include <plat/hardware.h>
#include <plat/mux.h>
#include <linux/switch.h>
#include <linux/i2c/twl4030-madc.h>
#include <linux/i2c/twl.h>
#include <linux/delay.h>
#include <linux/wakelock.h>
#include <plat/mux.h>
#include <mach/board-latona.h>

/* Debug */

#define ZEUS_DEBUG 0

#if ZEUS_DEBUG
#define HEADSET_DBG(fmt, arg...) printk(KERN_INFO "[JACKKEY] %s" fmt "\r\n", __func__,## arg)
#else
#define HEADSET_DBG(fmt, arg...) do {} while(0)
#endif

#define KEYCODE_SENDEND 226

#define SEND_END_CHECK_COUNT	2
#define SEND_END_CHECK_TIME get_jiffies_64() + (HZ/11)// 1000ms / 11 = 90ms
#define WAKE_LOCK_TIME (HZ*1)// 1000ms  = 1sec

static int __devinit zeus_key_driver_probe(struct platform_device *plat_dev);
static irqreturn_t powerkey_press_handler(int irq_num, void * dev);
static irqreturn_t homekey_press_handler(int irq_num, void * dev);
int home_key_press_status = 0;
int last_home_key_press_status = 0;
static struct timespec home_key_up_time = {0, 0};

struct work_struct  lcd_work;
static struct workqueue_struct *lcd_wq=NULL;

struct switch_dev switch_sendend = {
        .name = "send_end",
};

static irqreturn_t earkey_press_handler(int irq_num, void * dev);

static int get_adc_data( int ch )
{
	int ret = 0;
	struct twl4030_madc_request req;
	HEADSET_DBG("  ");

	#ifdef USE_ADC_SEL_GPIO
	gpio_direction_output(EAR_ADC_SEL_GPIO , 0);
	#endif
    
	req.channels = ( 1 << ch );
	req.do_avg = 0;
	req.method = TWL4030_MADC_SW1;
	req.active = 0;
	req.func_cb = NULL;
	twl4030_madc_conversion( &req );

	ret = req.rbuf[ch];
	//printk("ear key adc value is : %d\n", ret);

	#ifdef USE_ADC_SEL_GPIO
	gpio_direction_output(EAR_ADC_SEL_GPIO , 1);
	#endif

	return ret;
}


static struct timer_list send_end_key_event_timer;
static unsigned int send_end_key_timer_token;
static unsigned int send_end_irq_token;
static unsigned int earkey_stats = 0;

struct input_dev *ip_device = NULL;

struct wake_lock earkey_wakelock;

static void release_sysfs_event(struct work_struct *work)
{
	int adc_value = get_adc_data(2);
	
	HEADSET_DBG(" %d ", earkey_stats);
		
	if(earkey_stats)
	{
		if(adc_value > 1)
		{
			wake_lock( &earkey_wakelock);
			switch_set_state(&switch_sendend, 1);
			input_report_key(ip_device,KEYCODE_SENDEND,1);
		  	input_sync(ip_device);
		}
		else
		{
			printk("adc is too low, ignore ear key event %d\n", adc_value);
		}
	}
	else
	{	
		switch_set_state(&switch_sendend, 0);
		wake_unlock( &earkey_wakelock);
		wake_lock_timeout(&earkey_wakelock, WAKE_LOCK_TIME);
	}
}
static DECLARE_DELAYED_WORK(release_sysfs_event_work, release_sysfs_event);

void ear_key_disable_irq(void)
{
	if(send_end_irq_token > 0)
	{
		printk("[EAR_KEY]ear key disable\n");
		earkey_stats = 0;
		schedule_delayed_work(&release_sysfs_event_work, 0);
		//switch_set_state(&switch_sendend, 0);
		send_end_irq_token--;
	}
}
EXPORT_SYMBOL_GPL(ear_key_disable_irq);

void ear_key_enable_irq(void)
{
	if(send_end_irq_token == 0)
	{
		printk("[EAR_KEY]ear key enable\n");
		send_end_irq_token++;
	}
}
EXPORT_SYMBOL_GPL(ear_key_enable_irq);

static void send_end_key_event_timer_handler(unsigned long arg)
{
	int sendend_state = 0;

	sendend_state = gpio_get_value(EAR_KEY_GPIO) ^ EAR_KEY_INVERT_ENABLE;

	if((get_headset_status() == HEADSET_4POLE_WITH_MIC) && sendend_state)
	{
		if(send_end_key_timer_token < SEND_END_CHECK_COUNT)
		{	
			send_end_key_timer_token++;
			send_end_key_event_timer.expires = SEND_END_CHECK_TIME; 
			add_timer(&send_end_key_event_timer);
			HEADSET_DBG("SendEnd Timer Restart %d", send_end_key_timer_token);
		}
		else if((send_end_key_timer_token == SEND_END_CHECK_COUNT) && send_end_irq_token)
		{
			printk("SEND/END is pressed\n");
			earkey_stats = 1;
			schedule_delayed_work(&release_sysfs_event_work, 0);
			send_end_key_timer_token = 0;
		}
		else
			printk(KERN_ALERT "[Headset]wrong timer counter %d\n", send_end_key_timer_token);
	}
	else
	{		
		printk(KERN_ALERT "[Headset]GPIO Error\n %d, %d", get_headset_status(), sendend_state);
	}
}

static void ear_switch_change(struct work_struct *ignored)
{
  	int state;
	HEADSET_DBG("");
	
	if(!ip_device){
    		dev_err(ip_device->dev.parent,"Input Device not allocated\n");
		return;
  	}
  
  	state = gpio_get_value(EAR_KEY_GPIO) ^ EAR_KEY_INVERT_ENABLE;
  	if( state < 0 ){
 	   	dev_err(ip_device->dev.parent,"Failed to read GPIO value\n");
		return;
  	}

	del_timer(&send_end_key_event_timer);
	send_end_key_timer_token = 0;	

	//printk("ear key %d", state);
	if((get_headset_status() == HEADSET_4POLE_WITH_MIC) && send_end_irq_token)//  4 pole headset connected && send irq enable
	{
		if(state)
		{
			//wake_lock(&headset_sendend_wake_lock);
			send_end_key_event_timer.expires = SEND_END_CHECK_TIME; // 10ms ??
			add_timer(&send_end_key_event_timer);		
			HEADSET_DBG("SEND/END %s.timer start \n", "pressed");
			
		}else{
			HEADSET_DBG(KERN_ERR "SISO:sendend isr work queue\n");    			
		 	input_report_key(ip_device,KEYCODE_SENDEND,0);
  			input_sync(ip_device);
			printk("SEND/END %s.\n", "released");
			earkey_stats = 0;
			switch_set_state(&switch_sendend, 0);
			wake_unlock( &earkey_wakelock);
			wake_lock_timeout(&earkey_wakelock, WAKE_LOCK_TIME);
			//schedule_delayed_work(&release_sysfs_event_work, 10);
			//wake_unlock(&headset_sendend_wake_lock);			
		}

	}else{
		HEADSET_DBG("SEND/END Button is %s but headset disconnect or irq disable.\n", state?"pressed":"released");
	}

}
static DECLARE_WORK(ear_switch_work, ear_switch_change);

static irqreturn_t earkey_press_handler(int irq_num, void * dev)
{
	HEADSET_DBG("earkey isr");
	schedule_work(&ear_switch_work);
	return IRQ_HANDLED;
}

void lcd_work_func(struct work_struct *work)
{

	twl_i2c_write_u8(TWL4030_MODULE_PM_RECEIVER, 0x1, 0x22);
	twl_i2c_write_u8(TWL4030_MODULE_PM_RECEIVER, 0xe0, 0x1F);

	twl_i2c_write_u8(TWL4030_MODULE_PM_RECEIVER, 0x9, 0x26);
	twl_i2c_write_u8(TWL4030_MODULE_PM_RECEIVER, 0xe0, 0x23);
}

// For calculation of home key delay
static inline u_int64_t ts_sub_to_ms(struct timespec dest, struct timespec src) {
	u_int64_t result;
	
	if (src.tv_sec > dest.tv_sec)
		return 0;
	if (src.tv_sec == dest.tv_sec && src.tv_nsec >= dest.tv_nsec)
		return 0;
	
	result = (dest.tv_sec - src.tv_sec) * MSEC_PER_SEC;
	result += (dest.tv_nsec - src.tv_nsec) / NSEC_PER_MSEC;
	return result;
}

ssize_t gpiokey_pressed_show(struct device *dev, struct device_attribute *attr, char *buf)
{
  unsigned int key_press_status=0;
  key_press_status = gpio_get_value(OMAP_GPIO_KEY_PWRON);
  key_press_status &= ~0x2;
  key_press_status |= ((!gpio_get_value(OMAP_GPIO_KEY_HOME)) << 1);

  return sprintf(buf, "%u\n", key_press_status);
}

static DEVICE_ATTR(gpiokey_pressed, S_IRUGO, gpiokey_pressed_show, NULL);
  
static irqreturn_t powerkey_press_handler(int irq_num, void * dev)
{
  struct input_dev *ip_dev = (struct input_dev *)dev;
  int key_press_status=0; 

  if(!ip_dev){
    dev_err(ip_dev->dev.parent,"Input Device not allocated\n");
    return IRQ_HANDLED;
  }
  
  key_press_status = gpio_get_value(OMAP_GPIO_KEY_PWRON);

  if( key_press_status < 0 ){
    dev_err(ip_dev->dev.parent,"Failed to read GPIO value\n");
    return IRQ_HANDLED;
  }
  
  input_report_key(ip_dev,KEY_POWER,key_press_status);
  input_sync(ip_dev);
#if ZEUS_DEBUG
  dev_dbg(ip_dev->dev.parent,"Sent KEY_POWER event = %d\n",key_press_status);
  printk("[PWR-KEY] KEY_POWER event = %d\n",key_press_status);
#endif

  if (lcd_wq && key_press_status)  
  	queue_work(lcd_wq, &lcd_work);
  
  return IRQ_HANDLED;
}

static irqreturn_t homekey_press_handler(int irq_num, void * dev)
{
  struct input_dev *ip_dev = (struct input_dev *)dev;
  
  if(!ip_dev){
    dev_err(ip_dev->dev.parent,"Input Device not allocated\n");
    return IRQ_HANDLED;
  }
  
  home_key_press_status = !gpio_get_value(OMAP_GPIO_KEY_HOME);
  
  if( home_key_press_status < 0 ){
    dev_err(ip_dev->dev.parent,"Failed to read GPIO value\n");
    return IRQ_HANDLED;
  }

  /* If the button seems to be pressed for more than 3 seconds, we assume that the filter lead to a key release being ignored and reset the status.
  */ 
  if (ts_sub_to_ms(current_kernel_time(), home_key_up_time) > 3000) {
    last_home_key_press_status = 0;

#if ZEUS_DEBUG
    printk("Status of last KEY_HOME event reset to zero\n");
#endif
  }

  /* A typical key press lasts ~100ms. We can't press this button again in less than 50ms. 
     In this case, the button must still be pressed down and  we  have to ignore the key press event.
     In case the event is the same as last time, it has to be a falsely recognized event and we have to ignore it, too. */
  if (ts_sub_to_ms(current_kernel_time(), home_key_up_time) < 50 || home_key_press_status == last_home_key_press_status) {
#if ZEUS_DEBUG
    printk("KEY_HOME event ignored, probably unwanted keypress\n");
#endif

    return IRQ_HANDLED;
  }

  last_home_key_press_status = home_key_press_status;
  input_report_key(ip_dev,KEY_HOME,home_key_press_status);
  input_sync(ip_dev);
  /* "Rearm" home key event timer */ 
  home_key_up_time = current_kernel_time();

#if ZEUS_DEBUG
  dev_dbg(ip_dev->dev.parent,"Sent KEY_HOME event = %d\n",home_key_press_status);
  printk("Sent KEY_HOME event = %d\n",home_key_press_status);
#endif

  if (lcd_wq && home_key_press_status)
	queue_work(lcd_wq, &lcd_work);
  	
  return IRQ_HANDLED;
}

static int __devinit zeus_key_driver_probe(struct platform_device *plat_dev)
{
  struct input_dev *zeus_key=NULL;
  int pwr_key_irq=-1, err=0;

  struct kobject *gpiokey;
  int home_key_irq = -1;
  int ear_key_irq = -1;

  gpiokey = kobject_create_and_add("gpiokey", NULL);
  if (!gpiokey) {
    printk("Failed to create sysfs(gpiokey)!\n");
    return -ENOMEM;
  }
  if (sysfs_create_file(gpiokey, &dev_attr_gpiokey_pressed.attr)< 0)
  printk("Failed to create device file(%s)!\n", dev_attr_gpiokey_pressed.attr.name);
   
  INIT_WORK(&lcd_work, lcd_work_func);
  lcd_wq = create_singlethread_workqueue("lcd_vaux_wq");

  pwr_key_irq = platform_get_irq(plat_dev, 0);
  if(pwr_key_irq <= 0 ){
    dev_err(&plat_dev->dev,"failed to map the power key to an IRQ %d\n", pwr_key_irq);
    err = -ENXIO;
    return err;
  }  

  home_key_irq = platform_get_irq(plat_dev, 1);
  if(home_key_irq <= 0){
    dev_err(&plat_dev->dev,"failed to map the power key to an IRQ %d\n", home_key_irq);
    err = -ENXIO;
    return err;
  }

  ear_key_irq = platform_get_irq(plat_dev, 2);
  if(ear_key_irq <= 0 ){
    dev_err(&plat_dev->dev,"failed to map the ear key to an IRQ %d\n",ear_key_irq);
    err = -ENXIO;
    return err;
  }

  zeus_key = input_allocate_device();
  if(!zeus_key)
  {
    dev_err(&plat_dev->dev,"failed to allocate an input devd %d \n",pwr_key_irq);
    err = -ENOMEM;
    return err;
  }
  err = request_irq(pwr_key_irq, &powerkey_press_handler ,IRQF_TRIGGER_FALLING|IRQF_TRIGGER_RISING,
                    "zeuskey_power",zeus_key);

  err |= request_irq(home_key_irq, &homekey_press_handler ,IRQF_TRIGGER_FALLING|IRQF_TRIGGER_RISING,
                    "zeuskey_home",zeus_key);

  err |= request_irq(ear_key_irq, &earkey_press_handler,IRQF_TRIGGER_FALLING|IRQF_TRIGGER_RISING,
                    "zeuskey_ear",zeus_key);

  if(err) {

    dev_err(&plat_dev->dev,"failed to request an IRQ handler for num %d & %d\n",pwr_key_irq, home_key_irq);

    goto free_input_dev;
  }

  dev_dbg(&plat_dev->dev,"\n Power Key Drive:Assigned IRQ num %d SUCCESS \n",pwr_key_irq);
  dev_dbg(&plat_dev->dev,"\n HOME Key Drive:Assigned IRQ num %d SUCCESS \n",home_key_irq);
  dev_dbg(&plat_dev->dev,"\n ear Key Drive:Assigned IRQ num %d SUCCESS \n",ear_key_irq);

  /* register the input device now */
  input_set_capability(zeus_key, EV_KEY, KEY_POWER);
  input_set_capability(zeus_key, EV_KEY, KEY_HOME);
  set_bit(EV_SYN,zeus_key->evbit);
  set_bit(EV_KEY,zeus_key->evbit);
  set_bit(KEYCODE_SENDEND, zeus_key->keybit);

  zeus_key->name = "zeus_key";
  zeus_key->phys = "zeus_key/input0";
  zeus_key->dev.parent = &plat_dev->dev;
  platform_set_drvdata(plat_dev, zeus_key);

  err = input_register_device(zeus_key);
  if (err) {
    dev_err(&plat_dev->dev, "power key couldn't be registered: %d\n", err);
    goto release_irq_num;
  }

  err = switch_dev_register(&switch_sendend);
  if (err < 0) {
	printk(KERN_ERR "Failed to register switch sendend device\n");
	goto free_input_dev;
  }

  init_timer(&send_end_key_event_timer);
  send_end_key_event_timer.function = send_end_key_event_timer_handler;
  ip_device = zeus_key;

  wake_lock_init( &earkey_wakelock, WAKE_LOCK_SUSPEND, "ear_key");
 
  return 0;

release_irq_num:
  free_irq(pwr_key_irq,NULL);
  free_irq(home_key_irq, NULL);
  free_irq(ear_key_irq,NULL);

free_input_dev:
  input_free_device(zeus_key);
  switch_dev_unregister(&switch_sendend);

return err;

}

static int __devexit zeus_key_driver_remove(struct platform_device *plat_dev)
{
  struct input_dev *ip_dev= platform_get_drvdata(plat_dev);
  int pwr_key_irq=0;
  int home_key_irq=0;

  pwr_key_irq = platform_get_irq(plat_dev,0);
  home_key_irq = platform_get_irq(plat_dev,1);

  free_irq(pwr_key_irq,ip_dev);
  free_irq(home_key_irq,ip_dev);
  switch_dev_unregister(&switch_sendend);

  input_unregister_device(ip_dev);

  if (lcd_wq)
		destroy_workqueue(lcd_wq);

  return 0;
}
struct platform_driver zeus_key_driver_t __refdata = {
        .probe          = &zeus_key_driver_probe,
        .remove         = __devexit_p(zeus_key_driver_remove),
        .driver         = {
                .name   = "zeuskey_device", 
                .owner  = THIS_MODULE,
        },
};

static int __init zeus_key_driver_init(void)
{
        return platform_driver_register(&zeus_key_driver_t);
}
module_init(zeus_key_driver_init);

static void __exit zeus_key_driver_exit(void)
{
        platform_driver_unregister(&zeus_key_driver_t);
}
module_exit(zeus_key_driver_exit);

MODULE_ALIAS("platform:power key driver");
MODULE_DESCRIPTION("Zeus Button Key Driver - HOME / POWER");
MODULE_LICENSE("GPL");
