#include <linux/delay.h>
#include "n28_hi846_front_truly_mipiraw_otp.h"

#if Hi846_OTP_FUNCTION

#define HI846_SLAVE_ID  0x40

#define PFX "hi846_camera_otp"
#define LOG_INF(format, args...)    \
	pr_info(PFX "[%s] " format, __func__, ##args)

#define LOG_ERR(format, args...)  \
	pr_err(PFX "[%s] " format, __func__, ##args)

unsigned char hi846_data_lsc[LSC_DATA_SIZE + 1] = {0};/*Add check sum*/
unsigned char hi846_data_awb[AWB_DATA_SIZE + 1] = {0};/*Add check sum*/
unsigned char hi846_data_info[MODULE_INFO_SIZE + 1] = {0};/*Add check sum*/
//static unsigned char hi846_data_sn[SN_INFO_SIZE + 1] = {0};/*Add check sum*/
unsigned char hi846_module_id = 0;
unsigned char hi846_lsc_valid = 0;
unsigned char hi846_awb_valid = 0;

kal_uint16 read_cmos_hi846_sensor(kal_uint32 addr)
{
	kal_uint16 get_byte = 0;
	char pu_send_cmd[2] = {(char)(addr >> 8), (char)(addr & 0xFF) };

	iReadRegI2C(pu_send_cmd, 2, (u8 *)&get_byte, 1, HI846_SLAVE_ID);

	return get_byte;
}

void write_cmos_hi846_sensor(kal_uint32 addr, kal_uint32 para)
{
	char pu_send_cmd[4] = {(char)(addr >> 8),
		(char)(addr & 0xFF), (char)(para >> 8), (char)(para & 0xFF)};

	iWriteRegI2C(pu_send_cmd, 4, HI846_SLAVE_ID);
}

void write_cmos_sensor_8(kal_uint32 addr, kal_uint32 para)
{
	char pu_send_cmd[4] = {(char)(addr >> 8),
		(char)(addr & 0xFF), (char)(para & 0xFF)};

	iWriteRegI2C(pu_send_cmd, 3, HI846_SLAVE_ID);
}

static void hi846_disable_otp_func(void)
{
	write_cmos_hi846_sensor(0x0a00, 0x00);
	mdelay(10);
	write_cmos_hi846_sensor(0x003e, 0x00);
	write_cmos_hi846_sensor(0x0a00, 0x01);
}

static int read_hi846_module_info(void)
{
	int otp_grp_flag = 0, minfo_start_addr = 0;
	int year = 0, month = 0, day = 0;
	int position = 0,lens_id = 0,vcm_id = 0;
	int check_sum = 0, check_sum_cal = 0;
	int i = 0;

	/* read flag */
	write_cmos_sensor_8(0x070a,((MODULE_GROUP_FLAG)>>8)&0xff);//start address H
	write_cmos_sensor_8(0x070b,(MODULE_GROUP_FLAG)&0xff);//start address L
	write_cmos_sensor_8(0x0702,0x01);//read enable
	otp_grp_flag = read_cmos_hi846_sensor(0x0708);//OTP data read

	if (otp_grp_flag == VALID_OTP_GROUP1_FLAG) {
		minfo_start_addr = MODULE_GROUP_FLAG + 1;
		//check_sum_cal += VALID_OTP_GROUP1_FLAG;
		LOG_INF("the group1 is valid,otp_grp_flag = 0x%x,info_start_addr:0x%x\n",otp_grp_flag,minfo_start_addr);
	} else if (otp_grp_flag == VALID_OTP_GROUP2_FLAG) {
		minfo_start_addr = MODULE_GROUP_FLAG + (MODULE_INFO_SIZE+ 1)+ 1;
		//check_sum_cal += VALID_OTP_GROUP2_FLAG;
		LOG_INF("the group2 is valid,otp_grp_flag = 0x%x,info_start_addr:0x%x\n",otp_grp_flag,minfo_start_addr);
	} else if (otp_grp_flag == VALID_OTP_GROUP3_FLAG) {
		minfo_start_addr = MODULE_GROUP_FLAG + (MODULE_INFO_SIZE+ 1)*2+ 1;
		//check_sum_cal += VALID_OTP_GROUP3_FLAG;
		LOG_INF("the group3 is valid,otp_grp_flag = 0x%x,info_start_addr:0x%x\n",otp_grp_flag,minfo_start_addr);
	} else {
		LOG_ERR("the group is invalid or empty,otp_grp_flag = 0x%x\n",otp_grp_flag);
		return 0;
	}

	if (minfo_start_addr != 0) {
		write_cmos_sensor_8(0x070a,((minfo_start_addr)>>8)&0xff);
		write_cmos_sensor_8(0x070b,(minfo_start_addr)&0xff);
		write_cmos_sensor_8(0x0702,0x01);
		for(i = 0; i < MODULE_INFO_SIZE + 1; i++) {
			hi846_data_info[i]=read_cmos_hi846_sensor(0x0708);
			LOG_INF("111 hi846_data_info[%d]:%d \n", i, hi846_data_info[i]);
		}
		for(i = 0; i < MODULE_INFO_SIZE; i++) {
			check_sum_cal += hi846_data_info[i];
			LOG_INF("222 hi846_data_info[%d]:%d \n", i, hi846_data_info[i]);
		}

		check_sum_cal = (check_sum_cal % 255) + 1;
		hi846_module_id = hi846_data_info[0];
		position = hi846_data_info[1];
		lens_id = hi846_data_info[2];
		vcm_id = hi846_data_info[3];
		year = hi846_data_info[4];
		month = hi846_data_info[5];
		day = hi846_data_info[6];
		check_sum = hi846_data_info[MODULE_INFO_SIZE];
	}

	//LOG_INF("module_id=0x%x position=0x%x\n", hi846_module_id, position);
	LOG_INF("=== HI846 INFO module_id=0x%x position=0x%x ===\n", hi846_module_id, position);
	LOG_INF("=== HI846 INFO lens_id=0x%x,vcm_id=0x%x ===\n",lens_id, vcm_id);
	LOG_INF("=== HI846 INFO date is %d-%d-%d ===\n",year,month,day);
	LOG_INF("=== HI846 INFO check_sum=0x%x,check_sum_cal=0x%x ===\n", check_sum, check_sum_cal);
	if (check_sum == check_sum_cal) {
		LOG_INF("check_sum_module_info success!\n");
		return 1;
	} else {
	         LOG_ERR("check_sum_module_info fail!\n");
		return 0;
	}
}

static int read_hi846_awb_info(void)
{
	int otp_grp_flag = 0, awb_start_addr=0;
	int check_sum_awb = 0, check_sum_awb_cal = 0;
	UINT32 r = 0,b = 0,gr = 0, gb = 0, golden_r = 0, golden_b = 0, golden_gr = 0, golden_gb = 0;
	int i = 0;
        //UINT32 r_gain = 0,b_gain = 0, g_gain = 0;

	/* awb group 1 */
	/* read flag */
	write_cmos_sensor_8(0x070a,((AWB_GROUP_FLAG)>>8)&0xff);
	write_cmos_sensor_8(0x070b,(AWB_GROUP_FLAG)&0xff);
	write_cmos_sensor_8(0x0702,0x01);
	otp_grp_flag = read_cmos_hi846_sensor(0x0708);

	if(otp_grp_flag == VALID_OTP_GROUP1_FLAG) {
		awb_start_addr = AWB_GROUP_FLAG + 1;
		//check_sum_awb_cal += VALID_OTP_GROUP1_FLAG;
		LOG_INF("the group1 is valid,otp_grp_flag = 0x%x,info_start_addr:0x%x\n",otp_grp_flag,awb_start_addr);
	} else if (otp_grp_flag == VALID_OTP_GROUP2_FLAG) {
                awb_start_addr = AWB_GROUP_FLAG + (AWB_DATA_SIZE+ 1)+ 1;
		//check_sum_awb_cal += VALID_OTP_GROUP2_FLAG;
		LOG_INF("the group2 is valid,otp_grp_flag = 0x%x,info_start_addr:0x%x\n",otp_grp_flag,awb_start_addr);
	} else if (otp_grp_flag == VALID_OTP_GROUP3_FLAG) {
		awb_start_addr = AWB_GROUP_FLAG + (AWB_DATA_SIZE+ 1)*2+ 1;
		//check_sum_awb_cal += VALID_OTP_GROUP3_FLAG;
		LOG_INF("the group3 is valid,otp_grp_flag = 0x%x,info_start_addr:0x%x\n",otp_grp_flag,awb_start_addr);
	} else {
		LOG_ERR("the group is invalid or empty,otp_grp_flag = 0x%x\n",otp_grp_flag);
		return 0;
	}

	if(awb_start_addr != 0)
	{
		write_cmos_sensor_8(0x070a,((awb_start_addr)>>8)&0xff);
		write_cmos_sensor_8(0x070b,(awb_start_addr)&0xff);
		write_cmos_sensor_8(0x0702,0x01);
		for(i = 0; i < AWB_DATA_SIZE + 1; i++) {
			hi846_data_awb[i]=read_cmos_hi846_sensor(0x0708);
		}
		for(i = 0; i < AWB_DATA_SIZE; i++) {
			check_sum_awb_cal += hi846_data_awb[i];
		}
		LOG_INF("check_sum_awb_cal =0x%x \n",check_sum_awb_cal);
		r = ((hi846_data_awb[1]<<8)&0xff00)|(hi846_data_awb[0]&0xff);
		b = ((hi846_data_awb[3]<<8)&0xff00)|(hi846_data_awb[2]&0xff);
		gr = ((hi846_data_awb[5]<<8)&0xff00)|(hi846_data_awb[4]&0xff);
		gb = ((hi846_data_awb[7]<<8)&0xff00)|(hi846_data_awb[6]&0xff);
		golden_r = ((hi846_data_awb[9]<<8)&0xff00)|(hi846_data_awb[8]&0xff);
		golden_b = ((hi846_data_awb[11]<<8)&0xff00)|(hi846_data_awb[10]&0xff);
		golden_gr = ((hi846_data_awb[13]<<8)&0xff00)|(hi846_data_awb[12]&0xff);
		golden_gb = ((hi846_data_awb[15]<<8)&0xff00)|(hi846_data_awb[14]&0xff);
		check_sum_awb = hi846_data_awb[AWB_DATA_SIZE];
		check_sum_awb_cal = (check_sum_awb_cal % 255) + 1;
	}

	LOG_INF("=== HI846 AWB r=0x%x, b=0x%x, gr=%x, gb=0x%x ===\n", r, b,gb, gr);
	LOG_INF("=== HI846 AWB gr=0x%x,gb=0x%x,gGr=%x, gGb=0x%x ===\n", golden_r, golden_b, golden_gr, golden_gb);
	LOG_INF("=== HI846 AWB check_sum_awb=0x%x,check_sum_awb_cal=0x%x ===\n",check_sum_awb,check_sum_awb_cal);
	if (check_sum_awb == check_sum_awb_cal) {
		LOG_INF("check_sum_awb success!\n");
		//HI846_Sensor_update_wb_gain(r,g,b);
		return 1;
	} else {
	         LOG_ERR("check_sum_awb fail!\n");
		return 0;
	}
}
static int read_hi846_lsc_info(void)
{
	int otp_grp_flag = 0, lsc_start_addr = 0;
	int check_sum_lsc = 0, check_sum_lsc_cal = 0;
         int i = 0;

	/* lsc group */
	/* read flag */
	write_cmos_sensor_8(0x070a,((LSC_GROUP_FLAG)>>8)&0xff);
	write_cmos_sensor_8(0x070b,(LSC_GROUP_FLAG)&0xff);
	write_cmos_sensor_8(0x0702,0x01);
	otp_grp_flag = read_cmos_hi846_sensor(0x0708);

	if(otp_grp_flag == VALID_OTP_GROUP1_FLAG) {
		lsc_start_addr = LSC_GROUP_FLAG + 1;//0x24E+1
		//check_sum_lsc_cal += VALID_OTP_GROUP1_FLAG;
		LOG_INF("the group1 is valid,otp_grp_flag = 0x%x,info_start_addr:0x%x\n",otp_grp_flag,lsc_start_addr);
	} else if (otp_grp_flag == VALID_OTP_GROUP2_FLAG) {
		lsc_start_addr = LSC_GROUP_FLAG + (LSC_DATA_SIZE+ 1)+ 1;//+S96818AA1-3562,wuwenhao2.wt,ADD,2023/06/05,hi846 lsc checksum error
		//check_sum_lsc_cal += VALID_OTP_GROUP2_FLAG;
		LOG_INF("the group2 is valid,otp_grp_flag = 0x%x,info_start_addr:0x%x\n",otp_grp_flag,lsc_start_addr);
	} else if (otp_grp_flag == VALID_OTP_GROUP3_FLAG) {
		lsc_start_addr = LSC_GROUP_FLAG + (LSC_DATA_SIZE+ 1)*2+ 1;//+S96818AA1-3562,wuwenhao2.wt,ADD,2023/06/05,hi846 lsc checksum error
		//check_sum_lsc_cal += VALID_OTP_GROUP3_FLAG;
		LOG_INF("the group3 is valid,otp_grp_flag = 0x%x,info_start_addr:0x%x\n",otp_grp_flag,lsc_start_addr);
	} else {
		LOG_ERR("the group is invalid or empty,otp_grp_flag = 0x%x\n",otp_grp_flag);
		return 0;
	}

	if (lsc_start_addr != 0) {
		write_cmos_sensor_8(0x070a,((lsc_start_addr)>>8)&0xff);
		write_cmos_sensor_8(0x070b,(lsc_start_addr)&0xff);
		write_cmos_sensor_8(0x0702,0x01);
		for(i = 0; i < LSC_DATA_SIZE + 1; i++)
		{
			hi846_data_lsc[i] = read_cmos_hi846_sensor(0x0708);
		}
		for(i = 0; i < LSC_DATA_SIZE; i++) {
			check_sum_lsc_cal += hi846_data_lsc[i];
		}
		LOG_INF("check_sum_lsc_cal =0x%x \n",check_sum_lsc_cal);
		check_sum_lsc = hi846_data_lsc[LSC_DATA_SIZE];
		check_sum_lsc_cal = (check_sum_lsc_cal % 255) + 1;
	}

	LOG_INF("=== HI846 LSC check_sum_lsc=0x%x, check_sum_lsc_cal=0x%x ===\n", check_sum_lsc, check_sum_lsc_cal);
	if (check_sum_lsc == check_sum_lsc_cal) {
		LOG_INF("check_sum_lsc success!\n");
		return 1;
	} else {
	         LOG_ERR("check_sum_lsc fail!\n");
		return 0;
	}
}

int hi846_sensor_otp_info(void)
{
	int ret = 0;

	LOG_INF("come to %s:%d E!\n", __func__, __LINE__);

	/* 1. sensor init */
    write_cmos_hi846_sensor(0x0A00, 0x0000);
    write_cmos_hi846_sensor(0x2000, 0x0000);
    write_cmos_hi846_sensor(0x2002, 0x00FF);
    write_cmos_hi846_sensor(0x2004, 0x0000);
    write_cmos_hi846_sensor(0x2008, 0x3FFF);
    write_cmos_hi846_sensor(0x23FE, 0xC056);
    write_cmos_hi846_sensor(0x0A00, 0x0000);
    write_cmos_hi846_sensor(0x0E04, 0x0012);
    write_cmos_hi846_sensor(0x0F08, 0x2F04);
    write_cmos_hi846_sensor(0x0F30, 0x001F);
    write_cmos_hi846_sensor(0x0F36, 0x001F);
    write_cmos_hi846_sensor(0x0F04, 0x3A00);
    write_cmos_hi846_sensor(0x0F32, 0x025A);
    write_cmos_hi846_sensor(0x0F38, 0x025A);
    write_cmos_hi846_sensor(0x0F2A, 0x4124);
    write_cmos_hi846_sensor(0x006A, 0x0100);
    write_cmos_hi846_sensor(0x004C, 0x0100);

	/* 2. init OTP setting*/
	write_cmos_sensor_8(0x0A02, 0x01); //Fast sleep on
	write_cmos_sensor_8(0x0A00, 0x00);//stand by on
    mdelay(10);
    write_cmos_sensor_8(0x0f02, 0x00); // pll disable
    write_cmos_sensor_8(0x071a, 0x01); // CP TRIM_H
    write_cmos_sensor_8(0x071b, 0x09); // IPGM TRIM_H
    write_cmos_sensor_8(0x0d04, 0x00); // Fsync(OTP busy) Output Enable
    write_cmos_sensor_8(0x0d00, 0x07); // Fsync(OTP busy) Output Drivability
    write_cmos_sensor_8(0x003e, 0x10); // OTP R/W mode
    write_cmos_sensor_8(0x0a00, 0x01); // stand by off
    mdelay(1);

	/* 3. read eeprom data */
	//minfo && awb &&lsc group
	ret = read_hi846_module_info();
	if (ret != 1) {
		hi846_module_id = 0;
		LOG_ERR("=== hi846_data_info invalid ===\n");
	}
	ret = read_hi846_awb_info();
	if (ret != 1) {
		hi846_awb_valid = 0;
		LOG_ERR("=== hi846_data_awb invalid ===\n");
	} else {
		hi846_awb_valid = 1;
	}
	ret = read_hi846_lsc_info();
	if (ret != 1) {
		hi846_lsc_valid = 0;
		LOG_ERR("=== hi846_data_lsc invalid ===\n");
	} else {
		hi846_lsc_valid = 1;
		//Hi846_Sensor_OTP_update_LSC();
	}

	/* 4. disable otp function */
	hi846_disable_otp_func();
	if (hi846_module_id == 0 || hi846_lsc_valid == 0 || hi846_awb_valid == 0) {
		LOG_INF("=== hi846_otp_data_invalid ===\n");
	} else {
		LOG_INF("=== hi846_otp_data_sucess ===\n");
	}

	if (!ret) {
		//*sensor_id = 0xFFFFFFFF;
		LOG_INF("n28hi846fronttruly read OTP failed ret:%d, \n",ret);
		//return ERROR_SENSOR_CONNECT_FAIL;
	} else {
		LOG_INF("n28hi846fronttruly read OTP successed: ret: %d \n",ret);
		n28hi846fronttruly_eeprom_data.dataBuffer = kmalloc(n28hi846fronttruly_eeprom_data.dataLength + OTP_MAP_VENDOR_HEAD_ID_BYTES, GFP_KERNEL);
		if (n28hi846fronttruly_eeprom_data.dataBuffer == NULL) {
			LOG_INF("n28hi846fronttruly_eeprom_data->dataBuffer is malloc fail\n");
			return -EFAULT;
		}
		memcpy(n28hi846fronttruly_eeprom_data.dataBuffer, (void *)&(n28hi846fronttruly_eeprom_data.sensorVendorid), OTP_MAP_VENDOR_HEAD_ID_BYTES);
		memcpy(n28hi846fronttruly_eeprom_data.dataBuffer + OTP_MAP_VENDOR_HEAD_ID_BYTES, (u8 *)&hi846_data_info, MODULE_INFO_SIZE);
		//memcpy(n28hi846fronttruly_eeprom_data.dataBuffer+MODULE_INFO_SIZE, (u8 *)&hi846_data_sn, SN_INFO_SIZE);
		memcpy(n28hi846fronttruly_eeprom_data.dataBuffer+MODULE_INFO_SIZE + OTP_MAP_VENDOR_HEAD_ID_BYTES, (u8 *)&hi846_data_awb, AWB_DATA_SIZE);
		memcpy(n28hi846fronttruly_eeprom_data.dataBuffer+MODULE_INFO_SIZE + AWB_DATA_SIZE + OTP_MAP_VENDOR_HEAD_ID_BYTES, (u8 *)&hi846_data_lsc, LSC_DATA_SIZE);
		imgsensor_set_eeprom_data_bywing(&n28hi846fronttruly_eeprom_data, NULL);
	}
	return ret;
}

#endif