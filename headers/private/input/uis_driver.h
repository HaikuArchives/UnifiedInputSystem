#ifndef _UIS_DRIVER_H
#define _UIS_DRIVER_H

#include <SupportDefs.h>
#include <Drivers.h>


enum {
	UIS_DEVICE_INFO = B_DEVICE_OP_CODES_END,
	UIS_REPORT_INFO,
	UIS_ITEM_INFO,
	UIS_READ,
	UIS_STOP,
	UIS_STRING_INFO,
};


typedef struct {
	uint32	usage;
	int32	reportCount[3];
	uint32	name;
} uis_device_info;


typedef union {
	struct {
		uint8	type;
		int32	index;
	} in;
	struct {
		void *	report;
		uint8	id;
		int32	itemCount;
	} out;
} uis_report_info;


typedef union {
	struct {
		void *	report;
		int32	index;
	} in;
	struct {
		void *	item;
		uint16	usagePage;
		uint16	usageId;
		bool	isRelative;
	} out;
} uis_item_info;


typedef struct {
	int32	index;
	float	value;
} uis_item_data;


typedef union _uis_report_data {
	struct {
		void *			report;
	} in;
	struct _uis_report_data_out {
		int32			items;
		uis_item_data	item[0];
	} out;
} uis_report_data;


typedef struct {
	uint32	id;
	int32	length;
	char *	string;
	uint32	encoding;
} uis_string_info;


#endif // _UIS_DRIVER_H