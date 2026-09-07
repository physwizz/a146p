#ifndef __W2_S5KJN1RST_MIPIRAW_OTP__
#define __W2_S5KJN1RST_MIPIRAW_OTP__

#define MODULE_ID_OFFSET 0x0000
#define I2C_BUFFER_LEN 225    /* trans# max is 255, each 3 bytes */
#define W2_TXD_S5KJN1_OTP 1
#define USE_BURST_MODE 1
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
#define S5KJN1_OTP_XTC_TOTAL_LEN   9490
#define EEPROM_BL24SA64D_ID 0xA0

#include "kd_camera_typedef.h"
#include "kd_imgsensor.h"
#include "kd_imgsensor_define.h"
#include "kd_imgsensor_errcode.h"

extern int eeprom_do_checksum(u8* kbuffer, struct stCAM_CAL_CHECKSUM_STRUCT* cData);
extern kal_uint16 read_cmos_eeprom_8(kal_uint16 addr);
void read_eepromData_HW_GGC(void);
void write_sensor_HW_GGC(void);
extern bool imgsensor_set_eeprom_data_bywing(struct stCAM_CAL_DATAINFO_STRUCT* pData, struct stCAM_CAL_CHECKSUM_STRUCT* checksumData);
extern void s5krst_write_cmos_sensor(kal_uint16 addr, kal_uint16 para);
int s5kjn1_init_sensor_cali_info(void);
extern kal_uint16 s5kst_table_write_cmos_sensor(kal_uint16 *para, kal_uint32 len);
extern void s5krst_read_4cell_from_eeprom(char *data, kal_uint16 datasize);
#endif