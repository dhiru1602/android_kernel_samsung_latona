#include <linux/module.h>
#include <linux/kernel_stat.h>
#include <linux/init.h>
#include <linux/time.h>
#include <linux/mutex.h>
#include <linux/interrupt.h>
#include <linux/random.h>
#include <linux/syscalls.h>
#include <linux/kthread.h>
//#include <linux/slab_def.h>
#include <linux/slab.h>
#include <linux/i2c.h>
#include <linux/slab.h>
#include <linux/clk.h>
#include <linux/device.h>
#include <linux/platform_device.h>

#include <linux/i2c/atmel_mxt_ts.h>


#include <asm/irq.h>
#include <asm/mach/irq.h>
//#include <asm/arch/gpio.h>
//#include <asm/arch/mux.h>
#include <plat/gpio.h>	//ryun
#include <plat/mux.h>	//ryun 

//#include <asm/arch/hardware.h>
#include <mach/hardware.h>	// ryun
#include <linux/types.h>

#include "atmel_touch.h"

#define __CONFIG_ATMEL__

#define I2C_M_WR 0	// ryun

#ifdef __CONFIG_ATMEL__
#define u8	__u8
#endif

static int i2c_tsp_sensor_attach_adapter(struct i2c_adapter *adapter);
static int i2c_tsp_sensor_probe_client(struct i2c_client *client,const struct i2c_device_id *id);
static int i2c_tsp_sensor_detach_client(struct i2c_client *client);

//#define TSP_SENSOR_ADDRESS		0x4b	// onegun test

//#define CYPRESS_TSP_SENSOR_ADDRESS			0x20
//#define SYNAPTICS_TSP_SENSOR_ADDRESS			0x2C
#define ATMEL_TSP_SENSOR_ADDRESS			0x4A


extern u32 hw_revision;
#define IS_ATMEL	1


#define I2C_DF_NOTIFY				0x01

/* Each client has this additional data */
struct qt602240_data 
{
    unsigned int irq;
    struct i2c_client *client;
    struct input_dev *input_dev;
    struct work_struct tsp_work;
    struct work_struct ta_work;
    struct qt602240_platform_data *pdata;
    struct qt602240_info *info;
    struct qt602240_object *object_table;
    struct qt602240_message *object_message;	
};


static const struct i2c_device_id qt602240_id[] = {
	{ "qt602240_ts", 0 },
	{ }
};
MODULE_DEVICE_TABLE(i2c, qt602240_id);

struct i2c_driver tsp_sensor_driver =
{
	//.name	= "tsp_sensor_driver",
	.driver	= {
		.name	= "qt602240_ts",
		.owner	= THIS_MODULE,
	},
	.probe		= i2c_tsp_sensor_probe_client,
	.remove		= i2c_tsp_sensor_detach_client,
	.id_table	= qt602240_id,
};




static struct qt602240_data *g_client;

#ifdef __CONFIG_ATMEL__
#define U16	unsigned short int
#define U8	unsigned char

#endif

#if 1
extern unsigned int g_i2c_debugging_enable;

int i2c_tsp_sensor_read(u16 reg, u8 *read_val, unsigned int len )
{
	int id;
	int 	 err, i;
	struct 	 i2c_msg msg[1];
	unsigned char data[2];
	
	if( (g_client->client == NULL) || (!g_client->client->adapter) )
	{
		return -ENODEV;
	}
	
	msg->addr 	= g_client->client->addr;
	msg->flags 	= I2C_M_WR;
	msg->len 	= 2;
	msg->buf 	= data;
	data[0] = reg & 0x00ff;
	data[1] = reg >> 8;
	
	

	err = i2c_transfer(g_client->client->adapter, msg, 1);
#if 1//for debug
	if(g_i2c_debugging_enable == 1)
	{
		printk(KERN_DEBUG "[TSP][I2C] read addr[0] = 0x%x, addr[1] = 0x%x\n", data[0], data[1]);
	}
#endif

	if (err >= 0) 
	{
		msg->flags = I2C_M_RD;
		msg->len   = len;
		msg->buf   = read_val;
		err = i2c_transfer(g_client->client->adapter, msg, 1);

#if 1//for debug
		if(g_i2c_debugging_enable == 1)
		{
			printk(KERN_DEBUG "[TSP][I2C] read data = ");
			for(i=0 ; i<len ; i++)
			{
				printk(KERN_DEBUG "%d(0x%x), ", msg->buf[i],  msg->buf[i]);
			}
			printk(KERN_DEBUG "// len = %d, rtn=%d\n",msg->len, err);
		}
#endif
	}

	if (err >= 0) 
	{
		return 0;
	}

	id = i2c_adapter_id(g_client->client->adapter);
//	printk("[TSP] %s() - end\n", __FUNCTION__);
	printk("tsp_read returning %d\n",err);
	return err;
}
#else
int i2c_tsp_sensor_read(u8 reg, u8 *val, unsigned int len )
{
	int id;
	int 	 err;
	struct 	 i2c_msg msg[1];
	unsigned char data[1];

	if( (g_client->client == NULL) || (!g_client->client->adapter) )
	{
		return -ENODEV;
	}
	
	msg->addr 	= g_client->client->addr;
	msg->flags 	= I2C_M_WR;
	msg->len 	= 1;
	msg->buf 	= data;
	*data       = reg;

	err = i2c_transfer(g_client->client->adapter, msg, 1);

	if (err >= 0) 
	{
		msg->flags = I2C_M_RD;
		msg->len   = len;
		msg->buf   = val;
		err = i2c_transfer(g_client->client->adapter, msg, 1);
	}

	if (err >= 0) 
	{
		return 0;
	}

	id = i2c_adapter_id(g_client->client->adapter);

	return err;
}

#endif
#define I2C_MAX_SEND_LENGTH	300
int i2c_tsp_sensor_write(u16 reg
	, u8 *read_val, unsigned int len)
{
	int err;
	struct i2c_msg msg[1];
	unsigned char data[300];
	int i ;

	if( (g_client->client == NULL) || (!g_client->client->adapter) )
		return -ENODEV;

	if(len +2 > I2C_MAX_SEND_LENGTH)
	{
		printk("[TSP][ERROR] %s() data length error\n", __FUNCTION__);
		return -ENODEV;
	}
		
	msg->addr = g_client->client->addr;
	msg->flags = I2C_M_WR;
	msg->len = len + 2;
	msg->buf = data;
	data[0] = reg & 0x00ff;
	data[1] = reg >> 8;
	

	for (i = 0; i < len; i++)
	{
		data[i+2] = *(read_val+i);
	}

	err = i2c_transfer(g_client->client->adapter, msg, 1);

	if (err >= 0) return 0;

	return err;
}
void i2c_tsp_sensor_write_reg(u8 address, int data)
{
	u8 i2cdata[1];

	i2cdata[0] = data;
	i2c_tsp_sensor_write(address, i2cdata, 1);
}

static int i2c_tsp_sensor_probe_client(struct i2c_client *client,const struct i2c_device_id *id)
{
	struct qt602240_data *data;

    	int ret;

    	data = kzalloc(sizeof(struct qt602240_data), GFP_KERNEL);

    	if (!data) {
        	dev_err(&client->dev, "Failed to allocate memory\n");
        	ret = -ENOMEM;
        	goto err_free_mem;
    	}

	client->irq = OMAP_GPIO_TOUCH_INT;

    	data->client = client;
    	data->irq = client->irq;

	i2c_set_clientdata(client, data);

	g_client = data;

	return 0;

err_free_mem:
    kfree(data);
    return ret;
}

static int i2c_tsp_sensor_detach_client(struct i2c_client *client)
{
	
	printk(KERN_DEBUG "[TSP] %s() - start\n", __FUNCTION__);	// ryun 20091126
  	/* Try to detach the client from i2c space */
	//if ((err = i2c_detach_client(client))) {
        //return err;
	//}
	i2c_set_clientdata(client,NULL);

	kfree(client); /* Frees client data too, if allocated at the same time */
	g_client = NULL;
	return 0;
}


int i2c_tsp_sensor_init(void)
{
	int ret;

	if ( (ret = i2c_add_driver(&tsp_sensor_driver)) ) 
	{
		printk("TSP I2C Driver registration failed!\n");
		return ret;
	}

	return 0;
}
uint8_t read_boot_state(u8 *data)
{
	struct i2c_msg rmsg;
	int ret;

	rmsg.addr = QT602240_I2C_BOOT_ADDR ;
	rmsg.flags = I2C_M_RD;
	rmsg.len = 1;
	rmsg.buf = data;
	ret = i2c_transfer(g_client->client->adapter, &rmsg, 1);

	if ( ret < 0 )
	{
		printk("[TSP] %s,%d - Fail!!!! ret = %d\n", __func__, __LINE__, ret );
		return READ_MEM_FAILED; 
	}	
	else 
	{
		return READ_MEM_OK;
	}
}

uint8_t boot_unlock(void)
{
	struct i2c_msg wmsg;
	int ret;
	unsigned char data[2];

	printk(KERN_DEBUG "[TSP] %s - start, %d\n", __func__, __LINE__ );

	data[0] = 0xDC;
	data[1] = 0xAA;

	wmsg.addr = QT602240_I2C_BOOT_ADDR ;
	wmsg.flags = I2C_M_WR;
	wmsg.len = 2;
	wmsg.buf = data;

	ret = i2c_transfer(g_client->client->adapter, &wmsg, 1);

	if ( ret >= 0 )
		return WRITE_MEM_OK;
	else 
		return WRITE_MEM_FAILED; 
}

uint8_t boot_write_mem(uint16_t ByteCount, unsigned char * Data )
{
	struct i2c_msg wmsg;
//	unsigned char data[I2C_MAX_SEND_LENGTH];
	int ret;
//	int i;

	wmsg.addr = QT602240_I2C_BOOT_ADDR ;
	wmsg.flags = I2C_M_WR;
	wmsg.len = ByteCount;
	wmsg.buf = Data;

	ret = i2c_transfer(g_client->client->adapter, &wmsg, 1);
	if ( ret >= 0 )
	{
		return WRITE_MEM_OK;
	}	
	else 
	{
		printk("[TSP] %s,%d - Fail!!!!\n", __func__, __LINE__ );
		return WRITE_MEM_FAILED; 
	}
}

