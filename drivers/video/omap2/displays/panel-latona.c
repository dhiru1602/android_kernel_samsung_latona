/*
 * Latona panel support (Hydis WVGA with NT35510 controller)
 *
 * Author:
 * Mark "Hill Beast" Kennard <komcomputers@gmail.com>
 * Phinitnan "Crackerizer" Chanasabaeng <phinitnan_c@xtony.us>
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
#include <linux/spi/spi.h>
#include <linux/delay.h>
#include <linux/backlight.h>
#include <linux/regulator/consumer.h>
#include <linux/gpio.h>

#include <video/omapdss.h>

#define LATONA_PANEL_ID								0
#define LATONA_PANEL_BL_LV						127
#define LATONA_PANEL_BL_MAX_LV				255

/* SPI for LCD */
static struct spi_device *latona_panel_spi;

#define LP_SPI_CMD(cmd) \
  spi_write(latona_panel_spi, (unsigned char*)(0x0000 | cmd), 2)
#define LP_SPI_DAT(dat) \
  spi_write(latona_panel_spi, (unsigned char*)(0x0100 | dat), 2)

/* LCD regulator */
static struct regulator *latona_panel_vreg18;
static struct regulator *latona_panel_vreg28;

/* LCD backlight */
static struct backlight_device *latona_panel_bl;

/* LCD parameters */
static struct omap_video_timings latona_panel_timings = {
	.x_res          = 480,
	.y_res          = 800,
	.pixel_clock    = 24000,
	.hfp            = 10,
	.hsw            = 10,
	.hbp            = 10,
	.vfp            = 4,
	.vsw            = 2,
	.vbp            = 9
};

/**************************** Latona panel ops *******************************/
void latona_panel_init(void)
{    
  LP_SPI_CMD(0x11);   /* sleep power out */
  
  msleep(10);

	/* setup LCD */
  LP_SPI_CMD(0x3A);   /* interface pixel format */
  LP_SPI_DAT(0x77);   /* 24 bpp */

	LP_SPI_CMD(0x3B);   /* ????? - not defined in the datasheet */
	LP_SPI_DAT(0x07);
	LP_SPI_DAT(0x0A);
	LP_SPI_DAT(0x0E);
	LP_SPI_DAT(0x0A);
	LP_SPI_DAT(0x0A);

	LP_SPI_CMD(0x2A);   /* column index */
	LP_SPI_DAT(0x00);   /* start index - 0px */
	LP_SPI_DAT(0x00);
	LP_SPI_DAT(0x01);   /* end index - 479px */
	LP_SPI_DAT(0xDF);

	LP_SPI_CMD(0x2B);   /* row index */
	LP_SPI_DAT(0x00);   /* start - 0px */
	LP_SPI_DAT(0x00);
	LP_SPI_DAT(0x03);   /* end - 799px */
	LP_SPI_DAT(0x1F);	
  
  LP_SPI_CMD(0x36);   /* memory data access control */
  LP_SPI_DAT(0xD4);   /* See the datasheet */   
  
  LP_SPI_CMD(0x55);   /* CABC mode */
	LP_SPI_DAT(0x01);   /* User interface image */

  LP_SPI_CMD(0x5E);   /* min CABC brightness */
	LP_SPI_DAT(0x00);  

	LP_SPI_CMD(0x53);   /* Display control */
	LP_SPI_DAT(0x2C);   /* see the datasheet */
	
	LP_SPI_CMD(0x29);   /* Display on */
}

void latona_panel_exit(void)
{
	LP_SPI_CMD(0x28);   /* Display off */
	msleep(1);	
	LP_SPI_CMD(0x10);	  /* Sleep in */
	msleep(150);	
}

int latona_panel_power_up(struct omap_dss_device *dssdev)
{
  if(dssdev->state == OMAP_DSS_DISPLAY_ACTIVE)
    return 0;
    
	/* enable both regulator */
	if(regulator_enable(latona_panel_vreg28))
	 {
		dev_err(&dssdev->dev, "[%s] failed to enable LCD 2.8V regulator\n", 
		                      __func__);		                      
		return -ENODEV;
	 }
	
	if(regulator_enable(latona_panel_vreg18))
	 {
		dev_err(&dssdev->dev, "[%s] failed to enable LCD 1.8V regulator\n",
		                      __func__);		                      
		return -ENODEV;
	 }

	/* waiting for power up */
	msleep(150);

	/* reset sequence */
	gpio_set_value(dssdev->reset_gpio, 0);	
	msleep(1);
	gpio_set_value(dssdev->reset_gpio, 1);
	msleep(10);
	
	return 0;
}

void latona_panel_power_down(void)
{
  regulator_put(latona_panel_vreg28);
  regulator_put(latona_panel_vreg18);
}

void latona_panel_set_brightness(void)
{
  LP_SPI_CMD(0x51);
  LP_SPI_DAT(latona_panel_bl->props.brightness);
}

/************************* Latona panel backlight ****************************/
static int latona_panel_bl_set_brightness(struct backlight_device *bd)
{
  latona_panel_bl->props.brightness = bd->props.brightness;
  
  latona_panel_set_brightness(); 

	return 0;
}

static int latona_panel_bl_get_brightness(struct backlight_device *bd)
{
  return latona_panel_bl->props.brightness;
}

static struct backlight_ops latona_panel_bl_ops = {
	.get_brightness = latona_panel_bl_get_brightness,
	.update_status  = latona_panel_bl_set_brightness,
};

/************************** Latona panel driver ******************************/
static int latona_panel_probe(struct omap_dss_device *dssdev)
{
  struct backlight_properties props;

	/* setup LCD regulator */
	latona_panel_vreg28 = regulator_get(&dssdev->dev, 
	                                    "latona_panel_vreg28");

	if(IS_ERR(latona_panel_vreg28))
	 {
		dev_err(&dssdev->dev, "[%s] failed to get LCD 2.8V regulator\n", __func__);
		return -ENODEV;
	 }

	latona_panel_vreg18 = regulator_get(&dssdev->dev, 
	                                    "latona_panel_vreg18");

	if(IS_ERR(latona_panel_vreg18))
	 {
		dev_err(&dssdev->dev, "[%s] failed to get LCD 1.8V regulator\n", __func__);
		return -ENODEV;
	 }

	/* register LCD reset pin */
	if (gpio_request(dssdev->reset_gpio, "MLCD_RST") < 0)
	 {
		dev_err(&dssdev->dev, "[%s] failed to request LCD reset GPIO\n", __func__);
		return -ENODEV;
	 }

	gpio_direction_output(dssdev->reset_gpio, 1);

	/* setup LCD parameter */
	dssdev->panel.config = OMAP_DSS_LCD_TFT | OMAP_DSS_LCD_IVS |
		OMAP_DSS_LCD_IPC | OMAP_DSS_LCD_IHS | OMAP_DSS_LCD_ONOFF | OMAP_DSS_LCD_IEO;

	dssdev->panel.acb = 0;
	dssdev->panel.timings = latona_panel_timings;

	props.brightness = LATONA_PANEL_BL_LV;
	props.max_brightness = LATONA_PANEL_BL_MAX_LV;

	/* register backlight device */
	if((latona_panel_bl = backlight_device_register("latona_panel_bl", 
	                     &dssdev->dev, NULL, 
	                     &latona_panel_bl_ops, &props)))
	 {
		dev_err(&dssdev->dev, "[%s] failed to register backlight device\n",
		                    __func__);
		//return -ENODEV;
	 }
	
	return 0;
}

static void latona_panel_remove(struct omap_dss_device *dssdev)
{
  latona_panel_exit();
	latona_panel_power_down();
	backlight_device_unregister(latona_panel_bl);
	gpio_free(dssdev->reset_gpio);
	
	dssdev->state = OMAP_DSS_DISPLAY_DISABLED;
}

static int latona_panel_enable(struct omap_dss_device *dssdev)
{
	int status;

	if(dssdev->platform_enable)
	 {
		if((status = dssdev->platform_enable(dssdev)))
		 {
			omapdss_dpi_display_disable(dssdev);
			return status;
		 }
   }				

	if((status = omapdss_dpi_display_enable(dssdev)))
	 {
		dev_err(&dssdev->dev, "[%s] failed to enable dpi display\n",
		                    __func__);
		return status;	  
	 }
	
	if((status = latona_panel_power_up(dssdev)))
    return status;
    
	latona_panel_init();
	latona_panel_set_brightness();
		
	dssdev->state = OMAP_DSS_DISPLAY_ACTIVE;

	return 0;
}

static void latona_panel_disable(struct omap_dss_device *dssdev)
{
  if(dssdev->state != OMAP_DSS_DISPLAY_ACTIVE)
		return;
		
	latona_panel_exit();
	latona_panel_power_down();
	
	if (dssdev->platform_disable)
		dssdev->platform_disable(dssdev);
		
	mdelay(4);

	omapdss_dpi_display_disable(dssdev);

	dssdev->state = OMAP_DSS_DISPLAY_DISABLED;
}

static int latona_panel_suspend(struct omap_dss_device *dssdev)
{  
  latona_panel_disable(dssdev);
	dssdev->state = OMAP_DSS_DISPLAY_SUSPENDED;
	
	return 0;
}

static int latona_panel_resume(struct omap_dss_device *dssdev)
{
  return latona_panel_enable(dssdev);
}

static struct omap_dss_driver latona_panel_driver = {
	.probe          = latona_panel_probe,
	.remove         = latona_panel_remove,
	.enable         = latona_panel_enable,
	.disable        = latona_panel_disable,
	.suspend        = latona_panel_suspend,
	.resume         = latona_panel_resume,

	.driver		= {
		.name	= "latona_panel",
		.owner 	= THIS_MODULE,
	},
};

/************************ Latona panel SPI driver ****************************/
static int latona_panel_spi_probe(struct spi_device *spi)
{	
  int status;
  
	latona_panel_spi = spi;
	latona_panel_spi->mode = SPI_MODE_0;
	latona_panel_spi->bits_per_word = 16;	
	
	/* setup spi */
	status = spi_setup(latona_panel_spi);
	
	if(status < 0)
	 {
    dev_err(&spi->dev, "[%s] SPI setup failed %s, status %d\n",
        __func__, dev_name(&latona_panel_spi->dev), status);
		
		return status;
	 }
	
	/* register panel */
	status = omap_dss_register_driver(&latona_panel_driver);

	if (status < 0)
   {
    dev_err(&spi->dev, "[%s] OMAP DSS register failed %s, status %d\n",
        __func__, dev_name(&latona_panel_spi->dev), status);
		
		return status;   
   }	

	return 0;
}

static int latona_panel_spi_remove(struct spi_device *spi)
{	
	omap_dss_unregister_driver(&latona_panel_driver);
	
	return 0;
}

static struct spi_driver latona_panel_spi_driver = {
	.probe    = latona_panel_spi_probe,
	.remove   = __devexit_p(latona_panel_spi_remove),
	.driver   = {
		.name   = "latona_panel_spi",
		.bus    = &spi_bus_type,
		.owner  = THIS_MODULE,
	},
};

/****************************** Module register *****************************/
static int __init latona_lcd_init(void)
{
   	return spi_register_driver(&latona_panel_spi_driver);
}

static void __exit latona_lcd_exit(void)
{
	return spi_unregister_driver(&latona_panel_spi_driver);
}

module_init(latona_lcd_init);
module_exit(latona_lcd_exit);
MODULE_LICENSE("GPL");
