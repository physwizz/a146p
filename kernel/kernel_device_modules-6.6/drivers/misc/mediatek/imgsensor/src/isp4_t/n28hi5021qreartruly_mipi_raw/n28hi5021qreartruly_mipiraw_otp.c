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

#include "n28hi5021qreartruly_mipiraw_Sensor.h"
#include "n28hi5021qreartruly_mipiraw_otp.h"

#define PFX "n28hi5021qreartruly_camera_otp"
#define LOG_INF(format, args...)    \
     pr_info(PFX "[%s] " format, __func__, ##args)

#define EEPROM_BL24SA64D_ID 0xA0

static kal_uint8 Truly_XGCbuffer[hi5021Q_OTP_XGC_LEN] = {0};
static kal_uint8 Truly_QGCbuffer[hi5021Q_OTP_QGC_LEN] = {0};
static kal_uint8 Truly_PGCbuffer[hi5021Q_OTP_PGC_LEN] = {0};

static kal_uint16 Truly_XGC_GB_setting_burst[XGC_GB_DATA_SIZE];
static kal_uint16 Truly_XGC_GR_setting_burst[XGC_GR_DATA_SIZE];
static kal_uint16 Truly_QBGC_setting_burst[QBGC_DATA_SIZE];
static kal_uint16 Truly_PGC_GB_setting_burst[PGC_GB_DATA_SIZE];
static kal_uint16 Truly_PGC_GR_setting_burst[PGC_GR_DATA_SIZE];

struct stCAM_CAL_DATAINFO_STRUCT n28hi5021qreartruly_eeprom_data ={
     .sensorID= N28HI5021QREARTRULY_SENSOR_ID,
     .deviceID = 0x01,
     .dataLength = 0x2149,
     .sensorVendorid = 0x06000101,
     .vendorByte = {1,2,3,4},
     .dataBuffer = NULL,
};

struct stCAM_CAL_CHECKSUM_STRUCT n28hi5021qreartruly_Checksum[12] =
{
      {MODULE_ITEM, 0x0000 + OTP_MAP_VENDOR_HEAD_ID_BYTES, 0x0000 + OTP_MAP_VENDOR_HEAD_ID_BYTES, 0x0007 + OTP_MAP_VENDOR_HEAD_ID_BYTES, 0x0008 + OTP_MAP_VENDOR_HEAD_ID_BYTES, 0x55},
      {SN_DATA, 0x0009 + OTP_MAP_VENDOR_HEAD_ID_BYTES, 0x0009 + OTP_MAP_VENDOR_HEAD_ID_BYTES, 0x0015 + OTP_MAP_VENDOR_HEAD_ID_BYTES, 0x0016 + OTP_MAP_VENDOR_HEAD_ID_BYTES, 0x55},
      {AWB_ITEM, 0x0017 + OTP_MAP_VENDOR_HEAD_ID_BYTES, 0x0017 + OTP_MAP_VENDOR_HEAD_ID_BYTES, 0x0027 + OTP_MAP_VENDOR_HEAD_ID_BYTES, 0x0028 + OTP_MAP_VENDOR_HEAD_ID_BYTES, 0x55},
      {AF_ITEM, 0x0029 + OTP_MAP_VENDOR_HEAD_ID_BYTES, 0x0029 + OTP_MAP_VENDOR_HEAD_ID_BYTES, 0x002E + OTP_MAP_VENDOR_HEAD_ID_BYTES, 0x002F + OTP_MAP_VENDOR_HEAD_ID_BYTES, 0x55},
      {LSC_ITEM, 0x0030 + OTP_MAP_VENDOR_HEAD_ID_BYTES, 0x0030 + OTP_MAP_VENDOR_HEAD_ID_BYTES, 0x077C + OTP_MAP_VENDOR_HEAD_ID_BYTES, 0x077D + OTP_MAP_VENDOR_HEAD_ID_BYTES, 0x55},
      {PDAF_ITEM, 0x077E + OTP_MAP_VENDOR_HEAD_ID_BYTES, 0x077E + OTP_MAP_VENDOR_HEAD_ID_BYTES, 0x096E + OTP_MAP_VENDOR_HEAD_ID_BYTES, 0x096F + OTP_MAP_VENDOR_HEAD_ID_BYTES, 0x55},
      {PDAF_PROC2_ITEM, 0x0970 + OTP_MAP_VENDOR_HEAD_ID_BYTES, 0x0970 + OTP_MAP_VENDOR_HEAD_ID_BYTES, 0x0D5C + OTP_MAP_VENDOR_HEAD_ID_BYTES, 0x0D5D + OTP_MAP_VENDOR_HEAD_ID_BYTES, 0x55},
      {hi5021q_XGC, 0x0D5E + OTP_MAP_VENDOR_HEAD_ID_BYTES, 0x0D5E + OTP_MAP_VENDOR_HEAD_ID_BYTES, 0x14DE + OTP_MAP_VENDOR_HEAD_ID_BYTES, 0x14DF + OTP_MAP_VENDOR_HEAD_ID_BYTES, 0x55},
      {hi5021q_QGC, 0x14E0 + OTP_MAP_VENDOR_HEAD_ID_BYTES, 0x14E0 + OTP_MAP_VENDOR_HEAD_ID_BYTES, 0x19E0 + OTP_MAP_VENDOR_HEAD_ID_BYTES, 0x19E1 + OTP_MAP_VENDOR_HEAD_ID_BYTES, 0x55},
      {hi5021q_PGC, 0x19E2 + OTP_MAP_VENDOR_HEAD_ID_BYTES, 0x19E2 + OTP_MAP_VENDOR_HEAD_ID_BYTES, 0x2146 + OTP_MAP_VENDOR_HEAD_ID_BYTES, 0x2147 + OTP_MAP_VENDOR_HEAD_ID_BYTES, 0x55},
      {TOTAL_ITEM, 0x0000 + OTP_MAP_VENDOR_HEAD_ID_BYTES, 0x0000 + OTP_MAP_VENDOR_HEAD_ID_BYTES, 0x2147 + OTP_MAP_VENDOR_HEAD_ID_BYTES, 0x2148 + OTP_MAP_VENDOR_HEAD_ID_BYTES, 0x55},
      {MAX_ITEM, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0x55},  // this line must haved
};

static u8* hi5021q_read_eeprom_onboot(int size) {
    int i = 0;
    u8* ebuf = NULL;
    ebuf = kmalloc(size, GFP_KERNEL);
    memcpy(ebuf, (void *)&(n28hi5021qreartruly_eeprom_data.sensorVendorid), OTP_MAP_VENDOR_HEAD_ID_BYTES);
    for (i = OTP_MAP_VENDOR_HEAD_ID_BYTES; i < size; i++) {
        ebuf[i] = hi5021q_read_eeprom(i - OTP_MAP_VENDOR_HEAD_ID_BYTES);
    }
    return ebuf;
}

void Truly_apply_sensor_Cali(void)
{
     kal_uint8 *XGC_CalData = (kal_uint8*)Truly_XGCbuffer;
     kal_uint8 *QGC_CalData = (kal_uint8*)Truly_QGCbuffer;
     kal_uint8 *PGC_CalData = (kal_uint8*)Truly_PGCbuffer;

     kal_uint16 length_xgc_gb = 0;
     kal_uint16 length_xgc_gr = 0;

     kal_uint16 length_qbgc = 0;

     kal_uint16 length_pgc_gb = 0;
     kal_uint16 length_pgc_gr = 0;

     kal_uint16 hi5021_xgc_gb_addr;
     kal_uint16 hi5021_xgc_gb_data;

     kal_uint16 hi5021_xgc_gr_addr;
     kal_uint16 hi5021_xgc_gr_data;

     kal_uint16 hi5021_qbgc_addr;
     kal_uint16 hi5021_qbgc_data;

     kal_uint16 hi5021_pgc_gb_addr;
     kal_uint16 hi5021_pgc_gb_data;

     kal_uint16 hi5021_pgc_gr_addr;
     kal_uint16 hi5021_pgc_gr_data;

     kal_uint16 TEMP1;
     kal_uint16 XGC_Flag;
     kal_uint16 QGC_Flag;
     kal_uint16 PGC_Flag;
     int i = 0;
#if HI5021Q_ISP_CALIBRATION_OTP_DUMP
     int n = 0;
#endif
     //////////// XGC_calibration_write////////////
     LOG_INF("[Hynix]:New_setting_new_SK_burst_v1.4 End\n");
     //LOG_INF("[Start]:apply_sensor_Cali start\n");

     /***********STEP 1: Streamoff**************/
     skhynix_write_cmos_sensor(0x0b00,0x0000);
     TEMP1= skhynix_read_cmos_sensor(0x0318);
     TEMP1=(TEMP1<<8)+(skhynix_read_cmos_sensor(0x0319));
     LOG_INF("XGC_TEPM1 = %d", TEMP1);
     skhynix_write_cmos_sensor(0x0318, TEMP1 & 0xFFEF); // XGC data loading disable B[4]
     XGC_Flag=((skhynix_read_cmos_sensor(0x0318) << 8) | skhynix_read_cmos_sensor(0x0319));
     LOG_INF("XGC_Flag = %d", XGC_Flag);
     // XGC(Gb line) SRAM memory access enable
//*********Burst write apply XGC Gb line start**************//
     {
          skhynix_write_cmos_sensor(0xffff, 0x0640); //// XGC(Gb line)  enable
          hi5021_xgc_gb_addr = 0x0000;
          for(i = 0; i < 960;i+=2)
          { 
		      hi5021_xgc_gb_data = ((((XGC_CalData[i+1]) & (0x00ff))<<8) +((XGC_CalData[i]) & (0x00ff)));
#if HI5021Q_ISP_CALIBRATION_OTP_DUMP
          LOG_INF("hi5021_xgc_gb_addr:0x%x,XGC_CalData_Low[%d]:0x%x,XGC_CalData_High[%d]:0x%x,hi5021_xgc_gb_data:0x%x\n", hi5021_xgc_gb_addr,i,XGC_CalData[i],i+1,XGC_CalData[i+1],hi5021_xgc_gb_data);
#endif
              Truly_XGC_GB_setting_burst[2*length_xgc_gb] = hi5021_xgc_gb_addr;
              Truly_XGC_GB_setting_burst[2*length_xgc_gb+1] = hi5021_xgc_gb_data;
              length_xgc_gb++;
              hi5021_xgc_gb_addr += 2;
              //write_cmos_sensor(sensor_bpgc_addr,hi5021_xgc_gb_data);
          }

//Burst write XGC_Gb line //
          if(length_xgc_gb == (960/2))
          {
               skhynix_write_burst_mode(Truly_XGC_GB_setting_burst,
                    sizeof(Truly_XGC_GB_setting_burst)/sizeof(kal_uint16));
          }
#if HI5021Q_ISP_CALIBRATION_OTP_DUMP
          pr_info("================hi5021q XGC_GB burst write data.================\n");
          for(n=0; n<960; n=n+2)
          {
               pr_info("hi5021q Xgc_gb addr:0x%x,data:0x%x", Truly_XGC_GB_setting_burst[n], Truly_XGC_GB_setting_burst[n+1]);
               if(n%10 == 9)
               {
                    pr_info("\n");
               }
          }
#endif
          // XGC(Gb line) SRAM memory access disable
          skhynix_write_cmos_sensor(0xffff, 0x0000); // XGC(Gr line) disable
          LOG_INF("[End]:XGC Gb Line End\n");

          //*********Burst write apply XGC Gb line end**************//
     }
//***************************************************************************************************************//

//*********Burst write apply XGC GR line start**************//
     {
          skhynix_write_cmos_sensor(0xffff, 0x0740); // XGC(Gr line) enable
          hi5021_xgc_gr_addr = 0x0000;
          for(i = 960; i < 1920;i+=2)
          {
               hi5021_xgc_gr_data = ((((XGC_CalData[i+1]) & (0x00ff))<<8) +((XGC_CalData[i]) & (0x00ff)));
#if HI5021Q_ISP_CALIBRATION_OTP_DUMP
            LOG_INF("hi5021_xgc_gr_addr:0x%x,XGC_GR_CalData_Low[%d]:0x%x,XGC_GR_CalData_High[%d]:0x%x,hi5021_xgc_gr_data:0x%x\n", hi5021_xgc_gr_addr,i,XGC_CalData[i],i+1,XGC_CalData[i+1],hi5021_xgc_gr_data);
#endif
               Truly_XGC_GR_setting_burst[2*length_xgc_gr] = hi5021_xgc_gr_addr;
               Truly_XGC_GR_setting_burst[2*length_xgc_gr+1] = hi5021_xgc_gr_data;
               length_xgc_gr++;
               hi5021_xgc_gr_addr += 2;
          }
          //Burst write XGC_Gr line //
          if(length_xgc_gr == (960/2))
          {
               skhynix_write_burst_mode(Truly_XGC_GR_setting_burst,
                    sizeof(Truly_XGC_GR_setting_burst)/sizeof(kal_uint16));
          }
#if HI5021Q_ISP_CALIBRATION_OTP_DUMP
          pr_info("================hi5021q XGC_GR burst write data.================\n");
          for(n=0; n<960; n=n+2)
          {
               pr_info("hi5021q Xgc_gr addr:0x%x,data:0x%x", Truly_XGC_GR_setting_burst[n], Truly_XGC_GR_setting_burst[n+1]);
               if(n%10 == 9)
               {
                    pr_info("\n");
               }
          }
#endif
          // XGC(GR line) SRAM memory access disable
          skhynix_write_cmos_sensor(0xffff, 0x0000); // XGC(Gr line) disable
          skhynix_write_cmos_sensor(0x0b32, 0xAAAA); //F/W run and qbgc data copy
          LOG_INF("[End]:XGC Gr Line End\n");

          //*********Burst write apply XGC GR line end**************//
     }

     /////////// QBGC_calibration_write//////////////
     LOG_INF("[Start]:QGC  start\n");

     TEMP1= skhynix_read_cmos_sensor(0x0318);
     TEMP1=(TEMP1<<8)+(skhynix_read_cmos_sensor(0x0319));
     LOG_INF("QGC_TEPM1 = %d", TEMP1);

     skhynix_write_cmos_sensor(0x0318, TEMP1 & 0xFFF7); // QBGC data loading disable B[3]
     QGC_Flag=((skhynix_read_cmos_sensor(0x0318) << 8) | skhynix_read_cmos_sensor(0x0319));
     LOG_INF("QGC_Flag = %d", QGC_Flag);

     //*********Burst write apply QBGC  start**************//
     {
          skhynix_write_cmos_sensor(0xffff, 0x0A40); // QBGC enable
          hi5021_qbgc_addr = 0x0000;
          for(i = 0; i < 1280;i+=2)
          {
              hi5021_qbgc_data = ((((QGC_CalData[i+1]) & (0x00ff))<<8) +((QGC_CalData[i]) & (0x00ff)));
#if HI5021Q_ISP_CALIBRATION_OTP_DUMP
              LOG_INF("hi5021_qbgc_addr:0x%x,QGC_CalData_Low[%d]:0x%x,QGC_CalData_High[%d]:0x%x,hi5021_qbgc_data:0x%x\n", hi5021_qbgc_addr,i,QGC_CalData[i],i+1,QGC_CalData[i+1],hi5021_qbgc_data);
#endif
              Truly_QBGC_setting_burst[2*length_qbgc] = hi5021_qbgc_addr;
              Truly_QBGC_setting_burst[2*length_qbgc+1] = hi5021_qbgc_data;
              length_qbgc++;
              hi5021_qbgc_addr += 2;
          }
          //Burst write QBGC  //
          if(length_qbgc == (1280/2))
          {
               skhynix_write_burst_mode(Truly_QBGC_setting_burst,
                    sizeof(Truly_QBGC_setting_burst)/sizeof(kal_uint16));
          }
#if HI5021Q_ISP_CALIBRATION_OTP_DUMP
          pr_info("================hi5021q QBGC burst write data.================\n");
          for(n=0; n<1280; n=n+2)
          {
               pr_info("hi5021q QBGC addr:0x%x,data:0x%x", Truly_QBGC_setting_burst[n], Truly_QBGC_setting_burst[n+1]);
               if(n%10 == 9)
               {
                    pr_info("\n");
               }
          }
#endif
          // QBGC SRAM memory access disable
          skhynix_write_cmos_sensor(0xffff, 0x0000); // QBGC disable
          skhynix_write_cmos_sensor(0x0b32, 0xAAAA); //F/W run and qbgc data copy
          LOG_INF("[End]:QBGC Line End\n");

          //*********Burst write apply QBGC end**************//
     }
     LOG_INF("[End]:QBGC End\n");

     ///////////// PGC_calibration_write///////
     TEMP1= skhynix_read_cmos_sensor(0x0318);
     TEMP1=(TEMP1<<8)+(skhynix_read_cmos_sensor(0x0319));
     LOG_INF("PGC_TEPM1 = %d", TEMP1);
     skhynix_write_cmos_sensor(0x0318, TEMP1 & 0xFFDF); // PGC data loading disable B[5]
     PGC_Flag=((skhynix_read_cmos_sensor(0x0318) << 8) | skhynix_read_cmos_sensor(0x0319));
     LOG_INF("PGC_Flag = %d", PGC_Flag);
     skhynix_write_cmos_sensor(0xffff, 0x0B40); // PGC(Gb line) enable
     //*********Burst write apply PGC Gb line start**************//
     {
          hi5021_pgc_gb_addr = 0x0000;
          for(i = 0; i < 946;i+=2)
          {
               hi5021_pgc_gb_data = ((((PGC_CalData[i+1]) & (0x00ff))<<8) +((PGC_CalData[i]) & (0x00ff)));
#if HI5021Q_ISP_CALIBRATION_OTP_DUMP
          LOG_INF("hi5021_pgc_gb_addr:0x%x,PGC_CalData_Low[%d]:0x%x,PGC_CalData_High[%d]:0x%x,hi5021_pgc_gb_data:0x%x\n", hi5021_pgc_gb_addr,i,PGC_CalData[i],i+1,PGC_CalData[i+1],hi5021_pgc_gb_data);
#endif
               Truly_PGC_GB_setting_burst[2*length_pgc_gb] = hi5021_pgc_gb_addr;
               Truly_PGC_GB_setting_burst[2*length_pgc_gb+1] = hi5021_pgc_gb_data;
               length_pgc_gb++;
               hi5021_pgc_gb_addr += 2;
               //write_cmos_sensor(sensor_bpgc_addr,hi5021_pgc_gb_data);
          }
          //Burst write PGC_Gb line //
          if(length_pgc_gb == (946/2))
          {
               skhynix_write_burst_mode(Truly_PGC_GB_setting_burst,
                    sizeof(Truly_PGC_GB_setting_burst)/sizeof(kal_uint16));
          }
#if HI5021Q_ISP_CALIBRATION_OTP_DUMP
          pr_info("================hi5021q PGC_GB burst write    data.================\n");
          for(n=0; n<946; n=n+2)
          {
               pr_info("hi5021q pgc_gb addr:0x%x,data:0x%x", Truly_PGC_GB_setting_burst[n], Truly_PGC_GB_setting_burst[n+1]);
               if(n%10 == 9)
               {
                    pr_info("\n");
               }
          }
#endif
          // PGC(Gb line) SRAM memory access disable
          skhynix_write_cmos_sensor(0xffff, 0x0000); // PGC(Gr line) disable
          LOG_INF("[End]:PGC Gb Line End\n");

          //*********Burst write apply PGC Gb line end**************//
     }

     //*********Burst write apply PGC GR line start**************//
     {
          skhynix_write_cmos_sensor(0xffff, 0x0C40); // PGC(Gr line) enable
          hi5021_pgc_gr_addr = 0x0000;
          for(i = 946; i < 1892;i+=2)
          {
               hi5021_pgc_gr_data = ((((PGC_CalData[i+1]) & (0x00ff))<<8) +((PGC_CalData[i]) & (0x00ff)));
#if HI5021Q_ISP_CALIBRATION_OTP_DUMP
          LOG_INF("hi5021_pgc_gr_addr:0x%x,PGC_GR_CalData_Low[%d]:0x%x,PGC_GR_CalData_High[%d]:0x%x,hi5021_pgc_gr_data:0x%x\n", hi5021_pgc_gr_addr,i,PGC_CalData[i],i+1,PGC_CalData[i+1],hi5021_pgc_gr_data);
#endif
               Truly_PGC_GR_setting_burst[2*length_pgc_gr] = hi5021_pgc_gr_addr;
               Truly_PGC_GR_setting_burst[2*length_pgc_gr+1] = hi5021_pgc_gr_data;
               length_pgc_gr++;
               hi5021_pgc_gr_addr += 2;
          }
          //Burst write PGC_Gr line //
          if(length_pgc_gr == (946/2))
          {
               skhynix_write_burst_mode(Truly_PGC_GR_setting_burst,
                    sizeof(Truly_PGC_GR_setting_burst)/sizeof(kal_uint16));
          }
#if HI5021Q_ISP_CALIBRATION_OTP_DUMP
          pr_info("================hi5021q PGC_GR burst write data.================\n");
          for(n=0; n<946; n=n+2)
          {
               pr_info("hi5021q pgc_gr addr:0x%x,data:0x%x ", Truly_PGC_GR_setting_burst[n], Truly_PGC_GR_setting_burst[n+1]);
               if(n%10 == 9)
               {
                    pr_info("\n");
               }
          }
#endif
          // PGC(GR line) SRAM memory access disable
          skhynix_write_cmos_sensor(0xffff, 0x0000); // PGC(Gr line) disable
          LOG_INF("[End]:PGC Gr Line End\n");
          //*********Burst write apply PGC GR line end**************//
     }
}

void hi5021qtruly_init_sensor_cali_info(void) {
     if (NULL == n28hi5021qreartruly_eeprom_data.dataBuffer) {
          n28hi5021qreartruly_eeprom_data.dataBuffer = hi5021q_read_eeprom_onboot(n28hi5021qreartruly_eeprom_data.dataLength + OTP_MAP_VENDOR_HEAD_ID_BYTES);
     }
     if (eeprom_do_checksum(n28hi5021qreartruly_eeprom_data.dataBuffer, n28hi5021qreartruly_Checksum) == 0) {
          imgsensor_set_eeprom_data_bywing(&n28hi5021qreartruly_eeprom_data,n28hi5021qreartruly_Checksum);
     }
     pr_info("get eeprom data success\n");

     #if hi5021q_XGC_QGC_PGC_CALIB
     //get the pgc qgc xgc buffer
     memcpy(Truly_XGCbuffer, (kal_uint8*)&n28hi5021qreartruly_eeprom_data.dataBuffer[hi5021Q_OTP_XGC_OFFSET+1+OTP_MAP_VENDOR_HEAD_ID_BYTES], hi5021Q_OTP_XGC_LEN);
     memcpy(Truly_QGCbuffer, (kal_uint8*)&n28hi5021qreartruly_eeprom_data.dataBuffer[hi5021Q_OTP_QGC_OFFSET+1+OTP_MAP_VENDOR_HEAD_ID_BYTES], hi5021Q_OTP_QGC_LEN);
     memcpy(Truly_PGCbuffer, (kal_uint8*)&n28hi5021qreartruly_eeprom_data.dataBuffer[hi5021Q_OTP_PGC_OFFSET+1+OTP_MAP_VENDOR_HEAD_ID_BYTES], hi5021Q_OTP_PGC_LEN);
     #endif //hi5021q_XGC_QGC_PGC_CALIB
}