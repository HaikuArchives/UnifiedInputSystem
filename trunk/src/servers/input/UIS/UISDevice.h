#ifndef _UIS_DEVICE_H
#define _UIS_DEVICE_H

#include <UISKit.h>

#include "UISString.h"


class UISReport;
class UISManager;

class UISDevice {
public:
					UISDevice(uis_device_id id, UISManager *manager,
						const char *path);
					~UISDevice();

	UISManager *	Manager() { return fUISManager; };
	bool			HasPath(const char *path);
	bool			HasName(const char *name);

	const char *	Path() { return fPath; };
	const char *	Name() { return fName.String(); };
	uint32			Usage() { return fUsage; };
	int32			CountReports(uint8 type);
	UISReport *		ReportAt(uint8 type, int32 index);

	void			Remove();

private:
	uis_device_id	fDeviceId;
	UISManager *	fUISManager;
	char *			fPath;
	int				fDevice;
	UISString		fName;
	uint32			fUsage;
	UISReport **	fReports[3];
	int32			fReportsCount[3];
};

#endif // _UIS_DEVICE_H
