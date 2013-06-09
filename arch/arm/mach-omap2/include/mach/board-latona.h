/*
 * Modified from mach-omap2/include/mach/board-zoom.h for Samsung Latona board
 *
 * Dheeraj CVR "dhiru1602" <cvr.dheeraj@gmail.com>
 * Mark "Hill Beast" Kennard <komcomputers@gmail.com>
 * crackerizer <github.com/crackerizer>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * Defines for latona boards
 */
#include <video/omapdss.h>

#include "board-latona-mtd.h"
#include "board-latona-mux_r08.h"

#define LATONA_WIFI_PMENA_GPIO		160
#define LATONA_WIFI_IRQ_GPIO		99

extern int  __init latona_debugboard_init(void);
extern void __init latona_peripherals_init(void);
extern void __init latona_display_init(void);
extern void __init latona_phone_svnet_init(void);
extern void __init latona_battery_init(void);
extern short int get_headset_status(void);
extern void __init latona_cmdline_set_serialno(void);

/* GPIO SWITCH */
#define HEADSET_DISCONNET			0
#define HEADSET_3POLE				2 
#define HEADSET_4POLE_WITH_MIC	1

#define EAR_MIC_BIAS_GPIO OMAP_GPIO_EAR_MICBIAS_EN
#define EAR_KEY_GPIO OMAP_GPIO_EAR_SEND_END 
#define EAR_DET_GPIO OMAP_GPIO_DET_3_5
#define EAR_DETECT_INVERT_ENABLE 1

#define EAR_KEY_INVERT_ENABLE 1

#define EAR_ADC_SEL_GPIO OMAP_GPIO_EARPATH_SEL
#define USE_ADC_SEL_GPIO 1
/* End: GPIO SWITCH */
