/* arch/arm/mach-omap2/board-latona-connector.c
 *
 * Copyright (C) 2013 Dheeraj CVR (cvr.dheeraj@gmail.com)
 *
 * Based on mach-omap2/board-tuna-connector.c
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

#include <linux/device.h>
#include <linux/err.h>
#include <linux/gpio.h>
#include <linux/i2c.h>
#include <linux/init.h>
#include <linux/interrupt.h>
#include <linux/kernel.h>
#include <linux/fsa9480.h>
#include <linux/usb/otg.h>
#include <linux/delay.h>
#include <linux/i2c/twl.h>
#include <linux/mutex.h>
#include <linux/switch.h>
#include <mach/board-latona.h>
#include <linux/wakelock.h>

struct latona_otg {
	struct otg_transceiver	otg;
	struct device		dev;

	struct mutex		lock;

	bool			usb_connected;
	bool			charger_connected;
	int			current_device;
	int                     irq_ta_nconnected;
	struct wake_lock 	usb_lock;
};

static struct latona_otg latona_otg_xceiv;

static int latona_otg_set_peripheral(struct otg_transceiver *otg,
				 struct usb_gadget *gadget)
{
	otg->gadget = gadget;
	if (!gadget)
		otg->state = OTG_STATE_UNDEFINED;
	return 0;
}

static void latona_usb_attach(struct latona_otg *latona_otg)
{
	pr_info("[%s]\n", __func__);

	if (!latona_otg->usb_connected) {
		latona_otg->otg.state = OTG_STATE_B_IDLE;
		latona_otg->otg.default_a = false;
		latona_otg->otg.last_event = USB_EVENT_VBUS;

		atomic_notifier_call_chain(&latona_otg->otg.notifier,
				   USB_EVENT_VBUS, latona_otg->otg.gadget);

		latona_otg->usb_connected = true;

		if(!wake_lock_active(&latona_otg->usb_lock))
			wake_lock(&latona_otg->usb_lock);
	}
}

static void latona_detach(struct latona_otg *latona_otg)
{
	pr_info("[%s]\n", __func__);

	if ( latona_otg->usb_connected || latona_otg->charger_connected ) {
		latona_otg->otg.state = OTG_STATE_B_IDLE;
		latona_otg->otg.default_a = false;
		latona_otg->otg.last_event = USB_EVENT_NONE;

		atomic_notifier_call_chain(&latona_otg->otg.notifier,
				   USB_EVENT_NONE, latona_otg->otg.gadget);

		latona_otg->usb_connected = false;
		latona_otg->charger_connected = false;

		if(wake_lock_active(&latona_otg->usb_lock))
			wake_unlock(&latona_otg->usb_lock);
	}
}

static void latona_charger_attach(struct latona_otg *latona_otg)
{
	pr_info("[%s]\n", __func__);

	if (!latona_otg->charger_connected) {
		latona_otg->otg.state = OTG_STATE_B_IDLE;
		latona_otg->otg.default_a = false;
		latona_otg->otg.last_event = USB_EVENT_CHARGER;
		atomic_notifier_call_chain(&latona_otg->otg.notifier,
			USB_EVENT_CHARGER, latona_otg->otg.gadget);

		latona_otg->charger_connected = true;
	}
}

static void latona_fsa_device_detected(int device)
{
	struct latona_otg *latona_otg = &latona_otg_xceiv;

	mutex_lock(&latona_otg->lock);

	pr_debug("detected %x\n", device);
	switch (device) {
	case MICROUSBIC_USB_CABLE:
		latona_usb_attach(latona_otg);
		break;
	case MICROUSBIC_USB_CHARGER:
	case MICROUSBIC_5W_CHARGER:
	case MICROUSBIC_TA_CHARGER:
		latona_charger_attach(latona_otg);
		break;
	case MICROUSBIC_NO_DEVICE:
	default:
		latona_detach(latona_otg);
		break;
	}

	mutex_unlock(&latona_otg->lock);
}

static struct fsa9480_platform_data latona_fsa9480_pdata = {
	.detected	= latona_fsa_device_detected,
};

static struct i2c_board_info __initdata latona_connector_i2c2_boardinfo[] = {
	{
		I2C_BOARD_INFO("fsa9480", 0x25),
		.flags = I2C_CLIENT_WAKE,
		.irq = OMAP_GPIO_IRQ(OMAP_GPIO_JACK_NINT),
		.platform_data = &latona_fsa9480_pdata,
	},
};

void __init latona_connector_init(void)
{
	struct latona_otg *latona_otg = &latona_otg_xceiv;
	int ret;

	mutex_init(&latona_otg->lock);

	wake_lock_init(&latona_otg->usb_lock, WAKE_LOCK_SUSPEND, "latona_usb_wakelock");

	device_initialize(&latona_otg->dev);
	dev_set_name(&latona_otg->dev, "%s", "latona_otg");
	ret = device_add(&latona_otg->dev);
	if (ret) {
		pr_err("%s: cannot reg device '%s' (%d)\n", __func__,
		       dev_name(&latona_otg->dev), ret);
		return;
	}

	dev_set_drvdata(&latona_otg->dev, latona_otg);

	latona_otg->otg.dev		= &latona_otg->dev;
	latona_otg->otg.label		= "latona_otg_xceiv";
	latona_otg->otg.set_peripheral	= latona_otg_set_peripheral;

	ATOMIC_INIT_NOTIFIER_HEAD(&latona_otg->otg.notifier);

	ret = otg_set_transceiver(&latona_otg->otg);
	if (ret)
		pr_err("latona_otg: cannot set transceiver (%d)\n", ret);

	i2c_register_board_info(2, latona_connector_i2c2_boardinfo,
				ARRAY_SIZE(latona_connector_i2c2_boardinfo));
}
