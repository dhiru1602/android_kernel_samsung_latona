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
extern int latona_update_reboot_reason(char mode, const char *cmd);
extern short int get_headset_status(void);

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
/* End: Reboot modes */

/* Default Parameter Values */
#define PARAM_MAGIC			0x72726624
#define PARAM_VERSION			0x13	/* Rev 1.3 */
#define PARAM_STRING_SIZE		1024	/* 1024 Characters */

#define MAX_PARAM			20
#define MAX_STRING_PARAM		5

#define SERIAL_SPEED			7	/* Baudrate */
#define LCD_LEVEL			0x061	/* Backlight Level */
#define BOOT_DELAY			0	/* Boot Wait Time */
#define LOAD_RAMDISK			0	/* Enable Ramdisk Loading */
#define SWITCH_SEL			1	/* Switch Setting (UART[1], USB[0]) */
#define PHONE_DEBUG_ON			0	/* Enable Phone Debug Mode */
#define LCD_DIM_LEVEL			0x011	/* Backlight Dimming Level */
#define LCD_DIM_TIME			0
#define MELODY_MODE			0	/* Melody Mode */
#define REBOOT_MODE			0	/* Reboot Mode */
#define NATION_SEL			0	/* Language Configuration */
#define LANGUAGE_SEL			0
#define SET_DEFAULT_PARAM		0	/* Set Param to Default */
#define VERSION_LINE			"I8315XXIE00"	/* Set Image Info */
#define COMMAND_LINE			"console=ttySAC2,115200"
#define BOOT_VERSION			" version=Sbl(1.0.0) "

typedef enum {
	__SERIAL_SPEED,
	__LOAD_RAMDISK,
	__BOOT_DELAY,
	__LCD_LEVEL,
	__SWITCH_SEL,
	__PHONE_DEBUG_ON,
	__LCD_DIM_LEVEL,
	__LCD_DIM_TIME,
	__MELODY_MODE,
	__REBOOT_MODE,
	__NATION_SEL,
	__LANGUAGE_SEL,
	__SET_DEFAULT_PARAM,
	__DEBUG_BLOCKPOPUP,
	__PARAM_INT_14,		/* Reserved. */
	__VERSION,
	__CMDLINE,
	__PARAM_STR_2,
	__DEBUG_LEVEL,
	__PARAM_STR_4		/* Reserved. */
} param_idx;

typedef struct _param_int_t {
	param_idx ident;
	int value;
} param_int_t;

typedef struct _param_str_t {
	param_idx ident;
	char value[PARAM_STRING_SIZE];
} param_str_t;

typedef struct {
	int param_magic;
	int param_version;
	param_int_t param_list[MAX_PARAM - MAX_STRING_PARAM];
	param_str_t param_str_list[MAX_STRING_PARAM];
} status_t;

extern void (*latona_set_param_value) (int idx, void *value);
extern void (*latona_get_param_value) (int idx, void *value);

#define USB_SEL_MASK			(1 << 0)
#define UART_SEL_MASK			(1 << 1)
/* END: Default Parameter Values */
