#ifndef __EEPROM_WING_KAL_DEFINE__
#define __EEPROM_WING_KAL_DEFINE__

#define OTP_MAP_VENDOR_HEAD_ID_BYTES (4)

struct stCAM_CAL_DATAINFO_STRUCT{
	u32 sensorID; // Sensor ID
	u32 deviceID; // MAIN = 0x01, SUB  = 0x02, MAIN_2 = 0x04
	u32 dataLength; //Data len
	u32 sensorVendorid; // Module ID | Pos ID | Vcm ID | Len ID
	u8  vendorByte[4]; // Module ID offset, Pos ID offset, Vcm ID offset,  Len ID offset
	u8  *dataBuffer; //It's need malloc dataLength cache
};

typedef enum{
	MODULE_ITEM = 0,
	SN_DATA,
	AWB_ITEM,
	SEGMENT_ITEM,
	AF_ITEM,
	LSC_ITEM,
	PDAF_ITEM,
	PDAF_PROC2_ITEM,
	hi5022q_XGC,
	hi5022q_QGC,
	hi5022q_OPC,
	hi5021q_XGC,
	hi5021q_QGC,
	hi5021q_PGC,
	SXTC_ITEM,
	PDXTC_ITEM,
	XTC_DATA,
	SENSOR_XTC_DATA,
	PDXTC_DATA,
	SWGCC_DATA,
	HWGCC_DATA,
	DUALCAM_ITEM,
	TOTAL_ITEM,
	MAX_ITEM,
}stCAM_CAL_CHECKSUM_ITEM;

struct stCAM_CAL_CHECKSUM_STRUCT{
	stCAM_CAL_CHECKSUM_ITEM item;
	u32 flagAdrees;
	u32 startAdress;
	u32 endAdress;
	u32 checksumAdress;
	u8  validFlag;
};

#endif