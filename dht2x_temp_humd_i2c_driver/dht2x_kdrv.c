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
#define dev_fmt(fmt) "%s(): " fmt, __func__

#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/sched.h>
#include <linux/slab.h>
#include <linux/version.h>
#if LINUX_VERSION_CODE >= KERNEL_VERSION(4, 11, 0)
#include <linux/uaccess.h>
#include <linux/sched/signal.h>
#else
#include <asm/uaccess.h>
#endif
#include <linux/device.h>
#include <linux/i2c.h>
#include <linux/delay.h>
#include <linux/crc8.h>

MODULE_AUTHOR("Kaiwan N Billimoria");
MODULE_DESCRIPTION("Kernel driver for the DHT2x (dht20/21/22) temperature & humidity sensor");
MODULE_LICENSE("Dual MIT/GPL");

// Registers
#define DHT2X_CMD_REG		0x71
#define DHT2X_CMD_TRIGGER_MSMT	0xAC

#define IS_BIT_SET(var, pos) ((var) & (1<<(pos)))

#define GET_SHOW_STATUS(client, stat) do { \
	stat = i2c_smbus_read_byte_data(client, DHT2X_CMD_REG); \
	dev_dbg(&client->dev, "chip status (0x%x): calibration[b3]: 0x%x   busy[b7]: 0x%x\n", \
		stat, IS_BIT_SET((stat), 3), IS_BIT_SET((stat), 7)); \
} while (0)

struct dht2x_data {
	struct i2c_client *client;
	struct mutex lock;
	unsigned char buf[8];
	int crc_wrong;
};
static struct dht2x_data *gdata;

static int dht2x_read_block_data(struct i2c_client *client, unsigned char reg,
				 unsigned char length, unsigned char *buf)
{
	struct i2c_msg msgs[] = {
			// setup a (1) 'write cmd' from master, followed by, (2) read from slave
		{	/* setup write from master to slave of 1 byte, the cmd */
		 .addr = client->addr,
		 .len = 1,
		 .buf = &reg,
		 },
			/* setup read from slave to master of 'length'
			 * bytes (here it's 7), the data received */
		{
		 .addr = client->addr,
		 .flags = I2C_M_RD,
		 .len = length,
		 .buf = buf},
	};

	if ((i2c_transfer(client->adapter, msgs, 2)) != 2) {
		dev_err(&client->dev, "%s(): i2c read error\n", __func__);
		return -EIO;
	}

	return 0;
}

/* Src: https://stackoverflow.com/a/51773839/779269
 */
static u8 gencrc8(u8 *data, size_t len)
{
	u8 crc = 0xff;
	size_t i, j;

	for (i = 0; i < len; i++) {
		crc ^= data[i];
		for (j = 0; j < 8; j++) {
			if ((crc & 0x80) != 0)
				crc = (uint8_t) ((crc << 1) ^ 0x31);
			else
				crc <<= 1;
		}
	}
	return crc;
}

#if 0
#define CRC8_INIT_VALUE     0xFF
DECLARE_CRC8_TABLE(dht2x_crc8_table);
#endif

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
	u8 crc_obtained;
	char str_crc[3];

	/* Check chip is calibrated
	 * Datasheet (pg 10, 11):
	 1.After power-on, wait no less than 100ms. Before reading the
	 temperature and humidity value, get a byte of status
	 word by sending 0x71. If the status word and 0x18 are not equal to 0x18
	 , initialize the 0x1B, 0x1C, 0x1E registers ...
	 */
	dev_dbg(dev, "Reading status reg and checking it's calibrated\n");
	stat = i2c_smbus_read_byte_data(client, DHT2X_CMD_REG);
	dev_dbg(dev, "stat=0x%x\n", stat);	// get 0x18 or 0x1c; calib ok
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

	/* Check all ok via CRC #
	 * Datasheet:
	 * 4.After receiving six bytes, the next byte is the CRC check data.
	 * The user can read it out as needed. If the receiving end needs CRC
	 * check, an ACK will be sent after the sixth byte is received. Reply,
	 * otherwise send NACK to end, the initial value of CRC is 0XFF, and
	 * the CRC8 check polynomial is: CRC [7:0] = 1+X^4+X^5+X^8
	 */
	gdata->crc_wrong = 0;
	str_crc[2] = '\0';
	snprintf(str_crc, 3, "%x", gdata->buf[6]);
	//pr_debug("str_crc=%s\n", str_crc);
	if (!kstrtou8(str_crc, 16, &crc_obtained)) {	// ret 0 on success
#if 0
		/* The kernel's crc8() API on RPi requires loading module crc8!
		 *  u8 crc8(const u8 table[CRC8_TABLE_SIZE], u8 *pdata, size_t nbytes, u8 crc);
		 * -still doesn't seem to work?
		 */
		u8 crc = crc8(dht2x_crc8_table, gdata->buf, 6, CRC8_INIT_VALUE);
#else
		u8 crc = gencrc8(gdata->buf, 6);
#endif
		dev_dbg(dev, "crc obtd=0x%x crc=0x%x\n", crc_obtained, crc);
		if (crc != crc_obtained) {
			dev_info(dev, "CRC checksum error!\n");
			gdata->crc_wrong = 1;
		}
	}

	return 0;
}

static ssize_t dht2x_humd_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	int n;
	s32 hraw, humidity;

	if (mutex_lock_interruptible(&gdata->lock))
		return -EINTR;
	dev_dbg(dev, "In the humidity 'show' method\n");
	dht2x_read_sensors(dev);
	if (gdata->crc_wrong) {
		n = strscpy(buf, "CRC-err", 8);
		mutex_unlock(&gdata->lock);
		return n;
	}
	// Calculate humdity; see datasheet
	hraw = ((gdata->buf[3] & 0xf0) >> 4) + (gdata->buf[1] << 12) + (gdata->buf[2] << 4);
	humidity = (100 * hraw) / 1048;
	dev_dbg(dev, "Rel-humidity=%d milli%%\n", humidity);

	/* Max data that can be passed back to userspace via sysfs is 1 page;
	 * Here we're passing back the RH (relative humidity); the range will
	 * be [0-100.000]%. Hence, max bytes to pass back will be for the value
	 * 100,000, i.e., 6 bytes. Add 1 for the null byte, so, 7 bytes max.
	 */
	n = snprintf(buf, 7, "%d", humidity);
	mutex_unlock(&gdata->lock);
	return n;
}

/*
 * The macro DEVICE_ATTR_XX(foo) generates the structure dev_attr_foo !
 * So, here, it generates the struct dev_attr_dht2x_humd
 */
static DEVICE_ATTR_RO(dht2x_humd);	/* it's show callback is above.. */

static ssize_t dht2x_temp_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	int n;
	s32 traw, temperature;

	if (mutex_lock_interruptible(&gdata->lock))
		return -EINTR;
	dev_dbg(dev, "In the temperature 'show' method\n");
	dht2x_read_sensors(dev);
	if (gdata->crc_wrong) {
		n = strscpy(buf, "CRC-err", 8);
		mutex_unlock(&gdata->lock);
		return n;
	}
	// Calculate temperature (in millidegrees Celsius); see datasheet
	traw = ((gdata->buf[3] & 0xf) << 16) + (gdata->buf[4] << 8) + gdata->buf[5];
	temperature = ((200 * traw) / 1048) - 50000;
	dev_dbg(dev, "Temperature=%d milliC\n", temperature);

	/* Max data that can be passed back to userspace via sysfs is 1 page;
	 * Here we're passing back the temperature in millidegrees Celsius;
	 * the range as per the datasheet is [-40 to +80]C. Hence, max bytes
	 * to pass back will be for the value 80,000, i.e., 5 bytes. Add 1 for
	 * the null byte, so, 6 bytes max.
	 */
	n = snprintf(buf, 6, "%d", temperature);
	mutex_unlock(&gdata->lock);
	return n;
}

/*
 * The macro DEVICE_ATTR_XX(foo) generates the structure dev_attr_foo !
 * So, here, it generates the struct dev_attr_dht2x_temp
 */
static DEVICE_ATTR_RO(dht2x_temp);	/* it's show callback is above.. */

/* The probe method of our driver */
static int dht2x_probe(struct i2c_client *client,	// named as 'client' or 'dev'
		       const struct i2c_device_id *id)
{
	int stat;
	/*
	 * first param: struct i2c_client *client
	 * - the specialized structure for this kernel framework (eg. i2c, spi, usb,
	 * pci, platform, etc); we often/usually extract the 'device' pointer from it..
	 */
	struct device *dev = &client->dev;

	dev_info(dev, "hey, in probe! name=%s addr=0x%x\n", client->name, client->addr);

	/*
	 * Your (typical) work in the probe() routine:
	 * 1. Initialize the device
	 * 2. Prepare driver work: allocate a structure for a suitable
	 *    framework, allocate memory, map I/O memory, register interrupts...
	 * 3. When everything is ready, register the new device to the framework
	 */
	if (!i2c_check_functionality(client->adapter,
				     I2C_FUNC_I2C
				     | I2C_FUNC_SMBUS_READ_BYTE_DATA |
				     I2C_FUNC_SMBUS_WRITE_BYTE_DATA |
				     I2C_FUNC_SMBUS_READ_WORD_DATA |
				     I2C_FUNC_SMBUS_WRITE_WORD_DATA)) {
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
	// TODO - register with a kernel framework; f.e. hwmon framework

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
	{"knb,dht2x_kdrv"},		/* matching by name; required for platform and i2c
				 * devices & drivers */
	// f.e.: { "pcf8563", 0 },
	{}
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
	{.compatible = "knb,dht2x_kdrv"},
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
 * The 'foo'_driver structure:
 * where 'foo' is typically one of:
 *  acpi_button, cnic, cpufreq, gameport, hid, i2c, ide_pci, ipmi, mbus, mmc,
 *  pci, platform, pnp, scsi, sdio, serio, spi, tty, usb, usb_serial, vme
 */
static struct i2c_driver dht2x_driver = {
	.driver = {
		.name = "dht2x_kdrv",/* platform and I2C use the
				 * 'name' field for the match and thus the bind between the
				 * DT desc/device and driver */
		.of_match_table = of_match_ptr(dht2x_of_match),
	},
	.probe = dht2x_probe,	// invoked on driver/device bind
	.remove = dht2x_remove,	// optional; invoked on driver/device detach
//      .disconnect = dht2x_disconnect,// optional; invoked on device disconnect
	.id_table = dht2x_id,
//      .suspend    = dht2x_suspend,    // optional
//      .resume     = dht2x_resume,     // optional
};

/*
 * Init and Cleanup ::
 * Instead of manually specifiying the init and cleanup handlers in the
 * 'usual' manner, a lot of boilerplate is avoided (when nothing special is
 * required in the init and cleanup code paths) by using the
 *
 * module_foo_driver() macro;
 *
 * where 'foo' is typically one of:
 *  acpi_button, cnic, cpufreq, gameport, hid, i2c, ide_pci, ipmi, mbus, mmc,
 *  pci, platform, pnp, scsi, sdio, serio, spi, tty, usb, usb_serial, vme
 * There are several foo_register_driver() APIs; see a list (for 5.4.0) here:
 *  https://gist.github.com/kaiwan/04cfaca711aed9e59282601fafd8aa24
 */
module_i2c_driver(dht2x_driver);
