#ifndef __N28_S5KJN1_REAR_TRULY_MIPIRAW_OTP__
#define __N28_S5KJN1_REAR_TRULY_MIPIRAW_OTP__

#define N28_TRULY_S5KJN1_OTP 1
#define JN1_HW_GGC_SIZE 173
#define JN1_HW_GGC_START_ADDR 0x3027

#define TRULY_S5KJN1_OTP_XTC_OFFSET   0x0D5E
#define TRULY_S5KJN1_OTP_SENSORXTC_OFFSET   0x1B0E
#define TRULY_S5KJN1_OTP_PDXTC_OFFSET   0x1E10
#define TRULY_S5KJN1_OTP_SWGCC_OFFSET   0x2DB2

#define S5KJN1_OTP_XTC_LEN   3502
#define S5KJN1_OTP_SENSORXTC_LEN   768
#define S5KJN1_OTP_PDXTC_LEN   4000
#define S5KJN1_OTP_SWGCC_LEN   626
#define S5KJN1_OTP_HWGCC_LEN   346
#define S5KJN1_OTP_XTC_TOTAL_LEN   9490
#define EEPROM_GT9778_ID 0xA0
extern struct stCAM_CAL_DATAINFO_STRUCT n28s5kjn1reartruly_eeprom_data;
extern struct stCAM_CAL_CHECKSUM_STRUCT n28s5kjn1reartruly_Checksum[14];
extern void s5k_write_cmos_sensor(kal_uint16 addr, kal_uint16 para);
extern void s5k_write_cmos_sensor_8(kal_uint16 addr, kal_uint8 para);
extern kal_uint16 s5k_read_cmos_eeprom_8(kal_uint16 addr);
extern kal_uint16 s5k_table_write_cmos_sensor(kal_uint16 *para, kal_uint32 len);
extern void truly_write_sensor_HW_GGC(void);
extern void read_4cell_from_eeprom(char *data, kal_uint16 datasize);
extern kal_uint16 n28s5kjn1reartruly_read_eeprom(kal_uint32 addr);
extern int eeprom_do_checksum(u8* kbuffer, struct stCAM_CAL_CHECKSUM_STRUCT* cData);
extern u8* n28s5kjn1reartruly_read_eeprom_onboot(int size);
extern bool imgsensor_set_eeprom_data_bywing(struct stCAM_CAL_DATAINFO_STRUCT* pData, struct stCAM_CAL_CHECKSUM_STRUCT* checkData);
extern void s5kjn1truly_init_sensor_cali_info(void);
#endif