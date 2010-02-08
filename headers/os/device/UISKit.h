#ifndef _UISKIT_H
#define _UISKIT_H

#include <SupportDefs.h>

typedef int32 uis_device_id;

enum {
	B_UIS_DEVICE_ADDED = 1,
	B_UIS_DEVICE_REMOVED,
};

class BLooper;


class BUISItem {
public:
						~BUISItem();

	status_t			SetTarget(BLooper *looper);
	status_t			SetTarget(BLooper *looper, void *cookie);

	uint16				UsagePage() { return fUsagePage; };
	uint16				UsageId() { return fUsageId; };

private:
						BUISItem(uis_device_id device, int32 report, int32 item,
							uint16 usagePage, uint16 usageId);
						friend class BUISReport;
						friend class BUISDevice;

	uis_device_id		fDevice;
	int32				fReport;
	int32				fItem;

	void *				fTarget;

	uint16				fUsagePage;
	uint16				fUsageId;
};


class BUISReport {
public:
	int32				CountItems() { return fItems; };
	BUISItem *			ItemAt(int32 index);

private:
						BUISReport(uis_device_id device, int32 report,
							int32 items);
						friend class BUISDevice;

	uis_device_id		fDevice;
	int32				fReport;
	int32				fItems;
};


class BUISDevice {
public:
						BUISDevice(uis_device_id device);
						~BUISDevice();

	status_t			InitCheck() { return fStatus; };

	uis_device_id		Device() { return fDevice; };

	int32				CountReports() { return fInputReports; };
	BUISReport *		ReportAt(int32 index);

	BUISItem *			FindItem(uint32 usage);

	const char *		Name() { return fName; };
	const char *		Path() { return fPath; };

private:
	uis_device_id		fDevice;
	char *				fName;
	char *				fPath;
	uint32				fUsage;
	int32				fInputReports;

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
