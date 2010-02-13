#ifndef _UIS_DRIVER_H
#define _UIS_DRIVER_H

#include <SupportDefs.h>
#include <Drivers.h>


enum {
	UIS_DEVICE_INFO = B_DEVICE_OP_CODES_END,
	UIS_REPORT_INFO,
	UIS_ITEM_INFO,
	UIS_STRING_INFO,
	UIS_READ,
	UIS_SEND,
	UIS_STOP,
};


enum {
	UIS_REPORT_TYPE_INPUT = 0,
	UIS_REPORT_TYPE_OUTPUT,
	UIS_REPORT_TYPE_FEATURE,
	UIS_REPORT_TYPES,
};


typedef union {
	struct {
		uint16	id;
		uint16	page;
	};
	uint32	extended;
} uis_usage;


typedef struct {
	uis_usage	usage;
	int32		reportCount[3];
	uint32		name;
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
		void *		report;
		int32		index;
	} in;
	struct {
		void *		item;
		uis_usage	usage;
		bool		isRelative;
	} out;
} uis_item_info;


typedef struct {
	int32	index;
	float	value;
} uis_item_data;


typedef struct _uis_report_data {
	void *			report;
	int32			items;
	uis_item_data	item[0];
} uis_report_data;


typedef struct {
	uint32	id;
	int32	length;
	char *	string;
	uint32	encoding;
} uis_string_info;


#endif // _UIS_DRIVER_H
