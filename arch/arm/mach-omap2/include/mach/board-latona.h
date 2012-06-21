/*
 * Modified from mach-omap2/include/mach/board-zoom.h for Samsung Latona board
 *
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

#define LATONA_NAND_CS    0

extern int __init latona_debugboard_init(void);
extern void __init latona_peripherals_init(void);
extern void __init latona_display_init(void);

#if (defined(CONFIG_VIDEO_IMX046) || defined(CONFIG_VIDEO_IMX046_MODULE)) && \
	defined(CONFIG_VIDEO_OMAP3)
#include <media/imx046.h>
extern struct imx046_platform_data latona_imx046_platform_data;
#endif

#ifdef CONFIG_VIDEO_OMAP3
extern void latona_cam_init(void);
#else
#define latona_cam_init()	NULL
#endif

#if (defined(CONFIG_VIDEO_LV8093) || defined(CONFIG_VIDEO_LV8093_MODULE)) && \
	defined(CONFIG_VIDEO_OMAP3)
#include <media/lv8093.h>
extern struct lv8093_platform_data latona_lv8093_platform_data;
#define LV8093_PS_GPIO		7
/* GPIO7 is connected to lens PS pin through inverter */
#define LV8093_PWR_OFF		1
#define LV8093_PWR_ON		(!LV8093_PWR_OFF)
#endif

#define LATONA_HEADSET_EXTMUTE_GPIO	153
