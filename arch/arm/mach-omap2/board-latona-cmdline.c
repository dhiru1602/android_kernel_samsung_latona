/* 
 * arch/arm/mach-omap2/board-latona-cmdline.c
 *
 * Copyright (C) 2011 LGE, Inc
 *
 * Dheeraj CVR "dhiru1602" <cvr.dheeraj@gmail.com>
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include <linux/kernel.h>
#include <linux/string.h>
#include <asm/setup.h>
#include <linux/string.h>
#include <linux/slab.h>
#include <linux/mm.h> 
#include <plat/io.h>

#include <mach/board-latona.h>

#define isspace(c)      (c == ' ' || c == '\t' || c == 10 || c == 13 || c == 0)

/*
 * Android expects the bootloader to pass the device serial number as a
 * parameter on the kernel command line.
 * Use the last 16 bytes of the DIE id as device serial number.
 */
#define L4_WK_34XX_PHYS		L4_WK_34XX_BASE	/* 0x48300000 --> 0xfa300000 */
#define DIE_ID_REG_BASE		(L4_WK_34XX_PHYS + 0xA000)
#define DIE_ID_REG_OFFSET	0x218

#define SERIALNO_PARAM		"androidboot.serialno"
void __init latona_cmdline_set_serialno(void)
{
	/*
	 * The final cmdline will have 16 digits, a space, an =, and a trailing
	 * \0 as well as the contents of saved_command_line and SERIALNO_PARAM
	*/
	size_t len = strlen(saved_command_line) + strlen(SERIALNO_PARAM) + 19;
	char *buf = kmalloc(len, GFP_ATOMIC);
	if (buf) {
		snprintf(buf, len, "%s %s=%08X%08X",
			saved_command_line,
			SERIALNO_PARAM,
			omap_readl(DIE_ID_REG_BASE + DIE_ID_REG_OFFSET),
			omap_readl(DIE_ID_REG_BASE + DIE_ID_REG_OFFSET + 0x4));
		saved_command_line = buf;
	}
}

#if !(defined(CONFIG_CMDLINE_EXTEND) || defined(CONFIG_CMDLINE_FORCE))
struct cmdline_parameter {
        char *key;
        char *value; /* If you want to remove key, set as "" */
        int new;     /* if you add new parameter, set as 1 */
};

static struct cmdline_parameter cmdline_parameters[] __initdata = {
        {"omapfb.vram", "0:5M", 0},
        {"androidboot.hardware", "latona", 1},
        {"mem", "512M", 1},
};

static char *matchstr(const char *s1, const char *s2)
{
        char *p, *p2;
        bool matched;

        p = s1;
        do {
                p = strstr(p, s2);
                if (p) {
                        matched = false;
                        if (p == s1)
                                matched = true;
                        else if (isspace(*(p-1)))
                                matched = true;

                        if (matched) {
                                p2 = p + strlen(s2);
                                if (isspace(*p2) || *p2 == '=')
                                        return p;
                        }
                        p += strlen(s2);
                }
        } while (p);

        return NULL;
}

static void __init fill_whitespace(char *s)
{
        char *p;

        for (p = s; !isspace(*p) && *p != '\0'; ++p)
                *p = ' ';
}

void __init latona_manipulate_cmdline(char *default_command_line)
{
        char *s, *p, *p2;
        int i;
        int ws;

        s = default_command_line;

        /* remove parameters */
        for (i = 0; i < ARRAY_SIZE(cmdline_parameters); i++) {
                p = s;
                do {
                        p = matchstr(p, cmdline_parameters[i].key);
                        if (p)
                                fill_whitespace(p);
                } while(p);
        }

        /* trim the whitespace */
        p = p2 = s;
        ws = 0; /* whitespace */
        for (i = 0; i < COMMAND_LINE_SIZE; i++) {
                if (!isspace(p[i])) {
                        if (ws) {
                                ws = 0;
                                *p2++ = ' ';
                        }
                        *p2++ = p[i];
                }
                else {
                        ws = 1;
                }
        }
        *p2 = '\0';
        /* add parameters */
        for (i = 0; i < ARRAY_SIZE(cmdline_parameters); i++) {
                static char param[COMMAND_LINE_SIZE];
                memset(param, 0, COMMAND_LINE_SIZE);
                if (strlen(cmdline_parameters[i].value) ||
                                cmdline_parameters[i].new) {
                        strcat(param, cmdline_parameters[i].key);
                        if (strlen(cmdline_parameters[i].value)) {
                                strcat(param, "=");
                                strcat(param, cmdline_parameters[i].value);
                        }
                }
                strlcat(s, " ", COMMAND_LINE_SIZE);
                strlcat(s, param, COMMAND_LINE_SIZE);
        }

}

void __init manipulate_cmdline(char *default_command_line,
		const char *tag_command_line, size_t size)
{
	strlcpy(default_command_line, tag_command_line, size);
	latona_manipulate_cmdline(default_command_line);
}
#endif
