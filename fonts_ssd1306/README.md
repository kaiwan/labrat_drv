**labrat_drv / fonts_ssd1306 : README**

The new and redone SSD1306 OLED display driver-with-a-few-larger-fonts project !

We assume the OLED SSD1306 is in landscape orientation..
Dimensions in landscape orientation (which we always assume its in):
 18 x 8 (cols x rows)

Colo(u)rs:
 Row 0 and row 1 renders in yellow color
 Rows 2 to 7 render in blue color

You can render the display in two fonts:
1. The 'default' small 8x8 bit font:
   Supported characters:
   A-Z, digits 0-9, space, %
2. The 'LARGE' font:
   This renders only a few supported chars in a 'large' font occupying rows 2 to 6
   (thus rows 0, 1 and 7 are still available for small font chars)
   Supported characters:
   digits 0-9, space, C, %
   [This particular implementation's geared towards rendering a Temperature and
   Humidity values in Celsius (C) or as a percentage (%) resp]

The ssd1306 kernel module driver sets up several sysfs pseudofiles which is of
course the interface via which we can write to the rows of the display; the
following sysfs entries are created under (typically)
/sys/bus/i2c/devices/1-003c :

--w--w--w- 1 root root 4.0K Nov  2 16:09 write_largefont_rows2to6
--w--w--w- 1 root root 4.0K Nov  2 16:10 write_smallfont_to_row0
--w--w--w- 1 root root 4.0K Nov  2 16:10 write_smallfont_to_row1
--w--w--w- 1 root root 4.0K Nov  2 16:10 write_smallfont_to_row2
--w--w--w- 1 root root 4.0K Nov  2 16:10 write_smallfont_to_row3
--w--w--w- 1 root root 4.0K Nov  2 16:10 write_smallfont_to_row4
--w--w--w- 1 root root 4.0K Nov  2 16:10 write_smallfont_to_row5
--w--w--w- 1 root root 4.0K Nov  2 16:10 write_smallfont_to_row6
--w--w--w- 1 root root 4.0K Nov  2 16:10 write_smallfont_to_row7

Note that the entries are root writable only by default (our try script chmod's
the perm to make it world-writable).

See the `try` script to learn how to write to the display via several examples..



*WIP*
                
   COLS --->

ROWS

&nbsp;&nbsp;&nbsp;+------------------------------------------------+

&nbsp;&nbsp;&nbsp;|

0|    small 8x8 font : row 0                      |

|         1|    small 8x8 font : row 1                      |
|          +------------------------------------------------+
|         2|              LARGER FONT for                   |
|         3|                these rows                      |
|         4|                                                |
|         5|                                                |
|         6|                 2 to 6                         |
|          +------------------------------------------------+
|         7|    small 8x8 font : row 7                      |
v          +------------------------------------------------+


