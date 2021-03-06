#include "msm_sensor.h"
#include "msm_sd.h"
#include "camera.h"
#include "msm_cci.h"
#include "msm_camera_io_util.h"
#include "msm_camera_i2c_mux.h"

#undef CDBG
//#define CONFIG_MSMB_CAMERA_DEBUG
#ifdef CONFIG_MSMB_CAMERA_DEBUG
#define CDBG(fmt, args...) pr_err(fmt, ##args)
#else
#define CDBG(fmt, args...) do { } while (0)
#endif

uint16_t  t4k35_af_macro_value = 0; 
uint16_t  t4k35_af_inifity_value = 0;
uint16_t  t4k35_af_otp_status = 0;

//void t4k35_otp_init_setting(struct msm_sensor_ctrl_t *s_ctrl);

typedef struct t4k35_otp_struct 
{
  uint8_t LSC[53];              /* LSC */
  uint8_t AWB[8];               /* AWB */
  uint8_t Module_Info[9];
  uint16_t AF_Macro;
  uint16_t AF_Inifity;
} st_t4k35_otp;

#define T4K35_OTP_PSEL 0x3502
#define T4K35_OTP_CTRL 0x3500
#define T4K35_OTP_DATA_BEGIN_ADDR 0x3504
#define T4K35_OTP_DATA_END_ADDR 0x3543

static uint16_t t4k35_otp_data[T4K35_OTP_DATA_END_ADDR - T4K35_OTP_DATA_BEGIN_ADDR + 1] = {0x00};
static uint16_t t4k35_otp_data_backup[T4K35_OTP_DATA_END_ADDR - T4K35_OTP_DATA_BEGIN_ADDR + 1] = {0x00};

static uint16_t t4k35_r_golden_value=0x50; //0x91
static uint16_t t4k35_g_golden_value=0x90; //0xA6
static uint16_t t4k35_b_golden_value=0x5d; //0x81
static uint16_t t4k35_af_macro_pos = 500;
static uint16_t t4k35_af_inifity_pos = 100;

//#define SET_T4K35_REG(req, val)  msm_camera_i2c_write(s_ctrl->sensor_i2c_client,reg,val, MSM_CAMERA_I2C_BYTE_DATA)
//#define GET_T4K35_REG(reg, val)   msm_camera_i2c_read(s_ctrl->sensor_i2c_client, reg,&val, MSM_CAMERA_I2C_BYTE_DATA)
/*
static int32_t SET_T4K35_REG(uint32_t addr, uint16_t data, struct msm_sensor_ctrl_t *s_ctrl)
{
	int32_t rc = 0;
	rc = s_ctrl->sensor_i2c_client->i2c_func_tbl->i2c_write(
			s_ctrl->sensor_i2c_client,
			addr,
			data,
			MSM_CAMERA_I2C_BYTE_DATA);
	if (rc < 0) {
		msleep(10);
    	rc = s_ctrl->sensor_i2c_client->i2c_func_tbl->i2c_write(
    			s_ctrl->sensor_i2c_client,
    			addr,
    			data,
    			MSM_CAMERA_I2C_BYTE_DATA);		
	}
	CDBG("%s addr=%x, data=%x\n", __func__, addr, data);
	return rc;
}

static int32_t GET_T4K35_REG(uint32_t addr, uint16_t *data, struct msm_sensor_ctrl_t *s_ctrl)
{
	int32_t rc = 0;
	uint16_t chipid = 0;
	rc = s_ctrl->sensor_i2c_client->i2c_func_tbl->i2c_read(
			s_ctrl->sensor_i2c_client,
			addr,
			&chipid, MSM_CAMERA_I2C_WORD_DATA);
	if (rc < 0) {
		pr_err("%s: read imx214 otp failed\n", __func__);
		return rc;
	}
	chipid = chipid >> 8;
	*data = chipid;
	CDBG("%s: read imx214 otp addr: %x value %x:\n", __func__, addr,
		chipid);
	return chipid;
}
*/

static int32_t t4k35_sensor_i2c_read(struct msm_sensor_ctrl_t *s_ctrl, 
         uint16_t addr, uint16_t *data, enum msm_camera_i2c_data_type data_type)
{
	int32_t rc = 0;
	rc = s_ctrl->sensor_i2c_client->i2c_func_tbl->i2c_read(
			s_ctrl->sensor_i2c_client,
			addr,
			data,
			data_type);
	
	return rc;
}

static int32_t t4k35_sensor_i2c_write(struct msm_sensor_ctrl_t *s_ctrl, 
	  uint16_t addr, uint16_t data, enum msm_camera_i2c_data_type data_type)
{
	int32_t rc = 0;
	rc = s_ctrl->sensor_i2c_client->i2c_func_tbl->i2c_write(
			s_ctrl->sensor_i2c_client,
			addr,
			data, data_type);

	if (rc < 0) {
		msleep(10);
		rc = s_ctrl->sensor_i2c_client->i2c_func_tbl->i2c_write(
				s_ctrl->sensor_i2c_client,
				addr,
				data, data_type);
	}

	return rc;
}

#define SET_T4K35_REG(req, val) t4k35_sensor_i2c_write(s_ctrl, req, val, MSM_CAMERA_I2C_BYTE_DATA)
#define GET_T4K35_REG(req, val) t4k35_sensor_i2c_read(s_ctrl, req, &val, MSM_CAMERA_I2C_BYTE_DATA)

static void t4k35_otp_set_page(struct msm_sensor_ctrl_t *s_ctrl,uint16_t page)
{
    SET_T4K35_REG(T4K35_OTP_PSEL, page);
}

static void t4k35_otp_access(struct msm_sensor_ctrl_t *s_ctrl)
{
	uint16_t reg_val;
	GET_T4K35_REG(T4K35_OTP_CTRL, reg_val);
	SET_T4K35_REG(T4K35_OTP_CTRL, reg_val | 0x80);
	msleep(10);
}
static void t4k35_otp_read_enble(struct msm_sensor_ctrl_t *s_ctrl,uint8_t enble)
{
    if(enble)
        SET_T4K35_REG(T4K35_OTP_CTRL, 0x01);
    else
        SET_T4K35_REG(T4K35_OTP_CTRL, 0x00);
}

static int32_t t4k35_otp_read_data(struct msm_sensor_ctrl_t *s_ctrl, uint16_t* otp_data)
{
  uint16_t i = 0;
  int32_t rc = 0;
  uint8_t *buff = NULL;
  int num_byte = 0;

  /*
    for (i = 0; i <= (T4K35_OTP_DATA_END_ADDR - T4K35_OTP_DATA_BEGIN_ADDR); i++)
	{
        GET_T4K35_REG(T4K35_OTP_DATA_BEGIN_ADDR+i,otp_data[i]);
    }*/

  num_byte = T4K35_OTP_DATA_END_ADDR - T4K35_OTP_DATA_BEGIN_ADDR + 1;
  buff = kzalloc(num_byte, GFP_KERNEL);
  if (buff == NULL)
  {
      pr_err("t4k35_otp_read_data alloc no memory \n");
	  return -ENOMEM;
  }

  rc = s_ctrl->sensor_i2c_client->i2c_func_tbl->i2c_read_seq(
  	                   s_ctrl->sensor_i2c_client,
  	                   T4K35_OTP_DATA_BEGIN_ADDR,
  	                   buff, num_byte);

  for (i = 0; i < num_byte; i++)
  {
      otp_data[i] = buff[i];
  }

  kfree(buff);
  return 0;
}

static void t4k35_update_awb(struct msm_sensor_ctrl_t *s_ctrl,struct t4k35_otp_struct *p_otp)
{
  uint16_t rg,bg,r_otp,g_otp,b_otp;

  //printk("%s:E\n",__func__);
  r_otp=p_otp->AWB[1]+(p_otp->AWB[0]<<8);
  g_otp=(p_otp->AWB[3]+(p_otp->AWB[2]<<8)+p_otp->AWB[5]+(p_otp->AWB[4]<<8))/2;
  b_otp=p_otp->AWB[7]+(p_otp->AWB[6]<<8);
  
  rg = 256*(t4k35_r_golden_value *g_otp)/(r_otp*t4k35_g_golden_value);
  bg = 256*(t4k35_b_golden_value*g_otp)/(b_otp*t4k35_g_golden_value);

  printk("r_golden=0x%x,g_golden=0x%x, b_golden=0x%0x\n", t4k35_r_golden_value,t4k35_g_golden_value,t4k35_b_golden_value);
  printk("r_otp=0x%x,g_opt=0x%x, b_otp=0x%0x\n", r_otp,g_otp,b_otp);
  printk("rg=0x%x, bg=0x%0x\n", rg,bg);

  SET_T4K35_REG(0x0212, rg >> 8);
  SET_T4K35_REG(0x0213, rg & 0xff);

  SET_T4K35_REG(0x0214, bg >> 8);
  SET_T4K35_REG(0x0215, bg & 0xff);
  //printk("%s:X\n",__func__);
}


static void t4k35_update_lsc(struct msm_sensor_ctrl_t *s_ctrl,struct t4k35_otp_struct *p_otp)
{
  uint16_t addr;
  int i;

  printk("%s:E\n",__func__);
  //set lsc parameters
  addr = 0x323A;
  SET_T4K35_REG(addr, p_otp->LSC[0]);
  addr = 0x323E;
  for(i = 1; i < 53; i++) 
  {
    //printk(" SET LSC[%d], addr:0x%0x, val:0x%0x\n", i, addr, p_otp->LSC[i]);
    SET_T4K35_REG(addr++, p_otp->LSC[i]);
  }
  SET_T4K35_REG(0x3237,0x80);
  printk("%s:X\n",__func__);
}

static int32_t t4k35_otp_init_lsc_awb(struct msm_sensor_ctrl_t *s_ctrl,struct t4k35_otp_struct *p_otp)
{
  int i,j;
  uint16_t check_sum=0x00;
  printk("%s:E\n",__func__);
  for(i = 3; i >= 0; i--) 
  {
  	t4k35_otp_read_enble(s_ctrl,1);
  	//read data area

	t4k35_otp_set_page(s_ctrl,i);
    t4k35_otp_access(s_ctrl);
	//printk(" otp lsc data area data:%d\n",i);
    t4k35_otp_read_data(s_ctrl, t4k35_otp_data);
	//printk(" otp lsc backup area data:%d\n",i+6);
	t4k35_otp_set_page(s_ctrl,i+6);
	//OTP access
    t4k35_otp_access(s_ctrl);
	
    t4k35_otp_read_data(s_ctrl, t4k35_otp_data_backup);
  	t4k35_otp_read_enble(s_ctrl,0);

    for(j = 0; j < 64; j++) 
	{
        t4k35_otp_data[j]=t4k35_otp_data[j]|t4k35_otp_data_backup[j];
    }
    if (0 == t4k35_otp_data[0]) 
	{
      continue;
    }
	else 
	{
	  for(j = 2; j < 64; j++) 
	  {
        check_sum=check_sum+t4k35_otp_data[j];
      }

	  if((check_sum&0xFF)==t4k35_otp_data[1])
      {
	    printk(" otp lsc checksum ok!\n");
		for(j=3;j<=55;j++)
		{
			p_otp->LSC[j-3]=t4k35_otp_data[j];
		}
		for(j=56;j<=63;j++)
		{
			p_otp->AWB[j-56]=t4k35_otp_data[j];
        }
		return 0;
      }
	  else
      {
		printk(" otp lsc checksum error!\n");
		return -1;
      }
    }
  }

  if (i < 0) 
  {
    return -1;
  }
  else
  {
    return 0;
  }
}

static int32_t t4k35_otp_init_module_info(struct msm_sensor_ctrl_t *s_ctrl,struct t4k35_otp_struct *p_otp)
{
  int i,pos;
  uint16_t check_sum=0x00;
  printk("%s:E\n",__func__);
  //otp enable
  t4k35_otp_read_enble(s_ctrl,1);
  //set page
  t4k35_otp_set_page(s_ctrl,4);
  t4k35_otp_access(s_ctrl);
  //printk(" data area data:\n");
  t4k35_otp_read_data(s_ctrl,t4k35_otp_data);

  t4k35_otp_set_page(s_ctrl,10);
  t4k35_otp_access(s_ctrl);
  t4k35_otp_read_data(s_ctrl,t4k35_otp_data_backup);
  t4k35_otp_read_enble(s_ctrl,0);		
  for(i = 0; i < 64; i++) 
  {
	  t4k35_otp_data[i]=t4k35_otp_data[i]|t4k35_otp_data_backup[i];
	//  printk("t4k35_otp_data[%d] = %x ",i,t4k35_otp_data[i]);
  }

  if(t4k35_otp_data[32])
  {
	  pos=32;
  }
  else if(t4k35_otp_data[0])
  {
  	  pos=0;
  }
  else
  {
  	  printk(" otp no module information!\n");
  	  return -1;
  }

  //checking check sum
  for(i = pos+2; i <pos+32; i++) 
  {
     check_sum=check_sum+t4k35_otp_data[i];
  }

  if((check_sum&0xFF)==t4k35_otp_data[pos+1])
  {
        int moduleId = t4k35_otp_data[pos + 6];
		printk("fuyipeng --- t4k35 moduleId :%d \n", moduleId);
		
	  	//printk(" otp module info checksum ok!\n");
		if((t4k35_otp_data[pos+15]==0x00)&&(t4k35_otp_data[pos+16]==0x00)
			&&(t4k35_otp_data[pos+17]==0x00)&&(t4k35_otp_data[pos+18]==0x00)
			&&(t4k35_otp_data[pos+19]==0x00)&&(t4k35_otp_data[pos+20]==0x00)
			&&(t4k35_otp_data[pos+21]==0x00)&&(t4k35_otp_data[pos+22]==0x00))
			return 0;
			
		t4k35_r_golden_value=t4k35_otp_data[pos+16]+(t4k35_otp_data[pos+15]<<8);
		t4k35_g_golden_value=(t4k35_otp_data[pos+18]+(t4k35_otp_data[pos+17]<<8)+t4k35_otp_data[pos+20]+(t4k35_otp_data[pos+19]<<8))/2;
		t4k35_b_golden_value=t4k35_otp_data[pos+22]+(t4k35_otp_data[pos+21]<<8);

        printk(" otp module info checksum sucesse!\n");
		return 0;
  }
  else
  {
	printk(" otp module info checksum error!\n");
	return -1;
  }
}

static int32_t  t4k35_otp_init_af(struct msm_sensor_ctrl_t *s_ctrl)
{
    int i,pos;
    uint16_t  check_sum=0x00;
  
   	t4k35_otp_read_enble(s_ctrl,1);
  
 	t4k35_otp_set_page(s_ctrl,5);
    t4k35_otp_access(s_ctrl);
    t4k35_otp_read_data(s_ctrl, t4k35_otp_data);

    t4k35_otp_set_page(s_ctrl, 11);
	t4k35_otp_access(s_ctrl);
	t4k35_otp_read_data(s_ctrl, t4k35_otp_data_backup);
	t4k35_otp_read_enble(s_ctrl, 0);

	// get final OTP data
	for (i = 0; i < 64; i++)
	{
	    t4k35_otp_data[i] = t4k35_otp_data[i]|t4k35_otp_data_backup[i];
	}

	// macro AF
	// check flag
	if (t4k35_otp_data[24])
	{
	    pos = 24;
	}
	else if (t4k35_otp_data[16])
	{
	    pos = 16;
	}
	else if (t4k35_otp_data[8])
	{
	    pos = 8;
	}
	else if (t4k35_otp_data[0])
	{
	    pos = 0;
	}
	else
	{
	    printk("no otp macro AF information!\n");
		return -1;
	}

    check_sum = 0x00;
	for (i = pos + 2; i < pos + 8; i++)
	{
	    check_sum = check_sum + t4k35_otp_data[i];
	}

	if ((check_sum & 0xFF)==t4k35_otp_data[pos+1])
	{
	    printk("otp macro AF checksum ok!\n");
		t4k35_af_macro_value = (t4k35_otp_data[pos+3]<<8) + t4k35_otp_data[pos + 4];
		if (t4k35_af_macro_value == 0x00)
		{
		    t4k35_af_macro_value = t4k35_af_macro_pos;
		}
	}
	else
	{
	    printk("otp macro AF checksum error!\n");
		t4k35_af_macro_value = t4k35_af_macro_pos;
	}

	// inifity AF
	if (t4k35_otp_data[56])
	{
	    pos = 56;
	}
	else if (t4k35_otp_data[48])
	{
	    pos = 48;
	}
	else if (t4k35_otp_data[40])
	{
	    pos = 40;
	}
	else if (t4k35_otp_data[32])
	{
	    pos = 32;
	}
	else
	{
	    printk("no otp inifity AF information!\n");
		return -1;
	}

	// checking the sum
	check_sum = 0x00;
	for (i = pos + 2; i < pos + 8; i++)
	{
	    check_sum = check_sum + t4k35_otp_data[i];
	}

	if ((check_sum & 0xFF) == t4k35_otp_data[pos+1])
	{
	    printk("otp inifity AF checksum ok!\n");
		t4k35_af_inifity_value = (t4k35_otp_data[pos+4]<<8) + t4k35_otp_data[pos + 5];
		if (t4k35_af_inifity_value == 0x00)
		{
		    t4k35_af_inifity_value = t4k35_af_inifity_pos;
		}
	}
	else
	{
	    printk("otp inifity AF checksum error!\n");
		t4k35_af_inifity_value = t4k35_af_inifity_pos;	    
	}

	t4k35_af_otp_status = 1;
	return 0;

}

void t4k35_otp_init_setting(struct msm_sensor_ctrl_t *s_ctrl)
{
    int32_t rc = 0;
	st_t4k35_otp t4k35_data;

    t4k35_af_otp_status = 0;
    rc = t4k35_otp_init_module_info(s_ctrl, &t4k35_data);

    rc = t4k35_otp_init_lsc_awb(s_ctrl, &t4k35_data);
	if (rc == 0x00)
	{
        t4k35_update_lsc(s_ctrl,&t4k35_data);
        t4k35_update_awb(s_ctrl,&t4k35_data);
	}

    t4k35_otp_init_af(s_ctrl);

    return;
}

