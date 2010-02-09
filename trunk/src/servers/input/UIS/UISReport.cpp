#include "UISReport.h"
#include "UISDevice.h"
#include "UISItem.h"

#include <uis_driver.h>
#include <UISProtocol.h>

#include <errno.h>
#include <stdio.h>
#include <new>

using std::nothrow;

#include "UIS_debug.h"


static const uint32 kReportThreadPriority = B_FIRST_REAL_TIME_PRIORITY + 4;


UISReport::UISReport(int fd, UISDevice *device, uint8 type, uint8 index)
	:
	fStatus(B_NO_INIT),
	fDevice(fd),
	fUISDevice(device),
	fType(type),
	fReport(NULL),
	fId(0),
	fReadingThread(-1),
	fThreadActive(false),
	fItems(NULL),
	fItemsCount(0)
{
	uis_report_info reportDesc;
	reportDesc.in.type = type;
	reportDesc.in.index = index;
	fStatus = ioctl(fd, UIS_REPORT_INFO, &reportDesc);
	if (fStatus != B_OK)
		return;
	fReport = reportDesc.out.report;
	fId = reportDesc.out.id;
	TRACE("create report type: %d, id: %d, items: %d\n", fType, fId,
		reportDesc.out.itemCount);

	fItems = new (std::nothrow) UISReportItem *[reportDesc.out.itemCount];
	if (fItems == NULL)
		return;
	for (int32 n = 0 ; n < reportDesc.out.itemCount; n++) {
		UISReportItem *item = new (std::nothrow) UISReportItem(fd, this, n);
		if (item == NULL)
			break;
		if (item->InitCheck() != B_OK) {
			delete item;
			break;
		}
		fItems[fItemsCount++] = item;
	}

	char threadName[B_OS_NAME_LENGTH];
	snprintf(threadName, B_OS_NAME_LENGTH, "uis report %08x reader",
		(unsigned int) this);
		// FIXME: fix the name of the thread
	fReadingThread = spawn_thread(_ReadingThreadEntry, threadName,
		kReportThreadPriority, (void *) this);

	if (fReadingThread < B_OK) {
		fStatus = fReadingThread;
		return;
	}

	fThreadActive = true;
	fStatus = resume_thread(fReadingThread);
	if (fStatus != B_OK)
		fThreadActive = false;
}


UISReport::~UISReport()
{
	//TRACE("delete report type: %d, id: %d\n", fType, fId);

	//TRACE("will stop the thread\n");
	if (fThreadActive) {
		fThreadActive = false;
			// this will eventually bring waiting thread down

		if (ioctl(fDevice, UIS_STOP, &fReport) == B_OK) {
			TRACE("wait for reading thread to quit...\n");
			wait_for_thread(fReadingThread, NULL);
				// wait only if ioctl succeeded, it'll eventually kill
				// the thread otherwise. don't have better idea for now
		}
	}

	for (int32 i = 0 ; i < fItemsCount; i++)
		delete fItems[i];
	delete [] fItems;
}


void
UISReport::SetReport(uis_report_data *data)
{
	//TRACE("has items: %d\n", data->out.items);
	for (int32 i = 0; i < data->out.items; i++) {
		//TRACE("index of item: %d\n", data->out.item[i].index);
		UISReportItem *item = ItemAt(data->out.item[i].index);
			// TODO: check numbering
		if (item != NULL)
			item->SetValue(data->out.item[i].value);
		//TRACE("set value report %d %d %08x\n", fType, fId, i);
	}
}


UISReportItem *
UISReport::ItemAt(int32 index)
{
	if (index >= CountItems())
		return NULL;
	return fItems[index];
}


status_t
UISReport::_ReadingThreadEntry(void *arg)
{
	UISReport *report = (UISReport *) arg;
	report->_ReadingThread();
	return B_OK;
}


void
UISReport::_ReadingThread()
{
	TRACE("entering thread for report id: %d\n", fId);

	uint8 *buffer = new (std::nothrow) uint8[sizeof(uis_report_data)
			+ sizeof(uis_report_info) * CountItems()];
				// size is calculated for all possible items included
	if (buffer == NULL) {
		fThreadActive = false;
		return;
	}

	while (fThreadActive) {
		uis_report_data *data = (uis_report_data *) buffer;
		data->in.report = fReport;
		if (ioctl(fDevice, UIS_READ, data) != B_OK) {
			if (errno == B_DEV_NOT_READY) {
				delete [] buffer;
				fThreadActive = false;
				fUISDevice->Remove();
					// BONKERS !
				return;
			}

			TRACE("ioctl status = %08x\n", errno);
			fThreadActive = false;
			break;
		}

		SetReport(data);
	}

	delete [] buffer;

	TRACE("leaving thread for report id: %d\n", fId);
}
