/*
 *
 *  bsp_isp_algo.h
 *
 *  Author: yangfeng
 *  Version: 1.0
 *  
 */
#ifndef __BSP__ISP__ALGO__H
#define __BSP__ISP__ALGO__H

#include "isp_module_cfg.h" //TODO

//#include <linux/mutex.h>
#define  MAX(a,b)              (((a) > (b)) ? (a) : (b))
#define  MIN(a,b)              (((a) < (b)) ? (a) : (b))
#define  SQUARE(x)             ((x) * (x))
#define  CLIP(a,i,s)           (((a) > (s)) ? (s) : MAX(a,i))

#define  SATURATION_MAX             150
#define  ISP_DEFOG_MIN_RGB          1023

/* General 3A Window Range Constants */
#define ISP_3A_WIN_HOR_NUM_MIN      1
#define ISP_3A_WIN_HOR_NUM_MAX      8
#define ISP_3A_WIN_VER_NUM_MIN      1
#define ISP_3A_WIN_VER_NUM_MAX      8 
#define ISP_3A_WIN_HEIGHT_MIN       3
#define ISP_3A_WIN_HEIGHT_MAX       2047
#define ISP_3A_WIN_WIDTH_MIN        3
#define ISP_3A_WIN_WIDTH_MAX        2047
#define ISP_3A_WIN_HOR_START_MIN    0
#define ISP_3A_WIN_HOR_START_MAX    2047
#define ISP_3A_WIN_VER_START_MIN    0
#define ISP_3A_WIN_VER_START_MAX    2047

#define ISP_RED_INDEX               0
#define ISP_GREEN_INDEX             1
#define ISP_BLUE_INDEX              2

#define ISP_LENS_SLOP_OFFSET        3
#define ISP_LENS_TBL_NUM            6


/* AE Range Constants */
#define ISP_AE_HIGH_BRI_TH_MAX  
#define ISP_AE_LOW_BRI_TH_MAX    

#define ISP_AE_EXP_CHANGE_INTER    2
#define ISP_AE_DIG_CHANGE_INTER    0


/* AWB Range Constants */
#define ISP_AWB_R_SAT_LIM_MAX
#define ISP_AWB_G_SAT_LIM_MAX
#define ISP_AWB_B_SAT_LIM_MAX
#define ISP_AWB_SUM_TH_MAX    

/* AF Range Constant */
#define ISP_AF_SAP_LIM_MAX

/* HIST Range Constants */
#define ISP_HIST_HEIGHT_MIN      0
#define ISP_HIST_HEIGHT_MAX      4096
#define ISP_HIST_WIDTH_MIN       0
#define ISP_HIST_WIDTH_MAX       4096
#define ISP_HIST_HOR_START_MIN   0
#define ISP_HIST_HOR_START_MAX   4095
#define ISP_HIST_VER_START_MIN   0
#define ISP_HIST_VER_START_MAX   4095

#define ISP_SHARP_LEVEL_MIN      0
#define ISP_SHARP_LEVEL_MAX      15
#define ISP_SHARP_MIN_VALUE      32
#define ISP_SHARP_MAX_VALUE      128

/* 3A Default Constants */
#define DBG_FRAME_WIDTH          2592
#define DBG_FRAME_HEIGHT         1936



typedef enum isp_bool
{
  ISP_FALSE    = 0,
  ISP_TRUE     = !ISP_FALSE,
} isp_bool;

enum auto_focus_mode
{
  AF_MANUAL               = 0,
  AF_AUTO_CONTINUEOUS     = 1,
  AF_AUTO_SINGLE          = 2,
};

enum auto_focus_win_mode
{
  AF_AUTO_WIN             = 0,    
  AF_NUM_WIN              = 1,
};

enum auto_focus_range
{       
  AF_RANGE_AUTO           = 0,
  AF_RANGE_NORMAL         = 1,
  AF_RANGE_MACRO          = 2,
  AF_RANGE_INFINITY       = 3,
};

enum auto_focus_status
{
  AUTO_FOCUS_STATUS_IDLE     =0,
  AUTO_FOCUS_STATUS_BUSY     =1,
  AUTO_FOCUS_STATUS_REACHED  =2,
  AUTO_FOCUS_STATUS_APPROCH  =3,
  AUTO_FOCUS_STATUS_REFOCUS  =4,
  AUTO_FOCUS_STATUS_FINDED   =5,
  AUTO_FOCUS_STATUS_FAILED   =6,
};

enum exposure_mode
{
  EXP_AUTO         = 0,
  EXP_MANUAL       = 1,
};

enum exposure_win_mode 
{
  AE_AUTO_WIN     = 0,
  AE_SINGLE_WIN   = 1,
};

enum iso_mode
{
  ISO_MANUAL         = 0,
  ISO_AUTO,
};

enum auto_exposure_status 
{
  AUTO_EXPOSURE_STATUS_IDLE      =0,
  AUTO_EXPOSURE_STATUS_BUSY      =1,
};

enum power_line_frequency 
{
  FREQUENCY_DISABLED  = 0,
  FREQUENCY_50HZ      = 1,
  FREQUENCY_60HZ      = 2,
  FREQUENCY_AUTO      = 3,
};

enum detected_flicker_type 
{
  FLICKER_NO        = 0,
  FLICKER_50HZ      = 1,
  FLICKER_60HZ      = 2,
};

enum scene_mode 
{
  SCENE_MODE_NONE            = 0,
  SCENE_MODE_BACKLIGHT       = 1,
  SCENE_MODE_BEACH_SNOW      = 2,
  SCENE_MODE_CANDLE_LIGHT    = 3,
  SCENE_MODE_DAWN_DUSK       = 4,
  SCENE_MODE_FALL_COLORS     = 5,
  SCENE_MODE_FIREWORKS       = 6,
  SCENE_MODE_LANDSCAPE       = 7,
  SCENE_MODE_NIGHT           = 8,
  SCENE_MODE_PARTY_INDOOR    = 9,
  SCENE_MODE_PORTRAIT        = 10,
  SCENE_MODE_SPORTS          = 11,
  SCENE_MODE_SUNSET          = 12,
  SCENE_MODE_TEXT            = 13,
};

enum colorfx
{
  COLORFX_NONE               = 0,
  COLORFX_BW                 = 1,
  COLORFX_SEPIA              = 2,
  COLORFX_NEGATIVE           = 3,
  COLORFX_EMBOSS             = 4,
  COLORFX_SKETCH             = 5,
  COLORFX_SKY_BLUE           = 6,
  COLORFX_GRASS_GREEN        = 7,
  COLORFX_SKIN_WHITEN        = 8,
  COLORFX_VIVID              = 9,
  COLORFX_AQUA               = 10,
  COLORFX_ART_FREEZE         = 11,
  COLORFX_SILHOUETTE         = 12,
  COLORFX_SOLARIZATION       = 13,
  COLORFX_ANTIQUE            = 14,
  COLORFX_SET_CBCR           = 15,
};

enum white_balance_mode
{        
  WB_MANUAL        = 0,
  WB_AUTO          = 1,
  WB_INCANDESCENT  = 2,
  WB_FLUORESCENT   = 3,
  WB_FLUORESCENT_H = 4,
  WB_HORIZON       = 5,
  WB_DAYLIGHT      = 6,
  WB_FLASH         = 7,
  WB_CLOUDY        = 8,
  WB_SHADE         = 9,
  WB_TUNGSTEN      = 10,
};

enum auto_white_balance_status
{
  AUTO_WHITE_BALANECE_STATUS_IDLE     = 0,
  AUTO_WHITE_BALANECE_BUSY            = 1,
};

enum gsensor_direction
{
  GSENSOR_ANGLE_0     = 0,
  GSENSOR_ANGLE_90,
  GSENSOR_ANGLE_180,
  GSENSOR_ANGLE_270,
};
enum flash_mode
{
  FLASH_MODE_OFF      = 0,
  FLASH_MODE_ON       = 1,
  FLASH_MODE_TORCH    = 2,
  FLASH_MODE_AUTO     = 3,
  FLASH_MODE_RED_EYE  = 4,
};

enum isp_stat_buf_status
{
  BUF_IDLE            = 0,
  BUF_ACTIVE          = 1,
  BUF_LOCKED          = 2,
};


enum frame_rate 
{
  FRAME_RATE_DIV_BY_1        = 120,
  FRAME_RATE_DIV_BY_1_dot_5  = 80,
  FRAME_RATE_DIV_BY_2        = 60,
  FRAME_RATE_DIV_BY_2_dot_5  = 48,
  FRAME_RATE_DIV_BY_3        = 40,
  FRAME_RATE_DIV_BY_4        = 30,
  FRAME_RATE_DIV_BY_6        = 20,
  FRAME_RATE_DIV_BY_8        = 15,
};

enum isp_test_mode
{  
  ISP_TEST_ALL_ENABLE           = 0,
  ISP_TEST_BAYER_GAINOFFSET     = 1,
  ISP_TEST_LENS                 = 2,
  ISP_TEST_GAMMA                = 3,
  ISP_TEST_COLOR                = 4,
  ISP_TEST_COLOR_MATRIX         = 5,
  ISP_TEST_VCM_RANGE            = 6,
  ISP_TEST_MANUAL    			= 7,
};

struct sensor_band_step_config
{
  //unsigned int reCal;
  //unsigned int sensor_change_flag;
  unsigned int t_line;
  unsigned int band_step_50hz;
  unsigned int band_step_60hz;
};

struct vcm_para
{
  int vcm_curr_code;
  int vcm_max_code;
  int vcm_min_code;
  int vcm_step;
  int vcm_table;
};

struct isp_stat_buffer
{
  unsigned int buf_size;
  unsigned int frame_number;
  unsigned int cur_frame;
  unsigned int config_counter;  
  enum isp_stat_buf_status buf_status;
  void *stat_buf;
};

struct h3a_win
{ 
  struct isp_h3a_coor_win af_coor;
  struct isp_h3a_coor_win awb_coor;
  
  /* AE and HIST windows should be same */
  struct isp_h3a_coor_win ae_coor;
  struct isp_h3a_coor_win hist_coor;  
};

/*
 *   
 *   struct isp_3a_output - Stores the results of 3A.
 *   It will be used to adjust exposure time, vcm and other regs.
 * 
 */
struct isp_3a_result
{
  /* AE Output */
  unsigned int exp_line_num;          //20bits,Q4
  unsigned int exp_analog_gain;       //16bits,Q8
  unsigned int exp_digital_gain;      //16bits,Q8
  unsigned int ae_gain;               //16bits,Q8
  unsigned int sensor_exp_gain;
  unsigned int night_mode_change;  
  unsigned int night_mode;

  unsigned int exp_value;                       /* us */
  int min_rgb_pre[8];

  unsigned int exp_temp;              //20bits,Q4
  unsigned int analog_gain_temp;      //16bits,Q8
  unsigned int digital_gain_temp;     //16bits,Q8
  unsigned int frame_rate_temp;

  unsigned int ae_interval_cnt;
  unsigned int ae_frame_cnt_temp;
  
  unsigned int ae_algo_adjust_cnt;      
  unsigned int ae_algo_adjust_interval; 

  unsigned int ae_avp_pre_buf[64];
  unsigned int ae_change_af_flag;
  unsigned int ae_avp_to_af;
  
  struct isp_denoise filter_2d_coef;
  enum isp_bndf_mode filter_mode;
    
  enum scene_mode auto_scene_mode;
  unsigned int avg_bri;
  int flash_on;
  int hdr_req;

  int iso_value;                  /* ISO*/
  int exp_time;                   /* us */
  unsigned int exp_cal_gain;      /* AE_GAIN */

  /* Flicker Output */
  
  unsigned int ae_exp_time;        
  unsigned int band_step;           
  unsigned int auto_afs_flag;
  
  enum detected_flicker_type flicker_type;
  unsigned int band_step_50;
  unsigned int band_step_60;
  
  /* AF Output */
  int real_vcm_step;
  unsigned int real_vcm_pos;
  enum auto_focus_status af_status;
  unsigned int image_quality;

  //struct mutex isp_3a_result_mutex;
};

/*
 *   
 *   struct isp_driver_to_3a_stat - Stores the 3A stat buffer and related settings.
 * 
 */
struct isp_driver_to_3a_stat
{  
  /* v4l2 drivers fill */
  /* Frame isp_size and scale ratio will be used to set the current stat window */
  struct isp_size pic_size; 
  
  /*added by zenglingying 2013-2-26 18:48:57*/
  /*save the current image timing and cfg limits*/
  unsigned int hoffset;     //receive hoffset from sensor output
  unsigned int voffset;     //receive voffset from sensor output
  unsigned int hts;         //h size of timing, unit: pclk
  unsigned int vts;         //v size of timing, unit: line
  unsigned int pclk;        //pixel clock of 1sec
  unsigned int intg_min;    //integration min, unit: line, Q8
  unsigned int intg_max;    //integration max, unit: line, Q8
  unsigned int fps_fixed;   //fps mode 1=fixed fps
                            //N=varied fps to 1/N of org fps
  unsigned int bin_factor;  //binning factor
  unsigned int gain_min;    //sensor gain min, Q8
  unsigned int gain_max;    //sensor gain max, Q8
  
  int have_shading;
  int isp_lens_cx;
  int isp_lens_cy;
  
  unsigned int scale_ratio;
  struct vcm_para vcm_cfg;

  unsigned int curr_exp_line;         /* Q4 */
  unsigned int curr_ang_gain;         /* Q4 */
  unsigned int curr_dig_gain;

  /* v4l2 drivers fill */
  struct isp_stat_buffer *ae_buf;
  struct isp_stat_buffer *af_buf;
  struct isp_stat_buffer *awb_buf;
  struct isp_stat_buffer *hist_buf;
  struct isp_stat_buffer *afs_buf;
  
  
  /* ISP drivers fill */
  struct isp_h3a_reg_win ae_reg_win_saved;
  struct isp_white_balance_gain wb_gain_saved;
  struct isp_wb_diff_threshold diff_th_saved;
  struct isp_awb_avp_stat awb_avp_saved;
  int min_rgb_saved;
};


/*
 *   
 *   struct exposure_settings - Stores the exposure related settings.
 * 
 */
struct exposure_settings
{
  int exp_compensation;
  int exp_priorty;

  unsigned int exp_absolute;
  int iso_sensitivity;
  
  enum flash_mode flash_mode; 
  enum exposure_mode exp_mode;
  enum exposure_win_mode exp_win_mode;
  unsigned int key_block_win[64];
  enum iso_mode iso_mode;
  enum power_line_frequency flicker_mode;
  isp_bool exposure_lock;
};

/*
 *   
 *   struct auto_focus_settings - Stores the auto focuse related settings.
 * 
 */
struct auto_focus_settings
{ 
  int focus_absolute;
  int focus_relative; 
  enum auto_focus_mode af_mode;
  enum auto_focus_win_mode af_win_mode;
  enum auto_focus_range af_range;   
  isp_bool focus_lock;
};

/*
 *   
 *   struct white_balance_settings - Stores the white balance related settings.
 * 
 */
struct white_balance_settings
{  
  enum white_balance_mode wb_mode;
  enum colorfx effect;
  int wb_temperature;
  isp_bool white_balance_lock;
};

struct drc_gen_ctrl
{
  unsigned int pic_size;
  unsigned int pic_avg;
  unsigned int hi_cnt;
};



/*
*
*struct isp_alg_para.
*
*/
struct isp_alg_para
{
  int defog_max_value;
  int defog_min_rgb;
  
  unsigned int filter_min_gain; /*2*/
  unsigned int filter_max_gain; /*12*/
  int filter_slope;  /*361*/
  int filter_min_th_def;  /*8*/
  int filter_max_th_def;  /*16*/
  
  //AE
  unsigned int ae_cal_scp;
  unsigned int ae_cal_ssp;
  unsigned int ae_max_exp_line;
  unsigned int ae_max_analog_gain;
  
  unsigned int awb_interval_frame;
  unsigned int af_interval_frame;

  //AFS
  unsigned int afs_def_min_exp;

  //AF
  int af_small_step;
  int af_mid_step;
  int af_min_focus_value;
  int af_monitor_start_frame;
  int af_monitor_th_dec_slop1;
  int af_monitor_th_dec_slop2;
  int af_monitor_th_inc_slop1;
  int af_monitor_th_inc_slop2;
  int af_monitor_num;
  int af_monitor_th_toss_range;
  int af_focus_value_rs;
  int af_vcm_def_pos;

  //AWB
  
};
struct isp_init_config 
{
  /*isp test param */
  int isp_test_mode;
  int isp_dbg_level;
  int isp_focus_len;
  int isp_gain;  
  int isp_exp_line;
  
  /*isp enable param */  
  int sprite_en;
  int lsc_en;
  int ae_en;
  int af_en;
  int awb_en;
  int drc_en;  
  int high_quality_mode_en;
  /*maybe change color matrix*/
  int defog_en;
  int satur_en;
  int pri_contrast_en;
  
  /*isp tune param */
  int denoise_level;
  int sharpness_level;
  int defog_value;
  int gain_delay_frame;
  int pri_contrast;  
  int lsc_center[2];
  int vcm_min_code;
  int vcm_max_code;  	
  int bayer_gain_offset[8];
  unsigned char  denoise_tbl[12];
  unsigned short lsc_tbl[7][768];
  unsigned short hdr_tbl[4][256];
  unsigned short gamma_tbl[256];  
  struct isp_rgb2rgb_gain_offset color_matrix_ini;  
};

/*
 *   
 *   struct isp_gen_settings - Stores the isp settings
 *   also stores the stat buffer for 3a algorithms.
 */
struct isp_gen_settings 
{
  /*  Exposure settings */
  struct exposure_settings exp_settings;

  struct drc_gen_ctrl  drc_gen;
  
  /* Auto focus settings */
  struct auto_focus_settings af_settings;
  
  /* White balance settings */
  struct white_balance_settings wb_settings;
  
  /* Others config */
  enum scene_mode scene_mode;
  unsigned int contrast;
  int sharpness;
  int brightness;
  int saturation;
  int hue;
  /* gsensor��ֵ */
  enum gsensor_direction gsensor_dir;
  
  struct sensor_band_step_config band_step_cfg;
  
  /* 3A windows HAL settings */
  struct h3a_win win; 
  unsigned int awb_inter_frame_cnt;
  unsigned int awb_frame_cnt;
  
  unsigned int ae_frame_cnt;
  unsigned int af_frame_cnt;
  
  /* 3A statistic buffers and other values*/
  struct isp_driver_to_3a_stat stat;
  
  /* ISP settings changed flags */
  unsigned int isp_3a_change_flags;
  
  /* ISP module config */
  struct isp_module_config module_cfg;
  //unsigned int isp_module_update_flags;
  
  struct isp_alg_para alg_para;
  struct isp_init_config isp_ini_cfg;

  unsigned alg_frame_cnt;
  unsigned take_pic_start_cnt;
  int take_picture_flag;
  int isp_nigth_mode_flag;
  int take_picture_done;
  
  enum isp_test_mode test_mode;
  int man_focus_len;
  int man_gain;

};

/*
 *   Flags raised when a setting is changed.
 */
enum e3a_settings_flags
{
  SET_SCENE_MODE             = 1 << 0,
  SET_WHITE_BALLANCE_MODE    = 1 << 1,
  SET_FLICKER                = 1 << 2,
  SET_SHARPNESS              = 1 << 3,
  SET_BRIGHTNESS_CONTRAST    = 1 << 4,
  SET_SATURATION             = 1 << 5,
  SET_EFFECT                 = 1 << 6,
  SET_FOCUS_WIN_MODE         = 1 << 7,
  SET_EXP_WIN_MODE           = 1 << 8,
  SET_LENS                   = 1 << 9,
  SET_HUE                    = 1 << 10,

  ISP_SETTING_MAX,
  
  /* all possible flags raised */
  ISP_SETTINGS_ALL = (((ISP_SETTING_MAX -1 ) << 1) -1 ),
};

/*
 *  ISP Module API
 */

void isp_param_init(struct isp_gen_settings *isp_gen);
int  isp_module_init(struct isp_gen_settings *isp_gen, struct isp_3a_result *isp_result);
void isp_module_cleanup(struct isp_gen_settings *isp_gen);
void isp_isr(struct isp_gen_settings *isp_gen, struct isp_3a_result *isp_result);
void isp_module_restore_context(struct isp_gen_settings *isp_gen);

void bsp_isp_s_brightness(struct isp_gen_settings *isp_gen, int value);
void bsp_isp_s_contrast(struct isp_gen_settings *isp_gen, int value);
void bsp_isp_s_saturation(struct isp_gen_settings *isp_gen, int value);
void bsp_isp_s_hue(struct isp_gen_settings *isp_gen, int value);
void bsp_isp_s_auto_white_balance(struct isp_gen_settings *isp_gen, int value);
void bsp_isp_s_exposure(struct isp_gen_settings *isp_gen, int value);
void bsp_isp_s_auto_gain(struct isp_gen_settings *isp_gen, int value);
void bsp_isp_s_gain(struct isp_gen_settings *isp_gen, int value);
void bsp_isp_s_hflip(struct isp_gen_settings *isp_gen, int value);
void bsp_isp_s_vflip(struct isp_gen_settings *isp_gen, int value);
void bsp_isp_s_power_line_frequency(struct isp_gen_settings *isp_gen, int value);
void bsp_isp_s_hue_auto(struct isp_gen_settings *isp_gen, int value);
void bsp_isp_s_white_balance_temperature(struct isp_gen_settings *isp_gen, int value);
void bsp_isp_s_sharpness(struct isp_gen_settings *isp_gen, int value);
void bsp_isp_s_chroma_agc(struct isp_gen_settings *isp_gen, int value);
void bsp_isp_s_colorfx(struct isp_gen_settings *isp_gen, int value);
void bsp_isp_s_auto_brightness(struct isp_gen_settings *isp_gen, int value);
void bsp_isp_s_band_stop_filter(struct isp_gen_settings *isp_gen, int value);
void bsp_isp_s_power_line_frequency_auto(struct isp_gen_settings *isp_gen, int value);
void bsp_isp_s_illuminators_1(struct isp_gen_settings *isp_gen, int value);
void bsp_isp_s_illuminators_2(struct isp_gen_settings *isp_gen, int value);
void bsp_isp_s_laststp1(struct isp_gen_settings *isp_gen, int value);
void bsp_isp_s_private_base(struct isp_gen_settings *isp_gen, int value);
void bsp_isp_s_hflip_thumb(struct isp_gen_settings *isp_gen, int value);
void bsp_isp_s_vflip_thumb(struct isp_gen_settings *isp_gen, int value);
void bsp_isp_s_auto_focus_win_num(struct isp_gen_settings *isp_gen, int value, struct isp_h3a_coor_win *coor);
void bsp_isp_s_auto_focus_ctrl(struct isp_gen_settings *isp_gen, int value);
void bsp_isp_s_flash_mode(struct isp_gen_settings *isp_gen, int value);
void bsp_isp_s_auto_exposure_win_num(struct isp_gen_settings *isp_gen, int value, struct isp_h3a_coor_win *coor);
void bsp_isp_s_gsensor_rotation(struct isp_gen_settings *isp_gen, int value);
void bsp_isp_s_exposure_auto(struct isp_gen_settings *isp_gen, int value);
void bsp_isp_s_exposure_absolute(struct isp_gen_settings *isp_gen, int value);
void bsp_isp_s_exposure_auto_priority(struct isp_gen_settings *isp_gen, int value);
void bsp_isp_s_focus_absolute(struct isp_gen_settings *isp_gen, int value);
void bsp_isp_s_focus_relative(struct isp_gen_settings *isp_gen, int value);
void bsp_isp_s_focus_auto(struct isp_gen_settings *isp_gen, int value);
void bsp_isp_s_auto_exposure_bias(struct isp_gen_settings *isp_gen, int value);
void bsp_isp_s_auto_n_preset_white_balance(struct isp_gen_settings *isp_gen, int value);
void bsp_isp_s_wide_dynamic_rage(struct isp_gen_settings *isp_gen, int value);
void bsp_isp_s_image_stabilization(struct isp_gen_settings *isp_gen, int value);
void bsp_isp_s_iso_sensitivity(struct isp_gen_settings *isp_gen, int value);
void bsp_isp_s_iso_sensitivity_auto(struct isp_gen_settings *isp_gen, int value);
void bsp_isp_s_scene_mode(struct isp_gen_settings *isp_gen, int value);
void bsp_isp_s_3a_lock(struct isp_gen_settings *isp_gen, int value);
void bsp_isp_s_auto_focus_start(struct isp_gen_settings *isp_gen, int value);
void bsp_isp_s_auto_focus_stop(struct isp_gen_settings *isp_gen, int value);
void bsp_isp_s_auto_focus_status(struct isp_gen_settings *isp_gen, int value);
void bsp_isp_s_auto_focus_range(struct isp_gen_settings *isp_gen, int value);

#endif //__BSP__ISP__ALGO__H


