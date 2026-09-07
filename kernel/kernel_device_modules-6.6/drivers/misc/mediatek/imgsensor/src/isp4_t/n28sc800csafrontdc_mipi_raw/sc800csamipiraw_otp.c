#include <linux/delay.h>
#include "sc800csamipiraw_otp.h"

#if SC800CSA_OTP_FUNCTION

#define PFX "sc800csamipiraw_otp"
#define LOG_INF(format, args...)    pr_info(PFX "[%s] " format, __FUNCTION__, ##args)

static int otp_group_1 = 0x01;
static int otp_group_2 = 0x07;

static unsigned char sc800csa_data_lsc[LSC_INFO_SIZE + 1] = {0};/*Add check sum*/
static unsigned char sc800csa_data_awb[AWB_INFO_SIZE + 1] = {0};/*Add check sum*/
static unsigned char sc800csa_data_info[MODULE_INFO_SIZE + 1] = {0};/*Add check sum*/
static unsigned char sc800csa_data_sn[SN_INFO_SIZE + 1] = {0};/*Add check sum*/

#define SC800CSA_SLAVE_ID  0x20
inline UINT16 sc800csa_read_eeprom(UINT32 addr)
{
	UINT16 get_byte = 0;
	char pu_send_cmd[2] = {(char)(addr >> 8) , (char)(addr & 0xFF) };

	iReadRegI2C(pu_send_cmd, 2, (u8*)&get_byte, 1, SC800CSA_SLAVE_ID);

	return get_byte;
}

inline void sc800csa_write_eeprom(UINT32 addr, UINT32 para)
{
	char pu_send_cmd[3] = {(char)(addr >> 8), (char)(addr & 0xFF), (char)(para & 0xFF)};

	iWriteRegI2C(pu_send_cmd, 3, SC800CSA_SLAVE_ID);
}

static unsigned int sc800csa_sensor_otp_read_lsc_data(int page,unsigned int ui4_length,unsigned char *pinputdata)
{
	int i;
	UINT32 checksum_cal = 0;
	UINT32 start_addr = 0;
	//UINT32 end_addr = 0;
	UINT32 ui4_lsc_offset = 0;
	UINT32 ui4_lsc_length = 0;

	start_addr = page * 0x200 + 0x7E00;//set start address in page
	//end_addr = start_addr + 0x1ff;//set end address in page
	ui4_lsc_offset = start_addr + 122;
	ui4_lsc_length = ui4_length;
	switch(page)
	{
		case 2:
			ui4_lsc_offset = 0x82CA;
			break;
		case 3:
			ui4_lsc_offset = 0x847A;
			break;
		case 4:
			ui4_lsc_offset = 0x867A;
			break;
		case 5:
			ui4_lsc_offset = 0x887A;
			break;
		case 6:
			ui4_lsc_offset = 0x8A7A;
			break;
		case 7:
			ui4_lsc_offset = 0x8C7A;
			break;
		case 8:
			ui4_lsc_offset = 0x8E7A;
			break;
		case 9:
			ui4_lsc_offset = 0x907A;
			break;
		case 10:
			ui4_lsc_offset = 0x927A;
			break;
		case 11:
			ui4_lsc_offset = 0x947A;
			break;
		default:
			break;
	}

	for(i =0; i<ui4_lsc_length; i++)
	{
		pinputdata[i] = sc800csa_read_eeprom(ui4_lsc_offset + i);
		checksum_cal += pinputdata[i];
#if SC800CSA_ST_DEBUG_ON
		pr_info("sc800csa_sensor_otp_read_data addr=0x%x, data=0x%x\n", ui4_lsc_offset + i, pinputdata[i]);
#endif
	}

	checksum_cal = checksum_cal % 255;
	pr_info("sc800csa_sensor read otp page:%d lsc_data success,checksum_cal:0x%x\n", page, checksum_cal);

	return checksum_cal;
}

static int sc800csa_set_page_and_load_data(int page)//set page
{
	UINT32 start_addr = 0;
	UINT32 end_addr = 0;
	int delay = 0;
	int page_flag = 0;

	start_addr = page * 0x200 + 0x7E00;  //start address
	end_addr = start_addr + 0x1ff;       //end address
	page_flag = page*2 -1;               //page flag
	sc800csa_write_eeprom(0x4408, (start_addr>>8) & 0xff);   //start address high 8 bit
	sc800csa_write_eeprom(0x4409, start_addr & 0xff);        //start address low 8 bit
	sc800csa_write_eeprom(0x440a, (end_addr>>8) & 0xff);     //end address high 8 bit
	sc800csa_write_eeprom(0x440b, end_addr & 0xff);          //end address low 8 bit

	sc800csa_write_eeprom(0x4401, 0x13);                //address set finished
	sc800csa_write_eeprom(0x4412, page_flag & 0xff);    //set page
	sc800csa_write_eeprom(0x4407, 0x00);                //set page finished
	sc800csa_write_eeprom(0x4400, 0x11);                //manual load begin

	while((sc800csa_read_eeprom(0x4420)&0x01) == 0x01)  //wait for manual load finished
	{
		delay++;
		LOG_INF("sc800csa_set_page waitting, OTP is still busy for loading %d times\n", delay);
		if(delay == 10)
		{
			LOG_INF("sc800csa_set_page fail, load timeout!!!\n");
			return SC800CSA_ST_RET_FAIL;
		}
		mdelay(10);
	}
	LOG_INF("sc800csa_set_page success\n");
	return SC800CSA_ST_RET_SUCCESS;
}

static int sc800csa_set_threshold(u8 threshold)//set thereshold
{
	int threshold_reg1[3] = {0x48, 0x48, 0x48};
	int threshold_reg2[3] = {0x38, 0x18, 0x58};
	int threshold_reg3[3] = {0x41, 0x41, 0x41};

	sc800csa_write_eeprom(0x36b0, threshold_reg1[threshold]);
	sc800csa_write_eeprom(0x36b1, threshold_reg2[threshold]);
	sc800csa_write_eeprom(0x36b2, threshold_reg3[threshold]);

	LOG_INF("sc800csa_set_threshold %d\n", threshold);

	return SC800CSA_ST_RET_SUCCESS;
}

static int sc800csa_sensor_otp_read_lsc_info_group1(void)
{
	int ret = SC800CSA_ST_RET_FAIL;
	int page = SC800CSA_ST_PAGE2;
	UINT32 checksum_cal = 0;
	UINT32 checksum = 0;

	pr_info("sc800csa_sensor_otp_read_lsc_info group1 begin!\n");
	{
		checksum_cal += sc800csa_sensor_otp_read_lsc_data(page, 0x136, &sc800csa_data_lsc[0]);
		page = page + 1;
		for(; page <= 5; page++)
		{
			sc800csa_set_page_and_load_data(page);   //set page--3,4,5 && load data
			checksum_cal += sc800csa_sensor_otp_read_lsc_data(page, 0x186, &sc800csa_data_lsc[0x136+(page-3)*0x186]);
		}
		sc800csa_set_page_and_load_data(page); //set page6
		checksum_cal += sc800csa_sensor_otp_read_lsc_data(page, 0x184, &sc800csa_data_lsc[0x136+(page-3)*0x186]);

		checksum_cal = checksum_cal%255 + 1;
		checksum  = sc800csa_read_eeprom(LSC_GROUP1_CHECKSUM);
		if(checksum_cal == checksum)
		{
			pr_info("sc800csa_sensor_otp_read_lsc_info group1 checksum pass!\n");
			ret = SC800CSA_ST_RET_SUCCESS;
		}
		else
		{
			pr_info("sc800csa_sensor_otp_read_lsc_info group1 checksum fail,checksum_cal:0x%x,checksum:0x%x!\n", checksum_cal, checksum);
			ret = SC800CSA_ST_RET_FAIL;
		}
	}
	pr_info("sc800csa_sensor_otp_read_lsc_info group1 end!\n");

	return ret;
}

static int sc800csa_sensor_otp_read_lsc_info_group2(void)
{
	int ret = SC800CSA_ST_RET_FAIL;
	int page = 6;
	UINT32 checksum_cal = 0;
	UINT32 checksum = 0;

	pr_info("sc800csa_sensor_otp_read_lsc_info group2 begin!\n");
	{
		sc800csa_set_page_and_load_data(page);  //set page--6
		checksum_cal += sc800csa_read_eeprom(LSC_GROUP2_FIRST);
		page = page + 1;
		for(; page <= 10; page++)
		{
			sc800csa_set_page_and_load_data(page);//set page--7,8,9,10, && load data
			checksum_cal += sc800csa_sensor_otp_read_lsc_data(page, 0x186, &sc800csa_data_lsc[0x01+(page-7)*0x186]);
		}
		sc800csa_set_page_and_load_data(page);//set page11
		checksum_cal += sc800csa_sensor_otp_read_lsc_data(page, 0x133, &sc800csa_data_lsc[0x01+(page-7)*0x186]);

		checksum_cal = checksum_cal%255 + 1;
		checksum = sc800csa_read_eeprom(LSC_GROUP2_CHECKSUM);
		if(checksum_cal == checksum)
		{
			pr_info("sc800csa_sensor_otp_read_lsc_info group2 checksum pass!\n");
			ret = SC800CSA_ST_RET_SUCCESS;
		}
		else
		{
			pr_info("sc800csa_sensor_otp_read_lsc_info group2 checksum fail,checksum_cal:0x%x,checksum:0x%x!\n",checksum_cal,checksum);
			ret = SC800CSA_ST_RET_FAIL;
		}
	}
	pr_info("sc800csa_sensor_otp_read_lsc_info group2 end!\n");

	return ret;
}
static int sc800csa_sensor_otp_read_data(unsigned int ui4_offset,unsigned int ui4_length, unsigned char *pinputdata)
{
	int i;
	UINT32 checksum_cal = 0;
	UINT32 checksum = 0;

	for(i=0; i < ui4_length; i++)
	{
		pinputdata[i] = sc800csa_read_eeprom(ui4_offset + i);
		checksum_cal += pinputdata[i];
#if SC800CSA_ST_DEBUG_ON
		LOG_INF("sc800csa_sensor_otp_read_data addr=0x%x, data=0x%x\n", ui4_offset + i, pinputdata[i]);
#endif
	}

	checksum = pinputdata[i-1];
	checksum_cal -= checksum;
	checksum_cal = checksum_cal%255 + 1;
	if(checksum_cal != checksum)
	{
		LOG_INF("sc800csa_sensor_otp_read_data checksum fail, checksum_cal=0x%x, checksum=0x%x\n", checksum_cal, checksum);
		return SC800CSA_ST_RET_FAIL;
	}
	LOG_INF("sc800csa_sensor_otp_read_data success\n");

	return SC800CSA_ST_RET_SUCCESS;
}

//read otp module_info
static int sc800csa_sensor_otp_read_module_info(void)
{
	int ret = SC800CSA_ST_RET_FAIL;

	LOG_INF("sc800csa_sensor_otp_read_module_info begin!\n");
	if(sc800csa_read_eeprom(MODULE_INFO_FLAG) == otp_group_1)
	{
		ret = sc800csa_sensor_otp_read_data(MODULE_GROUP1_ADDR, MODULE_INFO_SIZE + 1, &sc800csa_data_info[0]);
		LOG_INF("sc800csa_sensor_otp_read_module_info group1 checksum success!\n");
	} else if(sc800csa_read_eeprom(MODULE_INFO_FLAG) == otp_group_2) {
		ret = sc800csa_sensor_otp_read_data(MODULE_GROUP2_ADDR, MODULE_INFO_SIZE + 1, &sc800csa_data_info[0]);
		LOG_INF("sc800csa_sensor_otp_read_module_info group2 checksum success!\n");
	} else {
		LOG_INF("sc800csa_sensor_otp_read_module_info flag failed:%d\n", sc800csa_read_eeprom(MODULE_INFO_FLAG));
	}
	LOG_INF("sc800csa_sensor_otp_read_module_info end!\n");

	return ret;
}

//read otp sn
static int sc800csa_sensor_otp_read_sn_info(void)
{
	int ret = SC800CSA_ST_RET_FAIL;

	LOG_INF("sc800csa_sensor_otp_read_sn_info begin!\n");
	if(sc800csa_read_eeprom(SN_INFO_FLAG) == otp_group_1)
	{
		ret = sc800csa_sensor_otp_read_data(SN_GROUP1_ADDR, SN_INFO_SIZE + 1, &sc800csa_data_sn[0]);
		LOG_INF("sc800csa_sensor_otp_read_sn_info group1 checksum success!\n");
	} else if(sc800csa_read_eeprom(SN_INFO_FLAG) == otp_group_2) {
		ret = sc800csa_sensor_otp_read_data(SN_GROUP2_ADDR, SN_INFO_SIZE + 1, &sc800csa_data_sn[0]);
		LOG_INF("sc800csa_sensor_otp_read_sn_info group2 checksum success!\n");
	} else {
		LOG_INF("sc800csa_sensor_otp_read_sn_info flag failed:%d\n", sc800csa_read_eeprom(SN_INFO_FLAG));
	}
	LOG_INF("sc800csa_sensor_otp_read_sn_info end!\n");

	return ret;
}

//read otp awb
static int sc800csa_sensor_otp_read_awb_info(void)
{
	int ret = SC800CSA_ST_RET_FAIL;

	LOG_INF("sc800csa_sensor_otp_read_awb_info begin!\n");
	if(sc800csa_read_eeprom(AWB_INFO_FLAG) == otp_group_1)
	{
		ret = sc800csa_sensor_otp_read_data(AWB_GROUP1_FLAG, AWB_INFO_SIZE + 1, &sc800csa_data_awb[0]);
		LOG_INF("sc800csa_sensor_otp_read_awb group1 checksum success!\n");
	} else if(sc800csa_read_eeprom(AWB_INFO_FLAG) == otp_group_2) {
		ret = sc800csa_sensor_otp_read_data(AWB_GROUP2_FLAG, AWB_INFO_SIZE + 1, &sc800csa_data_awb[0]);
		LOG_INF("sc800csa_sensor_otp_read_awb group2 checksum success!\n");
	} else {
		LOG_INF("sc800csa_sensor_otp_read_awb flag failed:%d\n", sc800csa_read_eeprom(AWB_INFO_FLAG));
	}
	LOG_INF("sc800csa_sensor_otp_read_awb_info end!\n");

	return ret;
}

//read otp lsc
static int sc800csa_sensor_otp_read_lsc_info(void)
{
	int ret = SC800CSA_ST_RET_FAIL;

	LOG_INF("sc800csa_sensor_otp_read_lsc_info begin!\n");
	if(sc800csa_read_eeprom(LSC_INFO_FLAG) == otp_group_1)
	{
		ret = sc800csa_sensor_otp_read_lsc_info_group1();
		LOG_INF("sc800csa_sensor_otp_read_lsc group1 checksum success!\n");
	} else if(sc800csa_read_eeprom(LSC_INFO_FLAG) == otp_group_2) {
		ret = sc800csa_sensor_otp_read_lsc_info_group2();
		LOG_INF("sc800csa_sensor_otp_read_lsc group2 checksum success!\n");
	} else {
		LOG_INF("sc800csa_sensor_otp_read_lsc flag failed:%d\n", sc800csa_read_eeprom(LSC_INFO_FLAG));
	}
	LOG_INF("sc800csa_sensor_otp_read_lsc_info end!\n");

	return ret;
}

int sc800csa_sensor_otp_info(void)
{
	int ret = SC800CSA_ST_RET_FAIL;
	int threshold = 0;

	//read module info to judge the otp data which group
	for(threshold=0; threshold < 3; threshold++)
	{
		sc800csa_set_threshold(threshold);
		sc800csa_set_page_and_load_data(SC800CSA_ST_PAGE2);
		LOG_INF("sc800csa read module info in treshold R%d\n", threshold);
		ret = sc800csa_sensor_otp_read_module_info();
		if(ret == SC800CSA_ST_RET_FAIL){
			LOG_INF("sc800csa read module info failed!!!\n");
			continue;
		}
		ret = sc800csa_sensor_otp_read_sn_info();
		if(ret == SC800CSA_ST_RET_FAIL){
			LOG_INF("sc800csa read sn info failed!!!\n");
			continue;
		}
		ret = sc800csa_sensor_otp_read_awb_info();
		if(ret == SC800CSA_ST_RET_FAIL){
			LOG_INF("sc800csa read awb info failed!!!\n");
			continue;
		}
		ret = sc800csa_sensor_otp_read_lsc_info();
		if(ret == SC800CSA_ST_RET_FAIL){
			LOG_INF("sc800csa read lsc info failed!!!\n");
			continue;
		}
		break;  //If read OTP correctly once, exit the loop
	}

	if(ret == SC800CSA_ST_RET_FAIL)
	{
		LOG_INF("sc800csa read lsc info in treshold R1 R2 R3 all failed!!!\n");
		return ret;
	}
	else
	{
		n28sc800csafrontdc_eeprom_data.dataBuffer = kmalloc(n28sc800csafrontdc_eeprom_data.dataLength + OTP_MAP_VENDOR_HEAD_ID_BYTES, GFP_KERNEL);
		if (n28sc800csafrontdc_eeprom_data.dataBuffer == NULL) {
			LOG_INF("n28sc800csafrontdc_eeprom_data->dataBuffer is malloc fail\n");
			return -EFAULT;
		}

		memcpy(n28sc800csafrontdc_eeprom_data.dataBuffer, (void *)&(n28sc800csafrontdc_eeprom_data.sensorVendorid), OTP_MAP_VENDOR_HEAD_ID_BYTES);
		memcpy(n28sc800csafrontdc_eeprom_data.dataBuffer + MODULE_OFFSET, (u8 *)&sc800csa_data_info, MODULE_INFO_SIZE);
		memcpy(n28sc800csafrontdc_eeprom_data.dataBuffer + SN_OFFSET, (u8 *)&sc800csa_data_sn, SN_INFO_SIZE);
		memcpy(n28sc800csafrontdc_eeprom_data.dataBuffer + AWB_OFFSET, (u8 *)&sc800csa_data_awb, AWB_INFO_SIZE);
		memcpy(n28sc800csafrontdc_eeprom_data.dataBuffer + LSC_OFFSET, (u8 *)&sc800csa_data_lsc, LSC_INFO_SIZE);
		imgsensor_set_eeprom_data_bywing(&n28sc800csafrontdc_eeprom_data, NULL);
		LOG_INF("read sc800csa otp data success %d \n",ret);
	}

	return ret;
}

#endif