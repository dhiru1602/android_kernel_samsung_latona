/* arch/arm/mach-omap2/include/mach/board-latona-mtd.h
*
* Partition Layout for Samsung Galaxy SL (I9003)
* Author : Dheeraj CVR (dhiru1602)
*
*/

#include <plat/onenand.h>

/*
==== PARTITION INFORMATION ====
 ID         : IBL+PBL (0x0)
 ATTR       : RO SLC (0x1002)
 FIRST_UNIT : 0
 NO_UNITS   : 1
===============================
 ID         : PIT (0x1)
 ATTR       : RO SLC (0x1002)
 FIRST_UNIT : 1
 NO_UNITS   : 1
===============================
 ID         : EFS (0x14)
 ATTR       : RW STL SLC (0x1101)
 FIRST_UNIT : 2
 NO_UNITS   : 40
===============================
 ID         : SBL (0x3)
 ATTR       : RO SLC (0x1002)
 FIRST_UNIT : 42
 NO_UNITS   : 5
===============================
 ID         : SBL2 (0x4)
 ATTR       : RO SLC (0x1002)
 FIRST_UNIT : 47
 NO_UNITS   : 5
===============================
 ID         : PARAM (0x15)
 ATTR       : RW STL SLC (0x1101)
 FIRST_UNIT : 52
 NO_UNITS   : 30
===============================
 ID         : NORMALBOOT (0x5)
 ATTR       : RO SLC (0x1002)
 FIRST_UNIT : 82
 NO_UNITS   : 32
===============================
 ID         : RECOVERY (0x8)
 ATTR       : RO SLC (0x1002)
 FIRST_UNIT : 114
 NO_UNITS   : 32
===============================
 ID         : SYSTEM (0x16)
 ATTR       : RW STL SLC (0x1101)
 FIRST_UNIT : 146
 NO_UNITS   : 1334
===============================
 ID         : USERDATA (0x17)
 ATTR       : RW STL SLC (0x1101)
 FIRST_UNIT : 1480
 NO_UNITS   : 312
===============================
 ID         : CACHE (0x18)
 ATTR       : RW STL SLC (0x1101)
 FIRST_UNIT : 1792
 NO_UNITS   : 140
===============================
 ID         : FOTA (0x9)
 ATTR       : RO SLC (0x1002)
 FIRST_UNIT : 1932
 NO_UNITS   : 2
===============================
 ID         : MODEM (0xa)
 ATTR       : RO SLC (0x1002)
 FIRST_UNIT : 1934
 NO_UNITS   : 70
===============================

FSR VERSION: FSR_1.2.1p1_b139_RTM
minor position size units id
1: 0x00000000-0x00040000 0x00040000 1 0
2: 0x00040000-0x00080000 0x00040000 1 1
3: 0x00080000-0x00a80000 0x00a00000 40 20
4: 0x00a80000-0x00bc0000 0x00140000 5 3
5: 0x00bc0000-0x00d00000 0x00140000 5 4
6: 0x00d00000-0x01480000 0x00780000 30 21
7: 0x01480000-0x01c80000 0x00800000 32 5
8: 0x01c80000-0x02480000 0x00800000 32 8
9: 0x02480000-0x17200000 0x14d80000 1334 22
10: 0x17200000-0x1c000000 0x04e00000 312 23
11: 0x1c000000-0x1e300000 0x02300000 140 24
12: 0x1e300000-0x1e380000 0x00080000 2 9
13: 0x1e380000-0x1f500000 0x01180000 70 10

*/

static struct mtd_partition onenand_partitions[] = {
      {
		.name		= "boot",
		.offset		= (82*SZ_256K),
		.size		= (32*SZ_256K), //113
	},
	{
		.name		= "recovery",
		.offset		= (114*SZ_256K),
		.size		= (32*SZ_256K), //145
	},
	{
		.name		= "system",
		.offset		= (146*SZ_256K),
		.size		= (1734*SZ_256K), //1879
	},
	{
		.name		= "radio",
		.offset  	= (1880*SZ_256K),
		.size		= (74*SZ_256K), //1953
	},
	{     
		.name		= "efs",
		.offset		= (1954*SZ_256K),
		.size		= (50*SZ_256K), //2003
	},
	{       /* The reservoir area is used by Samsung's Block Management Layer (BML)
	           to map good blocks from this reservoir to bad blocks in user
	           partitions. A special tool (bml_over_mtd) is needed to write
	           partition images using bad block mapping.
	           Currently, this is required for flashing the "boot" partition,
	           as Samsung's stock bootloader expects BML partitions.*/
		.name		= "reservoir",
		.offset		= (2004*SZ_256K),
		.size	        = (44*SZ_256K), //2047
	},

};
