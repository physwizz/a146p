#ifndef __W2_HI1336_FRONT_TRULY_MIPIRAW_OTP__
#define __W2_HI1336_FRONT_TRULY_MIPIRAW_OTP__

extern int hi1336_init_sensor_cali_info(void);
extern void write_cmos_sensor(kal_uint32 addr, kal_uint32 para);
extern void write_cmos_sensor_8(kal_uint32 addr, kal_uint32 para);
extern kal_uint16 read_cmos_sensor(kal_uint32 addr);
#endif
