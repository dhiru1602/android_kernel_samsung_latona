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
#include <plat/mux_latona_rev_r08.h>

#include "board-latona-mtd.h"

#define LATONA_NAND_CS    0
#define LATONA_WIFI_PMENA_GPIO		160
#define LATONA_WIFI_IRQ_GPIO		99

extern int  __init latona_debugboard_init(void);
extern void __init latona_peripherals_init(void);
extern void __init latona_display_init(void);
extern void __init latona_phone_svnet_init(void);
extern void __init latona_battery_init(void);
extern int __init latona_reboot_init(void);
extern int __init latona_reboot_post_init(void);
extern int sec_common_update_reboot_reason(char mode, const char *cmd);

/* Reboot modes */
#define REBOOTMODE_NORMAL 		(1 << 0)
#define REBOOTMODE_RECOVERY 		(1 << 1)
#define REBOOTMODE_FOTA 		(1 << 2)
#define REBOOTMODE_KERNEL_PANIC		(1 << 3)
#define REBOOTMODE_SHUTDOWN		(1 << 4)
#define REBOOTMODE_DOWNLOAD             (1 << 5)
#define REBOOTMODE_USER_PANIC 		(1 << 6)
#define REBOOTMODE_CP_CRASH		(1 << 9)
#define REBOOTMODE_FORCED_UPLOAD	(1 << 10)
#define REBOOT_MODE_NONE		0
#define REBOOT_MODE_FACTORYTEST		1
#define REBOOT_MODE_RECOVERY		2
#define REBOOT_MODE_ARM11_FOTA		3
#define REBOOT_MODE_DOWNLOAD		4
#define REBOOT_MODE_CHARGING		5
#define REBOOT_MODE_ARM9_FOTA		6
#define REBOOT_MODE_CP_CRASH		7
/* End: Reboot modes */




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
