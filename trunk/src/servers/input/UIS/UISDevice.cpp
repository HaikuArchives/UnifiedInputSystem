#include "UISDevice.h"
#include "UISManager.h"
#include "UISReport.h"

#include <uis_driver.h>
#include <UISProtocol.h>

#include <stdlib.h>
#include <string.h>
#include <new>

using std::nothrow;

#include "UIS_debug.h"


UISDevice::UISDevice(uis_device_id id, UISManager *manager, const char *path)
	:
	fDeviceId(id),
	fUISManager(manager),
	fPath(strdup(path)),
	fDevice(-1),
	fUsage(0)
{
	fReports[0] = NULL;
	fReports[1] = NULL;
	fReports[2] = NULL;
	fReportsCount[0] = 0;
	fReportsCount[1] = 0;
	fReportsCount[2] = 0;

	TRACE("create device at %s\n", path);

	fDevice = open(fPath, O_RDWR);
	if (fDevice == -1)
		return;

	uis_device_info info;
	status_t status = ioctl(fDevice, UIS_DEVICE_INFO, &info);
	if (status != B_OK)
		return;

	fUsage = info.usage;
	fName.SetTo(fDevice, info.name);
	//TRACE("usage: %08x, input report count: %d, name: %d\n", fUsage,
	//	info.reportCount, info.name);

	for (uint8 type = 0; type < 3; type ++) {
		fReports[type] = new (std::nothrow) UISReport *[info.reportCount[type]];
		if (fReports[type] == NULL)
			return;
		for (int32 i = 0; i < info.reportCount[type]; i++) {
			UISReport *report = new (std::nothrow) UISReport(fDevice, this,
				1, i);
					// FIXME: un-fix the report type
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

	for (uint8 type = 0; type < 3; type ++) {
		for (int32 n = 0; n < fReportsCount[type]; n++)
			delete fReports[type][n];
		delete [] fReports[type];
	}

	//TRACE("will close the device\n");
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
	return (type < 3) ? fReportsCount[type] : 0;
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
