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
#include "n28s5kjn1reartruly_mipi_raw_Sensor.h"
#include "n28s5kjn1reartruly_mipi_raw_otp.h"

#define PFX "n28s5kjn1reartruly_mipi_raw_otp"
#define LOG_INF(format, args...)    \
     pr_info(PFX "[%s] " format, __func__, ##args)
static kal_uint8 XTCBuffer[S5KJN1_OTP_XTC_LEN] = {0};
static kal_uint8 SENSORXTCBuffer[S5KJN1_OTP_SENSORXTC_LEN] = {0};
static kal_uint8 PDXTCXTCBuffer[S5KJN1_OTP_PDXTC_LEN] = {0};
static kal_uint8 SWGCCBuffer[S5KJN1_OTP_SWGCC_LEN] = {0};
static kal_uint16 TRULY_JN1_HW_GGC[JN1_HW_GGC_SIZE];
static kal_uint16 TRULY_JN1_HW_GGC_setting[JN1_HW_GGC_SIZE*2];

struct stCAM_CAL_DATAINFO_STRUCT n28s5kjn1reartruly_eeprom_data ={
    .sensorID= N28S5KJN1REARTRULY_SENSOR_ID,
    .deviceID = 0x01,
    .dataLength = 0x3183,
    .sensorVendorid = 0x06000101,
    .vendorByte = {1, 2, 3, 4},
    .dataBuffer = NULL,
};

struct stCAM_CAL_CHECKSUM_STRUCT n28s5kjn1reartruly_Checksum[14] =
{
    {MODULE_ITEM, 0x0000 + OTP_MAP_VENDOR_HEAD_ID_BYTES, 0x0000 + OTP_MAP_VENDOR_HEAD_ID_BYTES, 0x0007 + OTP_MAP_VENDOR_HEAD_ID_BYTES, 0x0008 + OTP_MAP_VENDOR_HEAD_ID_BYTES, 0x55},
    {SN_DATA, 0x0009 + OTP_MAP_VENDOR_HEAD_ID_BYTES, 0x0009 + OTP_MAP_VENDOR_HEAD_ID_BYTES, 0x0015 + OTP_MAP_VENDOR_HEAD_ID_BYTES, 0x0016 + OTP_MAP_VENDOR_HEAD_ID_BYTES, 0x55},
    {AWB_ITEM, 0x0017 + OTP_MAP_VENDOR_HEAD_ID_BYTES, 0x0017 + OTP_MAP_VENDOR_HEAD_ID_BYTES, 0x0027 + OTP_MAP_VENDOR_HEAD_ID_BYTES, 0x0028 + OTP_MAP_VENDOR_HEAD_ID_BYTES, 0x55},
    {AF_ITEM, 0x0029 + OTP_MAP_VENDOR_HEAD_ID_BYTES, 0x0029 + OTP_MAP_VENDOR_HEAD_ID_BYTES, 0x002E + OTP_MAP_VENDOR_HEAD_ID_BYTES, 0x002F + OTP_MAP_VENDOR_HEAD_ID_BYTES, 0x55},
    {LSC_ITEM, 0x0030 + OTP_MAP_VENDOR_HEAD_ID_BYTES, 0x0030 + OTP_MAP_VENDOR_HEAD_ID_BYTES, 0x077C + OTP_MAP_VENDOR_HEAD_ID_BYTES, 0x077D + OTP_MAP_VENDOR_HEAD_ID_BYTES, 0x55},
    {PDAF_ITEM, 0x077E + OTP_MAP_VENDOR_HEAD_ID_BYTES, 0x077E + OTP_MAP_VENDOR_HEAD_ID_BYTES, 0x096E + OTP_MAP_VENDOR_HEAD_ID_BYTES, 0x096F + OTP_MAP_VENDOR_HEAD_ID_BYTES, 0x55},
    {PDAF_PROC2_ITEM, 0x0970 + OTP_MAP_VENDOR_HEAD_ID_BYTES, 0x0970 + OTP_MAP_VENDOR_HEAD_ID_BYTES, 0x0D5C + OTP_MAP_VENDOR_HEAD_ID_BYTES, 0x0D5D + OTP_MAP_VENDOR_HEAD_ID_BYTES,0x55},
    {XTC_DATA, 0x0D5E + OTP_MAP_VENDOR_HEAD_ID_BYTES, 0x0D5E + OTP_MAP_VENDOR_HEAD_ID_BYTES, 0x1B0C + OTP_MAP_VENDOR_HEAD_ID_BYTES, 0x1B0D + OTP_MAP_VENDOR_HEAD_ID_BYTES, 0x55},
    {SENSOR_XTC_DATA, 0x1B0E + OTP_MAP_VENDOR_HEAD_ID_BYTES, 0x1B0E + OTP_MAP_VENDOR_HEAD_ID_BYTES, 0x1E0E + OTP_MAP_VENDOR_HEAD_ID_BYTES, 0x1E0F + OTP_MAP_VENDOR_HEAD_ID_BYTES,0x55},
    {PDXTC_DATA, 0x1E10 + OTP_MAP_VENDOR_HEAD_ID_BYTES, 0x1E10 + OTP_MAP_VENDOR_HEAD_ID_BYTES, 0x2DB0 + OTP_MAP_VENDOR_HEAD_ID_BYTES, 0x2DB1 + OTP_MAP_VENDOR_HEAD_ID_BYTES, 0x55},
    {SWGCC_DATA, 0x2DB2 + OTP_MAP_VENDOR_HEAD_ID_BYTES, 0x2DB2 + OTP_MAP_VENDOR_HEAD_ID_BYTES, 0x3024 + OTP_MAP_VENDOR_HEAD_ID_BYTES, 0x3025 + OTP_MAP_VENDOR_HEAD_ID_BYTES, 0x55},
    {HWGCC_DATA, 0x3026 + OTP_MAP_VENDOR_HEAD_ID_BYTES, 0x3026 + OTP_MAP_VENDOR_HEAD_ID_BYTES, 0x3180 + OTP_MAP_VENDOR_HEAD_ID_BYTES, 0x3181 + OTP_MAP_VENDOR_HEAD_ID_BYTES, 0x55},
    {TOTAL_ITEM, 0x0000 + OTP_MAP_VENDOR_HEAD_ID_BYTES, 0x0000 + OTP_MAP_VENDOR_HEAD_ID_BYTES, 0x3181 + OTP_MAP_VENDOR_HEAD_ID_BYTES, 0x3182 + OTP_MAP_VENDOR_HEAD_ID_BYTES, 0x55},
    {MAX_ITEM, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0x55},  // this line must haved
};

kal_uint16 n28s5kjn1reartruly_read_eeprom(kal_uint32 addr)
{
     kal_uint16 get_byte = 0;

     char pu_send_cmd[2] = { (char)(addr >> 8), (char)(addr & 0xFF) };
     iReadRegI2C(pu_send_cmd, 2, (u8*)&get_byte, 1, EEPROM_GT9778_ID);

     return get_byte;
}

u8* n28s5kjn1reartruly_read_eeprom_onboot(int size) {
    int i = 0;
    u8* ebuf = NULL;
    ebuf = kmalloc(size, GFP_KERNEL);
    memcpy(ebuf, (void *)&(n28s5kjn1reartruly_eeprom_data.sensorVendorid), OTP_MAP_VENDOR_HEAD_ID_BYTES);
    for (i = OTP_MAP_VENDOR_HEAD_ID_BYTES; i < size; i++) {
        ebuf[i] = n28s5kjn1reartruly_read_eeprom(i - OTP_MAP_VENDOR_HEAD_ID_BYTES);
    }
    return ebuf;
}

static void truly_read_eepromData_HW_GGC(void)
{
    kal_uint16 idx = 0, idx_2 = 0;
    for (idx = 0; idx < (JN1_HW_GGC_SIZE*2); idx = idx + 2) {
        TRULY_JN1_HW_GGC[idx_2] = ((s5k_read_cmos_eeprom_8(JN1_HW_GGC_START_ADDR + idx) << 8) & 0xff00) | (s5k_read_cmos_eeprom_8(JN1_HW_GGC_START_ADDR + idx + 1) & 0x00ff);
        idx_2++;
    }
    for (idx = 0; idx <JN1_HW_GGC_SIZE; idx++) {
        TRULY_JN1_HW_GGC_setting[2*idx] = 0x6F12;
        TRULY_JN1_HW_GGC_setting[2*idx + 1] = TRULY_JN1_HW_GGC[idx];
    }
    LOG_INF("read_eepromData_HW_GGC");
}

void truly_write_sensor_HW_GGC(void)
{
    s5k_write_cmos_sensor(0x6028, 0x2400);
    s5k_write_cmos_sensor(0x602A, 0x0CFC);
    #if N28_TRULY_S5KJN1_OTP
    s5k_table_write_cmos_sensor(TRULY_JN1_HW_GGC_setting,
        sizeof(TRULY_JN1_HW_GGC_setting) / sizeof(kal_uint16));
    LOG_INF("burst write_sensor_HW_GGC");
    #else
    kal_uint16 idx = 0;
    for (idx = 0; idx <JN1_HW_GGC_SIZE; idx++) {
        s5k_write_cmos_sensor_8(0x6F12, TRULY_JN1_HW_GGC[idx]);
    }
    #endif
    LOG_INF("write_sensor_HW_GGC");
}

void s5kjn1truly_init_sensor_cali_info(void) {
    if (NULL == n28s5kjn1reartruly_eeprom_data.dataBuffer) {
        n28s5kjn1reartruly_eeprom_data.dataBuffer = n28s5kjn1reartruly_read_eeprom_onboot(n28s5kjn1reartruly_eeprom_data.dataLength + OTP_MAP_VENDOR_HEAD_ID_BYTES);
    }
    if (eeprom_do_checksum(n28s5kjn1reartruly_eeprom_data.dataBuffer, n28s5kjn1reartruly_Checksum) == 0) {
        imgsensor_set_eeprom_data_bywing(&n28s5kjn1reartruly_eeprom_data, n28s5kjn1reartruly_Checksum);
    }
    LOG_INF("get eeprom data success\n");
    #if N28_TRULY_S5KJN1_OTP
    truly_read_eepromData_HW_GGC();
    memcpy(XTCBuffer, (kal_uint8*)&n28s5kjn1reartruly_eeprom_data.dataBuffer[TRULY_S5KJN1_OTP_XTC_OFFSET+1+OTP_MAP_VENDOR_HEAD_ID_BYTES], S5KJN1_OTP_XTC_LEN);
    memcpy(SENSORXTCBuffer, (kal_uint8*)&n28s5kjn1reartruly_eeprom_data.dataBuffer[TRULY_S5KJN1_OTP_SENSORXTC_OFFSET+1+OTP_MAP_VENDOR_HEAD_ID_BYTES], S5KJN1_OTP_SENSORXTC_LEN);
    memcpy(PDXTCXTCBuffer, (kal_uint8*)&n28s5kjn1reartruly_eeprom_data.dataBuffer[TRULY_S5KJN1_OTP_PDXTC_OFFSET+1+OTP_MAP_VENDOR_HEAD_ID_BYTES], S5KJN1_OTP_PDXTC_LEN);
    memcpy(SWGCCBuffer, (kal_uint8*)&n28s5kjn1reartruly_eeprom_data.dataBuffer[TRULY_S5KJN1_OTP_SWGCC_OFFSET+1+OTP_MAP_VENDOR_HEAD_ID_BYTES], S5KJN1_OTP_SWGCC_LEN);
    #endif
}
