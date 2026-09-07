#include <linux/videodev2.h>
#include <linux/i2c.h>
#include <linux/platform_device.h>
#include <linux/delay.h>
#include <linux/cdev.h>
#include <linux/uaccess.h>
#include <linux/fs.h>
#include <linux/atomic.h>
#include <linux/types.h>
#include "cam_cal_define.h"
#include <linux/slab.h>

#include "w2hi1336fronttruly_mipiraw_Sensor.h"
#include "w2hi1336fronttruly_mipiraw_feature.h"

#include "w2hi1336fronttruly_mipiraw_otp.h"

#define PFX "hi1336_camera_sensor"
#define LOG_INF(format, args...)    \
	pr_info(PFX "[%s] " format, __func__, ##args)

#define GROUP_FLAG 0x0400
#define GROUP1_ADDR 0x0401
#define GROUP2_ADDR 0x0B75

#define SN_INFO_SIZE 12
#define MODULE_INFO_SIZE 7
#define AWB_DATA_SIZE 16
#define LSC_DATA_SIZE 1868

#define OTP_READ_ADDR 0x0308

static unsigned char hi1336_data_sn[SN_INFO_SIZE + 1] = {0};/*Add check sum*/
static unsigned char hi1336_data_lsc[LSC_DATA_SIZE + 1] = {0};/*Add check sum*/
static unsigned char hi1336_data_awb[AWB_DATA_SIZE + 1] = {0};/*Add check sum*/
static unsigned char hi1336_data_module[MODULE_INFO_SIZE + 1] = {0};/*Add check sum*/
extern bool imgsensor_set_eeprom_data_bywing(struct stCAM_CAL_DATAINFO_STRUCT* pData, struct stCAM_CAL_CHECKSUM_STRUCT* checksumData);
#if HI1336_OTP_DUMP
static void dumpEEPROMData1(int u4Length,u8* pu1Params)
{
	int i = 0;
	for(i = 0; i < u4Length; i += 16){
		if(u4Length - i  >= 16){
			LOG_INF("eeprom[%d-%d]:0x%2x 0x%2x 0x%2x 0x%2x 0x%2x 0x%2x 0x%2x 0x%2x 0x%2x 0x%2x 0x%2x 0x%2x 0x%2x 0x%2x 0x%2x 0x%2x ",
			i,i+15,pu1Params[i],pu1Params[i+1],pu1Params[i+2],pu1Params[i+3],pu1Params[i+4],pu1Params[i+5],pu1Params[i+6]
			,pu1Params[i+7],pu1Params[i+8],pu1Params[i+9],pu1Params[i+10],pu1Params[i+11],pu1Params[i+12],pu1Params[i+13],pu1Params[i+14]
			,pu1Params[i+15]);
		}else{
			int j = i;
			for(;j < u4Length;j++)
			LOG_INF("eeprom[%d] = 0x%2x ",j,pu1Params[j]);
		}
	}
	LOG_INF("\n");
}
#endif

static int hi1336_sensor_otp_info(void)
{
	uint8_t flag;
	int result = 0;
	int check_sum_cal = 0;
	uint16_t Address_start;
	int r = 0,b = 0,gr = 0, gb = 0, golden_r = 0, golden_b = 0, golden_gr = 0, golden_gb = 0;
	int i = 0;

	LOG_INF("come to %s:%d E!\n", __func__, __LINE__);

	/* 1.sensor init */
	write_cmos_sensor(0x0b00, 0x0000);
	write_cmos_sensor(0x2000, 0x0021);
	write_cmos_sensor(0x2002, 0x04A5);
	write_cmos_sensor(0x2004, 0x0004);
	write_cmos_sensor(0x2006, 0xC09C);
	write_cmos_sensor(0x2008, 0x0064);
	write_cmos_sensor(0x200A, 0x088E);
	write_cmos_sensor(0x200C, 0x01C2);
	write_cmos_sensor(0x200E, 0x00B4);
	write_cmos_sensor(0x2010, 0x4020);
	write_cmos_sensor(0x2012, 0x90F2);
	write_cmos_sensor(0x2014, 0x0010);
	write_cmos_sensor(0x2016, 0x0260);
	write_cmos_sensor(0x2018, 0x2002);
	write_cmos_sensor(0x201A, 0x12B0);
	write_cmos_sensor(0x201C, 0xD4AA);
	write_cmos_sensor(0x201E, 0x12B0);
	write_cmos_sensor(0x2020, 0xD5FA);
	write_cmos_sensor(0x2022, 0x4392);
	write_cmos_sensor(0x2024, 0x732A);
	write_cmos_sensor(0x2026, 0x4130);
	write_cmos_sensor(0x2FFE, 0xC114);
	write_cmos_sensor(0x3224, 0xF012);
	write_cmos_sensor(0x32A0, 0x0000);
	write_cmos_sensor(0x32A2, 0x0000);
	write_cmos_sensor(0x32A4, 0x0000);
	write_cmos_sensor(0x32B0, 0x0000);
	write_cmos_sensor(0x32C0, 0x0000);
	write_cmos_sensor(0x32C2, 0x0000);
	write_cmos_sensor(0x32C4, 0x0000);
	write_cmos_sensor(0x32C6, 0x0000);
	write_cmos_sensor(0x32C8, 0x0000);
	write_cmos_sensor(0x32CA, 0x0000);
	write_cmos_sensor(0x0734, 0x4b0b);
	write_cmos_sensor(0x0736, 0xd8b0);
	write_cmos_sensor(0x035e, 0x0701);
	write_cmos_sensor(0x027e, 0x0100);

	/* 2.otp read setting */
	write_cmos_sensor_8(0x0b02, 0x01);
	write_cmos_sensor_8(0x0809, 0x00);
	write_cmos_sensor_8(0x0b00, 0x00);
	mdelay(10);
	write_cmos_sensor_8(0x0260, 0x10);
	write_cmos_sensor_8(0x0809, 0x01);
	write_cmos_sensor_8(0x0b00, 0x01);
	mdelay(1); // sleep 1msec

	/* 3.otp read flag */
	write_cmos_sensor_8(0x030a, 0x04);
	write_cmos_sensor_8(0x030b, 0x00);
	write_cmos_sensor_8(0x0302, 0x01); //read enable
	flag= read_cmos_sensor(0x0308); //eeprom address:0x0400
	LOG_INF("HI1336 OTP flag(0x%x)\n",flag);
	if (flag == 0x01)
	{
		Address_start=GROUP1_ADDR;
		LOG_INF("HI1336 OTP USR GROUP1\n");
	}else{
	    Address_start=GROUP2_ADDR;
		LOG_INF("HI1336 OTP USR GROUP2\n");
	}

	/* 4.read module info */
	write_cmos_sensor_8(0x030a, ((Address_start+i) >> 8)&0xff);
	write_cmos_sensor_8(0x030b, (Address_start+i)&0xff);
	write_cmos_sensor_8(0x0302, 0x01); //read enable
	check_sum_cal = 0;
	for (i =0; i <MODULE_INFO_SIZE; i++) {
		hi1336_data_module[i] = read_cmos_sensor(OTP_READ_ADDR);	//otp data read
		check_sum_cal += hi1336_data_module[i];
	}
	hi1336_data_module[MODULE_INFO_SIZE] = read_cmos_sensor(OTP_READ_ADDR); // module checksum_value
	check_sum_cal = (check_sum_cal % 255) + 1;
	LOG_INF("=== HI1336 INFO module_id=0x%x position=0x%x ===\n", hi1336_data_module[0], hi1336_data_module[1]);
	LOG_INF("=== HI1336 INFO lens_id=0x%x,vcm_id=0x%x ===\n",hi1336_data_module[2], hi1336_data_module[3]);
	LOG_INF("=== HI1336 INFO date is %d-%d-%d ===\n",hi1336_data_module[4],hi1336_data_module[5],hi1336_data_module[6]);
	LOG_INF("=== HI1336 INFO check_sum=0x%x,check_sum_cal=0x%x ===\n", hi1336_data_module[7], check_sum_cal);
	#if HI1336_OTP_DUMP
	dumpEEPROMData1(MODULE_INFO_SIZE,&hi1336_data_module[0]);
	#endif
	if(check_sum_cal != hi1336_data_module[MODULE_INFO_SIZE])
	{
		LOG_INF("HI1336 read module info failed!!!\n");
		result = -1;
		goto function_exit;
	}

	/* 5.read sn info */
	check_sum_cal = 0;
	for (i =0; i <SN_INFO_SIZE; i++) {
		hi1336_data_sn[i] = read_cmos_sensor(OTP_READ_ADDR);	//otp data read
		check_sum_cal += hi1336_data_sn[i];
		LOG_INF("=== HI1336 SN[%d] is 0x%x ===\n",i,hi1336_data_sn[i]);
	}
	hi1336_data_sn[SN_INFO_SIZE] = read_cmos_sensor(OTP_READ_ADDR); // module checksum_value
	check_sum_cal = (check_sum_cal % 255) + 1;
	if(check_sum_cal != hi1336_data_sn[SN_INFO_SIZE])
	{
		LOG_INF("HI1336 read sn info failed!!!\n");
		//result = -1;
	}

	/* 6.read awb info */
	check_sum_cal = 0;
	for (i =0; i <AWB_DATA_SIZE; i++) {
		hi1336_data_awb[i] = read_cmos_sensor(OTP_READ_ADDR);	//otp data read
		check_sum_cal += hi1336_data_awb[i];
	}
	hi1336_data_awb[AWB_DATA_SIZE] = read_cmos_sensor(OTP_READ_ADDR); // module checksum_value
	check_sum_cal = (check_sum_cal % 255) + 1;
	r = ((hi1336_data_awb[1]<<8)&0xff00)|(hi1336_data_awb[0]&0xff);
	b = ((hi1336_data_awb[3]<<8)&0xff00)|(hi1336_data_awb[2]&0xff);
	gr = ((hi1336_data_awb[5]<<8)&0xff00)|(hi1336_data_awb[4]&0xff);
	gb = ((hi1336_data_awb[7]<<8)&0xff00)|(hi1336_data_awb[6]&0xff);
	golden_r = ((hi1336_data_awb[9]<<8)&0xff00)|(hi1336_data_awb[8]&0xff);
	golden_b = ((hi1336_data_awb[11]<<8)&0xff00)|(hi1336_data_awb[10]&0xff);
	golden_gr = ((hi1336_data_awb[13]<<8)&0xff00)|(hi1336_data_awb[12]&0xff);
	golden_gb = ((hi1336_data_awb[15]<<8)&0xff00)|(hi1336_data_awb[14]&0xff);
	LOG_INF("=== HI1336 AWB r=0x%x, b=0x%x, gr=%x, gb=0x%x ===\n", r, b,gb, gr);
	LOG_INF("=== HI1336 AWB gr=0x%x,gb=0x%x,gGr=%x, gGb=0x%x ===\n", golden_r, golden_b, golden_gr, golden_gb);
	LOG_INF("=== HI1336 AWB check_sum_awb=0x%x,check_sum_awb_cal=0x%x ===\n",hi1336_data_awb[AWB_DATA_SIZE],check_sum_cal);
	#if HI1336_OTP_DUMP
	dumpEEPROMData1(AWB_DATA_SIZE,&hi1336_data_awb[0]);
	#endif
	if(check_sum_cal != hi1336_data_awb[AWB_DATA_SIZE])
	{
		LOG_INF("HI1336 read AWB info failed!!!\n");
		result = -1;
		goto function_exit;
	}

	/* 7.read lsc info */
	check_sum_cal = 0;
	for (i =0; i <LSC_DATA_SIZE; i++) {
		hi1336_data_lsc[i] = read_cmos_sensor(OTP_READ_ADDR);	//otp data read
		check_sum_cal += hi1336_data_lsc[i];
	}
	hi1336_data_lsc[LSC_DATA_SIZE] = read_cmos_sensor(OTP_READ_ADDR); // module checksum_value
	check_sum_cal = (check_sum_cal % 255) + 1;
	LOG_INF("=== HI1336 LSC check_sum_lsc=0x%x, check_sum_lsc_cal=0x%x ===\n", hi1336_data_lsc[LSC_DATA_SIZE], check_sum_cal);
	#if HI1336_OTP_DUMP
	dumpEEPROMData1(LSC_DATA_SIZE,&hi1336_data_lsc[0]);
	#endif
	if(check_sum_cal != hi1336_data_lsc[LSC_DATA_SIZE])
	{
		LOG_INF("HI1336 read lsc info failed!!!\n");
		result = -1;
		goto function_exit;
	}

function_exit:
	write_cmos_sensor_8(0x0809, 0x00); // stream off
	write_cmos_sensor_8(0x0b00, 0x00); // stream off
	mdelay(10); // sleep 10msec
	write_cmos_sensor_8(0x0260, 0x00); // OTP mode display
	write_cmos_sensor_8(0x0809, 0x01); // stream on
	write_cmos_sensor_8(0x0b00, 0x01); // stream on
	mdelay(1);
	LOG_INF("%s Exit \n",__func__);
	return result;
}

#if 0
//+bug682590,zhanghengyuan.wt,ADD,2021/8/24,diff txd and st hi1336
static int read_hi1336_otp_moudle(void){

	int addr = imgsensor.i2c_write_id;
	char moudle_id;

	imgsensor.i2c_write_id = 0xA0;
	moudle_id = read_cmos_sensor(0x01);
	imgsensor.i2c_write_id = addr;

	if(moudle_id == 0x19){
		printk("this is txd hi1336 moudle\n");
		return 1;
	}

	return 0;
}
//-bug682590,zhanghengyuan.wt,ADD,2021/8/24,diff txd and st hi1336
#endif


static struct stCAM_CAL_DATAINFO_STRUCT w2hi1336fronttruly_eeprom_data ={
	.sensorID= W2HI1336FRONTTRULY_SENSOR_ID,
	.deviceID = 0x02,
	.dataLength =  0x0766,
	.sensorVendorid = 0x06050100,
	.vendorByte = {1,2,3,4},
	.dataBuffer = NULL,
};


int hi1336_init_sensor_cali_info(void)
{
	int ret;
	ret = hi1336_sensor_otp_info();
	if(ret<0){
		pr_err("get eeprom moudle failed\n");
		//*sensor_id = 0xFFFFFFFF;
		//return ERROR_SENSOR_CONNECT_FAIL;
	}else{
		pr_info("get eeprom data success\n");
		w2hi1336fronttruly_eeprom_data.dataBuffer = kmalloc(w2hi1336fronttruly_eeprom_data.dataLength + OTP_MAP_VENDOR_HEAD_ID_BYTES, GFP_KERNEL);
		if (w2hi1336fronttruly_eeprom_data.dataBuffer == NULL) {
			LOG_INF("w2hi1336fronttruly_eeprom_data->dataBuffer is malloc fail\n");
			return -EFAULT;
		}
		memcpy(w2hi1336fronttruly_eeprom_data.dataBuffer, (void *)&(w2hi1336fronttruly_eeprom_data.sensorVendorid), OTP_MAP_VENDOR_HEAD_ID_BYTES);

		memcpy(w2hi1336fronttruly_eeprom_data.dataBuffer + OTP_MAP_VENDOR_HEAD_ID_BYTES, (u8 *)&hi1336_data_module, MODULE_INFO_SIZE);
		memcpy(w2hi1336fronttruly_eeprom_data.dataBuffer + OTP_MAP_VENDOR_HEAD_ID_BYTES + MODULE_INFO_SIZE, (u8 *)&hi1336_data_awb, AWB_DATA_SIZE);
		memcpy(w2hi1336fronttruly_eeprom_data.dataBuffer + OTP_MAP_VENDOR_HEAD_ID_BYTES + MODULE_INFO_SIZE+AWB_DATA_SIZE, (u8 *)&hi1336_data_lsc, LSC_DATA_SIZE);

		imgsensor_set_eeprom_data_bywing(&w2hi1336fronttruly_eeprom_data, NULL);
	}
	return ret;
}