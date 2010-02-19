#ifndef _UIS_DEVICE_H
#define _UIS_DEVICE_H

#include <uis_driver.h>
#include <UISKit.h>

#include "UISString.h"


class UISReport;
class UISManager;

class UISDevice {
public:
					UISDevice(uis_device_id id, UISManager *manager,
						const char *path);
					~UISDevice();

	status_t		InitCheck() { return fStatus; };

	UISManager *	Manager() { return fUISManager; };
	bool			HasPath(const char *path);
	bool			HasName(const char *name);

	const char *	Path() { return fPath; };
	const char *	Name() { return fName.String(); };
	uint16			UsagePage() { return fUsagePage; };
	uint16			UsageId() { return fUsageId; };
	int32			CountReports(uint8 type);
	UISReport *		ReportAt(uint8 type, int32 index);

	void			Remove();

private:
	status_t		fStatus;
	uis_device_id	fDeviceId;
	UISManager *	fUISManager;
	char *			fPath;
	int				fDevice;
	UISString		fName;
	uint16			fUsagePage;
	uint16			fUsageId;
	UISReport **	fReports[UIS_REPORT_TYPES];
	int32			fReportsCount[UIS_REPORT_TYPES];
};

#endif // _UIS_DEVICE_H
