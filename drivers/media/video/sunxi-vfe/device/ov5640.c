/*
 * A V4L2 driver for ov5640 cameras.
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
MODULE_DESCRIPTION("A low-level driver for ov5640 sensors");
MODULE_LICENSE("GPL");

#define AF_WIN_NEW_COORD
//for internel driver debug
#define DEV_DBG_EN      0 
#if(DEV_DBG_EN == 1)    
#define vfe_dev_dbg(x,arg...) printk("[OV5640]"x,##arg)
#else
#define vfe_dev_dbg(x,arg...) 
#endif
#define vfe_dev_err(x,arg...) printk("[OV5640]"x,##arg)
#define vfe_dev_print(x,arg...) printk("[OV5640]"x,##arg)

#define CAP_BDG 0
#if(CAP_BDG == 1)
#define vfe_dev_cap_dbg(x,arg...) printk("[OV5640_CAP_DBG]"x,##arg)
#else
#define vfe_dev_cap_dbg(x,arg...)
#endif

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
#define CLK_POL           V4L2_MBUS_PCLK_SAMPLE_RISING
#define V4L2_IDENT_SENSOR 0x5640

//define the voltage level of control signal
#define CSI_STBY_ON     1
#define CSI_STBY_OFF    0
#define CSI_RST_ON      0
#define CSI_RST_OFF     1
#define CSI_PWR_ON      1
#define CSI_PWR_OFF     0
#define CSI_AF_PWR_ON   1
#define CSI_AF_PWR_OFF  0

#define regval_list reg_list_a16_d8

#define REG_TERM 0xfffe
#define VAL_TERM 0xfe
#define REG_DLY  0xffff

#define FLASH_EN_POL 1
#define FLASH_MODE_POL 1

//#define _FLASH_FUNC_

#ifdef _FLASH_FUNC_
#include "../flash_light/flash.h"

//extern int config_flash_mode(struct v4l2_subdev *sd, enum v4l2_flash_led_mode mode,
//                      struct flash_dev_info *fls_info);
//
//extern int io_set_flash_ctrl(struct v4l2_subdev *sd, enum sunxi_flash_ctrl ctrl,
//                      struct flash_dev_info *fls_info);

static struct flash_dev_info fl_info;
static unsigned int to_flash=0;
static unsigned int flash_auto_level=0x1c;
#endif

#define CONTINUEOUS_AF
//#define AUTO_FPS
#define DENOISE_LV_AUTO
#define SHARPNESS 0x18

#ifdef AUTO_FPS
//#define AF_FAST
#endif

#ifndef DENOISE_LV_AUTO
#define DENOISE_LV 0x8
#endif

#define AE_CW 1

unsigned int night_mode=0;
unsigned int Nfrms=1;
unsigned int cap_manual_gain=0x10;
#define CAP_GAIN_CAL 0//0--auto limit frames;1--manual fixed gain
#define CAP_MULTI_FRAMES
#ifdef CAP_MULTI_FRAMES
#define MAX_FRM_CAP 4
#else
#define MAX_FRM_CAP 1
#endif


/*
 * Our nominal (default) frame rate.
 */
#define SENSOR_FRAME_RATE 30

/*
 * The ov5640 sits on i2c with ID 0x78
 */
#define I2C_ADDR 0x78

//static struct delayed_work sensor_s_ae_ratio_work;
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
  {0x3103,0x11},//
  {0x3008,0x82},//reset
  {REG_DLY,0x1e},//delay 30ms
  {0x3008,0x42},//power down
  {0x3103,0x03},//
  {0x3017,0x00},//disable oe
  {0x3018,0x00},//
  //pll and clock setting
  {0x3034,0x18},//
  {0x3035,0x21},//
  {0x3036,0x46},//0x46->30fps
  {0x3037,0x13},//////div
  {0x3108,0x01},//
  {0x3824,0x01},//
  
  {0x3630,0x36},//
  {0x3631,0x0e},//
  {0x3632,0xe2},//
  {0x3633,0x12},//
  {0x3621,0xe0},//
  {0x3704,0xa0},//
  {0x3703,0x5a},//
  {0x3715,0x78},//
  {0x3717,0x01},//
  {0x370b,0x60},//
  {0x3705,0x1a},//
  {0x3905,0x02},//
  {0x3906,0x10},//
  {0x3901,0x0a},//
  {0x3731,0x12},//
  {0x3600,0x08},//
  {0x3601,0x33},//
//  {0x302d,0x60},//
  {0x3620,0x52},//
  {0x371b,0x20},//
  {0x471c,0x50},//
  {0x3a13,0x43},//
  {0x3a18,0x00},//
  {0x3a19,0x88},//
  {0x3635,0x13},//
  {0x3636,0x03},//
  {0x3634,0x40},//
  {0x3622,0x01},//
  {0x3c01,0x34},//
  {0x3c04,0x28},//
  {0x3c05,0x98},//
  {0x3c06,0x00},//
  {0x3c07,0x08},//
  {0x3c08,0x00},//
  {0x3c09,0x1c},//
  {0x3c0a,0x9c},//
  {0x3c0b,0x40},//
//  {0x3820,0x41},// binning
//  {0x3821,0x41},// binning
  {0x3814,0x31},//
  {0x3815,0x31},//
  {0x3800,0x00},//
  {0x3801,0x00},//
  {0x3802,0x00},//
  {0x3803,0x04},//
  {0x3804,0x0a},//
  {0x3805,0x3f},//
  {0x3806,0x07},//
  {0x3807,0x9b},//
  {0x3808,0x02},//
  {0x3809,0x80},//
  {0x380a,0x01},//
  {0x380b,0xe0},//
  {0x380c,0x07},//
  {0x380d,0x68},//
  {0x380e,0x03},//
  {0x380f,0xd8},//
  {0x3810,0x00},//
  {0x3811,0x10},//
  {0x3812,0x00},//
  {0x3813,0x06},//
  {0x3618,0x00},//
  {0x3612,0x29},//
  {0x3708,0x64},//
  {0x3709,0x52},//
  {0x370c,0x03},//
  {0x3a00,0x78},
  {0x3a02,0x03},//
  {0x3a03,0xd8},//
  {0x3a08,0x01},//
  {0x3a09,0x27},//
  {0x3a0a,0x00},//
  {0x3a0b,0xf6},//
  {0x3a0e,0x03},//
  {0x3a0d,0x04},//
  {0x3a14,0x03},//
  {0x3a15,0xd8},//
  {0x4001,0x02},//
  {0x4004,0x02},//
  {0x3000,0x00},//
  {0x3002,0x1c},//
  {0x3004,0xff},//
  {0x3006,0xc3},//
  {0x300e,0x58},//
//  {0x302e,0x00},//
  
  {0x302c,0xc2},//bit[7:6]: output drive capability
            //00: 1x   01: 2x  10: 3x  11: 4x

  {0x4300,0x30},//
  {0x501f,0x00},//
  {0x4713,0x03},//
  {0x4407,0x04},//
  {0x440e,0x00},//
  {0x460b,0x35},//
  {0x460c,0x20},//
  {0x4837,0x22},
  {0x5000,0xa7},//
  {0x5001,0xa3},//
  
  {0x4740,0x21},//hsync,vsync,clock pol,reference to application note,spec is wrong
                  
  //AWB   
	{0x3406,0x00},  //0x00}, // LA      ORG
  {0x5180,0xff},  //0xff},	// 0xff    0xff
	{0x5181,0xf2},  //0x50}, // 0xf2    0x50               	
	{0x5182,0x00},  //0x11}, // 0x00    0x11              	
	{0x5183,0x14},  //0x14}, // 0x14    0x14             	
	{0x5184,0x25},  //0x25}, // 0x25    0x25             	
	{0x5185,0x24},  //0x24}, // 0x24    0x24             	
	{0x5186,0x16},  //0x1c}, // 0x09    0x1c              	
	{0x5187,0x16},  //0x18}, // 0x09    0x18              	
	{0x5188,0x16},  //0x18}, // 0x09    0x18              	
	{0x5189,0x6e},  //0x6e}, // 0x75    0x6e             	
	{0x518a,0x68},  //0x68}, // 0x54    0x68             	
	{0x518b,0xe0},  //0xa8}, // 0xe0    0xa8             	
	{0x518c,0xb2},  //0xa8}, // 0xb2    0xa8             	
	{0x518d,0x42},  //0x3d}, // 0x42    0x3d             	
	{0x518e,0x3e},  //0x3d}, // 0x3d    0x3d             	
	{0x518f,0x4c},  //0x54}, // 0x56    0x54             	
	{0x5190,0x56},  //0x54}, // 0x46    0x54             	
	{0x5191,0xf8},  //0xf8}, // 0xf8    0xf8             	
	{0x5192,0x04},  //0x04}, // 0x04    0x04              	
	{0x5193,0x70},  //0x70}, // 0x70    0x70             	
	{0x5194,0xf0},  //0xf0}, // 0xf0    0xf0             	
	{0x5195,0xf0},  //0xf0}, // 0xf0    0xf0             	
	{0x5196,0x03},  //0x03}, // 0x03    0x03              	
	{0x5197,0x01},  //0x01}, // 0x01    0x01              	
	{0x5198,0x04},  //0x05}, // 0x04    0x05              	
	{0x5199,0x12},  //0x7c}, // 0x12    0x7c
	{0x519a,0x04},  //0x04}, // 0x04    0x04
	{0x519b,0x00},  //0x00}, // 0x00    0x00
	{0x519c,0x06},  //0x06}, // 0x06    0x06
	{0x519d,0x82},  //0x79}, // 0x82    0x79
	{0x519e,0x38},  //0x38}, // 0x38    0x38
  //Color                   // LA      ORG      
	{0x5381,0x1e},  //0x1e}, // 0x1e    0x1e
	{0x5382,0x5b},  //0x5b}, // 0x5b    0x5b
	{0x5383,0x14},  //0x08}, // 0x08    0x08
	{0x5384,0x05},  //0x0a}, // 0x0a    0x05
	{0x5385,0x77},  //0x7e}, // 0x7e    0x72
	{0x5386,0x7c},  //0x88}, // 0x88    0x77
	{0x5387,0x72},  //0x7c}, // 0x7c    0x6d
	{0x5388,0x58},  //0x6c}, // 0x6c    0x4d
	{0x5389,0x1a},  //0x10}, // 0x10    0x20
	{0x538a,0x01},  //0x01}, // 0x01    0x01
	{0x538b,0x98},  //0x98}, // 0x98    0x98

  //Sharpness/Denoise     
  {0x5300,0x08}, 
  {0x5301,0x30},      
  {0x5302,0x30}, 
  {0x5303,0x10}, 
	{0x5308,0x25}, //sharpness/noise auto
  {0x5304,0x08}, 
  {0x5305,0x30}, 
  {0x5306,0x1c}, 
  {0x5307,0x2c}, 
  {0x5309,0x08}, 
  {0x530a,0x30}, 
  {0x530b,0x04}, 
  {0x530c,0x06}, 

	//Gamma        
  {0x5480,0x01},  //???     // LA     ORG
  {0x5481,0x06},  //0x08},  // 0x08     0x06
  {0x5482,0x12},  //0x14},  // 0x14     0x15
  {0x5483,0x1e},  //0x28},  // 0x28     0x28
  {0x5484,0x4a},  //0x51},  // 0x51     0x3b
  {0x5485,0x58},  //0x65},  // 0x65     0x50
  {0x5486,0x65},  //0x71},  // 0x71     0x5d
  {0x5487,0x72},  //0x7d},  // 0x7d     0x6a
  {0x5488,0x7d},  //0x87},  // 0x87     0x75
  {0x5489,0x88},  //0x91},  // 0x91     0x80
  {0x548a,0x92},  //0x9a},  // 0x9a     0x8a
  {0x548b,0xa3},  //0xaa},  // 0xaa     0x9b
  {0x548c,0xb2},  //0xb8},  // 0xb8     0xaa
  {0x548d,0xc8},  //0xcd},  // 0xcd     0xc0
  {0x548e,0xdd},  //0xdd},  // 0xdd     0xd5
  {0x548f,0xf0},  //0xea},  // 0xea     0xe8
  {0x5490,0x15},  //0x1d},  // 0x1d     0x20
    
  //UV  
  {0x5580,0x06}, 
  {0x5583,0x40}, 
  {0x5584,0x10}, 
  {0x5589,0x10}, 
  {0x558a,0x00}, 
  {0x558b,0xf8}, 
  {0x501d,0x40}, 
  
//  {0x5587,0x05},
//  {0x5588,0x09},
  //Lens Shading 
  {0x5800,0x15},    //0xa7}, //LA        org
  {0x5801,0x10},    //0x23}, //0x23      0x17
  {0x5802,0x0D},    //0x14}, //0x14      0x10
  {0x5803,0x0D},    //0x0f}, //0x0f      0x0e
  {0x5804,0x0F},    //0x0f}, //0x0f      0x0e
  {0x5805,0x15},    //0x12}, //0x12      0x11
  {0x5806,0x0A},    //0x26}, //0x26      0x1b
  {0x5807,0x07},    //0x0c}, //0x0c      0x0b
  {0x5808,0x05},    //0x08}, //0x08      0x07
  {0x5809,0x05},    //0x05}, //0x05      0x05
  {0x580A,0x07},    //0x05}, //0x05      0x06
  {0x580B,0x0B},    //0x08}, //0x08      0x09
  {0x580C,0x07},    //0x0d}, //0x0d      0x0e
  {0x580D,0x03},    //0x08}, //0x08      0x06
  {0x580E,0x01},    //0x03}, //0x03      0x02
  {0x580F,0x01},    //0x00}, //0x00      0x00
  {0x5810,0x03},    //0x00}, //0x00      0x00
  {0x5811,0x07},    //0x03}, //0x03      0x03
  {0x5812,0x07},    //0x09}, //0x09      0x09
  {0x5813,0x03},    //0x07}, //0x07      0x06
  {0x5814,0x01},    //0x03}, //0x03      0x03
  {0x5815,0x01},    //0x00}, //0x00      0x00
  {0x5816,0x03},    //0x01}, //0x01      0x00
  {0x5817,0x06},    //0x03}, //0x03      0x03
  {0x5818,0x0D},    //0x08}, //0x08      0x09
  {0x5819,0x08},    //0x0d}, //0x0d      0x0b
  {0x581A,0x06},    //0x08}, //0x08      0x08
  {0x581B,0x06},    //0x05}, //0x05      0x05
  {0x581C,0x07},    //0x06}, //0x06      0x05
  {0x581D,0x0B},    //0x08}, //0x08      0x08
  {0x581E,0x14},    //0x0e}, //0x0e      0x0e
  {0x581F,0x13},    //0x29}, //0x29      0x18
  {0x5820,0x0E},    //0x17}, //0x17      0x12
  {0x5821,0x0E},    //0x11}, //0x11      0x0f
  {0x5822,0x12},    //0x11}, //0x11      0x0f
  {0x5823,0x12},    //0x15}, //0x15      0x12
  {0x5824,0x46},    //0x28}, //0x28      0x1a
  {0x5825,0x26},    //0x46}, //0x46      0x0a
  {0x5826,0x06},    //0x26}, //0x26      0x0a
  {0x5827,0x46},    //0x08}, //0x08      0x0a
  {0x5828,0x44},    //0x26}, //0x26      0x0a
  {0x5829,0x26},    //0x64}, //0x64      0x46
  {0x582A,0x24},    //0x26}, //0x26      0x2a
  {0x582B,0x42},    //0x24}, //0x24      0x24
  {0x582C,0x24},    //0x22}, //0x22      0x44
  {0x582D,0x46},    //0x24}, //0x24      0x24
  {0x582E,0x24},    //0x24}, //0x24      0x28
  {0x582F,0x42},    //0x06}, //0x06      0x08
  {0x5830,0x60},    //0x22}, //0x22      0x42
  {0x5831,0x42},    //0x40}, //0x40      0x40
  {0x5832,0x24},    //0x42}, //0x42      0x42
  {0x5833,0x26},    //0x24}, //0x24      0x28
  {0x5834,0x24},    //0x26}, //0x26      0x0a
  {0x5835,0x24},    //0x24}, //0x24      0x26
  {0x5836,0x24},    //0x22}, //0x22      0x24
  {0x5837,0x46},    //0x22}, //0x22      0x26
  {0x5838,0x44},    //0x26}, //0x26      0x28
  {0x5839,0x46},    //0x44}, //0x44      0x4a
  {0x583A,0x26},    //0x24}, //0x24      0x0a
  {0x583B,0x48},    //0x26}, //0x26      0x0c
  {0x583C,0x44},    //0x28}, //0x28      0x2a
  {0x583D,0xBF},    //0x42}, //0x42      0x28
   
  //EV
  {0x3a0f,0x30}, 
  {0x3a10,0x28}, 
  {0x3a1b,0x30}, 
  {0x3a1e,0x26}, 
  {0x3a11,0x60}, 
  {0x3a1f,0x14}, 

  {0x5025,0x00}, 
  
	{0x3031,0x08}, //disable internal LDO
//  {0x4005,0x1a},
//  //power down release
  {0x3008,0x02}, 
  //{REG_TERM,VAL_TERM},
};                                                           

//for capture                                                                         
static struct regval_list sensor_qsxga_regs[] = { //qsxga: 2592*1936
  //capture 5Mega 7.5fps
  //power down
//  {0x3008,0x42},
  //pll and clock setting
  {0x3820,0x40},
  {0x3821,0x06},
  {0x3034,0x18},                                  
#ifndef FPGA
  {0x3035,0x21},    
#else
  {0x3035,0x41},                         
#endif                              
  {0x3036,0x54},                                                                
  {0x3037,0x13},                                  
  {0x3108,0x01},                                  
  {0x3824,0x01},                                  
  {REG_DLY,0x05},//delay 5ms              
  //timing                                              
  //2592*1936                                           
  {0x3808,0x0a}, //H size MSB                    
  {0x3809,0x20}, //H size LSB                    
  {0x380a,0x07}, //V size MSB                    
  {0x380b,0x90}, //V size LSB                    
  {0x380c,0x0b}, //HTS MSB                       
  {0x380d,0x1c}, //HTS LSB                       
  {0x380e,0x07}, //VTS MSB                       
  {0x380f,0xb0}, //LSB                           
#ifndef FPGA
  //banding step                                        
  {0x3a08,0x00}, //50HZ step MSB                 
  {0x3a09,0x93}, //50HZ step LSB                 
  {0x3a0a,0x00}, //60HZ step MSB                 
  {0x3a0b,0x7b}, //60HZ step LSB                 
  {0x3a0e,0x0d}, //50HZ step max                 
  {0x3a0d,0x10}, //60HZ step max                 
#else
  //banding step                                        
  {0x3a08,0x00}, //50HZ step MSB                 
  {0x3a09,0x49}, //50HZ step LSB                 
  {0x3a0a,0x00}, //60HZ step MSB                 
  {0x3a0b,0x3d}, //60HZ step LSB                 
  {0x3a0e,0x1a}, //50HZ step max                 
  {0x3a0d,0x20}, //60HZ step max 
#endif                                                                                                
  {0x3503,0x07}, //AEC disable                                                                                             
  {0x350c,0x00},                                   
  {0x350d,0x00},         
  {0x3c07,0x07}, //light meter 1 thereshold                                 
                                                                   
  {0x3814,0x11}, //horizton subsample
  {0x3815,0x11}, //vertical subsample
  {0x3800,0x00}, //x address start high byte
  {0x3801,0x00}, //x address start low byte  
  {0x3802,0x00},  //y address start high byte 
  {0x3803,0x00}, //y address start low byte 
  {0x3804,0x0a}, //x address end high byte
  {0x3805,0x3f}, //x address end low byte 
  {0x3806,0x07}, //y address end high byte
  {0x3807,0x9f}, //y address end low byte 
  {0x3810,0x00}, //isp hortizontal offset high byte
  {0x3811,0x10}, //isp hortizontal offset low byte
  {0x3812,0x00}, //isp vertical offset high byte
  {0x3813,0x04},  //isp vertical offset low byte 
  
//  {0x5308,0x65},    //sharpen manual    
//  {0x5302,0x20}, //sharpness      
                                                     
  {0x4002,0xc5},  //BLC related                  
  {0x4005,0x1a}, // BLC related               
                                                                               
  {0x3618,0x04},                                 
  {0x3612,0x2b},                                 
  {0x3709,0x12},                                 
  {0x370c,0x00},                                 
  {0x3a02,0x07}, //60HZ max exposure limit MSB   
  {0x3a03,0xb0}, //60HZ max exposure limit LSB   
  {0x3a14,0x07}, //50HZ max exposure limit MSB   
  {0x3a15,0xb0}, //50HZ max exposure limit LSB   
  {0x4004,0x06}, //BLC line number               
  {0x4837,0x2c},//PCLK period                    
  {0x5001,0xa3},//ISP effect    
  
  {0x302c,0xc2},//bit[7:6]: output drive capability
            //00: 1x   01: 2x  10: 3x  11: 4x      
  //power down release
//  {0x3008,0x02}, 
//  {REG_DLY,0x32},//delay 50ms  
  //{REG_TERM,VAL_TERM},           
};

static struct regval_list sensor_qxga_regs[] = { //qxga: 2048*1536
  //capture 3Mega 7.5fps
  //power down
//  {0x3008,0x42},
  //pll and clock setting
  {0x3034,0x18},                               
#ifndef FPGA
  {0x3035,0x21},                          
#else
  {0x3035,0x41},                         
#endif                               
  {0x3036,0x54},                               
  {0x3037,0x13},                               
  {0x3108,0x01},                               
  {0x3824,0x01},                               
  {REG_DLY,0x05},//delay 5ms              
  //timing                                           
  //2048*1536                                        
  {0x3808,0x08}, //H size MSB                 
  {0x3809,0x00}, //H size LSB                 
  {0x380a,0x06}, //V size MSB                 
  {0x380b,0x00}, //V size LSB                 
  {0x380c,0x0b}, //HTS MSB                    
  {0x380d,0x1c}, //HTS LSB                    
  {0x380e,0x07}, //VTS MSB                    
  {0x380f,0xb0}, //LSB                        
#ifndef FPGA
  //banding step                                        
  {0x3a08,0x00}, //50HZ step MSB                 
  {0x3a09,0x93}, //50HZ step LSB                 
  {0x3a0a,0x00}, //60HZ step MSB                 
  {0x3a0b,0x7b}, //60HZ step LSB                 
  {0x3a0e,0x0d}, //50HZ step max                 
  {0x3a0d,0x10}, //60HZ step max                 
#else
  //banding step                                        
  {0x3a08,0x00}, //50HZ step MSB                 
  {0x3a09,0x49}, //50HZ step LSB                 
  {0x3a0a,0x00}, //60HZ step MSB                 
  {0x3a0b,0x3d}, //60HZ step LSB                 
  {0x3a0e,0x1a}, //50HZ step max                 
  {0x3a0d,0x20}, //60HZ step max 
#endif        
                                                     
//  {0x3503,0x07}, //AEC disable                                             
  {0x350c,0x00},                              
  {0x350d,0x00},                              
  {0x3c07,0x07}, //light meter 1 thereshold   
                                                     
  {0x3814,0x11}, //horizton subsample
  {0x3815,0x11}, //vertical subsample
  {0x3800,0x00}, //x address start high byte
  {0x3801,0x00}, //x address start low byte  
  {0x3802,0x00},  //y address start high byte 
  {0x3803,0x00}, //y address start low byte 
  {0x3804,0x0a}, //x address end high byte
  {0x3805,0x3f}, //x address end low byte 
  {0x3806,0x07}, //y address end high byte
  {0x3807,0x9f}, //y address end low byte 
  {0x3810,0x00}, //isp hortizontal offset high byte
  {0x3811,0x10}, //isp hortizontal offset low byte
  {0x3812,0x00}, //isp vertical offset high byte
  {0x3813,0x04},  //isp vertical offset low byte 
  
//  {0x5308,0x65},    //sharpen manual                                                   
//  {0x5302,0x20}, //sharpness                    
                                       
  {0x4002,0xc5},  //BLC related               
  {0x4005,0x1a}, // BLC related                              
                                                                               
  {0x3618,0x04},                              
  {0x3612,0x2b},                              
  {0x3709,0x12},                              
  {0x370c,0x00}, 
  {0x3a02,0x07}, //60HZ max exposure limit MSB
  {0x3a03,0xb0}, //60HZ max exposure limit LSB
  {0x3a14,0x07}, //50HZ max exposure limit MSB
  {0x3a15,0xb0}, //50HZ max exposure limit LSB
  {0x4004,0x06}, //BLC line number                                         
  {0x4837,0x2c},//PCLK period                              
  {0x5001,0xa3},//ISP effect  
  {0x302c,0xc2},//bit[7:6]: output drive capability
            //00: 1x   01: 2x  10: 3x  11: 4x   
  //power down release
//  {0x3008,0x02},     
//  {REG_DLY,0x32},//delay 50ms
  //{REG_TERM,VAL_TERM},           
};                                      

static struct regval_list sensor_uxga_regs[] = { //UXGA: 1600*1200
    //capture 2Mega 7.5fps
  //power down
//  {0x3008,0x42},
  //pll and clock setting                                                                                                                        
  {0x3034,0x18},                                                                                                             
#ifndef FPGA
  {0x3035,0x21},                          
#else
  {0x3035,0x41},                         
#endif                                                                                                           
  {0x3036,0x54},                                                                                                             
  {0x3037,0x13},                                                                                                             
  {0x3108,0x01},                                                                                                             
  {0x3824,0x01},                                                                                                             
  {REG_DLY,0x05},//delay 5ms                                        
  //timing                                                                                                                             
  //1600*1200                                                                                                                          
  {0x3808,0x06}, //H size MSB                                                                                             
  {0x3809,0x40}, //H size LSB                                                                                             
  {0x380a,0x04}, //V size MSB                                                                                             
  {0x380b,0xb0}, //V size LSB                                                                                             
  {0x380c,0x0b}, //HTS MSB                                                                                                
  {0x380d,0x1c}, //HTS LSB                                                                                                
  {0x380e,0x07}, //VTS MSB                                                                                                
  {0x380f,0xb0}, //LSB                                                                                                    
#ifndef FPGA
  //banding step                                        
  {0x3a08,0x00}, //50HZ step MSB                 
  {0x3a09,0x93}, //50HZ step LSB                 
  {0x3a0a,0x00}, //60HZ step MSB                 
  {0x3a0b,0x7b}, //60HZ step LSB                 
  {0x3a0e,0x0d}, //50HZ step max                 
  {0x3a0d,0x10}, //60HZ step max                 
#else
  //banding step                                        
  {0x3a08,0x00}, //50HZ step MSB                 
  {0x3a09,0x49}, //50HZ step LSB                 
  {0x3a0a,0x00}, //60HZ step MSB                 
  {0x3a0b,0x3d}, //60HZ step LSB                 
  {0x3a0e,0x1a}, //50HZ step max                 
  {0x3a0d,0x20}, //60HZ step max 
#endif                                                                                          
                                                                                                                                       
//  {0x3503,0x07}, //AEC disable                                                                                                                                                                                                                  
  {0x350c,0x00},                                                                                                                 
  {0x350d,0x00},                                                                                                                 
  {0x3c07,0x07}, //light meter 1 thereshold                                                                                      
                                                                                                                                                                                                                        
  {0x3814,0x11}, //horizton subsample
  {0x3815,0x11}, //vertical subsample
  {0x3800,0x00}, //x address start high byte
  {0x3801,0x00}, //x address start low byte  
  {0x3802,0x00},  //y address start high byte 
  {0x3803,0x00}, //y address start low byte 
  {0x3804,0x0a}, //x address end high byte
  {0x3805,0x3f}, //x address end low byte 
  {0x3806,0x07}, //y address end high byte
  {0x3807,0x9f}, //y address end low byte 
  {0x3810,0x00}, //isp hortizontal offset high byte
  {0x3811,0x10}, //isp hortizontal offset low byte
  {0x3812,0x00}, //isp vertical offset high byte
  {0x3813,0x04},  //isp vertical offset low byte 
                                                                                                                                       
  {0x4002,0xc5}, //BLC related                                                                                                     
  {0x4005,0x12}, //BLC related                                                                                        
//  {0x5308,0x65},    //sharpen manual
//  {0x5302,0x20},//sharpness                                                                                          
                                                                                                                                                                                                                                       
  {0x3618,0x04},                                                                                                               
  {0x3612,0x2b},                                                                                                               
  {0x3709,0x12},                                                                                                               
  {0x370c,0x00},                                                                                                               
  {0x3a02,0x07},//60HZ max exposure limit MSB                                                                                                                  
  {0x3a03,0xb0},//60HZ max exposure limit LSB                                                                                   
  {0x3a14,0x07},//50HZ max exposure limit MSB                                                                                   
  {0x3a15,0xb0},//50HZ max exposure limit LSB                                                                                   
  {0x4004,0x06},//BLC line number                                                                                               
                                                                                                                                                                               
                                                                                                                       
  {0x4837,0x2c}, //PCLK period                                                                                                  
  {0x5001,0xa3}, //ISP effect  
  {0x302c,0xc2},//bit[7:6]: output drive capability
            //00: 1x   01: 2x  10: 3x  11: 4x                                                                                  
  //power down release
//  {0x3008,0x02},     
//  {REG_DLY,0x32},//delay 50ms
  //{REG_TERM,VAL_TERM},
};

#if 1
static struct regval_list sensor_sxga_regs[] = { //SXGA: 1280*960
  //capture 1.3Mega 7.5fps
  //power down
//  {0x3008,0x42},
  {0x3820,0x40},
  {0x3821,0x06},
  //pll and clock setting                                                                                   
  {0x3034,0x18},                                                      
#ifndef FPGA
  {0x3035,0x11},                          
#else
  {0x3035,0x41},                         
#endif                                                        
  {0x3036,0x54},                                                      
  {0x3037,0x13},                                                      
  {0x3108,0x01},                                                      
  {0x3824,0x01},                                                      
  {REG_DLY,0x05},//delay 5ms                                  
  //timing                                                                                                  
  //1280*960                                                                                                
  {0x3808,0x05}, //H size MSB                                                                  
  {0x3809,0x00}, //H size LSB                                                                  
  {0x380a,0x03}, //V size MSB                                                                  
  {0x380b,0xc0}, //V size LSB                                                                  
  {0x380c,0x0b}, //HTS MSB                                                                     
  {0x380d,0x1c}, //HTS LSB                                                                     
  {0x380e,0x07}, //VTS MSB                                                                     
  {0x380f,0xb0}, //LSB                                                                         
#ifndef FPGA 
  {0x3a08,0x00}, //50HZ step MSB                 
  {0x3a09,0x94}, //50HZ step LSB                 
  {0x3a0a,0x00}, //60HZ step MSB                 
  {0x3a0b,0x7b}, //60HZ step LSB                 
  {0x3a0e,0x06}, //50HZ step max                 
  {0x3a0d,0x08}, //60HZ step max                 
#else
  //banding step                                        
  {0x3a08,0x00}, //50HZ step MSB                 
  {0x3a09,0x49}, //50HZ step LSB                 
  {0x3a0a,0x00}, //60HZ step MSB                 
  {0x3a0b,0x3d}, //60HZ step LSB                 
  {0x3a0e,0x1a}, //50HZ step max                 
  {0x3a0d,0x20}, //60HZ step max 
#endif                                                                      
                                                                                                            
  {0x3503,0x00},  //AEC enable                                                                                                                                                          
  {0x350c,0x00},                                                                                     
  {0x350d,0x00},                                                                                     
  {0x3c07,0x07}, //light meter 1 thereshold                                                          
                                                                                                                                                                
  {0x3814,0x11}, //horizton subsample
  {0x3815,0x11}, //vertical subsample
  {0x3800,0x00}, //x address start high byte
  {0x3801,0x00}, //x address start low byte  
  {0x3802,0x00},  //y address start high byte 
  {0x3803,0x00}, //y address start low byte 
  {0x3804,0x0a}, //x address end high byte
  {0x3805,0x3f}, //x address end low byte 
  {0x3806,0x07}, //y address end high byte
  {0x3807,0x9f}, //y address end low byte 
  {0x3810,0x00}, //isp hortizontal offset high byte
  {0x3811,0x10}, //isp hortizontal offset low byte
  {0x3812,0x00}, //isp vertical offset high byte
  {0x3813,0x04},  //isp vertical offset low byte                                                                                                          
                                                                                              
  {0x4002,0xc5}, //BLC related                                                           
  {0x4005,0x12}, //BLC related                                                             
//  {0x5308,0x65},    //sharpen manual
//  {0x5302,0x20},//sharpness                                                                            
                                                                                                                                                                                                
  {0x3618,0x04},                                                                                    
  {0x3612,0x2b},                                                                                    
  {0x3709,0x12},                                                                                    
  {0x370c,0x00},                                                       
  {0x3a02,0x07}, //60HZ max exposure limit MSB                                                      
  {0x3a03,0xb0}, //60HZ max exposure limit LSB                                                      
  {0x3a14,0x07}, //50HZ max exposure limit MSB                                                      
  {0x3a15,0xb0}, //50HZ max exposure limit LSB                                                      
  {0x4004,0x06}, //BLC line number                                                                                  
                                                                             
  {0x4837,0x2c}, //PCLK period
  {0x5001,0xa3}, //ISP effect   
  {0x302c,0xc2},//bit[7:6]: output drive capability
            //00: 1x   01: 2x  10: 3x  11: 4x 
  {0x3a18,0x00},//
  {0x3a19,0xf8},//  
  //power down release
//  {0x3008,0x02},     
//  {REG_DLY,0x32},//delay 50ms
  //{REG_TERM,VAL_TERM},
};
#else
static struct regval_list sensor_sxga_regs[] = { //1280*960
  //for video
//	//power down
//	{0x30,0x08,0x42},
  {0x3820,0x41},
  {0x3821,0x07},
//	//pll and clock setting
	{0x3034,0x14},
#ifndef CSI_VER_FOR_FPGA
	{0x3035,0x61},	//0x11:60fps 0x21:30fps 0x61:15fps
#else
	{0x3035,0x61},	//0x11:60fps 0x21:30fps 0x41:15fps 0xa1:7.5fps
#endif
	{0x3036,0x54},
	{0x3037,0x13},
	{0x3108,0x01},
	{0x3824,0x01},
	{0xffff,0x05},//delay 5ms
	//timing
	//1280x960
	{0x3808,0x05},	//H size MSB
	{0x3809,0x00},	//H size LSB
	{0x380a,0x03},	//V size MSB
	{0x380b,0xc0},	//V size LSB
	{0x380c,0x07},	//HTS MSB        
	{0x380d,0x68},	//HTS LSB   
	{0x380e,0x03},	//VTS MSB        
	{0x380f,0xd8},	//LSB       
#ifndef CSI_VER_FOR_FPGA
	//banding step  
	{0x3a08,0x01},//50HZ step MSB 
	{0x3a09,0x27},//50HZ step LSB 
	{0x3a0a,0x00},//60HZ step MSB 
	{0x3a0b,0xf6},//60HZ step LSB 
	{0x3a0e,0x03},//50HZ step max 
	{0x3a0d,0x04},//60HZ step max 
#else
	//banding step 
	{0x3a08,0x00},//50HZ step MSB 
	{0x3a09,0x93},//50HZ step LSB 
	{0x3a0a,0x00},//60HZ step MSB 
	{0x3a0b,0x7b},//60HZ step LSB 
	{0x3a0e,0x06},//50HZ step max 
	{0x3a0d,0x08},//60HZ step max 
#endif	
  {0x3503,0x00},  //AEC enable
  {0x350c,0x00},
  {0x350d,0x00},
	{0x3c07,0x07}, //light meter 1 thereshold   
	{0x3814,0x31}, //horizton subsample
	{0x3815,0x31}, //vertical subsample
	{0x3800,0x00}, //x address start high byte
	{0x3801,0x00}, //x address start low byte  
	{0x3802,0x00},	//y address start high byte 
	{0x3803,0x04}, //y address start low byte 
	{0x3804,0x0a}, //x address end high byte
	{0x3805,0x3f}, //x address end low byte 
	{0x3806,0x07}, //y address end high byte
	{0x3807,0x9b}, //y address end low byte 
	{0x3810,0x00}, //isp hortizontal offset high byte
	{0x3811,0x10}, //isp hortizontal offset low byte
	{0x3812,0x00}, //isp vertical offset high byte
	{0x3813,0x06},	//isp vertical offset low byte
	
//	{0x5308,0x65},		//sharpen manual
//	{0x5302,0x00},		//sharpen offset 1
	{0x4002,0x45},		//BLC related
	{0x4005,0x18},		//BLC related
	
	{0x3618,0x00},
	{0x3612,0x29},
	{0x3709,0x52},
	{0x370c,0x03},
	{0x3a02,0x03}, //60HZ max exposure limit MSB 
	{0x3a03,0xd8}, //60HZ max exposure limit LSB 
	{0x3a14,0x03}, //50HZ max exposure limit MSB 
	{0x3a15,0xd8}, //50HZ max exposure limit LSB 
	
	{0x4004,0x02}, //BLC line number
	{0x3002,0x1c}, //reset JFIFO SFIFO JPG
	{0x3006,0xc3}, //enable xx clock
	{0x460b,0x37},	//debug mode
	{0x460c,0x20}, //PCLK Manuale
	{0x4837,0x2c}, //PCLK period
	{0x5001,0x83}, //ISP effect
//	{0x3503,0x00},//AEC enable
	
	{0x302c,0xc2},//bit[7:6]: output drive capability
						//00: 1x   01: 2x  10: 3x  11: 4x 
  {0x3a18,0x00},//
  {0x3a19,0xf8},//
//	//power down release
//	{0x3008,0x02},     
//	{0xffff,0x32},//delay 50ms
};
#endif

static struct regval_list sensor_xga_regs[] = { //XGA: 1024*768
  //capture 1Mega 7.5fps
  //power down
//  {0x3008,0x42},
  //pll and clock setting
  {0x3034,0x18},
#ifndef FPGA
  {0x3035,0x21},                          
#else
  {0x3035,0x41},                         
#endif
  {0x3036,0x54},
  {0x3037,0x13},
  {0x3108,0x01},
  {0x3824,0x01},
  {REG_DLY,0x05},//delay 5ms
  //timing
  //1024*768
  {0x3808,0x04}, //H size MSB
  {0x3809,0x00}, //H size LSB
  {0x380a,0x03}, //V size MSB
  {0x380b,0x00}, //V size LSB
  {0x380c,0x0b}, //HTS MSB    
  {0x380d,0x1c}, //HTS LSB     
  {0x380e,0x07}, //VTS MSB    
  {0x380f,0xb0}, //LSB
#ifndef FPGA
  //banding step                                        
  {0x3a08,0x00}, //50HZ step MSB                 
  {0x3a09,0x93}, //50HZ step LSB                 
  {0x3a0a,0x00}, //60HZ step MSB                 
  {0x3a0b,0x7b}, //60HZ step LSB                 
  {0x3a0e,0x0d}, //50HZ step max                 
  {0x3a0d,0x10}, //60HZ step max                 
#else
  //banding step                                        
  {0x3a08,0x00}, //50HZ step MSB                 
  {0x3a09,0x49}, //50HZ step LSB                 
  {0x3a0a,0x00}, //60HZ step MSB                 
  {0x3a0b,0x3d}, //60HZ step LSB                 
  {0x3a0e,0x1a}, //50HZ step max                 
  {0x3a0d,0x20}, //60HZ step max 
#endif
//  {0x3503,0x07}, //AEC disable                                                                                              
  {0x350c,0x00},                              
  {0x350d,0x00},                              
  {0x3c07,0x07}, //light meter 1 thereshold   
                                                      

  {0x3814,0x11}, //horizton subsample
  {0x3815,0x11}, //vertical subsample
  {0x3800,0x00}, //x address start high byte
  {0x3801,0x00}, //x address start low byte  
  {0x3802,0x00},  //y address start high byte 
  {0x3803,0x00}, //y address start low byte 
  {0x3804,0x0a}, //x address end high byte
  {0x3805,0x3f}, //x address end low byte 
  {0x3806,0x07}, //y address end high byte
  {0x3807,0x9f}, //y address end low byte 
  {0x3810,0x00}, //isp hortizontal offset high byte
  {0x3811,0x10}, //isp hortizontal offset low byte
  {0x3812,0x00}, //isp vertical offset high byte
  {0x3813,0x04},  //isp vertical offset low byte
  
//  {0x5308,0x65},    //sharpen manual
//  {0x5302,0x20},    //sharpen offset 1
  {0x4002,0xc5},    //BLC related
  {0x4005,0x12},    //BLC related
     
  {0x3618,0x00},      
  {0x3612,0x29},      
  {0x3709,0x52},      
  {0x370c,0x03},      
  {0x3a02,0x03},  //60HZ max exposure limit MSB 
  {0x3a03,0xd8},  //60HZ max exposure limit LSB     
  {0x3a14,0x03},  //50HZ max exposure limit MSB     
  {0x3a15,0xd8},  //50HZ max exposure limit LSB     
  {0x4004,0x02},  //BLC line number    
  
  {0x4837,0x22},  //PCLK period    
  {0x5001,0xa3},  //ISP effect
                                                                                                                                                     
  {0x3618,0x04},                               
  {0x3612,0x2b},                               
  {0x3709,0x12},                               
  {0x370c,0x00},                               
  {0x3a02,0x07}, //60HZ max exposure limit MSB
  {0x3a03,0xb0}, //60HZ max exposure limit LSB
  {0x3a14,0x07}, //50HZ max exposure limit MSB
  {0x3a15,0xb0}, //50HZ max exposure limit LSB
  {0x4004,0x06}, //BLC line number            
  {0x4837,0x2c}, //PCLK period 
  {0x5001,0xa3}, //ISP effect
  
  {0x302c,0xc2},//bit[7:6]: output drive capability
            //00: 1x   01: 2x  10: 3x  11: 4x 
  //power down release
//  {0x3008,0x02},     
//  {REG_DLY,0x32},//delay 50ms
  //{REG_TERM,VAL_TERM},
};

//for video
static struct regval_list sensor_1080p_regs[] = { //1080: 1920*1080 
  //power down
//  {0x3008,0x42},
  {0x3820,0x40},
  {0x3821,0x06},
  //pll and clock setting
  {0x3034,0x18},
#ifndef FPGA
  {0x3035,0x21},  //0x11:30fps 0x21:15fps
#else
  {0x3035,0x41},  //0x11:30fps 0x21:15fps 0x41:7.5fps
#endif  
  {0x3036,0x54},
  {0x3037,0x13},
  {0x3108,0x01},
  {0x3824,0x01},
  {REG_DLY,0x05},//delay 5ms
  //timing
  //1920x1080
  {0x3808,0x07},  //H size MSB
  {0x3809,0x80},  //H size LSB
  {0x380a,0x04},  //V size MSB
  {0x380b,0x38},  //V size LSB
  {0x380c,0x09},  //HTS MSB        
  {0x380d,0xc4},  //HTS LSB   
  {0x380e,0x04},  //VTS MSB        
  {0x380f,0x60},  //VTS LSB       
#ifndef FPGA
  //banding step
  {0x3a08,0x01}, //50HZ step MSB 
  {0x3a09,0x50}, //50HZ step LSB 
  {0x3a0a,0x01}, //60HZ step MSB 
  {0x3a0b,0x18}, //60HZ step LSB 
  {0x3a0e,0x03}, //50HZ step max 
  {0x3a0d,0x04}, //60HZ step max 
#else
  //banding step
  {0x3a08,0x00}, //50HZ step MSB 
  {0x3a09,0x54}, //50HZ step LSB 
  {0x3a0a,0x00}, //60HZ step MSB 
  {0x3a0b,0x46}, //60HZ step LSB 
  {0x3a0e,0x0d}, //50HZ step max 
  {0x3a0d,0x10}, //60HZ step max 
#endif  
  {0x3503,0x00},  //AEC enable
  {0x350c,0x00},
  {0x350d,0x00},
  {0x3c07,0x07}, //light meter 1 thereshold   
  {0x3814,0x11}, //horizton subsample
  {0x3815,0x11}, //vertical subsample
  {0x3800,0x01}, //x address start high byte
  {0x3801,0x50}, //x address start low byte  
  {0x3802,0x01},  //y address start high byte 
  {0x3803,0xb2}, //y address start low byte 
  {0x3804,0x08}, //x address end high byte
  {0x3805,0xef}, //x address end low byte 
  {0x3806,0x05}, //y address end high byte
  {0x3807,0xf1}, //y address end low byte 
  {0x3810,0x00}, //isp hortizontal offset high byte
  {0x3811,0x10}, //isp hortizontal offset low byte
  {0x3812,0x00}, //isp vertical offset high byte
  {0x3813,0x04},  //isp vertical offset low byte
  
//  {0x5308,0x65},    //sharpen manual
//  {0x5302,0x00},    //sharpen offset 1
  {0x4002,0x45},    //BLC related
  {0x4005,0x18},    //BLC related
  
  {0x3618,0x04},
  {0x3612,0x2b},
  {0x3709,0x12},
  {0x370c,0x00},
  {0x3a02,0x04}, //60HZ max exposure limit MSB 
  {0x3a03,0x60}, //60HZ max exposure limit LSB 
  {0x3a14,0x04}, //50HZ max exposure limit MSB 
  {0x3a15,0x60}, //50HZ max exposure limit LSB 
  
  {0x4004,0x06}, //BLC line number
  {0x3002,0x1c}, //reset JFIFO SFIFO JPG
  {0x3006,0xc3}, //enable xx clock
  {0x460b,0x37},  //debug mode
  {0x460c,0x20}, //PCLK Manuale
  {0x4837,0x16}, //PCLK period
  {0x5001,0x83}, //ISP effect
//  {0x3503,0x00},//AEC enable
  {0x302c,0xc2},//bit[7:6]: output drive capability
            //00: 1x   01: 2x  10: 3x  11: 4x 
  {0x3a18,0x00},//
  {0x3a19,0x80},//
  //power down release
//  {0x3008,0x02},     
//  {REG_DLY,0x32},//delay 50ms
  //{REG_TERM,VAL_TERM},
};

static struct regval_list sensor_720p_regs[] = { //1280*720
//  //power down
//  {0x3008,0x42},
  {0x3820,0x41},
  {0x3821,0x07},
//  //pll and clock setting
  {0x3034,0x18},
#ifndef FPGA
  {0x3035,0x21},  //0x11:60fps 0x21:30fps 0x41:15fps
#else
  {0x3035,0x41},  //0x11:60fps 0x21:30fps 0x41:15fps 0xa1:7.5fps
#endif
  {0x3036,0x54},
  {0x3037,0x13},
  {0x3108,0x01},
  {0x3824,0x01},
  {REG_DLY,0x05},//delay 5ms
  //timing
  //1280x720
  {0x3808,0x05},  //H size MSB
  {0x3809,0x00},  //H size LSB
  {0x380a,0x02},  //V size MSB
  {0x380b,0xd0},  //V size LSB
  {0x380c,0x07},  //HTS MSB        
  {0x380d,0x64},  //HTS LSB   
  {0x380e,0x02},  //VTS MSB        
  {0x380f,0xe4},  //LSB       
#ifndef FPGA
  //banding step
  {0x3a08,0x00}, //50HZ step MSB 
  {0x3a09,0xdd}, //50HZ step LSB 
  {0x3a0a,0x00}, //60HZ step MSB 
  {0x3a0b,0xb8}, //60HZ step LSB 
  {0x3a0e,0x03}, //50HZ step max 
  {0x3a0d,0x04}, //60HZ step max 
#else
  //banding step
  {0x3a08,0x00}, //50HZ step MSB 
  {0x3a09,0x6e}, //50HZ step LSB 
  {0x3a0a,0x00}, //60HZ step MSB 
  {0x3a0b,0x5c}, //60HZ step LSB 
  {0x3a0e,0x06}, //50HZ step max 
  {0x3a0d,0x08}, //60HZ step max 
#endif  
  {0x3503,0x00},  //AEC enable
  {0x350c,0x00},
  {0x350d,0x00},
  {0x3c07,0x07}, //light meter 1 thereshold   
  {0x3814,0x31}, //horizton subsample
  {0x3815,0x31}, //vertical subsample
  {0x3800,0x00}, //x address start high byte
  {0x3801,0x00}, //x address start low byte  
  {0x3802,0x00},  //y address start high byte 
  {0x3803,0xfa}, //y address start low byte 
  {0x3804,0x0a}, //x address end high byte
  {0x3805,0x3f}, //x address end low byte 
  {0x3806,0x06}, //y address end high byte
  {0x3807,0xa9}, //y address end low byte 
  {0x3810,0x00}, //isp hortizontal offset high byte
  {0x3811,0x10}, //isp hortizontal offset low byte
  {0x3812,0x00}, //isp vertical offset high byte
  {0x3813,0x04},  //isp vertical offset low byte
  
//  {0x5308,0x65},    //sharpen manual
//  {0x5302,0x00},    //sharpen offset 1
  {0x4002,0x45},    //BLC related
  {0x4005,0x18},    //BLC related
  
  {0x3618,0x00},
  {0x3612,0x29},
  {0x3709,0x52},
  {0x370c,0x03},
  {0x3a02,0x02}, //60HZ max exposure limit MSB 
  {0x3a03,0xe0}, //60HZ max exposure limit LSB 
  {0x3a14,0x02}, //50HZ max exposure limit MSB 
  {0x3a15,0xe0}, //50HZ max exposure limit LSB 
  
  {0x4004,0x02}, //BLC line number
  {0x3002,0x1c}, //reset JFIFO SFIFO JPG
  {0x3006,0xc3}, //enable xx clock
  {0x460b,0x37},  //debug mode
  {0x460c,0x20}, //PCLK Manuale
  {0x4837,0x16}, //PCLK period
  {0x5001,0x83}, //ISP effect
//  {0x3503,0x00},//AEC enable
  
  {0x302c,0xc2},//bit[7:6]: output drive capability
            //00: 1x   01: 2x  10: 3x  11: 4x 
  {0x3a18,0x00},//
  {0x3a19,0xd8},//
//  //power down release
//  {0x3008,0x02},     
//  {REG_DLY,0x32},//delay 50ms
  //{REG_TERM,VAL_TERM},
};

static struct regval_list sensor_svga_regs[] = { //SVGA: 800*600
//  //power down
//  {0x3008,0x42},
//  //pll and clock setting
  {0x3034,0x1a},                
#ifndef FPGA
  {0x3035,0x11}, 
#else
  {0x3035,0x21}, //0x11:30fps 0x21:15fps
#endif               
  {0x3036,0x46},                
  {0x3037,0x13},                
  {0x3108,0x01},                
  {0x3824,0x01},                
  {REG_DLY,0x05},//delay 5ms
  //timing                             
  //800x600                            
  {0x3808,0x3 }, //H size MSB   
  {0x3809,0x20}, //H size LSB   
  {0x380a,0x2 }, //V size MSB   
  {0x380b,0x58}, //V size LSB   
  {0x380c,0x07}, //HTS MSB      
  {0x380d,0x68}, //HTS LSB      
  {0x380e,0x03}, //VTS MSB      
  {0x380f,0xd8}, //LSB          
#ifndef FPGA
  //banding step  
  {0x3a08,0x01},//50HZ step MSB 
  {0x3a09,0x27},//50HZ step LSB 
  {0x3a0a,0x00},//60HZ step MSB 
  {0x3a0b,0xf6},//60HZ step LSB 
  {0x3a0e,0x03},//50HZ step max 
  {0x3a0d,0x04},//60HZ step max 
#else
  //banding step 
  {0x3a08,0x00},//50HZ step MSB 
  {0x3a09,0x93},//50HZ step LSB 
  {0x3a0a,0x00},//60HZ step MSB 
  {0x3a0b,0x7b},//60HZ step LSB 
  {0x3a0e,0x06},//50HZ step max 
  {0x3a0d,0x08},//60HZ step max 
#endif  
  
//  {0x3503,0x00},  //AEC enable
  {0x3c07,0x08},   //light meter 1 thereshold   
  
  {0x3814,0x31}, //horizton subsample
  {0x3815,0x31}, //vertical subsample
  {0x3800,0x00}, //x address start high byte
  {0x3801,0x00}, //x address start low byte  
  {0x3802,0x00},  //y address start high byte 
  {0x3803,0x04}, //y address start low byte 
  {0x3804,0x0a}, //x address end high byte
  {0x3805,0x3f}, //x address end low byte 
  {0x3806,0x07}, //y address end high byte
  {0x3807,0x9b}, //y address end low byte 
  {0x3810,0x00}, //isp hortizontal offset high byte
  {0x3811,0x10}, //isp hortizontal offset low byte
  {0x3812,0x00}, //isp vertical offset high byte
  {0x3813,0x06},  //isp vertical offset low byte
  
//  {0x5308,0x65},    //sharpen manual
//  {0x5302,0x00},    //sharpen offset 1
  {0x4002,0x45},    //BLC related
  {0x4005,0x18},    //BLC related
     
  {0x3618,0x00},      
  {0x3612,0x29},      
  {0x3709,0x52},      
  {0x370c,0x03},      
  {0x3a02,0x03},  //60HZ max exposure limit MSB 
  {0x3a03,0xd8},  //60HZ max exposure limit LSB     
  {0x3a14,0x03},  //50HZ max exposure limit MSB     
  {0x3a15,0xd8},  //50HZ max exposure limit LSB     
  {0x4004,0x02},  //BLC line number    
  
  {0x4837,0x22},  //PCLK period    
  {0x5001,0xa3},  //ISP effect
  
  {0x302c,0xc2},//bit[7:6]: output drive capability
            //00: 1x   01: 2x  10: 3x  11: 4x 
//  //power down release
//  {0x3008,0x02},     
//  {REG_DLY,0x32},//delay 50ms
  //{REG_TERM,VAL_TERM},
};

static struct regval_list sensor_vga_regs[] = { //VGA:  640*480
  
  //timing                             
  //640x480   
  //power down
//  {0x3008,0x42},
  {0x3820,0x41},
  {0x3821,0x07},
//  //pll and clock setting
  {0x3034,0x1a},                
#ifndef FPGA
  {0x3035,0x11},                             
#else
  {0x3035,0x21},                            
#endif    
  {0x3036,0x46},                
  {0x3037,0x13},                
  {0x3108,0x01},                
  {0x3824,0x01},                
  {REG_DLY,0x05}, //delay 50ms 
                           
  {0x3808,0x02}, //H size MSB   
  {0x3809,0x80}, //H size LSB   
  {0x380a,0x01}, //V size MSB   
  {0x380b,0xe0}, //V size LSB   
  {0x380c,0x07}, //HTS MSB      
  {0x380d,0x68}, //HTS LSB      
  {0x380e,0x03}, //VTS MSB      
  {0x380f,0xd8}, //LSB          
              
#ifndef FPGA
  //banding step  
  {0x3a08,0x01},//50HZ step MSB 
  {0x3a09,0x27},//50HZ step LSB 
  {0x3a0a,0x00},//60HZ step MSB 
  {0x3a0b,0xf6},//60HZ step LSB 
  {0x3a0e,0x03},//50HZ step max 
  {0x3a0d,0x04},//60HZ step max 
#else
  //banding step 
  {0x3a08,0x00},//50HZ step MSB 
  {0x3a09,0x93},//50HZ step LSB 
  {0x3a0a,0x00},//60HZ step MSB 
  {0x3a0b,0x7b},//60HZ step LSB 
  {0x3a0e,0x06},//50HZ step max 
  {0x3a0d,0x08},//60HZ step max 
#endif  
  {0x3618,0x00},      
  {0x3612,0x29},      
  {0x3709,0x52},      
  {0x370c,0x03},      
  {0x3a02,0x03},  //60HZ max exposure limit MSB 
  {0x3a03,0xd8},  //60HZ max exposure limit LSB     
  {0x3a14,0x03},  //50HZ max exposure limit MSB     
  {0x3a15,0xd8},  //50HZ max exposure limit LSB     
  {0x4004,0x02},  //BLC line number 
  
  {0x3503,0x00},  //AEC enable
  {0x350c,0x00},
  {0x350d,0x00},
  {0x3c07,0x08},   //light meter 1 thereshold   
  
  {0x3814,0x31}, //horizton subsample
  {0x3815,0x31}, //vertical subsample
  {0x3800,0x00}, //x address start high byte
  {0x3801,0x00}, //x address start low byte  
  {0x3802,0x00},  //y address start high byte 
  {0x3803,0x04}, //y address start low byte 
  {0x3804,0x0a}, //x address end high byte
  {0x3805,0x3f}, //x address end low byte 
  {0x3806,0x07}, //y address end high byte
  {0x3807,0x9b}, //y address end low byte 
  {0x3810,0x00}, //isp hortizontal offset high byte
  {0x3811,0x10}, //isp hortizontal offset low byte
  {0x3812,0x00}, //isp vertical offset high byte
  {0x3813,0x06},  //isp vertical offset low byte
  
//  {0x5308,0x65},    //sharpen manual
//  {0x5302,0x00},    //sharpen offset 1
  {0x4002,0x45},    //BLC related
  {0x4005,0x18},    //BLC related
     
   
  
  {0x4837,0x22},  //PCLK period    
  {0x5001,0xa3},  //ISP effect
  
  {0x302c,0xc2},//bit[7:6]: output drive capability
            //00: 1x   01: 2x  10: 3x  11: 4x 
  {0x3a18,0x00},//
  {0x3a19,0xf8},//
//  //power down release
//  {0x3008,0x02},     
//  {REG_DLY,0x32},//delay 50ms
  //{REG_TERM,VAL_TERM},
};

#ifdef AUTO_FPS	
//auto framerate mode
static struct regval_list sensor_auto_fps_mode[] = {
	{0x3008,0x42},
	{0x3a00,0x7c},  //night mode bit2
	{0x3a02,0x07},  //60HZ max exposure limit MSB
	{0x3a03,0xb0},  //60HZ max exposure limit LSB 
	{0x3a14,0x07},  //50HZ max exposure limit MSB  
	{0x3a15,0xb0},  //50HZ max exposure limit LSB
	{0x3008,0x02},
  	//{REG_TERM,VAL_TERM},
};

//auto framerate mode
static struct regval_list sensor_fix_fps_mode[] = {
	{0x3a00,0x78},//night mode bit2
  	//{REG_TERM,VAL_TERM},
};
#endif
//misc
static struct regval_list sensor_oe_disable_regs[] = {
  {0x3017,0x00},
  {0x3018,0x00},
  //{REG_TERM,VAL_TERM},
};

static struct regval_list sensor_oe_enable_regs[] = {
  {0x3017,0x7f},
  {0x3018,0xfc},
  //{REG_TERM,VAL_TERM},
};

static struct regval_list sensor_sw_stby_on_regs[] = {
  {0x3008,0x42},
  //{REG_TERM,VAL_TERM},
};

static struct regval_list sensor_sw_stby_off_regs[] = {
  {0x3008,0x02},
  //{REG_TERM,VAL_TERM},
};

static unsigned char sensor_af_fw_regs[] = {
  0x02, 0x0f, 0xd6, 0x02, 0x0a, 0x39, 0xc2, 0x01, 0x22, 0x22, 0x00, 0x02, 0x0f, 0xb2, 0xe5, 0x1f, //0x8000,
  0x70, 0x72, 0xf5, 0x1e, 0xd2, 0x35, 0xff, 0xef, 0x25, 0xe0, 0x24, 0x4e, 0xf8, 0xe4, 0xf6, 0x08, //0x8010,
  0xf6, 0x0f, 0xbf, 0x34, 0xf2, 0x90, 0x0e, 0x93, 0xe4, 0x93, 0xff, 0xe5, 0x4b, 0xc3, 0x9f, 0x50, //0x8020,
  0x04, 0x7f, 0x05, 0x80, 0x02, 0x7f, 0xfb, 0x78, 0xbd, 0xa6, 0x07, 0x12, 0x0f, 0x04, 0x40, 0x04, //0x8030,
  0x7f, 0x03, 0x80, 0x02, 0x7f, 0x30, 0x78, 0xbc, 0xa6, 0x07, 0xe6, 0x18, 0xf6, 0x08, 0xe6, 0x78, //0x8040,
  0xb9, 0xf6, 0x78, 0xbc, 0xe6, 0x78, 0xba, 0xf6, 0x78, 0xbf, 0x76, 0x33, 0xe4, 0x08, 0xf6, 0x78, //0x8050,
  0xb8, 0x76, 0x01, 0x75, 0x4a, 0x02, 0x78, 0xb6, 0xf6, 0x08, 0xf6, 0x74, 0xff, 0x78, 0xc1, 0xf6, //0x8060,
  0x08, 0xf6, 0x75, 0x1f, 0x01, 0x78, 0xbc, 0xe6, 0x75, 0xf0, 0x05, 0xa4, 0xf5, 0x4b, 0x12, 0x0a, //0x8070,
  0xff, 0xc2, 0x37, 0x22, 0x78, 0xb8, 0xe6, 0xd3, 0x94, 0x00, 0x40, 0x02, 0x16, 0x22, 0xe5, 0x1f, //0x8080,
  0xb4, 0x05, 0x23, 0xe4, 0xf5, 0x1f, 0xc2, 0x01, 0x78, 0xb6, 0xe6, 0xfe, 0x08, 0xe6, 0xff, 0x78, //0x8090,
  0x4e, 0xa6, 0x06, 0x08, 0xa6, 0x07, 0xa2, 0x37, 0xe4, 0x33, 0xf5, 0x3c, 0x90, 0x30, 0x28, 0xf0, //0x80a0,
  0x75, 0x1e, 0x10, 0xd2, 0x35, 0x22, 0xe5, 0x4b, 0x75, 0xf0, 0x05, 0x84, 0x78, 0xbc, 0xf6, 0x90, //0x80b0,
  0x0e, 0x8c, 0xe4, 0x93, 0xff, 0x25, 0xe0, 0x24, 0x0a, 0xf8, 0xe6, 0xfc, 0x08, 0xe6, 0xfd, 0x78, //0x80c0,
  0xbc, 0xe6, 0x25, 0xe0, 0x24, 0x4e, 0xf8, 0xa6, 0x04, 0x08, 0xa6, 0x05, 0xef, 0x12, 0x0f, 0x0b, //0x80d0,
  0xd3, 0x78, 0xb7, 0x96, 0xee, 0x18, 0x96, 0x40, 0x0d, 0x78, 0xbc, 0xe6, 0x78, 0xb9, 0xf6, 0x78, //0x80e0,
  0xb6, 0xa6, 0x06, 0x08, 0xa6, 0x07, 0x90, 0x0e, 0x8c, 0xe4, 0x93, 0x12, 0x0f, 0x0b, 0xc3, 0x78, //0x80f0,
  0xc2, 0x96, 0xee, 0x18, 0x96, 0x50, 0x0d, 0x78, 0xbc, 0xe6, 0x78, 0xba, 0xf6, 0x78, 0xc1, 0xa6, //0x8100,
  0x06, 0x08, 0xa6, 0x07, 0x78, 0xb6, 0xe6, 0xfe, 0x08, 0xe6, 0xc3, 0x78, 0xc2, 0x96, 0xff, 0xee, //0x8110,
  0x18, 0x96, 0x78, 0xc3, 0xf6, 0x08, 0xa6, 0x07, 0x90, 0x0e, 0x95, 0xe4, 0x18, 0x12, 0x0e, 0xe9, //0x8120,
  0x40, 0x02, 0xd2, 0x37, 0x78, 0xbc, 0xe6, 0x08, 0x26, 0x08, 0xf6, 0xe5, 0x1f, 0x64, 0x01, 0x70, //0x8130,
  0x4a, 0xe6, 0xc3, 0x78, 0xc0, 0x12, 0x0e, 0xdf, 0x40, 0x05, 0x12, 0x0e, 0xda, 0x40, 0x39, 0x12, //0x8140,
  0x0f, 0x02, 0x40, 0x04, 0x7f, 0xfe, 0x80, 0x02, 0x7f, 0x02, 0x78, 0xbd, 0xa6, 0x07, 0x78, 0xb9, //0x8150,
  0xe6, 0x24, 0x03, 0x78, 0xbf, 0xf6, 0x78, 0xb9, 0xe6, 0x24, 0xfd, 0x78, 0xc0, 0xf6, 0x12, 0x0f, //0x8160,
  0x02, 0x40, 0x06, 0x78, 0xc0, 0xe6, 0xff, 0x80, 0x04, 0x78, 0xbf, 0xe6, 0xff, 0x78, 0xbe, 0xa6, //0x8170,
  0x07, 0x75, 0x1f, 0x02, 0x78, 0xb8, 0x76, 0x01, 0x02, 0x02, 0x4a, 0xe5, 0x1f, 0x64, 0x02, 0x60, //0x8180,
  0x03, 0x02, 0x02, 0x2a, 0x78, 0xbe, 0xe6, 0xff, 0xc3, 0x78, 0xc0, 0x12, 0x0e, 0xe0, 0x40, 0x08, //0x8190,
  0x12, 0x0e, 0xda, 0x50, 0x03, 0x02, 0x02, 0x28, 0x12, 0x0f, 0x02, 0x40, 0x04, 0x7f, 0xff, 0x80, //0x81a0,
  0x02, 0x7f, 0x01, 0x78, 0xbd, 0xa6, 0x07, 0x78, 0xb9, 0xe6, 0x04, 0x78, 0xbf, 0xf6, 0x78, 0xb9, //0x81b0,
  0xe6, 0x14, 0x78, 0xc0, 0xf6, 0x18, 0x12, 0x0f, 0x04, 0x40, 0x04, 0xe6, 0xff, 0x80, 0x02, 0x7f, //0x81c0,
  0x00, 0x78, 0xbf, 0xa6, 0x07, 0xd3, 0x08, 0xe6, 0x64, 0x80, 0x94, 0x80, 0x40, 0x04, 0xe6, 0xff, //0x81d0,
  0x80, 0x02, 0x7f, 0x00, 0x78, 0xc0, 0xa6, 0x07, 0xc3, 0x18, 0xe6, 0x64, 0x80, 0x94, 0xb3, 0x50, //0x81e0,
  0x04, 0xe6, 0xff, 0x80, 0x02, 0x7f, 0x33, 0x78, 0xbf, 0xa6, 0x07, 0xc3, 0x08, 0xe6, 0x64, 0x80, //0x81f0,
  0x94, 0xb3, 0x50, 0x04, 0xe6, 0xff, 0x80, 0x02, 0x7f, 0x33, 0x78, 0xc0, 0xa6, 0x07, 0x12, 0x0f, //0x8200,
  0x02, 0x40, 0x06, 0x78, 0xc0, 0xe6, 0xff, 0x80, 0x04, 0x78, 0xbf, 0xe6, 0xff, 0x78, 0xbe, 0xa6, //0x8210,
  0x07, 0x75, 0x1f, 0x03, 0x78, 0xb8, 0x76, 0x01, 0x80, 0x20, 0xe5, 0x1f, 0x64, 0x03, 0x70, 0x26, //0x8220,
  0x78, 0xbe, 0xe6, 0xff, 0xc3, 0x78, 0xc0, 0x12, 0x0e, 0xe0, 0x40, 0x05, 0x12, 0x0e, 0xda, 0x40, //0x8230,
  0x09, 0x78, 0xb9, 0xe6, 0x78, 0xbe, 0xf6, 0x75, 0x1f, 0x04, 0x78, 0xbe, 0xe6, 0x75, 0xf0, 0x05, //0x8240,
  0xa4, 0xf5, 0x4b, 0x02, 0x0a, 0xff, 0xe5, 0x1f, 0xb4, 0x04, 0x10, 0x90, 0x0e, 0x94, 0xe4, 0x78, //0x8250,
  0xc3, 0x12, 0x0e, 0xe9, 0x40, 0x02, 0xd2, 0x37, 0x75, 0x1f, 0x05, 0x22, 0x30, 0x01, 0x03, 0x02, //0x8260,
  0x04, 0xc0, 0x30, 0x02, 0x03, 0x02, 0x04, 0xc0, 0x90, 0x51, 0xa5, 0xe0, 0x78, 0x93, 0xf6, 0xa3, //0x8270,
  0xe0, 0x08, 0xf6, 0xa3, 0xe0, 0x08, 0xf6, 0xe5, 0x1f, 0x70, 0x3c, 0x75, 0x1e, 0x20, 0xd2, 0x35, //0x8280,
  0x12, 0x0c, 0x7a, 0x78, 0x7e, 0xa6, 0x06, 0x08, 0xa6, 0x07, 0x78, 0x8b, 0xa6, 0x09, 0x18, 0x76, //0x8290,
  0x01, 0x12, 0x0c, 0x5b, 0x78, 0x4e, 0xa6, 0x06, 0x08, 0xa6, 0x07, 0x78, 0x8b, 0xe6, 0x78, 0x6e, //0x82a0,
  0xf6, 0x75, 0x1f, 0x01, 0x78, 0x93, 0xe6, 0x78, 0x90, 0xf6, 0x78, 0x94, 0xe6, 0x78, 0x91, 0xf6, //0x82b0,
  0x78, 0x95, 0xe6, 0x78, 0x92, 0xf6, 0x22, 0x79, 0x90, 0xe7, 0xd3, 0x78, 0x93, 0x96, 0x40, 0x05, //0x82c0,
  0xe7, 0x96, 0xff, 0x80, 0x08, 0xc3, 0x79, 0x93, 0xe7, 0x78, 0x90, 0x96, 0xff, 0x78, 0x88, 0x76, //0x82d0,
  0x00, 0x08, 0xa6, 0x07, 0x79, 0x91, 0xe7, 0xd3, 0x78, 0x94, 0x96, 0x40, 0x05, 0xe7, 0x96, 0xff, //0x82e0,
  0x80, 0x08, 0xc3, 0x79, 0x94, 0xe7, 0x78, 0x91, 0x96, 0xff, 0x12, 0x0c, 0x8e, 0x79, 0x92, 0xe7, //0x82f0,
  0xd3, 0x78, 0x95, 0x96, 0x40, 0x05, 0xe7, 0x96, 0xff, 0x80, 0x08, 0xc3, 0x79, 0x95, 0xe7, 0x78, //0x8300,
  0x92, 0x96, 0xff, 0x12, 0x0c, 0x8e, 0x12, 0x0c, 0x5b, 0x78, 0x8a, 0xe6, 0x25, 0xe0, 0x24, 0x4e, //0x8310,
  0xf8, 0xa6, 0x06, 0x08, 0xa6, 0x07, 0x78, 0x8a, 0xe6, 0x24, 0x6e, 0xf8, 0xa6, 0x09, 0x78, 0x8a, //0x8320,
  0xe6, 0x24, 0x01, 0xff, 0xe4, 0x33, 0xfe, 0xd3, 0xef, 0x94, 0x0f, 0xee, 0x64, 0x80, 0x94, 0x80, //0x8330,
  0x40, 0x04, 0x7f, 0x00, 0x80, 0x05, 0x78, 0x8a, 0xe6, 0x04, 0xff, 0x78, 0x8a, 0xa6, 0x07, 0xe5, //0x8340,
  0x1f, 0xb4, 0x01, 0x0a, 0xe6, 0x60, 0x03, 0x02, 0x04, 0xc0, 0x75, 0x1f, 0x02, 0x22, 0x12, 0x0c, //0x8350,
  0x7a, 0x78, 0x80, 0xa6, 0x06, 0x08, 0xa6, 0x07, 0x12, 0x0c, 0x7a, 0x78, 0x82, 0xa6, 0x06, 0x08, //0x8360,
  0xa6, 0x07, 0x78, 0x6e, 0xe6, 0x78, 0x8c, 0xf6, 0x78, 0x6e, 0xe6, 0x78, 0x8d, 0xf6, 0x7f, 0x01, //0x8370,
  0xef, 0x25, 0xe0, 0x24, 0x4f, 0xf9, 0xc3, 0x78, 0x81, 0xe6, 0x97, 0x18, 0xe6, 0x19, 0x97, 0x50, //0x8380,
  0x0a, 0x12, 0x0c, 0x82, 0x78, 0x80, 0xa6, 0x04, 0x08, 0xa6, 0x05, 0x74, 0x6e, 0x2f, 0xf9, 0x78, //0x8390,
  0x8c, 0xe6, 0xc3, 0x97, 0x50, 0x08, 0x74, 0x6e, 0x2f, 0xf8, 0xe6, 0x78, 0x8c, 0xf6, 0xef, 0x25, //0x83a0,
  0xe0, 0x24, 0x4f, 0xf9, 0xd3, 0x78, 0x83, 0xe6, 0x97, 0x18, 0xe6, 0x19, 0x97, 0x40, 0x0a, 0x12, //0x83b0,
  0x0c, 0x82, 0x78, 0x82, 0xa6, 0x04, 0x08, 0xa6, 0x05, 0x74, 0x6e, 0x2f, 0xf9, 0x78, 0x8d, 0xe6, //0x83c0,
  0xd3, 0x97, 0x40, 0x08, 0x74, 0x6e, 0x2f, 0xf8, 0xe6, 0x78, 0x8d, 0xf6, 0x0f, 0xef, 0x64, 0x10, //0x83d0,
  0x70, 0x9e, 0xc3, 0x79, 0x81, 0xe7, 0x78, 0x83, 0x96, 0xff, 0x19, 0xe7, 0x18, 0x96, 0x78, 0x84, //0x83e0,
  0xf6, 0x08, 0xa6, 0x07, 0xc3, 0x79, 0x8c, 0xe7, 0x78, 0x8d, 0x96, 0x08, 0xf6, 0xd3, 0x79, 0x81, //0x83f0,
  0xe7, 0x78, 0x7f, 0x96, 0x19, 0xe7, 0x18, 0x96, 0x40, 0x05, 0x09, 0xe7, 0x08, 0x80, 0x06, 0xc3, //0x8400,
  0x79, 0x7f, 0xe7, 0x78, 0x81, 0x96, 0xff, 0x19, 0xe7, 0x18, 0x96, 0xfe, 0x78, 0x86, 0xa6, 0x06, //0x8410,
  0x08, 0xa6, 0x07, 0x79, 0x8c, 0xe7, 0xd3, 0x78, 0x8b, 0x96, 0x40, 0x05, 0xe7, 0x96, 0xff, 0x80, //0x8420,
  0x08, 0xc3, 0x79, 0x8b, 0xe7, 0x78, 0x8c, 0x96, 0xff, 0x78, 0x8f, 0xa6, 0x07, 0xe5, 0x1f, 0x64, //0x8430,
  0x02, 0x70, 0x69, 0x90, 0x0e, 0x91, 0x93, 0xff, 0x18, 0xe6, 0xc3, 0x9f, 0x50, 0x72, 0x12, 0x0c, //0x8440,
  0x4a, 0x12, 0x0c, 0x2f, 0x90, 0x0e, 0x8e, 0x12, 0x0c, 0x38, 0x78, 0x80, 0x12, 0x0c, 0x6b, 0x7b, //0x8450,
  0x04, 0x12, 0x0c, 0x1d, 0xc3, 0x12, 0x06, 0x45, 0x50, 0x56, 0x90, 0x0e, 0x92, 0xe4, 0x93, 0xff, //0x8460,
  0x78, 0x8f, 0xe6, 0x9f, 0x40, 0x02, 0x80, 0x11, 0x90, 0x0e, 0x90, 0xe4, 0x93, 0xff, 0xd3, 0x78, //0x8470,
  0x89, 0xe6, 0x9f, 0x18, 0xe6, 0x94, 0x00, 0x40, 0x03, 0x75, 0x1f, 0x05, 0x12, 0x0c, 0x4a, 0x12, //0x8480,
  0x0c, 0x2f, 0x90, 0x0e, 0x8f, 0x12, 0x0c, 0x38, 0x78, 0x7e, 0x12, 0x0c, 0x6b, 0x7b, 0x40, 0x12, //0x8490,
  0x0c, 0x1d, 0xd3, 0x12, 0x06, 0x45, 0x40, 0x18, 0x75, 0x1f, 0x05, 0x22, 0xe5, 0x1f, 0xb4, 0x05, //0x84a0,
  0x0f, 0xd2, 0x01, 0xc2, 0x02, 0xe4, 0xf5, 0x1f, 0xf5, 0x1e, 0xd2, 0x35, 0xd2, 0x33, 0xd2, 0x36, //0x84b0,
  0x22, 0xef, 0x8d, 0xf0, 0xa4, 0xa8, 0xf0, 0xcf, 0x8c, 0xf0, 0xa4, 0x28, 0xce, 0x8d, 0xf0, 0xa4, //0x84c0,
  0x2e, 0xfe, 0x22, 0xbc, 0x00, 0x0b, 0xbe, 0x00, 0x29, 0xef, 0x8d, 0xf0, 0x84, 0xff, 0xad, 0xf0, //0x84d0,
  0x22, 0xe4, 0xcc, 0xf8, 0x75, 0xf0, 0x08, 0xef, 0x2f, 0xff, 0xee, 0x33, 0xfe, 0xec, 0x33, 0xfc, //0x84e0,
  0xee, 0x9d, 0xec, 0x98, 0x40, 0x05, 0xfc, 0xee, 0x9d, 0xfe, 0x0f, 0xd5, 0xf0, 0xe9, 0xe4, 0xce, //0x84f0,
  0xfd, 0x22, 0xed, 0xf8, 0xf5, 0xf0, 0xee, 0x84, 0x20, 0xd2, 0x1c, 0xfe, 0xad, 0xf0, 0x75, 0xf0, //0x8500,
  0x08, 0xef, 0x2f, 0xff, 0xed, 0x33, 0xfd, 0x40, 0x07, 0x98, 0x50, 0x06, 0xd5, 0xf0, 0xf2, 0x22, //0x8510,
  0xc3, 0x98, 0xfd, 0x0f, 0xd5, 0xf0, 0xea, 0x22, 0xe8, 0x8f, 0xf0, 0xa4, 0xcc, 0x8b, 0xf0, 0xa4, //0x8520,
  0x2c, 0xfc, 0xe9, 0x8e, 0xf0, 0xa4, 0x2c, 0xfc, 0x8a, 0xf0, 0xed, 0xa4, 0x2c, 0xfc, 0xea, 0x8e, //0x8530,
  0xf0, 0xa4, 0xcd, 0xa8, 0xf0, 0x8b, 0xf0, 0xa4, 0x2d, 0xcc, 0x38, 0x25, 0xf0, 0xfd, 0xe9, 0x8f, //0x8540,
  0xf0, 0xa4, 0x2c, 0xcd, 0x35, 0xf0, 0xfc, 0xeb, 0x8e, 0xf0, 0xa4, 0xfe, 0xa9, 0xf0, 0xeb, 0x8f, //0x8550,
  0xf0, 0xa4, 0xcf, 0xc5, 0xf0, 0x2e, 0xcd, 0x39, 0xfe, 0xe4, 0x3c, 0xfc, 0xea, 0xa4, 0x2d, 0xce, //0x8560,
  0x35, 0xf0, 0xfd, 0xe4, 0x3c, 0xfc, 0x22, 0x75, 0xf0, 0x08, 0x75, 0x82, 0x00, 0xef, 0x2f, 0xff, //0x8570,
  0xee, 0x33, 0xfe, 0xcd, 0x33, 0xcd, 0xcc, 0x33, 0xcc, 0xc5, 0x82, 0x33, 0xc5, 0x82, 0x9b, 0xed, //0x8580,
  0x9a, 0xec, 0x99, 0xe5, 0x82, 0x98, 0x40, 0x0c, 0xf5, 0x82, 0xee, 0x9b, 0xfe, 0xed, 0x9a, 0xfd, //0x8590,
  0xec, 0x99, 0xfc, 0x0f, 0xd5, 0xf0, 0xd6, 0xe4, 0xce, 0xfb, 0xe4, 0xcd, 0xfa, 0xe4, 0xcc, 0xf9, //0x85a0,
  0xa8, 0x82, 0x22, 0xb8, 0x00, 0xc1, 0xb9, 0x00, 0x59, 0xba, 0x00, 0x2d, 0xec, 0x8b, 0xf0, 0x84, //0x85b0,
  0xcf, 0xce, 0xcd, 0xfc, 0xe5, 0xf0, 0xcb, 0xf9, 0x78, 0x18, 0xef, 0x2f, 0xff, 0xee, 0x33, 0xfe, //0x85c0,
  0xed, 0x33, 0xfd, 0xec, 0x33, 0xfc, 0xeb, 0x33, 0xfb, 0x10, 0xd7, 0x03, 0x99, 0x40, 0x04, 0xeb, //0x85d0,
  0x99, 0xfb, 0x0f, 0xd8, 0xe5, 0xe4, 0xf9, 0xfa, 0x22, 0x78, 0x18, 0xef, 0x2f, 0xff, 0xee, 0x33, //0x85e0,
  0xfe, 0xed, 0x33, 0xfd, 0xec, 0x33, 0xfc, 0xc9, 0x33, 0xc9, 0x10, 0xd7, 0x05, 0x9b, 0xe9, 0x9a, //0x85f0,
  0x40, 0x07, 0xec, 0x9b, 0xfc, 0xe9, 0x9a, 0xf9, 0x0f, 0xd8, 0xe0, 0xe4, 0xc9, 0xfa, 0xe4, 0xcc, //0x8600,
  0xfb, 0x22, 0x75, 0xf0, 0x10, 0xef, 0x2f, 0xff, 0xee, 0x33, 0xfe, 0xed, 0x33, 0xfd, 0xcc, 0x33, //0x8610,
  0xcc, 0xc8, 0x33, 0xc8, 0x10, 0xd7, 0x07, 0x9b, 0xec, 0x9a, 0xe8, 0x99, 0x40, 0x0a, 0xed, 0x9b, //0x8620,
  0xfd, 0xec, 0x9a, 0xfc, 0xe8, 0x99, 0xf8, 0x0f, 0xd5, 0xf0, 0xda, 0xe4, 0xcd, 0xfb, 0xe4, 0xcc, //0x8630,
  0xfa, 0xe4, 0xc8, 0xf9, 0x22, 0xeb, 0x9f, 0xf5, 0xf0, 0xea, 0x9e, 0x42, 0xf0, 0xe9, 0x9d, 0x42, //0x8640,
  0xf0, 0xe8, 0x9c, 0x45, 0xf0, 0x22, 0xe8, 0x60, 0x0f, 0xec, 0xc3, 0x13, 0xfc, 0xed, 0x13, 0xfd, //0x8650,
  0xee, 0x13, 0xfe, 0xef, 0x13, 0xff, 0xd8, 0xf1, 0x22, 0xe8, 0x60, 0x0f, 0xef, 0xc3, 0x33, 0xff, //0x8660,
  0xee, 0x33, 0xfe, 0xed, 0x33, 0xfd, 0xec, 0x33, 0xfc, 0xd8, 0xf1, 0x22, 0xe4, 0x93, 0xfc, 0x74, //0x8670,
  0x01, 0x93, 0xfd, 0x74, 0x02, 0x93, 0xfe, 0x74, 0x03, 0x93, 0xff, 0x22, 0xe6, 0xfb, 0x08, 0xe6, //0x8680,
  0xf9, 0x08, 0xe6, 0xfa, 0x08, 0xe6, 0xcb, 0xf8, 0x22, 0xec, 0xf6, 0x08, 0xed, 0xf6, 0x08, 0xee, //0x8690,
  0xf6, 0x08, 0xef, 0xf6, 0x22, 0xa4, 0x25, 0x82, 0xf5, 0x82, 0xe5, 0xf0, 0x35, 0x83, 0xf5, 0x83, //0x86a0,
  0x22, 0xd0, 0x83, 0xd0, 0x82, 0xf8, 0xe4, 0x93, 0x70, 0x12, 0x74, 0x01, 0x93, 0x70, 0x0d, 0xa3, //0x86b0,
  0xa3, 0x93, 0xf8, 0x74, 0x01, 0x93, 0xf5, 0x82, 0x88, 0x83, 0xe4, 0x73, 0x74, 0x02, 0x93, 0x68, //0x86c0,
  0x60, 0xef, 0xa3, 0xa3, 0xa3, 0x80, 0xdf, 0x90, 0x38, 0x04, 0x78, 0x52, 0x12, 0x0b, 0xfd, 0x90, //0x86d0,
  0x38, 0x00, 0xe0, 0xfe, 0xa3, 0xe0, 0xfd, 0xed, 0xff, 0xc3, 0x12, 0x0b, 0x9e, 0x90, 0x38, 0x10, //0x86e0,
  0x12, 0x0b, 0x92, 0x90, 0x38, 0x06, 0x78, 0x54, 0x12, 0x0b, 0xfd, 0x90, 0x38, 0x02, 0xe0, 0xfe, //0x86f0,
  0xa3, 0xe0, 0xfd, 0xed, 0xff, 0xc3, 0x12, 0x0b, 0x9e, 0x90, 0x38, 0x12, 0x12, 0x0b, 0x92, 0xa3, //0x8700,
  0xe0, 0xb4, 0x31, 0x07, 0x78, 0x52, 0x79, 0x52, 0x12, 0x0c, 0x13, 0x90, 0x38, 0x14, 0xe0, 0xb4, //0x8710,
  0x71, 0x15, 0x78, 0x52, 0xe6, 0xfe, 0x08, 0xe6, 0x78, 0x02, 0xce, 0xc3, 0x13, 0xce, 0x13, 0xd8, //0x8720,
  0xf9, 0x79, 0x53, 0xf7, 0xee, 0x19, 0xf7, 0x90, 0x38, 0x15, 0xe0, 0xb4, 0x31, 0x07, 0x78, 0x54, //0x8730,
  0x79, 0x54, 0x12, 0x0c, 0x13, 0x90, 0x38, 0x15, 0xe0, 0xb4, 0x71, 0x15, 0x78, 0x54, 0xe6, 0xfe, //0x8740,
  0x08, 0xe6, 0x78, 0x02, 0xce, 0xc3, 0x13, 0xce, 0x13, 0xd8, 0xf9, 0x79, 0x55, 0xf7, 0xee, 0x19, //0x8750,
  0xf7, 0x79, 0x52, 0x12, 0x0b, 0xd9, 0x09, 0x12, 0x0b, 0xd9, 0xaf, 0x47, 0x12, 0x0b, 0xb2, 0xe5, //0x8760,
  0x44, 0xfb, 0x7a, 0x00, 0xfd, 0x7c, 0x00, 0x12, 0x04, 0xd3, 0x78, 0x5a, 0xa6, 0x06, 0x08, 0xa6, //0x8770,
  0x07, 0xaf, 0x45, 0x12, 0x0b, 0xb2, 0xad, 0x03, 0x7c, 0x00, 0x12, 0x04, 0xd3, 0x78, 0x56, 0xa6, //0x8780,
  0x06, 0x08, 0xa6, 0x07, 0xaf, 0x48, 0x78, 0x54, 0x12, 0x0b, 0xb4, 0xe5, 0x43, 0xfb, 0xfd, 0x7c, //0x8790,
  0x00, 0x12, 0x04, 0xd3, 0x78, 0x5c, 0xa6, 0x06, 0x08, 0xa6, 0x07, 0xaf, 0x46, 0x7e, 0x00, 0x78, //0x87a0,
  0x54, 0x12, 0x0b, 0xb6, 0xad, 0x03, 0x7c, 0x00, 0x12, 0x04, 0xd3, 0x78, 0x58, 0xa6, 0x06, 0x08, //0x87b0,
  0xa6, 0x07, 0xc3, 0x78, 0x5b, 0xe6, 0x94, 0x08, 0x18, 0xe6, 0x94, 0x00, 0x50, 0x05, 0x76, 0x00, //0x87c0,
  0x08, 0x76, 0x08, 0xc3, 0x78, 0x5d, 0xe6, 0x94, 0x08, 0x18, 0xe6, 0x94, 0x00, 0x50, 0x05, 0x76, //0x87d0,
  0x00, 0x08, 0x76, 0x08, 0x78, 0x5a, 0x12, 0x0b, 0xc6, 0xff, 0xd3, 0x78, 0x57, 0xe6, 0x9f, 0x18, //0x87e0,
  0xe6, 0x9e, 0x40, 0x0e, 0x78, 0x5a, 0xe6, 0x13, 0xfe, 0x08, 0xe6, 0x78, 0x57, 0x12, 0x0c, 0x08, //0x87f0,
  0x80, 0x04, 0x7e, 0x00, 0x7f, 0x00, 0x78, 0x5e, 0x12, 0x0b, 0xbe, 0xff, 0xd3, 0x78, 0x59, 0xe6, //0x8800,
  0x9f, 0x18, 0xe6, 0x9e, 0x40, 0x0e, 0x78, 0x5c, 0xe6, 0x13, 0xfe, 0x08, 0xe6, 0x78, 0x59, 0x12, //0x8810,
  0x0c, 0x08, 0x80, 0x04, 0x7e, 0x00, 0x7f, 0x00, 0xe4, 0xfc, 0xfd, 0x78, 0x62, 0x12, 0x06, 0x99, //0x8820,
  0x78, 0x5a, 0x12, 0x0b, 0xc6, 0x78, 0x57, 0x26, 0xff, 0xee, 0x18, 0x36, 0xfe, 0x78, 0x66, 0x12, //0x8830,
  0x0b, 0xbe, 0x78, 0x59, 0x26, 0xff, 0xee, 0x18, 0x36, 0xfe, 0xe4, 0xfc, 0xfd, 0x78, 0x6a, 0x12, //0x8840,
  0x06, 0x99, 0x12, 0x0b, 0xce, 0x78, 0x66, 0x12, 0x06, 0x8c, 0xd3, 0x12, 0x06, 0x45, 0x40, 0x08, //0x8850,
  0x12, 0x0b, 0xce, 0x78, 0x66, 0x12, 0x06, 0x99, 0x78, 0x54, 0x12, 0x0b, 0xd0, 0x78, 0x6a, 0x12, //0x8860,
  0x06, 0x8c, 0xd3, 0x12, 0x06, 0x45, 0x40, 0x0a, 0x78, 0x54, 0x12, 0x0b, 0xd0, 0x78, 0x6a, 0x12, //0x8870,
  0x06, 0x99, 0x78, 0x61, 0xe6, 0x90, 0x60, 0x01, 0xf0, 0x78, 0x65, 0xe6, 0xa3, 0xf0, 0x78, 0x69, //0x8880,
  0xe6, 0xa3, 0xf0, 0x78, 0x55, 0xe6, 0xa3, 0xf0, 0x7d, 0x01, 0x78, 0x61, 0x12, 0x0b, 0xe9, 0x24, //0x8890,
  0x01, 0x12, 0x0b, 0xa6, 0x78, 0x65, 0x12, 0x0b, 0xe9, 0x24, 0x02, 0x12, 0x0b, 0xa6, 0x78, 0x69, //0x88a0,
  0x12, 0x0b, 0xe9, 0x24, 0x03, 0x12, 0x0b, 0xa6, 0x78, 0x6d, 0x12, 0x0b, 0xe9, 0x24, 0x04, 0x12, //0x88b0,
  0x0b, 0xa6, 0x0d, 0xbd, 0x05, 0xd4, 0xc2, 0x0e, 0xc2, 0x06, 0x22, 0x85, 0x08, 0x41, 0x90, 0x30, //0x88c0,
  0x24, 0xe0, 0xf5, 0x3d, 0xa3, 0xe0, 0xf5, 0x3e, 0xa3, 0xe0, 0xf5, 0x3f, 0xa3, 0xe0, 0xf5, 0x40, //0x88d0,
  0xa3, 0xe0, 0xf5, 0x3c, 0xd2, 0x34, 0xe5, 0x41, 0x12, 0x06, 0xb1, 0x09, 0x31, 0x03, 0x09, 0x35, //0x88e0,
  0x04, 0x09, 0x3b, 0x05, 0x09, 0x3e, 0x06, 0x09, 0x41, 0x07, 0x09, 0x4a, 0x08, 0x09, 0x5b, 0x12, //0x88f0,
  0x09, 0x73, 0x18, 0x09, 0x89, 0x19, 0x09, 0x5e, 0x1a, 0x09, 0x6a, 0x1b, 0x09, 0xad, 0x80, 0x09, //0x8900,
  0xb2, 0x81, 0x0a, 0x1d, 0x8f, 0x0a, 0x09, 0x90, 0x0a, 0x1d, 0x91, 0x0a, 0x1d, 0x92, 0x0a, 0x1d, //0x8910,
  0x93, 0x0a, 0x1d, 0x94, 0x0a, 0x1d, 0x98, 0x0a, 0x17, 0x9f, 0x0a, 0x1a, 0xec, 0x00, 0x00, 0x0a, //0x8920,
  0x38, 0x12, 0x0f, 0x74, 0x22, 0x12, 0x0f, 0x74, 0xd2, 0x03, 0x22, 0xd2, 0x03, 0x22, 0xc2, 0x03, //0x8930,
  0x22, 0xa2, 0x37, 0xe4, 0x33, 0xf5, 0x3c, 0x02, 0x0a, 0x1d, 0xc2, 0x01, 0xc2, 0x02, 0xc2, 0x03, //0x8940,
  0x12, 0x0d, 0x0d, 0x75, 0x1e, 0x70, 0xd2, 0x35, 0x02, 0x0a, 0x1d, 0x02, 0x0a, 0x04, 0x85, 0x40, //0x8950,
  0x4a, 0x85, 0x3c, 0x4b, 0x12, 0x0a, 0xff, 0x02, 0x0a, 0x1d, 0x85, 0x4a, 0x40, 0x85, 0x4b, 0x3c, //0x8960,
  0x02, 0x0a, 0x1d, 0xe4, 0xf5, 0x22, 0xf5, 0x23, 0x85, 0x40, 0x31, 0x85, 0x3f, 0x30, 0x85, 0x3e, //0x8970,
  0x2f, 0x85, 0x3d, 0x2e, 0x12, 0x0f, 0x46, 0x80, 0x1f, 0x75, 0x22, 0x00, 0x75, 0x23, 0x01, 0x74, //0x8980,
  0xff, 0xf5, 0x2d, 0xf5, 0x2c, 0xf5, 0x2b, 0xf5, 0x2a, 0x12, 0x0f, 0x46, 0x85, 0x2d, 0x40, 0x85, //0x8990,
  0x2c, 0x3f, 0x85, 0x2b, 0x3e, 0x85, 0x2a, 0x3d, 0xe4, 0xf5, 0x3c, 0x80, 0x70, 0x12, 0x0f, 0x16, //0x89a0,
  0x80, 0x6b, 0x85, 0x3d, 0x45, 0x85, 0x3e, 0x46, 0xe5, 0x47, 0xc3, 0x13, 0xff, 0xe5, 0x45, 0xc3, //0x89b0,
  0x9f, 0x50, 0x02, 0x8f, 0x45, 0xe5, 0x48, 0xc3, 0x13, 0xff, 0xe5, 0x46, 0xc3, 0x9f, 0x50, 0x02, //0x89c0,
  0x8f, 0x46, 0xe5, 0x47, 0xc3, 0x13, 0xff, 0xfd, 0xe5, 0x45, 0x2d, 0xfd, 0xe4, 0x33, 0xfc, 0xe5, //0x89d0,
  0x44, 0x12, 0x0f, 0x90, 0x40, 0x05, 0xe5, 0x44, 0x9f, 0xf5, 0x45, 0xe5, 0x48, 0xc3, 0x13, 0xff, //0x89e0,
  0xfd, 0xe5, 0x46, 0x2d, 0xfd, 0xe4, 0x33, 0xfc, 0xe5, 0x43, 0x12, 0x0f, 0x90, 0x40, 0x05, 0xe5, //0x89f0,
  0x43, 0x9f, 0xf5, 0x46, 0x12, 0x06, 0xd7, 0x80, 0x14, 0x85, 0x40, 0x48, 0x85, 0x3f, 0x47, 0x85, //0x8a00,
  0x3e, 0x46, 0x85, 0x3d, 0x45, 0x80, 0x06, 0x02, 0x06, 0xd7, 0x12, 0x0d, 0x7e, 0x90, 0x30, 0x24, //0x8a10,
  0xe5, 0x3d, 0xf0, 0xa3, 0xe5, 0x3e, 0xf0, 0xa3, 0xe5, 0x3f, 0xf0, 0xa3, 0xe5, 0x40, 0xf0, 0xa3, //0x8a20,
  0xe5, 0x3c, 0xf0, 0x90, 0x30, 0x23, 0xe4, 0xf0, 0x22, 0xc0, 0xe0, 0xc0, 0x83, 0xc0, 0x82, 0xc0, //0x8a30,
  0xd0, 0x90, 0x3f, 0x0c, 0xe0, 0xf5, 0x32, 0xe5, 0x32, 0x30, 0xe3, 0x74, 0x30, 0x36, 0x66, 0x90, //0x8a40,
  0x60, 0x19, 0xe0, 0xf5, 0x0a, 0xa3, 0xe0, 0xf5, 0x0b, 0x90, 0x60, 0x1d, 0xe0, 0xf5, 0x14, 0xa3, //0x8a50,
  0xe0, 0xf5, 0x15, 0x90, 0x60, 0x21, 0xe0, 0xf5, 0x0c, 0xa3, 0xe0, 0xf5, 0x0d, 0x90, 0x60, 0x29, //0x8a60,
  0xe0, 0xf5, 0x0e, 0xa3, 0xe0, 0xf5, 0x0f, 0x90, 0x60, 0x31, 0xe0, 0xf5, 0x10, 0xa3, 0xe0, 0xf5, //0x8a70,
  0x11, 0x90, 0x60, 0x39, 0xe0, 0xf5, 0x12, 0xa3, 0xe0, 0xf5, 0x13, 0x30, 0x01, 0x06, 0x30, 0x33, //0x8a80,
  0x03, 0xd3, 0x80, 0x01, 0xc3, 0x92, 0x09, 0x30, 0x02, 0x06, 0x30, 0x33, 0x03, 0xd3, 0x80, 0x01, //0x8a90,
  0xc3, 0x92, 0x0a, 0x30, 0x33, 0x0c, 0x30, 0x03, 0x09, 0x20, 0x02, 0x06, 0x20, 0x01, 0x03, 0xd3, //0x8aa0,
  0x80, 0x01, 0xc3, 0x92, 0x0b, 0x90, 0x30, 0x01, 0xe0, 0x44, 0x40, 0xf0, 0xe0, 0x54, 0xbf, 0xf0, //0x8ab0,
  0xe5, 0x32, 0x30, 0xe1, 0x14, 0x30, 0x34, 0x11, 0x90, 0x30, 0x22, 0xe0, 0xf5, 0x08, 0xe4, 0xf0, //0x8ac0,
  0x30, 0x00, 0x03, 0xd3, 0x80, 0x01, 0xc3, 0x92, 0x08, 0xe5, 0x32, 0x30, 0xe5, 0x12, 0x90, 0x56, //0x8ad0,
  0xa1, 0xe0, 0xf5, 0x09, 0x30, 0x31, 0x09, 0x30, 0x05, 0x03, 0xd3, 0x80, 0x01, 0xc3, 0x92, 0x0d, //0x8ae0,
  0x90, 0x3f, 0x0c, 0xe5, 0x32, 0xf0, 0xd0, 0xd0, 0xd0, 0x82, 0xd0, 0x83, 0xd0, 0xe0, 0x32, 0x90, //0x8af0,
  0x0e, 0x7e, 0xe4, 0x93, 0xfe, 0x74, 0x01, 0x93, 0xff, 0xc3, 0x90, 0x0e, 0x7c, 0x74, 0x01, 0x93, //0x8b00,
  0x9f, 0xff, 0xe4, 0x93, 0x9e, 0xfe, 0xe4, 0x8f, 0x3b, 0x8e, 0x3a, 0xf5, 0x39, 0xf5, 0x38, 0xab, //0x8b10,
  0x3b, 0xaa, 0x3a, 0xa9, 0x39, 0xa8, 0x38, 0xaf, 0x4b, 0xfc, 0xfd, 0xfe, 0x12, 0x05, 0x28, 0x12, //0x8b20,
  0x0d, 0xe1, 0xe4, 0x7b, 0xff, 0xfa, 0xf9, 0xf8, 0x12, 0x05, 0xb3, 0x12, 0x0d, 0xe1, 0x90, 0x0e, //0x8b30,
  0x69, 0xe4, 0x12, 0x0d, 0xf6, 0x12, 0x0d, 0xe1, 0xe4, 0x85, 0x4a, 0x37, 0xf5, 0x36, 0xf5, 0x35, //0x8b40,
  0xf5, 0x34, 0xaf, 0x37, 0xae, 0x36, 0xad, 0x35, 0xac, 0x34, 0xa3, 0x12, 0x0d, 0xf6, 0x8f, 0x37, //0x8b50,
  0x8e, 0x36, 0x8d, 0x35, 0x8c, 0x34, 0xe5, 0x3b, 0x45, 0x37, 0xf5, 0x3b, 0xe5, 0x3a, 0x45, 0x36, //0x8b60,
  0xf5, 0x3a, 0xe5, 0x39, 0x45, 0x35, 0xf5, 0x39, 0xe5, 0x38, 0x45, 0x34, 0xf5, 0x38, 0xe4, 0xf5, //0x8b70,
  0x22, 0xf5, 0x23, 0x85, 0x3b, 0x31, 0x85, 0x3a, 0x30, 0x85, 0x39, 0x2f, 0x85, 0x38, 0x2e, 0x02, //0x8b80,
  0x0f, 0x46, 0xe0, 0xa3, 0xe0, 0x75, 0xf0, 0x02, 0xa4, 0xff, 0xae, 0xf0, 0xc3, 0x08, 0xe6, 0x9f, //0x8b90,
  0xf6, 0x18, 0xe6, 0x9e, 0xf6, 0x22, 0xff, 0xe5, 0xf0, 0x34, 0x60, 0x8f, 0x82, 0xf5, 0x83, 0xec, //0x8ba0,
  0xf0, 0x22, 0x78, 0x52, 0x7e, 0x00, 0xe6, 0xfc, 0x08, 0xe6, 0xfd, 0x02, 0x04, 0xc1, 0xe4, 0xfc, //0x8bb0,
  0xfd, 0x12, 0x06, 0x99, 0x78, 0x5c, 0xe6, 0xc3, 0x13, 0xfe, 0x08, 0xe6, 0x13, 0x22, 0x78, 0x52, //0x8bc0,
  0xe6, 0xfe, 0x08, 0xe6, 0xff, 0xe4, 0xfc, 0xfd, 0x22, 0xe7, 0xc4, 0xf8, 0x54, 0xf0, 0xc8, 0x68, //0x8bd0,
  0xf7, 0x09, 0xe7, 0xc4, 0x54, 0x0f, 0x48, 0xf7, 0x22, 0xe6, 0xfc, 0xed, 0x75, 0xf0, 0x04, 0xa4, //0x8be0,
  0x22, 0x12, 0x06, 0x7c, 0x8f, 0x48, 0x8e, 0x47, 0x8d, 0x46, 0x8c, 0x45, 0x22, 0xe0, 0xfe, 0xa3, //0x8bf0,
  0xe0, 0xfd, 0xee, 0xf6, 0xed, 0x08, 0xf6, 0x22, 0x13, 0xff, 0xc3, 0xe6, 0x9f, 0xff, 0x18, 0xe6, //0x8c00,
  0x9e, 0xfe, 0x22, 0xe6, 0xc3, 0x13, 0xf7, 0x08, 0xe6, 0x13, 0x09, 0xf7, 0x22, 0xad, 0x39, 0xac, //0x8c10,
  0x38, 0xfa, 0xf9, 0xf8, 0x12, 0x05, 0x28, 0x8f, 0x3b, 0x8e, 0x3a, 0x8d, 0x39, 0x8c, 0x38, 0xab, //0x8c20,
  0x37, 0xaa, 0x36, 0xa9, 0x35, 0xa8, 0x34, 0x22, 0x93, 0xff, 0xe4, 0xfc, 0xfd, 0xfe, 0x12, 0x05, //0x8c30,
  0x28, 0x8f, 0x37, 0x8e, 0x36, 0x8d, 0x35, 0x8c, 0x34, 0x22, 0x78, 0x84, 0xe6, 0xfe, 0x08, 0xe6, //0x8c40,
  0xff, 0xe4, 0x8f, 0x37, 0x8e, 0x36, 0xf5, 0x35, 0xf5, 0x34, 0x22, 0x90, 0x0e, 0x8c, 0xe4, 0x93, //0x8c50,
  0x25, 0xe0, 0x24, 0x0a, 0xf8, 0xe6, 0xfe, 0x08, 0xe6, 0xff, 0x22, 0xe6, 0xfe, 0x08, 0xe6, 0xff, //0x8c60,
  0xe4, 0x8f, 0x3b, 0x8e, 0x3a, 0xf5, 0x39, 0xf5, 0x38, 0x22, 0x78, 0x4e, 0xe6, 0xfe, 0x08, 0xe6, //0x8c70,
  0xff, 0x22, 0xef, 0x25, 0xe0, 0x24, 0x4e, 0xf8, 0xe6, 0xfc, 0x08, 0xe6, 0xfd, 0x22, 0x78, 0x89, //0x8c80,
  0xef, 0x26, 0xf6, 0x18, 0xe4, 0x36, 0xf6, 0x22, 0x75, 0x89, 0x03, 0x75, 0xa8, 0x01, 0x75, 0xb8, //0x8c90,
  0x04, 0x75, 0x34, 0xff, 0x75, 0x35, 0x0e, 0x75, 0x36, 0x15, 0x75, 0x37, 0x0d, 0x12, 0x0e, 0x9a, //0x8ca0,
  0x12, 0x00, 0x09, 0x12, 0x0f, 0x16, 0x12, 0x00, 0x06, 0xd2, 0x00, 0xd2, 0x34, 0xd2, 0xaf, 0x75, //0x8cb0,
  0x34, 0xff, 0x75, 0x35, 0x0e, 0x75, 0x36, 0x49, 0x75, 0x37, 0x03, 0x12, 0x0e, 0x9a, 0x30, 0x08, //0x8cc0,
  0x09, 0xc2, 0x34, 0x12, 0x08, 0xcb, 0xc2, 0x08, 0xd2, 0x34, 0x30, 0x0b, 0x09, 0xc2, 0x36, 0x12, //0x8cd0,
  0x02, 0x6c, 0xc2, 0x0b, 0xd2, 0x36, 0x30, 0x09, 0x09, 0xc2, 0x36, 0x12, 0x00, 0x0e, 0xc2, 0x09, //0x8ce0,
  0xd2, 0x36, 0x30, 0x0e, 0x03, 0x12, 0x06, 0xd7, 0x30, 0x35, 0xd3, 0x90, 0x30, 0x29, 0xe5, 0x1e, //0x8cf0,
  0xf0, 0xb4, 0x10, 0x05, 0x90, 0x30, 0x23, 0xe4, 0xf0, 0xc2, 0x35, 0x80, 0xc1, 0xe4, 0xf5, 0x4b, //0x8d00,
  0x90, 0x0e, 0x7a, 0x93, 0xff, 0xe4, 0x8f, 0x37, 0xf5, 0x36, 0xf5, 0x35, 0xf5, 0x34, 0xaf, 0x37, //0x8d10,
  0xae, 0x36, 0xad, 0x35, 0xac, 0x34, 0x90, 0x0e, 0x6a, 0x12, 0x0d, 0xf6, 0x8f, 0x37, 0x8e, 0x36, //0x8d20,
  0x8d, 0x35, 0x8c, 0x34, 0x90, 0x0e, 0x72, 0x12, 0x06, 0x7c, 0xef, 0x45, 0x37, 0xf5, 0x37, 0xee, //0x8d30,
  0x45, 0x36, 0xf5, 0x36, 0xed, 0x45, 0x35, 0xf5, 0x35, 0xec, 0x45, 0x34, 0xf5, 0x34, 0xe4, 0xf5, //0x8d40,
  0x22, 0xf5, 0x23, 0x85, 0x37, 0x31, 0x85, 0x36, 0x30, 0x85, 0x35, 0x2f, 0x85, 0x34, 0x2e, 0x12, //0x8d50,
  0x0f, 0x46, 0xe4, 0xf5, 0x22, 0xf5, 0x23, 0x90, 0x0e, 0x72, 0x12, 0x0d, 0xea, 0x12, 0x0f, 0x46, //0x8d60,
  0xe4, 0xf5, 0x22, 0xf5, 0x23, 0x90, 0x0e, 0x6e, 0x12, 0x0d, 0xea, 0x02, 0x0f, 0x46, 0xe5, 0x40, //0x8d70,
  0x24, 0xf2, 0xf5, 0x37, 0xe5, 0x3f, 0x34, 0x43, 0xf5, 0x36, 0xe5, 0x3e, 0x34, 0xa2, 0xf5, 0x35, //0x8d80,
  0xe5, 0x3d, 0x34, 0x28, 0xf5, 0x34, 0xe5, 0x37, 0xff, 0xe4, 0xfe, 0xfd, 0xfc, 0x78, 0x18, 0x12, //0x8d90,
  0x06, 0x69, 0x8f, 0x40, 0x8e, 0x3f, 0x8d, 0x3e, 0x8c, 0x3d, 0xe5, 0x37, 0x54, 0xa0, 0xff, 0xe5, //0x8da0,
  0x36, 0xfe, 0xe4, 0xfd, 0xfc, 0x78, 0x07, 0x12, 0x06, 0x56, 0x78, 0x10, 0x12, 0x0f, 0x9a, 0xe4, //0x8db0,
  0xff, 0xfe, 0xe5, 0x35, 0xfd, 0xe4, 0xfc, 0x78, 0x0e, 0x12, 0x06, 0x56, 0x12, 0x0f, 0x9d, 0xe4, //0x8dc0,
  0xff, 0xfe, 0xfd, 0xe5, 0x34, 0xfc, 0x78, 0x18, 0x12, 0x06, 0x56, 0x78, 0x08, 0x12, 0x0f, 0x9a, //0x8dd0,
  0x22, 0x8f, 0x3b, 0x8e, 0x3a, 0x8d, 0x39, 0x8c, 0x38, 0x22, 0x12, 0x06, 0x7c, 0x8f, 0x31, 0x8e, //0x8de0,
  0x30, 0x8d, 0x2f, 0x8c, 0x2e, 0x22, 0x93, 0xf9, 0xf8, 0x02, 0x06, 0x69, 0x00, 0x00, 0x00, 0x00, //0x8df0,
  0x12, 0x01, 0x17, 0x08, 0x31, 0x15, 0x53, 0x54, 0x44, 0x20, 0x20, 0x20, 0x20, 0x20, 0x13, 0x01, //0x8e00,
  0x10, 0x01, 0x56, 0x40, 0x1a, 0x30, 0x29, 0x7e, 0x00, 0x30, 0x04, 0x20, 0xdf, 0x30, 0x05, 0x40, //0x8e10,
  0xbf, 0x50, 0x03, 0x00, 0xfd, 0x50, 0x27, 0x01, 0xfe, 0x60, 0x00, 0x11, 0x00, 0x3f, 0x05, 0x30, //0x8e20,
  0x00, 0x3f, 0x06, 0x22, 0x00, 0x3f, 0x01, 0x2a, 0x00, 0x3f, 0x02, 0x00, 0x00, 0x36, 0x06, 0x07, //0x8e30,
  0x00, 0x3f, 0x0b, 0x0f, 0xf0, 0x00, 0x00, 0x00, 0x00, 0x30, 0x01, 0x40, 0xbf, 0x30, 0x01, 0x00, //0x8e40,
  0xbf, 0x30, 0x29, 0x70, 0x00, 0x3a, 0x00, 0x00, 0xff, 0x3a, 0x00, 0x00, 0xff, 0x36, 0x03, 0x36, //0x8e50,
  0x02, 0x41, 0x44, 0x58, 0x20, 0x18, 0x10, 0x0a, 0x04, 0x04, 0x00, 0x03, 0xff, 0x64, 0x00, 0x00, //0x8e60,
  0x80, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x02, 0x04, 0x06, 0x06, 0x00, 0x03, 0x51, 0x00, 0x7a, //0x8e70,
  0x50, 0x3c, 0x28, 0x1e, 0x10, 0x10, 0x50, 0x2d, 0x28, 0x16, 0x10, 0x10, 0x02, 0x00, 0x10, 0x0c, //0x8e80,
  0x10, 0x04, 0x0c, 0x6e, 0x06, 0x05, 0x00, 0xa5, 0x5a, 0x00, 0xae, 0x35, 0xaf, 0x36, 0xe4, 0xfd, //0x8e90,
  0xed, 0xc3, 0x95, 0x37, 0x50, 0x33, 0x12, 0x0f, 0xe2, 0xe4, 0x93, 0xf5, 0x38, 0x74, 0x01, 0x93, //0x8ea0,
  0xf5, 0x39, 0x45, 0x38, 0x60, 0x23, 0x85, 0x39, 0x82, 0x85, 0x38, 0x83, 0xe0, 0xfc, 0x12, 0x0f, //0x8eb0,
  0xe2, 0x74, 0x03, 0x93, 0x52, 0x04, 0x12, 0x0f, 0xe2, 0x74, 0x02, 0x93, 0x42, 0x04, 0x85, 0x39, //0x8ec0,
  0x82, 0x85, 0x38, 0x83, 0xec, 0xf0, 0x0d, 0x80, 0xc7, 0x22, 0x78, 0xbe, 0xe6, 0xd3, 0x08, 0xff, //0x8ed0,
  0xe6, 0x64, 0x80, 0xf8, 0xef, 0x64, 0x80, 0x98, 0x22, 0x93, 0xff, 0x7e, 0x00, 0xe6, 0xfc, 0x08, //0x8ee0,
  0xe6, 0xfd, 0x12, 0x04, 0xc1, 0x78, 0xc1, 0xe6, 0xfc, 0x08, 0xe6, 0xfd, 0xd3, 0xef, 0x9d, 0xee, //0x8ef0,
  0x9c, 0x22, 0x78, 0xbd, 0xd3, 0xe6, 0x64, 0x80, 0x94, 0x80, 0x22, 0x25, 0xe0, 0x24, 0x0a, 0xf8, //0x8f00,
  0xe6, 0xfe, 0x08, 0xe6, 0xff, 0x22, 0xe5, 0x3c, 0xd3, 0x94, 0x00, 0x40, 0x0b, 0x90, 0x0e, 0x88, //0x8f10,
  0x12, 0x0b, 0xf1, 0x90, 0x0e, 0x86, 0x80, 0x09, 0x90, 0x0e, 0x82, 0x12, 0x0b, 0xf1, 0x90, 0x0e, //0x8f20,
  0x80, 0xe4, 0x93, 0xf5, 0x44, 0xa3, 0xe4, 0x93, 0xf5, 0x43, 0xd2, 0x06, 0x30, 0x06, 0x03, 0xd3, //0x8f30,
  0x80, 0x01, 0xc3, 0x92, 0x0e, 0x22, 0xa2, 0xaf, 0x92, 0x32, 0xc2, 0xaf, 0xe5, 0x23, 0x45, 0x22, //0x8f40,
  0x90, 0x0e, 0x5d, 0x60, 0x0e, 0x12, 0x0f, 0xcb, 0xe0, 0xf5, 0x2c, 0x12, 0x0f, 0xc8, 0xe0, 0xf5, //0x8f50,
  0x2d, 0x80, 0x0c, 0x12, 0x0f, 0xcb, 0xe5, 0x30, 0xf0, 0x12, 0x0f, 0xc8, 0xe5, 0x31, 0xf0, 0xa2, //0x8f60,
  0x32, 0x92, 0xaf, 0x22, 0xd2, 0x01, 0xc2, 0x02, 0xe4, 0xf5, 0x1f, 0xf5, 0x1e, 0xd2, 0x35, 0xd2, //0x8f70,
  0x33, 0xd2, 0x36, 0xd2, 0x01, 0xc2, 0x02, 0xf5, 0x1f, 0xf5, 0x1e, 0xd2, 0x35, 0xd2, 0x33, 0x22, //0x8f80,
  0xfb, 0xd3, 0xed, 0x9b, 0x74, 0x80, 0xf8, 0x6c, 0x98, 0x22, 0x12, 0x06, 0x69, 0xe5, 0x40, 0x2f, //0x8f90,
  0xf5, 0x40, 0xe5, 0x3f, 0x3e, 0xf5, 0x3f, 0xe5, 0x3e, 0x3d, 0xf5, 0x3e, 0xe5, 0x3d, 0x3c, 0xf5, //0x8fa0,
  0x3d, 0x22, 0xc0, 0xe0, 0xc0, 0x83, 0xc0, 0x82, 0x90, 0x3f, 0x0d, 0xe0, 0xf5, 0x33, 0xe5, 0x33, //0x8fb0,
  0xf0, 0xd0, 0x82, 0xd0, 0x83, 0xd0, 0xe0, 0x32, 0x90, 0x0e, 0x5f, 0xe4, 0x93, 0xfe, 0x74, 0x01, //0x8fc0,
  0x93, 0xf5, 0x82, 0x8e, 0x83, 0x22, 0x78, 0x7f, 0xe4, 0xf6, 0xd8, 0xfd, 0x75, 0x81, 0xcd, 0x02, //0x8fd0,
  0x0c, 0x98, 0x8f, 0x82, 0x8e, 0x83, 0x75, 0xf0, 0x04, 0xed, 0x02, 0x06, 0xa5, //0x8fe0
};

/*
 * The white balance settings
 * Here only tune the R G B channel gain. 
 * The white balance enalbe bit is modified in sensor_s_autowb and sensor_s_wb
 */
static struct regval_list sensor_wb_manual[] = {  
  {0x3406,0x1 },
  //{REG_TERM,VAL_TERM},
};

static struct regval_list sensor_wb_auto_regs[] = {
  //simple awb
//  {0x3406,0x0},
//  {0x5183,0x94},
//  {0x5191,0xff},
//  {0x5192,0x00},

  //advanced awb
  {0x3406,0x00},
//  {0x5192,0x04},
//  {0x5191,0xf8},
//  {0x5193,0x70},
//  {0x5194,0xf0},
//  {0x5195,0xf0},
//  {0x518d,0x3d},
//  {0x518f,0x54},
//  {0x518e,0x3d},
//  {0x5190,0x54},
//  {0x518b,0xa8},
//  {0x518c,0xa8},
//  {0x5187,0x18},
//  {0x5188,0x18},
//  {0x5189,0x6e},
//  {0x518a,0x68},
//  {0x5186,0x1c},
//  {0x5181,0x50},
//  {0x5184,0x25},
//  {0x5182,0x11},
//  {0x5183,0x14},
//  {0x5184,0x25},
//  {0x5185,0x24},
  //{REG_TERM,VAL_TERM},
};

static struct regval_list sensor_wb_incandescence_regs[] = {
  //bai re guang
  {0x3406,0x1 },
  {0x3400,0x5 },
  {0x3401,0x48},
  {0x3402,0x4 },
  {0x3403,0x0 },
  {0x3404,0x7 },
  {0x3405,0xcf},
  //{REG_TERM,VAL_TERM},
};

static struct regval_list sensor_wb_fluorescent_regs[] = {
  //ri guang deng
  {0x3406,0x1 },
  {0x3400,0x5 },
  {0x3401,0x48},
  {0x3402,0x4 },
  {0x3403,0x0 },
  {0x3404,0x7 },
  {0x3405,0xcf},
  //{REG_TERM,VAL_TERM},
};

static struct regval_list sensor_wb_tungsten_regs[] = {
  //wu si deng
  {0x3406,0x1 },
  {0x3400,0x4 },
  {0x3401,0x10},
  {0x3402,0x4 },
  {0x3403,0x0 },
  {0x3404,0x8 },
  {0x3405,0xb6},
};

static struct regval_list sensor_wb_horizon[] = { 
//null
};

static struct regval_list sensor_wb_daylight_regs[] = {
  //tai yang guang
  {0x3406,0x1 },
  {0x3400,0x6 },
  {0x3401,0x1c},
  {0x3402,0x4 },
  {0x3403,0x0 },
  {0x3404,0x4 },
  {0x3405,0xf3},
  //{REG_TERM,VAL_TERM},
};

static struct regval_list sensor_wb_flash[] = { 
//null
};

static struct regval_list sensor_wb_cloud_regs[] = {  
  {0x3406,0x1 },
  {0x3400,0x6 },
  {0x3401,0x48},
  {0x3402,0x4 },
  {0x3403,0x0 },
  {0x3404,0x4 },
  {0x3405,0xd3},
  //{REG_TERM,VAL_TERM},
};

static struct regval_list sensor_wb_shade[] = { 
//null
};

static struct cfg_array sensor_wb[] = {
  { 
  	.regs = sensor_wb_manual,             //V4L2_WHITE_BALANCE_MANUAL       
    .size = ARRAY_SIZE(sensor_wb_manual),
  },
  {
  	.regs = sensor_wb_auto_regs,          //V4L2_WHITE_BALANCE_AUTO      
    .size = ARRAY_SIZE(sensor_wb_auto_regs),
  },
  {
  	.regs = sensor_wb_incandescence_regs, //V4L2_WHITE_BALANCE_INCANDESCENT 
    .size = ARRAY_SIZE(sensor_wb_incandescence_regs),
  },
  {
  	.regs = sensor_wb_fluorescent_regs,   //V4L2_WHITE_BALANCE_FLUORESCENT  
    .size = ARRAY_SIZE(sensor_wb_fluorescent_regs),
  },
  {
  	.regs = sensor_wb_tungsten_regs,      //V4L2_WHITE_BALANCE_FLUORESCENT_H
    .size = ARRAY_SIZE(sensor_wb_tungsten_regs),
  },
  {
  	.regs = sensor_wb_horizon,            //V4L2_WHITE_BALANCE_HORIZON    
    .size = ARRAY_SIZE(sensor_wb_horizon),
  },  
  {
  	.regs = sensor_wb_daylight_regs,      //V4L2_WHITE_BALANCE_DAYLIGHT     
    .size = ARRAY_SIZE(sensor_wb_daylight_regs),
  },
  {
  	.regs = sensor_wb_flash,              //V4L2_WHITE_BALANCE_FLASH        
    .size = ARRAY_SIZE(sensor_wb_flash),
  },
  {
  	.regs = sensor_wb_cloud_regs,         //V4L2_WHITE_BALANCE_CLOUDY       
    .size = ARRAY_SIZE(sensor_wb_cloud_regs),
  },
  {
  	.regs = sensor_wb_shade,              //V4L2_WHITE_BALANCE_SHADE  
    .size = ARRAY_SIZE(sensor_wb_shade),
  },
//  {
//  	.regs = NULL,
//    .size = 0,
//  },
};
                                          

/*
 * The color effect settings
 */
static struct regval_list sensor_colorfx_none_regs[] = {
//  {0x5001,0x7f},
  {0x5580,0x06},
	{0x5583,0x40},
  {0x5584,0x10},
  //{REG_TERM,VAL_TERM},
};

static struct regval_list sensor_colorfx_bw_regs[] = {
//  {0x5001,0x7f},
  {0x5580,0x1e},
  {0x5583,0x80},
  {0x5584,0x80},
  //{REG_TERM,VAL_TERM},
};

static struct regval_list sensor_colorfx_sepia_regs[] = {
//  {0x5001,0x7f},
  {0x5580,0x1e},
  {0x5583,0x40},
  {0x5584,0xa0},
  //{REG_TERM,VAL_TERM},
};

static struct regval_list sensor_colorfx_negative_regs[] = {
//  {0x5001,0x7f},
  {0x5580,0x46},
  //{REG_TERM,VAL_TERM},
};

static struct regval_list sensor_colorfx_emboss_regs[] = {
//  {0x5001,0x7f},
  {0x5580,0x1e},
  {0x5583,0x80},
  {0x5584,0xc0},
  //{REG_TERM,VAL_TERM},
};

static struct regval_list sensor_colorfx_sketch_regs[] = {
//  {0x5001,0x7f},
  {0x5580,0x1e},
  {0x5583,0x80},
  {0x5584,0xc0},
  //{REG_TERM,VAL_TERM},
};

static struct regval_list sensor_colorfx_sky_blue_regs[] = {
//  {0x5001,0x7f},
  {0x5580,0x1e},
  {0x5583,0xa0},
  {0x5584,0x40},
  //{REG_TERM,VAL_TERM},
};

static struct regval_list sensor_colorfx_grass_green_regs[] = {
//  {0x5001,0x7f},
  {0x5580,0x1e},
  {0x5583,0x60},
  {0x5584,0x60},
  //{REG_TERM,VAL_TERM},
};

static struct regval_list sensor_colorfx_skin_whiten_regs[] = {
//NULL
};

static struct regval_list sensor_colorfx_vivid_regs[] = {
//NULL
};

static struct regval_list sensor_colorfx_aqua_regs[] = {
//null
};

static struct regval_list sensor_colorfx_art_freeze_regs[] = {
//null
};

static struct regval_list sensor_colorfx_silhouette_regs[] = {
//null
};

static struct regval_list sensor_colorfx_solarization_regs[] = {
//null
};

static struct regval_list sensor_colorfx_antique_regs[] = {
//null
};

static struct regval_list sensor_colorfx_set_cbcr_regs[] = {
//null
};

static struct cfg_array sensor_colorfx[] = {
  {
  	.regs = sensor_colorfx_none_regs,         //V4L2_COLORFX_NONE = 0,         
    .size = ARRAY_SIZE(sensor_colorfx_none_regs),
  },
  {
  	.regs = sensor_colorfx_bw_regs,           //V4L2_COLORFX_BW   = 1,  
    .size = ARRAY_SIZE(sensor_colorfx_bw_regs),
  },
  {
  	.regs = sensor_colorfx_sepia_regs,        //V4L2_COLORFX_SEPIA  = 2,   
    .size = ARRAY_SIZE(sensor_colorfx_sepia_regs),
  },
  {
  	.regs = sensor_colorfx_negative_regs,     //V4L2_COLORFX_NEGATIVE = 3,     
    .size = ARRAY_SIZE(sensor_colorfx_negative_regs),
  },
  {
  	.regs = sensor_colorfx_emboss_regs,       //V4L2_COLORFX_EMBOSS = 4,       
    .size = ARRAY_SIZE(sensor_colorfx_emboss_regs),
  },
  {
  	.regs = sensor_colorfx_sketch_regs,       //V4L2_COLORFX_SKETCH = 5,       
    .size = ARRAY_SIZE(sensor_colorfx_sketch_regs),
  },
  {
  	.regs = sensor_colorfx_sky_blue_regs,     //V4L2_COLORFX_SKY_BLUE = 6,     
    .size = ARRAY_SIZE(sensor_colorfx_sky_blue_regs),
  },
  {
  	.regs = sensor_colorfx_grass_green_regs,  //V4L2_COLORFX_GRASS_GREEN = 7,  
    .size = ARRAY_SIZE(sensor_colorfx_grass_green_regs),
  },
  {
  	.regs = sensor_colorfx_skin_whiten_regs,  //V4L2_COLORFX_SKIN_WHITEN = 8,  
    .size = ARRAY_SIZE(sensor_colorfx_skin_whiten_regs),
  },
  {
  	.regs = sensor_colorfx_vivid_regs,        //V4L2_COLORFX_VIVID = 9,        
    .size = ARRAY_SIZE(sensor_colorfx_vivid_regs),
  },
  {
  	.regs = sensor_colorfx_aqua_regs,         //V4L2_COLORFX_AQUA = 10,        
    .size = ARRAY_SIZE(sensor_colorfx_aqua_regs),
  },
  {
  	.regs = sensor_colorfx_art_freeze_regs,   //V4L2_COLORFX_ART_FREEZE = 11,  
    .size = ARRAY_SIZE(sensor_colorfx_art_freeze_regs),
  },
  {
  	.regs = sensor_colorfx_silhouette_regs,   //V4L2_COLORFX_SILHOUETTE = 12,  
    .size = ARRAY_SIZE(sensor_colorfx_silhouette_regs),
  },
  {
  	.regs = sensor_colorfx_solarization_regs, //V4L2_COLORFX_SOLARIZATION = 13,
    .size = ARRAY_SIZE(sensor_colorfx_solarization_regs),
  },
  {
  	.regs = sensor_colorfx_antique_regs,      //V4L2_COLORFX_ANTIQUE = 14,     
    .size = ARRAY_SIZE(sensor_colorfx_antique_regs),
  },
  {
  	.regs = sensor_colorfx_set_cbcr_regs,     //V4L2_COLORFX_SET_CBCR = 15, 
    .size = ARRAY_SIZE(sensor_colorfx_set_cbcr_regs),
  },
};


#if 1
static struct regval_list sensor_sharpness_auto_regs[] = {
  {0x5308,0x25},
  {0x5300,0x08},
  {0x5301,0x30},
  {0x5302,0x10},
  {0x5303,0x00},
  {0x5304,0x08},
  {0x5305,0x30},
  {0x5306,0x08},
  {0x5307,0x16},
  {0x5309,0x08},
  {0x530a,0x30},
  {0x530b,0x04},
  {0x530c,0x06},
  //{REG_TERM,VAL_TERM},
};
#endif
#if 1
static struct regval_list sensor_denoise_auto_regs[] = {
  {0x5304,0x08}, 
	{0x5305,0x30}, 
	{0x5306,0x1c}, 
	{0x5307,0x2c},
  //{REG_TERM,VAL_TERM},
};
#endif

/*
 * The brightness setttings
 */
static struct regval_list sensor_brightness_neg4_regs[] = {
//NULL
};

static struct regval_list sensor_brightness_neg3_regs[] = {
//NULL
};

static struct regval_list sensor_brightness_neg2_regs[] = {
//NULL
};

static struct regval_list sensor_brightness_neg1_regs[] = {
//NULL
};

static struct regval_list sensor_brightness_zero_regs[] = {
//NULL
};

static struct regval_list sensor_brightness_pos1_regs[] = {
//NULL
};

static struct regval_list sensor_brightness_pos2_regs[] = {
//NULL
};

static struct regval_list sensor_brightness_pos3_regs[] = {
//NULL
};

static struct regval_list sensor_brightness_pos4_regs[] = {
//NULL
};

static struct cfg_array sensor_brightness[] = {
  {
  	.regs = sensor_brightness_neg4_regs,
  	.size = ARRAY_SIZE(sensor_brightness_neg4_regs),
  },
  {
  	.regs = sensor_brightness_neg3_regs,
  	.size = ARRAY_SIZE(sensor_brightness_neg3_regs),
  },
  {
  	.regs = sensor_brightness_neg2_regs,
  	.size = ARRAY_SIZE(sensor_brightness_neg2_regs),
  },
  {
  	.regs = sensor_brightness_neg1_regs,
  	.size = ARRAY_SIZE(sensor_brightness_neg1_regs),
  },
  {
  	.regs = sensor_brightness_zero_regs,
  	.size = ARRAY_SIZE(sensor_brightness_zero_regs),
  },
  {
  	.regs = sensor_brightness_pos1_regs,
  	.size = ARRAY_SIZE(sensor_brightness_pos1_regs),
  },
  {
  	.regs = sensor_brightness_pos2_regs,
  	.size = ARRAY_SIZE(sensor_brightness_pos2_regs),
  },
  {
  	.regs = sensor_brightness_pos3_regs,
  	.size = ARRAY_SIZE(sensor_brightness_pos3_regs),
  },
  {
  	.regs = sensor_brightness_pos4_regs,
  	.size = ARRAY_SIZE(sensor_brightness_pos4_regs),
  },
};

/*
 * The contrast setttings
 */
static struct regval_list sensor_contrast_neg4_regs[] = {
//NULL
};

static struct regval_list sensor_contrast_neg3_regs[] = {
//NULL
};

static struct regval_list sensor_contrast_neg2_regs[] = {
//NULL
};

static struct regval_list sensor_contrast_neg1_regs[] = {
//NULL
};

static struct regval_list sensor_contrast_zero_regs[] = {
//NULL
};

static struct regval_list sensor_contrast_pos1_regs[] = {
//NULL
};

static struct regval_list sensor_contrast_pos2_regs[] = {
//NULL
};

static struct regval_list sensor_contrast_pos3_regs[] = {
//NULL
};

static struct regval_list sensor_contrast_pos4_regs[] = {
//NULL
};

static struct cfg_array sensor_contrast[] = {
  {
  	.regs = sensor_contrast_neg4_regs,
  	.size = ARRAY_SIZE(sensor_contrast_neg4_regs),
  },
  {
  	.regs = sensor_contrast_neg3_regs,
  	.size = ARRAY_SIZE(sensor_contrast_neg3_regs),
  },
  {
  	.regs = sensor_contrast_neg2_regs,
  	.size = ARRAY_SIZE(sensor_contrast_neg2_regs),
  },
  {
  	.regs = sensor_contrast_neg1_regs,
  	.size = ARRAY_SIZE(sensor_contrast_neg1_regs),
  },
  {
  	.regs = sensor_contrast_zero_regs,
  	.size = ARRAY_SIZE(sensor_contrast_zero_regs),
  },
  {
  	.regs = sensor_contrast_pos1_regs,
  	.size = ARRAY_SIZE(sensor_contrast_pos1_regs),
  },
  {
  	.regs = sensor_contrast_pos2_regs,
  	.size = ARRAY_SIZE(sensor_contrast_pos2_regs),
  },
  {
  	.regs = sensor_contrast_pos3_regs,
  	.size = ARRAY_SIZE(sensor_contrast_pos3_regs),
  },
  {
  	.regs = sensor_contrast_pos4_regs,
  	.size = ARRAY_SIZE(sensor_contrast_pos4_regs),
  },
};

/*
 * The saturation setttings
 */
static struct regval_list sensor_saturation_neg4_regs[] = {
//NULL
};

static struct regval_list sensor_saturation_neg3_regs[] = {
//NULL
};

static struct regval_list sensor_saturation_neg2_regs[] = {
//NULL
};

static struct regval_list sensor_saturation_neg1_regs[] = {
//NULL
};

static struct regval_list sensor_saturation_zero_regs[] = {
//NULL
};

static struct regval_list sensor_saturation_pos1_regs[] = {
//NULL
};

static struct regval_list sensor_saturation_pos2_regs[] = {
//NULL
};

static struct regval_list sensor_saturation_pos3_regs[] = {
//NULL
};

static struct regval_list sensor_saturation_pos4_regs[] = {
//NULL
};

static struct cfg_array sensor_saturation[] = {
  {
  	.regs = sensor_saturation_neg4_regs,
  	.size = ARRAY_SIZE(sensor_saturation_neg4_regs),
  },
  {
  	.regs = sensor_saturation_neg3_regs,
  	.size = ARRAY_SIZE(sensor_saturation_neg3_regs),
  },
  {
  	.regs = sensor_saturation_neg2_regs,
  	.size = ARRAY_SIZE(sensor_saturation_neg2_regs),
  },
  {
  	.regs = sensor_saturation_neg1_regs,
  	.size = ARRAY_SIZE(sensor_saturation_neg1_regs),
  },
  {
  	.regs = sensor_saturation_zero_regs,
  	.size = ARRAY_SIZE(sensor_saturation_zero_regs),
  },
  {
  	.regs = sensor_saturation_pos1_regs,
  	.size = ARRAY_SIZE(sensor_saturation_pos1_regs),
  },
  {
  	.regs = sensor_saturation_pos2_regs,
  	.size = ARRAY_SIZE(sensor_saturation_pos2_regs),
  },
  {
  	.regs = sensor_saturation_pos3_regs,
  	.size = ARRAY_SIZE(sensor_saturation_pos3_regs),
  },
  {
  	.regs = sensor_saturation_pos4_regs,
  	.size = ARRAY_SIZE(sensor_saturation_pos4_regs),
  },
};

/*
 * The exposure target setttings
 */
#if 0 
static struct regval_list sensor_ev_neg4_regs[] = {
  {0x3a0f,0x10},  //-1.7EV
  {0x3a10,0x08},
  {0x3a1b,0x10},
  {0x3a1e,0x08},
  {0x3a11,0x20},
  {0x3a1f,0x10},
  //{REG_TERM,VAL_TERM},
};

static struct regval_list sensor_ev_neg3_regs[] = {
  {0x3a0f,0x18},  //-1.3EV
  {0x3a10,0x10},
  {0x3a1b,0x18},
  {0x3a1e,0x10},
  {0x3a11,0x30},
  {0x3a1f,0x10},
  //{REG_TERM,VAL_TERM},
};

static struct regval_list sensor_ev_neg2_regs[] = {
  {0x3a0f,0x20},  //-1.0EV
  {0x3a10,0x18},
  {0x3a1b,0x20},
  {0x3a1e,0x18},
  {0x3a11,0x41},
  {0x3a1f,0x10},
  //{REG_TERM,VAL_TERM},
};

static struct regval_list sensor_ev_neg1_regs[] = {
  {0x3a0f,0x30},  //-0.7EV
  {0x3a10,0x28},
  {0x3a1b,0x30},
  {0x3a1e,0x28},
  {0x3a11,0x51},
  {0x3a1f,0x10},
  //{REG_TERM,VAL_TERM},
};                     

static struct regval_list sensor_ev_zero_regs[] = {
  {0x3a0f,0x38},    //default
  {0x3a10,0x30},
  {0x3a1b,0x38},
  {0x3a1e,0x30},
  {0x3a11,0x61},
  {0x3a1f,0x10},
  //{REG_TERM,VAL_TERM},
};

static struct regval_list sensor_ev_pos1_regs[] = {
  {0x3a0f,0x48},  //0.7EV
  {0x3a10,0x40},
  {0x3a1b,0x48},
  {0x3a1e,0x40},
  {0x3a11,0x80},
  {0x3a1f,0x20},
  //{REG_TERM,VAL_TERM},
};

static struct regval_list sensor_ev_pos2_regs[] = {
  {0x3a0f,0x50},  //1.0EV
  {0x3a10,0x48},
  {0x3a1b,0x50},
  {0x3a1e,0x48},
  {0x3a11,0x90},
  {0x3a1f,0x20},
  //{REG_TERM,VAL_TERM},
};

static struct regval_list sensor_ev_pos3_regs[] = {
  {0x3a0f,0x58},  //1.3EV
  {0x3a10,0x50},
  {0x3a1b,0x58},
  {0x3a1e,0x50},
  {0x3a11,0x91},
  {0x3a1f,0x20},
  //{REG_TERM,VAL_TERM},
};

static struct regval_list sensor_ev_pos4_regs[] = {
  {0x3a0f,0x60},  //1.7EV
  {0x3a10,0x58},
  {0x3a1b,0x60},
  {0x3a1e,0x58},
  {0x3a11,0xa0},
  {0x3a1f,0x20},
  //{REG_TERM,VAL_TERM}, 
}; 
#else
static struct regval_list sensor_ev_neg4_regs[] = {
  {0x3a0f,0x10},  //-1.7EV
  {0x3a10,0x08},
  {0x3a1b,0x10},
  {0x3a1e,0x08},
  {0x3a11,0x20},
  {0x3a1f,0x10},
  //{REG_TERM,VAL_TERM},
};

static struct regval_list sensor_ev_neg3_regs[] = {
  {0x3a0f,0x18},  //-1.3EV
  {0x3a10,0x10},
  {0x3a1b,0x18},
  {0x3a1e,0x10},
  {0x3a11,0x30},
  {0x3a1f,0x10},
  //{REG_TERM,VAL_TERM},
};

static struct regval_list sensor_ev_neg2_regs[] = {
  {0x3a0f,0x20},  //-1.0EV
  {0x3a10,0x18},
  {0x3a1b,0x20},
  {0x3a1e,0x18},
  {0x3a11,0x41},
  {0x3a1f,0x10},
  //{REG_TERM,VAL_TERM},
};

static struct regval_list sensor_ev_neg1_regs[] = {
  {0x3a0f,0x28},  //-0.7EV
  {0x3a10,0x20},
  {0x3a1b,0x28},
  {0x3a1e,0x20},
  {0x3a11,0x51},
  {0x3a1f,0x10},
  //{REG_TERM,VAL_TERM},
};                     

static struct regval_list sensor_ev_zero_regs[] = {
  {0x3a0f,0x30},    //default
  {0x3a10,0x28},
  {0x3a1b,0x30},
  {0x3a1e,0x28},
  {0x3a11,0x61},
  {0x3a1f,0x10},
  //{REG_TERM,VAL_TERM},
};

static struct regval_list sensor_ev_pos1_regs[] = {
  {0x3a0f,0x38},  //0.7EV
  {0x3a10,0x30},
  {0x3a1b,0x38},
  {0x3a1e,0x30},
  {0x3a11,0x61},
  {0x3a1f,0x10},
  //{REG_TERM,VAL_TERM},
};

static struct regval_list sensor_ev_pos2_regs[] = {
  {0x3a0f,0x40},  //1.0EV
  {0x3a10,0x38},
  {0x3a1b,0x40},
  {0x3a1e,0x38},
  {0x3a11,0x71},
  {0x3a1f,0x10},
  //{REG_TERM,VAL_TERM},
};

static struct regval_list sensor_ev_pos3_regs[] = {
  {0x3a0f,0x48},  //1.3EV
  {0x3a10,0x40},
  {0x3a1b,0x48},
  {0x3a1e,0x40},
  {0x3a11,0x80},
  {0x3a1f,0x20},
  //{REG_TERM,VAL_TERM},
};

static struct regval_list sensor_ev_pos4_regs[] = {
  {0x3a0f,0x50},  //1.7EV
  {0x3a10,0x48},
  {0x3a1b,0x50},
  {0x3a1e,0x48},
  {0x3a11,0x90},
  {0x3a1f,0x20},
  //{REG_TERM,VAL_TERM},  
};
#endif

static struct cfg_array sensor_ev[] = {
  {
  	.regs = sensor_ev_neg4_regs,
  	.size = ARRAY_SIZE(sensor_ev_neg4_regs),
  },
  {
  	.regs = sensor_ev_neg3_regs,
  	.size = ARRAY_SIZE(sensor_ev_neg3_regs),
  },
  {
  	.regs = sensor_ev_neg2_regs,
  	.size = ARRAY_SIZE(sensor_ev_neg2_regs),
  },
  {
  	.regs = sensor_ev_neg1_regs,
  	.size = ARRAY_SIZE(sensor_ev_neg1_regs),
  },
  {
  	.regs = sensor_ev_zero_regs,
  	.size = ARRAY_SIZE(sensor_ev_zero_regs),
  },
  {
  	.regs = sensor_ev_pos1_regs,
  	.size = ARRAY_SIZE(sensor_ev_pos1_regs),
  },
  {
  	.regs = sensor_ev_pos2_regs,
  	.size = ARRAY_SIZE(sensor_ev_pos2_regs),
  },
  {
  	.regs = sensor_ev_pos3_regs,
  	.size = ARRAY_SIZE(sensor_ev_pos3_regs),
  },
  {
  	.regs = sensor_ev_pos4_regs,
  	.size = ARRAY_SIZE(sensor_ev_pos4_regs),
  },
};

/*
 * Here we'll try to encapsulate the changes for just the output
 * video format.
 * 
 */


static struct regval_list sensor_fmt_yuv422_yuyv[] = {  
  {0x4300,0x30},  //YUYV
  //{REG_TERM,VAL_TERM},
};

static struct regval_list sensor_fmt_yuv422_yvyu[] = {
  {0x4300,0x31},  //YVYU
  //{REG_TERM,VAL_TERM},
};

static struct regval_list sensor_fmt_yuv422_vyuy[] = {
  {0x4300,0x33},  //VYUY
  //{REG_TERM,VAL_TERM},
};

static struct regval_list sensor_fmt_yuv422_uyvy[] = {
  {0x4300,0x32},  //UYVY
  //{REG_TERM,VAL_TERM},
};

//static struct regval_list sensor_fmt_raw[] = {
//  
//};

static struct regval_list ae_average_tbl[] = {
  /* Whole Image Average */
	{0x5688, 0x11}, /* Zone 1/Zone 0 weight */
	{0x5689, 0x11}, /* Zone 3/Zone 2 weight */
	{0x569a, 0x11}, /* Zone 5/Zone 4 weight */
	{0x569b, 0x11}, /* Zone 7/Zone 6 weight */
	{0x569c, 0x11}, /* Zone 9/Zone 8 weight */
	{0x569d, 0x11}, /* Zone b/Zone a weight */
	{0x569e, 0x11}, /* Zone d/Zone c weight */
	{0x569f, 0x11}, /* Zone f/Zone e weight */
};

static struct regval_list ae_centerweight_tbl[] = {
  /* Whole Image Center More weight */
	{0x5688, 0x62},
	{0x5689, 0x26},
	{0x568a, 0xe6},
	{0x568b, 0x6e},
	{0x568c, 0xea},
	{0x568d, 0xae},
	{0x568e, 0xa6},
	{0x568f, 0x6a},
};

/*
 * Low-level register I/O.
 *
 */


/*
 * On most platforms, we'd rather do straight i2c I/O.
 */
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
  	return -EINVAL;
  
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
 * Write a list of continuous register setting;
 */
static int sensor_write_continuous(struct v4l2_subdev *sd, unsigned short addr, unsigned char vals[] , uint size)
{
  struct i2c_client *client = v4l2_get_subdevdata(sd);
  struct i2c_msg msg;
  unsigned char data[2+32];
  unsigned char *p = vals;
  int ret,i;
  
  while (size > 0) {
    int len = size > 32 ? 32 : size;
    data[0] = (addr&0xff00) >> 8;
    data[1] = (addr&0x00ff);
    
    for(i = 2; i < 2+len; i++)
      data[i] = *p++;

    msg.addr = client->addr;
    msg.flags = 0;  
    msg.len = 2+len;
    msg.buf = data;
  
    ret = i2c_transfer(client->adapter, &msg, 1);
    
    if (ret > 0) {
      ret = 0;
    } else if (ret < 0) {
      vfe_dev_err("sensor_write error!\n");
    }
    addr += len;
    size -= len;
  }
  return ret;
}


static unsigned char current_lum=0xff;
static unsigned char sensor_get_lum(struct v4l2_subdev *sd)
{
  sensor_read(sd, 0x56a1, &current_lum);
  vfe_dev_cap_dbg("check luminance=0x%x\n",current_lum);
  return current_lum;
}

/* stuff about exposure when capturing image and video*/
static int sensor_s_denoise_value(struct v4l2_subdev *sd, unsigned char value);
unsigned char ogain,oexposurelow,oexposuremid,oexposurehigh;
unsigned int preview_exp_line,preview_fps;
unsigned long preview_pclk;

static unsigned int cal_cap_gain(unsigned char prv_gain, unsigned char lum)
{
  unsigned int gain_ret=0x18;
	
	vfe_dev_cap_dbg("current_lum=0x%x\n",lum);
  
	if(current_lum>0xa0)
	{
//	  night_mode=0;
	  //Nfrms=1;
	  if(ogain>0x40)
	    gain_ret=0x20;
	  else if(ogain>0x20)
	    gain_ret=0x18;
	  else
	    gain_ret=0x10;
	}
	else if(current_lum>0x80)
	{
//	  night_mode=0;
	  //Nfrms=1;
	  if(ogain>0x40)
	    gain_ret=0x30;
	  else if(ogain>0x20)
	    gain_ret=0x28;
	  else
	    gain_ret=0x20;
	}
	else if(current_lum>0x40)
	{
//	  night_mode=0;
	  //Nfrms=1;
	  if(ogain>0x60)
	    gain_ret=ogain/3;
	  else if(ogain>0x40)
	    gain_ret=ogain/2;
	  else
	    gain_ret=ogain;
	}
	else if(current_lum>0x20)
	{
//	  night_mode=0;
	  //Nfrms=1;
	  if(ogain>0x60)
	    gain_ret=ogain/6;
	  else if(ogain>0x20)
	    gain_ret=ogain/2;
	  else
	    gain_ret=ogain;
	}
	else
	{
	  vfe_dev_cap_dbg("low_light=0x%x\n",lum);
//	  night_mode=1;
	  if(ogain>0xf0)
	  {
	    gain_ret = 0x10;
	    //Nfrms=4;
	  }
	  else if(ogain>0xe0)
	  {
	    gain_ret = 0x14;
	    //Nfrms=3;
	  }
	  else
	  {
	    gain_ret = 0x18;
	    //Nfrms=2;
	  }
  }
  
  if(gain_ret<0x10)
    gain_ret=0x10;
  
  vfe_dev_cap_dbg("gain return=0x%x\n",gain_ret);
  return gain_ret;
}

static int sensor_set_capture_exposure(struct v4l2_subdev *sd)
{
	unsigned long lines_10ms;
	unsigned int capture_expLines;
	unsigned int preview_explines;
	unsigned long previewExposure;
	unsigned long capture_Exposure;
	unsigned long capture_exposure_gain;
	unsigned long capture_gain;
	unsigned char gain,exposurelow,exposuremid,exposurehigh;
	unsigned int cap_vts=0;
	unsigned int cap_vts_diff=0;
	unsigned int bd_step=1;
#ifndef FPGA
	unsigned int capture_fps = 75;
//	unsigned int preview_fps = 3000;
#else
	unsigned int capture_fps = 25;
//	unsigned int preview_fps = 750;
#endif
	struct sensor_info *info = to_state(sd);
	unsigned char rdval;

  vfe_dev_dbg("sensor_set_capture_exposure\n");
	
//	if(info->tpf.numerator!=0)
//		preview_fps = info->tpf.denominator/info->tpf.numerator*100; 
  
	preview_fps = preview_fps*10;
	
	if(info->low_speed == 1) {
	  //preview_fps = preview_fps/2;
		capture_fps = capture_fps/2;
	}
  //vfe_dev_dbg("capture_fps=0x%x\n",capture_fps);
		
	preview_explines = preview_exp_line;//984;
	capture_expLines = 1968;
	if(info->band_filter == V4L2_CID_POWER_LINE_FREQUENCY_60HZ)
	  lines_10ms = capture_fps * capture_expLines *1000/12000;
	else
	  lines_10ms = capture_fps * capture_expLines *1000/10000;
  //vfe_dev_dbg("lines_10ms=0x%x\n",lines_10ms);

	previewExposure = ((unsigned int)(oexposurehigh))<<12 ;
	previewExposure += ((unsigned int)oexposuremid)<<4 ;
	previewExposure += (oexposurelow >>4);
  vfe_dev_cap_dbg("previewExposure=0x%x\n",previewExposure);
	
	if(0 == preview_explines || 0== lines_10ms)
	{
		return 0;
	}
	
	if(preview_explines == 0 || preview_fps == 0)
	  return -EFAULT;
	  
	if(night_mode==0)
	{
//	  signed int exp_ratio;
//	  if(info->exp_bias>4 || info->exp_bias<-4)
//	    exp_ratio=8;
//	  else
//	    exp_ratio=8+info->exp_bias;
//  	
//  	exp_ratio=exp_ratio>>2;
  	
  	capture_Exposure =
  		(1*(previewExposure*(capture_fps)*(capture_expLines))/
  	(((preview_explines)*(preview_fps))));
    vfe_dev_cap_dbg("cal from prv: capture_Exposure=0x%x\n",capture_Exposure);
  }
  else
  {
  	capture_Exposure =
  		(night_mode*(previewExposure*(capture_fps)*(capture_expLines))/
  	(((preview_explines)*(preview_fps))));
  }
  vfe_dev_dbg("capture_Exposure=0x%lx\n",capture_Exposure);
  
  if(CAP_GAIN_CAL==0)//auto_limit_frames_mode
  {
    capture_gain=(unsigned long)cal_cap_gain(ogain,current_lum);
    vfe_dev_cap_dbg("auto_limit_frames_mode: ogain=0x%x, capture_gain=0x%x\n",ogain, capture_gain);
    capture_Exposure=capture_Exposure*ogain/capture_gain; //fixed_gain 
    vfe_dev_cap_dbg("auto_limit_frames_mode: capture_Exposure=0x%x\n",capture_Exposure);
    capture_exposure_gain=capture_Exposure*capture_gain;
    vfe_dev_cap_dbg("auto_limit_frames_mode: capture_exposure_gain=0x%x\n",capture_exposure_gain);
  	
  	if(capture_Exposure>Nfrms*capture_expLines)
  	{
      vfe_dev_cap_dbg("auto_limit_frames_mode: longer than %d frames\n", Nfrms);
      capture_gain=capture_exposure_gain/(Nfrms*capture_expLines);
      vfe_dev_cap_dbg("auto_limit_frames_mode: exceed %d frames\n", Nfrms);
      vfe_dev_cap_dbg("auto_limit_frames_mode: re cal capture_gain = 0x%x\n", capture_gain);
      capture_Exposure=Nfrms*capture_expLines;
  	}
  	if(capture_gain>0xf8)
  	  capture_gain=0xf8;
  }
  else//manual_gain_mode
  {
    vfe_dev_cap_dbg("manual_gain_mode: before capture_Exposure=0x%x\n",capture_Exposure);
    capture_gain=cap_manual_gain;
    vfe_dev_cap_dbg("manual_gain_mode: capture_gain=0x%x\n",capture_gain);
  	capture_Exposure=capture_Exposure*ogain/capture_gain; //fixed_gain 
    vfe_dev_cap_dbg("manual_gain_mode: after capture_Exposure=0x%x\n",capture_Exposure);
  }
	
	//banding
	//capture_Exposure = capture_Exposure * 1000;
	if (capture_Exposure*1000  > lines_10ms)
	{
	  vfe_dev_cap_dbg("lines_10ms=0x%x\n",lines_10ms);
		bd_step=capture_Exposure*1000 / lines_10ms;
	  vfe_dev_cap_dbg("bd_step=0x%x\n",bd_step);
		//capture_Exposure =bd_step * lines_10ms;
	}
	//capture_Exposure = capture_Exposure / 1000;
	
	if(capture_Exposure == 0)
		capture_Exposure = 1;
	
  vfe_dev_dbg("capture_Exposure = 0x%lx\n",capture_Exposure);
	
	if( (1000*capture_Exposure-bd_step*lines_10ms)*16 > lines_10ms )
	{
	  vfe_dev_cap_dbg("(1000*capture_Exposure-bd_step*lines_10ms)*16=%d\n",16*(1000*capture_Exposure-bd_step*lines_10ms));
	  capture_gain=capture_exposure_gain/capture_Exposure;
	  vfe_dev_cap_dbg("after banding re cal capture_gain = 0x%x\n", capture_gain);
	}
	
	if(capture_Exposure>1968)
	{
	  cap_vts=capture_Exposure;
	  cap_vts_diff=capture_Exposure-1968;
	  vfe_dev_cap_dbg("cap_vts =%d, cap_vts_diff=%d\n",cap_vts,cap_vts_diff);
	}
	else
	{
	  cap_vts=1968;
	  cap_vts_diff=0;
	}
//	capture_Exposure=1968;
	exposurelow = ((unsigned char)capture_Exposure)<<4;
	exposuremid = (unsigned char)(capture_Exposure >> 4) & 0xff;
	exposurehigh = (unsigned char)(capture_Exposure >> 12);
	gain =(unsigned char) capture_gain;
	
  sensor_read(sd, 0x3503, &rdval);
  vfe_dev_dbg("capture:agc/aec:0x%x,gain:0x%x,exposurelow:0x%x,exposuremid:0x%x,exposurehigh:0x%x\n",\
                  rdval,gain,exposurelow,exposuremid,exposurehigh);
  
#ifdef DENOISE_LV_AUTO	
	sensor_s_denoise_value(sd,1 + gain*gain/0x100); //denoise via gain
#else
  sensor_s_denoise_value(sd, DENOISE_LV);
#endif
	
//	cap_vts=0x1fff;
//	cap_vts_diff=0x1000;
  sensor_write(sd, 0x380e, (unsigned char)(cap_vts>>8) );
  sensor_write(sd, 0x380f, (unsigned char)(cap_vts)  );
  sensor_write(sd, 0x350c, (unsigned char)((cap_vts_diff)>>8) );
  sensor_write(sd, 0x350d, (unsigned char)( cap_vts_diff)  );
  
  sensor_write(sd, 0x350b, gain);
  sensor_write(sd, 0x3502, exposurelow);
  sensor_write(sd, 0x3501, exposuremid);
  sensor_write(sd, 0x3500, exposurehigh);
  
//sensor_write(sd,0x5001 ,0xff);
//sensor_write(sd,0x5587 ,0x10);
//sensor_write(sd,0x5580 ,0x04);
//sensor_write(sd,0x5588 ,0x09);
	return 0;
}
static int sensor_get_pclk(struct v4l2_subdev *sd)
{
  unsigned long pclk;
  unsigned char pre_div,mul,sys_div,pll_rdiv,bit_div,sclk_rdiv;
  
  sensor_read(sd, 0x3037, &pre_div);
  pre_div = pre_div & 0x0f;
  
  if(pre_div == 0)
    pre_div = 1;
  
  sensor_read(sd, 0x3036, &mul);
  if(mul < 128)
    mul = mul;
  else
    mul = mul/2*2;
  
  sensor_read(sd, 0x3035, &sys_div);
  sys_div = (sys_div & 0xf0) >> 4;
  
  sensor_read(sd, 0x3037, &pll_rdiv);
  pll_rdiv = (pll_rdiv & 0x10) >> 4;
  pll_rdiv = pll_rdiv + 1;
  
  sensor_read(sd, 0x3034, &bit_div);
  bit_div = (bit_div & 0x0f);
  
  sensor_read(sd, 0x3108, &sclk_rdiv);
  sclk_rdiv = (sclk_rdiv & 0x03);
  sclk_rdiv = sclk_rdiv << sclk_rdiv;
  
  vfe_dev_dbg("pre_div = %d,mul = %d,sys_div = %d,pll_rdiv = %d,sclk_rdiv = %d\n",\
          pre_div,mul,sys_div,pll_rdiv,sclk_rdiv);
  
  if((pre_div&&sys_div&&pll_rdiv&&sclk_rdiv) == 0)
    return -EFAULT;
  
  if(bit_div == 8)
    pclk = MCLK / pre_div * mul / sys_div / pll_rdiv / 2 / sclk_rdiv;
  else if(bit_div == 10)
    pclk = MCLK / pre_div * mul / sys_div / pll_rdiv * 2 / 5 / sclk_rdiv;
  else
    pclk = MCLK / pre_div * mul / sys_div / pll_rdiv / 1 / sclk_rdiv;
  
  vfe_dev_dbg("pclk = %ld\n",pclk);
  
  preview_pclk = pclk;
  return 0;
}

static int sensor_get_fps(struct v4l2_subdev *sd)
{
  unsigned char vts_low,vts_high,hts_low,hts_high,vts_extra_high,vts_extra_low;
  unsigned long vts,hts,vts_extra;
  
  sensor_read(sd, 0x380c, &hts_high);
  sensor_read(sd, 0x380d, &hts_low);
  sensor_read(sd, 0x380e, &vts_high);
  sensor_read(sd, 0x380f, &vts_low);
  sensor_read(sd, 0x350c, &vts_extra_high);
  sensor_read(sd, 0x350d, &vts_extra_low);
   
  hts = hts_high * 256 + hts_low;
  vts = vts_high * 256 + vts_low;
  vts_extra = vts_extra_high * 256 + vts_extra_low;
  
  if((hts&&(vts+vts_extra)) == 0)
    return -EFAULT;
    
  if(sensor_get_pclk(sd))
    vfe_dev_err("get pclk error!\n");

  preview_fps = preview_pclk / ((vts_extra+vts) * hts);
  vfe_dev_dbg("preview fps = %d\n",preview_fps);
  
  return 0;
}

static int sensor_get_preview_exposure(struct v4l2_subdev *sd)
{
	unsigned char vts_low,vts_high,vts_extra_high,vts_extra_low;
	unsigned long vts,vts_extra;
  sensor_read(sd, 0x350b, &ogain);
  sensor_read(sd, 0x3502, &oexposurelow);
  sensor_read(sd, 0x3501, &oexposuremid);
  sensor_read(sd, 0x3500, &oexposurehigh);  
	sensor_read(sd, 0x380e, &vts_high);
  sensor_read(sd, 0x380f, &vts_low);
	sensor_read(sd, 0x350c, &vts_extra_high);
	sensor_read(sd, 0x350d, &vts_extra_low);
	
	vts = vts_high * 256 + vts_low;
	vts_extra = vts_extra_high * 256 + vts_extra_low;
	preview_exp_line = vts + vts_extra;
  
	vfe_dev_dbg("preview_exp_line = %d\n",preview_exp_line);
  vfe_dev_dbg("preview:gain:0x%x,exposurelow:0x%x,exposuremid:0x%x,exposurehigh:0x%x\n",\
                  ogain,oexposurelow,oexposuremid,oexposurehigh);
  
  return 0;
}

//static void sensor_s_ae_ratio(struct work_struct *work)
//{
//  vfe_dev_dbg("sensor_s_ae_ratio\n");
//  sensor_write(glb_sd, 0x3a05, 0x30);//normal aec ratio
//}

static int sensor_set_preview_exposure(struct v4l2_subdev *sd)
{ 
  unsigned char rdval;
  sensor_read(sd, 0x3503, &rdval);
  vfe_dev_dbg("preview:agc/aec:0x%x,gain:0x%x,exposurelow:0x%x,exposuremid:0x%x,exposurehigh:0x%x\n",
                  rdval,ogain,oexposurelow,oexposuremid,oexposurehigh);
  
//  sensor_read(sd, 0x3001, &rdval);
//  sensor_write(sd, 0x3001, rdval|0x3);  //reset AE
//  msleep(10);
//  sensor_write(sd, 0x3001, rdval);  //release reset AE
  
  sensor_write(sd, 0x350b, ogain);
  sensor_write(sd, 0x3502, oexposurelow);
  sensor_write(sd, 0x3501, oexposuremid);
  sensor_write(sd, 0x3500, oexposurehigh);
  
//  sensor_write(sd, 0x3a05, 0x3f);//max aec ratio
//  vfe_dev_dbg("set max aec ratio\n");
//  schedule_delayed_work(&sensor_s_ae_ratio_work, msecs_to_jiffies(500));
//sensor_write(sd,0x5001 ,0xff);
//sensor_write(sd,0x5587 ,0x00);
//sensor_write(sd,0x5580 ,0x04);
//sensor_write(sd,0x5588 ,0x01);
  return 0;
}

#ifdef _FLASH_FUNC_
void check_to_flash(struct v4l2_subdev *sd)
{
  struct sensor_info *info = to_state(sd);
  if(info->flash_mode==V4L2_FLASH_LED_MODE_FLASH)
  {
    to_flash=1;
  }
  else if(info->flash_mode==V4L2_FLASH_LED_MODE_AUTO)
  {
    sensor_get_lum(sd);
    if( current_lum<flash_auto_level )
      to_flash=1;
    else
      to_flash=0;
  }
  else
  {
    to_flash=0;
  }
  
  vfe_dev_dbg("to_flash=%d\n",to_flash);
}
#endif

/* stuff about auto focus */

static int sensor_download_af_fw(struct v4l2_subdev *sd)
{
  int ret,cnt;
  unsigned char rdval;
	int reload_cnt = 0;
//	struct csi_dev *dev=(struct csi_dev *)dev_get_drvdata(sd->v4l2_dev->dev);
	
  struct regval_list af_fw_reset_reg[] = {
    {0x3000,0x20},
    //{REG_TERM,VAL_TERM},
  };
  struct regval_list af_fw_start_reg[] = {
    {0x3022,0x00},
    {0x3023,0x00},
    {0x3024,0x00},
    {0x3025,0x00},
    {0x3026,0x00},
    {0x3027,0x00},
    {0x3028,0x00},
    {0x3029,0x7f},
    {0x3000,0x00},  //start firmware for af
    //{REG_TERM,VAL_TERM},
  };
  
  //reset sensor MCU
  ret = sensor_write_array(sd, af_fw_reset_reg ,ARRAY_SIZE(af_fw_reset_reg));
  if(ret < 0) {
    vfe_dev_err("reset sensor MCU error\n");
    return ret;
  }
    
  //download af fw
  ret =sensor_write_continuous(sd, 0x8000, sensor_af_fw_regs, ARRAY_SIZE(sensor_af_fw_regs));
  if(ret < 0) {
    vfe_dev_err("download af fw error\n");
    return ret;
  }
  vfe_dev_print("sizeof(af_fw_start_reg)=%d\n",sizeof(af_fw_start_reg));
  //start af firmware
  ret = sensor_write_array(sd, af_fw_start_reg ,ARRAY_SIZE(af_fw_start_reg));
  if(ret < 0) {
    vfe_dev_err("start af firmware error\n");
    return ret;
  }
  
  msleep(10);
  //check the af firmware status
  rdval = 0xff;
  cnt = 0;
recheck_af_fw:	
  while(rdval!=0x70) {
    ret = sensor_read(sd, 0x3029, &rdval);
    if (ret < 0)
    {
      vfe_dev_err("sensor check the af firmware status err !\n");
      return ret;
    }
    cnt++;
		if(cnt > 3) {
      vfe_dev_err("AF firmware check status time out !\n");
			reload_cnt++;
			if(reload_cnt <= 2) {
				vfe_dev_err("AF firmware check status retry cnt = %d!\n",reload_cnt);
				vfe_gpio_write(sd,PWDN,CSI_STBY_ON);
				usleep_range(10000,12000);
				vfe_gpio_write(sd,PWDN,CSI_STBY_OFF);
				usleep_range(10000,12000);
				goto recheck_af_fw;
			}
      return -EFAULT;
    }
		usleep_range(5000,10000);
  }
  vfe_dev_print("AF firmware check status complete, 0x3029 = 0x%x\n",rdval);

  return 0;
}

static int sensor_g_single_af(struct v4l2_subdev *sd)
{
  unsigned char rdval;
  struct sensor_info *info = to_state(sd);
  
//  vfe_dev_dbg("sensor_g_single_af\n");
  if(info->focus_status!=1)
    return V4L2_AUTO_FOCUS_STATUS_IDLE;
  
  rdval = 0xff;
  
  LOG_ERR_RET(sensor_read(sd, 0x3029, &rdval))
//  printk("0x3029=0x%x\n",rdval);

  if(rdval == 0x10)
  {
    int ret=0;
    info->focus_status = 0; //idle
    sensor_read(sd, 0x3028, &rdval);
    if(rdval==0)
    {
	    vfe_dev_print("Single AF focus fail, 0x3028 = 0x%x\n",rdval);
	    ret = V4L2_AUTO_FOCUS_STATUS_FAILED;
    }
    else
    {
		  vfe_dev_dbg("Single AF focus ok, 0x3028 = 0x%x\n",rdval);
		  ret = V4L2_AUTO_FOCUS_STATUS_REACHED;
    }
    #ifdef _FLASH_FUNC_
    if(info->flash_mode!=V4L2_FLASH_LED_MODE_NONE)
    {
      vfe_dev_print("shut flash when af fail/ok\n");
      io_set_flash_ctrl(sd, SW_CTRL_FLASH_OFF, info->fl_dev_info);
    }
    #endif
    return ret;
  }
  else if(rdval==0x70)
  {
  	info->focus_status = 0;
    //vfe_dev_print("Single AF focus idle,0x3029 = 0x%x\n",rdval);
    #ifdef _FLASH_FUNC_
    if(info->flash_mode!=V4L2_FLASH_LED_MODE_NONE)
    {
      vfe_dev_print("shut flash when af idle 2\n");
      io_set_flash_ctrl(sd, SW_CTRL_FLASH_OFF, info->fl_dev_info);
    }
    #endif
    return V4L2_AUTO_FOCUS_STATUS_IDLE;
  }
  else if(rdval==0x00)
  {
  	info->focus_status = 1;
    //vfe_dev_print("Single AF focus ing,0x3029 = 0x%x\n",rdval);
    return V4L2_AUTO_FOCUS_STATUS_BUSY;
  }
  
  //vfe_dev_dbg("Single AF focus is running,0x3029 = 0x%x\n",rdval);
  
  return V4L2_AUTO_FOCUS_STATUS_BUSY;
}

static int sensor_g_contin_af(struct v4l2_subdev *sd)
{
  unsigned char rdval;
  struct sensor_info *info = to_state(sd);
  
//  vfe_dev_dbg("sensor_g_contin_af\n");
  
  rdval = 0xff;
  
  LOG_ERR_RET(sensor_read(sd, 0x3029, &rdval))
//  printk("0x3029=0x%x\n",rdval);
//
//  LOG_ERR_RET(sensor_read(sd, 0x3028, &rdval))
//  printk("0x3028=0x%x\n",rdval);

  if(rdval == 0x20 || rdval == 0x10)
  {
    info->focus_status = 0; //idle
    sensor_read(sd, 0x3028, &rdval);
    if(rdval==0)
    {
//	    vfe_dev_print("Contin AF focus fail, 0x3028 = 0x%x\n",rdval);
	    return V4L2_AUTO_FOCUS_STATUS_FAILED;
    }
    else
    {
//		  vfe_dev_dbg("Contin AF focus ok, 0x3028 = 0x%x\n",rdval);
		  return V4L2_AUTO_FOCUS_STATUS_REACHED;
    }
    	
  }
  else if(rdval==0x00)
  {
  	info->focus_status = 1;//busy
//    vfe_dev_dbg("Contin AF focus ing,0x3029 = 0x%x\n",rdval);
    return V4L2_AUTO_FOCUS_STATUS_BUSY;
  }
  else// if(rdval==0x70)
  {
  	info->focus_status = 0;//idle
//  	vfe_dev_dbg("Contin AF focus is idle,0x3029 = 0x%x\n",rdval);
  	return V4L2_AUTO_FOCUS_STATUS_IDLE;
  }
  
  //return V4L2_AUTO_FOCUS_STATUS_BUSY;
}

static int sensor_g_af_status(struct v4l2_subdev *sd)
{
  int ret=0;
  struct sensor_info *info = to_state(sd);
	
	if(info->auto_focus==1)
		ret = sensor_g_contin_af(sd);
	else
		ret = sensor_g_single_af(sd);
	
	return ret;
}

static int sensor_g_3a_lock(struct v4l2_subdev *sd)
{
  //int ret=0;
  struct sensor_info *info = to_state(sd);
  return ( (info->auto_focus==0)?V4L2_LOCK_FOCUS:~V4L2_LOCK_FOCUS |
           (info->autowb==0)?V4L2_LOCK_WHITE_BALANCE:~V4L2_LOCK_WHITE_BALANCE |
           (~V4L2_LOCK_EXPOSURE)
         );
}

static int sensor_s_init_af(struct v4l2_subdev *sd)
{
	int ret;
  struct sensor_info *info = to_state(sd);
	ret=sensor_download_af_fw(sd);
	if(ret==0)
		info->af_first_flag=0;
	//other config
	return ret;
}

static int sensor_s_single_af(struct v4l2_subdev *sd)
{
  int ret;
  struct sensor_info *info = to_state(sd);
  unsigned char rdval=0xff;
  unsigned int cnt=0;
  
  vfe_dev_print("sensor_s_single_af\n");
  //trig single af
    
  info->focus_status = 0; //idle  
  
  sensor_write(sd, 0x3023, 0x01);
  
  ret = sensor_write(sd, 0x3022, 0x03);
  if (ret < 0) {
    vfe_dev_err("sensor tigger single af err !\n");
    return ret;
  }
  
  while(rdval!=0 && cnt<10)
  {
  	usleep_range(1000,1200);
  	ret=sensor_read(sd, 0x3023, &rdval);
  	cnt++;
  }
  if(cnt>10)
  		vfe_dev_dbg("set single af timeout\n");
  
  #ifdef _FLASH_FUNC_
  if(info->flash_mode!=V4L2_FLASH_LED_MODE_NONE)
  {
    check_to_flash(sd);
    if(to_flash==1)
    {
      vfe_dev_print("open torch when start single af\n");
      io_set_flash_ctrl(sd, SW_CTRL_TORCH_ON, info->fl_dev_info);
    }
  }
  #endif
    
//  info->contin_focus=0;
  info->focus_status = 1; //busy
  info->auto_focus=0;
  return 0;
}

static int sensor_s_continueous_af(struct v4l2_subdev *sd, int value)
{
  struct sensor_info *info = to_state(sd);
  vfe_dev_print("sensor_s_continueous_af[0x%x]\n",value);
	if(info->focus_status==1)
	{
	  vfe_dev_err("continous focus not accepted when single focus\n");
	  return -1;
	}
	if( (info->auto_focus==value) )
	{
	  vfe_dev_dbg("already in same focus mode\n");
	  return 0;
	}
	
	if(value==1)
  {
  	LOG_ERR_RET(sensor_write(sd, 0x3022, 0x04))
    LOG_ERR_RET(sensor_write(sd, 0x3022, 0x80))
  	info->auto_focus=1;
  }
  else
  {
  	LOG_ERR_RET(sensor_write(sd, 0x3022, 0x06))//pause af
//  	LOG_ERR_RET(sensor_write(sd, 0x3022, 0x08))//release af
		info->auto_focus=0;
  }
  return 0;
}

static int sensor_s_pause_af(struct v4l2_subdev *sd)
{ 
  //pause af poisition
  vfe_dev_print("sensor_s_pause_af\n");

  LOG_ERR_RET(sensor_write(sd, 0x3022, 0x06))
    
  //msleep(5);
  return 0;
}

static int sensor_s_release_af(struct v4l2_subdev *sd)
{
  //release focus
  vfe_dev_print("sensor_s_release_af\n");
  
  //release single af
  LOG_ERR_RET(sensor_write(sd, 0x3022, 0x08))
  return 0;
}

#if 1
static int sensor_s_relaunch_af_zone(struct v4l2_subdev *sd)
{
//  struct regval_list regs;
//  int ret;
  //relaunch defalut af zone
  vfe_dev_print("sensor_s_relaunch_af_zone\n");
  LOG_ERR_RET(sensor_write(sd, 0x3023, 0x01))
  LOG_ERR_RET(sensor_write(sd, 0x3022, 0x80))

  usleep_range(5000,6000);
  return 0;
}
#endif

static int sensor_s_af_zone(struct v4l2_subdev *sd,
														struct v4l2_win_coordinate * win_c)
{
  struct sensor_info *info = to_state(sd);
  int ret;
  
  int x1,y1,x2,y2;
  unsigned int xc,yc;
  unsigned int prv_x,prv_y;
  
  vfe_dev_print("sensor_s_af_zone\n");
  
  if(info->width == 0 || info->height == 0) {
    vfe_dev_err("current width or height is zero!\n");
    return -EINVAL;
  }
  
  prv_x=(int)info->width;
  prv_y=(int)info->height;
  
  x1=win_c->x1;
  y1=win_c->y1;
  x2=win_c->x2;
  y2=win_c->y2;
  
#ifdef AF_WIN_NEW_COORD
  xc=prv_x*((unsigned int)(2000+x1+x2)/2)/2000;
  yc=(prv_y*((unsigned int)(2000+y1+y2)/2)/2000);
#else
  xc=(x1+x2)/2;
  yc=(y1+y2)/2;
#endif
  
  vfe_dev_dbg("af zone input xc=%d,yc=%d\n",xc,yc);
  
  if(x1>x2 || y1>y2 ||
  	 xc>info->width || yc>info->height)
  {
  	vfe_dev_dbg("invalid af win![%d,%d][%d,%d] prv[%d/%d]\n",x1,y1,x2,y2,prv_x,prv_y);
  	return -EINVAL;
  }
  
  if(info->focus_status == 1) //can not set af zone when focus is busy
    return 0;
  
  xc = (xc * 80 * 2 / info->width + 1) / 2;
  if((info->width == HD720_WIDTH && info->height == HD720_HEIGHT) || \
     (info->width == HD1080_WIDTH && info->height == HD1080_HEIGHT)) {
    yc = (yc * 45 * 2 / info->height + 1) / 2;
  } else {
    yc = (yc * 60 * 2 / info->height + 1) / 2;
  }
  
  vfe_dev_dbg("af zone after xc=%d,yc=%d\n",xc,yc);
    
  //set x center
  ret = sensor_write(sd, 0x3024, xc);
  if (ret < 0)
  {
    vfe_dev_err("sensor_s_af_zone_xc error!\n");
    return ret;
  }
  //set y center
  ret = sensor_write(sd, 0x3025, yc);
  if (ret < 0)
  {
    vfe_dev_err("sensor_s_af_zone_yc error!\n");
    return ret;
  }
  
  ret = sensor_write(sd, 0x3023, 0x01);
  //set af zone
  ret |= sensor_write(sd, 0x3022, 0x81);
  if (ret < 0)
  {
    vfe_dev_err("sensor_s_af_zone error!\n");
    return ret;
  }

  //msleep(5);
  sensor_s_relaunch_af_zone(sd);
  
  return 0;
}

static int sensor_s_3a_lock(struct v4l2_subdev *sd, int value)
{
  //struct sensor_info *info = to_state(sd);
  int ret;
  
  //vfe_dev_dbg("sensor_s_3a_lock=0x%x\n", value);
  value=!((value&V4L2_LOCK_FOCUS)>>2);
  if(value==0)
    ret=sensor_s_pause_af(sd);
  else
    ret=sensor_s_relaunch_af_zone(sd);
  
  return ret;
}

#if 1
static int sensor_s_sharpness_auto(struct v4l2_subdev *sd)
{
	unsigned char rdval;
	sensor_read(sd,0x5308,&rdval);
	sensor_write(sd,0x5308,rdval&0xbf); //bit6 is sharpness manual enable
	return sensor_write_array(sd, sensor_sharpness_auto_regs ,ARRAY_SIZE(sensor_sharpness_auto_regs));
}
#endif

static int sensor_s_sharpness_value(struct v4l2_subdev *sd, unsigned char value)
{
	unsigned char rdval;
	sensor_read(sd,0x5308,&rdval);
	sensor_write(sd,0x5308,rdval|0x40); //bit6 is sharpness manual enable
  return sensor_write(sd,0x5302,value); 
}

#if 1
static int sensor_s_denoise_auto(struct v4l2_subdev *sd)
{
	unsigned char rdval;
	sensor_read(sd,0x5308,&rdval);
	sensor_write(sd,0x5308,rdval&0xef); //bit4 is denoise manual enable
	return sensor_write_array(sd, sensor_denoise_auto_regs ,ARRAY_SIZE(sensor_denoise_auto_regs));
}
#endif

static int sensor_s_denoise_value(struct v4l2_subdev *sd, unsigned char value)
{
	unsigned char rdval;
	sensor_read(sd,0x5308,&rdval);
	sensor_write(sd,0x5308,rdval|0x10); //bit4 is denoise manual enable
	return sensor_write(sd,0x5306,value); 
}




/* *********************************************begin of ******************************************** */

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

static int sensor_g_autogain(struct v4l2_subdev *sd, __s32 *value)
{
  struct sensor_info *info = to_state(sd);
  unsigned char rdval;
  
  LOG_ERR_RET(sensor_read(sd, 0x3503, &rdval))

  if ((rdval&0x02) == 0x02) {
    *value = 0;
  }
  else
  {
    *value = 1;
  }
  
  info->autogain = *value;
  return 0;
}

static int sensor_s_autogain(struct v4l2_subdev *sd, int value)
{
  struct sensor_info *info = to_state(sd);
  unsigned char rdval;
  
  LOG_ERR_RET(sensor_read(sd, 0x3503, &rdval))
  
  switch (value) {
    case 0:
      rdval |= 0x02;
      break;
    case 1:
      rdval &= 0xfd;
      break;
    default:
      return -EINVAL;
  }
    
  LOG_ERR_RET(sensor_write(sd, 0x3503, rdval))

  info->autogain = value; 
  return 0;
}

static int sensor_g_autoexp(struct v4l2_subdev *sd, __s32 *value)
{
  struct sensor_info *info = to_state(sd);
  unsigned char rdval;
  
  LOG_ERR_RET(sensor_read(sd, 0x3503, &rdval))

  if ((rdval&0x01) == 0x01) {
    *value = V4L2_EXPOSURE_MANUAL;
  }
  else
  {
    *value = V4L2_EXPOSURE_AUTO;
  }
  
  info->autoexp = *value;
  return 0;
}

static int sensor_s_autoexp(struct v4l2_subdev *sd,
    enum v4l2_exposure_auto_type value)
{
  struct sensor_info *info = to_state(sd);
  unsigned char rdval;
  
  LOG_ERR_RET(sensor_read(sd, 0x3503, &rdval))
  
  switch (value) {
    case V4L2_EXPOSURE_AUTO:
      rdval &= 0xfe;
      break;
    case V4L2_EXPOSURE_MANUAL:
      rdval |= 0x01;
      break;
    case V4L2_EXPOSURE_SHUTTER_PRIORITY:
      return -EINVAL;    
    case V4L2_EXPOSURE_APERTURE_PRIORITY:
      return -EINVAL;
    default:
      return -EINVAL;
  }
    
  LOG_ERR_RET(sensor_write(sd, 0x3503, rdval))

//  msleep(10);
  info->autoexp = value;  
  return 0;
}

static int sensor_g_autowb(struct v4l2_subdev *sd, int *value)
{
  struct sensor_info *info = to_state(sd);
  unsigned char rdval;
  
  LOG_ERR_RET(sensor_read(sd, 0x3406, &rdval))

  rdval &= (1<<1);
  rdval = rdval>>1;   //0x3406 bit0 is awb enable
    
  *value = (rdval == 1)?0:1;
  info->autowb = *value;
  return 0;
}

static int sensor_s_autowb(struct v4l2_subdev *sd, int value)
{
  struct sensor_info *info = to_state(sd);
  unsigned char rdval;

  if(info->autowb == value)
    return 0;
  
  LOG_ERR_RET(sensor_write_array(sd, sensor_wb_auto_regs ,ARRAY_SIZE(sensor_wb_auto_regs)))  
  LOG_ERR_RET(sensor_read(sd, 0x3406, &rdval))

  switch(value) {
  case 0:
    rdval |= 0x01;
    break;
  case 1:
    rdval &= 0xfe;
    break;
  default:
    break;
  }
  
  LOG_ERR_RET(sensor_write(sd, 0x3406, rdval))
  
  //msleep(10);   
  info->autowb = value;
  return 0;
}

static int sensor_g_hue(struct v4l2_subdev *sd, __s32 *value)
{
  return -EINVAL;
}

static int sensor_s_hue(struct v4l2_subdev *sd, int value)
{
  return -EINVAL;
}

static int sensor_g_gain(struct v4l2_subdev *sd, __s32 *value)
{
  return -EINVAL;
}

static int sensor_s_gain(struct v4l2_subdev *sd, int value)
{
  return -EINVAL;
}

static int sensor_g_band_filter(struct v4l2_subdev *sd, 
    __s32 *value)
{
  struct sensor_info *info = to_state(sd);
  unsigned char rdval;
  
  LOG_ERR_RET(sensor_read(sd, 0x3a00, &rdval))
  
  if((rdval & (1<<5))== (1<<5))
    info->band_filter = V4L2_CID_POWER_LINE_FREQUENCY_DISABLED;
  else {
    LOG_ERR_RET(sensor_read(sd, 0x3c00, &rdval))
    if((rdval & (1<<2))== (1<<2))
      info->band_filter = V4L2_CID_POWER_LINE_FREQUENCY_50HZ;
    else
      info->band_filter = V4L2_CID_POWER_LINE_FREQUENCY_60HZ;
  }
  return 0;
}

static int sensor_s_band_filter(struct v4l2_subdev *sd, 
    enum v4l2_power_line_frequency value)
{
  struct sensor_info *info = to_state(sd);
  unsigned char rdval;

  if(info->band_filter == value)
    return 0;
  
  switch(value) {
    case V4L2_CID_POWER_LINE_FREQUENCY_DISABLED:  
      LOG_ERR_RET(sensor_read(sd,0x3a00,&rdval))
      LOG_ERR_RET(sensor_write(sd,0x3a00,rdval&0xdf))//turn off band filter 
      break;
    case V4L2_CID_POWER_LINE_FREQUENCY_50HZ:
      LOG_ERR_RET(sensor_write(sd,0x3c00,0x04))//50hz 
      LOG_ERR_RET(sensor_write(sd,0x3c01,0x80))//manual band filter
      LOG_ERR_RET(sensor_read(sd,0x3a00,&rdval))
      LOG_ERR_RET(sensor_write(sd,0x3a00,rdval|0x20))//turn on band filter
      break;
    case V4L2_CID_POWER_LINE_FREQUENCY_60HZ:
      LOG_ERR_RET(sensor_write(sd,0x3c00,0x00))//60hz 
      LOG_ERR_RET(sensor_write(sd,0x3c01,0x80))//manual band filter
      LOG_ERR_RET(sensor_read(sd,0x3a00,&rdval))
      LOG_ERR_RET(sensor_write(sd,0x3a00,rdval|0x20))//turn on band filter
      break;
    case V4L2_CID_POWER_LINE_FREQUENCY_AUTO:
      break;
    default:
      break;
  }
  //msleep(10);
  info->band_filter = value;
  return 0;
}

/* *********************************************end of ******************************************** */

static int sensor_g_brightness(struct v4l2_subdev *sd, __s32 *value)
{
  struct sensor_info *info = to_state(sd);
  
  *value = info->brightness;
  return 0;
}

static int sensor_s_brightness(struct v4l2_subdev *sd, int value)
{
  struct sensor_info *info = to_state(sd);
  
  if(info->brightness == value)
    return 0;
  
  if(value < -4 || value > 4)
    return -ERANGE;
  
  LOG_ERR_RET(sensor_write_array(sd, sensor_brightness[value+4].regs, sensor_brightness[value+4].size))

  info->brightness = value;
  return 0;
}

static int sensor_g_contrast(struct v4l2_subdev *sd, __s32 *value)
{
  struct sensor_info *info = to_state(sd);
  
  *value = info->contrast;
  return 0;
}

static int sensor_s_contrast(struct v4l2_subdev *sd, int value)
{
  struct sensor_info *info = to_state(sd);
  
  if(info->contrast == value)
    return 0;
  
  if(value < -4 || value > 4)
    return -ERANGE;
    
  LOG_ERR_RET(sensor_write_array(sd, sensor_contrast[value+4].regs, sensor_contrast[value+4].size))
  
  info->contrast = value;
  return 0;
}

static int sensor_g_saturation(struct v4l2_subdev *sd, __s32 *value)
{
  struct sensor_info *info = to_state(sd);
  
  *value = info->saturation;
  return 0;
}

static int sensor_s_saturation(struct v4l2_subdev *sd, int value)
{
  struct sensor_info *info = to_state(sd);
  
  if(info->saturation == value)
    return 0;

  if(value < -4 || value > 4)
    return -ERANGE;
      
  LOG_ERR_RET(sensor_write_array(sd, sensor_saturation[value+4].regs, sensor_saturation[value+4].size))

  info->saturation = value;
  return 0;
}

static int sensor_g_exp_bias(struct v4l2_subdev *sd, __s32 *value)
{
  struct sensor_info *info = to_state(sd);
  
  *value = info->exp_bias;
  return 0;
}

static int sensor_s_exp_bias(struct v4l2_subdev *sd, int value)
{
  struct sensor_info *info = to_state(sd);

  if(info->exp_bias == value)
    return 0;

  if(value < -4 || value > 4)
    return -ERANGE;
      
    sensor_write(sd, 0x3503, 0x07);
    sensor_get_preview_exposure(sd);
    sensor_write(sd, 0x3503, 0x00);
//  if(value>=1)
//  {
//    night_mode=value;
//    //Nfrms=value+1;
//    cap_manual_gain=value*0x10;
//    printk("night_mode=%d Nfrms=%d\n",night_mode, Nfrms);
//    //sensor_write(sd, 0x5690, 0x01);
//  }
//  else
//  {
//    unsigned char lum;
//    night_mode=0;
//    //Nfrms=1;
//    sensor_get_lum(sd);
//    sensor_get_preview_exposure(sd);
//    //sensor_write(sd, 0x5690, 0x00);
//  }
  
  LOG_ERR_RET(sensor_write_array(sd, sensor_ev[value+4].regs, sensor_ev[value+4].size))

  info->exp_bias = value;
  return 0;
}

static int sensor_g_wb(struct v4l2_subdev *sd, int *value)
{
  struct sensor_info *info = to_state(sd);
  enum v4l2_auto_n_preset_white_balance *wb_type = (enum v4l2_auto_n_preset_white_balance*)value;
  
  *wb_type = info->wb;
  
  return 0;
}

static int sensor_s_wb(struct v4l2_subdev *sd,
    enum v4l2_auto_n_preset_white_balance value)
{
  struct sensor_info *info = to_state(sd);
  
  if(info->capture_mode == V4L2_MODE_IMAGE)
    return 0;
  
  if(info->wb == value)
    return 0;
  
  LOG_ERR_RET(sensor_write_array(sd, sensor_wb[value].regs ,sensor_wb[value].size) )
  
  if (value == V4L2_WHITE_BALANCE_AUTO) 
    info->autowb = 1;
  else
    info->autowb = 0;
  
  info->wb = value;
  return 0;
}

static int sensor_g_colorfx(struct v4l2_subdev *sd,
    __s32 *value)
{
  struct sensor_info *info = to_state(sd);
  enum v4l2_colorfx *clrfx_type = (enum v4l2_colorfx*)value;
  
  *clrfx_type = info->clrfx;
  return 0;
}

static int sensor_s_colorfx(struct v4l2_subdev *sd,
    enum v4l2_colorfx value)
{
  struct sensor_info *info = to_state(sd);

  if(info->clrfx == value)
    return 0;
  
  LOG_ERR_RET(sensor_write_array(sd, sensor_colorfx[value].regs, sensor_colorfx[value].size))

  info->clrfx = value;
  return 0;
}

static int sensor_g_flash_mode(struct v4l2_subdev *sd,
    __s32 *value)
{
  struct sensor_info *info = to_state(sd);
  enum v4l2_flash_led_mode *flash_mode = (enum v4l2_flash_led_mode*)value;
  
  *flash_mode = info->flash_mode;
  return 0;
}

static int sensor_s_flash_mode(struct v4l2_subdev *sd,
    enum v4l2_flash_led_mode value)
{
  struct sensor_info *info = to_state(sd);
  vfe_dev_dbg("sensor_s_flash_mode[0x%d]!\n",value);
  
  #ifdef _FLASH_FUNC_
  //vfe_dev_print("config flash mode[0x%d]------------\n",value);
  config_flash_mode(sd, value, info->fl_dev_info);
  #endif
  info->flash_mode = value;
  return 0;
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
      #ifdef _FLASH_FUNC_
      io_set_flash_ctrl(sd, SW_CTRL_FLASH_OFF, to_state(sd)->fl_dev_info);
      #endif
      sensor_s_release_af(sd);
      //software standby
      ret = sensor_write_array(sd, sensor_sw_stby_on_regs ,ARRAY_SIZE(sensor_sw_stby_on_regs));
      if(ret < 0)
        vfe_dev_err("soft stby falied!\n");
      usleep_range(10000,12000);
      //disable io oe
      vfe_dev_print("disalbe oe!\n");
      ret = sensor_write_array(sd, sensor_oe_disable_regs, ARRAY_SIZE(sensor_oe_disable_regs));
      if(ret < 0)
        vfe_dev_err("disalbe oe falied!\n");
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
      vfe_dev_print("enable oe!\n");
      ret = sensor_write_array(sd, sensor_oe_enable_regs,  ARRAY_SIZE(sensor_oe_enable_regs));
      if(ret < 0)
        vfe_dev_err("enable oe falied!\n");
      //software standby
      ret = sensor_write_array(sd, sensor_sw_stby_off_regs ,ARRAY_SIZE(sensor_sw_stby_off_regs));
      if(ret < 0)
        vfe_dev_err("soft stby off falied!\n");
      usleep_range(10000,12000);
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
  
  LOG_ERR_RET(sensor_read(sd, 0x300a, &rdval))
  
  if(rdval != 0x56)
    return -ENODEV;
  
  LOG_ERR_RET(sensor_read(sd, 0x300b, &rdval))
  
  if(rdval != 0x40)
    return -ENODEV;
  
  return 0;
}

static int sensor_init(struct v4l2_subdev *sd, u32 val)
{
  int ret;
  struct sensor_info *info = to_state(sd);
  #ifdef _FLASH_FUNC_
  struct vfe_dev *dev=(struct vfe_dev *)dev_get_drvdata(sd->v4l2_dev->dev);
  #endif
  
  vfe_dev_dbg("sensor_init 0x%x\n",val);
  
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
  info->width = 0;
  info->height = 0;
  info->brightness = 0;
  info->contrast = 0;
  info->saturation = 0;
  info->hue = 0;
  info->hflip = 0;
  info->vflip = 0;
  info->gain = 0;
  info->autogain = 1;
  info->exp_bias = 0;
  info->autoexp = 1;
  info->autowb = 1;
  info->wb = V4L2_WHITE_BALANCE_AUTO;
  info->clrfx = V4L2_COLORFX_NONE;
  info->band_filter = V4L2_CID_POWER_LINE_FREQUENCY_50HZ;

//  info->af_ctrl = V4L2_AF_RELEASE;
  info->tpf.numerator = 1;            
  info->tpf.denominator = 30;    /* 30fps */    
  
  ret = sensor_write_array(sd, sensor_default_regs, ARRAY_SIZE(sensor_default_regs));  
  if(ret < 0) {
    vfe_dev_err("write sensor_default_regs error\n");
    return ret;
  }
    
  sensor_s_band_filter(sd, V4L2_CID_POWER_LINE_FREQUENCY_50HZ);
  
  if(info->stby_mode == 0)
    info->init_first_flag = 0;
  
  info->preview_first_flag = 1;
//  INIT_DELAYED_WORK(&sensor_s_ae_ratio_work, sensor_s_ae_ratio);
  night_mode=0;
	Nfrms = MAX_FRM_CAP;
	
	if(1==AE_CW)
	  sensor_write_array(sd, ae_centerweight_tbl, ARRAY_SIZE(ae_centerweight_tbl));
	else
	  sensor_write_array(sd, ae_average_tbl, ARRAY_SIZE(ae_average_tbl));
	
  #ifdef _FLASH_FUNC_
  if(dev->flash_used==1)
  {
    info->fl_dev_info=&fl_info;
    info->fl_dev_info->dev_if=0;
    info->fl_dev_info->en_pol=FLASH_EN_POL;
    info->fl_dev_info->fl_mode_pol=FLASH_MODE_POL;
    info->fl_dev_info->light_src=0x01;
    info->fl_dev_info->flash_intensity=400;
    info->fl_dev_info->flash_level=0x01;
    info->fl_dev_info->torch_intensity=200;
    info->fl_dev_info->torch_level=0x01;
    info->fl_dev_info->timeout_counter=300*1000;
    config_flash_mode(sd, V4L2_FLASH_LED_MODE_NONE,
                      info->fl_dev_info);
    io_set_flash_ctrl(sd, SW_CTRL_FLASH_OFF, info->fl_dev_info);
  }
  #endif
  return 0;
}

static long sensor_ioctl(struct v4l2_subdev *sd, unsigned int cmd, void *arg)
{
  int ret=0;
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
} sensor_formats[] = {
  {
    .desc   = "YUYV 4:2:2",
    .mbus_code  = V4L2_MBUS_FMT_YUYV8_2X8,
    .regs     = sensor_fmt_yuv422_yuyv,
    .regs_size = ARRAY_SIZE(sensor_fmt_yuv422_yuyv),
    .bpp    = 2,
  },
  {
    .desc   = "YVYU 4:2:2",
    .mbus_code  = V4L2_MBUS_FMT_YVYU8_2X8,
    .regs     = sensor_fmt_yuv422_yvyu,
    .regs_size = ARRAY_SIZE(sensor_fmt_yuv422_yvyu),
    .bpp    = 2,
  },
  {
    .desc   = "UYVY 4:2:2",
    .mbus_code  = V4L2_MBUS_FMT_UYVY8_2X8,
    .regs     = sensor_fmt_yuv422_uyvy,
    .regs_size = ARRAY_SIZE(sensor_fmt_yuv422_uyvy),
    .bpp    = 2,
  },
  {
    .desc   = "VYUY 4:2:2",
    .mbus_code  = V4L2_MBUS_FMT_VYUY8_2X8,
    .regs     = sensor_fmt_yuv422_vyuy,
    .regs_size = ARRAY_SIZE(sensor_fmt_yuv422_vyuy),
    .bpp    = 2,
  },
//  {
//    .desc   = "Raw RGB Bayer",
//    .mbus_code  = V4L2_MBUS_FMT_SBGGR8_1X8,
//    .regs     = sensor_fmt_raw,
//    .regs_size = ARRAY_SIZE(sensor_fmt_raw),
//    .bpp    = 1
//  },
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
    .hoffset    = 0,
    .voffset    = 0,
    .regs       = sensor_qsxga_regs,
    .regs_size  = ARRAY_SIZE(sensor_qsxga_regs),
    .set_size   = NULL,
  },
  /* qxga: 2048*1536 */
  {
    .width      = QXGA_WIDTH,
    .height     = QXGA_HEIGHT,
    .hoffset    = 0,
    .voffset    = 0,
    .regs       = sensor_qxga_regs,
    .regs_size  = ARRAY_SIZE(sensor_qxga_regs),
    .set_size   = NULL,
  },
  /* 1080P */
  {
    .width      = HD1080_WIDTH,
    .height     = HD1080_HEIGHT,
    .hoffset    = 0,
    .voffset    = 0,
    .regs       = sensor_1080p_regs,
    .regs_size  = ARRAY_SIZE(sensor_1080p_regs),
    .set_size   = NULL,
  },
  /* UXGA */
  {
    .width      = UXGA_WIDTH,
    .height     = UXGA_HEIGHT,
    .hoffset    = 0,
    .voffset    = 0,
    .regs       = sensor_uxga_regs,
    .regs_size  = ARRAY_SIZE(sensor_uxga_regs),
    .set_size   = NULL,
  },
  /* SXGA */
  {
    .width      = SXGA_WIDTH,
    .height     = SXGA_HEIGHT,
    .hoffset    = 0,
    .voffset    = 0,
    .regs       = sensor_sxga_regs,
    .regs_size  = ARRAY_SIZE(sensor_sxga_regs),
    .set_size   = NULL,
  },
  /* 720p */
  {
    .width      = HD720_WIDTH,
    .height     = HD720_HEIGHT,
    .hoffset    = 0,
    .voffset    = 0,
    .regs       = sensor_720p_regs,
    .regs_size  = ARRAY_SIZE(sensor_720p_regs),
    .set_size   = NULL,
  },
  /* XGA */
  {
    .width      = XGA_WIDTH,
    .height     = XGA_HEIGHT,
    .hoffset    = 0,
    .voffset    = 0,
    .regs       = sensor_xga_regs,
    .regs_size  = ARRAY_SIZE(sensor_xga_regs),
    .set_size   = NULL,
  },
  /* SVGA */
  {
    .width      = SVGA_WIDTH,
    .height     = SVGA_HEIGHT,
    .hoffset    = 0,
    .voffset    = 0,
    .regs       = sensor_svga_regs,
    .regs_size  = ARRAY_SIZE(sensor_svga_regs),
    .set_size   = NULL,
  },
  /* VGA */
  {
    .width      = VGA_WIDTH,
    .height     = VGA_HEIGHT,
    .hoffset    = 0,
    .voffset    = 0,
    .regs       = sensor_vga_regs,
    .regs_size  = ARRAY_SIZE(sensor_vga_regs),
    .set_size   = NULL,
  },
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
  cfg->type = V4L2_MBUS_PARALLEL;
  cfg->flags = V4L2_MBUS_MASTER | VREF_POL | HREF_POL | CLK_POL ;
  
  return 0;
}

/*
 * Set fps
 */

//static int sensor_s_fps(struct v4l2_subdev *sd)
//{
//  struct sensor_info *info = to_state(sd);
//  unsigned char div,sys_div;
//  unsigned char band_50_high,band_50_low,band_60_high,band_60_low;
//  unsigned char band_50_step,band_60_step,vts_high,vts_low;
//  int band_50,band_60,vts;
//
//  struct regval_list regs_fr[] = {
//    {0x3035,0xee},
//    {0x3a08,0xee},//50HZ step MSB 
//    {0x3a09,0xee},//50HZ step LSB 
//    {0x3a0a,0xee},//60HZ step MSB 
//    {0x3a0b,0xee},//60HZ step LSB 
//    {0x3a0e,0xee},//50HZ step max 
//    {0x3a0d,0xee},//60HZ step max 
//  	//{REG_TERM,VAL_TERM},
//  };
//
//  vfe_dev_dbg("sensor_s_fps\n");
//  
//  if (info->tpf.numerator == 0)
//    return -EINVAL;
//    
//  div = info->tpf.numerator;
//  
//  //power down
////  ret = sensor_write(sd, 0x3008, 0x42);
////  if(ret<0) {
////    vfe_dev_err("power down error at sensor_s_parm!\n");
////    return ret;
////  }
//  
//  LOG_ERR_RET(sensor_read(sd, 0x3035, &sys_div))  
//  LOG_ERR_RET(sensor_read(sd, 0x3a08, &band_50_high))
//  LOG_ERR_RET(sensor_read(sd, 0x3a09, &band_50_low))
//  
//  band_50 = band_50_high*256+band_50_low;
//  
//  LOG_ERR_RET(sensor_read(sd, 0x3a0a, &band_60_high))
//  LOG_ERR_RET(sensor_read(sd, 0x3a0b, &band_60_low))
//    
//  band_60 = band_60_high*256+band_60_low;
//  
//  LOG_ERR_RET(sensor_read(sd, 0x380e, &vts_high)) 
//  LOG_ERR_RET(sensor_read(sd, 0x380f, &vts_low))
//  
//  vts = vts_high*256+vts_low;
//  
////  vfe_dev_dbg("sys_div=%x,band50=%x,band_60=%x\n",sys_div,band_50,band_60);
//  
//  sys_div = (sys_div & 0x0f) | ((sys_div & 0xf0)*div);
//  band_50 = band_50/div;
//  band_60 = band_60/div;
//  band_50_step = vts/band_50;
//  band_60_step = vts/band_60;
//  
////  vfe_dev_dbg("sys_div=%x,band50=%x,band_60=%x,band_50_step=%x,band_60_step=%x\n",sys_div,band_50,band_60,band_50_step,band_60_step);
//  
//  regs_fr[0].data = sys_div;
//  regs_fr[1].data = (band_50&0xff00)>>8;
//  regs_fr[2].data = (band_50&0x00ff)>>0;
//  regs_fr[3].data = (band_60&0xff00)>>8;
//  regs_fr[4].data = (band_60&0x00ff)>>0;
//  regs_fr[5].data = band_50_step;
//  regs_fr[6].data = band_60_step;
//  
//  LOG_ERR_RET(sensor_write_array(sd, regs_fr, ARRAY_SIZE(regs_fr)))
//  
////#if DEV_DBG_EN == 1 
////  {
////    int i;  
////    for(i=0;i<7;i++) {
////      sensor_read(sd,regs_fr[i].reg_num,regs_fr[i].value);
////      vfe_dev_print("address 0x%2x%2x = %4x",regs_fr[i].reg_num[0],regs_fr[i].reg_num[1],regs_fr[i].value[0]);
////    }
////  }
////#endif
//	
////	//release power down
////  ret = sensor_write(sd, 0x3008, 0x02);
////  if(ret<0) {
////    vfe_dev_err("release power down error at sensor_s_parm!\n");
////    return ret;
////  }
//  
//  //msleep(500);
//  vfe_dev_dbg("set frame rate %d\n",info->tpf.denominator/info->tpf.numerator);
//  
//  return 0;
//}

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
  
  sensor_write_array(sd, sensor_oe_disable_regs ,ARRAY_SIZE(sensor_oe_disable_regs));
  
  ret = sensor_try_fmt_internal(sd, fmt, &sensor_fmt, &wsize);
  if (ret)
    return ret;
  
  if(info->capture_mode == V4L2_MODE_VIDEO)
  {
    //video
    #ifdef _FLASH_FUNC_
    if(info->flash_mode!=V4L2_FLASH_LED_MODE_NONE)
    {
      //printk("shut flash when preview\n");
      io_set_flash_ctrl(sd, SW_CTRL_FLASH_OFF, info->fl_dev_info);
    }
    #endif
  }
  else if(info->capture_mode == V4L2_MODE_IMAGE)
  {
    //image 
    ret = sensor_s_autoexp(sd,V4L2_EXPOSURE_MANUAL);
    if (ret < 0)
      vfe_dev_err("sensor_s_autoexp off err when capturing image!\n");
    
    ret = sensor_s_autogain(sd,0);
    if (ret < 0)
      vfe_dev_err("sensor_s_autogain off err when capturing image!\n");
    
    if (wsize->width > SVGA_WIDTH) {
      #ifdef _FLASH_FUNC_
      check_to_flash(sd);
      #endif
      sensor_get_lum(sd);
      sensor_get_preview_exposure(sd);
      sensor_get_fps(sd);
//      ret = sensor_set_capture_exposure(sd);
//      if (ret < 0)
//        vfe_dev_err("sensor_set_capture_exposure err !\n");
    }
    
    #ifdef _FLASH_FUNC_
    if(info->flash_mode!=V4L2_FLASH_LED_MODE_NONE)
    {
      if(to_flash==1)
      {
        vfe_dev_cap_dbg("open flash when capture\n");
        io_set_flash_ctrl(sd, SW_CTRL_FLASH_ON, info->fl_dev_info);
        sensor_get_lum(sd);
        sensor_get_preview_exposure(sd);
        sensor_get_fps(sd);
        msleep(50);
      }
    }
    #endif
    
    ret = sensor_s_autowb(sd,0); //lock wb
    if (ret < 0)
      vfe_dev_err("sensor_s_autowb off err when capturing image!\n");
  }
  
  sensor_write_array(sd, sensor_fmt->regs, sensor_fmt->regs_size);
  
  //printk("wsize->regs_size=%d\n", wsize->regs_size);
  if (wsize->regs)
    LOG_ERR_RET(sensor_write_array(sd, wsize->regs, wsize->regs_size))
  
  if (wsize->set_size)
    LOG_ERR_RET(wsize->set_size(sd))
  
  sensor_s_hflip(sd,info->hflip);
  sensor_s_vflip(sd,info->vflip);
  
  if(info->capture_mode == V4L2_MODE_VIDEO ||
  	info->capture_mode == V4L2_MODE_PREVIEW)
  {
    //video
    	//sensor_s_fps(sd);
    	
#ifdef AUTO_FPS		
		if(info->capture_mode == V4L2_MODE_PREVIEW) {
	      sensor_write_array(sd, sensor_auto_fps_mode, ARRAY_SIZE(sensor_auto_fps_mode));
	    } else {
	      sensor_write_array(sd, sensor_fix_fps_mode, ARRAY_SIZE(sensor_fix_fps_mode));
	    }
#endif    	
    
    ret = sensor_set_preview_exposure(sd);
    if (ret < 0)
      vfe_dev_err("sensor_set_preview_exposure err !\n");
      
    ret = sensor_s_autoexp(sd,V4L2_EXPOSURE_AUTO);
    if (ret < 0)
      vfe_dev_err("sensor_s_autoexp on err when capturing video!\n");
    
    ret = sensor_s_autogain(sd,1);
    if (ret < 0)
      vfe_dev_err("sensor_s_autogain on err when capturing video!\n");    
    
    if (info->wb == V4L2_WHITE_BALANCE_AUTO) {
      ret = sensor_s_autowb(sd,1); //unlock wb
      if (ret < 0)
        vfe_dev_err("sensor_s_autowb on err when capturing image!\n");
    }
		
    if(info->capture_mode == V4L2_MODE_VIDEO) {
      //printk("~~~~~~~~~set sharpness and dns~~~~~~~\n");
      if(wsize->width==640)
      {
        sensor_s_sharpness_value(sd,0x20);
  	    sensor_s_denoise_value(sd,0x04);
      }
  		else if(wsize->height==960)
      {
        sensor_s_sharpness_value(sd,0x08);
  	    sensor_s_denoise_value(sd,0x08);
      }
  		else if(wsize->height==720)
      {
        sensor_s_sharpness_value(sd,0x08);
  	    sensor_s_denoise_value(sd,0x04);
      }
  		else if(wsize->width==1920)
      {
        sensor_s_sharpness_value(sd,0x08);
  	    sensor_s_denoise_value(sd,0x14);
      }
  		else
  		{
  		  sensor_s_sharpness_auto(sd); //sharpness auto
  		  sensor_s_denoise_auto(sd);
  		}
  	} else if(info->capture_mode == V4L2_MODE_PREVIEW) {
  	  sensor_s_sharpness_value(sd,0x20); //sharpness fix value
  	  sensor_s_denoise_value(sd,0x10);
//  		  sensor_s_sharpness_auto(sd); //sharpness auto
//  		  sensor_s_denoise_auto(sd);
  	}
    
		if(info->low_speed == 1) {
			if(info->preview_first_flag == 1) {
				info->preview_first_flag = 0;
				msleep(600);
			} else {
				msleep(200);
			}		
		}
		
		if( (info->width!=QSXGA_WIDTH)&&(info->preview_first_flag != 1) )
		{  
  		ret = sensor_s_relaunch_af_zone(sd);
  		if (ret < 0) {
  			vfe_dev_err("sensor_s_relaunch_af_zone err !\n");
  			//return ret;
  		}
  		
  		//msleep(100);
  		ret = sensor_write(sd, 0x3022, 0x03);		//sensor_s_single_af
  		if (ret < 0) {
  			vfe_dev_err("sensor_s_single_af err !\n");
  			//return ret;
  		}
  		
  		if(info->auto_focus==1)
  			sensor_s_continueous_af(sd, 1);
		  
		  msleep(100);
		}
		else
		  msleep(150);
	} else {
    if (wsize->width > SVGA_WIDTH) {
      ret = sensor_set_capture_exposure(sd);
      if (ret < 0)
        vfe_dev_err("sensor_set_capture_exposure err !\n");
    }
    
		//capture image
		sensor_s_sharpness_value(sd,SHARPNESS); //sharpness 0x0
		//sensor_s_sharpness_auto(sd); //sharpness auto
    
		if(info->low_speed == 1) {
			unsigned char rdval;
			sensor_read(sd,0x3035,&rdval);
			sensor_write(sd,0x3035,(rdval&0x0f)|((rdval&0xf0)*2));
			//sensor_write(sd,0x3037,0x14);
		}
		
		msleep(150);
	}
	
  info->fmt = sensor_fmt;
  info->width = wsize->width;
  info->height = wsize->height;
  
  vfe_dev_print("s_fmt set width = %d, height = %d\n",wsize->width,wsize->height);
  
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
	
	cp->timeperframe.numerator = info->tpf.numerator;
	cp->timeperframe.denominator = info->tpf.denominator;
	 
	return 0;
}

static int sensor_s_parm(struct v4l2_subdev *sd, struct v4l2_streamparm *parms)
{
  struct v4l2_captureparm *cp = &parms->parm.capture;
  struct v4l2_fract *tpf = &cp->timeperframe;
  struct sensor_info *info = to_state(sd);
  unsigned char div;
  
  vfe_dev_dbg("sensor_s_parm\n");
  
  if (parms->type != V4L2_BUF_TYPE_VIDEO_CAPTURE){
  	vfe_dev_dbg("parms->type!=V4L2_BUF_TYPE_VIDEO_CAPTURE\n");
    return -EINVAL;
  }
  
  if (info->tpf.numerator == 0){
  	vfe_dev_dbg("info->tpf.numerator == 0\n");
    return -EINVAL;
  }
    
  info->capture_mode = cp->capturemode;
  
  if (info->capture_mode == V4L2_MODE_IMAGE) {
    vfe_dev_dbg("capture mode is not video mode,can not set frame rate!\n");
    return 0;
  }
    
  if (tpf->numerator == 0 || tpf->denominator == 0) {
    tpf->numerator = 1;
    tpf->denominator = SENSOR_FRAME_RATE;/* Reset to full rate */
    vfe_dev_err("sensor frame rate reset to full rate!\n");
  }
  
  div = SENSOR_FRAME_RATE/(tpf->denominator/tpf->numerator);
  if(div > 15 || div == 0)
  {
  	vfe_dev_print("SENSOR_FRAME_RATE=%d\n",SENSOR_FRAME_RATE);
  	vfe_dev_print("tpf->denominator=%d\n",tpf->denominator);
  	vfe_dev_print("tpf->numerator=%d\n",tpf->numerator);
    return -EINVAL;
  }
  
  vfe_dev_dbg("set frame rate %d\n",tpf->denominator/tpf->numerator);
  
  info->tpf.denominator = SENSOR_FRAME_RATE; 
  info->tpf.numerator = div;
  
	if(info->tpf.denominator/info->tpf.numerator < 30)
		info->low_speed = 1;
    
  return 0;
}


/* 
 * Code for dealing with controls.
 * fill with different sensor module
 * different sensor module has different settings here
 * if not support the follow function ,retrun -EINVAL
 */

/* *********************************************begin of ******************************************** */
static int sensor_queryctrl(struct v4l2_subdev *sd,
    struct v4l2_queryctrl *qc)
{
  /* Fill in min, max, step and default value for these controls. */
  /* see include/linux/videodev2.h for details */
//  vfe_dev_dbg("queryctrl qc->id=0x%8x\n", qc->id);
  switch (qc->id) {
//  case V4L2_CID_BRIGHTNESS:
//    return v4l2_ctrl_query_fill(qc, -4, 4, 1, 1);
//  case V4L2_CID_CONTRAST:
//    return v4l2_ctrl_query_fill(qc, -4, 4, 1, 1);
//  case V4L2_CID_SATURATION:
//    return v4l2_ctrl_query_fill(qc, -4, 4, 1, 1);
//  case V4L2_CID_HUE:
//    return v4l2_ctrl_query_fill(qc, -180, 180, 5, 0);
  case V4L2_CID_VFLIP:
  case V4L2_CID_HFLIP:
    return v4l2_ctrl_query_fill(qc, 0, 1, 1, 0);
//  case V4L2_CID_GAIN:
//    return v4l2_ctrl_query_fill(qc, 0, 255, 1, 128);
//  case V4L2_CID_AUTOGAIN:
//    return v4l2_ctrl_query_fill(qc, 0, 1, 1, 1);
  case V4L2_CID_EXPOSURE:
  case V4L2_CID_AUTO_EXPOSURE_BIAS:
    return v4l2_ctrl_query_fill(qc, -4, 4, 1, 0);
  case V4L2_CID_EXPOSURE_AUTO:
    return v4l2_ctrl_query_fill(qc, 0, 1, 1, 0);
  case V4L2_CID_AUTO_N_PRESET_WHITE_BALANCE:
    return v4l2_ctrl_query_fill(qc, 0, 9, 1, 1);
  case V4L2_CID_AUTO_WHITE_BALANCE:
    return v4l2_ctrl_query_fill(qc, 0, 1, 1, 1);
  case V4L2_CID_COLORFX:
    return v4l2_ctrl_query_fill(qc, 0, 15, 1, 0);
  case V4L2_CID_FLASH_LED_MODE:
    return v4l2_ctrl_query_fill(qc, 0, 4, 1, 0);  
  
  case V4L2_CID_3A_LOCK:
    return v4l2_ctrl_query_fill(qc, 0, V4L2_LOCK_FOCUS, 1, 0);
//  case V4L2_CID_AUTO_FOCUS_RANGE:
//    return v4l2_ctrl_query_fill(qc, 0, 0, 0, 0);//only auto
  case V4L2_CID_AUTO_FOCUS_INIT:
  case V4L2_CID_AUTO_FOCUS_RELEASE:
  case V4L2_CID_AUTO_FOCUS_START:
  case V4L2_CID_AUTO_FOCUS_STOP:
  case V4L2_CID_AUTO_FOCUS_STATUS:
    return v4l2_ctrl_query_fill(qc, 0, 0, 0, 0);
  case V4L2_CID_FOCUS_AUTO:
    return v4l2_ctrl_query_fill(qc, 0, 1, 1, 0);
  case V4L2_CID_AUTO_EXPOSURE_WIN_NUM:
    return v4l2_ctrl_query_fill(qc, 0, 1, 1, 0);
  case V4L2_CID_AUTO_FOCUS_WIN_NUM:
    return v4l2_ctrl_query_fill(qc, 0, 1, 1, 0);
  }
  return -EINVAL;
}

static int sensor_g_ctrl(struct v4l2_subdev *sd, struct v4l2_control *ctrl)
{
  //vfe_dev_dbg("sensor_g_ctrl ctrl->id=0x%8x\n", ctrl->id);
  switch (ctrl->id) {
  case V4L2_CID_BRIGHTNESS:
    return sensor_g_brightness(sd, &ctrl->value);
  case V4L2_CID_CONTRAST:
    return sensor_g_contrast(sd, &ctrl->value);
  case V4L2_CID_SATURATION:
    return sensor_g_saturation(sd, &ctrl->value);
  case V4L2_CID_HUE:
    return sensor_g_hue(sd, &ctrl->value);  
  case V4L2_CID_VFLIP:
    return sensor_g_vflip(sd, &ctrl->value);
  case V4L2_CID_HFLIP:
    return sensor_g_hflip(sd, &ctrl->value);
  case V4L2_CID_GAIN:
    return sensor_g_gain(sd, &ctrl->value);
  case V4L2_CID_AUTOGAIN:
    return sensor_g_autogain(sd, &ctrl->value);
  case V4L2_CID_EXPOSURE:
  case V4L2_CID_AUTO_EXPOSURE_BIAS:
    return sensor_g_exp_bias(sd, &ctrl->value);
  case V4L2_CID_EXPOSURE_AUTO:
    return sensor_g_autoexp(sd, &ctrl->value);
  case V4L2_CID_AUTO_N_PRESET_WHITE_BALANCE:
    return sensor_g_wb(sd, &ctrl->value);
  case V4L2_CID_AUTO_WHITE_BALANCE:
    return sensor_g_autowb(sd, &ctrl->value);
  case V4L2_CID_COLORFX:
    return sensor_g_colorfx(sd, &ctrl->value);
  case V4L2_CID_FLASH_LED_MODE:
    return sensor_g_flash_mode(sd, &ctrl->value);
  case V4L2_CID_POWER_LINE_FREQUENCY:
    return sensor_g_band_filter(sd, &ctrl->value);
  
  case V4L2_CID_3A_LOCK:
  	return sensor_g_3a_lock(sd);
//  case V4L2_CID_AUTO_FOCUS_RANGE:
//  	ctrl->value=0;//only auto
//  	return 0;
//  case V4L2_CID_AUTO_FOCUS_INIT:
//  case V4L2_CID_AUTO_FOCUS_RELEASE:
//  case V4L2_CID_AUTO_FOCUS_START:
//  case V4L2_CID_AUTO_FOCUS_STOP:
  case V4L2_CID_AUTO_FOCUS_STATUS:
  	return sensor_g_af_status(sd);
//  case V4L2_CID_FOCUS_AUTO:
  case V4L2_CID_AUTO_FOCUS_WIN_NUM:
  	ctrl->value=1;
  	return 0;
  case V4L2_CID_AUTO_EXPOSURE_WIN_NUM:
  	ctrl->value=1;
  	return 0;
  }
  return -EINVAL;
}

static int sensor_s_ctrl(struct v4l2_subdev *sd, struct v4l2_control *ctrl)
{
  struct v4l2_queryctrl qc;
  int ret;
  
//  vfe_dev_dbg("sensor_s_ctrl ctrl->id=0x%8x\n", ctrl->id);
  qc.id = ctrl->id;
  ret = sensor_queryctrl(sd, &qc);
  if (ret < 0) {
    return ret;
  }

	if (qc.type == V4L2_CTRL_TYPE_MENU ||
		qc.type == V4L2_CTRL_TYPE_INTEGER ||
		qc.type == V4L2_CTRL_TYPE_BOOLEAN)
	{
	  if (ctrl->value < qc.minimum || ctrl->value > qc.maximum) {
	    return -ERANGE;
	  }
	}
	
  switch (ctrl->id) {
    case V4L2_CID_BRIGHTNESS:
      return sensor_s_brightness(sd, ctrl->value);
    case V4L2_CID_CONTRAST:
      return sensor_s_contrast(sd, ctrl->value);
    case V4L2_CID_SATURATION:
      return sensor_s_saturation(sd, ctrl->value);
    case V4L2_CID_HUE:
      return sensor_s_hue(sd, ctrl->value);   
    case V4L2_CID_VFLIP:
      return sensor_s_vflip(sd, ctrl->value);
    case V4L2_CID_HFLIP:
      return sensor_s_hflip(sd, ctrl->value);
    case V4L2_CID_GAIN:
      return sensor_s_gain(sd, ctrl->value);
    case V4L2_CID_AUTOGAIN:
      return sensor_s_autogain(sd, ctrl->value);
    case V4L2_CID_EXPOSURE:
    case V4L2_CID_AUTO_EXPOSURE_BIAS:
      return sensor_s_exp_bias(sd, ctrl->value);
    case V4L2_CID_EXPOSURE_AUTO:
      return sensor_s_autoexp(sd,
          (enum v4l2_exposure_auto_type) ctrl->value);
    case V4L2_CID_AUTO_N_PRESET_WHITE_BALANCE:
  		return sensor_s_wb(sd,
          (enum v4l2_auto_n_preset_white_balance) ctrl->value); 
    case V4L2_CID_AUTO_WHITE_BALANCE:
      return sensor_s_autowb(sd, ctrl->value);
    case V4L2_CID_COLORFX:
      return sensor_s_colorfx(sd,
          (enum v4l2_colorfx) ctrl->value);
    case V4L2_CID_FLASH_LED_MODE:
      return sensor_s_flash_mode(sd,
          (enum v4l2_flash_led_mode) ctrl->value);
    case V4L2_CID_POWER_LINE_FREQUENCY:
      return sensor_s_band_filter(sd,
          (enum v4l2_power_line_frequency) ctrl->value);
    
    case V4L2_CID_3A_LOCK:
    	return sensor_s_3a_lock(sd, ctrl->value);
//    case V4L2_CID_AUTO_FOCUS_RANGE:
//  	  return 0;
	  case V4L2_CID_AUTO_FOCUS_INIT:
	  	return sensor_s_init_af(sd);
	  case V4L2_CID_AUTO_FOCUS_RELEASE:
	  	return sensor_s_release_af(sd);
	  case V4L2_CID_AUTO_FOCUS_START:
	  	return sensor_s_single_af(sd);
	  case V4L2_CID_AUTO_FOCUS_STOP:
	  	return sensor_s_pause_af(sd);
	//  case V4L2_CID_AUTO_FOCUS_STATUS:
	  case V4L2_CID_FOCUS_AUTO:
	  	return sensor_s_continueous_af(sd, ctrl->value);
	  case V4L2_CID_AUTO_FOCUS_WIN_NUM:
	  	vfe_dev_dbg("s_ctrl win value=%d\n",ctrl->value);
	  	return sensor_s_af_zone(sd, (struct v4l2_win_coordinate *)(ctrl->user_pt));
	  case V4L2_CID_AUTO_EXPOSURE_WIN_NUM:
	  	return 0;
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
  info->auto_focus = 0;

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
  { "ov5640", 0 },
  { }
};
MODULE_DEVICE_TABLE(i2c, sensor_id);


static struct i2c_driver sensor_driver = {
  .driver = {
    .owner = THIS_MODULE,
  .name = "ov5640",
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

