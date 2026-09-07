#include <linux/delay.h>
#include "gc08a3mipi_otp.h"

#if GC08A3_OTP_FUNCTION

#define PFX "gc08a3_camera_otp"

#define GC08A3_DEBUG                0
#if GC08A3_DEBUG
#define LOG_INF(format, args...)    pr_debug(PFX "[%s] " format, __func__, ##args)
#else
#define LOG_INF(format, args...)
#endif

static unsigned char gc08a3_data_lsc[LSC_INFO_SIZE + 1] = {0};/*Add check sum*/
static unsigned char gc08a3_data_awb[AWB_INFO_SIZE + 1] = {0};/*Add check sum*/
static unsigned char gc08a3_data_info[MODULE_INFO_SIZE + 1] = {0};/*Add check sum*/
static unsigned char gc08a3_data_sn[SN_INFO_SIZE + 1] = {0};/*Add check sum*/

#define GC08A3_SLAVE_ID  0x62
kal_uint16 read_cmos_gc08a3_sensor(kal_uint32 addr)
{
	kal_uint16 get_byte = 0;
	char pu_send_cmd[2] = {
		(char)((addr >> 8) & 0xff),
		(char)(addr & 0xff)
	};

	iReadRegI2C(pu_send_cmd, 2, (u8 *)&get_byte, 1, GC08A3_SLAVE_ID);

	return get_byte;
}

void write_cmos_gc08a3_sensor(kal_uint32 addr, kal_uint32 para)
{
	char pu_send_cmd[4] = {
		(char)((addr >> 8) & 0xff),
		(char)(addr & 0xff),
		(char)((para >> 8) & 0xff),
		(char)(para & 0xff)
	};

	iWriteRegI2C(pu_send_cmd, 4, GC08A3_SLAVE_ID);
}

static void gc08a3_otp_init(void)
{
	write_cmos_sensor_8bit(0x0315, 0x80);
	write_cmos_sensor_8bit(0x031c, 0x60);

	write_cmos_sensor_8bit(0x0324, 0x42);
	write_cmos_sensor_8bit(0x0316, 0x09);
	write_cmos_sensor_8bit(0x0a67, 0x80);
	write_cmos_sensor_8bit(0x0313, 0x00);
	write_cmos_sensor_8bit(0x0a53, 0x0e);
	write_cmos_sensor_8bit(0x0a65, 0x17);
	write_cmos_sensor_8bit(0x0a68, 0xa1);
	write_cmos_sensor_8bit(0x0a47, 0x00);
	write_cmos_sensor_8bit(0x0a58, 0x00);
	write_cmos_sensor_8bit(0x0ace, 0x0c);
}
static void gc08a3_otp_close(void)
{
	write_cmos_sensor_8bit(0x0316, 0x01);
	write_cmos_sensor_8bit(0x0a67, 0x00);
}
static u16 gc08a3_otp_read_group(u16 addr, u8 *data, u16 length)
{
	u16 i = 0;

	write_cmos_sensor_8bit(0x0313, 0x00);
	write_cmos_sensor_8bit(0x0a69, (addr >> 8) & 0xff);
	write_cmos_sensor_8bit(0x0a6a, addr & 0xff);
	write_cmos_sensor_8bit(0x0313, 0x20);
	write_cmos_sensor_8bit(0x0313, 0x12);

	for (i = 0; i < length; i++) {
		data[i] = read_cmos_gc08a3_sensor(0x0a6c);
	    //LOG_INF("addr = 0x%x, data = 0x%x\n", addr + i * 8, data[i]);
	}
	return 0;
}
static kal_uint16 read_cmos_sensor_otp(u16 addr)
{
	write_cmos_sensor_8bit(0x0313, 0x00);
	write_cmos_sensor_8bit(0x0a69, (addr >> 8) & 0xff);
	write_cmos_sensor_8bit(0x0a6a, addr & 0xff);
	write_cmos_sensor_8bit(0x0313, 0x20);
	write_cmos_sensor_8bit(0x0313, 0x12);
	return read_cmos_gc08a3_sensor(0x0a6c);
}

static int read_gc08a3_module_info(void)
{
	int otp_grp_flag = 0, minfo_start_addr = 0;
	//int year = 0, month = 0, day = 0;
	//int position = 0,lens_id = 0,vcm_id = 0,gc08a3_module_id = 0;
	int check_sum = 0, check_sum_cal = 0;
	int i;

	otp_grp_flag = read_cmos_sensor_otp(MODULE_INFO_FLAG);
	LOG_INF("module info otp_grp_flag = 0x%x",otp_grp_flag);

	if (otp_grp_flag == 0x01)
		minfo_start_addr = MODULE_GROUP1_ADDR;
	else if (otp_grp_flag == 0x07)
		minfo_start_addr = MODULE_GROUP2_ADDR;
	else if (otp_grp_flag == 0x1f)
		minfo_start_addr = MODULE_GROUP3_ADDR;
	else {
		LOG_INF("no module info OTP gc08a3_data_info\n");
		return 0;
	}

	gc08a3_otp_read_group(minfo_start_addr,gc08a3_data_info,MODULE_INFO_SIZE + 1);

	for(i = 0; i <  MODULE_INFO_SIZE ; i++) {
		check_sum_cal += gc08a3_data_info[i];
	}

	check_sum_cal = (check_sum_cal % 255) + 1;
/*	gc08a3_module_id = gc08a3_data_info[0];
	position = gc08a3_data_info[1];
	lens_id = gc08a3_data_info[2];
	vcm_id = gc08a3_data_info[3];
	year = gc08a3_data_info[4];
	month = gc08a3_data_info[5];
	day = gc08a3_data_info[6];
*/
	check_sum = gc08a3_data_info[MODULE_INFO_SIZE];

	//LOG_INF("module_id=0x%x position=0x%x\n", gc08a3_module_id, position);
	LOG_INF("=== GC08A3 INFO module_id=0x%x position=0x%x ===\n", gc08a3_data_info[0], gc08a3_data_info[1]);
	LOG_INF("=== GC08A3 INFO lens_id=0x%x,vcm_id=0x%x ===\n",gc08a3_data_info[2], gc08a3_data_info[3]);
	LOG_INF("=== GC08A3 INFO date is %d-%d-%d ===\n",gc08a3_data_info[4],gc08a3_data_info[5],gc08a3_data_info[6]);
	LOG_INF("=== GC08A3 INFO check_sum=0x%x,check_sum_cal=0x%x ===\n", check_sum, check_sum_cal);
	if (check_sum == check_sum_cal) {
		LOG_INF("=== GC08A3 INFO date sucess!\n");
		return 1;
	} else {
		return 0;
	}
}

static int read_gc08a3_sn_info(void)
{
	int otp_grp_flag = 0, minfo_start_addr = 0;
	int check_sum = 0, check_sum_cal = 0;
	int i;


	otp_grp_flag = read_cmos_sensor_otp(SN_INFO_FLAG);
	LOG_INF("sn info otp_grp_flag = 0x%x",otp_grp_flag);

	/* select group */
	if (otp_grp_flag == 0x01)
		minfo_start_addr = SN_GROUP1_ADDR;
	else if (otp_grp_flag == 0x07)
		minfo_start_addr = SN_GROUP2_ADDR;
	else if (otp_grp_flag == 0x1f)
		minfo_start_addr = SN_GROUP3_ADDR;
	else {
		LOG_INF("no sn info OTP gc08a3_data_info\n");
		return 0;
	}

	gc08a3_otp_read_group(minfo_start_addr,gc08a3_data_sn,SN_INFO_SIZE + 1);

	for(i = 0; i <  SN_INFO_SIZE ; i++) {
		check_sum_cal += gc08a3_data_sn[i];
		LOG_INF("sn info gc08a3_data_sn[%d] = 0x%x",i,gc08a3_data_sn[i]);
	}
	check_sum_cal = (check_sum_cal % 255) + 1;
	check_sum=gc08a3_data_sn[SN_INFO_SIZE];
	LOG_INF("=== GC08A3 SN check_sum=0x%x,check_sum_cal=0x%x ===\n", check_sum, check_sum_cal);
	if (check_sum == check_sum_cal) {
		LOG_INF("=== GC08A3 SN date sucess!\n");
		return 1;
	} else {
		return 0;
	}
}
static int read_gc08a3_awb_info(void)
{
	int awb_grp_flag = 0, awb_start_addr=0;
	//int check_sum_awb_cal = 0;
	//int check_sum_awb = 0, r = 0,b = 0,gr = 0, gb = 0, golden_r = 0, golden_b = 0, golden_gr = 0, golden_gb = 0;
	//int i;

	awb_grp_flag = read_cmos_sensor_otp(AWB_INFO_FLAG);
	LOG_INF("awb awb_grp_flag = 0x%x",awb_grp_flag);

	if (awb_grp_flag == 0x01)
		awb_start_addr = AWB_GROUP1_FLAG;
	else if (awb_grp_flag == 0x07)
		awb_start_addr = AWB_GROUP2_FLAG;
	else if (awb_grp_flag == 0x1f)
		awb_start_addr = AWB_GROUP3_FLAG;
	else {
		LOG_INF("no awb OTP gc08a3_data_info\n");
		return 0;
	}

	//check_sum_awb_cal += awb_grp_flag;

	gc08a3_otp_read_group(awb_start_addr,gc08a3_data_awb,AWB_INFO_SIZE + 1);
	//for(i = 0; i < AWB_INFO_SIZE; i++){
		//check_sum_awb_cal += gc08a3_data_awb[i];
	//}
	//LOG_INF("check_sum_awb_cal =0x%x \n",check_sum_awb_cal);
/*
#if 0
    //old module r gr gb b
	r = ((gc08a3_data_awb[1]<<8)&0xff00)|(gc08a3_data_awb[0]&0xff);
	gr = ((gc08a3_data_awb[3]<<8)&0xff00)|(gc08a3_data_awb[2]&0xff);
	gb = ((gc08a3_data_awb[5]<<8)&0xff00)|(gc08a3_data_awb[4]&0xff);
	b = ((gc08a3_data_awb[7]<<8)&0xff00)|(gc08a3_data_awb[6]&0xff);
	golden_r = ((gc08a3_data_awb[9]<<8)&0xff00)|(gc08a3_data_awb[8]&0xff);
	golden_gr = ((gc08a3_data_awb[11]<<8)&0xff00)|(gc08a3_data_awb[10]&0xff);
	golden_gb = ((gc08a3_data_awb[13]<<8)&0xff00)|(gc08a3_data_awb[12]&0xff);
	golden_b = ((gc08a3_data_awb[15]<<8)&0xff00)|(gc08a3_data_awb[14]&0xff);
	check_sum_awb = gc08a3_data_awb[AWB_INFO_SIZE];
	check_sum_awb_cal = (check_sum_awb_cal % 255) + 1;
#else
   //new module r b gr gb
	r = ((gc08a3_data_awb[1]<<8)&0xff00)|(gc08a3_data_awb[0]&0xff);
	b = ((gc08a3_data_awb[3]<<8)&0xff00)|(gc08a3_data_awb[2]&0xff);
	gr = ((gc08a3_data_awb[5]<<8)&0xff00)|(gc08a3_data_awb[4]&0xff);
	gb = ((gc08a3_data_awb[7]<<8)&0xff00)|(gc08a3_data_awb[6]&0xff);
	golden_r = ((gc08a3_data_awb[9]<<8)&0xff00)|(gc08a3_data_awb[8]&0xff);
	golden_b = ((gc08a3_data_awb[11]<<8)&0xff00)|(gc08a3_data_awb[10]&0xff);
	golden_gr = ((gc08a3_data_awb[13]<<8)&0xff00)|(gc08a3_data_awb[12]&0xff);
	golden_gb = ((gc08a3_data_awb[15]<<8)&0xff00)|(gc08a3_data_awb[14]&0xff);
	check_sum_awb = gc08a3_data_awb[AWB_INFO_SIZE];
	check_sum_awb_cal = (check_sum_awb_cal % 255) + 1;

#endif

	LOG_INF("=== GC08A3 AWB r=0x%x, b=0x%x, gr=%x, gb=0x%x ===\n", r, b,gb, gr);
	LOG_INF("=== GC08A3 AWB gr=0x%x,gb=0x%x,gGr=%x, gGb=0x%x ===\n", golden_r, golden_b, golden_gr, golden_gb);
	LOG_INF("=== GC08A3 AWB check_sum_awb=0x%x,check_sum_awb_cal=0x%x ===\n",check_sum_awb,check_sum_awb_cal);
	if(check_sum_awb == check_sum_awb_cal){
		LOG_INF("=== GC08A3 AWB date sucess!\n");
		return 1;
	}else{
		return 0;
	}
*/
		return 1;
}

static int read_gc08a3_lsc_info(void)
{
	int lsc_grp_flag = 0, lsc_start_addr = 0;
	int check_sum_lsc = 0, check_sum_lsc_cal = 0;
	int i;

	lsc_grp_flag = read_cmos_sensor_otp(LSC_INFO_FLAG);//1948
	LOG_INF("lsc lsc_grp_flag = 0x%x",lsc_grp_flag);

	/* select group */
	if (lsc_grp_flag == 0x01)
		lsc_start_addr = LSC_GROUP1_FLAG;//1950
	else if (lsc_grp_flag == 0x07)
		lsc_start_addr = LSC_GROUP2_FLAG;//53b8
	else if (lsc_grp_flag == 0x1f)
		lsc_start_addr = LSC_GROUP3_FLAG;//8E20
	else {
		LOG_INF("no lsc OTP gc08a3_data_info\n");
		return 0;
	}

	/* read & checksum */
	//check_sum_lsc_cal += lsc_grp_flag;

	gc08a3_otp_read_group(lsc_start_addr,gc08a3_data_lsc,LSC_INFO_SIZE + 1);

	for(i = 0; i < LSC_INFO_SIZE; i++) {
		check_sum_lsc_cal += gc08a3_data_lsc[i];
	}
	LOG_INF("check_sum_lsc_cal =0x%x \n",check_sum_lsc_cal);
	check_sum_lsc = gc08a3_data_lsc[LSC_INFO_SIZE];
	check_sum_lsc_cal = (check_sum_lsc_cal % 255) + 1;

	LOG_INF("=== GC08A3 LSC check_sum_lsc=0x%x, check_sum_lsc_cal=0x%x ===\n", check_sum_lsc, check_sum_lsc_cal);
	if (check_sum_lsc == check_sum_lsc_cal) {
		LOG_INF("=== GC08A3 LSC date sucess!\n");
		return 1;
	} else {
		return 0;
	}
}

int gc0a83_sensor_otp_info(void)
{
	int ret = 0, gc08a3_module_valid = 0, gc08a3_awb_valid = 0, gc08a3_lsc_valid = 0;
	//int  gc08a3_sn_valid = 0;
	LOG_INF("come to %s:%d E!\n", __func__, __LINE__);

	gc08a3_otp_init();
	mdelay(10);

	ret = read_gc08a3_module_info();
	if (ret != 1) {
		gc08a3_module_valid = 0;
		LOG_INF("=== gc08a3_data_info invalid ===\n");
	} else {
		gc08a3_module_valid = 1;
	}
	ret = read_gc08a3_sn_info();
	/*
	if(ret != 1){
		gc08a3_sn_valid = 0;
		LOG_INF("=== gc08a3_data_sn invalid ===\n");
	}else{
		gc08a3_sn_valid = 1;
	}
	*/
	ret = read_gc08a3_awb_info();
	if (ret != 1) {
		gc08a3_awb_valid = 0;
		LOG_INF("=== gc08a3_data_awb invalid ===\n");
	} else {
		gc08a3_awb_valid = 1;
	}
	ret = read_gc08a3_lsc_info();
	if (ret != 1) {
		gc08a3_lsc_valid = 0;
		LOG_INF("=== gc08a3_data_lsc invalid ===\n");
	} else {
		gc08a3_lsc_valid = 1;
	}

	gc08a3_otp_close();
	if (gc08a3_module_valid == 0 || gc08a3_awb_valid == 0 || gc08a3_lsc_valid == 0) {
		LOG_INF("=== gc08a3_otp_data_invalid ===\n");
	} else {
		LOG_INF("=== gc08a3_otp_data_sucess ===\n");
	}

	if (!ret) {
		//*sensor_id = 0xFFFFFFFF;
		LOG_INF("gc0a83 read OTP failed ret:%d, \n",ret);
		//return ERROR_SENSOR_CONNECT_FAIL;
	} else {
		LOG_INF("gc0a83 read OTP successed: ret: %d \n",ret);
		n28gc0a83frontcxt_eeprom_data.dataBuffer = kmalloc(n28gc0a83frontcxt_eeprom_data.dataLength  + OTP_MAP_VENDOR_HEAD_ID_BYTES, GFP_KERNEL);
		if (n28gc0a83frontcxt_eeprom_data.dataBuffer == NULL) {
			LOG_INF("n28gc0a83frontcxt_eeprom_data->dataBuffer is malloc fail\n");
			return -EFAULT;
		}
		memcpy(n28gc0a83frontcxt_eeprom_data.dataBuffer, (void *)&(n28gc0a83frontcxt_eeprom_data.sensorVendorid), OTP_MAP_VENDOR_HEAD_ID_BYTES);
		memcpy(n28gc0a83frontcxt_eeprom_data.dataBuffer + OTP_MAP_VENDOR_HEAD_ID_BYTES, (u8 *)&gc08a3_data_info, MODULE_INFO_SIZE);
		memcpy(n28gc0a83frontcxt_eeprom_data.dataBuffer + MODULE_INFO_SIZE + OTP_MAP_VENDOR_HEAD_ID_BYTES, (u8 *)&gc08a3_data_sn, SN_INFO_SIZE);
		memcpy(n28gc0a83frontcxt_eeprom_data.dataBuffer + SN_INFO_SIZE + MODULE_INFO_SIZE + OTP_MAP_VENDOR_HEAD_ID_BYTES, (u8 *)&gc08a3_data_awb, AWB_INFO_SIZE);
		memcpy(n28gc0a83frontcxt_eeprom_data.dataBuffer + SN_INFO_SIZE + MODULE_INFO_SIZE+AWB_INFO_SIZE + OTP_MAP_VENDOR_HEAD_ID_BYTES, (u8 *)&gc08a3_data_lsc, LSC_INFO_SIZE);
		imgsensor_set_eeprom_data_bywing(&n28gc0a83frontcxt_eeprom_data, NULL);
	}
	return ret;
}
#endif