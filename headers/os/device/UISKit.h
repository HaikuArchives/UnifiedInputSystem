#ifndef _UISKIT_H
#define _UISKIT_H

#include <map>

#include <SupportDefs.h>

typedef int32 uis_device_id;

enum {
	B_UIS_DEVICE_ADDED = 1,
	B_UIS_DEVICE_REMOVED,
};

enum {
	UIS_TYPE_INPUT,
	UIS_TYPE_OUTPUT,
	UIS_TYPE_FEATURE,
	UIS_TYPES,
};

class BLooper;
class BMessage;

class BUISDevice;
class BUISReport;
class BUISItem;


class BUISRoster {
public:
	static BUISDevice*	FindByName(const char* name);

						BUISRoster();
							// TODO: add optional filter functionality
	virtual				~BUISRoster();

	status_t			GetNextDevice(BUISDevice** device);
	void				Rewind();

	status_t			StartWatching(BLooper* looper);
							// TODO: probably change to handler/looper
							// or messenger
	void				StopWatching();

private:
	uis_device_id		fCookie;
	void*				fTarget;
};


class BUISDevice {
public:
						BUISDevice(uis_device_id device);
						~BUISDevice();

	status_t			InitCheck() const { return fStatus; };

	uis_device_id		Device() const { return fDevice; };

	int32				CountReports(uint8 type) const;
	BUISReport*			ReportAt(uint8 type, int32 index);

	BUISItem*			FindItem(uint16 usagePage, uint16 usageId,
							uint8 type = UIS_TYPE_INPUT);

	const char*			Name() const { return fName; };
	const char*			Path() const { return fPath; };
	uint16				UsagePage() const { return fUsagePage; };
	uint16				UsageId() const { return fUsageId; };

private:
	uis_device_id		fDevice;
	char*				fName;
	char*				fPath;
	uint16				fUsagePage;
	uint16				fUsageId;

	int32				fReports[UIS_TYPES];
	typedef std::map<int32, BUISReport*> ReportMap;
	ReportMap			fReportMap[UIS_TYPES];

	status_t			fStatus;
};


class BUISReport {
public:
	BUISDevice*			Device() const { return fDevice; };
	uint8				Type() const { return fType; };
	int32				Index() const { return fIndex; };

	int32				CountItems() const { return fItems; };
	BUISItem*			ItemAt(int32 index);

	status_t			SetItemValue(int32 index, float value);
	status_t			Send();
	void				MakeEmpty();

private:
						BUISReport(BUISDevice* device, uint8 type, int32 index,
							int32 items);
						~BUISReport();
						friend class BUISDevice;

	BUISDevice*			fDevice;
	uint8				fType;
	int32				fIndex;

	int32				fItems;
	typedef std::map<int32, BUISItem*> ItemMap;
	ItemMap				fItemMap;

	BMessage*			fSendMessage;
};


class BUISItem {
public:
	BUISReport*			Report() const { return fReport; };
	uint8				Type() const { return fReport->Type(); };
	uint16				UsagePage() const { return fUsagePage; };
	uint16				UsageId() const { return fUsageId; };
	bool				IsRelative() const { return fIsRelative; };

	status_t			Value(float& value);
	status_t			SetValue(float value);

	status_t			SetTarget(BLooper *looper);
	status_t			SetTarget(BLooper *looper, void *cookie);

private:
						BUISItem(BUISReport* report, int32 item,
							uint16 usagePage, uint16 usageId, bool isRelative);
						~BUISItem();
						friend class BUISReport;
						friend class BUISDevice;

	BUISReport*			fReport;
	int32				fIndex;

	uint16				fUsagePage;
	uint16				fUsageId;
	bool				fIsRelative;

	void*				fTarget;
};


#endif // _UISKIT_H
