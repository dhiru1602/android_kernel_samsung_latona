/*
 * board-latona-phone_svnet.c
 *
 * Dheeraj CVR "dhiru1602" <cvr.dheeraj@gmail.com>
 * Aditya Patange "Adi_Pat" <adithemagnificent@gmail.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * Includes all necessary ipc_spi and modemctl data and init function. 
 *
 */ 

#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/irq.h>
#include <linux/phone_svn/modemctl.h>
#include <linux/phone_svn/ipc_spi.h>
#include <linux/spi/spi.h>
#include <linux/gpio.h>
#include <linux/irq.h>
#include <plat/mcspi.h>
#include <mach/board-latona.h>

/* MODEMCTL */ 

static void modemctl_cfg_gpio(void);
static void ipc_spi_cfg_gpio(void);

static struct modemctl_platform_data mdmctl_data = {
	.name = "xmm",
	.gpio_phone_active = OMAP_GPIO_PHONE_ACTIVE,
	.gpio_pda_active = OMAP_GPIO_PDA_ACTIVE,
	.gpio_cp_reset = OMAP_GPIO_CP_RST, // cp_rst gpio - 43
	.gpio_reset_req_n = OMAP_GPIO_RESET_REQ_N,
	.gpio_con_cp_sel = OMAP_GPIO_CON_CP_SEL,
	.cfg_gpio = modemctl_cfg_gpio,
};

static void modemctl_cfg_gpio( void )
{
	int err = 0;
	
	unsigned gpio_cp_rst = mdmctl_data.gpio_cp_reset;
	unsigned gpio_pda_active = mdmctl_data.gpio_pda_active;
	unsigned gpio_phone_active = mdmctl_data.gpio_phone_active;
	unsigned gpio_reset_req_n = mdmctl_data.gpio_reset_req_n;
	unsigned gpio_con_cp_sel = mdmctl_data.gpio_con_cp_sel;

	gpio_free( gpio_cp_rst );
	err = gpio_request( gpio_cp_rst, "CP_RST" );
	if( err ) {
		printk( "modemctl_cfg_gpio - fail to request gpio %s : %d\n", "CP_RST", err );
	}
	else {
		gpio_direction_output( gpio_cp_rst, 1 );
	}

	gpio_free( gpio_pda_active );
	err = gpio_request( gpio_pda_active, "PDA_ACTIVE" );
	if( err ) {
		printk( "modemctl_cfg_gpio - fail to request gpio %s : %d\n", "PDA_ACTIVE", err );
	}
	else {
		gpio_direction_output( gpio_pda_active, 0 );
	}

	gpio_free( gpio_phone_active );
	err = gpio_request( gpio_phone_active, "PHONE_ACTIVE" );
	if( err ) {
		printk( "modemctl_cfg_gpio - fail to request gpio %s : %d\n", "PHONE_ACTIVE", err );
	}
	else {
		gpio_direction_input( gpio_phone_active );
	}

	gpio_free( gpio_reset_req_n );
	err = gpio_request( gpio_reset_req_n, "RESET_REQ_N" );
	if( err ) {
		printk( "modemctl_cfg_gpio - fail to request gpio %s : %d\n", "RESET_REQ_N", err );
	}
	else {
		gpio_direction_output( gpio_reset_req_n, 0 );
	}

	gpio_free( gpio_con_cp_sel );
	err = gpio_request( gpio_con_cp_sel, "CON_CP_SEL" );
	if( err ) {
		printk( "modemctl_cfg_gpio - fail to request gpio %s : %d\n", "CON_CP_SEL", err );
	}
	else {
		gpio_direction_output( gpio_con_cp_sel, 0 );
	}
	
	irq_set_irq_type( OMAP_GPIO_IRQ( OMAP_GPIO_PHONE_ACTIVE ), IRQ_TYPE_EDGE_BOTH );
}

static struct resource mdmctl_res[] = {
	[ 0 ] = {
		.start = OMAP_GPIO_IRQ( OMAP_GPIO_PHONE_ACTIVE ),
		.end = OMAP_GPIO_IRQ( OMAP_GPIO_PHONE_ACTIVE ),
		.flags = IORESOURCE_IRQ,
	},
};

/* IPC_SPI */

static struct ipc_spi_platform_data ipc_spi_data = {
	.gpio_ipc_mrdy = OMAP_GPIO_IPC_MRDY,
	.gpio_ipc_srdy = OMAP_GPIO_IPC_SRDY,	

	.cfg_gpio = ipc_spi_cfg_gpio,
};

static struct resource ipc_spi_res[] = {
	[ 0 ] = {
		.start = OMAP_GPIO_IRQ( OMAP_GPIO_IPC_SRDY ),
		.end = OMAP_GPIO_IRQ( OMAP_GPIO_IPC_SRDY ),
		.flags = IORESOURCE_IRQ,
	},
};

static void ipc_spi_cfg_gpio( void )
{
	int err = 0;
	
	unsigned gpio_ipc_mrdy = ipc_spi_data.gpio_ipc_mrdy;
	unsigned gpio_ipc_srdy = ipc_spi_data.gpio_ipc_srdy;

	// Mux Setting -> mux_xxxx_rxx.c

	gpio_free( gpio_ipc_mrdy );
	err = gpio_request( gpio_ipc_mrdy, "IPC_MRDY" );
	if( err ) {
		printk( "ipc_spi_cfg_gpio - fail to request gpio %s : %d\n", "IPC_MRDY", err );
	}
	else {
		gpio_direction_output( gpio_ipc_mrdy, 0 );
	}

	gpio_free( gpio_ipc_srdy );
	err = gpio_request( gpio_ipc_srdy, "IPC_SRDY" );
	if( err ) {
		printk( "ipc_spi_cfg_gpio - fail to request gpio %s : %d\n", "IPC_SRDY", err );
	}	
}

/* spi board data */

static struct omap2_mcspi_device_config board_ipc_spi_mcspi_config = {
	.turbo_mode     =   0,
	.single_channel =   1,
};

static struct spi_board_info board_spi_board_info[] __initdata = {
	[ 0 ] = {
		.modalias = "ipc_spi",
		.bus_num = 2,
		.chip_select = 0,
		.max_speed_hz = 24000000,
		.controller_data = &board_ipc_spi_mcspi_config,
	},

};


/* Platform data */

static struct platform_device ipc_spi = {
	.name = "onedram",
	.id = -1,
	.num_resources = ARRAY_SIZE( ipc_spi_res ),
	.resource = ipc_spi_res,
	.dev = {
		.platform_data = &ipc_spi_data,
	},
};

static struct platform_device modemctl = {
	.name = "modemctl",
	.id = -1,
	.num_resources = ARRAY_SIZE( mdmctl_res ),
	.resource = mdmctl_res,
	.dev = {
		.platform_data = &mdmctl_data,
	},
};

static struct platform_device *phone_svnet[] __initdata = {
	&ipc_spi,
	&modemctl,
};

void latona_phone_svnet_init(void)
{
	printk(KERN_DEBUG "%s\n",__func__);
	platform_add_devices(phone_svnet,ARRAY_SIZE(phone_svnet));
	spi_register_board_info(board_spi_board_info,ARRAY_SIZE(board_spi_board_info));	
}
