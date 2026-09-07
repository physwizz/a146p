#ifndef __N28_HI5021Q_REAR_DC_MIPIRAW_OTP__
#define __N28_HI5021Q_REAR_DC_MIPIRAW_OTP__

#define hi5021q_XGC_QGC_PGC_CALIB 1
#define HI5021Q_ISP_CALIBRATION_OTP_DUMP 0

#define hi5021Q_OTP_XGC_OFFSET   0x0D5E
#define hi5021Q_OTP_QGC_OFFSET   0x14E0
#define hi5021Q_OTP_PGC_OFFSET   0x19E2

#define hi5021Q_OTP_XGC_CHECKSUM   0x14DF
#define hi5021Q_OTP_QGC_CHECKSUM  0x19E1
#define hi5021Q_OTP_PGC_CHECKSUM   0x2147

#define hi5021Q_OTP_XGC_LEN   1920
#define hi5021Q_OTP_QGC_LEN   1280
#define hi5021Q_OTP_PGC_LEN   1892

#define XGC_GB_DATA_SIZE 960
#define XGC_GR_DATA_SIZE 960

#define QBGC_DATA_SIZE 1280

#define PGC_GB_DATA_SIZE 946
#define PGC_GR_DATA_SIZE 946

extern void Dc_apply_sensor_Cali(void);
extern bool imgsensor_set_eeprom_data_bywing(struct stCAM_CAL_DATAINFO_STRUCT* pData, struct stCAM_CAL_CHECKSUM_STRUCT* checksumData);
extern int eeprom_do_checksum(u8* kbuffer, struct stCAM_CAL_CHECKSUM_STRUCT* cData);
extern void skhynix_write_cmos_sensor(kal_uint32 addr, kal_uint32 para);
extern kal_uint16 skhynix_read_cmos_sensor(kal_uint32 addr);
extern bool skhynix_write_burst_mode(kal_uint16 setting_table[], kal_uint32 nSetSize);
extern kal_uint16 hi5021q_read_eeprom(kal_uint32 addr);
extern struct stCAM_CAL_DATAINFO_STRUCT n28hi5021qreardc_eeprom_data;
extern struct stCAM_CAL_CHECKSUM_STRUCT n28hi5021qreardc_Checksum[12];
extern void hi5021q_init_sensor_cali_info(void);
#endif