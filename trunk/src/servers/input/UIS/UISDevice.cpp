#include "UISDevice.h"
#include "UISManager.h"
#include "UISReport.h"

#include <UISProtocol.h>
#include "kb_mouse_driver.h"

#include <stdlib.h>
#include <string.h>
#include <new>

using std::nothrow;

#include "UIS_debug.h"


UISDevice::UISDevice(UISManager *manager, const char *path)
	:
	fUISManager(manager),
	fPath(strdup(path)),
	fDevice(-1),
	fUsage(0)
{
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

	for (uint8 n = 0; n<info.reportCount; n++) {
		UISReport *report = new (std::nothrow) UISReport(fDevice, this, 1, n);
			// FIXME: un-fix the report type
		if (report == NULL)
			break;
		if (report->InitCheck() != B_OK) {
			delete report;
			break;
		}
		fReportList[0].AddItem(report);
	}
}


UISDevice::~UISDevice()
{
	TRACE("delete device at: %s\n", fPath);

	fReportList[0].DoForEach(_RemoveReportListItem);

	//TRACE("will close the device\n");
	if (fDevice != -1) {
		close(fDevice);
		fDevice = -1;
	}

	free(fPath);
}


bool
UISDevice::_RemoveReportListItem(void *arg)
{
	delete (UISReport *) arg;
	return false;
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


uint8
UISDevice::CountReports(uint8 type)
{
	return (type < 3) ? (uint8) fReportList[type].CountItems() : 0;
}


UISReport *
UISDevice::ReportAt(uint8 type, uint8 index)
{
	if (index >= CountReports(type))
		return NULL;
	return (UISReport *) fReportList[type].ItemAt(index);
}


void
UISDevice::Remove()
{
	fUISManager->RemoveDevice(this);
}
