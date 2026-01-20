/*
 * ssd1306.c
 *
 * Part of the SSD1306 OLED display driver-with-a-few-larger-fonts project
 * Tested on : Raspberry Pi family
 *
 * (c) Kaiwan N Billimoria, kaiwanTECH
 * License: MIT
 *
 * TODO-
 *  [+] clear full screen
 *  [ ] clear curr row
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
#include "font_l.h"	// all our 'fonts'

MODULE_AUTHOR("EmbeTronicX,Subhrajyoti S,Kaiwan N Billimoria");
MODULE_DESCRIPTION("SSD1306 OLED display simple I2C driver (with custom font)");
MODULE_LICENSE("GPL");

/* Module parameters */
static int i2cbus = -1;
module_param(i2cbus, int, 0600);
MODULE_PARM_DESC(i2cbus, "I2C bus # to use:\n"
"Set to 1	For the Raspberry Pi boards     -OR-"
"	For the TI BeaglePlay connected via a Grove connector\n"
"Set to 3	For the TI BeaglePlay connected via the MikroBUS connector");

#define SLAVE_DEVICE_NAME   "oled_ssd1306"	// Device and Driver Name
#define SSD1306_SLAVE_ADDR   0x3C	// SSD1306 OLED Slave Address

#define CMD	(true)
#define DATA	(false)

static struct i2c_adapter *oled_i2c_adapter;	// I2C adapter Structure
static struct i2c_client *i2c_client_oled;	// I2C client Structure (the SSD1306 OLED)
static DEFINE_MUTEX(mtx);

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

#if 0
// this func is currently unused
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
#endif

/*
 * This function is specific to the SSD_1306 OLED.
 * This function sends the command/data to the OLED.
 *  Arguments:
 *      is_cmd -> CMD (Bool true) => command,
 *                DATA (Bool false) => data
 *      data   -> the data item to be written
 */
static void SSD1306_Write(bool is_cmd, unsigned char data)
{
	unsigned char buf[2] = { 0 };

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
	I2C_Write(buf, 2);
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

// Pretty horizontal lines all in same col increasing lengths!
#define	TEST_H_LINES() do { \
	int x = 3, start_row = 3; \
	LINE_H_HALFU(x, start_row); \
	LINE_H_1U(x, start_row+1); \
	LINE_H_1PT5U(x, start_row+2); \
	LINE_H_2U(x, start_row+3); \
} while (0)

// Pretty vertical lines all in same row increasing heights!
#define	TEST_V_LINES() do { \
	int y = 1, start_col = 70; \
	LINE_V_1U(start_col, y); \
	LINE_V_2U(start_col+5, y); \
	LINE_V_3U(start_col+10, y); \
	LINE_V_4U(start_col+15, y); \
} while (0)

#if 0
#define	DISPLAY_TEMPERATURE(X, Y) do { \
	int h_off = 30;    \
	DIGIT_2((X), (Y));   \
	DIGIT_4((X)+h_off, (Y));   \
	PERIOD((X)+h_off+25, (5));   \
	DIGIT_9((X)+h_off+25+15, (Y));  \
	LETTER_C((X)+h_off+25+15+28, (Y));  \
	/* so h_off+25+15+28 = 98 ; \
	 * And the last # starts here with len of about LINE_1U_LEN/2 ie 15 \
	 * So, it's totally about 98+15 = 113 cols used! (127 is last col) \
	 */
} while (0)

#define	DISPLAY_HUMIDITY(X, Y) do { \
	int h_off = 30;    \
	DIGIT_8((X), (Y));   \
	DIGIT_5((X)+h_off, (Y));   \
	PERIOD((X)+h_off+25, (5));   \
	DIGIT_1((X)+h_off+25+15, (Y));  \
	LETTER_H((X)+h_off+25+15+25, (Y)); \
	/* so h_off+25+15+28 = 98 ; \
	 * And the last # starts here with len of about LINE_1U_LEN/2 ie 15 \
	 * So, it's totally about 98+15 = 113 cols used! (127 is last col) \
	 */
} while (0)
#endif

static inline u8 centre_pos(s8);
#define MAXCHARS_SMALLFONT	18
// Approx centre the string
static inline u8 centre_pos(s8 len)
{
	s8 col;

	// each display char takes 8 bits
	col = MAXCHARS_SMALLFONT*8-len*8;
	if (col > 1)
		col /= 2;
	else
		col = 0;

	return col;
}

static inline u8 setup_str_to_display(const char *buf, size_t len)
{
	/* If a newline's present, ignore it */
	if (buf[len-1] == 0x0a)
		len--;
	return centre_pos(len);
}

#define	CLEAR_ROW(ROW)  do {  \
	int j;  \
	START_POS_SMALL_LETTERS(0, (ROW));  \
	for (j = 0; j < MAX_COL+1; j++)  \
		SSD1306_Write(DATA, 0x00);  \
} while (0)


/*
 * Write the user-supplied string in the XXXXX format - ONLY 5 chars allowed -
 * to rows 2 to 6 of the OLED (landscape orientation assumed) in our LARGE
 * custom font.
 * Practically, it's meant for the use case where we display temperature
 * and humidity values...
 *
 * TIP for userspace:
 * Do 'echo -n "XXXXX" > ...' not echo "XXXXX" > ...' to avoid passing the newline
 * char
 */
static ssize_t write_largefont_rows2to6_store(struct device *dev, struct device_attribute *attr,
			const char *buf, size_t count)
{
	int i, h_off = 20;
	int X = 10, Y = 2; // from col X, row Y
	static const char large_msg_warning[] = "WARNING: for the 'large' font, ONLY this format is allowed: NN.NX; where N=0..9 and X is either C (Celsius for temperature) or % (for Humidity percentage)";
	// Select X offset to next char dynamically! based on the prev char...
	int x_offset[] = {
		30, // for char 0
		10, // for char 1
		25, // for char 2
		25, // for char 3
		25, // for char 4
		25, // for char 5
		25, // for char 6
		25, // for char 7
		25, // for char 8
		25, // for char 9
	};
#define X_COL_FOR_C	98
//#define X_COL_FOR_H	80
#define ROW_FOR_PERIOD	 6

	if (mutex_lock_interruptible(&mtx))
		return -EINTR;

	//--- validity checks
	if (count > 5) {
		dev_warn(dev, "error: max of 5 chars allowed (%lu passed)\nTip: use echo -n 'NN.N{C%%}' > ...\n%s\n",
			count, large_msg_warning);
		mutex_unlock(&mtx);
		return -ERANGE;
	}
	if (buf[count-1] == '%') {
		if (count == 4) //  N.N %
			X = 38;
		else
			X = 20; // NN.N %
	}

#if 1
	//dev_info(dev, "buf[%d-1]=%c\n", count-1, buf[count-1]);
	if ((buf[count-1] != 'C') && (buf[count-1] != '%')) {
		dev_warn(dev, "%s\n", large_msg_warning);
		mutex_unlock(&mtx);
		return -EINVAL;
	}
#endif
	pr_debug("buf=%s len=%zu\n", buf, count);

	for (i = 2; i <= 6; i++)
		CLEAR_ROW(i);

	for (i = 0; i < count; i++) {
		switch (buf[i]) {
		case '0':
			DIGIT_0(X, Y);
			  /* Use a dynamic X (col) offset value; we use a table
			   * lookup for it... idx is (buf[i]-48) as ASCII 48 is '0'
			   */
			X += x_offset[(int)buf[i]-48];
			//X += h_off;
			break;
		case '1':
			DIGIT_1(X, Y);
			X += x_offset[(int)buf[i]-48];
			break;
		case '2':
			DIGIT_2(X, Y);
			X += x_offset[(int)buf[i]-48];
			break;
		case '3':
			DIGIT_3(X, Y);
			X += x_offset[(int)buf[i]-48];
			break;
		case '4':
			DIGIT_4(X, Y);
			X += x_offset[(int)buf[i]-48];
			break;
		case '5':
			DIGIT_5(X, Y);
			X += x_offset[(int)buf[i]-48];
			break;
		case '6':
			DIGIT_6(X, Y);
			X += x_offset[(int)buf[i]-48];
			break;
		case '7':
			DIGIT_7(X, Y);
			X += x_offset[(int)buf[i]-48];
			break;
		case '8':
			DIGIT_8(X, Y);
			X += x_offset[(int)buf[i]-48];
			break;
		case '9':
			DIGIT_9(X, Y);
			X += x_offset[(int)buf[i]-48];
			break;
		case '.':
			PERIOD(X, ROW_FOR_PERIOD);
			/*
			 * Both KASAN & UBSAN detect a bug here!!
			 * X += x_offset[(int)buf[i]-48];
			 * ...
			 * UBSAN: array-index-out-of-bounds in /.../fonts_ssd1306/ssd1306.c:385:17
			 * index -2 is out of range for type 'int [10]'
			 * ...
			 * Root cause analysis:
			 * buf[i] is '.' (period char), ASCII value 46;
			 * so buf[i]-48 = 46-48 = -2 ! INVALID!
			 */
			X += 12;
			break;
		case 'C':
			LETTER_C(X_COL_FOR_C, Y);
			X += h_off;
			break;
		/* Exception: the '%' is in the small 8x8 font; at least
		 * until we successfully render a large font ver
		 */
		case '%':
			PERCENT_SMALLFONT(X+7, Y+2);
			X += h_off;
			break;
#if 0
		case 'H': //pr_debug("X=%d\n", X); /* 'C' or 'H' must be the last char */
			  //LETTER_H(X, Y);
			  LETTER_H(X_COL_FOR_H, Y);
			  X += h_off;
			  break;
#endif
		default:
			dev_warn(dev, "INVALID char '%c'(0x%x)\n%s\n",
				buf[i], buf[i], large_msg_warning);
			break;
		}
	}
	mutex_unlock(&mtx);

	return count;
}
static DEVICE_ATTR_WO(write_largefont_rows2to6);

/*
 * 3 rows available on the OLED in landscape mode: 0, 1 and 7 only.
 * The MAX # of chars in a single row: MAXCHARS_SMALLFONT (18)
 * ASSUMPTIONS:
 * - OLED display is in landscape orientation
 * - 'buf' contains an ASCIIZ string (NUL-terminated)
 */

static void write_string_smallfont(const char *buf, u8 x, u8 y)
{
	int j, len = strlen(buf);
	s8 char2write;

	if ((y < 0 || (y > MAX_ROW_PAGE)) ||
		((x < 0) || (x >= MAX_COL-8)))
		pr_debug("warning: exceeding row/col limits: trying to write to col %d row %d\n",
			x, y);

	/* If a newline's present, ignore it */
	if (buf[len-1] == 0x0a)
		len--;
	if (len > MAXCHARS_SMALLFONT)
		pr_debug("warning: exceeding max chars per row (%d), trying to write %d\nstr: %s\n",
			MAXCHARS_SMALLFONT, len, buf);

	CLEAR_ROW(y);
	START_POS_SMALL_LETTERS(x, y);
	for (j = 0; j < len; j++) {
		/* ONLY consider ASCII 48 - 58 (digits 0-9),
		 * the uppercase letters A-Z, AND whitespace char (32)
		 * NO.. this is v limiting..
		 */
		char2write = toupper(buf[j]);
		//pr_debug("buf = %c(%d)\n", buf[j], (int)buf[j]);
	#if 0
		if (char2write != ' ' && (char2write < '0' || char2write > 'Z'))
			SSD1306_Write(DATA, 0x00);
		else
	#endif
			RENDER_SMALLFONT(char2write);
	}
}


/*
 * Write the user-supplied string to row 7 of the OLED (landscape orientation assumed)
 * ASSUME that 'buf' contains an ASCIIZ string (NUL-terminated)
 */
static ssize_t write_smallfont_to_row7_store(struct device *dev, struct device_attribute *attr,
			const char *buf, size_t count)
{
	//char *kbuf;

	if (mutex_lock_interruptible(&mtx))
		return -EINTR;

#if 0
	kbuf = kzalloc(count, GFP_KERNEL);
	if (unlikely(!kbuf)) {
		mutex_unlock(&mtx);
		return -ENOMEM;
	}
	if (copy_from_user(kbuf, buf, count)) {
		kfree(kbuf);
		mutex_unlock(&mtx);
		return -EIO;
	}
	// Get rid of the newline if any
	len = strlen(kbuf);
	//print_hex_dump_bytes("kbuf", DUMP_PREFIX_OFFSET, kbuf, len);
	if (kbuf[len-1] == 0x0a) {
		kbuf[len-1] = '\0';
		len--;
	}
#endif

	write_string_smallfont(buf, setup_str_to_display(buf, count), 7);

//	kfree(kbuf);
	mutex_unlock(&mtx);

	return count;
}
static DEVICE_ATTR_WO(write_smallfont_to_row7);
/*
 * Write the user-supplied string to row 6 of the OLED (landscape orientation assumed)
 */
static ssize_t write_smallfont_to_row6_store(struct device *dev, struct device_attribute *attr,
			const char *buf, size_t count)
{
	if (mutex_lock_interruptible(&mtx))
		return -EINTR;
	write_string_smallfont(buf, setup_str_to_display(buf, count), 6);
	mutex_unlock(&mtx);

	return count;
}
static DEVICE_ATTR_WO(write_smallfont_to_row6);
/*
 * Write the user-supplied string to row 5 of the OLED (landscape orientation assumed)
 */
static ssize_t write_smallfont_to_row5_store(struct device *dev, struct device_attribute *attr,
			const char *buf, size_t count)
{
	if (mutex_lock_interruptible(&mtx))
		return -EINTR;
	write_string_smallfont(buf, setup_str_to_display(buf, count), 5);
	mutex_unlock(&mtx);

	return count;
}
static DEVICE_ATTR_WO(write_smallfont_to_row5);
/*
 * Write the user-supplied string to row 4 of the OLED (landscape orientation assumed)
 */
static ssize_t write_smallfont_to_row4_store(struct device *dev, struct device_attribute *attr,
			const char *buf, size_t count)
{
	if (mutex_lock_interruptible(&mtx))
		return -EINTR;
	write_string_smallfont(buf, setup_str_to_display(buf, count), 4);
	mutex_unlock(&mtx);

	return count;
}
static DEVICE_ATTR_WO(write_smallfont_to_row4);
/*
 * Write the user-supplied string to row 3 of the OLED (landscape orientation assumed)
 */
static ssize_t write_smallfont_to_row3_store(struct device *dev, struct device_attribute *attr,
			const char *buf, size_t count)
{
	if (mutex_lock_interruptible(&mtx))
		return -EINTR;
	write_string_smallfont(buf, setup_str_to_display(buf, count), 3);
	mutex_unlock(&mtx);

	return count;
}
static DEVICE_ATTR_WO(write_smallfont_to_row3);
/*
 * Write the user-supplied string to row 2 of the OLED (landscape orientation assumed)
 */
static ssize_t write_smallfont_to_row2_store(struct device *dev, struct device_attribute *attr,
			const char *buf, size_t count)
{
	if (mutex_lock_interruptible(&mtx))
		return -EINTR;
	write_string_smallfont(buf, setup_str_to_display(buf, count), 2);
	mutex_unlock(&mtx);

	return count;
}
static DEVICE_ATTR_WO(write_smallfont_to_row2);
/*
 * Write the user-supplied string to row 1 of the OLED (landscape orientation assumed)
 */
static ssize_t write_smallfont_to_row1_store(struct device *dev, struct device_attribute *attr,
			const char *buf, size_t count)
{
	if (mutex_lock_interruptible(&mtx))
		return -EINTR;
	write_string_smallfont(buf, setup_str_to_display(buf, count), 1);
	mutex_unlock(&mtx);

	return count;
}
static DEVICE_ATTR_WO(write_smallfont_to_row1);

/*
 * Write the user-supplied string to row 0 of the OLED (landscape orientation assumed)
 */
static ssize_t write_smallfont_to_row0_store(struct device *dev, struct device_attribute *attr,
			const char *buf, size_t count)
{
	if (mutex_lock_interruptible(&mtx))
		return -EINTR;
	write_string_smallfont(buf, setup_str_to_display(buf, count), 0);
	mutex_unlock(&mtx);

	return count;
}
static DEVICE_ATTR_WO(write_smallfont_to_row0);

#if LINUX_VERSION_CODE >= KERNEL_VERSION(6, 1, 0)
static void ssd1306_remove(struct i2c_client *client)
#else
static int ssd1306_remove(struct i2c_client *client)
#endif
{
	SSD1306_Fill(0x00);	//fill the OLED with this data
	SSD1306_Write(CMD, 0xAE);	// Entire Display OFF
#if 0
	device_remove_file(&client->dev, &dev_attr_row_end);
	device_remove_file(&client->dev, &dev_attr_row_start);
	device_remove_file(&client->dev, &dev_attr_col_end);
	device_remove_file(&client->dev, &dev_attr_col_start);
#endif

	device_remove_file(&client->dev, &dev_attr_write_largefont_rows2to6);
	device_remove_file(&client->dev, &dev_attr_write_smallfont_to_row7);
	device_remove_file(&client->dev, &dev_attr_write_smallfont_to_row6);
	device_remove_file(&client->dev, &dev_attr_write_smallfont_to_row5);
	device_remove_file(&client->dev, &dev_attr_write_smallfont_to_row4);
	device_remove_file(&client->dev, &dev_attr_write_smallfont_to_row3);
	device_remove_file(&client->dev, &dev_attr_write_smallfont_to_row2);
	device_remove_file(&client->dev, &dev_attr_write_smallfont_to_row1);
	device_remove_file(&client->dev, &dev_attr_write_smallfont_to_row0);
	pr_info("all sysfs files removed, display Off\n");

#if LINUX_VERSION_CODE < KERNEL_VERSION(6, 1, 0)
	return 0;
#endif
}

/* The probe method of our driver */
#if LINUX_VERSION_CODE >= KERNEL_VERSION(6, 3, 0)
static int ssd1306_probe(struct i2c_client *client) 	// named as 'client' or 'dev'
#else
static int ssd1306_probe(struct i2c_client *client,	// named as 'client' or 'dev'
		       const struct i2c_device_id *id)
#endif
{
	int ret = 0;

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
	SSD1306_Fill(0xAB);	// fill the OLED with this data

	/* Create the sysfs pseudofiles, one for each display row we can write to
	 * (so a total of 8 as there are 8 rows)
	 * Well, row 0 to row 7 small 8x8 font write AND 
	 * one sysfile entry for rows 2-6 LARGE font write (typically for the
	 * Temperature / Humidity values to be displayed)
	 */
	// rows 0 to 7 : SMALL 8x8 font display
	ret = device_create_file(&client->dev, &dev_attr_write_smallfont_to_row0);
	if (ret < 0) {
		dev_info(dev, "creating sysfs entry write_smallfont_to_row0 failed");
		goto out_fail0;
	}
	ret = device_create_file(&client->dev, &dev_attr_write_smallfont_to_row1);
	if (ret < 0) {
		dev_info(dev, "creating sysfs entry write_smallfont_to_row1 failed");
		goto out_fail1;
	}
	ret = device_create_file(&client->dev, &dev_attr_write_smallfont_to_row2);
	if (ret < 0) {
		dev_info(dev, "creating sysfs entry write_smallfont_to_row2 failed");
		goto out_fail2;
	}
	ret = device_create_file(&client->dev, &dev_attr_write_smallfont_to_row3);
	if (ret < 0) {
		dev_info(dev, "creating sysfs entry write_smallfont_to_row3 failed");
		goto out_fail3;
	}
	ret = device_create_file(&client->dev, &dev_attr_write_smallfont_to_row4);
	if (ret < 0) {
		dev_info(dev, "creating sysfs entry write_smallfont_to_row4 failed");
		goto out_fail4;
	}
	ret = device_create_file(&client->dev, &dev_attr_write_smallfont_to_row5);
	if (ret < 0) {
		dev_info(dev, "creating sysfs entry write_smallfont_to_row5 failed");
		goto out_fail5;
	}
	ret = device_create_file(&client->dev, &dev_attr_write_smallfont_to_row6);
	if (ret < 0) {
	//if (ret == 0) {
		dev_info(dev, "creating sysfs entry write_smallfont_to_row6 failed");
		goto out_fail6;
	}
	ret = device_create_file(&client->dev, &dev_attr_write_smallfont_to_row7);
	if (ret < 0) {
		dev_info(dev, "creating sysfs entry write_smallfont_to_row7 failed");
		goto out_fail7;
	}
	// rows 2 to 6 : LARGE font display
	ret = device_create_file(&client->dev, &dev_attr_write_largefont_rows2to6);
	if (ret < 0) {
		dev_info(dev, "creating sysfs entry write_largefont_rows2to6 failed");
		goto out_fail8;
	}

	dev_info(dev, "all sysfs files setup, display On\n");
	return ret;

out_fail8:
	device_remove_file(&client->dev, &dev_attr_write_smallfont_to_row7);
out_fail7:
	device_remove_file(&client->dev, &dev_attr_write_smallfont_to_row6);
out_fail6:
	device_remove_file(&client->dev, &dev_attr_write_smallfont_to_row5);
out_fail5:
	device_remove_file(&client->dev, &dev_attr_write_smallfont_to_row4);
out_fail4:
	device_remove_file(&client->dev, &dev_attr_write_smallfont_to_row3);
out_fail3:
	device_remove_file(&client->dev, &dev_attr_write_smallfont_to_row2);
out_fail2:
	device_remove_file(&client->dev, &dev_attr_write_smallfont_to_row1);
out_fail1:
	device_remove_file(&client->dev, &dev_attr_write_smallfont_to_row0);
out_fail0:
	return -ENODEV;
#if 0
	ret = device_create_file(&client->dev, &dev_attr_row_start);
	if (ret < 0) {
		dev_info(dev, "creating sysfs entry rowstart failed");
		return -ENODEV;
	}
	ret = device_create_file(&client->dev, &dev_attr_row_end);
	if (ret < 0) {
		dev_info(dev, "creating sysfs entry rowend failed");
		return -ENODEV;
	}
	ret = device_create_file(&client->dev, &dev_attr_col_start);
	if (ret < 0) {
		dev_info(dev, "creating sysfs entry colstart failed");
		return -ENODEV;
	}
	ret = device_create_file(&client->dev, &dev_attr_col_end);
	if (ret < 0) {
		dev_info(dev, "creating sysfs entry colend failed");
		return -ENODEV;
	}
#endif
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
/* DT isn't being used as of now at least ... */
static const struct of_device_id ssd1306_of_match[] = {
	/*
	 * ARM/PPC/etc: matching by DT 'compatible' property
	 * 'compatible' property: one or more strings that define the specific
	 * programming model for the device. This list of strings should be used
	 * by a client program for device driver selection. The property value
	 * consists of a concatenated list of null terminated strings,
	 * from most specific to most general.
	 */
	{.compatible = "solomon,ssd1306"},
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

	if (i2cbus == -1) {
		pr_info("need to pass the 'i2cbus' module parameter\n");
		return -EINVAL;
	}
	oled_i2c_adapter = i2c_get_adapter(i2cbus);
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
