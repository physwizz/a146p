#ifndef __W2_HI5022Q_REAR_TXD_MIPIRAW_OTP__
#define __W2_HI5022Q_REAR_TXD_MIPIRAW_OTP__

#define hi5022q_OTP_XGC_OFFSET   0x0D5E
#define hi5022q_OTP_QGC_OFFSET   0x14E0
#define hi5022q_OTP_OPC_OFFSET   0x19E2


#define hi5022q_OTP_XGC_CHECKSUM   0x14DF
#define hi5022q_OTP_QGC_CHECKSUM  0x19E1


#define hi5022q_OTP_XGC_LEN   1920
#define hi5022q_OTP_QGC_LEN   1280

#define XGC_GB_DATA_SIZE 960
#define XGC_GR_DATA_SIZE 960

#define QBGC_DATA_SIZE 1280

#define HI5022Q_OTP_OPC_LEN   2048
extern bool imgsensor_set_eeprom_data_bywing(struct stCAM_CAL_DATAINFO_STRUCT* pData, struct stCAM_CAL_CHECKSUM_STRUCT* checksumData);
extern int eeprom_do_checksum(u8* kbuffer, struct stCAM_CAL_CHECKSUM_STRUCT* cData);
extern bool skhynix_write_burst_mode(kal_uint16 setting_table[], kal_uint32 nSetSize);
extern void hi5022q_apply_sensor_Cali(void);
extern void read_4cell_from_eeprom(char *data, kal_uint16 datasize);
extern void skhynix_write_cmos_sensor(kal_uint32 addr, kal_uint32 para);
extern kal_uint16 skhynix_read_cmos_sensor(kal_uint32 addr);
extern kal_uint16 get_eeprom_0x003_value;
extern struct stCAM_CAL_DATAINFO_STRUCT w2hi5022qreartxd_eeprom_data;
extern struct stCAM_CAL_CHECKSUM_STRUCT w2hi5022qreartxd_Checksum[12];
extern void hi5022q_init_sensor_cali_info(void);
#endif
