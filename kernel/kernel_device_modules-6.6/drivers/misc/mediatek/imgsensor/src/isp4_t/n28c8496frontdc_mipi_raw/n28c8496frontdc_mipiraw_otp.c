#include <linux/delay.h>
#include "n28c8496frontdc_mipiraw_otp.h"

#if C8496_OTP_FUNCTION

#define PFX "c8496mipiraw_otp"
#define LOG_INF(format, args...)    pr_info(PFX "[%s] " format, __func__, ##args)

static unsigned char c8496_data_lsc[LSC_INFO_SIZE + 1] = {0};/*Add check sum*/
static unsigned char c8496_data_awb[AWB_INFO_SIZE + 1] = {0};/*Add check sum*/
static unsigned char c8496_data_info[MODULE_INFO_SIZE + 1] = {0};/*Add check sum*/
static unsigned char c8496_data_sn[SN_INFO_SIZE + 1] = {0};/*Add check sum*/

extern kal_uint16 read_cmos_sensor(kal_uint32 addr);
extern void write_cmos_sensor(kal_uint32 addr, kal_uint32 para);

static void c8496_otp_init(void)
{
	write_cmos_sensor(0x0100, 0x00);
	write_cmos_sensor(0x318F, 0x04);
	write_cmos_sensor(0x3584, 0x02);
	write_cmos_sensor(0x3A81, 0x00);
	write_cmos_sensor(0x3A80, 0x00);
	write_cmos_sensor(0x3A8C, 0x04);
	write_cmos_sensor(0x3A8D, 0x02);
	write_cmos_sensor(0x3AA0, 0x03);
	write_cmos_sensor(0x3A80, 0x08);
	write_cmos_sensor(0x3A90, 0x01);
	write_cmos_sensor(0x3A93, 0x01);
	write_cmos_sensor(0x3A94, 0x60);
}
static void c8496_otp_close(void)
{
	write_cmos_sensor(0x318f, 0x00);//Exit read mode
	write_cmos_sensor(0x3584, 0x22);
	write_cmos_sensor(0x3A81, 0x00);
	write_cmos_sensor(0x3A80, 0x00);
	write_cmos_sensor(0x3A93, 0x00);
	write_cmos_sensor(0x3A94, 0x00);
	write_cmos_sensor(0x3A90, 0x00);
}

static u16 c8496_otp_read_group(u16 addr, u8 *data, u16 length)
{
    u16 i = 0;
	write_cmos_sensor(0x3A80, 0x88);//enter read mode
	write_cmos_sensor(0x3A81, 0x02);
	mdelay(10);
	for (i = 0; i < length; i++) {
		data[i] = read_cmos_sensor(addr+i);
		udelay(500);
	    //LOG_INF("addr = 0x%x, data = 0x%x\n", addr + i * 8, data[i]);
	}
	return 0;
}
static kal_uint16 read_cmos_sensor_otp(u16 addr)
{

	write_cmos_sensor(0x3A80, 0x88);//enter read mode
	write_cmos_sensor(0x3A81, 0x02);
	mdelay(10);
	return read_cmos_sensor(addr);
}

static int read_c8496_module_info(void)
{
	int otp_grp_flag = 0, minfo_start_addr = 0;
	int year = 0, month = 0, day = 0;
	int position = 0,lens_id = 0,vcm_id = 0,c8496_module_id = 0;
	int check_sum = 0, check_sum_cal = 0;
	int i;

	otp_grp_flag = read_cmos_sensor_otp(MODULE_INFO_FLAG);
	mdelay(1);
	LOG_INF("module info otp_grp_flag = 0x%x",otp_grp_flag);

	if (otp_grp_flag == 0x1f)
		minfo_start_addr = MODULE_GROUP1_ADDR;
	else if (otp_grp_flag == 0x07)
		minfo_start_addr = MODULE_GROUP2_ADDR;
	else if (otp_grp_flag == 0x01)
		minfo_start_addr = MODULE_GROUP3_ADDR;
	else {
		LOG_INF("no module info OTP c8496_data_info\n");
		return 0;
	}

	c8496_otp_read_group(minfo_start_addr,c8496_data_info,MODULE_INFO_SIZE + 1);

	for (i = 0; i <  MODULE_INFO_SIZE ; i++) {
		check_sum_cal += c8496_data_info[i];
	}

	check_sum_cal = (check_sum_cal % 255) + 1;
	c8496_module_id = c8496_data_info[0];
	position = c8496_data_info[1];
	lens_id = c8496_data_info[2];
	vcm_id = c8496_data_info[3];
	year = c8496_data_info[4];
	month = c8496_data_info[5];
	day = c8496_data_info[6];
	check_sum = c8496_data_info[MODULE_INFO_SIZE];

	//LOG_INF("module_id=0x%x position=0x%x\n", c8496_module_id, position);
	LOG_INF("=== C8496 INFO module_id=0x%x position=0x%x ===\n", c8496_module_id, position);
	LOG_INF("=== C8496 INFO lens_id=0x%x,vcm_id=0x%x ===\n",lens_id, vcm_id);
	LOG_INF("=== C8496 INFO date is %d-%d-%d ===\n",year,month,day);
	LOG_INF("=== C8496 INFO check_sum=0x%x,check_sum_cal=0x%x ===\n", check_sum, check_sum_cal);
	if (check_sum == check_sum_cal) {
		LOG_INF("=== C8496 INFO date sucess!\n");
		return 1;
	} else {
		return 0;
	}
}

static int read_c8496_sn_info(void)
{
	int otp_grp_flag = 0, minfo_start_addr = 0;
	int check_sum = 0, check_sum_cal = 0;
	int i;


	otp_grp_flag = read_cmos_sensor_otp(SN_INFO_FLAG);
	LOG_INF("sn info otp_grp_flag = 0x%x",otp_grp_flag);

	if (otp_grp_flag == 0x1f)
		minfo_start_addr = SN_GROUP1_ADDR;
	else if (otp_grp_flag == 0x07)
		minfo_start_addr = SN_GROUP2_ADDR;
	else if (otp_grp_flag == 0x01)
		minfo_start_addr = SN_GROUP3_ADDR;
	else {
		LOG_INF("no sn info OTP gc08a3_data_info\n");
		return 0;
	}

	c8496_otp_read_group(minfo_start_addr,c8496_data_sn,SN_INFO_SIZE + 1);

	for (i = 0; i <  SN_INFO_SIZE ; i++) {
		check_sum_cal += c8496_data_sn[i];
		LOG_INF("sn info c8496_data_sn[%d] = 0x%x",i,c8496_data_sn[i]);
	}
	check_sum_cal = (check_sum_cal % 255) + 1;
	check_sum=c8496_data_sn[SN_INFO_SIZE];
	LOG_INF("=== C8496 SN check_sum=0x%x,check_sum_cal=0x%x ===\n", check_sum, check_sum_cal);
	if (check_sum == check_sum_cal) {
		LOG_INF("=== C8496 SN date sucess!\n");
		return 1;
	} else {
		return 0;
	}
}
static int read_c8496_awb_info(void)
{
	int awb_grp_flag = 0, awb_start_addr=0;
	int check_sum_awb = 0, check_sum_awb_cal = 0;
	int r = 0,b = 0,gr = 0, gb = 0, golden_r = 0, golden_b = 0, golden_gr = 0, golden_gb = 0;
	int i;

	awb_grp_flag = read_cmos_sensor_otp(AWB_INFO_FLAG);
	LOG_INF("awb awb_grp_flag = 0x%x",awb_grp_flag);

	if (awb_grp_flag == 0x1f)
		awb_start_addr = AWB_GROUP1_FLAG;
	else if (awb_grp_flag == 0x07)
		awb_start_addr = AWB_GROUP2_FLAG;
	else if (awb_grp_flag == 0x01)
		awb_start_addr = AWB_GROUP3_FLAG;
	else {
		LOG_INF("no awb OTP c8496_data_info\n");
		return 0;
	}

	//check_sum_awb_cal += awb_grp_flag;

	c8496_otp_read_group(awb_start_addr,c8496_data_awb,AWB_INFO_SIZE + 1);
	for (i = 0; i < AWB_INFO_SIZE; i++) {
		check_sum_awb_cal += c8496_data_awb[i];
	}
	LOG_INF("check_sum_awb_cal =0x%x \n",check_sum_awb_cal);
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
	r = ((c8496_data_awb[1]<<8)&0xff00)|(c8496_data_awb[0]&0xff);
	b = ((c8496_data_awb[3]<<8)&0xff00)|(c8496_data_awb[2]&0xff);
	gr = ((c8496_data_awb[5]<<8)&0xff00)|(c8496_data_awb[4]&0xff);
	gb = ((c8496_data_awb[7]<<8)&0xff00)|(c8496_data_awb[6]&0xff);
	golden_r = ((c8496_data_awb[9]<<8)&0xff00)|(c8496_data_awb[8]&0xff);
	golden_b = ((c8496_data_awb[11]<<8)&0xff00)|(c8496_data_awb[10]&0xff);
	golden_gr = ((c8496_data_awb[13]<<8)&0xff00)|(c8496_data_awb[12]&0xff);
	golden_gb = ((c8496_data_awb[15]<<8)&0xff00)|(c8496_data_awb[14]&0xff);
	check_sum_awb = c8496_data_awb[AWB_INFO_SIZE];
	check_sum_awb_cal = (check_sum_awb_cal % 255) + 1;

#endif

	LOG_INF("=== c8496 AWB r=0x%x, b=0x%x, gr=%x, gb=0x%x ===\n", r, b,gb, gr);
	LOG_INF("=== c8496 AWB gr=0x%x,gb=0x%x,gGr=%x, gGb=0x%x ===\n", golden_r, golden_b, golden_gr, golden_gb);
	LOG_INF("=== c8496 AWB check_sum_awb=0x%x,check_sum_awb_cal=0x%x ===\n",check_sum_awb,check_sum_awb_cal);
	if (check_sum_awb == check_sum_awb_cal) {
		LOG_INF("=== c8496 AWB date sucess!\n");
		return 1;
	} else {
		return 0;
	}
}

static int read_c8496_lsc_info(void)
{
	int lsc_grp_flag = 0, lsc_start_addr = 0;
	int check_sum_lsc = 0, check_sum_lsc_cal = 0;
	int i;

	lsc_grp_flag = read_cmos_sensor_otp(LSC_INFO_FLAG);//1948
	LOG_INF("lsc lsc_grp_flag = 0x%x",lsc_grp_flag);

	if (lsc_grp_flag == 0x1f)
		lsc_start_addr = LSC_GROUP1_FLAG;//1950
	else if (lsc_grp_flag == 0x07)
		lsc_start_addr = LSC_GROUP2_FLAG;//53b8
	else if (lsc_grp_flag == 0x01)
		lsc_start_addr = LSC_GROUP3_FLAG;//8E20
	else {
		LOG_INF("no lsc OTP c8496_data_info\n");
		return 0;
	}

	//check_sum_lsc_cal += lsc_grp_flag;

	c8496_otp_read_group(lsc_start_addr,c8496_data_lsc,LSC_INFO_SIZE + 1);

	for (i = 0; i < LSC_INFO_SIZE; i++) {
		check_sum_lsc_cal += c8496_data_lsc[i];
	}
	LOG_INF("check_sum_lsc_cal =0x%x \n",check_sum_lsc_cal);
	check_sum_lsc = c8496_data_lsc[LSC_INFO_SIZE];
	check_sum_lsc_cal = (check_sum_lsc_cal % 255) + 1;

	LOG_INF("=== c8496 LSC check_sum_lsc=0x%x, check_sum_lsc_cal=0x%x ===\n", check_sum_lsc, check_sum_lsc_cal);
	if (check_sum_lsc == check_sum_lsc_cal) {
		LOG_INF("=== c8496 LSC date sucess!\n");
		return 1;
	} else {
		return 0;
	}
}

int c8496_sensor_otp_info(void) {
	int ret = 0, c8496_module_valid = 0, c8496_awb_valid = 0,c8496_lsc_valid = 0;
	//int c8496_sn_valid = 0,
	LOG_INF("come to %s:%d E!\n", __func__, __LINE__);

	c8496_otp_init();
	ret = read_c8496_module_info();
	if (ret != 1) {
		c8496_module_valid = 0;
		LOG_INF("=== c8496_data_info invalid ===\n");
	} else {
		c8496_module_valid = 1;
	}
	ret = read_c8496_sn_info();
	/*
	if(ret != 1){
		c8496_sn_valid = 0;
		LOG_INF("=== c8496_data_sn invalid ===\n");
	}else{
		c8496_sn_valid = 1;
	}
	*/
	ret = read_c8496_awb_info();
	if (ret != 1) {
		c8496_awb_valid = 0;
		LOG_INF("=== c8496_data_awb invalid ===\n");
	} else {
		c8496_awb_valid = 1;
	}
	ret = read_c8496_lsc_info();
	if (ret != 1) {
		c8496_lsc_valid = 0;
		LOG_INF("=== c8496_data_lsc invalid ===\n");
	} else {
		c8496_lsc_valid = 1;
	}

	c8496_otp_close();
	if (c8496_module_valid == 0 || c8496_awb_valid == 0 || c8496_lsc_valid == 0) {
		LOG_INF("=== c8496_otp_data_invalid ===\n");
	} else {
		LOG_INF("=== c8496_otp_data_sucess ===\n");
	}
	
	if (!ret) {
		LOG_INF("n28c8496frontdc read OTP failed ret:%d, \n",ret);
		return ERROR_SENSOR_CONNECT_FAIL;
	} else {
		LOG_INF("n28c8496frontdc read OTP successed: ret: %d \n",ret);
		n28c8496frontdc_eeprom_data.dataBuffer = kmalloc(n28c8496frontdc_eeprom_data.dataLength + OTP_MAP_VENDOR_HEAD_ID_BYTES, GFP_KERNEL);
		if (n28c8496frontdc_eeprom_data.dataBuffer == NULL) {
			LOG_INF("n28c8496frontdc_eeprom_data->dataBuffer is malloc fail\n");
			return -EFAULT;
	}
	memcpy(n28c8496frontdc_eeprom_data.dataBuffer, (void *)&(n28c8496frontdc_eeprom_data.sensorVendorid), OTP_MAP_VENDOR_HEAD_ID_BYTES);
	memcpy(n28c8496frontdc_eeprom_data.dataBuffer + OTP_MAP_VENDOR_HEAD_ID_BYTES, (u8 *)&c8496_data_info, MODULE_INFO_SIZE);
	memcpy(n28c8496frontdc_eeprom_data.dataBuffer+ MODULE_INFO_SIZE + OTP_MAP_VENDOR_HEAD_ID_BYTES, (u8 *)&c8496_data_sn, SN_INFO_SIZE);
	memcpy(n28c8496frontdc_eeprom_data.dataBuffer+ SN_INFO_SIZE+MODULE_INFO_SIZE + OTP_MAP_VENDOR_HEAD_ID_BYTES, (u8 *)&c8496_data_awb, AWB_INFO_SIZE);
	memcpy(n28c8496frontdc_eeprom_data.dataBuffer+ SN_INFO_SIZE+MODULE_INFO_SIZE+AWB_INFO_SIZE + OTP_MAP_VENDOR_HEAD_ID_BYTES, (u8 *)&c8496_data_lsc, LSC_INFO_SIZE);
	imgsensor_set_eeprom_data_bywing(&n28c8496frontdc_eeprom_data, NULL);
	LOG_INF("read c8496 otp data success %d \n",ret);
	}
	return ret;
}
#endif