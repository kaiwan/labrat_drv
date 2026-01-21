/*
 * adxl345_i2c.c
 *
 * An I2C device driver for the well known ADXL345 accelerometer chip.
 * (Currently connected to a TI BeaglePlay via the Grove connector.)
 *
 * ADXL345 Datasheet PDF:
 * https://www.analog.com/media/en/technical-documentation/data-sheets/adxl345.pdf
 *
 * TODO:
 * [ ] Interrupt handling: INT1, INT2
 *
 * (c) Kaiwan N Billimoria, kaiwanTECH
 * Dual MIT/GPL
 */
#define pr_fmt(fmt) "%s:%s(): " fmt, KBUILD_MODNAME, __func__
//#define dev_fmt(fmt) "%s(): " fmt, __func__

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
#include <linux/sysfs.h>
#include <linux/i2c.h>

MODULE_AUTHOR("Kaiwan N Billimoria, kaiwanTECH");
MODULE_DESCRIPTION("Simple I2C driver for the ADXL345 accelerometer");
MODULE_LICENSE("Dual MIT/GPL");

// See Datasheet
/* ADXL345 Register Map */
#define ADXL345_REG_DEVID       0x00
#define ADXL345_REG_POWER_CTL   0x2D
#define ADXL345_REG_DATA_FORMAT 0x31
#define ADXL345_REG_DATAX0      0x32	// Start of data registers

/* ADXL345 constants */
#define ADXL345_DEVID_VAL       0xE5
#define ADXL345_POWER_MEASURE   0x08	// Bit 3 high to start measurement

struct my_adxl345_data {
	struct i2c_client *client;
	s16 x;
	s16 y;
	s16 z;
};

/* Read all 3 axes */
static int adxl345_read_axes(struct my_adxl345_data *data)
{
	struct i2c_client *client = data->client;
	u8 reg_addr = ADXL345_REG_DATAX0;
	u8 buf[6];  // dest buffer
	struct i2c_msg msgs[2];
	int ret;

	/*
	 * I2C read:
	 * 1. Write register address to start reading from
	 * 2. Read 6 bytes from the device
	 */
	msgs[0].addr = client->addr;
	msgs[0].flags = 0;	// write
	msgs[0].len = 1;
	msgs[0].buf = &reg_addr;

	msgs[1].addr = client->addr;
	msgs[1].flags = I2C_M_RD;	// read
	msgs[1].len = sizeof(buf);	// 6 bytes (X0, X1, Y0, Y1, Z0, Z1)
	msgs[1].buf = buf;

	ret = i2c_transfer(client->adapter, msgs, 2);
	if (ret < 0) {
		dev_err(&client->dev, "failed to read data registers\n");
		return ret;
	}

	/* Convert 2 bytes (LSB, MSB) into signed 16-bit integer */
	data->x = (s16) ((buf[1] << 8) | buf[0]);
	data->y = (s16) ((buf[3] << 8) | buf[2]);
	data->z = (s16) ((buf[5] << 8) | buf[4]);

	return 0;
}

/*
 * Sysfs 'show' function
 * Called back when you run 'cat /sys/.../adxl_axes'
 */
static ssize_t adxl_axes_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct my_adxl345_data *data = dev_get_drvdata(dev);

	// Refresh data from sensor
	adxl345_read_axes(data);

	// Format the output string
	return snprintf(buf, 27, "X: %d, Y: %d, Z: %d\n", data->x, data->y, data->z);
}

static DEVICE_ATTR_RO(adxl_axes);

/* The probe method of our driver */
#if LINUX_VERSION_CODE >= KERNEL_VERSION(6, 3, 0)
static int adxl345_probe(struct i2c_client *client)
#else
static int adxl345_probe(struct i2c_client *client, const struct i2c_device_id *id)
#endif
{
	/*
	 * first param: struct <foo>_client *client
	 * the specialized structure for this kernel framework (eg. i2c, spi, usb,
	 * pci, platform, etc); we often/usually extract the 'device' pointer from it..
	 */
	struct device *dev = &client->dev;
	struct my_adxl345_data *data;
	int devid, ret;

	pr_info("in probe!\n");
	/*
	 * Your work in the probe() routine:
	 * 1. Initialize the device
	 * 2. Prepare driver work: allocate a structure for a suitable
	 *    framework, allocate memory, map I/O memory, register interrupts...
	 * 3. When everything is ready, register the new device to the framework
	 */
	data = devm_kzalloc(dev, sizeof(struct my_adxl345_data), GFP_KERNEL);
	if (!data)
		return -ENOMEM;

	data->client = client;
	i2c_set_clientdata(client, data);

	// Verify Device ID
	devid = i2c_smbus_read_byte_data(client, ADXL345_REG_DEVID);
	if (devid != ADXL345_DEVID_VAL)
		dev_warn(&client->dev, "Unknown Device ID: 0x%02x\n", devid);

	/* Enable Measurement Mode
	 * Must write the ADXL345_POWER_MEASURE value to register 0x2D (POWER_CTL)
	 */
	ret = i2c_smbus_write_byte_data(client, ADXL345_REG_POWER_CTL, ADXL345_POWER_MEASURE);
	if (ret < 0) {
		dev_err(&client->dev, "Failed to enable measurement mode\n");
		return ret;
	}

	ret = device_create_file(dev, &dev_attr_adxl_axes);
	if (ret < 0) {
		dev_err(&client->dev, "Failed to create sysfs file\n");
		return ret;
	}

	dev_info(dev, "ADXL345 initialized\n");

	return 0;
}

#if LINUX_VERSION_CODE >= KERNEL_VERSION(6, 1, 0)
static void adxl345_remove(struct i2c_client *client)
#else
static int adxl345_remove(struct i2c_client *client)
#endif
{
	dev_dbg(&client->dev, "removed\n");

	device_remove_file(&client->dev, &dev_attr_adxl_axes);
	// Put sensor back to standby (optional)
	i2c_smbus_write_byte_data(client, ADXL345_REG_POWER_CTL, 0x00);

#if LINUX_VERSION_CODE < KERNEL_VERSION(6, 1, 0)
	return 0;
#endif
}

/*------- Matching the driver to the device ------------------
 * The driver, on init, registers with the appropriate bus driver
 * (for the bus it's on). This has the bus driver run a 'matching loop'..
 * The possible method for the match - 4 different ways - are:
 *  by name                     : for platform & I2C devices
 *  by VID,PID pair             : for PCI and USB devices
 *  by DT 'compatible' property : for devices on the Device Tree
 *                                 (ARM32, Aarch64, PPC, etc)
 *  by ACPI ID : for devices on ACPI tables (x86)
 */

#if 0
/*
 * 1. By name : for platform & I2C devices
 * The <foo>_device_id structure:
 * where <foo> is typically one of:
 *  acpi_button, cnic, cpufreq, gameport, hid, i2c, ide_pci, ipmi, mbus, mmc,
 *  pnp, platform, scsi, sdio, serio, spi, tty, usb, usb_serial, vme
 */
static const struct i2c_device_id adxl345_id[] = {
	{"adxl345", 0},		/* matching by name; required for platform and i2c
				 * devices & drivers */
	// f.e.: { "pcf8563", 0 },
	{}
};

MODULE_DEVICE_TABLE(<foo >, adxl345_id);
#endif

/* 3. By DT 'compatible' property : for devices on the Device Tree
 *                                 (ARM32, Aarch64, PPC, etc)
 */
#ifdef CONFIG_OF
static const struct of_device_id adxl345_of_match[] = {
	/*
	 * ARM/PPC/etc: matching by DT 'compatible' property
	 * 'compatible' property: one or more strings that define the specific
	 * programming model for the device. This list of strings should be used
	 * by a client program for device driver selection. The property value
	 * consists of a concatenated list of null terminated strings,
	 * from most specific to most general.
	 */
	//{ .compatible = "<manufacturer>,<model-abc-xyz>", "<oem>,<model-abc>", /*<...>*/ },
	{.compatible = "custom,adxl345"},
	{}
};

MODULE_DEVICE_TABLE(of, adxl345_of_match);
#endif

/*
 * The <foo>_driver structure:
 * where <foo> is typically one of:
 *  acpi_button, cnic, cpufreq, gameport, hid, i2c, ide_pci, ipmi, mbus, mmc,
 *  pci, platform, pnp, scsi, sdio, serio, spi, tty, usb, usb_serial, vme
 */
static struct i2c_driver adxl345_driver = {
	.driver = {
		   .name = "custom,adxl345",
		   /* platform and I2C use the
		    * 'name' field for the match and thus the bind between the
		    * DT desc/device and driver
		    */
		   .of_match_table = of_match_ptr(adxl345_of_match),
		   },
	.probe = adxl345_probe,	// invoked on driver/device bind
	.remove = adxl345_remove,	// optional; invoked on driver/device detach
//      .id_table   = adxl345_id,
};

/*
 * Init and Cleanup ::
 * Instead of manually specifiying the init and cleanup handlers in the
 * 'usual' manner, a lot of boilerplate is avoided (when nothing special is
 * required in the init and cleanup code paths) by using the
 *
 * module_foo_driver() macro;
 *
 * where <foo> is typically one of:
 *  acpi_button, cnic, cpufreq, gameport, hid, i2c, ide_pci, ipmi, mbus, mmc,
 *  pci, platform, pnp, scsi, sdio, serio, spi, tty, usb, usb_serial, vme
 * There are several foo_register_driver() APIs; see a list (for 5.4.0) here:
 *  https://gist.github.com/kaiwan/04cfaca711aed9e59282601fafd8aa24
 *
 * This internally invokes the registration of this cient driver with the
 * 'foo' bus driver!
 */
module_i2c_driver(adxl345_driver);
