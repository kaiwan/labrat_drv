/*
 * ssd1306.c

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

#define RENDER(n) \
{ int i; \
	for(i = 0; i < 7; i++) \
       		SSD1306_Write(false,render[n][i]);\
}

#define I2C_BUS_AVAILABLE   (          1 )	// I2C Bus available in our Raspberry Pi
#define SLAVE_DEVICE_NAME   ( "oled_ssd1306" )	// Device and Driver Name
#define SSD1306_SLAVE_ADDR  (       0x3C )	// SSD1306 OLED Slave Address

static struct i2c_adapter *oled_i2c_adapter = NULL;	// I2C Adapter Structure
static struct i2c_client *etx_i2c_client_oled = NULL;	// I2C Cient Structure (In our case it is OLED)
static u8 render[10][7] = {
	{0x00, 0x7f, 0x41, 0x41, 0x41, 0x7f, 0x00},	// 0
	{0x00, 0x44, 0x42, 0x7f, 0x40, 0x40, 0x00},	// 1
	{0x00, 0x79, 0x49, 0x49, 0x49, 0x4f, 0x00},	// 2
	{0x00, 0x49, 0x49, 0x49, 0x49, 0x7f, 0x00},	// 3
	{0x00, 0x0f, 0x08, 0x08, 0x08, 0x7f, 0x00},	// 4
	{0x00, 0x4f, 0x49, 0x49, 0x49, 0x79, 0x00},	// 5
	{0x00, 0x7f, 0x49, 0x49, 0x49, 0x79, 0x00},	// 6
	{0x00, 0x01, 0x01, 0x01, 0x01, 0x7f, 0x00},	// 7
	{0x00, 0x7f, 0x49, 0x49, 0x49, 0x7f, 0x00},	// 8
	{0x00, 0x4f, 0x49, 0x49, 0x49, 0x7f, 0x00}	// 9
};

/*
** This function writes the data into the I2C client
**
**  Arguments:
**      buff -> buffer to be sent
**      len  -> Length of the data
**   
*/
static int I2C_Write(unsigned char *buf, unsigned int len)
{
	/*
	 ** Sending Start condition, Slave address with R/W bit, 
	 ** ACK/NACK and Stop condtions will be handled internally.
	 */
	int ret = i2c_master_send(etx_i2c_client_oled, buf, len);

	return ret;
}

/*
** This function reads one byte of the data from the I2C client
**
**  Arguments:
**      out_buff -> buffer wherer the data to be copied
**      len      -> Length of the data to be read
** 
*/
static int I2C_Read(unsigned char *out_buf, unsigned int len)
{
	/*
	 ** Sending Start condition, Slave address with R/W bit, 
	 ** ACK/NACK and Stop condtions will be handled internally.
	 */
	int ret = i2c_master_recv(etx_i2c_client_oled, out_buf, len);

	return ret;
}

/*
** This function is specific to the SSD_1306 OLED.
** This function sends the command/data to the OLED.
**
**  Arguments:
**      is_cmd -> true = command, flase = data
**      data   -> data to be written
** 
*/
static void SSD1306_Write(bool is_cmd, unsigned char data)
{
	unsigned char buf[2] = { 0 };
	int ret;

	/*
	 ** First byte is always control byte. Data is followed after that.
	 **
	 ** There are two types of data in SSD_1306 OLED.
	 ** 1. Command
	 ** 2. Data
	 **
	 ** Control byte decides that the next byte is, command or data.
	 **
	 ** -------------------------------------------------------                        
	 ** |              Control byte's | 6th bit  |   7th bit  |
	 ** |-----------------------------|----------|------------|    
	 ** |   Command                   |   0      |     0      |
	 ** |-----------------------------|----------|------------|
	 ** |   data                      |   1      |     0      |
	 ** |-----------------------------|----------|------------|
	 ** 
	 ** Please refer the datasheet for more information. 
	 **    
	 */
	if (is_cmd == true)
		buf[0] = 0x00;
	else
		buf[0] = 0x40;
	buf[1] = data;
	ret = I2C_Write(buf, 2);
}

/*
** This function sends the commands that need to used to Initialize the OLED.
**
**  Arguments:
**      none
** 
*/
static int SSD1306_DisplayInit(void)
{
	msleep(100);		// delay

	/*
	 ** Commands to initialize the SSD_1306 OLED Display
	 */
	SSD1306_Write(true, 0xAE);	// Entire Display OFF
	SSD1306_Write(true, 0xD5);	// Set Display Clock Divide Ratio and Oscillator Frequency
	SSD1306_Write(true, 0x80);	// Default Setting for Display Clock Divide Ratio and Oscillator Frequency that is recommended
	SSD1306_Write(true, 0xA8);	// Set Multiplex Ratio
	SSD1306_Write(true, 0x3F);	// 64 COM lines
	SSD1306_Write(true, 0xD3);	// Set display offset
	SSD1306_Write(true, 0x00);	// 0 offset
	SSD1306_Write(true, 0x40);	// Set first line as the start line of the display
	SSD1306_Write(true, 0x8D);	// Charge pump
	SSD1306_Write(true, 0x14);	// Enable charge dump during display on
	SSD1306_Write(true, 0x20);	// Set memory addressing mode
	SSD1306_Write(true, 0x00);	// Horizontal addressing mode
	SSD1306_Write(true, 0xA1);	// Set segment remap with column address 127 mapped to segment 0
	SSD1306_Write(true, 0xC8);	// Set com output scan direction, scan from com63 to com 0
	SSD1306_Write(true, 0xDA);	// Set com pins hardware configuration
	SSD1306_Write(true, 0x12);	// Alternative com pin configuration, disable com left/right remap
	SSD1306_Write(true, 0x81);	// Set contrast control
	SSD1306_Write(true, 0x80);	// Set Contrast to 128
	SSD1306_Write(true, 0xD9);	// Set pre-charge period
	SSD1306_Write(true, 0xF1);	// Phase 1 period of 15 DCLK, Phase 2 period of 1 DCLK
	SSD1306_Write(true, 0xDB);	// Set Vcomh deselect level
	SSD1306_Write(true, 0x20);	// Vcomh deselect level ~ 0.77 Vcc
	SSD1306_Write(true, 0xA4);	// Entire display ON, resume to RAM content display
	SSD1306_Write(true, 0xA6);	// Set Display in Normal Mode, 1 = ON, 0 = OFF
	SSD1306_Write(true, 0x2E);	// Deactivate scroll
	SSD1306_Write(true, 0xAF);	// Display ON in normal mode

	return 0;
}

/*
** This function Fills the complete OLED with this data byte.
**
**  Arguments:
**      data  -> Data to be filled in the OLED
** 
*/
static void SSD1306_Fill(unsigned char data)
{
	unsigned int total = 128 * 8;	// 8 pages x 128 segments x 8 bits of data
	unsigned int i = 0;

	//Fill the Display
	for (i = 0; i < total; i++) {
		SSD1306_Write(false, data);
	}
}

ssize_t writechar_store(struct device *dev, struct device_attribute *attr,
			const char *buf, size_t count)
{
	int j;
	int num;

	SSD1306_Write(true, 0x22);	// set page addr
	SSD1306_Write(true, 0);
	SSD1306_Write(true, 7);

	SSD1306_Write(true, 0x21);
	SSD1306_Write(true, 0);
	SSD1306_Write(true, 127);

	SSD1306_Fill(0x00);
	pr_info("Buff = %s count = %d\n", buf, count);
	for (j = 0; j < count; j++) {
		if (buf[j] < '0' || buf[j] > '9') {
			SSD1306_Write(false, 0x00);
			SSD1306_Write(false, 0x00);
			SSD1306_Write(false, 0x00);
			SSD1306_Write(false, 0x00);
			SSD1306_Write(false, 0x00);
		} else {
			num = buf[j] - '0';
			RENDER(num);
		}
	}
	SSD1306_Write(false, 0x00);
	SSD1306_Write(false, 0x00);
	return count;
}

DEVICE_ATTR_WO(writechar);
/*
** This function getting called when the slave has been found
** Note : This will be called only once when we load the driver.
static int etx_oled_probe(struct i2c_client *client,
                         const struct i2c_device_id *id)
{
	int i;
    	SSD1306_DisplayInit();
    
    	//fill the OLED with this data
    	SSD1306_Fill(0x00);

	RENDER(2);
	RENDER(1);
	RENDER(2);
	RENDER(7);
	RENDER(3);
	RENDER(2);

	device_create_file(&client->dev,&dev_attr_writechar);
    	pr_info("OLED Probed!!!\n");
    
    	return 0;
}
*/

static int ssd1306_remove(struct i2c_client *client)
{
	//fill the OLED with this data
	SSD1306_Fill(0x00);
	device_remove_file(&client->dev, &dev_attr_writechar);
	pr_info("OLED Removed!!!\n");
	return 0;
}

//////////////////////////////////////////////////////////////////////

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

	return 0;
}

/*
** Structure that has slave device id
static const struct i2c_device_id etx_oled_id[] = {
        { SLAVE_DEVICE_NAME, 0 },
        { }
};
MODULE_DEVICE_TABLE(i2c, etx_oled_id);
*/

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
** I2C driver Structure that has to be added to linux
*/
static struct i2c_driver ssd1306_driver = {
	.driver = {
		   .name = "ssd1306",
		   .owner = THIS_MODULE,
		   },
	.probe = ssd1306_probe,
	.remove = ssd1306_remove,
	.id_table = ssd1306_id,
	//.probe          = etx_oled_probe,
	//.remove         = etx_oled_remove,
	//.id_table       = etx_oled_id,
};

/*
** I2C Board Info strucutre
*/
static struct i2c_board_info oled_i2c_board_info = {
	I2C_BOARD_INFO(SLAVE_DEVICE_NAME, SSD1306_SLAVE_ADDR)
};

/*
** Module Init function
*/
static int __init oled_driver_init(void)
{
	int ret = -1;
	oled_i2c_adapter = i2c_get_adapter(I2C_BUS_AVAILABLE);

	if (oled_i2c_adapter != NULL) {
		etx_i2c_client_oled =
		    i2c_new_client_device(oled_i2c_adapter, &oled_i2c_board_info);
		if (etx_i2c_client_oled != NULL) {
			i2c_add_driver(&ssd1306_driver);
			ret = 0;
		}
		i2c_put_adapter(oled_i2c_adapter);
	}

	pr_info("Loaded\n");
	return ret;
}

/*
** Module Exit function
*/
static void __exit oled_driver_exit(void)
{
	i2c_unregister_device(etx_i2c_client_oled);
	i2c_del_driver(&ssd1306_driver);
	pr_info("Driver Removed!!!\n");
}

module_init(oled_driver_init);
module_exit(oled_driver_exit);

MODULE_AUTHOR("EmbeTronicX,Subhrajyoti S,Kaiwan NB");
MODULE_DESCRIPTION("SSD1306 OLED display simple I2C driver");
MODULE_LICENSE("GPL");
