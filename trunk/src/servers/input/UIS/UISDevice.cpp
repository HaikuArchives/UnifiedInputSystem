#include "UISDevice.h"
#include "UISManager.h"
#include "UISReport.h"

#include <UISProtocol.h>

#include <stdlib.h>
#include <string.h>
#include <new>

using std::nothrow;

#include "UIS_debug.h"


UISDevice::UISDevice(uis_device_id id, UISManager *manager, const char *path)
	:
	fStatus(B_NO_INIT),
	fDeviceId(id),
	fUISManager(manager),
	fPath(strdup(path)),
	fDevice(-1),
	fUsagePage(0),
	fUsageId(0)
{
	fReports[UIS_REPORT_TYPE_INPUT] = NULL;
	fReports[UIS_REPORT_TYPE_OUTPUT] = NULL;
	fReports[UIS_REPORT_TYPE_FEATURE] = NULL;
	fReportsCount[UIS_REPORT_TYPE_INPUT] = 0;
	fReportsCount[UIS_REPORT_TYPE_OUTPUT] = 0;
	fReportsCount[UIS_REPORT_TYPE_FEATURE] = 0;

	TRACE("create device at %s\n", path);

	fDevice = open(fPath, O_RDWR);
	if (fDevice == -1)
		return;

	uis_device_info info;
	fStatus = ioctl(fDevice, UIS_DEVICE_INFO, &info);
	if (fStatus != B_OK)
		return;

	fUsagePage = info.usage.page;
	fUsageId = info.usage.id;
	fName.SetTo(fDevice, info.name);
	//TRACE("usage: %08x, input report count: %d, name: %d\n", fUsage,
	//	info.reportCount, info.name);

	for (uint8 type = 0; type < UIS_REPORT_TYPES; type ++) {
		fReports[type] = new (std::nothrow) UISReport *[info.reportCount[type]];
		if (fReports[type] == NULL)
			return;

		for (int32 i = 0; i < info.reportCount[type]; i++) {
			UISReport *report = new (std::nothrow) UISReport(fDevice, this,
				type, i);
			if (report == NULL)
				break;
			if (report->InitCheck() != B_OK) {
				delete report;
				break;
			}
			fReports[type][fReportsCount[type]++] = report;
		}
	}
}


UISDevice::~UISDevice()
{
	TRACE("delete device at: %s\n", fPath);

	for (uint8 type = 0; type < UIS_REPORT_TYPES; type ++) {
		for (int32 n = 0; n < fReportsCount[type]; n++)
			delete fReports[type][n];
		delete [] fReports[type];
	}

	if (fDevice != -1) {
		close(fDevice);
		fDevice = -1;
	}

	free(fPath);
}


bool
UISDevice::HasPath(const char *path)
{
	return strcmp(path, fPath) == 0;
}


bool
UISDevice::HasName(const char *name)
{
	if (name == NULL)
		return false;
	const char *localName = fName.String();
	return (localName != NULL) ? strcmp(name, localName) == 0 : false;
}


int32
UISDevice::CountReports(uint8 type)
{
	return (type < UIS_REPORT_TYPES) ? fReportsCount[type] : 0;
}


UISReport *
UISDevice::ReportAt(uint8 type, int32 index)
{
	if (index >= CountReports(type))
		return NULL;
	return fReports[type][index];
}


void
UISDevice::Remove()
{
	fUISManager->RemoveDevice(fDeviceId);
}
