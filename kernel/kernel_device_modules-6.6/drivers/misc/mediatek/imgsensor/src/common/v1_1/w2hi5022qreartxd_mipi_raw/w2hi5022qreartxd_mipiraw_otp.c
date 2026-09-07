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
#include "w2hi5022qreartxd_mipiraw_Sensor.h"
#include "w2hi5022qreartxd_mipiraw_feature.h"

#include "w2hi5022qreartxd_mipiraw_otp.h"

#define PFX "w2hi5022qreartxd_camera_otp"
#define LOG_INF(format, args...)    \
     pr_info(PFX "[%s] " format, __func__, ##args)

#define EEPROM_BL24SA64D_ID 0xA0
kal_uint16 get_eeprom_0x003_value=0;
static kal_uint8 OPCBuffer[HI5022Q_OTP_OPC_LEN] = {0};
static kal_uint8 XGCbuffer[hi5022q_OTP_XGC_LEN] = {0};
static kal_uint8 QGCbuffer[hi5022q_OTP_QGC_LEN] = {0};
static kal_uint16 XGC_GB_setting_burst[XGC_GB_DATA_SIZE];
static kal_uint16 XGC_GR_setting_burst[XGC_GR_DATA_SIZE];
static kal_uint16 QBGC_setting_burst[QBGC_DATA_SIZE];
static kal_bool xgc_init_flag = false;
struct stCAM_CAL_DATAINFO_STRUCT w2hi5022qreartxd_eeprom_data ={
     .sensorID= W2HI5022QREARTXD_SENSOR_ID,
     .deviceID = 0x01,
     .dataLength = 0x21E5,
     .sensorVendorid = 0x19000101,
     .vendorByte = {1,2,3,4},
     .dataBuffer = NULL,
};
struct stCAM_CAL_CHECKSUM_STRUCT w2hi5022qreartxd_Checksum[12] =
{
      {MODULE_ITEM, 0x0000 + OTP_MAP_VENDOR_HEAD_ID_BYTES, 0x0000 + OTP_MAP_VENDOR_HEAD_ID_BYTES, 0x0007 + OTP_MAP_VENDOR_HEAD_ID_BYTES, 0x0008 + OTP_MAP_VENDOR_HEAD_ID_BYTES, 0x55},
      {SN_DATA, 0x0009 + OTP_MAP_VENDOR_HEAD_ID_BYTES, 0x0009 + OTP_MAP_VENDOR_HEAD_ID_BYTES, 0x0015 + OTP_MAP_VENDOR_HEAD_ID_BYTES, 0x0016 + OTP_MAP_VENDOR_HEAD_ID_BYTES, 0x55},
      {AWB_ITEM, 0x0017 + OTP_MAP_VENDOR_HEAD_ID_BYTES, 0x0017 + OTP_MAP_VENDOR_HEAD_ID_BYTES, 0x0027 + OTP_MAP_VENDOR_HEAD_ID_BYTES, 0x0028 + OTP_MAP_VENDOR_HEAD_ID_BYTES, 0x55},
      {AF_ITEM, 0x0029 + OTP_MAP_VENDOR_HEAD_ID_BYTES, 0x0029 + OTP_MAP_VENDOR_HEAD_ID_BYTES, 0x002E + OTP_MAP_VENDOR_HEAD_ID_BYTES, 0x002F + OTP_MAP_VENDOR_HEAD_ID_BYTES, 0x55},
      {LSC_ITEM, 0x0030 + OTP_MAP_VENDOR_HEAD_ID_BYTES, 0x0030 + OTP_MAP_VENDOR_HEAD_ID_BYTES, 0x077C + OTP_MAP_VENDOR_HEAD_ID_BYTES, 0x077D + OTP_MAP_VENDOR_HEAD_ID_BYTES, 0x55},
      {PDAF_ITEM, 0x077E + OTP_MAP_VENDOR_HEAD_ID_BYTES, 0x077E + OTP_MAP_VENDOR_HEAD_ID_BYTES, 0x096E + OTP_MAP_VENDOR_HEAD_ID_BYTES, 0x096F + OTP_MAP_VENDOR_HEAD_ID_BYTES, 0x55},
      {PDAF_PROC2_ITEM, 0x0970 + OTP_MAP_VENDOR_HEAD_ID_BYTES, 0x0970 + OTP_MAP_VENDOR_HEAD_ID_BYTES, 0x0D5C + OTP_MAP_VENDOR_HEAD_ID_BYTES, 0x0D5D + OTP_MAP_VENDOR_HEAD_ID_BYTES, 0x55},
      {hi5022q_XGC, 0x0D5E + OTP_MAP_VENDOR_HEAD_ID_BYTES, 0x0D5E + OTP_MAP_VENDOR_HEAD_ID_BYTES, 0x14DE + OTP_MAP_VENDOR_HEAD_ID_BYTES, 0x14DF + OTP_MAP_VENDOR_HEAD_ID_BYTES, 0x55},
      {hi5022q_QGC, 0x14E0 + OTP_MAP_VENDOR_HEAD_ID_BYTES, 0x14E0 + OTP_MAP_VENDOR_HEAD_ID_BYTES, 0x19E0 + OTP_MAP_VENDOR_HEAD_ID_BYTES, 0x19E1 + OTP_MAP_VENDOR_HEAD_ID_BYTES, 0x55},
      {hi5022q_OPC, 0x19E2 + OTP_MAP_VENDOR_HEAD_ID_BYTES, 0x19E2 + OTP_MAP_VENDOR_HEAD_ID_BYTES, 0x21E2 + OTP_MAP_VENDOR_HEAD_ID_BYTES, 0x21E3 + OTP_MAP_VENDOR_HEAD_ID_BYTES, 0x55},
      {TOTAL_ITEM, 0x0000 + OTP_MAP_VENDOR_HEAD_ID_BYTES, 0x0000 + OTP_MAP_VENDOR_HEAD_ID_BYTES, 0x21E3 + OTP_MAP_VENDOR_HEAD_ID_BYTES, 0x21E4 + OTP_MAP_VENDOR_HEAD_ID_BYTES, 0x55},
      {MAX_ITEM, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0x55},  // this line must haved
};

static kal_uint16 hi5022q_read_eeprom(kal_uint32 addr)
{
     kal_uint16 get_byte=0;

     char pu_send_cmd[2] = { (char)(addr >> 8), (char)(addr & 0xFF) };
     iReadRegI2C(pu_send_cmd, 2, (u8*)&get_byte, 1, EEPROM_BL24SA64D_ID);

     return get_byte;
}

static u8* hi5022q_read_eeprom_onboot(int size) {	
	int i = 0;
	u8* ebuf = NULL;
	ebuf = kmalloc(size, GFP_KERNEL);
	memcpy(ebuf, (void *)&(w2hi5022qreartxd_eeprom_data.sensorVendorid), OTP_MAP_VENDOR_HEAD_ID_BYTES);
	for (i = OTP_MAP_VENDOR_HEAD_ID_BYTES; i < size; i++) {			
			ebuf[i] = hi5022q_read_eeprom(i - OTP_MAP_VENDOR_HEAD_ID_BYTES);
	}
	return ebuf;
}

void hi5022q_apply_sensor_Cali(void)
{
     kal_uint8 *XGC_CalData = (kal_uint8*)XGCbuffer;
     kal_uint8 *QGC_CalData = (kal_uint8*)QGCbuffer;
     kal_uint16 length_xgc_gb = 0;
     kal_uint16 length_xgc_gr = 0;
     kal_uint16 length_qbgc = 0;
     kal_uint16 hi5021_xgc_gb_addr;
     kal_uint16 hi5021_xgc_gb_data;
     kal_uint16 hi5021_xgc_gr_addr;
     kal_uint16 hi5021_xgc_gr_data;
     kal_uint16 hi5021_qbgc_addr;
     kal_uint16 hi5021_qbgc_data;
     kal_uint16 TEMP1;
     kal_uint16 XGC_Flag;
     kal_uint16 QGC_Flag;

     int i = 0;
	 if (!xgc_init_flag)
	 	return;
     //////////// XGC_calibration_write////////////
     LOG_INF("[Hynix]:New_setting_new_SK_burst_v1.4 End\n");
     //LOG_INF("[Start]:apply_sensor_Cali start\n");

     /***********STEP 1: Streamoff**************/
     skhynix_write_cmos_sensor(0x0b00,0x0000);
     TEMP1= skhynix_read_cmos_sensor(0x0318);
     TEMP1= (TEMP1<<8)+(skhynix_read_cmos_sensor(0x0319));
     LOG_INF("XGC_TEPM1 = %d", TEMP1);
     skhynix_write_cmos_sensor(0x0318, TEMP1 & 0xFFEF); // XGC data loading disable B[4]
     XGC_Flag= ((skhynix_read_cmos_sensor(0x0318) << 8) | skhynix_read_cmos_sensor(0x0319));
     LOG_INF("XGC_Flag = %d", XGC_Flag);
     // XGC(Gb line) SRAM memory access enable
	//*********Burst write apply XGC Gb line start**************//

      skhynix_write_cmos_sensor(0xffff, 0x0A40); //// XGC(Gb line)  enable
      hi5021_xgc_gb_addr = 0x0000;
      for(i = 0; i < 960;i+=2)
      {
          hi5021_xgc_gb_data = ((((XGC_CalData[i+1]) & (0x00ff))<<8) +((XGC_CalData[i]) & (0x00ff)));
          XGC_GB_setting_burst[2*length_xgc_gb]=hi5021_xgc_gb_addr;
          XGC_GB_setting_burst[2*length_xgc_gb+1]=hi5021_xgc_gb_data;
          length_xgc_gb++;
          hi5021_xgc_gb_addr += 2;
          //skhynix_write_cmos_sensor(sensor_bpgc_addr,hi5021_xgc_gb_data);
      }

      if(length_xgc_gb == (960/2))
      {
           //hi5022qrearqt_table_write_cmos_sensor(
           //XGC_GB_setting_burst,sizeof(XGC_GB_setting_burst)/sizeof(kal_uint16));
           skhynix_write_burst_mode(XGC_GB_setting_burst,
                sizeof(XGC_GB_setting_burst)/sizeof(kal_uint16));
      }

      skhynix_write_cmos_sensor(0xffff, 0x0000); // XGC(Gr line) disable
      LOG_INF("[End]:XGC Gb Line End\n");

      skhynix_write_cmos_sensor(0xffff, 0x0B40); // XGC(Gr line) enable
      hi5021_xgc_gr_addr = 0x0000;
      for(i = 960; i < 1920;i+=2)
      {
           hi5021_xgc_gr_data = ((((XGC_CalData[i+1]) & (0x00ff))<<8) +((XGC_CalData[i]) & (0x00ff)));
           XGC_GR_setting_burst[2*length_xgc_gr]=hi5021_xgc_gr_addr;
           XGC_GR_setting_burst[2*length_xgc_gr+1]=hi5021_xgc_gr_data;
           length_xgc_gr++;
           hi5021_xgc_gr_addr += 2;
      }

      //Burst write XGC_Gr line //
      if(length_xgc_gr == (960/2))
      {
           //hi5022qrearqt_table_write_cmos_sensor(
           //XGC_GR_setting_burst,sizeof(XGC_GR_setting_burst)/sizeof(kal_uint16));
           skhynix_write_burst_mode(XGC_GR_setting_burst,
                sizeof(XGC_GR_setting_burst)/sizeof(kal_uint16));
      }

      // XGC(GR line) SRAM memory access disable
      skhynix_write_cmos_sensor(0xffff, 0x0000); // XGC(Gr line) disable
      skhynix_write_cmos_sensor(0x0b32, 0xAAAA); //F/W run and qbgc data copy
      LOG_INF("[End]:XGC Gr Line End\n");

      //*********Burst write apply XGC GR line end**************//


     /////////// QBGC_calibration_write//////////////
     LOG_INF("[Start]:QGC  start\n");

     TEMP1= skhynix_read_cmos_sensor(0x0318);
     TEMP1=(TEMP1<<8)+(skhynix_read_cmos_sensor(0x0319));
     LOG_INF("QGC_TEPM1 = %d", TEMP1);

     skhynix_write_cmos_sensor(0x0318, TEMP1 & 0xFFF7); // QBGC data loading disable B[3]
     QGC_Flag=((skhynix_read_cmos_sensor(0x0318) << 8) | skhynix_read_cmos_sensor(0x0319));
     LOG_INF("QGC_Flag = %d", QGC_Flag);

     //*********Burst write apply QBGC  start**************//

	skhynix_write_cmos_sensor(0xffff, 0x0E40); // QBGC enable
	hi5021_qbgc_addr = 0x0000;
	for(i = 0; i < 1280;i+=2)
	{
	   hi5021_qbgc_data = ((((QGC_CalData[i+1]) & (0x00ff))<<8) +((QGC_CalData[i]) & (0x00ff)));
	   QBGC_setting_burst[2*length_qbgc]=hi5021_qbgc_addr;
	   QBGC_setting_burst[2*length_qbgc+1]=hi5021_qbgc_data;
	   length_qbgc++;
	   hi5021_qbgc_addr += 2;
	}

	//Burst write QBGC  //
	if(length_qbgc == (1280/2))
	{
	   //hi5022qrearqt_table_write_cmos_sensor(
	   //QBGC_setting_burst,sizeof(QBGC_setting_burst)/sizeof(kal_uint16));
	   skhynix_write_burst_mode(QBGC_setting_burst,
	        sizeof(QBGC_setting_burst)/sizeof(kal_uint16));
	}

	// QBGC SRAM memory access disable
	skhynix_write_cmos_sensor(0xffff, 0x0000); // QBGC disable
	skhynix_write_cmos_sensor(0x0b32, 0xAAAA); //F/W run and qbgc data copy
	LOG_INF("[End]:QBGC Line End\n");

	//*********Burst write apply QBGC end**************//
	LOG_INF("[End]:QBGC End\n");
     
}

void hi5022q_init_sensor_cali_info(void) {
	kal_uint16 moduleId = hi5022q_read_eeprom(0x0001);
	kal_uint16 lensId = hi5022q_read_eeprom(0x0003);
	LOG_INF("read module id 0x%x ,lensId is 0x%x\n",moduleId,lensId);
	if((w2hi5022qreartxd_eeprom_data.sensorVendorid >> 24) != moduleId) {
		LOG_INF("module id is not txd\n");
	} else {
	  if( 0X01 == lensId){
		  get_eeprom_0x003_value = 1;
		  LOG_INF("old hi5022 chip HI5022_FGLAG: %d \n", get_eeprom_0x003_value);
	  } else if( 0X02 == lensId){
		  get_eeprom_0x003_value = 2;
		  LOG_INF("new hi5022 chip HI5022_FGLAG: %d \n", get_eeprom_0x003_value);
	  }

	}
	if (NULL == w2hi5022qreartxd_eeprom_data.dataBuffer)
		w2hi5022qreartxd_eeprom_data.dataBuffer = hi5022q_read_eeprom_onboot(w2hi5022qreartxd_eeprom_data.dataLength + OTP_MAP_VENDOR_HEAD_ID_BYTES);
	if (eeprom_do_checksum(w2hi5022qreartxd_eeprom_data.dataBuffer, w2hi5022qreartxd_Checksum) == 0)
		imgsensor_set_eeprom_data_bywing(&w2hi5022qreartxd_eeprom_data, w2hi5022qreartxd_Checksum);
    if((w2hi5022qreartxd_eeprom_data.sensorVendorid >> 24) != moduleId) {
		LOG_INF("get eeprom data failed\n");
		if(w2hi5022qreartxd_eeprom_data.dataBuffer != NULL) {
			kfree(w2hi5022qreartxd_eeprom_data.dataBuffer);
			w2hi5022qreartxd_eeprom_data.dataBuffer = NULL;
		}
	} else {
		LOG_INF("get eeprom data success\n");
	}
	
	if (w2hi5022qreartxd_eeprom_data.dataBuffer != NULL) {
		memcpy(XGCbuffer, (kal_uint8*)&w2hi5022qreartxd_eeprom_data.dataBuffer[hi5022q_OTP_XGC_OFFSET + 1 + OTP_MAP_VENDOR_HEAD_ID_BYTES], hi5022q_OTP_XGC_LEN);
		memcpy(QGCbuffer, (kal_uint8*)&w2hi5022qreartxd_eeprom_data.dataBuffer[hi5022q_OTP_QGC_OFFSET + 1 + OTP_MAP_VENDOR_HEAD_ID_BYTES], hi5022q_OTP_QGC_LEN);
		memcpy(OPCBuffer, (kal_uint8*)&w2hi5022qreartxd_eeprom_data.dataBuffer[hi5022q_OTP_OPC_OFFSET + 1 + OTP_MAP_VENDOR_HEAD_ID_BYTES], HI5022Q_OTP_OPC_LEN);
		xgc_init_flag = true;
		LOG_INF("xgc eeprom data read success\n");
	}

	//LOG_INF("hi5022q_eeprom:pgc_addr:0x%x\n",w2hi5022qreartxd_Checksum[hi5022q_PGC- 1].startAdress + 3);
	LOG_INF("hi5022q_eeprom:qgc_addr:0x%x\n",w2hi5022qreartxd_Checksum[hi5022q_QGC - 1].startAdress + 3);
	LOG_INF("hi5022q_eeprom:xgc_addr:0x%x\n",w2hi5022qreartxd_Checksum[hi5022q_XGC - 1].startAdress + 3);

	LOG_INF("=====================hi5022q dump pgceeprom data start====================\n");
	//dumpEEPROMData(PGC_DATA_SIZE,pgc_data_buffer);
	LOG_INF("=====================hi5022q dump pgceeprom data end======================\n");

	LOG_INF("=====================hi5022q dump qgceeprom data start====================\n");
	//dumpEEPROMData(QGC_DATA_SIZE,qgc_data_buffer);
	LOG_INF("=====================hi5022q dump qgceeprom data end======================\n");

	LOG_INF("=====================hi5022q dump xgceeprom data start====================\n");
	//dumpEEPROMData(XGC_DATA_SIZE,xgc_data_buffer);
	LOG_INF("=====================hi5022q dump xgceeprom data end======================\n");
}

//+bug767771, liudijin.wt, modify, 2022/08/11, hi5022 remosaic bringup.
void read_4cell_from_eeprom(char *data, kal_uint16 datasize)
{
    if(datasize == (HI5022Q_OTP_OPC_LEN+2)){
        kal_uint16 dataLen = HI5022Q_OTP_OPC_LEN;
        kal_uint8 size[2] = {0};
        size[0] = (kal_uint8)(dataLen & 0xFF);
        size[1] = (kal_uint8)(dataLen >> 8);
        memcpy(data, size, 2);
        memcpy(&data[2], OPCBuffer, HI5022Q_OTP_OPC_LEN);
    }else if(datasize == hi5022q_OTP_XGC_LEN){
        memcpy(data, XGCbuffer, datasize);
    }
	LOG_INF("read_4cell_from_eeprom datasize = %d\n", datasize);
}
//-bug767771, liudijin.wt, modify, 2022/08/11, hi5022 remosaic bringup.

