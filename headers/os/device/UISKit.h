#ifndef _UISKIT_H
#define _UISKIT_H

#include <SupportDefs.h>

typedef int32 uis_device_id;

enum {
	B_UIS_DEVICE_ADDED = 1,
	B_UIS_DEVICE_REMOVED,
};

enum {
	UIS_TYPE_INPUT		= 1,
	UIS_TYPE_OUTPUT,
	UIS_TYPE_FEATURE	= 4,
	UIS_TYPE_ANY		= 7,
};

class BLooper;
class BMessage;


class BUISItem {
public:
						~BUISItem();

	uint8				Type() { return fType; };
	uint16				UsagePage() { return fUsagePage; };
	uint16				UsageId() { return fUsageId; };
	bool				IsRelative() { return fIsRelative; };

	status_t			Update();
	float				Value() { return fValue; };

	status_t			SetTarget(BLooper *looper);
	status_t			SetTarget(BLooper *looper, void *cookie);

private:
						BUISItem(uis_device_id device, int32 report, int32 item,
							uint8 type, uint16 usagePage, uint16 usageId,
							bool isRelative, float value);
						friend class BUISReport;
						friend class BUISDevice;

	uis_device_id		fDevice;
	int32				fReport;
	int32				fItem;

	void *				fTarget;

	uint8				fType;
	uint16				fUsagePage;
	uint16				fUsageId;
	bool				fIsRelative;

	float				fValue;
};


class BUISReport {
public:
	int32				CountItems() { return fItems; };
	BUISItem *			ItemAt(int32 index);

	status_t			AddItemValue(int32 index, float value);
	status_t			Send();
	void				MakeEmpty();

private:
						BUISReport(uis_device_id device, int32 report,
							int32 items, uint8 type);
						friend class BUISDevice;

	uis_device_id		fDevice;
	int32				fReport;
	int32				fItems;

	uint8				fType;

	BMessage *			fSendMessage;
};


class BUISDevice {
public:
						BUISDevice(uis_device_id device);
						~BUISDevice();

	status_t			InitCheck() { return fStatus; };

	uis_device_id		Device() { return fDevice; };

	int32				CountReports(uint8 type);
	BUISReport *		ReportAt(uint8 type, int32 index);

	BUISItem *			FindItem(uint16 usagePage, uint16 usageId);

	const char *		Name() { return fName; };
	const char *		Path() { return fPath; };

private:
	uis_device_id		fDevice;
	char *				fName;
	char *				fPath;
	uint16				fUsagePage;
	uint16				fUsageId;
	int32				fInputReports;
	int32				fOutputReports;
	int32				fFeatureReports;

	status_t			fStatus;
};


class BUISRoster {
public:
	static BUISDevice *	FindByName(const char *name);

						BUISRoster();
							// TODO: add optional filter functionality
	virtual				~BUISRoster();

	status_t			GetNextDevice(BUISDevice **device);
	void				Rewind();

	status_t			StartWatching(BLooper *looper);
							// TODO: probably change to handler/looper
							// or messenger
	void				StopWatching();

private:
	uis_device_id		fCookie;
	void *				fTarget;

};


#endif // _UISKIT_H
