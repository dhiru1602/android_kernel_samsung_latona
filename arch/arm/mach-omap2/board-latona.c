/*
 * Copyright (C) 2009-2010 Texas Instruments Inc.
 * Mikkel Christensen <mlc@ti.com>
 * Felipe Balbi <balbi@ti.com>
 *
 * Modified from mach-omap2/board-zoom.c for Samsung Latona board
 *
 * Aditya Patange aka "Adi_Pat" <adithemagnficent@gmail.com> 
 * Mark "Hill Beast" Kennard <komcomputers@gmail.com>
 * Phinitnan "Crackerizer" Chanasabaeng <phinitnan_c@xtony.us>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/platform_device.h>
#include <linux/input.h>
#include <linux/gpio.h>
#include <linux/i2c/twl.h>
#include <linux/memblock.h>
#include <linux/skbuff.h>
#include <linux/ti_wilink_st.h>
#include <linux/wl12xx.h>

#include <asm/setup.h>
#include <asm/mach-types.h>
#include <asm/mach/arch.h>
#include <asm/sizes.h>
#include <plat/common.h>
#include <plat/board.h>
#include <plat/usb.h>

#include <mach/board-latona.h>

#include "board-flash.h"
#include "mux.h"
#include "sdram-qimonda-hyb18m512160af-6.h"
#include "omap_ion.h"

#define LATONA_BT_RESET_GPIO             109

#define WILINK_UART_DEV_NAME            "/dev/ttyO1"

#ifdef CONFIG_OMAP_MUX
extern struct omap_board_mux *latona_board_mux_ptr;
extern struct omap_board_mux *latona_board_wk_mux_ptr;
#else
#define latona_board_mux_ptr		NULL
#define latona_board_wk_mux_ptr		NULL
#endif

static void __init latona_init_early(void)
{
	omap2_init_common_infrastructure();
	omap2_init_common_devices(hyb18m512160af6_sdrc_params,
					  hyb18m512160af6_sdrc_params);
}

static const struct usbhs_omap_board_data usbhs_bdata __initconst = {
	.port_mode[0]		= OMAP_USBHS_PORT_MODE_UNUSED,
	.port_mode[1]		= OMAP_EHCI_PORT_MODE_PHY,
	.port_mode[2]		= OMAP_USBHS_PORT_MODE_UNUSED,
	.phy_reset		= true,
	.reset_gpio_port[0]	= -EINVAL,
	.reset_gpio_port[1]	= 64,
	.reset_gpio_port[2]	= -EINVAL,
};

static int plat_kim_suspend(struct platform_device *pdev, pm_message_t state)
{
	/* TODO: wait for HCI-LL sleep */
	return 0;
}
static int plat_kim_resume(struct platform_device *pdev)
{
	return 0;
}

/* wl127x BT, FM, GPS connectivity chip */
struct ti_st_plat_data wilink_pdata = {
	.nshutdown_gpio = 109,
	.dev_name = WILINK_UART_DEV_NAME,
	.flow_cntrl = 1,
	.baud_rate = 3686400,
	.suspend = plat_kim_suspend,
	.resume = plat_kim_resume,
};
static struct platform_device wl127x_device = {
	.name           = "kim",
	.id             = -1,
	.dev.platform_data = &wilink_pdata,
};
static struct platform_device btwilink_device = {
	.name = "btwilink",
	.id = -1,
};

static struct platform_device *latona_devices[] __initdata = {
	&wl127x_device,
	&btwilink_device,
};

/* Fix to prevent VIO leakage on wl127x */
static int wl127x_vio_leakage_fix(void)
{
	int ret = 0;

	pr_info(" wl127x_vio_leakage_fix\n");

	ret = gpio_request(LATONA_BT_RESET_GPIO, "wl127x_bten");
	if (ret < 0) {
		pr_err("wl127x_bten gpio_%d request fail",
			LATONA_BT_RESET_GPIO);
		goto fail;
	}

	gpio_direction_output(LATONA_BT_RESET_GPIO, 1);
	mdelay(10);
	gpio_direction_output(LATONA_BT_RESET_GPIO, 0);
	udelay(64);

	gpio_free(LATONA_BT_RESET_GPIO);
fail:
	return ret;
}

static struct wl12xx_platform_data latona_wlan_data __initdata = {
	.irq = OMAP_GPIO_IRQ(LATONA_WIFI_IRQ_GPIO),
	.board_ref_clock = WL12XX_REFCLOCK_38,
};

static void latona_wifi_init(void)
{
	if (wl12xx_set_platform_data(&latona_wlan_data))
		pr_err("Error setting wl12xx data\n");
}

static void __init latona_init(void)
{
	omap3_mux_init(latona_board_mux_ptr, OMAP_PACKAGE_CBP);
	latona_mux_init_gpio_out();
	latona_mux_set_wakeup_gpio();

	usbhs_init(&usbhs_bdata);

	latona_wifi_init();
	latona_peripherals_init();
	latona_display_init();
	omap_register_ion();
	/* Added to register latona devices */
	platform_add_devices(latona_devices, ARRAY_SIZE(latona_devices));
	wl127x_vio_leakage_fix();
}

static void __init latona_reserve(void)
{
	/* do the static reservations first */
	memblock_remove(OMAP3_PHYS_ADDR_SMC_MEM, PHYS_ADDR_SMC_SIZE);

#ifdef CONFIG_ION_OMAP
	omap_ion_init();
#endif
	omap_reserve();
}

static void __init latona_fixup(struct machine_desc *desc,
				    struct tag *tags, char **cmdline,
				    struct meminfo *mi)
{
	mi->nr_banks = 2;

	mi->bank[0].start = 0x80000000;
	mi->bank[0].size = SZ_256M;

	mi->bank[1].start = 0x90000000;
	mi->bank[1].size = SZ_256M;
}

MACHINE_START(LATONA, "LATONA")
	.boot_params	= 0x80000100,
	.fixup          = latona_fixup,
	.reserve	= latona_reserve,
	.map_io		= omap3_map_io,
	.init_early	= latona_init_early,
	.init_irq	= omap_init_irq,
	.init_machine	= latona_init,
	.timer		= &omap_timer,
MACHINE_END
