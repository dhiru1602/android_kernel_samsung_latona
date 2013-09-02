/*
 * nt35510 LDI support
 *
 * Copyright (C) 2009 Samsung Corporation
 * Author: Samsung Electronics..
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 as published by
 * the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <linux/module.h>
#include <linux/delay.h>
#include <linux/platform_device.h>
#include <linux/i2c/twl.h>
#include <linux/spi/spi.h>
#include <linux/regulator/consumer.h>
#include <linux/err.h>

#include <plat/gpio.h>
#include <plat/hardware.h>
#include <plat/mux.h>
#include <asm/mach-types.h>

#include <video/omapdss.h>

#include <linux/backlight.h>
#include  <../../../../arch/arm/mach-omap2/mux.h>
#include  <../../../../arch/arm/mach-omap2/control.h>
#include <linux/i2c/twl.h>

//Debugging
//#define DEBUG

#ifdef DEBUG
#define LCD_DEBUG 1
#else
#define LCD_DEBUG 0
#endif

#define LCD_XRES			480
#define LCD_YRES			800
#define LCD_PIXCLOCK_MAX		24000 // 26000
#define LCD_PIXCLOCK_MAX_SONY		20571

static int current_panel = -1;	// 0:sony, 1:Hitachi(20mA) , 2:Hydis, 3:SMD, 4:Sony(a-Si), 5:Hitachi(17mA)
static int is_nt35510_spi_shutdown = 0;

#define GPIO_LEVEL_LOW   0
#define GPIO_LEVEL_HIGH  1

#define POWER_OFF        0   // set in lcd_poweroff function.
#define POWER_ON         1   // set in lcd_poweron function

static struct spi_device *nt35510lcd_spi;

static atomic_t ldi_power_state = ATOMIC_INIT(POWER_ON);   // ldi power state

struct regulator *vaux3;
struct regulator *vaux4;

static void nt35510_lcd_poweron(void);
static void nt35510_lcd_poweroff(void);
static void nt35510_lcd_LDO_on(void);
static void nt35510_lcd_LDO_off(void);
static void nt35510_ldi_poweron_sony(void);
static void nt35510_ldi_poweroff_sony(void);
static void nt35510_ldi_poweron_hitachi(void);
static void nt35510_ldi_poweroff_hitachi(void);
static void aat1402_set_brightness(int);

/*
    PANEL TIMINGS
    24.10.2011 by codeworkx@cyanogenmod.com
 */

// default panel timings
static struct omap_video_timings nt35510_default_panel_timings = {

	.x_res          = 480,
	.y_res          = 800,
	.pixel_clock    = LCD_PIXCLOCK_MAX_SONY,
	.hfp            = 10,
	.hsw            = 10,
	.hbp            = 10,
	.vfp            = 4,
	.vsw            = 2,
	.vbp            = 9,

};

// hitachi panel timings
static struct omap_video_timings nt35510_hitachi_panel_timings = {

	.x_res          = 480,
	.y_res          = 800,
	.pixel_clock    = LCD_PIXCLOCK_MAX,
	.hfp            = 100,
	.hsw            = 2,
	.hbp            = 2,
	.vfp            = 8,
	.vsw            = 2,
	.vbp            = 10,

};

// smd panel timings
static struct omap_video_timings nt35510_smd_panel_timings = {

	.x_res          = 480,
	.y_res          = 800,
	.pixel_clock    = LCD_PIXCLOCK_MAX,
	.hfp            = 10,
	.hsw            = 4,
	.hbp            = 40,
	.vfp            = 6,
	.vsw            = 1,
	.vbp            = 7,

};

// sony (a-Si) panel timings
static struct omap_video_timings nt35510_sonyasi_panel_timings = {

	.x_res          = 480,
	.y_res          = 800,
	.pixel_clock    = LCD_PIXCLOCK_MAX_SONY,
	.hfp            = 14,
	.hsw            = 10,
	.hbp            = 14,
	.vfp            = 8,
	.vsw            = 2,
	.vbp            = 8,

};

static __init int setup_current_panel(char *opt)
{
	current_panel = (u32)memparse(opt, &opt);
	return 0;
}
__setup("androidboot.current_panel=", setup_current_panel);

static int nt35510_panel_probe(struct omap_dss_device *dssdev)
{
	dev_dbg(&dssdev->dev, "%s\n", __func__);

	vaux4 = regulator_get(&dssdev->dev, "vaux4");
	if (IS_ERR(vaux4))
		dev_err(&dssdev->dev, "failed to register vaux4\n");

	vaux3 = regulator_get(&dssdev->dev, "vaux3");
	if (IS_ERR(vaux3))
		dev_err(&dssdev->dev, "failed to register vaux3\n");

	nt35510_lcd_LDO_on();

	//MLCD pin set to OUTPUT.
	if (gpio_request(OMAP_GPIO_MLCD_RST, "MLCD_RST") < 0) {
		dev_err(&dssdev->dev, "failed to request GPIO %d\n", OMAP_GPIO_MLCD_RST);
		return -EPERM;
	}
	gpio_direction_output(OMAP_GPIO_MLCD_RST, 1);
	dev_info(&dssdev->dev, "current_panel=%d (0:sony, 1:Hitachi(20mA), 2:Hydis, 3:SMD, 4:Sony(a-Si), 5:Hitachi(17mA)\n",
		 current_panel);

	if (current_panel == 1 || current_panel == 4 || current_panel == 5) // Hitachi(20mA) || Sony(a-Si) || Hitachi(17mA)
		dssdev->panel.config = OMAP_DSS_LCD_TFT | OMAP_DSS_LCD_IVS |  OMAP_DSS_LCD_IPC |
				       OMAP_DSS_LCD_IHS | OMAP_DSS_LCD_ONOFF;
	else // if(current_panel == 0 || current_panel == 2 || current_panel == 3) // Sony || Hydis || SMD
		dssdev->panel.config = OMAP_DSS_LCD_TFT | OMAP_DSS_LCD_IVS | OMAP_DSS_LCD_IPC |
				       OMAP_DSS_LCD_IHS | OMAP_DSS_LCD_ONOFF | OMAP_DSS_LCD_IEO;

	dssdev->panel.acb = 0;

	dssdev->panel.timings = nt35510_default_panel_timings;

	if (current_panel == 1)
		dssdev->panel.timings = nt35510_hitachi_panel_timings;
	else if (current_panel == 3)
		dssdev->panel.timings = nt35510_smd_panel_timings;
	else if (current_panel == 4 || current_panel == 5)
		dssdev->panel.timings = nt35510_sonyasi_panel_timings;

	/* Since some android application use physical dimension, that information should be set here */
	dssdev->panel.width_in_um = 52000;      /* physical dimension in um */
	dssdev->panel.height_in_um = 86000;     /* physical dimension in um */

	return 0;
}

static void nt35510_panel_remove(struct omap_dss_device *dssdev)
{
	regulator_put(vaux4);
	regulator_put(vaux3);
}

static int nt35510_panel_enable(struct omap_dss_device *dssdev)
{
	static int boot = 1;
	int r = 0;

	r = omapdss_dpi_display_enable(dssdev);
	if (r)
		goto err0;

	/* Delay recommended by panel DATASHEET */
	mdelay(4);
	if (dssdev->platform_enable) {
		r = dssdev->platform_enable(dssdev);
		if (r)
			goto err1;
	}

	/* Prevent LCD blink on boot */
	if (boot) {
		boot = 0;
		goto ret;
	}

	nt35510_lcd_poweron();

ret:
	dssdev->state = OMAP_DSS_DISPLAY_ACTIVE;
	return r;
err1:
	omapdss_dpi_display_disable(dssdev);
err0:
	return r;
}

static void nt35510_panel_disable(struct omap_dss_device *dssdev)
{
	if (is_nt35510_spi_shutdown == 1) {
		dev_info(&dssdev->dev, "skip omapdss_dpi_display_disable\n");
		return;
	}

	nt35510_lcd_poweroff();
	if (dssdev->platform_disable)
		dssdev->platform_disable(dssdev);
	mdelay(4);

	omapdss_dpi_display_disable(dssdev);

	dssdev->state = OMAP_DSS_DISPLAY_DISABLED;
}

static int nt35510_panel_suspend(struct omap_dss_device *dssdev)
{
	dev_dbg(&dssdev->dev, "panel suspend\n");

	//necessary?
	spi_setup(nt35510lcd_spi);

	if (dssdev->state != OMAP_DSS_DISPLAY_ACTIVE)
		return -EINVAL;

	gpio_set_value(OMAP_GPIO_LCD_EN_SET, GPIO_LEVEL_LOW);
	mdelay(1);

	nt35510_panel_disable(dssdev);

	dssdev->state = OMAP_DSS_DISPLAY_SUSPENDED;

	return 0;
}

static int nt35510_panel_resume(struct omap_dss_device *dssdev)
{
	int r;

	dev_dbg(&dssdev->dev, "panel resume\n");

	//necessary?
	spi_setup(nt35510lcd_spi);

	//msleep(150);

	if (dssdev->state != OMAP_DSS_DISPLAY_SUSPENDED) {
		return -EINVAL;
	}

	r = nt35510_panel_enable(dssdev);

	if (r) {
		dssdev->state = OMAP_DSS_DISPLAY_DISABLED;
	} else {
		gpio_set_value(OMAP_GPIO_LCD_EN_SET, GPIO_LEVEL_LOW);
		mdelay(1);
		gpio_set_value(OMAP_GPIO_LCD_EN_SET, GPIO_LEVEL_HIGH);
		dssdev->state = OMAP_DSS_DISPLAY_ACTIVE;
	}

	return r;
}

static void nt35510_panel_set_timings(struct omap_dss_device *dssdev,
				      struct omap_video_timings *timings)
{
	dpi_set_timings(dssdev, timings);
}

static void nt35510_panel_get_timings(struct omap_dss_device *dssdev,
				      struct omap_video_timings *timings)
{
	*timings = dssdev->panel.timings;
}

static int nt35510_panel_check_timings(struct omap_dss_device *dssdev,
				       struct omap_video_timings *timings)
{
	return dpi_check_timings(dssdev, timings);
}

static void nt35510_get_resolution(struct omap_dss_device *dssdev,
				   u16 *xres, u16 *yres)
{
	*yres = dssdev->panel.timings.y_res;
	*xres = dssdev->panel.timings.x_res;
}

static struct omap_dss_driver nt35510_driver = {
	.probe          = nt35510_panel_probe,
	.remove         = nt35510_panel_remove,

	.enable         = nt35510_panel_enable,
	.disable        = nt35510_panel_disable,
	.suspend        = nt35510_panel_suspend,
	.resume         = nt35510_panel_resume,

	.get_resolution	= nt35510_get_resolution,
	.get_recommended_bpp = omapdss_default_get_recommended_bpp,

	.set_timings	= nt35510_panel_set_timings,
	.get_timings	= nt35510_panel_get_timings,
	.check_timings	= nt35510_panel_check_timings,

	.driver		= {
		.name	= "nt35510_panel",
		.owner 	= THIS_MODULE,
	},
};

static void spi1writeindex(u8 index)
{
	volatile unsigned short cmd = 0;
	cmd = 0x0000|index;

	spi_write(nt35510lcd_spi,(unsigned char*)&cmd,2);

	udelay(100);
	udelay(100);
}

static void spi1writedata(u8 data)
{
	volatile unsigned short datas = 0;

	datas = 0x0100|data;
	spi_write(nt35510lcd_spi,(unsigned char*)&datas,2);

	udelay(100);
	udelay(100);
}

static void spi1write(u8 index, u8 data)
{
	volatile unsigned short cmd = 0;
	volatile unsigned short datas = 0;

	cmd = 0x0000 | index;
	datas = 0x0100 | data;

	spi_write(nt35510lcd_spi, (unsigned char*)&cmd, 2);
	udelay(100);
	spi_write(nt35510lcd_spi, (unsigned char*)&datas, 2);
	udelay(100);
	udelay(100);
}

static void nt35510_ldi_poweron_hitachi(void)
{
#if LCD_DEBUG
	printk("[LCD] %s() + \n", __func__);
#endif

	spi1writeindex(0x36);
	spi1writedata(0x00);

	spi1writeindex(0x3A);
	spi1writedata(0x77);

	spi1writeindex(0x11);

	msleep(150);

	spi1writeindex(0xB0);
	spi1writedata(0x04);

	spi1writeindex(0xC1);
	spi1writedata(0x42);
	spi1writedata(0x31);
	spi1writedata(0x04);

	spi1writeindex(0xB0);
	spi1writedata(0x03);

	aat1402_set_brightness(0); // default brightness : 0

	spi1writeindex(0x29);

	atomic_set(&ldi_power_state, POWER_ON);
#if LCD_DEBUG
	printk("[LCD] %s() -\n", __func__);
#endif
}


static void nt35510_ldi_poweroff_hitachi(void)
{
#if LCD_DEBUG
	printk(" **** %s\n", __func__);
#endif

	// SLEEP IN
	spi1writeindex(0x10);   // SEQ_SLEEP IN_SET

	//Wait 120ms
	msleep(150);

	// Display Off
	//spi1writeindex(0x28);	// SEQ_DISPOFF_SET
	spi1writeindex(0xB0);
	spi1writedata(0x04);
	spi1writeindex(0xB1);
	spi1writedata(0x01);
	msleep(2);

	atomic_set(&ldi_power_state, POWER_OFF);
}

static void nt35510_ldi_poweron_sony(void)
{
#if LCD_DEBUG
	printk("[LCD] %s() + \n", __func__);
#endif
	msleep(145);

	spi1writeindex(0x3A);
	spi1writedata(0x77);

	spi1writeindex(0x3B);
	spi1writedata(0x07);
	spi1writedata(0x0A);
	spi1writedata(0x0E);
	spi1writedata(0x0A);
	spi1writedata(0x0A);

	spi1writeindex(0x2A);
	spi1writedata(0x00);
	spi1writedata(0x00);
	spi1writedata(0x01);
	spi1writedata(0xDF);

	spi1writeindex(0x2B);
	spi1writedata(0x00);
	spi1writedata(0x00);
	spi1writedata(0x03);
	spi1writedata(0x1F);

	spi1writeindex(0x36);
	spi1writedata(0xD4);

	msleep(25);

	spi1writeindex(0x11);

	msleep(200);

	spi1writeindex(0x51);
	spi1writedata(0x0); // default brightness : 0
	//spi1writedata(0x6C); // default brightness : 108

	spi1writeindex(0x55);
	spi1writedata(0x01); // CABC Off, 01:User interface mode, 02:still image mode, 03:moving image mode

	spi1writeindex(0x5E);
	spi1writedata(0x00); // example value. *3)

	spi1writeindex(0x53);
	spi1writedata(0x2C);

	//msleep(200);

	spi1writeindex(0x29);

	atomic_set(&ldi_power_state, POWER_ON);
#if LCD_DEBUG
	printk("[LCD] %s() -\n", __func__);
#endif
}


static void nt35510_ldi_poweroff_sony(void)
{
#if LCD_DEBUG
	printk(" **** %s\n", __func__);
#endif

	// Display Off
	spi1writeindex(0x28);   // SEQ_DISPOFF_SET

	msleep(25);

	// SLEEP IN
	spi1writeindex(0x10);   // SEQ_SLEEP IN_SET

	//Wait 120ms
	msleep(150);

	atomic_set(&ldi_power_state, POWER_OFF);
}

static void nt35510_ldi_poweron_sony_a_si(void)
{
#if LCD_DEBUG
	printk("[LCD] %s() + \n", __func__);
#endif
	msleep(145);
	spi1writeindex(0x3A);
	spi1writedata(0x77);

	spi1writeindex(0x2A);
	spi1writedata(0x00);
	spi1writedata(0x00);
	spi1writedata(0x01);
	spi1writedata(0xDF);

	spi1writeindex(0x2B);
	spi1writedata(0x00);
	spi1writedata(0x00);
	spi1writedata(0x03);
	spi1writedata(0x1F);

	spi1writeindex(0x36); // SET_ADDRESS_MODE
	spi1writedata(0x00);

	spi1writeindex(0xF0);
	spi1writedata(0x5A);
	spi1writedata(0x5A);

	spi1writeindex(0xF1);
	spi1writedata(0x5A);
	spi1writedata(0x5A);

	spi1writeindex(0xB2);
	spi1writedata(0x22);
	spi1writedata(0x31);

	spi1writeindex(0xBA);
	spi1writedata(0x07);
	spi1writedata(0xAA);
	spi1writedata(0x10);
	spi1writedata(0x10);
	spi1writedata(0x0A);
	spi1writedata(0x18);
	spi1writedata(0x1F);
	spi1writedata(0x1F);

	spi1writeindex(0xBB);
	spi1writedata(0x07);
	spi1writedata(0xAA);
	spi1writedata(0x10);
	spi1writedata(0x10);
	spi1writedata(0x0A);
	spi1writedata(0x1C);
	spi1writedata(0x1F);
	spi1writedata(0x1F);

	spi1writeindex(0xF2);   // SET_DISPLAY_CONTROL
	spi1writedata(0x00);
	spi1writedata(0x08);    // NVBP
	spi1writedata(0x08);    // NVFB
	spi1writedata(0x08);    // BVBP
	spi1writedata(0x08);    // BVFP
	spi1writedata(0x14);    // NHBP
	spi1writedata(0x14);    // NHFP
	spi1writedata(0x14);    // BHBP
	spi1writedata(0x14);    // BHFP
	spi1writedata(0x08);    // RGB_NVBP
	spi1writedata(0x08);    // RGB_NVFP
	spi1writedata(0x08);    // GRB_BVBP
	spi1writedata(0x08);    // RGB_BVFP
	spi1writedata(0x31);    // VPL[7]:Low, HPL[6]:Low, DPL[5]:falling, EPL[4]:High  // spi1writedata(0x11);
	spi1writedata(0x00);
	spi1writedata(0x00);

	spi1writeindex(0xF4);
	spi1writedata(0x01);
	spi1writedata(0x15);
	spi1writedata(0x01);
	spi1writedata(0x04);
	spi1writedata(0x01);
	spi1writedata(0x23);
	spi1writedata(0x66);
	spi1writedata(0x00);
	spi1writedata(0x12);
	spi1writedata(0x12);
	spi1writedata(0x0A);

	spi1writeindex(0xD0);
	spi1writedata(0x07);

	spi1writeindex(0xF6);
	spi1writedata(0x08);
	spi1writedata(0x00);
	spi1writedata(0x00);
	spi1writedata(0x05);
	spi1writedata(0x01);
	spi1writedata(0x14);
	spi1writedata(0x14);
	spi1writedata(0x04);
	spi1writedata(0x04);
	spi1writedata(0x06);
	spi1writedata(0x06);

	spi1writeindex(0xF7);
	spi1writedata(0x01);
	spi1writedata(0xD8);
	spi1writedata(0x50);
	spi1writedata(0xFA);
	spi1writedata(0x4E);
	spi1writedata(0x08);
	spi1writedata(0x08);
	spi1writedata(0x01);
	spi1writedata(0x01);
	spi1writedata(0x00);

	spi1writeindex(0xFA);
	spi1writedata(0x02);
	spi1writedata(0x52);
	spi1writedata(0x52);
	spi1writedata(0x44);
	spi1writedata(0x38);
	spi1writedata(0x2D);
	spi1writedata(0x2D);
	spi1writedata(0x18);
	spi1writedata(0x18);
	spi1writedata(0x1E);
	spi1writedata(0x1E);
	spi1writedata(0x1B);
	spi1writedata(0x1D);
	spi1writedata(0x1B);
	spi1writedata(0x13);
	spi1writedata(0x04);
	spi1writedata(0x02);
	spi1writedata(0x52);
	spi1writedata(0x52);
	spi1writedata(0x43);
	spi1writedata(0x39);
	spi1writedata(0x2E);
	spi1writedata(0x2E);
	spi1writedata(0x18);
	spi1writedata(0x18);
	spi1writedata(0x1E);
	spi1writedata(0x1D);
	spi1writedata(0x1A);
	spi1writedata(0x1C);
	spi1writedata(0x1A);
	spi1writedata(0x12);
	spi1writedata(0x04);
	spi1writedata(0x02);
	spi1writedata(0x52);
	spi1writedata(0x52);
	spi1writedata(0x43);
	spi1writedata(0x38);
	spi1writedata(0x2C);
	spi1writedata(0x2D);
	spi1writedata(0x17);
	spi1writedata(0x17);
	spi1writedata(0x1D);
	spi1writedata(0x1C);
	spi1writedata(0x18);
	spi1writedata(0x1A);
	spi1writedata(0x18);
	spi1writedata(0x10);
	spi1writedata(0x04);

	spi1writeindex(0xFB);
	spi1writedata(0x02);
	spi1writedata(0x52);
	spi1writedata(0x52);
	spi1writedata(0x44);
	spi1writedata(0x38);
	spi1writedata(0x2D);
	spi1writedata(0x2D);
	spi1writedata(0x18);
	spi1writedata(0x18);
	spi1writedata(0x1E);
	spi1writedata(0x1E);
	spi1writedata(0x1B);
	spi1writedata(0x1D);
	spi1writedata(0x1B);
	spi1writedata(0x13);
	spi1writedata(0x04);
	spi1writedata(0x02);
	spi1writedata(0x52);
	spi1writedata(0x52);
	spi1writedata(0x43);
	spi1writedata(0x39);
	spi1writedata(0x2E);
	spi1writedata(0x2E);
	spi1writedata(0x18);
	spi1writedata(0x18);
	spi1writedata(0x1E);
	spi1writedata(0x1D);
	spi1writedata(0x1A);
	spi1writedata(0x1C);
	spi1writedata(0x1A);
	spi1writedata(0x12);
	spi1writedata(0x04);
	spi1writedata(0x02);
	spi1writedata(0x52);
	spi1writedata(0x52);
	spi1writedata(0x43);
	spi1writedata(0x38);
	spi1writedata(0x2C);
	spi1writedata(0x2D);
	spi1writedata(0x17);
	spi1writedata(0x17);
	spi1writedata(0x1D);
	spi1writedata(0x1C);
	spi1writedata(0x18);
	spi1writedata(0x1A);
	spi1writedata(0x18);
	spi1writedata(0x10);
	spi1writedata(0x04);

	msleep(25); // 25ms

	spi1writeindex(0x11);

	msleep(150); // 150ms

	spi1writeindex(0x51);
	spi1writedata(0x0); // default brightness : 0
	//spi1writedata(0x6C); // default brightness : 108

	spi1writeindex(0x55);
	spi1writedata(0x01); // CABC Off, 01:User interface mode, 02:still image mode, 03:moving image mode

	spi1writeindex(0x5E);
	spi1writedata(0x00); // example value. *3)

	spi1writeindex(0x53);
	spi1writedata(0x2C);

	spi1writeindex(0xC0);
	spi1writedata(0x80);
	spi1writedata(0x80);
	spi1writedata(0x30);

	spi1writeindex(0xC1);
	spi1writedata(0x11);

	spi1writeindex(0xC2);
	spi1writedata(0x08);
	spi1writedata(0x00);
	spi1writedata(0x00);
	spi1writedata(0x03);
	spi1writedata(0x1F);
	spi1writedata(0x00);
	spi1writedata(0x00);
	spi1writedata(0x01);
	spi1writedata(0xDF);

	spi1writeindex(0xC3);
	spi1writedata(0x01);
	spi1writedata(0x00);
	spi1writedata(0x28);

	msleep(200); // 200ms

	spi1writeindex(0xF0);
	spi1writedata(0xA5);
	spi1writedata(0xA5);

	spi1writeindex(0xF1);
	spi1writedata(0xA5);
	spi1writedata(0xA5);

	spi1writeindex(0x29);

	atomic_set(&ldi_power_state, POWER_ON);
#if LCD_DEBUG
	printk("[LCD] %s() -\n", __func__);
#endif
}


static void nt35510_ldi_poweroff_sony_a_si(void)
{
#if LCD_DEBUG
	printk(" **** %s\n", __func__);
#endif

	// Display Off
	spi1writeindex(0x28);   // SEQ_DISPOFF_SET

	msleep(50);

	// SLEEP IN
	spi1writeindex(0x10);   // SEQ_SLEEP IN_SET

	//Wait 120ms
	msleep(150);

	atomic_set(&ldi_power_state, POWER_OFF);
}

void lcd_hydis_gamma1(void)
{
	spi1writedata(0x00);
	spi1writedata(0x37);
	spi1writedata(0x00);
	spi1writedata(0x61);
	spi1writedata(0x00);
	spi1writedata(0x92);
	spi1writedata(0x00);
	spi1writedata(0xB4);
	spi1writedata(0x00);
	spi1writedata(0xCF);
	spi1writedata(0x01);
	spi1writedata(0x06);
	spi1writedata(0x01);
	spi1writedata(0x41);
	spi1writedata(0x01);
	spi1writedata(0x8A);
	spi1writedata(0x01);
	spi1writedata(0xA6);
	spi1writedata(0x01);
	spi1writedata(0xD1);
	spi1writedata(0x02);
	spi1writedata(0x01);
	spi1writedata(0x02);
	spi1writedata(0x3D);
	spi1writedata(0x02);
	spi1writedata(0x77);
	spi1writedata(0x02);
	spi1writedata(0x79);
	spi1writedata(0x02);
	spi1writedata(0xA5);
	spi1writedata(0x02);
	spi1writedata(0xD1);
	spi1writedata(0x02);
	spi1writedata(0xF9);
	spi1writedata(0x03);
	spi1writedata(0x25);
	spi1writedata(0x03);
	spi1writedata(0x43);
	spi1writedata(0x03);
	spi1writedata(0x6E);
	spi1writedata(0x03);
	spi1writedata(0x77);
	spi1writedata(0x03);
	spi1writedata(0x94);
	spi1writedata(0x03);
	spi1writedata(0x9F);
	spi1writedata(0x03);
	spi1writedata(0xAC);
	spi1writedata(0x03);
	spi1writedata(0xBA);
	spi1writedata(0x03);
	spi1writedata(0xF1);
}
void lcd_hydis_gamma2(void)
{
	spi1writedata(0x00);
	spi1writedata(0x37);
	spi1writedata(0x00);
	spi1writedata(0x50);
	spi1writedata(0x00);
	spi1writedata(0x89);
	spi1writedata(0x00);
	spi1writedata(0xA9);
	spi1writedata(0x00);
	spi1writedata(0xC0);
	spi1writedata(0x01);
	spi1writedata(0x06);
	spi1writedata(0x01);
	spi1writedata(0x26);
	spi1writedata(0x01);
	spi1writedata(0x54);
	spi1writedata(0x01);
	spi1writedata(0x79);
	spi1writedata(0x01);
	spi1writedata(0xB8);
	spi1writedata(0x01);
	spi1writedata(0xDF);
	spi1writedata(0x02);
	spi1writedata(0x2F);
	spi1writedata(0x02);
	spi1writedata(0x68);
	spi1writedata(0x02);
	spi1writedata(0x6A);
	spi1writedata(0x02);
	spi1writedata(0xA3);
	spi1writedata(0x02);
	spi1writedata(0xE0);
	spi1writedata(0x02);
	spi1writedata(0xF9);
	spi1writedata(0x03);
	spi1writedata(0x25);
	spi1writedata(0x03);
	spi1writedata(0x43);
	spi1writedata(0x03);
	spi1writedata(0x6E);
	spi1writedata(0x03);
	spi1writedata(0x77);
	spi1writedata(0x03);
	spi1writedata(0x94);
	spi1writedata(0x03);
	spi1writedata(0x9F);
	spi1writedata(0x03);
	spi1writedata(0xAC);
	spi1writedata(0x03);
	spi1writedata(0xBA);
	spi1writedata(0x03);
	spi1writedata(0xF1);

}

static void nt35510_ldi_poweron_hydis(void)
{
#if LCD_DEBUG
	printk("[LCD] %s() + \n", __func__);
#endif

// [[ User Set
	/* Test Commands */
	spi1writeindex(0xFF);
	spi1writedata(0xAA);
	spi1writedata(0x55);
	spi1writedata(0x25);
	spi1writedata(0x01);

	/* Test Commands */
	spi1writeindex(0xF3);
	spi1writedata(0x00);
	spi1writedata(0x32);
	spi1writedata(0x00);
	spi1writedata(0x38);
	spi1writedata(0x31);
	spi1writedata(0x08);
	spi1writedata(0x11);
	spi1writedata(0x00);

	/* Manufacture Command Set Selection */
	spi1writeindex(0xF0);
	spi1writedata(0x55);
	spi1writedata(0xAA);
	spi1writedata(0x52);
	spi1writedata(0x08);
	spi1writedata(0x00);

	/* SEC Setting */
	spi1writeindex(0xB0);
	spi1writedata(0x04);    // 0x00  //DE Mode/Rising Edge for PCLK
	spi1writedata(0x0A);    //VBP 10
	spi1writedata(0x0E);    //VFP 14
	spi1writedata(0x09);    //HBP 10
	spi1writedata(0x04);    //HFP 10

	/*  */
	spi1writeindex(0xB1);
	spi1writedata(0xCC);    // Backward for Gate
	spi1writedata(0x04);    // "04 : Backward / 00 : Forward

	/* data */
	spi1writeindex(0x36);
	spi1writedata(0x02); // 00 : Forward / 02 : Backward

	/* Display Clock in RGB Interface */
	spi1writeindex(0xB3);
	spi1writedata(0x00);

	/* Source Output Data Hold Time */
	spi1writeindex(0xB6);
	spi1writedata(0x03);

	/* GATE EQ */
	spi1writeindex(0xB7);
	spi1writedata(0x70);
	spi1writedata(0x70);

	/* SOURCE EQ */
	spi1writeindex(0xB8);
	spi1writedata(0x00);
	spi1writedata(0x06);
	spi1writedata(0x06);
	spi1writedata(0x06);

	/* Inversion Type */
	spi1writeindex(0xBC);
	spi1writedata(0x00);
	spi1writedata(0x00);
	spi1writedata(0x00);

	/* Display Timing Control */
	spi1writeindex(0xBD);
	spi1writedata(0x01);
	spi1writedata(0x84);
	spi1writedata(0x06);
	spi1writedata(0x50);
	spi1writedata(0x00);

	/* aRD(Gateless) Setting */
	spi1writeindex(0xCC);
	spi1writedata(0x03);
	spi1writedata(0x01);
	spi1writedata(0x06);
// ]] User Set

// [[ Power Set
	spi1writeindex(0xF0);
	spi1writedata(0x55);
	spi1writedata(0xAA);
	spi1writedata(0x52);
	spi1writedata(0x08);
	spi1writedata(0x01);

	spi1writeindex(0xB0);
	spi1writedata(0x05);
	spi1writedata(0x05);
	spi1writedata(0x05);

	spi1writeindex(0xB1);
	spi1writedata(0x05);
	spi1writedata(0x05);
	spi1writedata(0x05);

	spi1writeindex(0xB2);
	spi1writedata(0x03);
	spi1writedata(0x03);
	spi1writedata(0x03);

	spi1writeindex(0xB8);
	spi1writedata(0x25);
	spi1writedata(0x25);
	spi1writedata(0x25);

	spi1writeindex(0xB3);
	spi1writedata(0x0B);
	spi1writedata(0x0B);
	spi1writedata(0x0B);

	spi1writeindex(0xB9);
	spi1writedata(0x34);
	spi1writedata(0x34);
	spi1writedata(0x34);

	spi1writeindex(0xBF);
	spi1writedata(0x01);

	spi1writeindex(0xB5);
	spi1writedata(0x08);
	spi1writedata(0x08);
	spi1writedata(0x08);

	spi1writeindex(0xBA);
	spi1writedata(0x14);
	spi1writedata(0x14);
	spi1writedata(0x14);

	spi1writeindex(0xB4);
	spi1writedata(0x2E);
	spi1writedata(0x2E);
	spi1writedata(0x2E);

	spi1writeindex(0xBC);
	spi1writedata(0x00);
	spi1writedata(0x50);
	spi1writedata(0x00);

	spi1writeindex(0xBD);
	spi1writedata(0x00);
	spi1writedata(0x60);
	spi1writedata(0x00);

	spi1writeindex(0xBE);
	spi1writedata(0x00);
	spi1writedata(0x40);

	spi1writeindex(0xCE);
	spi1writedata(0x00);
	spi1writedata(0x00);
	spi1writedata(0x00);
	spi1writedata(0x00);
	spi1writedata(0x00);
	spi1writedata(0x00);
	spi1writedata(0x00);

// ]] P S

// [[ Gamma Control
	spi1writeindex(0xD0);
	spi1writedata(0x09);
	spi1writedata(0x14);
	spi1writedata(0x07);
	spi1writedata(0x0D);

	spi1writeindex(0xD1);
	lcd_hydis_gamma1();
	spi1writeindex(0xD2);
	lcd_hydis_gamma1();
	spi1writeindex(0xD3);
	lcd_hydis_gamma1();
	spi1writeindex(0xD4);
	lcd_hydis_gamma2();
	spi1writeindex(0xD5);
	lcd_hydis_gamma2();
	spi1writeindex(0xD6);
	lcd_hydis_gamma2();
// ]] Gamma Control

// [[
/* Manufacture Command Set Selection */
	spi1writeindex(0xF0);
	spi1writedata(0x55);
	spi1writedata(0xAA);
	spi1writedata(0x52);
	spi1writedata(0x08);
	spi1writedata(0x00);
// ]]
	spi1writeindex(0x11);   // SLPOUT

	spi1writeindex(0x51);
	spi1writedata(0x0); // default brightness : 0
	//spi1writedata(0x6C); // default brightness : 108

	spi1writeindex(0x53);
	spi1writedata(0x2C);

	msleep(120);            // 120ms
	spi1writeindex(0x29);   // DISPON

	atomic_set(&ldi_power_state, POWER_ON);
#if LCD_DEBUG
	printk("[LCD] %s() -- \n", __func__);
#endif
}

static void nt35510_ldi_poweroff_hydis(void)
{
#if LCD_DEBUG
	printk(" **** %s\n", __func__);
#endif

	// Display Off
	spi1writeindex(0x28);   // SEQ_DISPOFF_SET

	// SLEEP IN
	spi1writeindex(0x10);   // SEQ_SLEEP IN_SET

	//Wait 120ms
	msleep(150);

	atomic_set(&ldi_power_state, POWER_OFF);
}

static void nt35510_ldi_poweron_smd(void)
{
#if LCD_DEBUG
	printk("[LCD] %s() + \n", __func__);
#endif

	/* Initializing Sequence */
	spi1writeindex(0x36);
	spi1writedata(0x09); // 0A

	spi1writeindex(0xB0);
	spi1writedata(0x00);

	spi1writeindex(0xC0);
	spi1writedata(0x28);
	spi1writedata(0x08);

	spi1writeindex(0xC1);
	spi1writedata(0x01);
	spi1writedata(0x30);
	spi1writedata(0x15);
	spi1writedata(0x05);
	spi1writedata(0x22);

	spi1writeindex(0xC4);
	spi1writedata(0x10);
	spi1writedata(0x01);
	spi1writedata(0x00);

	spi1writeindex(0xC5);
	spi1writedata(0x06);
	spi1writedata(0x55);
	spi1writedata(0x03);
	spi1writedata(0x07);
	spi1writedata(0x0B);
	spi1writedata(0x33);
	spi1writedata(0x00);
	spi1writedata(0x01);
	spi1writedata(0x03);

	spi1writeindex(0xC6); // RGB Sync option
	spi1writedata(0x00);

	spi1writeindex(0xC8);   // gamma set RED
	spi1writedata(0x00);    // R_Posi
	spi1writedata(0x00);
	spi1writedata(0x0F);
	spi1writedata(0x29);
	spi1writedata(0x43);
	spi1writedata(0x4E);
	spi1writedata(0x4F);
	spi1writedata(0x52);
	spi1writedata(0x50);
	spi1writedata(0x56);
	spi1writedata(0x5B);
	spi1writedata(0x5A);
	spi1writedata(0x57);
	spi1writedata(0x51);
	spi1writedata(0x55);
	spi1writedata(0x55);
	spi1writedata(0x5D);
	spi1writedata(0x5F);
	spi1writedata(0x1C);

	spi1writedata(0x00); // R_Nega
	spi1writedata(0x00);
	spi1writedata(0x0F);
	spi1writedata(0x29);
	spi1writedata(0x43);
	spi1writedata(0x4F);
	spi1writedata(0x4E);
	spi1writedata(0x52);
	spi1writedata(0x50);
	spi1writedata(0x56);
	spi1writedata(0x5B);
	spi1writedata(0x5A);
	spi1writedata(0x57);
	spi1writedata(0x51);
	spi1writedata(0x55);
	spi1writedata(0x55);
	spi1writedata(0x5D);
	spi1writedata(0x5F);
	spi1writedata(0x1C);

	spi1writeindex(0xC9);   // gamma set GREEN
	spi1writedata(0x00);    // G_Posi
	spi1writedata(0x00);
	spi1writedata(0x28);
	spi1writedata(0x36);
	spi1writedata(0x48);
	spi1writedata(0x50);
	spi1writedata(0x4E);
	spi1writedata(0x52);
	spi1writedata(0x4E);
	spi1writedata(0x53);
	spi1writedata(0x56);
	spi1writedata(0x55);
	spi1writedata(0x52);
	spi1writedata(0x4B);
	spi1writedata(0x4D);
	spi1writedata(0x4E);
	spi1writedata(0x4F);
	spi1writedata(0x4F);
	spi1writedata(0x0C);

	spi1writedata(0x00); // G_Nega
	spi1writedata(0x00);
	spi1writedata(0x28);
	spi1writedata(0x36);
	spi1writedata(0x48);
	spi1writedata(0x50);
	spi1writedata(0x4E);
	spi1writedata(0x52);
	spi1writedata(0x4E);
	spi1writedata(0x53);
	spi1writedata(0x56);
	spi1writedata(0x55);
	spi1writedata(0x52);
	spi1writedata(0x4B);
	spi1writedata(0x4D);
	spi1writedata(0x4E);
	spi1writedata(0x4F);
	spi1writedata(0x4F);
	spi1writedata(0x0C);

	spi1writeindex(0xCA);   // gamma set BLUE
	spi1writedata(0x00);    // B_Posi
	spi1writedata(0x00);
	spi1writedata(0x26);
	spi1writedata(0x35);
	spi1writedata(0x48);
	spi1writedata(0x52);
	spi1writedata(0x51);
	spi1writedata(0x57);
	spi1writedata(0x54);
	spi1writedata(0x5B);
	spi1writedata(0x60);
	spi1writedata(0x60);
	spi1writedata(0x5E);
	spi1writedata(0x59);
	spi1writedata(0x5E);
	spi1writedata(0x5A);
	spi1writedata(0x5D);
	spi1writedata(0x5C);
	spi1writedata(0x2A);

	spi1writedata(0x00); // B_Nega
	spi1writedata(0x00);
	spi1writedata(0x26);
	spi1writedata(0x35);
	spi1writedata(0x48);
	spi1writedata(0x52);
	spi1writedata(0x51);
	spi1writedata(0x57);
	spi1writedata(0x54);
	spi1writedata(0x5B);
	spi1writedata(0x60);
	spi1writedata(0x60);
	spi1writedata(0x5E);
	spi1writedata(0x59);
	spi1writedata(0x5E);
	spi1writedata(0x5A);
	spi1writedata(0x5D);
	spi1writedata(0x5C);
	spi1writedata(0x2A);

	spi1writeindex(0xD1);
	spi1writedata(0x33);
	spi1writedata(0x13);

	spi1writeindex(0xD2);
	spi1writedata(0x11);
	spi1writedata(0x00);
	spi1writedata(0x00);

	spi1writeindex(0xD3);
	spi1writedata(0x50);
	spi1writedata(0x50);

	spi1writeindex(0xD5);
	spi1writedata(0x2F);
	spi1writedata(0x11);
	spi1writedata(0x1E);
	spi1writedata(0x46);

	spi1writeindex(0xD6);
	spi1writedata(0x11);
	spi1writedata(0x0A);

	// Set PWM
	spi1writeindex(0xB4);
	spi1writedata(0x0F);
	spi1writedata(0x00);
	spi1writedata(0x50);

	spi1writeindex(0xB5);
	spi1writedata(0x00);
	//spi1writedata(0x6C); // default brightness : 108

	spi1writeindex(0xB7);
	spi1writedata(0x24);

	spi1writeindex(0xB8);
	spi1writedata(0x01);

	/* Sleep Out */
	spi1writeindex(0x11);
	msleep(20); // 20ms

	/* NVM Load Sequence */
	//spi1writeindex(0xD4);
	//spi1writedata(0x50);
	//spi1writedata(0x53);

	spi1writeindex(0xF8);
	spi1writedata(0x01);
	spi1writedata(0xF5);
	spi1writedata(0xF2);
	spi1writedata(0x71);
	spi1writedata(0x44);

	spi1writeindex(0xFC);
	spi1writedata(0x00);
	spi1writedata(0x08);
	msleep(150); // 150ms

	/* Display On */
	spi1writeindex(0x29);

	atomic_set(&ldi_power_state, POWER_ON);
#if LCD_DEBUG
	printk("[LCD] %s() -- \n", __func__);
#endif
}

static void nt35510_ldi_poweroff_smd(void)
{
#if LCD_DEBUG
	printk(" **** %s\n", __func__);
#endif

	//Display Off Command
	spi1writeindex(0x28);

	// Sleep In
	spi1writeindex(0x10);
	msleep(120); // 120 ms

	atomic_set(&ldi_power_state, POWER_OFF);
}

static void nt35510_lcd_LDO_on(void)
{
	int ret;

#if LCD_DEBUG
	printk("+++ %s\n", __func__);
#endif
	//twl_i2c_read_regdump();

	ret = regulator_enable(vaux3); //VAUX3 - 1.8V
	if (ret)
		printk("%s: failed to enable vaux3\n", __func__);

	mdelay(1);

	ret = regulator_enable(vaux4); //VAUX4 - 2.8V
	if (ret)
		printk("%s: failed to enable vaux4\n", __func__);

	mdelay(1);

	if (current_panel == 3) // if SMD
		msleep(50);

#if LCD_DEBUG
	printk("--- %s\n", __func__);
#endif
}

static void nt35510_lcd_LDO_off(void)
{
	int ret;

#if LCD_DEBUG
	printk("+++ %s\n", __func__);
#endif

	// Reset Release (reset = L)
	gpio_set_value(OMAP_GPIO_MLCD_RST, GPIO_LEVEL_LOW);
	mdelay(10);

	// VCI 2.8V OFF
	ret = regulator_disable(vaux4);
	if (ret)
		printk("%s: failed to disable vaux4\n", __func__);
	mdelay(1);

	// VDD3 1.8V OFF
	ret = regulator_disable(vaux3);
	if (ret)
		printk("%s: failed to disable vaux3\n", __func__);

#if LCD_DEBUG
	printk("--- %s\n", __func__);
#endif
}

static void nt35510_lcd_poweroff(void)
{
	if (current_panel == 0) // Sony
		nt35510_ldi_poweroff_sony();
	else if (current_panel == 1 || current_panel == 5) // Hitachi 20mA, 17mA
		nt35510_ldi_poweroff_hitachi();
	else if (current_panel == 2) // Hydis
		nt35510_ldi_poweroff_hydis();
	else if (current_panel == 3) // SMD
		nt35510_ldi_poweroff_smd();
	else if (current_panel == 4) // Sony(a-Si)
		nt35510_ldi_poweroff_sony_a_si();

	// turn OFF VCI (2.8V)
	// turn OFF VDD3 (1.8V)
	nt35510_lcd_LDO_off();

#if 1
	omap_mux_init_signal("mcspi1_clk", OMAP_MUX_MODE7 | OMAP_PIN_INPUT_PULLDOWN);
	omap_mux_init_signal("mcspi1_simo",  OMAP_MUX_MODE7 | OMAP_PIN_INPUT_PULLDOWN);
	omap_mux_init_signal("mcspi1_somi", OMAP_MUX_MODE7 | OMAP_PIN_INPUT_PULLDOWN);
	omap_mux_init_signal("mcspi1_cs0", OMAP_MUX_MODE7 | OMAP_PIN_INPUT_PULLDOWN);
#endif
}

static void nt35510_lcd_poweron()
{
#if 1
	omap_mux_init_signal("mcspi1_clk", OMAP_MUX_MODE0 | OMAP_PIN_INPUT_PULLUP);
	omap_mux_init_signal("mcspi1_simo", OMAP_MUX_MODE0 | OMAP_PIN_INPUT_PULLDOWN);
	omap_mux_init_signal("mcspi1_somi", OMAP_MUX_MODE0 | OMAP_PIN_INPUT_PULLDOWN);
	omap_mux_init_signal("mcspi1_cs0", OMAP_MUX_MODE0 | OMAP_PIN_INPUT_PULLUP);
#endif

	nt35510_lcd_LDO_on();

	// Activate Reset
	gpio_set_value(OMAP_GPIO_MLCD_RST, GPIO_LEVEL_LOW);

	mdelay(1);
	gpio_set_value(OMAP_GPIO_MLCD_RST, GPIO_LEVEL_HIGH);
	mdelay(5);

	//MLCD pin set to InputPulldown.
	omap_ctrl_writew(0x010C, 0x1c6);

	if (current_panel == 0) // Sony
		nt35510_ldi_poweron_sony();
	else if (current_panel == 1 || current_panel == 5) // Hitachi 20mA, 17mA
		nt35510_ldi_poweron_hitachi();
	else if (current_panel == 2) // Hydis
		nt35510_ldi_poweron_hydis();
	else if (current_panel == 3) // SMD
		nt35510_ldi_poweron_smd();
	else if (current_panel == 4) // Sony(a-Si)
		nt35510_ldi_poweron_sony_a_si();
}

static void aat1402_set_brightness(int current_intensity)
{

#if LCD_DEBUG
	printk(KERN_DEBUG " *** aat1402_set_brightness : %d\n", current_intensity);
#endif
	if (current_panel == 1 || current_panel == 5) {  // if Hitachi 20mA, 17mA
		int orig_intensity = current_intensity;

		if (current_panel == 5) // Hitachi(17mA)
			if (current_intensity > 26)
				current_intensity = current_intensity - current_intensity/6 + 3;
#if LCD_DEBUG
		printk(KERN_DEBUG " HITACHI PANEL(%d)! orig_intensity=%d, current_intensity=%d\n", current_panel, orig_intensity, current_intensity);
#endif
		spi1writeindex(0xB0);
		spi1writedata(0x02);

		spi1writeindex(0xB9);
		spi1writedata(0x00);
		spi1writedata(current_intensity);  // PWM control. default brightness : 108
		spi1writedata(0x02);
		spi1writedata(0x08);

		spi1writeindex(0xB0);
		spi1writedata(0x03);

		current_intensity = orig_intensity;
	} else if (current_panel == 3) { // if SMD
		spi1writeindex(0xB4);
		spi1writedata(0x0F);
		spi1writedata(0x00);
		spi1writedata(0x50);

		spi1writeindex(0xB5);
		spi1writedata(current_intensity);

		spi1writeindex(0xB7);
		spi1writedata(0x24);

		spi1writeindex(0xB8);
		spi1writedata(0x01);
	} else  { // Sony, Hydis, Sony(a-Si)
		spi1writeindex(0x51);
		spi1writedata(current_intensity);
	}
}

static int aat1402_bl_get_intensity(struct backlight_device *bd)
{
	if (bd->props.fb_blank == FB_BLANK_UNBLANK &&
	    bd->props.power == FB_BLANK_UNBLANK)
		return bd->props.brightness;

	return 0;
}

static int aat1402_bl_set_intensity(struct backlight_device *bd)
{
	int level;

	if (atomic_read(&ldi_power_state) == POWER_OFF)
		return -EPERM;

	if (bd->props.fb_blank == FB_BLANK_UNBLANK &&
	    bd->props.power == FB_BLANK_UNBLANK)
		level = bd->props.brightness;
	else
		level = 0;

	aat1402_set_brightness(level);

	return 0;
}

static struct backlight_ops aat1402_bl_ops = {
	.get_brightness = aat1402_bl_get_intensity,
	.update_status	= aat1402_bl_set_intensity,
};

static int nt35510_spi_probe(struct spi_device *spi)
{
	struct backlight_properties props;
	struct backlight_device *bd;
	int status = 0;

	dev_dbg(&spi->dev, "nt35510_spi_probe\n");

	nt35510lcd_spi = spi;
	nt35510lcd_spi->mode = SPI_MODE_0;
	nt35510lcd_spi->bits_per_word = 9;

	dev_dbg(&spi->dev, "chip_select = %x, mode = %x\n", nt35510lcd_spi->chip_select, nt35510lcd_spi->mode);
	dev_dbg(&spi->dev, "max_speed_hz = %x, modalias = %s", nt35510lcd_spi->max_speed_hz, nt35510lcd_spi->modalias);

	status = spi_setup(nt35510lcd_spi);
	dev_dbg(&spi->dev, "spi_setup ret = %x\n", status);

	omap_dss_register_driver(&nt35510_driver);

	props.type = BACKLIGHT_RAW;
	bd = backlight_device_register("omap_bl", &spi->dev, NULL, &aat1402_bl_ops, &props);
	bd->props.fb_blank = FB_BLANK_UNBLANK;
	bd->props.power = FB_BLANK_UNBLANK;
	bd->props.max_brightness = 255;
	bd->props.brightness = 108;

	return 0;
}

static int nt35510_spi_remove(struct spi_device *spi)
{
	omap_dss_unregister_driver(&nt35510_driver);
	return 0;
}

static void nt35510_spi_shutdown(struct spi_device *spi)
{
	dev_dbg(&spi->dev, "first power off LCD\n");
	is_nt35510_spi_shutdown = 1;
	nt35510_lcd_poweroff();
	dev_dbg(&spi->dev, "power off - backlight.\n");
	gpio_set_value(OMAP_GPIO_LCD_EN_SET, GPIO_LEVEL_LOW);
}

static struct spi_driver nt35510_spi_driver = {
	.probe    = nt35510_spi_probe,
	.remove   = nt35510_spi_remove,
	.shutdown = nt35510_spi_shutdown,
	.driver   = {
		.name   = "nt35510_disp_spi",
		.bus    = &spi_bus_type,
		.owner  = THIS_MODULE,
	},
};

static int __init nt35510_lcd_init(void)
{
	return spi_register_driver(&nt35510_spi_driver);
}

static void __exit nt35510_lcd_exit(void)
{
	return spi_unregister_driver(&nt35510_spi_driver);
}

module_init(nt35510_lcd_init);
module_exit(nt35510_lcd_exit);
MODULE_LICENSE("GPL");
