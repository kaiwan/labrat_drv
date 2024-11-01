/*
 * font_l.h
 * Part of the SSD1306 driver-with-a-few-larger-fonts project
 *
 * (c) Kaiwan N Billimoria, kaiwanTECH
 * License: MIT
 */

/* Use the XWin 'bitmap' app to render the characters.
 * Keep last col zero to, in effect, have some spacing before the next char
 * The 'original' bitmap files are in font_bitmaps/
 * NOTE:
 * The bitmap needs to be rotated anti-clockwise 90 degrees to correctly
 * appear on the OLED when in landscape mode!
 * <In the bitmap app, after drawing it, press lower-left (SW) arrow followed
 *  by the upper-left (NW) arrow to do the xform>
 */
#undef USE_3D_FONT

#ifdef USE_3D_FONT
static unsigned char line_h_bits_3D[] = {  /* 4 vertical lines in 'bitmap' app
   which then get rotated clockwise 90 degrees on the OLED when in landscape mode.
   Now the line thickness is slightly better
   */
   // Interesting!! this one - where the h line is tapered at both ends -
   // (font_bitmaps/line-h-t.h) causes a 3D effect !!
   0x80, 0x00, 0x40, 0x01, 0xc0, 0x01, 0xc0, 0x01, 0xc0, 0x01, 0xc0, 0x01,
   0xc0, 0x01, 0xc0, 0x01, 0xc0, 0x01, 0xc0, 0x01, 0xc0, 0x01, 0xc0, 0x01,
   0xc0, 0x01, 0xc0, 0x01, 0x40, 0x01, 0x80, 0x00};
#else
/* ORIG - 2D: */
static unsigned char line_h_bits[] = {
   0x0f, 0x00, 0x0f, 0x00, 0x0f, 0x00, 0x0f, 0x00, 0x0f, 0x00, 0x0f, 0x00,
   0x0f, 0x00, 0x0f, 0x00, 0x0f, 0x00, 0x0f, 0x00, 0x0f, 0x00, 0x0f, 0x00,
   0x0f, 0x00, 0x0f, 0x00, 0x0f, 0x00, 0x0f, 0x00};
#endif

#if 0
  /* 2 vertical lines in 'bitmap' app
   which then get rotated clockwise 90 degrees on the OLED when in landscape mode
   0x03, 0x00, 0x03, 0x00, 0x03, 0x00, 0x03, 0x00, 0x03, 0x00, 0x03, 0x00,
   0x03, 0x00, 0x03, 0x00, 0x03, 0x00, 0x03, 0x00, 0x03, 0x00, 0x03, 0x00,
   0x03, 0x00, 0x03, 0x00, 0x03, 0x00, 0x03, 0x00}; 
   */
#endif


static unsigned char line_v_bits[] = {  /* 2 horiontal lines in 'bitmap' app
   which then get rotated clockwise 90 degrees on the OLED when in landscape mode
   */
   0xff, 0xff, 0xff, 0xff, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
   0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
   0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};

/*
static unsigned char line_slash_bits[] = {
   0x00, 0x80, 0x00, 0xc0, 0x00, 0x60, 0x00, 0x30, 0x00, 0x18, 0x00, 0x0c,
   0x00, 0x06, 0x00, 0x03, 0x80, 0x01, 0xc0, 0x00, 0x60, 0x00, 0x30, 0x00,
   0x18, 0x00, 0x0c, 0x00, 0x06, 0x00, 0x03, 0x00};
//
   0x00, 0xe0, 0x00, 0xf0, 0x00, 0x78, 0x00, 0x3c, 0x00, 0x1e, 0x00, 0x0f,
   0x80, 0x07, 0xc0, 0x03, 0xe0, 0x01, 0xf0, 0x00, 0x78, 0x00, 0x3c, 0x00,
   0x1e, 0x00, 0x0f, 0x00, 0x07, 0x00, 0x03, 0x00};*/

static unsigned char percent_bits[] = {
   0x00, 0x44, 0x24, 0x10, 0x08, 0x24, 0x22, 0x00};



/* See SSD1306 Solomon Systech datasheet Rev 1.1 pg 31 1st table 1st row */
// Our addr mode is Horizontal

#define CMD_SET_PAGE_ROW_0TO7	0x22
#define CMD_SET_COL_0TO127	0x21
#define MAX_ROW_PAGE	  7
#define MAX_COL		127

#define SET_START_END_PAGES(S, E) do { \
	SSD1306_Write(CMD, CMD_SET_PAGE_ROW_0TO7); \
	SSD1306_Write(CMD, (S)); \
	SSD1306_Write(CMD, (E)); \
} while(0)
#define SET_START_END_COLS(S, E) do { \
	SSD1306_Write(CMD, CMD_SET_COL_0TO127); \
	SSD1306_Write(CMD, (S)); \
	SSD1306_Write(CMD, MAX_COL); \
} while(0)

#define	SET_START2BLUE_LINE1 SSD1306_Write(CMD, 0x23) /* start page 3 - 1st blue line! */
#define	SET_START2BLUE_LINE2 SSD1306_Write(CMD, 0x24) /* start page 4 - 2nd blue line! */

/*
 * The 'large' Font 'Drawings' !
 * As we can't use the bitmap o/p directly - its far to small in size to read
 * at any distance - we're building our own 'large font'..
 * We base it on LINE drawing - horizontal and vertical lines, thats it!
 * Somewhat akin to the old 7-segment display fonts!
 * (See https://www.cufonfonts.com/images/116399/seven-segment-font-large-preview.png)
 */

// Size of the bitmap in bytes
#define LINE_1U_LEN 32 //ARRAY_LEN(line_bits1) //32
/* IU = 8 bits (1 'unit');
 * H = horizontal => landscape orintation of the OLED 16x8 display
 Coord system
 O(0,0)
 +-----------------------> X
 |
 |
 |
 v          ...          h(127,7)  <-- highest coord
 Y

 X is the column, Y is the row (page#)

The 7 segments:
  __
 |__|
 |__|

 */
//---------- HORIZONTAL LINEs (LANDSCAPE orientation) ------------------
#define __LINE_H_1U(X,Y,COUNT) do { \
	int i; \
	if (((Y)>=0) && ((Y)<=MAX_ROW_PAGE) && \
		((X)>=0) && ((X)<=MAX_COL-(COUNT))) { \
		SET_START_END_PAGES((Y),0); \
		SET_START_END_COLS((X),(X)+(COUNT)); \
		for (i=0; i<(COUNT); i++) \
			SSD1306_Write(DATA, line_h_bits[i]); \
	} \
	else \
		pr_debug("LINE_H_1U:--out of range--\n"); \
} while (0)
#define LINE_H_1U(X,Y) __LINE_H_1U((X),(Y),LINE_1U_LEN);

/* 
 * TODO / RELOOK
 * The 1.5U, 2U, etc macros - which draw line lengths of 1.5x, 2x, ... -
 * currently don't do a validity check before issuing the underlying
 * 'fundamental' __LINE_H_1U(); thus, we can get partial success
   (Done for the Vertical lines; need to imitate that..)
 */
#define LINE_H_HALFU(X,Y) __LINE_H_1U((X),(Y),(LINE_1U_LEN/2));
#define LINE_H_2U(X,Y) do { \
	LINE_H_1U((X),(Y)); \
	LINE_H_1U((X+LINE_1U_LEN),(Y)); \
} while (0)
#define LINE_H_1PT5U(X,Y) do { \
	LINE_H_1U((X),(Y)); \
	LINE_H_HALFU((X+LINE_1U_LEN),(Y)); \
} while (0)
#define LINE_H_3U(X,Y) do { \
	LINE_H_1U((X),(Y)); \
	LINE_H_1U((X+LINE_1U_LEN),(Y)); \
	LINE_H_1U((X+2*LINE_1U_LEN),(Y)); \
} while (0)
//---------------------------------------------------------------------

//---------- VERTICAL LINEs (LANDSCAPE orientation) -------------------
#define __LINE_V_1U(X,Y,COUNT) do { \
	int i; \
	if (((Y)>=0) && ((Y)<=MAX_ROW_PAGE-1) && \
		((X)>=0) && ((X)<=MAX_COL)) { \
		SET_START_END_PAGES((Y),0); \
		SET_START_END_COLS((X),(X)+(COUNT)); \
		for (i=0; i<(COUNT); i++) \
			SSD1306_Write(DATA, line_v_bits[i]); \
	} \
	else \
		pr_debug("LINE_V_1U:--out of range--\n"); \
} while (0)
#define LINE_V_1U(X,Y) __LINE_V_1U((X),(Y),LINE_1U_LEN);
//#define LINE_V_HALFU(X,Y) __LINE_V_1U((X),(Y),(LINE_1U_LEN/2));
#define LINE_V_2U(X,Y) do { \
	if (((Y)+1)<=MAX_ROW_PAGE-1) { \
		LINE_V_1U((X),(Y)); \
		LINE_V_1U((X),(Y+1)); \
	} else \
		pr_debug("LINE_V_2U:--out of range--\n"); \
} while (0)
#define LINE_V_3U(X,Y) do { \
	if (((Y)+2)<=MAX_ROW_PAGE-1) { \
		LINE_V_1U((X),(Y)); \
		LINE_V_1U((X),(Y+1)); \
		LINE_V_1U((X),(Y+2)); \
	} else \
		pr_debug("LINE_V_3U:--out of range--\n"); \
} while (0)
#define LINE_V_4U(X,Y) do { \
	if (((Y)+3)<=MAX_ROW_PAGE-1) { \
		LINE_V_1U((X),(Y)); \
		LINE_V_1U((X),(Y+1)); \
		LINE_V_1U((X),(Y+2)); \
		LINE_V_1U((X),(Y+3)); \
	} else \
		pr_debug("LINE_V_4U:--out of range--\n"); \
} while (0)
 //pr_info("x=%d y=%d\n", (X), (Y));
//---------------------------------------------------------------------

//---------- SLASH "/" 45 degree LINEs (LANDSCAPE orientation) ------------------
#define __LINE_SLASH_1U(X,Y,COUNT) do { \
	int i; \
	if (((Y)>=0) && ((Y)<=MAX_ROW_PAGE) && \
		((X)>=0) && ((X)<=MAX_COL-(COUNT))) { \
		SET_START_END_PAGES((Y),0); \
		SET_START_END_COLS((X),(X)+(COUNT)); \
		for (i=0; i<(COUNT)/2; i++) \
			SSD1306_Write(DATA, line_slash_bits[i]); \
	} \
	else \
		pr_debug("LINE_SLASH_1U:--out of range--\n"); \
} while (0)
#define LINE_SLASH_1U(X,Y) __LINE_SLASH_1U((X),(Y),LINE_1U_LEN);

#define LINE_SLASH_2U(X,Y) do { \
	if (((Y)+2)<=MAX_ROW_PAGE-1) { \
		LINE_SLASH_1U((X),(Y)); \
		LINE_SLASH_1U((X)+LINE_1U_LEN/2,(Y-1)); \
	} else \
		pr_debug("LINE_SLASH_2U:--out of range--\n"); \
} while (0)

//---------------------------------------------------------------------
/*
 * (weird) TIPS:
 * - Often we seem to need to offset a horizontal line by about 5 pixels
 *   to make it look good...
 * - draw the verticals first then the horizontals (?)
 * - The bitmap needs to be rotated anti-clockwise 90 degrees to correctly
 *   appear on the OLED when in landscape mode! -such that the letter's bottom
 *   is facing to the right!
 * <In the bitmap app, after drawing it, press lower-left (SW) arrow followed
 *  by the upper-left (NW) arrow to do the xform>
 */
#define DIGIT_0(X,Y) do { \
	LINE_V_4U((X),(Y)); \
	LINE_H_HALFU((X)+5,(Y)); \
	LINE_V_4U((X)+LINE_1U_LEN/2,(Y)); \
	LINE_H_HALFU((X),(Y)+4); \
} while (0)
#define DIGIT_1(X,Y) do { \
	LINE_V_4U((X),(Y)); \
} while (0)
#define DIGIT_2(X,Y) do { \
	LINE_H_HALFU((X),(Y)); \
	LINE_V_2U((X)+LINE_1U_LEN/2,(Y)); \
	LINE_V_2U((X),(Y)+2); \
	LINE_H_HALFU((X),(Y)+4); \
	LINE_H_HALFU((X)+5,(Y)+2); \
} while (0)
#define DIGIT_3(X,Y) do { \
	LINE_H_HALFU((X),(Y)); \
	LINE_V_2U((X)+LINE_1U_LEN/2,(Y)); \
	LINE_H_HALFU((X)+5,(Y)+2); \
	LINE_V_2U((X)+15,(Y)+2); \
	LINE_H_HALFU((X),(Y)+4); \
} while (0)
/* 4; origin is the upper tip of the 4
 * BUT- the slash portion's far too wide ??
 * So made it like this instead:
 *  |
 *  |__|
 *     |
 */
#define DIGIT_4(X,Y) do { \
	LINE_V_3U((X),(Y)); \
	LINE_H_HALFU((X)+2,(Y)+3); \
	LINE_V_2U((X)+LINE_1U_LEN/2,(Y)+2); \
} while (0)
/*
#define DIGIT_4(X,Y) do { \
	LINE_V_4U((X)+(LINE_1U_LEN/2)+3,(Y)); \
	LINE_SLASH_2U((X)-(LINE_1U_LEN/2)+3,(Y)+1); \
	LINE_H_1U((X)-(LINE_1U_LEN/2)+3,(Y)+2); \
} while (0) */

#define DIGIT_5(X,Y) do { \
	LINE_V_2U((X),(Y)); \
	LINE_V_2U((X)+LINE_1U_LEN/2-1,(Y)+2); \
	LINE_H_HALFU((X)+1,(Y)); \
	LINE_H_HALFU((X)+2,(Y)+2); \
	LINE_H_HALFU((X)+2,(Y)+4); \
} while (0)
#define DIGIT_6(X,Y) do { \
	LINE_V_4U((X),(Y)); \
	LINE_V_2U((X)+LINE_1U_LEN/2-1,(Y)+2); \
	LINE_H_HALFU((X)+1,(Y)); \
	LINE_H_HALFU((X)+2,(Y)+2); \
	LINE_H_HALFU((X)+2,(Y)+4); \
} while (0)
#define DIGIT_7(X,Y) do { \
	LINE_H_HALFU((X),(Y)); \
	LINE_V_4U((X)+LINE_1U_LEN/2,(Y)); \
} while (0)
#define DIGIT_8(X,Y) do { \
	LINE_V_4U((X),(Y)); \
	LINE_V_2U((X)+LINE_1U_LEN/2-1,(Y)); \
	LINE_V_2U((X)+LINE_1U_LEN/2-1,(Y)+2); \
	LINE_H_HALFU((X)+2,(Y)); \
	LINE_H_HALFU((X)+2,(Y)+2); \
	LINE_H_HALFU((X)+2,(Y)+4); \
} while (0)
#define DIGIT_9(X,Y) do { \
	LINE_V_2U((X),(Y)); \
	LINE_H_HALFU((X)+1,(Y)); \
	LINE_H_HALFU((X),(Y)+2); \
	LINE_V_4U((X)+LINE_1U_LEN/2,(Y)); \
} while (0)


#define PERIOD(X,Y) do { \
	LINE_V_1U((X),(Y)); \
	LINE_V_1U((X)+5,(Y)); \
} while (0)

#define LETTER_C(X,Y) do { \
	LINE_V_4U((X),(Y)); \
	LINE_H_HALFU((X)+3,(Y)); \
	LINE_H_HALFU((X)+3,(Y)+4); \
} while (0)
#define LETTER_H(X,Y) do { \
	LINE_V_4U((X),(Y)); \
	LINE_V_4U((X)+LINE_1U_LEN/2,(Y)); \
	LINE_H_HALFU((X)+3,(Y)+2); \
} while (0)

//---------------------------------------------------------------------
//------------ Small - standard 8x8 bitmap - size 'font' --------------
/* The 'render_smallfont[] array is arranged such that the index is the char
 * to render!
 *
 ascii(1)
 ...
 Dec Hex  Dec Hex  Dec Hex
 48 30 0  64 40 @  80 50 P
 49 31 1  65 41 A  81 51 Q
 50 32 2  66 42 B  82 52 R
 51 33 3  67 43 C  83 53 S
 52 34 4  68 44 D  84 54 T
 53 35 5  69 45 E  85 55 U
 54 36 6  70 46 F  86 56 V
 55 37 7  71 47 G  87 57 W
 56 38 8  72 48 H  88 58 X
 57 39 9  73 49 I  89 59 Y
 58 3A :  74 4A J  90 5A Z
 59 3B ;  75 4B K  91 5B [
 60 3C <  76 4C L  92 5C \
 61 3D =  77 4D M  93 5D ]
 62 3E >  78 4E N  94 5E ^
 63 3F ?  79 4F O  95 5F _
...

 Remember, with the X bitmap app: rotate the img as described below..
 <In the bitmap app, after drawing it, press lower-left (SW) arrow followed
  by the upper-left (NW) arrow to do the xform>
 */
static u8 render_smallfont[91][8] = {
	// element #s 0 to 31 follow... they're currently ignored
	{0x0},{0x0},{0x0},{0x0},{0x0},{0x0},{0x0},
	{0x0},{0x0},{0x0},{0x0},{0x0},{0x0},{0x0},
	{0x0},{0x0},{0x0},{0x0},{0x0},{0x0},{0x0},
	{0x0},{0x0},{0x0},{0x0},{0x0},{0x0},{0x0},
	{0x0},{0x0},{0x0},{0x0},			// idx 31
	//----- ASCII 32 is ' ' whitespace
	{0x00,0x00,0x00,0x00,0x00,0x00,0x00},		// ' ' ; idx 32
	// element #s 33 to 47 follow... they're currently ignored
	// element #s 33 to 36 follow... they're currently ignored
	{0x0},{0x0},{0x0},{0x0},
	//----- ASCII 37 is '%'
	{0x00,0x44,0x24,0x10,0x08,0x24,0x22,0x00},	// '%' ; idx 37
	// element #s 38 to 47 follow... they're currently ignored
	{0x0},{0x0},
	{0x0},{0x0},{0x0},{0x0},{0x0},{0x0},{0x0},
	{0x0},						// idx 47
	/*------ASCII 48 is 0 ------------------------------*/
	// element #s 0 to 9 follow... they're the digits 0 to 9
	{0x00, 0x7f, 0x41, 0x41, 0x41, 0x7f, 0x00},	// 0 ; idx 48
	{0x00, 0x44, 0x42, 0x7f, 0x40, 0x40, 0x00},	// 1
	{0x00, 0x79, 0x49, 0x49, 0x49, 0x4f, 0x00},	// 2 ; idx 50
	{0x00, 0x49, 0x49, 0x49, 0x49, 0x7f, 0x00},	// 3
	{0x00, 0x0f, 0x08, 0x08, 0x08, 0x7f, 0x00},	// 4
	{0x00, 0x4f, 0x49, 0x49, 0x49, 0x79, 0x00},	// 5
	{0x00, 0x7f, 0x49, 0x49, 0x49, 0x79, 0x00},	// 6
	{0x00, 0x01, 0x01, 0x01, 0x01, 0x7f, 0x00},	// 7 ; idx 55
	{0x00, 0x7f, 0x49, 0x49, 0x49, 0x7f, 0x00},	// 8
	{0x00, 0x4f, 0x49, 0x49, 0x49, 0x7f, 0x00},	// 9
	/*------ASCII 58 is : -------------------------------
	 * We're currently ignoring ASCII 58 to 64 inclusive... that's 7 chars */
	// element #s 10 to 16 follow... they're currently ignored
	{0x0},{0x0},{0x0},{0x0},{0x0},{0x0},{0x0},
	// element #s 17 to __ follow... they're the capital letters A to Z
	{0xc0, 0x38, 0x16, 0x11, 0x11, 0x16, 0x38, 0xc0}, // A ; idx 65
	{0x00, 0xff, 0x99, 0x99, 0x5a, 0x3c, 0x00, 0x00}, // B
	{0x18, 0x66, 0x81, 0x81, 0x81, 0x00, 0x00, 0x00}, // C
	{0x00, 0xff, 0x81, 0x81, 0x81, 0x42, 0x3c, 0x00}, // D
	{0x00, 0xff, 0x99, 0x99, 0x99, 0x81, 0x81, 0x81}, // E
	{0x00, 0xff, 0x19, 0x19, 0x19, 0x01, 0x01, 0x01}, // F ; idx 70
	{0x00, 0x7e, 0x42, 0x81, 0x91, 0x91, 0xf2, 0x00}, // G
	{0x00, 0xff, 0x18, 0x18, 0x18, 0x18, 0xff, 0x00}, // H
	{0x00, 0xc3, 0xc3, 0xff, 0xff, 0xc3, 0xc3, 0x00}, // I
	{0xe0, 0xc0, 0xc1, 0xc1, 0x7f, 0x3f, 0x01, 0x01}, // J
	{0x00, 0xff, 0xff, 0x08, 0x14, 0x22, 0x41, 0x80}, // K ; idx 75
	{0x00, 0xff, 0x80, 0x80, 0x80, 0x80, 0x80, 0x00}, // L
	{0x00, 0xff, 0x04, 0x08, 0x04, 0x02, 0xff, 0x00}, // M
	{0x00, 0xff, 0x06, 0x08, 0x10, 0x60, 0xff, 0x00}, // N
	{0x00, 0x7e, 0x81, 0x81, 0x81, 0x81, 0x7e, 0x00}, // O
	{0x00, 0xff, 0x11, 0x11, 0x09, 0x06, 0x00, 0x00}, // P ; idx 80
	{0x00, 0x1c, 0x22, 0x41, 0x41, 0x22, 0x5c, 0x80}, // Q
	{0x00, 0xff, 0x11, 0x11, 0x31, 0x6e, 0xc0, 0x80}, // R
	{0x00, 0x0c, 0x92, 0x91, 0x91, 0x91, 0x61, 0x00}, // S
	{0x00, 0x01, 0x01, 0x01, 0xff, 0x01, 0x01, 0x01}, // T
	{0x00, 0x3f, 0x40, 0x80, 0x80, 0x80, 0x3f, 0x00}, // U ; idx 85
	{0x00, 0x0e, 0x30, 0xc0, 0xc0, 0x30, 0x0e, 0x00}, // V
	{0x00, 0x7f, 0x40, 0x40, 0x38, 0x40, 0x7f, 0x00}, // W
	{0x81, 0x42, 0x24, 0x18, 0x18, 0x24, 0x42, 0x81}, // X
	{0x02, 0x04, 0x08, 0xf0, 0xf0, 0x08, 0x04, 0x02}, // Y
	{0x81, 0xc1, 0xa1, 0x91, 0x89, 0x85, 0x83, 0x81}, // Z ; idx 90
};

/*
 * Positioning:
 * Lets keep 'small' (default) size letters in only the top 2 rows - 0 & 1 -
 * and the bottom row 7. This is as the temperature/humidity large font display
 * takes up the other rows (3 to 6)..
 * It advances to the next col pos automatically by default!
 * so no need to specify every x,y; just the starting one is specified
 * when drawing small font text..
 */
#define START_POS_SMALL_LETTERS(X,Y) do { \
		SET_START_END_COLS((X),MAX_COL); \
		SET_START_END_PAGES((Y),0); \
} while (0)
/*	if ((Y)>=0 && ((Y)<=2 || (Y) == MAX_ROW_PAGE) && \
		((X)>=0) && ((X)<=MAX_COL-8)) { \
	else \
		pr_debug("START_POS_SMALL_LETTERS:--out of range--\n"); \
*/

#define RENDER_SMALLFONT(n) do { \
	int i; \
	for (i = 0; i < 7; i++) \
		SSD1306_Write(DATA, render_smallfont[n][i]); \
} while (0)

#define LETTER_A_SMALLFONT RENDER_SMALLFONT(0xa)
#define LETTER_C_SMALLFONT RENDER_SMALLFONT(0xc)


#define PERCENT_SMALLFONT(X,Y) do { \
	int i; \
	SET_START_END_PAGES((Y),0); \
	SET_START_END_COLS((X),(X)); \
	for (i = 0; i < 7; i++) \
		SSD1306_Write(DATA, percent_bits[i]); \
} while (0)


//---------------------------------------------------------------------
/*
 * How it works - TOO (Theory Of Operation):
 * (Do read the doc/rendering_chars.pdf - it makes it v clear!)
 *
 * Take the digit '0'; the render[0][0] represents it's bit pattern:
	{0x00, 0x7f, 0x41, 0x41, 0x41, 0x7f, 0x00},	// 0
 * Each byte data is written vertically into a 'page'; there are 8 pages,
 * page 0 to page 7. So 0x00 will go into page 0, 0x7f into page 1, 0x41
 * to page 2, and so on...
 * There are 128 columns, COL0 to COL127; each holds a bit. So 8 bits being
 * a byte, we can have upto 128/8 = 16 characters per row. So the effective
 * display resolution becomes 16x8 chars.
 *
 * Seen clearly here:
 * $ ls -l /sys/bus/i2c/devices/1-003c/
 * total 0
 * -rw-r--r-- 1 root root 4096 Oct 21 15:10 col_end
 * -rw-r--r-- 1 root root 4096 Oct 21 15:10 col_start
 * lrwxrwxrwx 1 root root    0 Oct 21 15:10 driver -> ../../../../../../bus/i2c/drivers/oled_ssd1306/
 * -r--r--r-- 1 root root 4096 Oct 21 15:10 modalias
 * -r--r--r-- 1 root root 4096 Oct 21 15:10 name
 * drwxr-xr-x 2 root root    0 Oct 21 15:10 power/
 * -rw-r--r-- 1 root root 4096 Oct 21 15:10 row_end
 * -rw-r--r-- 1 root root 4096 Oct 21 15:10 row_start
 * lrwxrwxrwx 1 root root    0 Oct 21 15:10 subsystem -> ../../../../../../bus/i2c/
 * -rw-r--r-- 1 root root 4096 Oct 21 15:10 uevent
 * --w------- 1 root root 4096 Oct 21 15:10 writechar
 * $ cat /sys/bus/i2c/devices/1-003c/col_start ; echo -n " ; " ; cat /sys/bus/i2c/devices/1-003c/col_end
 * 0 ; 127$  ### <---  COLs is 0 to 127
 * $ cat /sys/bus/i2c/devices/1-003c/row_start ; echo -n " ; " ; cat /sys/bus/i2c/devices/1-003c/row_end
 * 0 ; 7$    ### <---  ROWs is 0 to 7

 Page 0 to Page 7 vertically..
 - page 0 to 2, 3 rows : yellow color
 - page 3 to 7, 5 rows : blue color

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
