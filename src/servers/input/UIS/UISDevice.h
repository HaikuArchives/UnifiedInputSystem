#ifndef _UIS_DEVICE_H
#define _UIS_DEVICE_H

#include <List.h>

#include "UISString.h"


class UISReport;
class UISManager;

class UISDevice {
public:
					UISDevice(UISManager *manager, const char *path);
					~UISDevice();

	UISManager *	Manager() { return fUISManager; };
	bool			HasPath(const char *path);
	bool			HasName(const char *name);

	const char *	Path() { return fPath; };
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

#endif // _UIS_DEVICE_H
