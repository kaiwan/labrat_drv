/*
 * dht2x_kdrv.c
 *
 * Kernel driver for the DHT2x (dht20/21/22) temperature & humidity sensor.
 * Datasheet: https://aqicn.org/air/sensor/spec/asair-dht20.pdf
 *
 * Demo-
 * After loading:
 * $ cat /sys/bus/i2c/devices/1-0038/dht2x_temp
 * 25841$
 * $ cat /sys/bus/i2c/devices/1-0038/dht2x_humd
 * 59925$
 *
 * (c) 2022 Kaiwan N Billimoria, kaiwanTECH
 * License: Dual MIT/GPL
 */
#define pr_fmt(fmt) "%s:%s(): " fmt, KBUILD_MODNAME, __func__

#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/sched.h>
#include <linux/slab.h>
#include <linux/version.h>
#if LINUX_VERSION_CODE >= KERNEL_VERSION(4,11,0)
#include <linux/uaccess.h>
#include <linux/sched/signal.h>
#else
#include <asm/uaccess.h>
#endif
#include <linux/device.h>
#include <linux/i2c.h>
#include <linux/delay.h>

MODULE_AUTHOR("Kaiwan N Billy");
MODULE_DESCRIPTION("Kernel driver for the DHT2x (dht20/21/22) temperature & humidity sensor");
MODULE_LICENSE("Dual MIT/GPL");

// Registers
#define DHT2X_CMD_REG		0x71
#define DHT2X_CMD_TRIGGER_MSMT	0xAC


#define IS_BIT_SET(var,pos) ((var) & (1<<(pos)))

#define GET_SHOW_STATUS(client,stat) do { \
	stat = i2c_smbus_read_byte_data(client, DHT2X_CMD_REG); \
	dev_dbg(&client->dev, "chip status (0x%x): calibration[b3]: 0x%x   busy[b7]: 0x%x\n", \
		stat, IS_BIT_SET((stat), 3), IS_BIT_SET((stat), 7)); \
} while (0)

struct dht2x_data {
	struct i2c_client *client;
	struct mutex lock;
	unsigned char buf[8];
} *gdata;

static int dht2x_read_block_data(struct i2c_client *client, unsigned char reg,
				  unsigned char length, unsigned char *buf)
{
	struct i2c_msg msgs[] = {
		{		/* setup read ptr */
		 .addr = client->addr,
		 .len = 1,
		 .buf = &reg,
		 },
		{
		 .addr = client->addr,
		 .flags = I2C_M_RD,
		 .len = length,
		 .buf = buf
		 },
	};

	if ((i2c_transfer(client->adapter, msgs, 2)) != 2) {
		dev_err(&client->dev, "%s(): i2c read error\n", __func__);
		return -EIO;
	}

	return 0;
}

// https://stackoverflow.com/a/101613/779269
int ipow(int base, int exp)
{
    int result = 1;
    for (;;)
    {
        if (exp & 1)
            result *= base;
        exp >>= 1;
        if (!exp)
            break;
        base *= base;
    }

    return result;
}

static long dht2x_read_sensors(struct device *dev)
{
	struct i2c_client *client = to_i2c_client(dev);
	int stat, err, len = 7;
	// Read sensor requires cmd 0xAC followed by bytes 0x33, 0x00
#ifdef __BIG_ENDIAN
	u16 cmdbytes = (0x33 << 8) | 0x0;
#else
	u16 cmdbytes = 0x0033;
#endif
	long crc_obtained, crc;

	/* Check chip is calibrated
	 * Datasheet:
	   1.After power-on, wait no less than 100ms. Before reading the
	   temperature and humidity value, get a byte of status
	   word by sending 0x71. If the status word and 0x18 are not equal to 0x18
	   , initialize the 0x1B, 0x1C, 0x1E registers ...
	 */
	dev_dbg(dev, "Reading status reg and checking it's calibrated\n");
	stat = i2c_smbus_read_byte_data(client, DHT2X_CMD_REG);
	dev_dbg(dev, "stat=0x%x\n", stat); // get 0x1c; calib ok
	//if (stat != 0x18) {
	if (IS_BIT_SET(stat, 3) == 0) {
		dev_info(dev,
		"Initialization error, chip uncalibrated (stat=0x%x)...\n", stat);
		return -1;
	}

	/*
	 * Datasheet:
	 * 2.Wait 10ms to send the 0xAC command (trigger measurement). This
	 * command parameter has two bytes, the first byte is 0x33, and the
	 * second byte is 0x00.
	 */
	// s32 i2c_smbus_write_word_data(const struct i2c_client *client, u8 command,
	//           u16 value)
	dev_dbg(dev, "Writing cmd 0xAC followed by 0x%x (0x33,0x0)\n", cmdbytes);
	mdelay(20);
	err = i2c_smbus_write_word_data(client, DHT2X_CMD_TRIGGER_MSMT, cmdbytes);
	if (err != 0)
		dev_warn(dev, "register write failed\n");

	/* i2c read yields the temperature value
	 * Datasheet:
	 * 3.Wait 80ms for the measurement to be completed, if the read status
	 * word Bit [7] is 0, it means the measurement is completed, and then
	 * six bytes can be read continuously; otherwise, continue to wait.
	 */
	dev_dbg(dev, "Reading 7 bytes from chip now...\n");
	mdelay(100);
	do {
		GET_SHOW_STATUS(client, stat);
		dev_dbg(dev, "waiting...(stat=0x%x)\n", stat);
		mdelay(100);
	} while ((IS_BIT_SET(stat, 7) != 0));

	err = dht2x_read_block_data(client, DHT2X_CMD_REG, len, gdata->buf);
	if (err) {
		dev_warn(dev, "read 2 failed\n");
		return err;
	}
	/*
	 * Datasheet:
	 * The sensor needs time to collect. After the host sends a measurement
	 * command (0xAC), delay more than 80 milliseconds before reading the
	 * converted data and judging whether the returned status bit is normal. 
	 */
	msleep(100);
	print_hex_dump_bytes("gdata->buf:", DUMP_PREFIX_OFFSET, gdata->buf, len);

	/* TODO- check all ok via CRC #
	 * Datasheet:
	 * 4.After receiving six bytes, the next byte is the CRC check data. The user can read it out as needed. If the receiving
end needs CRC check, an ACK will be sent after the sixth byte is received. Reply, otherwise send NACK to end, the
initial value of CRC is 0XFF, and the CRC8 check polynomial is:
CRC [7:0] = 1+X^4+X^5+X^8
	 */
	if (!kstrtol(gdata->buf[6], 16, &crc_obtained)) { // ret 0 on success
		crc = 1 + int_pow(crc_obtained, 4) + int_pow(crc_obtained, 5) + int_pow(crc_obtained, 8);
		dev_dbg(dev, "crc obtd=0x%lx crc=0x%lx\n", crc_obtained, crc);
	}

	return 0;
}

static ssize_t dht2x_humd_show(struct device *dev,
			    struct device_attribute *attr, char *buf)
{
	int n;
	s32 hraw, humidity;

	if (mutex_lock_interruptible(&gdata->lock))
		return -EINTR;
	dev_dbg(dev, "In the humidity 'show' method\n");
	dht2x_read_sensors(dev);

	// Calculate humdity
	hraw = ((gdata->buf[3] & 0xf0) >> 4) + (gdata->buf[1] << 12) + (gdata->buf[2] << 4);
	humidity = (100 * hraw)/1048;
	dev_dbg(dev, "Rel-humidity=%d milli%%\n", humidity);

	n = snprintf(buf, 10, "%d", humidity);
	mutex_unlock(&gdata->lock);
	return n;
}
static DEVICE_ATTR_RO(dht2x_humd);	/* it's show callback is above.. */

static ssize_t dht2x_temp_show(struct device *dev,
			    struct device_attribute *attr, char *buf)
{
	int n;
	s32 traw, temperature;

	if (mutex_lock_interruptible(&gdata->lock))
		return -EINTR;
	dev_dbg(dev, "In the temperature 'show' method\n");
	dht2x_read_sensors(dev);

	// Calculate temperature (in millidegrees Celsius)
	traw = ((gdata->buf[3] & 0xf) << 16) + (gdata->buf[4] << 8) + gdata->buf[5];
	temperature = ((200 * traw) / 1048) - 50000;
	dev_dbg(dev, "Temperature=%d milliC\n", temperature);

	n = snprintf(buf, 10, "%d", temperature);
	mutex_unlock(&gdata->lock);
	return n;
}
static DEVICE_ATTR_RO(dht2x_temp);	/* it's show callback is above.. */

/* The probe method of our driver */
static int dht2x_probe(struct i2c_client *client, // named as 'client' or 'dev'
                const struct i2c_device_id *id)
{
	int stat;
	/*
	 * first param: struct dht2x_client *client
	 * the specialized structure for this kernel framework (eg. i2c, spi, usb,
	 * pci, platform, etc); we often/usually extract the 'device' pointer from it..
	 */
	struct device *dev = &client->dev;

	dev_info(dev, "hey, in probe! name=%s addr=0x%x\n",
		client->name, client->addr);

	/*
	 * Your work in the probe() routine:
	 * 1. Initialize the device
	 * 2. Prepare driver work: allocate a structure for a suitable
	 *    framework, allocate memory, map I/O memory, register interrupts...
	 * 3. When everything is ready, register the new device to the framework
	 */
	if (!i2c_check_functionality(client->adapter, \
		I2C_FUNC_I2C \
		| I2C_FUNC_SMBUS_READ_BYTE_DATA | I2C_FUNC_SMBUS_WRITE_BYTE_DATA | \
		I2C_FUNC_SMBUS_READ_WORD_DATA | I2C_FUNC_SMBUS_WRITE_WORD_DATA)) {
		dev_warn(dev, "i2c functionality issue?\n");
		return -ENODEV;
	}

	gdata = devm_kzalloc(dev, sizeof(struct dht2x_data), GFP_KERNEL);
	if (!gdata)
		return -ENOMEM;

	gdata->client = client;
	mutex_init(&gdata->lock);

	mdelay(100);
	GET_SHOW_STATUS(client, stat);
	if (stat)
		dev_info(dev, "chip found\n");

	// Setup sysfs files to read tempr and humidity
	stat = device_create_file(&client->dev, &dev_attr_dht2x_temp);
	if (stat) {
		dev_info(dev, "device_create_file 1 failed (%d), aborting now\n", stat);
		return -stat;
	}
	stat = device_create_file(&client->dev, &dev_attr_dht2x_humd);
	if (stat) {
		dev_info(dev, "device_create_file 2 failed (%d), aborting now\n", stat);
		return -stat;
	}

	// TODO - register with hwmon f/w

	return 0;
}

static int dht2x_remove(struct i2c_client *client)
{
	device_remove_file(&client->dev, &dev_attr_dht2x_humd);
	device_remove_file(&client->dev, &dev_attr_dht2x_temp);
	dev_dbg(&client->dev, "removed\n");

	return 0;
}

/*------- Matching the driver to the device ------------------
 * 3 different ways: 
 *  by name : for platform & I2C devices
 *  by DT 'compatible' property : for devices on the Device Tree
 *                                 (ARM32, Aarch64, PPC, etc)
 *  by ACPI ID : for devices on ACPI tables (x86)
 */

/* 
 * 1. By name : for platform & I2C devices
 * The <foo>_device_id structure:
 * where <foo> is typically one of:
 *  acpi_button, cnic, cpufreq, gameport, hid, i2c, ide_pci, ipmi, mbus, mmc,
 *  pnp, platform, scsi, sdio, serio, spi, tty, usb, usb_serial, vme
 */
static const struct i2c_device_id dht2x_id[] = { 
    {"knb,dht2x"},		/* matching by name; required for platform and i2c
					devices & drivers */
    // f.e.: { "pcf8563", 0 },
    { } 
};
MODULE_DEVICE_TABLE(i2c, dht2x_id);

/* 2. By DT 'compatible' property : for devices on the Device Tree
 *                                 (ARM32, Aarch64, PPC, etc)
 */
#ifdef CONFIG_OF
static const struct of_device_id dht2x_of_match[] = {
	/* 
	 * ARM/PPC/etc: matching by DT 'compatible' property
	 * 'compatible' property: one or more strings that define the specific
	 * programming model for the device. This list of strings should be used
	 * by a client program for device driver selection. The property value
	 * consists of a concatenated list of null terminated strings,
	 * from most specific to most general. 
	 */
    { .compatible = "knb,dht2x"},
 // f.e.:   { .compatible = "nxp,pcf8563" },
    {}
};
MODULE_DEVICE_TABLE(of, dht2x_of_match);
#endif

#if 0
/* 3. By ACPI ID : for devices on ACPI tables (x86) */
static const struct acpi_device_id dht2x_acpi_id[] = {
						/* x86: matching by ACPI ID */
	{"MMA7660", 0},
	{}
};
MODULE_DEVICE_TABLE(acpi, dht2x_acpi_id);
#endif


/*
 * The dht2x_driver structure:
 * where dht2x is typically one of:
 *  acpi_button, cnic, cpufreq, gameport, hid, i2c, ide_pci, ipmi, mbus, mmc,
 *  pci, platform, pnp, scsi, sdio, serio, spi, tty, usb, usb_serial, vme
 */
static struct i2c_driver dht2x_driver = {
	.driver     = {
		.name   = "dht2x",  /* platform and I2C use the
					'name' field for the match and thus the bind between the
					DT desc/device and driver */
		.of_match_table = of_match_ptr(dht2x_of_match),
	},
	.probe      = dht2x_probe,		// invoked on driver/device bind
	.remove     = dht2x_remove,	// optional; invoked on driver/device detach
//	.disconnect = dht2x_disconnect,// optional; invoked on device disconnect

	.id_table   = dht2x_id,

//	.suspend    = dht2x_suspend,	// optional
//	.resume     = dht2x_resume,	// optional
};


/* 
 * Init and Cleanup ::
 * Instead of manually specifiying the init and cleanup handlers in the
 * 'usual' manner, a lot of boilerplate is avoided (when nothing special is
 * required in the init and cleanup code paths) by using the
 *
 * module_foo_driver() macro;
 *
 * where dht2x is typically one of:
 *  acpi_button, cnic, cpufreq, gameport, hid, i2c, ide_pci, ipmi, mbus, mmc,
 *  pci, platform, pnp, scsi, sdio, serio, spi, tty, usb, usb_serial, vme
 * There are several foo_register_driver() APIs; see a list (for 5.4.0) here:
 *  https://gist.github.com/kaiwan/04cfaca711aed9e59282601fafd8aa24
 */
module_i2c_driver(dht2x_driver);
