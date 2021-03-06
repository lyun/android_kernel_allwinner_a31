/*
 * A V4L2 driver for s5k4e1_mipi cameras.
 *
 */
#include <linux/init.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/i2c.h>
#include <linux/delay.h>
#include <linux/videodev2.h>
#include <linux/clk.h>
#include <media/v4l2-device.h>
#include <media/v4l2-chip-ident.h>
#include <media/v4l2-mediabus.h>
#include <linux/io.h>


#include "camera.h"


MODULE_AUTHOR("raymonxiu");
MODULE_DESCRIPTION("A low-level driver for s5k4e1_mipi sensors");
MODULE_LICENSE("GPL");

//for internel driver debug
#define DEV_DBG_EN      0
#if(DEV_DBG_EN == 1)    
#define vfe_dev_dbg(x,arg...) printk("[s5k4e1_mipi]"x,##arg)
#else
#define vfe_dev_dbg(x,arg...) 
#endif
#define vfe_dev_err(x,arg...) printk("[s5k4e1_mipi]"x,##arg)
#define vfe_dev_print(x,arg...) printk("[s5k4e1_mipi]"x,##arg)

#define LOG_ERR_RET(x)  { \
                          int ret;  \
                          ret = x; \
                          if(ret < 0) {\
                            vfe_dev_err("error at %s\n",__func__);  \
                            return ret; \
                          } \
                        }

//define module timing
#define MCLK              (24*1000*1000)
#define VREF_POL          V4L2_MBUS_VSYNC_ACTIVE_HIGH
#define HREF_POL          V4L2_MBUS_HSYNC_ACTIVE_HIGH
#define CLK_POL           V4L2_MBUS_PCLK_SAMPLE_FALLING
#define V4L2_IDENT_SENSOR 0x4e10

//define the voltage level of control signal
#define CSI_STBY_ON     0
#define CSI_STBY_OFF    1
#define CSI_RST_ON      0
#define CSI_RST_OFF     1
#define CSI_PWR_ON      1
#define CSI_PWR_OFF     0
#define CSI_AF_PWR_ON   1
#define CSI_AF_PWR_OFF  0

//modified for each device for i2c access format
#define regval_list 		reg_list_a16_d8

#define REG_TERM 0xfffe
#define VAL_TERM 0xfe
#define REG_DLY  0xffff

/*
 * Our nominal (default) frame rate.
 */
#ifdef FPGA
#define SENSOR_FRAME_RATE 15
#else
#define SENSOR_FRAME_RATE 30
#endif

/*
 * The s5k4e1_mipi sits on i2c with ID 0x20
 */
#define I2C_ADDR (0x10<<1)

static struct v4l2_subdev *glb_sd;

/*
 * Information we maintain about a known sensor.
 */
struct sensor_format_struct;  /* coming later */

struct cfg_array { /* coming later */
	struct regval_list * regs;
	int size;
};

static inline struct sensor_info *to_state(struct v4l2_subdev *sd)
{
  return container_of(sd, struct sensor_info, sd);
}


/*
 * The default register settings
 *
 */


static struct regval_list sensor_default_regs[] = {
// Reset for operation ...
{0x0100,0x00},// stream off
{0x0103,0x01},// reset
{0x3030,0x06},//bit3=0 10bit raw
// Integration setting ... 
{0x0201,0x03},//fine integration time fixed
{0x0202,0x00},
{0x0202,0x07},//coarse integration time
{0x0203,0xA8},	                              
{0x0204,0x00},//analog gain[msb] 0100 x8 0080 x4
{0x0205,0x80},//analog gain[lsb] 0040 x2 0020 x1

//-->The below registers are for FACTORY ONLY. If you change them without prior notification,
//YOU are RESPONSIBLE for the FAILURE that will happen in the future.
//+++++++++++++++++++++++++++++++//                            
//Factory only set START
// Analog Setting
{0x3000,0x05},
{0x3001,0x03},
{0x3002,0x08},
{0x3003,0x09},
{0x3004,0x2E},
{0x3005,0x06},
{0x3006,0x34},
{0x3007,0x00},
{0x3008,0x3C},
{0x3009,0x3C},
{0x300A,0x28},
{0x300B,0x04},
{0x300C,0x0A},
{0x300D,0x02},
{0x300F,0x82},
{0x3010,0x00},
{0x3011,0x4C},
{0x3012,0x30},
{0x3013,0xC0},
{0x3014,0x00},
{0x3015,0x00},
{0x3016,0x2C},
{0x3017,0x94},
{0x3018,0x78},
{0x301B,0x83},
{0x301D,0xD4},
{0x3021,0x02},
{0x3022,0x24},
{0x3024,0x40},
{0x3027,0x08},
{0x3029,0xC6},
{0x30BC,0xB0},
{0x302B,0x01},
//// Pixel option setting ...
{0x301C,0x04},
{0x30D8,0x3F},
//Factory only set END
//+++++++++++++++++++++++++++++++//
// ADLC setting ...
{0x3070,0x5F},
{0x3071,0x00},
{0x3080,0x04},
{0x3081,0x38},

};

//for capture                                                                         
static struct regval_list sensor_qsxga_regs[] = { //qsxga: 84Mhz_2592x1936_15.0fps
//+++++++++++++++++++++++++++++++//
// Reset for operation ...
{0x0100,0x00},// stream off
{0x3030,0x06},//shut streaming off

//+++++++++++++++++++++++++++++++//
// Parallel setting
{0x3084,0x15},//SYNC Mode
//{0x30BE,0x08},//M_PCLKDIV_AUTO[4], M_DIV_PCLK[3:0]
//{0x30E2,0x01},//num lanes[1:0] = 1

//+++++++++++++++++++++++++++++++//
{0x3110,0x00},//PCLK INV[4]
{0x3117,0x06},//PCLK Delay(default=00)
{0x3119,0x0A},//VSYNC[3:2], HSYNC[1:0] strength
{0x311A,0xAA},//DATA[7:6], PCLK[5:4], Strength[3:0]

//+++++++++++++++++++++++++++++++//
// Frame Length
{0x0340,0x07},//2000
{0x0341,0xd0},//
// Line Length
{0x0342,0x0A},//2800
{0x0343,0xf0},

//+++++++++++++++++++++++++++++++//
// PLL setting ...
//// input clock 24MHz
////// Parallel (TST = 0100b), 15 fps
{0x0305,0x06},//PLL P = 6
{0x0306,0x00},//PLL M[8] = 0
{0x0307,0x69},//PLL M = 168/0xa8
{0x30B5,0x01},//PLL S = 0 

//+++++++++++++++++++++++++++++++//
// Size Setting
// 2608 x 1960
{0x30A9,0x03},//Horizontal Binning Off
{0x300E,0xE8},//Vertical Binning Off

{0x0380,0x00},//x_even_inc 1
{0x0381,0x01},
{0x0382,0x00},//x_odd_inc 1
{0x0383,0x01},
{0x0384,0x00},//y_even_inc 1
{0x0385,0x01},
{0x0386,0x00},//y_odd_inc 3
{0x0387,0x01},

{0x0344,0x00},//x_addr_start 0
{0x0345,0x00},
{0x0346,0x00},//y_addr_start 0
{0x0347,0x00},
{0x0348,0x0A},//x_addr_end 2607
{0x0349,0x2F},
{0x034A,0x07},//y_addr_end 1959
{0x034B,0xA7},

{0x034C,0x0A},//x_output size
{0x034D,0x30},
{0x034E,0x07},//y_output size
{0x034F,0xA8},
//+++++++++++++++++++++++++++++++//
// MIPI setting
{0x30BD,0x00},//SEL_CCP[0]
{0x30BE,0x05},//M_PCLKDIV_AUTO[4], M_DIV_PCLK[3:0]//0x1a
{0x30C1,0x01},//pack video enable [0]
{0x30EE,0x02},//DPHY enable [1]
{0x3111,0x86},//Embedded data off [5]

{0x30E2,0x02},//num lanes[1:0] = 2
{0x30F1,0x70},//DPHY BANDCTRL 480MHz=48MHz
#define CLOCK_INCONT 0
{0x30e8,0x0f&(~(CLOCK_INCONT<<3))},

{0x30BF,0xAB},//outif_enable[7], data_type[5:0](2Bh = bayer 10bit)
{0x30C0,0x80},//video_offset[7:4] 3260%12
{0x30C8,0x0C},//video_data_length 3260 = 2608 * 1.25
{0x30C9,0xBC},

{0x0100,0x01},// stream ON
};

//static struct regval_list sensor_qxga_regs[] = { //qxga: 2048*1536
//  
//  //{REG_TERM,VAL_TERM},   
//};                                      

//static struct regval_list sensor_uxga_regs[] = { //UXGA: 1600*1200
// 
//  //{REG_TERM,VAL_TERM},
//};

#if 1
static struct regval_list sensor_sxga_regs[] = { //SXGA: 1280*960@30fps //84MHz pclk           
//+++++++++++++++++++++++++++++++//                            
// Reset for operation ...                                     
{0x0100,0x00},//stream off
{0x3030,0x06},//shut streaming off

//+++++++++++++++++++++++++++++++//
// Parallel setting ...
{0x3084,0x15},//Sync Mode
//{0x30BE,0x08},//M_PCLKDIV_AUTO[4],M_DIV_PCLK[3:0]
//{0x30E2,0x01},//1 lane setting

//+++++++++++++++++++++++++++++++//
// CLK/DATA SYNC setting ...
{0x3110,0x00},//PCLK INV[4]
{0x3117,0x06},//PCLK Delay(default=00h)
{0x3119,0x0A},//VSYNC[3:2],HSYNC[1:0] strength
{0x311A,0xAA},//DATA[7:6],PCLK[5:4],Strength[3:0]
  
////+++++++++++++++++++++++++++++++//
//// Integration setting ...
//{0x0202,0x03},//coarse integration time
//{0x0203,0xD4},
//{0x0204,0x00},//analog gain[msb] 0100 x8 0080 x4
//{0x0205,0x80},//analog gain[lsb] 0040 x2 0020 x1
////Frame Length
//{0x0340,0x03},//Capture 07B4(1960[# of row]+12[V-blank])
//{0x0341,0xE0},//Preview 03E0(980[# of row]+12[V-blank])
//// Line Length
//{0x0342,0x0A},//2738
//{0x0343,0xB2},
//  
////+++++++++++++++++++++++++++++++//
//// PLL setting ...
////// input clock 24MHz
//////// Parallel (TST = 0100b), 30 fps
//{0x0305,0x06},//PLL P = 6
//{0x0306,0x00},//PLL M[8] = 0
//{0x0307,0x82},//PLL M = 101
//{0x30B5,0x01},//PLL S = 0
//{0x30E2,0x01},//num lanes[1:0] = 1
  
//+++++++++++++++++++++++++++++++//
//Frame Length
{0x0340,0x03},//1000
{0x0341,0xe8},
// Line Length
{0x0342,0x0a},//2800
{0x0343,0xf0},
  
//+++++++++++++++++++++++++++++++//
// PLL setting ...
//// input clock 24MHz
////// Parallel (TST = 0100b), 30 fps
{0x0305,0x06},//PLL P = 6
{0x0306,0x00},//PLL M[8] = 0
{0x0307,0x69},//PLL M = 168
{0x30B5,0x01},//PLL S = 2^1

//Size Setting ...
// 1280 x 960
{0x30A9,0x02},//0x00//Horizontal Binning On
{0x300E,0xEB},//Vertical Binning On

{0x0380,0x00},//x_even_inc 1
{0x0381,0x01},
{0x0382,0x00},//x_odd_inc 1
{0x0383,0x01},
{0x0384,0x00},//y_even_inc 1
{0x0385,0x01},
{0x0386,0x00},//y_odd_inc 3
{0x0387,0x03},

//2560*1920 binning to 1280*960
{0x0344,0x00},//x_addr_start 24
{0x0345,0x18},
{0x0346,0x00},//y_addr_start 20
{0x0347,0x14},
{0x0348,0x0A},//x_addr_end 2560+24-1
{0x0349,0x17},
{0x034A,0x07},//y_addr_end 1920+20-1
{0x034B,0x93},

{0x034C,0x05},//x_output_size 1280
{0x034D,0x00},
{0x034E,0x03},//y_output_size 960
{0x034F,0xc0},//d4

//+++++++++++++++++++++++++++++++//
// MIPI setting
{0x30BD,0x00},//SEL_CCP[0]
{0x30BE,0x1a},//M_PCLKDIV_AUTO[4], M_DIV_PCLK[3:0]//0x1a
{0x30C1,0x01},//pack video enable [0]
{0x30EE,0x02},//DPHY enable [1]
{0x3111,0x86},//Embedded data off [5]

{0x30E2,0x02},//num lanes[1:0] = 2
{0x30F1,0x70},//DPHY BANDCTRL 480MHz=48MHz
#define CLOCK_INCONT 0
{0x30e8,0x0f&(~(CLOCK_INCONT<<3))},

{0x30BF,0xAB},//outif_enable[7], data_type[5:0](2Bh = bayer 10bit)
{0x30C0,0x40},//video_offset[7:4] 1600%12
{0x30C8,0x06},//video_data_length 1600 = 1280 * 1.25
{0x30C9,0x40},

{0x0100,0x01},//stream ON
};
#else
static struct regval_list sensor_sxga_regs[] = { //SXGA: 1280*960@30fps //84MHz pclk           
//+++++++++++++++++++++++++++++++//                            
// Reset for operation ...                                     
{0x0100,0x00},//stream off
{0x3030,0x06},//shut streaming off

//+++++++++++++++++++++++++++++++//
// Parallel setting ...
{0x3084,0x15},//Sync Mode
//{0x30BE,0x08},//M_PCLKDIV_AUTO[4],M_DIV_PCLK[3:0]
//{0x30E2,0x01},//1 lane setting

//+++++++++++++++++++++++++++++++//
// CLK/DATA SYNC setting ...
{0x3110,0x00},//PCLK INV[4]
{0x3117,0x06},//PCLK Delay(default=00h)
{0x3119,0x0A},//VSYNC[3:2],HSYNC[1:0] strength
{0x311A,0xAA},//DATA[7:6],PCLK[5:4],Strength[3:0]
  
//+++++++++++++++++++++++++++++++//
//Frame Length
{0x0340,0x03},//1000
{0x0341,0xe8},
// Line Length
{0x0342,0x0a},//2800
{0x0343,0xf0},

//{0x0340,0x05},//07//Capture 07B4(1960[# of row]+12[V-blank])
//{0x0341,0xB4},//SXGA 03E0(980[# of row]+12[V-blank])
//// Line Length
//{0x0342,0x0A}, //2738
//{0x0343,0xB2},

//+++++++++++++++++++++++++++++++//
// PLL setting ...
//// input clock 24MHz
////// Parallel (TST = 0100b), 30 fps
{0x0305,0x06},//PLL P = 6
{0x0306,0x00},//PLL M[8] = 0
{0x0307,0x69},//PLL M = 168//0x69
{0x30B5,0x01},//PLL S = 2^1

//Size Setting ...
// 1280 x 960
{0x30A9,0x02},//0x00//Horizontal Binning On
{0x300E,0xEB},//Vertical Binning On

{0x0380,0x00},//x_even_inc 1
{0x0381,0x01},
{0x0382,0x00},//x_odd_inc 1
{0x0383,0x01},
{0x0384,0x00},//y_even_inc 1
{0x0385,0x01},
{0x0386,0x00},//y_odd_inc 3
{0x0387,0x03},

//2608*1960 binning to 1304*980
{0x0344,0x00},//x_addr_start 0
{0x0345,0x00},
{0x0346,0x00},//y_addr_start 0
{0x0347,0x00},
{0x0348,0x0A},//x_addr_end 2607
{0x0349,0x2F},
{0x034A,0x07},//y_addr_end 1959
{0x034B,0xA7},

{0x034C,0x05},//x_output_size 1304
{0x034D,0x18},
{0x034E,0x03},//y_output_size 980
{0x034F,0xd4},//

//+++++++++++++++++++++++++++++++//
// MIPI setting
{0x30BD,0x00},//SEL_CCP[0]
{0x30BE,0x1a},//M_PCLKDIV_AUTO[4], M_DIV_PCLK[3:0]//0x1a
{0x30C1,0x01},//pack video enable [0]
{0x30EE,0x02},//DPHY enable [1]
{0x3111,0x86},//Embedded data off [5]

{0x30E2,0x02},//num lanes[1:0] = 2
{0x30F1,0x70},//DPHY BANDCTRL 480MHz=48MHz
#define CLOCK_INCONT 0
{0x30e8,0x0f&(~(CLOCK_INCONT<<3))},

{0x30BF,0xAB},//outif_enable[7], data_type[5:0](2Bh = bayer 10bit)
{0x30C0,0xa0},//video_offset[7:4] 1630%12
{0x30C8,0x06},//video_data_length 1630 = 1304 * 1.25
{0x30C9,0x5e},

{0x0100,0x01},//stream ON
};

#endif

//static struct regval_list sensor_xga_regs[] = { //XGA: 1024*768
//  
//  //{REG_TERM,VAL_TERM},
//};

//for video
static struct regval_list sensor_1080p_regs[] = { //1080p: 1920*1080@15fps //84MHz pclk
//+++++++++++++++++++++++++++++++//
// Reset for operation ...
{0x0100,0x00},// stream off
{0x3030,0x06},//shut streaming off

//+++++++++++++++++++++++++++++++//
// Parallel setting
{0x3084,0x15},//SYNC Mode
//{0x30BE,0x08},//M_PCLKDIV_AUTO[4], M_DIV_PCLK[3:0]
//{0x30E2,0x01},//num lanes[1:0] = 1

//+++++++++++++++++++++++++++++++//
{0x3110,0x00},//PCLK INV[4]
{0x3117,0x06},//PCLK Delay(default=00)
{0x3119,0x0A},//VSYNC[3:2], HSYNC[1:0] strength
{0x311A,0xAA},//DATA[7:6], PCLK[5:4], Strength[3:0]

//+++++++++++++++++++++++++++++++//
// Frame Length
{0x0340,0x07},//2000
{0x0341,0xd0},//
// Line Length
{0x0342,0x0A},//2800
{0x0343,0xf0},

//+++++++++++++++++++++++++++++++//
// PLL setting ...
//// input clock 24MHz
////// Parallel (TST = 0100b), 15 fps
{0x0305,0x06},//PLL P = 6
{0x0306,0x00},//PLL M[8] = 0
{0x0307,0x69},//PLL M = 168
{0x30B5,0x01},//PLL S = 0 

//+++++++++++++++++++++++++++++++//
// Size Setting
// 1920 x 1080
{0x30A9,0x03},//Horizontal Binning Off
{0x300E,0xE8},//Vertical Binning Off

{0x0380,0x00},//x_even_inc 1
{0x0381,0x01},
{0x0382,0x00},//x_odd_inc 1
{0x0383,0x01},
{0x0384,0x00},//y_even_inc 1
{0x0385,0x01},
{0x0386,0x00},//y_odd_inc 1
{0x0387,0x01},

{0x0344,0x01},//x_addr_start 344
{0x0345,0x58},
{0x0346,0x01},//y_addr_start 440
{0x0347,0xb8},
{0x0348,0x08},//x_addr_end 1920+344-1
{0x0349,0xd7},
{0x034A,0x05},//y_addr_end 1080+440-1
{0x034B,0xef},

{0x034C,0x07},//x_output size
{0x034D,0x80},
{0x034E,0x04},//y_output size
{0x034F,0x38},


//+++++++++++++++++++++++++++++++//
// MIPI setting
{0x30BD,0x00},//SEL_CCP[0]
{0x30BE,0x1a},//M_PCLKDIV_AUTO[4], M_DIV_PCLK[3:0]//0x1a
{0x30C1,0x01},//pack video enable [0]
{0x30EE,0x02},//DPHY enable [1]
{0x3111,0x86},//Embedded data off [5]

{0x30E2,0x02},//num lanes[1:0] = 2
{0x30F1,0x70},//DPHY BANDCTRL 480MHz=48MHz
#define CLOCK_INCONT 0
{0x30e8,0x0f&(~(CLOCK_INCONT<<3))},

{0x30BF,0xAB},//outif_enable[7], data_type[5:0](2Bh = bayer 10bit)
{0x30C0,0x00},//video_offset[7:4] 1%12=6
{0x30C8,0x09},//video_data_length 2400 = 1920 * 1.25
{0x30C9,0x60},

{0x0100,0x01},// stream ON
};

static struct regval_list sensor_720p_regs[] = { //720: 1280*720@30fps //84MHz pclk
//+++++++++++++++++++++++++++++++//                            
// Reset for operation ...                                     
{0x0100,0x00},//stream off
{0x3030,0x06},//shut streaming off

//+++++++++++++++++++++++++++++++//
// Parallel setting ...
{0x3084,0x15},//Sync Mode
//{0x30BE,0x08},//M_PCLKDIV_AUTO[4],M_DIV_PCLK[3:0]
//{0x30E2,0x01},//1 lane setting

//+++++++++++++++++++++++++++++++//
// CLK/DATA SYNC setting ...
{0x3110,0x00},//PCLK INV[4]
{0x3117,0x06},//PCLK Delay(default=00h)
{0x3119,0x0A},//VSYNC[3:2],HSYNC[1:0] strength
{0x311A,0xAA},//DATA[7:6],PCLK[5:4],Strength[3:0]

//+++++++++++++++++++++++++++++++//
//Frame Length
{0x0340,0x03},//1000
{0x0341,0xe8},
// Line Length
{0x0342,0x0a},//2800
{0x0343,0xf0},
  
//+++++++++++++++++++++++++++++++//
// PLL setting ...
//// input clock 24MHz
////// Parallel (TST = 0100b), 30 fps
{0x0305,0x06},//PLL P = 6
{0x0306,0x00},//PLL M[8] = 0
{0x0307,0x69},//PLL M = 168
{0x30B5,0x01},//PLL S = 1

//Size Setting ...
// 1280 x 720
{0x30A9,0x00},//Horizontal Binning On
{0x300E,0xEB},//Vertical Binning On

{0x0380,0x00},//x_even_inc 1
{0x0381,0x01},
{0x0382,0x00},//x_odd_inc 1
{0x0383,0x01},
{0x0384,0x00},//y_even_inc 1
{0x0385,0x01},
{0x0386,0x00},//y_odd_inc 3
{0x0387,0x03},

//2560*1440 to binning
{0x0344,0x00},//x_addr_start 24
{0x0345,0x18},
{0x0346,0x01},//y_addr_start 260
{0x0347,0x04},
{0x0348,0x0A},//x_addr_end 2560+24-1
{0x0349,0x17},
{0x034A,0x06},//y_addr_end 1440+260-1
{0x034B,0xa3},

{0x034C,0x05},//x_output_size 1280
{0x034D,0x00},
{0x034E,0x02},//y_output_size 720
{0x034F,0xd0},//

//+++++++++++++++++++++++++++++++//
// MIPI setting
{0x30BD,0x00},//SEL_CCP[0]
{0x30BE,0x1a},//M_PCLKDIV_AUTO[4], M_DIV_PCLK[3:0]//0x1a
{0x30C1,0x01},//pack video enable [0]
{0x30EE,0x02},//DPHY enable [1]
{0x3111,0x86},//Embedded data off [5]

{0x30E2,0x02},//num lanes[1:0] = 2
{0x30F1,0x70},//DPHY BANDCTRL 480MHz=48MHz
#define CLOCK_INCONT 0
{0x30e8,0x0f&(~(CLOCK_INCONT<<3))},

{0x30BF,0xAB},//outif_enable[7], data_type[5:0](2Bh = bayer 10bit)
{0x30C0,0x40},//video_offset[7:4] 1600%12
{0x30C8,0x06},//video_data_length 1600 = 1280 * 1.25
{0x30C9,0x40},

{0x0100,0x01},//stream ON
};

//static struct regval_list sensor_svga_regs[] = { //SVGA: 800*600
//
//  //{REG_TERM,VAL_TERM},
//};

//static struct regval_list sensor_vga_regs[] = { //VGA:  640*480
//  
//  //{REG_TERM,VAL_TERM},
//};

//misc
static struct regval_list sensor_oe_disable_regs[] = {
//	{0x3002,0x00},
  //{REG_TERM,VAL_TERM},
};

static struct regval_list sensor_oe_enable_regs[] = {
//  {0x3002,0xe4},
  //{REG_TERM,VAL_TERM},
};

/*
 * Here we'll try to encapsulate the changes for just the output
 * video format.
 * 
 */

static struct regval_list sensor_fmt_raw[] = {

  //{REG_TERM,VAL_TERM},
};

/*
 * Low-level register I/O.
 *
 */


/*
 * On most platforms, we'd rather do straight i2c I/O.
 */
//!!!!modified type for each device, be careful of the para type!!!
static int sensor_read(struct v4l2_subdev *sd, unsigned short reg,
    unsigned char *value)
{
	int ret=0;
	int cnt=0;
	
  struct i2c_client *client = v4l2_get_subdevdata(sd);
  ret = cci_read_a16_d8(client,reg,value);
  while(ret!=0&&cnt<2)
  {
  	ret = cci_read_a16_d8(client,reg,value);
  	cnt++;
  }
  if(cnt>0)
  	vfe_dev_dbg("sensor read retry=%d\n",cnt);
  
  return ret;
}

static int sensor_write(struct v4l2_subdev *sd, unsigned short reg,
    unsigned char value)
{
	int ret=0;
	int cnt=0;
	
  struct i2c_client *client = v4l2_get_subdevdata(sd);
  
  ret = cci_write_a16_d8(client,reg,value);
  while(ret!=0&&cnt<2)
  {
  	ret = cci_write_a16_d8(client,reg,value);
  	cnt++;
  }
  if(cnt>0)
  	vfe_dev_dbg("sensor write retry=%d\n",cnt);
  
  return ret;
}

/*
 * Write a list of register settings;
 */
static int sensor_write_array(struct v4l2_subdev *sd, struct regval_list *regs, int array_size)
{
	int i=0;
	
  if(!regs)
  	return 0;
  	//return -EINVAL;
  
  while(i<array_size)
  {
    if(regs->addr == REG_DLY) {
      msleep(regs->data);
    } 
    else {  
    	//printk("write 0x%x=0x%x\n", regs->addr, regs->data);
      LOG_ERR_RET(sensor_write(sd, regs->addr, regs->data))
    }
    i++;
    regs++;
  }
  return 0;
}

/* 
 * Code for dealing with controls.
 * fill with different sensor module
 * different sensor module has different settings here
 * if not support the follow function ,retrun -EINVAL
 */

/* *********************************************begin of ******************************************** */
/*
static int sensor_g_hflip(struct v4l2_subdev *sd, __s32 *value)
{
  struct sensor_info *info = to_state(sd);
  unsigned char rdval;
    
  LOG_ERR_RET(sensor_read(sd, 0x3821, &rdval))
  
  rdval &= (1<<1);
  rdval >>= 1;
    
  *value = rdval;

  info->hflip = *value;
  return 0;
}

static int sensor_s_hflip(struct v4l2_subdev *sd, int value)
{
  struct sensor_info *info = to_state(sd);
  unsigned char rdval;
  
  if(info->hflip == value)
    return 0;
    
  LOG_ERR_RET(sensor_read(sd, 0x3821, &rdval))
  
  switch (value) {
    case 0:
      rdval &= 0xf9;
      break;
    case 1:
      rdval |= 0x06;
      break;
    default:
      return -EINVAL;
  }
  
  LOG_ERR_RET(sensor_write(sd, 0x3821, rdval))
  
  usleep_range(10000,12000);
  info->hflip = value;
  return 0;
}

static int sensor_g_vflip(struct v4l2_subdev *sd, __s32 *value)
{
  struct sensor_info *info = to_state(sd);
  unsigned char rdval;
  
  LOG_ERR_RET(sensor_read(sd, 0x3820, &rdval))
  
  rdval &= (1<<1);  
  *value = rdval;
  rdval >>= 1;
  
  info->vflip = *value;
  return 0;
}

static int sensor_s_vflip(struct v4l2_subdev *sd, int value)
{
  struct sensor_info *info = to_state(sd);
  unsigned char rdval;
  
  if(info->vflip == value)
    return 0;
  
  LOG_ERR_RET(sensor_read(sd, 0x3820, &rdval))

  switch (value) {
    case 0:
      rdval &= 0xf9;
      break;
    case 1:
      rdval |= 0x06;
      break;
    default:
      return -EINVAL;
  }

  LOG_ERR_RET(sensor_write(sd, 0x3820, rdval))
  
  usleep_range(10000,12000);
  info->vflip = value;
  return 0;
}
*/
static int sensor_g_exp(struct v4l2_subdev *sd, __s32 *value)
{
	struct sensor_info *info = to_state(sd);
	
	*value = info->exp;
	vfe_dev_dbg("sensor_get_exposure = %d\n", info->exp);
	return 0;
}

static int sensor_s_exp(struct v4l2_subdev *sd, unsigned int exp_val)
{
	unsigned char explow,exphigh;
	struct sensor_info *info = to_state(sd);

	vfe_dev_dbg("sensor_set_exposure = %d\n", exp_val);
	if(exp_val>0xffffff)
		exp_val=0xfffff0;
	if(exp_val<16)
		exp_val=16;
	
	exp_val=(exp_val+8)>>4;//rounding to 1
	
	vfe_dev_dbg("sensor_set_exposure real= %d\n", exp_val);
  
    exphigh = (unsigned char) ( (0xff00&exp_val)>>8);
    explow  = (unsigned char) ( (0x00ff&exp_val) );
	
	sensor_write(sd, 0x0203, explow);//coarse integration time
	sensor_write(sd, 0x0202, exphigh);	
	
	info->exp = exp_val;
	return 0;
}

static int sensor_g_gain(struct v4l2_subdev *sd, __s32 *value)
{
	struct sensor_info *info = to_state(sd);
	
	*value = info->gain;
	vfe_dev_dbg("sensor_get_gain = %d\n", info->gain);
	return 0;
}

static int sensor_s_gain(struct v4l2_subdev *sd, int gain_val)
{
	struct sensor_info *info = to_state(sd);
	unsigned char gainlow=0;
	unsigned char gainhigh=0;
//	unsigned char anagmax;
	
//	printk("org gain=%d\n",gain_val);
	gain_val=gain_val*2;//shift to 1/32 step
//	printk("re gain=%d\n",gain_val);
	
	gainlow=(unsigned char)(gain_val&0xff);
	gainhigh=(unsigned char)((gain_val>>8)&0xff);
	
	sensor_write(sd, 0x0205, gainlow);
	sensor_write(sd, 0x0204, gainhigh);
//	sensor_write(sd, 0xb7, anagmax);
	
	//printk("s5k4e1_mipi sensor_set_gain = %d, Done!\n", gain_val);
	info->gain = gain_val;
	
	return 0;
}

static int sensor_s_sw_stby(struct v4l2_subdev *sd, int on_off)
{
	int ret;
	unsigned char rdval;
	
	ret=sensor_read(sd, 0x0100, &rdval);
	if(ret!=0)
		return ret;
	
	if(on_off==CSI_STBY_ON)//sw stby on
	{
		ret=sensor_write(sd, 0x0100, rdval&0xfe);
	}
	else//sw stby off
	{
		ret=sensor_write(sd, 0x0100, rdval|0x01);
	}
	return ret;
}

/*
 * Stuff that knows about the sensor.
 */
 
static int sensor_power(struct v4l2_subdev *sd, int on)
{
  struct i2c_client *client = v4l2_get_subdevdata(sd);
  int ret;
  
  //insure that clk_disable() and clk_enable() are called in pair 
  //when calling CSI_SUBDEV_STBY_ON/OFF and CSI_SUBDEV_PWR_ON/OFF
  ret = 0;
  switch(on)
  {
    case CSI_SUBDEV_STBY_ON:
      vfe_dev_dbg("CSI_SUBDEV_STBY_ON!\n");
//      //disable io oe
//      vfe_dev_print("disalbe oe!\n");
//      ret = sensor_write_array(sd, sensor_oe_disable_regs, ARRAY_SIZE(sensor_oe_disable_regs));
//      if(ret < 0)
//        vfe_dev_err("disalbe oe falied!\n");
      //software standby on
      ret = sensor_s_sw_stby(sd, CSI_STBY_ON);
      if(ret < 0)
        vfe_dev_err("soft stby falied!\n");
      usleep_range(10000,12000);
      //make sure that no device can access i2c bus during sensor initial or power down
      //when using i2c_lock_adpater function, the following codes must not access i2c bus before calling i2c_unlock_adapter
      i2c_lock_adapter(client->adapter);
      //standby on io
      vfe_gpio_write(sd,PWDN,CSI_STBY_ON);
      //remember to unlock i2c adapter, so the device can access the i2c bus again
      i2c_unlock_adapter(client->adapter);  
      //inactive mclk after stadby in
      vfe_set_mclk(sd,OFF);
      break;
    case CSI_SUBDEV_STBY_OFF:
      vfe_dev_dbg("CSI_SUBDEV_STBY_OFF!\n");
      //make sure that no device can access i2c bus during sensor initial or power down
      //when using i2c_lock_adpater function, the following codes must not access i2c bus before calling i2c_unlock_adapter
      i2c_lock_adapter(client->adapter);    
      //active mclk before stadby out
      vfe_set_mclk_freq(sd,MCLK);
      vfe_set_mclk(sd,ON);
      usleep_range(10000,12000);
      //standby off io
      vfe_gpio_write(sd,PWDN,CSI_STBY_OFF);
      usleep_range(10000,12000);
      //remember to unlock i2c adapter, so the device can access the i2c bus again
      i2c_unlock_adapter(client->adapter);        
      //software standby
      ret = sensor_s_sw_stby(sd, CSI_STBY_OFF);
      if(ret < 0)
        vfe_dev_err("soft stby off falied!\n");
      usleep_range(10000,12000);
//      vfe_dev_print("enable oe!\n");
//      ret = sensor_write_array(sd, sensor_oe_enable_regs);
//      if(ret < 0)
//        vfe_dev_err("enable oe falied!\n");
      break;
    case CSI_SUBDEV_PWR_ON:
      vfe_dev_dbg("CSI_SUBDEV_PWR_ON!\n");
      //make sure that no device can access i2c bus during sensor initial or power down
      //when using i2c_lock_adpater function, the following codes must not access i2c bus before calling i2c_unlock_adapter
      i2c_lock_adapter(client->adapter);
      //power on reset
      vfe_gpio_set_status(sd,PWDN,1);//set the gpio to output
      vfe_gpio_set_status(sd,RESET,1);//set the gpio to output
      //power down io
      vfe_gpio_write(sd,PWDN,CSI_STBY_ON);
      //reset on io
      vfe_gpio_write(sd,RESET,CSI_RST_ON);
      usleep_range(1000,1200);
      //active mclk before power on
      vfe_set_mclk_freq(sd,MCLK);
      vfe_set_mclk(sd,ON);
      usleep_range(10000,12000);
      //power supply
      vfe_gpio_write(sd,POWER_EN,CSI_PWR_ON);
      vfe_set_pmu_channel(sd,IOVDD,ON);
      vfe_set_pmu_channel(sd,AVDD,ON);
      vfe_set_pmu_channel(sd,DVDD,ON);
      vfe_set_pmu_channel(sd,AFVDD,ON);
      //standby off io
      vfe_gpio_write(sd,PWDN,CSI_STBY_OFF);
      usleep_range(10000,12000);
      //reset after power on
      vfe_gpio_write(sd,RESET,CSI_RST_OFF);
      usleep_range(30000,31000);
      //remember to unlock i2c adapter, so the device can access the i2c bus again
      i2c_unlock_adapter(client->adapter);  
      break;
    case CSI_SUBDEV_PWR_OFF:
      vfe_dev_dbg("CSI_SUBDEV_PWR_OFF!\n");
      //make sure that no device can access i2c bus during sensor initial or power down
      //when using i2c_lock_adpater function, the following codes must not access i2c bus before calling i2c_unlock_adapter
      i2c_lock_adapter(client->adapter);
      //inactive mclk before power off
      vfe_set_mclk(sd,OFF);
      //power supply off
      vfe_gpio_write(sd,POWER_EN,CSI_PWR_OFF);
      vfe_set_pmu_channel(sd,AFVDD,OFF);
      vfe_set_pmu_channel(sd,DVDD,OFF);
      vfe_set_pmu_channel(sd,AVDD,OFF);
      vfe_set_pmu_channel(sd,IOVDD,OFF);  
      //standby and reset io
      usleep_range(10000,12000);
      vfe_gpio_write(sd,POWER_EN,CSI_STBY_OFF);
      vfe_gpio_write(sd,RESET,CSI_RST_ON);
      //set the io to hi-z
      vfe_gpio_set_status(sd,RESET,0);//set the gpio to input
      vfe_gpio_set_status(sd,PWDN,0);//set the gpio to input
      //remember to unlock i2c adapter, so the device can access the i2c bus again
      i2c_unlock_adapter(client->adapter);  
      break;
    default:
      return -EINVAL;
  }   

  return 0;
}
 
static int sensor_reset(struct v4l2_subdev *sd, u32 val)
{
  switch(val)
  {
    case 0:
      vfe_gpio_write(sd,RESET,CSI_RST_OFF);
      usleep_range(10000,12000);
      break;
    case 1:
      vfe_gpio_write(sd,RESET,CSI_RST_ON);
      usleep_range(10000,12000);
      break;
    default:
      return -EINVAL;
  }
    
  return 0;
}

static int sensor_detect(struct v4l2_subdev *sd)
{
  unsigned char rdval;
  
  LOG_ERR_RET(sensor_read(sd, 0x0000, &rdval))
//  vfe_dev_dbg("0x0000=0x%x\n",rdval);
  if(rdval != 0x4e)
    return -ENODEV;
  LOG_ERR_RET(sensor_read(sd, 0x0001, &rdval))
//  vfe_dev_dbg("0x0001=0x%x\n",rdval);
  if(rdval != 0x10)
    return -ENODEV;
 
  return 0;
}

static int sensor_init(struct v4l2_subdev *sd, u32 val)
{
  int ret;
  struct sensor_info *info = to_state(sd);
  
  vfe_dev_dbg("sensor_init\n");
  
  /*Make sure it is a target sensor*/
  ret = sensor_detect(sd);
  if (ret) {
    vfe_dev_err("chip found is not an target chip.\n");
    return ret;
  }
  
  vfe_get_standby_mode(sd,&info->stby_mode);
  
  if((info->stby_mode == HW_STBY || info->stby_mode == SW_STBY) \
      && info->init_first_flag == 0) {
    vfe_dev_print("stby_mode and init_first_flag = 0\n");
    return 0;
  } 
  
  info->focus_status = 0;
  info->low_speed = 0;
  info->width = QSXGA_WIDTH;
  info->height = QSXGA_HEIGHT;
  info->hflip = 0;
  info->vflip = 0;
  info->gain = 0;

  info->tpf.numerator = 1;            
  info->tpf.denominator = 15;    /* 30fps */    
  
  ret = sensor_write_array(sd, sensor_default_regs, ARRAY_SIZE(sensor_default_regs));  
  if(ret < 0) {
    vfe_dev_err("write sensor_default_regs error\n");
    return ret;
  }
  
  if(info->stby_mode == 0)
    info->init_first_flag = 0;
  
  info->preview_first_flag = 1;
  
  return 0;
}

static long sensor_ioctl(struct v4l2_subdev *sd, unsigned int cmd, void *arg)
{
  int ret=0;
  struct sensor_info *info = to_state(sd);
//  vfe_dev_dbg("[]cmd=%d\n",cmd);
//  vfe_dev_dbg("[]arg=%0x\n",arg);
  switch(cmd) {
    case GET_CURRENT_WIN_CFG:
      if(info->current_wins != NULL)
      {
        memcpy( arg,
                info->current_wins,
                sizeof(struct sensor_win_size) );
        ret=0;
      }
      else
      {
        vfe_dev_err("empty wins!\n");
        ret=-1;
      }
      break;
    default:
      return -EINVAL;
  }
  return ret;
}


/*
 * Store information about the video data format. 
 */
static struct sensor_format_struct {
  __u8 *desc;
  //__u32 pixelformat;
  enum v4l2_mbus_pixelcode mbus_code;
  struct regval_list *regs;
  int regs_size;
  int bpp;   /* Bytes per pixel */
}sensor_formats[] = {
	{
		.desc		= "Raw RGB Bayer",
		//.mbus_code	= V4L2_MBUS_FMT_SBGGR10_10X1,
		//.mbus_code	= V4L2_MBUS_FMT_SGBRG10_10X1,
		.mbus_code	= V4L2_MBUS_FMT_SGRBG10_10X1,
		//.mbus_code	= V4L2_MBUS_FMT_SRGGB10_10X1,
		.regs 		= sensor_fmt_raw,
		.regs_size = ARRAY_SIZE(sensor_fmt_raw),
		.bpp		= 1
	},
};
#define N_FMTS ARRAY_SIZE(sensor_formats)

  

/*
 * Then there is the issue of window sizes.  Try to capture the info here.
 */


static struct sensor_win_size sensor_win_sizes[] = {
	  /* qsxga: 2592*1936 */
	  {
      .width      = QSXGA_WIDTH,
      .height     = QSXGA_HEIGHT,
      .hoffset    = (2608-2592)/2,
      .voffset    = (1960-1936)/2,
      .hts        = 2800,//must over 2738, limited by sensor
      .vts        = 2000,
      .pclk       = 84*1000*1000,
      .mipi_bps		= 200*1000*1000,
      .fps_fixed  = 1,
      .bin_factor = 1,
      .intg_min   = 3<<4,
      .intg_max   = (2000-8)<<4,
      .gain_min   = 1<<4,
      .gain_max   = 8<<4,
      .regs       = sensor_qsxga_regs,
      .regs_size  = ARRAY_SIZE(sensor_qsxga_regs),
      .set_size   = NULL,
    },
    
    /* 1080P */
    {
      .width			= HD1080_WIDTH,
      .height 		= HD1080_HEIGHT,
      .hoffset	  = 0,
      .voffset	  = 0,
//      .hoffset    = (2608-1920)/3,
//      .voffset    = (1960-1080)/3,
      .hts        = 2800,//must over 2738, limited by sensor
      .vts        = 2000,
      .pclk       = 84*1000*1000,
      .mipi_bps		= 200*1000*1000,
      .fps_fixed  = 1,
      .bin_factor = 1,
      .intg_min   = 3<<4,
      .intg_max   = (2000-8)<<4,
      .gain_min   = 1<<4,
      .gain_max   = 8<<4,
      .regs       = sensor_1080p_regs,//sensor_qsxga_regs
      .regs_size  = ARRAY_SIZE(sensor_1080p_regs),//sensor_1080p_regs
      .set_size		= NULL,
    },
//    /* 1080P */
//    {
//      .width			= HD1080_WIDTH,
//      .height 		= HD1080_HEIGHT,
//      .hoffset	  = 0,
//      .voffset	  = 0,
//      .hts        = 2800,//must over 2738, limited by sensor
//      .vts        = 2000,
//      .pclk       = 84*1000*1000,
//      .fps_fixed  = 1,
//      .bin_factor = 1,
//      .intg_min   = 3<<4,
//      .intg_max   = (1200-8)<<4,
//      .gain_min   = 1<<4,
//      .gain_max   = 16<<4,
//      .regs       = sensor_1080p_regs,//
//      .regs_size  = ARRAY_SIZE(sensor_1080p_regs),//
//      .set_size		= NULL,
//    },
	/* UXGA */
//	{
//      .width			= UXGA_WIDTH,
//      .height 		= UXGA_HEIGHT,
//      .hoffset	  = 0,
//      .voffset	  = 0,
//      .hts        = 2800,//limited by sensor
//      .vts        = 1000,
//      .pclk       = 84*1000*1000,
//      .fps_fixed  = 1,
//      .bin_factor = 1,
//      .intg_min   = ,
//      .intg_max   = ,
//      .gain_min   = ,
//      .gain_max   = ,
//      .regs			= sensor_uxga_regs,
//      .regs_size	= ARRAY_SIZE(sensor_uxga_regs),
//      .set_size		= NULL,
//	},
  	/* SXGA */
    {
      .width			= SXGA_WIDTH,
      .height 		= SXGA_HEIGHT,
      .hoffset	  = 0,
      .voffset	  = 0,
      .hts        = 2800,//must > 2738, limited by sensor
      .vts        = 1000,
      .pclk       = 84*1000*1000,
      .mipi_bps		= 200*1000*1000,
      .fps_fixed  = 1,
      .bin_factor = 1,
      .intg_min   = 3<<4,
      .intg_max   = (1000-8)<<4,
      .gain_min   = 1<<4,
      .gain_max   = 16<<4,
      .regs		    = sensor_sxga_regs,
      .regs_size	= ARRAY_SIZE(sensor_sxga_regs),
      .set_size		= NULL,
    },
    /* 720p */
    {
      .width      = HD720_WIDTH,
      .height     = HD720_HEIGHT,
      .hoffset    = 0,
      .voffset    = 0,
      .hts        = 2800,//must > 2738, limited by sensor
      .vts        = 1000,
      .pclk       = 84*1000*1000,
      .mipi_bps		= 200*1000*1000,
      .fps_fixed  = 1,
      .bin_factor = 1,
      .intg_min   = 3<<4,
      .intg_max   = (1000-8)<<4,
      .gain_min   = 1<<4,
      .gain_max   = 16<<4,
      .regs			  = sensor_720p_regs,//
      .regs_size	= ARRAY_SIZE(sensor_720p_regs),//
      .set_size		= NULL,
    },
    /* XGA */
//    {
//      .width			= XGA_WIDTH,
//      .height 		= XGA_HEIGHT,
//      .hoffset    = 0,
//      .voffset    = 0,
//      .hts        = 2800,//limited by sensor
//      .vts        = 1000,
//      .pclk       = 84*1000*1000,
//      .fps_fixed  = 1,
//      .bin_factor = 1,
//      .intg_min   = ,
//      .intg_max   = ,
//      .gain_min   = ,
//      .gain_max   = ,
//      .regs			  = sensor_xga_regs,
//      .regs_size	= ARRAY_SIZE(sensor_xga_regs),
//      .set_size		= NULL,
//    },
  /* SVGA */
//    {
//      .width			= SVGA_WIDTH,
//      .height 		= SVGA_HEIGHT,
//      .hoffset	  = 0,
//      .voffset	  = 0,
//      .hts        = 2800,//limited by sensor
//      .vts        = 1000,
//      .pclk       = 84*1000*1000,
//      .fps_fixed  = 1,
//      .bin_factor = 1,
//      .intg_min   = ,
//      .intg_max   = ,
//      .gain_min   = ,
//      .gain_max   = ,
//      .regs       = sensor_svga_regs,
//      .regs_size  = ARRAY_SIZE(sensor_svga_regs),
//      .set_size   = NULL,
//    },
  /* VGA */
//    {
//      .width			= VGA_WIDTH,
//      .height 		= VGA_HEIGHT,
//      .hoffset	  = 0,
//      .voffset	  = 0,
//      .hts        = 2800,//limited by sensor
//      .vts        = 1000,
//      .pclk       = 84*1000*1000,
//      .fps_fixed  = 1,
//      .bin_factor = 1,
//      .intg_min   = ,
//      .intg_max   = ,
//      .gain_min   = ,
//      .gain_max   = ,
//      .regs       = sensor_vga_regs,
//      .regs_size  = ARRAY_SIZE(sensor_vga_regs),
//      .set_size   = NULL,
//    },
};

#define N_WIN_SIZES (ARRAY_SIZE(sensor_win_sizes))

static int sensor_enum_fmt(struct v4l2_subdev *sd, unsigned index,
                 enum v4l2_mbus_pixelcode *code)
{
  if (index >= N_FMTS)
    return -EINVAL;

  *code = sensor_formats[index].mbus_code;
  return 0;
}

static int sensor_enum_size(struct v4l2_subdev *sd,
                            struct v4l2_frmsizeenum *fsize)
{
  if(fsize->index > N_WIN_SIZES-1)
  	return -EINVAL;
  
  fsize->type = V4L2_FRMSIZE_TYPE_DISCRETE;
  fsize->discrete.width = sensor_win_sizes[fsize->index].width;
  fsize->discrete.height = sensor_win_sizes[fsize->index].height;
  
  return 0;
}


static int sensor_try_fmt_internal(struct v4l2_subdev *sd,
    struct v4l2_mbus_framefmt *fmt,
    struct sensor_format_struct **ret_fmt,
    struct sensor_win_size **ret_wsize)
{
  int index;
  struct sensor_win_size *wsize;
  struct sensor_info *info = to_state(sd);

  for (index = 0; index < N_FMTS; index++)
    if (sensor_formats[index].mbus_code == fmt->code)
      break;

  if (index >= N_FMTS) 
    return -EINVAL;
  
  if (ret_fmt != NULL)
    *ret_fmt = sensor_formats + index;
    
  /*
   * Fields: the sensor devices claim to be progressive.
   */
  
  fmt->field = V4L2_FIELD_NONE;
  
  /*
   * Round requested image size down to the nearest
   * we support, but not below the smallest.
   */
  for (wsize = sensor_win_sizes; wsize < sensor_win_sizes + N_WIN_SIZES;
       wsize++)
    if (fmt->width >= wsize->width && fmt->height >= wsize->height)
      break;
    
  if (wsize >= sensor_win_sizes + N_WIN_SIZES)
    wsize--;   /* Take the smallest one */
  if (ret_wsize != NULL)
    *ret_wsize = wsize;
  /*
   * Note the size we'll actually handle.
   */
  fmt->width = wsize->width;
  fmt->height = wsize->height;
  info->current_wins = wsize;
  //pix->bytesperline = pix->width*sensor_formats[index].bpp;
  //pix->sizeimage = pix->height*pix->bytesperline;

  return 0;
}

static int sensor_try_fmt(struct v4l2_subdev *sd, 
             struct v4l2_mbus_framefmt *fmt)
{
  return sensor_try_fmt_internal(sd, fmt, NULL, NULL);
}

static int sensor_g_mbus_config(struct v4l2_subdev *sd,
           struct v4l2_mbus_config *cfg)
{
  cfg->type = V4L2_MBUS_CSI2;
  cfg->flags = 0|V4L2_MBUS_CSI2_2_LANE|V4L2_MBUS_CSI2_CHANNEL_0;
  
  return 0;
}


/*
 * Set a format.
 */
static int sensor_s_fmt(struct v4l2_subdev *sd, 
             struct v4l2_mbus_framefmt *fmt)
{
  int ret;
  struct sensor_format_struct *sensor_fmt;
  struct sensor_win_size *wsize;
  struct sensor_info *info = to_state(sd);
  
  vfe_dev_dbg("sensor_s_fmt\n");
  
  sensor_write_array(sd, sensor_oe_disable_regs, ARRAY_SIZE(sensor_oe_disable_regs));
  
  LOG_ERR_RET(sensor_write_array(sd, sensor_default_regs, ARRAY_SIZE(sensor_default_regs)) );
  
  ret = sensor_try_fmt_internal(sd, fmt, &sensor_fmt, &wsize);
  if (ret)
    return ret;

  if(info->capture_mode == V4L2_MODE_VIDEO)
  {
    //video
  }
  else if(info->capture_mode == V4L2_MODE_IMAGE)
  {
    //image 
    
  }

  sensor_write_array(sd, sensor_fmt->regs, sensor_fmt->regs_size);

  ret = 0;
  if (wsize->regs)
    LOG_ERR_RET(sensor_write_array(sd, wsize->regs, wsize->regs_size))
  
  if (wsize->set_size)
    LOG_ERR_RET(wsize->set_size(sd))

  info->fmt = sensor_fmt;
  info->width = wsize->width;
  info->height = wsize->height;

  vfe_dev_print("s_fmt set width = %d, height = %d\n",wsize->width,wsize->height);

  if(info->capture_mode == V4L2_MODE_VIDEO)
  {
    //video
   
  } else {
    //capture image

  }
	
	sensor_write_array(sd, sensor_oe_enable_regs, ARRAY_SIZE(sensor_oe_enable_regs));
	
	return 0;
}

/*
 * Implement G/S_PARM.  There is a "high quality" mode we could try
 * to do someday; for now, we just do the frame rate tweak.
 */
static int sensor_g_parm(struct v4l2_subdev *sd, struct v4l2_streamparm *parms)
{
  struct v4l2_captureparm *cp = &parms->parm.capture;
  struct sensor_info *info = to_state(sd);

  if (parms->type != V4L2_BUF_TYPE_VIDEO_CAPTURE)
    return -EINVAL;
  
  memset(cp, 0, sizeof(struct v4l2_captureparm));
  cp->capability = V4L2_CAP_TIMEPERFRAME;
  cp->capturemode = info->capture_mode;
     
  return 0;
}

static int sensor_s_parm(struct v4l2_subdev *sd, struct v4l2_streamparm *parms)
{
  struct v4l2_captureparm *cp = &parms->parm.capture;
  //struct v4l2_fract *tpf = &cp->timeperframe;
  struct sensor_info *info = to_state(sd);
  //unsigned char div;
  
  vfe_dev_dbg("sensor_s_parm\n");
  
  if (parms->type != V4L2_BUF_TYPE_VIDEO_CAPTURE)
    return -EINVAL;
  
  if (info->tpf.numerator == 0)
    return -EINVAL;
    
  info->capture_mode = cp->capturemode;
  
  return 0;
}


static int sensor_queryctrl(struct v4l2_subdev *sd,
    struct v4l2_queryctrl *qc)
{
  /* Fill in min, max, step and default value for these controls. */
  /* see include/linux/videodev2.h for details */
  
  switch (qc->id) {
	case V4L2_CID_GAIN:
		return v4l2_ctrl_query_fill(qc, 1*16, 16*16, 1, 16);
	case V4L2_CID_EXPOSURE:
		return v4l2_ctrl_query_fill(qc, 3*16, 65535*16, 1, 3*16);
  }
  return -EINVAL;
}

static int sensor_g_ctrl(struct v4l2_subdev *sd, struct v4l2_control *ctrl)
{
  switch (ctrl->id) {
  case V4L2_CID_GAIN:
    return sensor_g_gain(sd, &ctrl->value);
  case V4L2_CID_EXPOSURE:
  	return sensor_g_exp(sd, &ctrl->value);
  }
  return -EINVAL;
}

static int sensor_s_ctrl(struct v4l2_subdev *sd, struct v4l2_control *ctrl)
{
  struct v4l2_queryctrl qc;
  int ret;
  
  qc.id = ctrl->id;
  ret = sensor_queryctrl(sd, &qc);
  if (ret < 0) {
    return ret;
  }

  if (ctrl->value < qc.minimum || ctrl->value > qc.maximum) {
    return -ERANGE;
  }
  
  switch (ctrl->id) {
    case V4L2_CID_GAIN:
      return sensor_s_gain(sd, ctrl->value);
    case V4L2_CID_EXPOSURE:
	  return sensor_s_exp(sd, ctrl->value);
  }
  return -EINVAL;
}


static int sensor_g_chip_ident(struct v4l2_subdev *sd,
    struct v4l2_dbg_chip_ident *chip)
{
  struct i2c_client *client = v4l2_get_subdevdata(sd);

  return v4l2_chip_ident_i2c_client(client, chip, V4L2_IDENT_SENSOR, 0);
}


/* ----------------------------------------------------------------------- */

static const struct v4l2_subdev_core_ops sensor_core_ops = {
  .g_chip_ident = sensor_g_chip_ident,
  .g_ctrl = sensor_g_ctrl,
  .s_ctrl = sensor_s_ctrl,
  .queryctrl = sensor_queryctrl,
  .reset = sensor_reset,
  .init = sensor_init,
  .s_power = sensor_power,
  .ioctl = sensor_ioctl,
};

static const struct v4l2_subdev_video_ops sensor_video_ops = {
  .enum_mbus_fmt = sensor_enum_fmt,
  .enum_framesizes = sensor_enum_size,
  .try_mbus_fmt = sensor_try_fmt,
  .s_mbus_fmt = sensor_s_fmt,
  .s_parm = sensor_s_parm,
  .g_parm = sensor_g_parm,
  .g_mbus_config = sensor_g_mbus_config,
};

static const struct v4l2_subdev_ops sensor_ops = {
  .core = &sensor_core_ops,
  .video = &sensor_video_ops,
};

/* ----------------------------------------------------------------------- */

static int sensor_probe(struct i2c_client *client,
      const struct i2c_device_id *id)
{
  struct v4l2_subdev *sd;
  struct sensor_info *info;
//  int ret;

  info = kzalloc(sizeof(struct sensor_info), GFP_KERNEL);
  if (info == NULL)
    return -ENOMEM;
  sd = &info->sd;
  glb_sd = sd;
  v4l2_i2c_subdev_init(sd, client, &sensor_ops);

  info->fmt = &sensor_formats[0];
  info->af_first_flag = 1;
  info->init_first_flag = 1;

  return 0;
}


static int sensor_remove(struct i2c_client *client)
{
  struct v4l2_subdev *sd = i2c_get_clientdata(client);

  v4l2_device_unregister_subdev(sd);
  kfree(to_state(sd));
  return 0;
}

static const struct i2c_device_id sensor_id[] = {
  { "s5k4e1_mipi", 0 },
  { }
};
MODULE_DEVICE_TABLE(i2c, sensor_id);


static struct i2c_driver sensor_driver = {
  .driver = {
    .owner = THIS_MODULE,
  .name = "s5k4e1_mipi",
  },
  .probe = sensor_probe,
  .remove = sensor_remove,
  .id_table = sensor_id,
};
static __init int init_sensor(void)
{
  return i2c_add_driver(&sensor_driver);
}

static __exit void exit_sensor(void)
{
  i2c_del_driver(&sensor_driver);
}

module_init(init_sensor);
module_exit(exit_sensor);

