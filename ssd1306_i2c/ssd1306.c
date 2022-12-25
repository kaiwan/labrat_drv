/*
 * ssd1306.c
 *
 * SSD1306 OLED display simple I2C driver
 * For the Raspberry Pi family host devices...
 *
 */
#define pr_fmt(fmt) "%s:%s(): " fmt, KBUILD_MODNAME, __func__
#define dev_fmt(fmt) "%s(): " fmt, __func__

#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/delay.h>
#include <linux/version.h>
#if LINUX_VERSION_CODE >= KERNEL_VERSION(4, 11, 0)
#include <linux/uaccess.h>
#include <linux/sched/signal.h>
#else
#include <asm/uaccess.h>
#endif
#include <linux/device.h>
#include <linux/i2c.h>

MODULE_AUTHOR("EmbeTronicX,Subhrajyoti S,Kaiwan NB");
MODULE_DESCRIPTION("SSD1306 OLED display simple I2C driver");
MODULE_LICENSE("GPL");

#define I2C_BUS_SSD1306       1	// I2C Bus available in our Raspberry Pi
#define SLAVE_DEVICE_NAME   "oled_ssd1306"	// Device and Driver Name
#define SSD1306_SLAVE_ADDR   0x3C	// SSD1306 OLED Slave Address

#define CMD	(true)
#define DATA	(false)

struct i2c_adapter *oled_i2c_adapter;	// I2C Adapter Structure
static struct i2c_client *i2c_client_oled;	// I2C Cient Structure (In our case it is OLED)
/* Use the XWin bitmap app to render the characters.
 * Keep last col zero to, in effect, have some spacing before the next char
 */
static u8 render[36][7] = {
	{0x00, 0x7f, 0x41, 0x41, 0x41, 0x7f, 0x00},	// 0
	{0x00, 0x44, 0x42, 0x7f, 0x40, 0x40, 0x00},	// 1
	{0x00, 0x79, 0x49, 0x49, 0x49, 0x4f, 0x00},	// 2
	{0x00, 0x49, 0x49, 0x49, 0x49, 0x7f, 0x00},	// 3
	{0x00, 0x0f, 0x08, 0x08, 0x08, 0x7f, 0x00},	// 4
	{0x00, 0x4f, 0x49, 0x49, 0x49, 0x79, 0x00},	// 5
	{0x00, 0x7f, 0x49, 0x49, 0x49, 0x79, 0x00},	// 6
	{0x00, 0x01, 0x01, 0x01, 0x01, 0x7f, 0x00},	// 7
	{0x00, 0x7f, 0x49, 0x49, 0x49, 0x7f, 0x00},	// 8
	{0x00, 0x4f, 0x49, 0x49, 0x49, 0x7f, 0x00},	// 9
	{0x00, 0x00, 0xf1, 0x91, 0x91, 0xfe, 0x00},	// a
	{0xff, 0x88, 0x88, 0x88, 0x88, 0x70, 0x00},	// b :correct!
	{0x00, 0xf8, 0x88, 0x88, 0x88, 0x88, 0x00},	// c
};

#define RENDER(n) do { \
	int i; \
	dev_dbg(dev,"n=%d", n); \
	for (i = 0; i < 7; i++) \
		SSD1306_Write(DATA, render[n][i]); \
} while (0)

/*
 * How it works - TOO (Theory Of Operation):
 * Take the digit '0'; the render[0][0] represents it's bit pattern:
	{0x00, 0x7f, 0x41, 0x41, 0x41, 0x7f, 0x00},	// 0
 * Each byte data is written vertically into a 'page'; there are 8 pages,
 * page 0 to page 7. So 0x00 will go into page 0, 0x7f into page 1, 0x41
 * to page 2, and so on...
 * There are 128 columns, COL0 to COL127; each holds a bit. So 8 bits being
 * a byte, we can have upto 128/8 = 16 characters per row. So the effective
 * display resolution becomes 16x8 chars.
 
   When writing out a byte, say 0x8f, the LSB nibble, i.e., 0xf = 1111, is
   written *first*, then the MSB nibble 0x8 = 1000. BUT it seems to render from
   bottom-to-top; so 0x8f it looks like this:
   (Legend: _ = 0 (empty, no pixel) , | = 1 (a single pixel))
  1 |
  1 |
  1 |
  1 |
  0 _
  0 _
  0 _
  1 |

  Lets draw out what digit '0' becomes:
   (Legend: _ = 0 , X = 1)
       C0 C1 C2 C3 C4 C5 C6 C7 C8 ... ...                        C127
     { 00 7f 41 41 41 7f 00},	// 0
   Pg0  _  _  _  _  _  _  _
   Pg1  _  X  X  X  X  X  _
   Pg2  _  X  _  _  _  X  _
   Pg3  _  X  _  _  _  X  _
   Pg4  _  X  _  _  _  X  _
   Pg5  _  X  _  _  _  X  _
   Pg6  _  X  _  _  _  X  _
   Pg7  _  X  X  X  X  X  _

  Lets draw out what 'b' becomes:
  (Legend: _ = 0 , X = 1)
       C0 C1 C2 C3 C4 C5 C6 C7 C8 ... ...                        C127
     { ff 88 88 88 88 70 00},	// b :correct!
   Pg0  X  _  _  _  _  _  _
   Pg1  X  _  _  _  _  _  _
   Pg2  X  _  _  _  _  _  _
   Pg3  X  X  X  X  X  _  _
   Pg4  X  _  _  _  _  X  _
   Pg5  X  _  _  _  _  X  _
   Pg6  X  _  _  _  _  X  _
   Pg7  X  X  X  X  X  _  _

 * Similary for the rest...
 */

/*
 * This function writes the data into the I2C client
 *  Arguments:
 *      buff -> buffer to be sent
 *      len  -> Length of the data
 */
static int I2C_Write(unsigned char *buf, unsigned int len)
{
	/*
	 * Sending Start condition, Slave address with R/W bit,
	 * ACK/NACK and Stop condtions will be handled internally.
	 */
	int ret = i2c_master_send(i2c_client_oled, buf, len);
	return ret;
}

/*
 * This function reads one byte of the data from the I2C client
 *  Arguments:
 *      out_buff -> buffer wherer the data to be copied
 *      len      -> Length of the data to be read
 */
static int I2C_Read(unsigned char *out_buf, unsigned int len)
{
	/*
	 * Sending Start condition, Slave address with R/W bit,
	 * ACK/NACK and Stop condtions will be handled internally.
	 */
	int ret = i2c_master_recv(i2c_client_oled, out_buf, len);
	return ret;
}

/*
 * This function is specific to the SSD_1306 OLED.
 * This function sends the command/data to the OLED.
 *  Arguments:
 *      is_cmd -> CMD (Bool true) => command,
 *                DATA (Bool false) => data
 *      data   -> data to be written
 */
static void SSD1306_Write(bool is_cmd, unsigned char data)
{
	unsigned char buf[2] = { 0 };
	int ret;

	/*
	 * First byte is always control byte. Data is followed after that.
	 *
	 * There are two types of data in SSD_1306 OLED.
	 * 1. Command
	 * 2. Data
	 *
	 * Control byte decides that the next byte is, command or data.
	 * -------------------------------------------------------
	 * |              Control byte's | 6th bit  |   7th bit  |
	 * |-----------------------------|----------|------------|
	 * |   Command                   |   0      |     0      |
	 * |-----------------------------|----------|------------|
	 * |   data                      |   1      |     0      |
	 * |-----------------------------|----------|------------|
	 *
	 * Please refer the datasheet for more information.
	 */
	if (is_cmd == CMD)
		buf[0] = 0x00;
	else
		buf[0] = 0x40;
	buf[1] = data;
	ret = I2C_Write(buf, 2);
}

/*
 * This function sends the commands that need to used to Initialize the OLED.
 */
static int SSD1306_DisplayInit(void)
{
	msleep(100);		// delay

	/*
	 * Commands to initialize the SSD_1306 OLED Display
	 */
	SSD1306_Write(CMD, 0xAE);	// Entire Display OFF
	SSD1306_Write(CMD, 0xD5);	// Set Display Clock Divide Ratio and Oscillator Frequency
	SSD1306_Write(CMD, 0x80);	// Default Setting for Display Clock Divide Ratio and Oscillator Frequency that is recommended

	SSD1306_Write(CMD, 0xA8);	// Set Multiplex Ratio
	SSD1306_Write(CMD, 0x3F);	// 64 COM lines

	SSD1306_Write(CMD, 0xD3);	// Set display offset
	SSD1306_Write(CMD, 0x00);	// 0 offset

	SSD1306_Write(CMD, 0x40);	// Set first line as the start line of the display
	SSD1306_Write(CMD, 0x8D);	// Charge pump
	SSD1306_Write(CMD, 0x14);	// Enable charge dump during display on

	SSD1306_Write(CMD, 0x20);	// Set memory addressing mode
	SSD1306_Write(CMD, 0x00);	// Horizontal addressing mode
	/* From datasheet:
	 * ... In horizontal addressing mode, after the display RAM is read/written,
	 * the column address pointer is increased automatically by 1. If the column
	 * address pointer reaches column end address, the column address pointer is
	 * reset to column start address and page address pointer is increased by 1
	 * ...
	 */

	SSD1306_Write(CMD, 0xA1);	// Set segment remap with column address 127 mapped to segment 0
	SSD1306_Write(CMD, 0xC8);	// Set com output scan direction, scan from com63 to com 0
	SSD1306_Write(CMD, 0xDA);	// Set com pins hardware configuration
	SSD1306_Write(CMD, 0x12);	// Alternative com pin configuration, disable com left/right remap

	SSD1306_Write(CMD, 0x81);	// Set contrast control
	SSD1306_Write(CMD, 0x80);	// Set Contrast to 128

	SSD1306_Write(CMD, 0xD9);	// Set pre-charge period
	SSD1306_Write(CMD, 0xF1);	// Phase 1 period of 15 DCLK, Phase 2 period of 1 DCLK

	SSD1306_Write(CMD, 0xDB);	// Set Vcomh deselect level
	SSD1306_Write(CMD, 0x20);	// Vcomh deselect level ~ 0.77 Vcc

	SSD1306_Write(CMD, 0xA4);	// Entire display ON, resume to RAM content display
	SSD1306_Write(CMD, 0xA6);	// Set Display in Normal Mode, 1 = ON, 0 = OFF
	SSD1306_Write(CMD, 0x2E);	// Deactivate scroll
	SSD1306_Write(CMD, 0xAF);	// Display ON in normal mode

	return 0;
}

/*
 * This function Fills the complete OLED with this data byte.
 *
 *  Arguments:
 *      data  -> Data to be filled in the OLED
 *
 */
static void SSD1306_Fill(unsigned char data)
{
	unsigned int total = 128 * 8;	// 8 pages x 128 segments x 8 bits of data
	unsigned int i = 0;

	// Fill the Display
	for (i = 0; i < total; i++)
		SSD1306_Write(DATA, data);
}

ssize_t writechar_store(struct device *dev, struct device_attribute *attr,
			const char *buf, size_t count)
{
	int j, num;
#define CMD_SET_PAGE_ROW_0TO6	0x22
#define CMD_SET_COL_0TO127	0x21

	// set page addr: ~ like row #; there r 7 of 'em(0-6)
	SSD1306_Write(CMD, CMD_SET_PAGE_ROW_0TO6);
	SSD1306_Write(CMD, 4);     // start addr
	SSD1306_Write(CMD, 7);     // end addr

	// set column address
	SSD1306_Write(CMD, CMD_SET_COL_0TO127);
	SSD1306_Write(CMD, 64);     //  col start
	SSD1306_Write(CMD, 127);   //  col end

	SSD1306_Fill(0x00);
	dev_dbg(dev, "buf = %s count = %d\n", buf, count);
	for (j = 0; j < count; j++) {
		if (buf[j] < '0' || buf[j] > 'z') {
			SSD1306_Write(DATA, 0x00);
			SSD1306_Write(DATA, 0x00);
			SSD1306_Write(DATA, 0x00);
			SSD1306_Write(DATA, 0x00);
			SSD1306_Write(DATA, 0x00);
		} else {
			dev_dbg(dev, "buf = %c(%d)\n", buf[j], (int)buf[j]);
			num = buf[j] - '0';
			if (buf[j] > '9')
				num = buf[j] - 87; /* as 'a' is ASCII 97 and our 'render'
				array has element 10 as 'a' */
			RENDER(num);
		}
	}
	SSD1306_Write(DATA, 0x00);
	SSD1306_Write(DATA, 0x00);

	return count;
}
DEVICE_ATTR_WO(writechar);

static int ssd1306_remove(struct i2c_client *client)
{
	SSD1306_Fill(0x00);	//fill the OLED with this data
	SSD1306_Write(CMD, 0xAE);	// Entire Display OFF
	device_remove_file(&client->dev, &dev_attr_writechar);
	pr_info("removed\n");

	return 0;
}

/* The probe method of our driver */
static int ssd1306_probe(struct i2c_client *client,	// named as 'client' or 'dev'
			 const struct i2c_device_id *id)
{
	/*
	 * first param: struct <foo>_client *client
	 * the specialized structure for this kernel framework (eg. i2c, spi, usb,
	 * pci, platform, etc); we often/usually extract the 'device' pointer from it..
	 */
	struct device *dev = &client->dev;

	dev_info(dev, "In probe! name=%s addr=0x%x\n", client->name, client->addr);

	/*
	 * Your work in the probe() routine:
	 * 1. Initialize the device
	 * 2. Prepare driver work: allocate a structure for a suitable
	 *    framework, allocate memory, map I/O memory, register interrupts...
	 * 3. When everything is ready, register the new device to the framework
	 */
	SSD1306_DisplayInit();
	SSD1306_Fill(0x00);	// fill the OLED with this data

	device_create_file(&client->dev, &dev_attr_writechar);

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
static const struct i2c_device_id ssd1306_id[] = {
	{SLAVE_DEVICE_NAME, 0},	/* matching by name; required for platform and i2c
				 * devices & drivers */
	// f.e.: { "pcf8563", 0 },
	{}
};

MODULE_DEVICE_TABLE(i2c, ssd1306_id);
/* 2. By DT 'compatible' property : for devices on the Device Tree
 *                                 (ARM32, Aarch64, PPC, etc)
 */
#ifdef CONFIG_OF
static const struct of_device_id ssd1306_of_match[] = {
	/*
	 * ARM/PPC/etc: matching by DT 'compatible' property
	 * 'compatible' property: one or more strings that define the specific
	 * programming model for the device. This list of strings should be used
	 * by a client program for device driver selection. The property value
	 * consists of a concatenated list of null terminated strings,
	 * from most specific to most general.
	 */
	{.compatible = "knb,ssd1306"},
	// f.e.:   { .compatible = "nxp,pcf8563" },
	{}
};

MODULE_DEVICE_TABLE(of, ssd1306_of_match);
#endif

/*
 * I2C driver Structure that has to be added to linux
 */
static struct i2c_driver ssd1306_driver = {
	.driver = {
		   .name = SLAVE_DEVICE_NAME,
		   .owner = THIS_MODULE,
		   },
	.probe = ssd1306_probe,
	.remove = ssd1306_remove,
	.id_table = ssd1306_id,
};

/*
 * I2C Board Info strucutre
 */
static struct i2c_board_info oled_i2c_board_info = {
	I2C_BOARD_INFO(SLAVE_DEVICE_NAME, SSD1306_SLAVE_ADDR)
};

static int __init oled_driver_init(void)
{
	int ret = -1;
	oled_i2c_adapter = i2c_get_adapter(I2C_BUS_SSD1306);

	if (oled_i2c_adapter != NULL) {
		i2c_client_oled =
		    i2c_new_client_device(oled_i2c_adapter, &oled_i2c_board_info);

		if (i2c_client_oled != NULL) {
			i2c_add_driver(&ssd1306_driver);
			ret = 0;
		}
		i2c_put_adapter(oled_i2c_adapter);
	}

	pr_info("Loaded\n");
	return ret;
}

static void __exit oled_driver_exit(void)
{
	i2c_unregister_device(i2c_client_oled);
	i2c_del_driver(&ssd1306_driver);
	pr_info("unloaded\n");
}

module_init(oled_driver_init);
module_exit(oled_driver_exit);
