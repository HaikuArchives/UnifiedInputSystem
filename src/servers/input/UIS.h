#ifndef _UIS_MANAGER_H
#define _UIS_MANAGER_H

#include <Looper.h>
#include <List.h>
#include <Locker.h>
#include <String.h>
#include <Messenger.h>

using namespace BPrivate;


typedef struct {
	team_id	team;
	port_id	port;
	int32	token;
	uint32	refCount;
} uis_target;


typedef struct {
	uis_target *	target;
	void *			cookie;
} uis_item_target;


class UISString {
public:
	static char *	StrDupTrim(const char *str);

					UISString();
					~UISString();

	void			SetTo(int fd, uint32 id);
	const char *	String();

private:
	int				fDevice;
	uint32			fId;
	char *			fString;
	bool			fIsValid;
};


class UISReport;


class UISReportItem {
public:
				UISReportItem(int fd, UISReport *report, uint32 index);
				~UISReportItem();

	status_t	InitCheck();
	void		SetValue(uint32 value);

	uint16		Id() { return fUsageId; };
	bool		IsRelative() { return fIsRelative; };
	uint32		Value() { return fValue; };

	void		SetTarget(team_id team, port_id port, int32 token, void *cookie,
					void **target);

private:
	UISReport *	fUISReport;
	void *		fItem;
	uint16		fUsagePage;
	uint16		fUsageId;
	bool		fIsRelative;
	uint32		fMinimum;
	uint32		fMaximum;
	uint32		fValue;
	BList		fItemTargetList;

	team_id		fTeamId;
	port_id		fPortId;
	int32		fObjectToken;
};


class UISDevice;
union _uis_report_data;
typedef _uis_report_data uis_report_data;


class UISReport {
public:
					UISReport(int fd, UISDevice *device, uint8 type,
						uint8 index);
					~UISReport();

	status_t		InitCheck() { return fStatus; };
	UISDevice *		Device() { return fUISDevice; };
	void *			Report() { return fReport; };

	void			SetReport(uis_report_data *data);
	int32			CountItems() { return fItemList.CountItems(); };
	UISReportItem *	ItemAt(int32 index);

private:
	static bool		_RemoveItemListItem(void *arg);
	static status_t	_ReadingThreadEntry(void *arg);
	void			_ReadingThread();

	status_t		fStatus;
	int				fDevice;
	UISDevice *		fUISDevice;
	uint8			fType;
	void *			fReport;
	uint8			fId;
	thread_id		fReadingThread;
	volatile bool	fThreadActive;
	BList			fItemList;
};


class UISManager;


class UISDevice {
public:
					UISDevice(UISManager *manager, const char *path);
					~UISDevice();

	UISManager *	Manager() { return fUISManager; };
	bool			HasPath(const char *path);
	bool			HasName(const char *name);

	const char *	Name() { return fName.String(); };
	uint32			Usage() { return fUsage; };
	uint8			CountReports(uint8 type);
	UISReport *		ReportAt(uint8 type, uint8 index);

	void			Remove();

private:
	static bool		_RemoveReportListItem(void *arg);

	UISManager *	fUISManager;
	char *			fPath;
	int				fDevice;
	UISString		fName;
	uint32			fUsage;
	BList			fReportList[3];
};


class UISManager : public BLooper {
public:
				UISManager();
				~UISManager();

	status_t	Start();
	void		Stop();

	void		MessageReceived(BMessage *message);
	status_t	HandleMessage(BMessage *message, BMessage *reply);

	void		RemoveDevice(UISDevice *device);

	uis_target *	FindOrAddTarget(team_id team, port_id port, int32 token);
	void		RemoveTarget(uis_target *target);
	void		SendEvents(BList *itemTargetList, UISReportItem *item);

private:
	static bool	_RemoveDeviceListItem(void *arg);
	void		_HandleAddRemoveDevice(BMessage *message);
	status_t	_SendEvent(uis_item_target *itemTarget, UISReportItem *item);

	bool		fIsRunning;
	BList		fUISDeviceList;
	BLocker		fUISDeviceListLocker;
	BList		fTargetList;
	BMessenger	fMessenger;
};


#endif // _UIS_MANAGER_H
