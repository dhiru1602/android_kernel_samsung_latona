/*
 * Copyright (C) 2010 Texas Instruments Inc.
 *
 * Modified from mach-omap2/board-zoom-display.c
 *
 * Dheeraj CVR "dhiru1602" <cvr.dheeraj@gmail.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/platform_device.h>
#include <linux/gpio.h>
#include <linux/i2c/twl.h>
#include <linux/spi/spi.h>
#include <plat/mcspi.h>
#include <video/omapdss.h>
#include <plat/omap-pm.h>
#include "mux.h"
#include <plat/clock.h>
#include <linux/clk.h>

static struct clk *dss_sys_fclk;
bool init_clk_disable = true;

static int latona_panel_enable_lcd(struct omap_dss_device *dssdev)
{
	// Run the L3@400Mhz while the LCD is ON
	omap_pm_set_min_bus_tput(&(dssdev->dev), OCP_INITIATOR_AGENT , 400000 * 4);

	return 0;
}

static void dss_clks_disable(void)
{
    clk_disable(dss_sys_fclk);
}

static void latona_panel_disable_lcd(struct omap_dss_device *dssdev)
{
	omap_pm_set_min_bus_tput(&(dssdev->dev), OCP_INITIATOR_AGENT, -1);

	if (init_clk_disable) {
		dss_clks_disable();
		init_clk_disable = false;
	}
}

struct omap_dss_device omap_board_lcd_device = {
    .name = "lcd",
    .driver_name = "nt35510_panel",
    .type = OMAP_DISPLAY_TYPE_DPI,
    .channel = OMAP_DSS_CHANNEL_LCD,
    .phy.dpi.data_lines = 24,
    .platform_enable = latona_panel_enable_lcd,
    .platform_disable = latona_panel_disable_lcd,
};

static struct omap_dss_device *omap_board_dss_devices[] = {
	&omap_board_lcd_device,
};

static struct omap_dss_board_info omap_board_dss_data = {
	.num_devices = ARRAY_SIZE(omap_board_dss_devices),
	.devices = omap_board_dss_devices,
	.default_device = &omap_board_lcd_device,
};


//ZEUS_LCD
static struct omap2_mcspi_device_config board_lcd_mcspi_config = {
    .turbo_mode = 0,
    .single_channel = 1,    /* 0: slave, 1: master */
};

static struct spi_board_info board_spi_board_info[] __initdata = {

    [0] = {
           .modalias = "nt35510_disp_spi",
           .bus_num = 1,
           .chip_select = 0,
           .max_speed_hz = 375000,
           .controller_data = &board_lcd_mcspi_config,
           },     
};

void __init latona_display_init(void)
{
	dss_sys_fclk = omap_clk_get_by_name("dss1_alwon_fck");
	if (IS_ERR(dss_sys_fclk)) {
		pr_err("Could not get dss system clock\n");
		/* return -ENOENT; */
	}
	clk_enable(dss_sys_fclk);

	omap_display_init(&omap_board_dss_data);
	spi_register_board_info(board_spi_board_info,
				ARRAY_SIZE(board_spi_board_info));
	
}

