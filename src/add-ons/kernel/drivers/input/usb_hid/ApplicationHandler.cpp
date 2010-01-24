#include "ApplicationHandler.h"

#include "Driver.h"
#include "HIDDevice.h"
#include "HIDReport.h"
#include "ReportHandler.h"

#include <UTF8.h>

#include <new>
#include <stdlib.h>

#include "kb_mouse_driver.h"


#ifdef TRACE
#undef TRACE
#define TRACE(a...) dprintf("\33[34mjoy_dev:\33[0m " a)
#endif
ApplicationHandler::ApplicationHandler(HIDDevice *device, uint32 usage)
	:
	fDevice(device),
	fUsage(usage),
	fPublishPath(NULL),
	fReportHandlerCount(0),
	fReportHandlers(NULL)
{
}


ApplicationHandler::~ApplicationHandler()
{
	while (fReportHandlerCount--)
		delete fReportHandlers[fReportHandlerCount];
	free(fReportHandlers);
	free(fPublishPath);
}


void
ApplicationHandler::AddReport(HIDReport *report)
{
	ReportHandler *handler = new (std::nothrow) ReportHandler(report);
	if (handler == NULL)
		return;
	if (handler->InitCheck() != B_OK) {
		delete handler;
		return;
	}

	ReportHandler **reportHandlers = (ReportHandler **)
		realloc(fReportHandlers, sizeof(ReportHandler **)
		* fReportHandlerCount + 1);
	if (reportHandlers == NULL) {
		TRACE("no memory for report handlers list\n");
		delete handler;
		return;
	}
	fReportHandlers = reportHandlers;
	fReportHandlers[fReportHandlerCount++] = handler;
}


void
ApplicationHandler::SetPublishPath(char *publishPath)
{
	free(fPublishPath);
	fPublishPath = publishPath;
}


void
ApplicationHandler::AddHandlers(HIDDevice *device,
	ApplicationHandler ***handlerList, uint32 *handlerCount)
{
	TRACE("adding handlers\n");

	HIDParser *parser = device->Parser();

	*handlerList = NULL;
	*handlerCount = 0;
	for (uint8 i = 0; i < parser->CountReports(HID_REPORT_TYPE_INPUT); i++) {
		HIDReport *report = parser->ReportAt(HID_REPORT_TYPE_INPUT, i);
		if (report == NULL)
			continue;
		ApplicationHandler *handler = NULL;
		if (*handlerList == NULL) {
			handler = new(std::nothrow)
				ApplicationHandler(device, report->ApplicationUsage());
			if (handler != NULL) {
				*handlerList = (ApplicationHandler **)
					malloc(sizeof(ApplicationHandler *));
				if (*handlerList == NULL) {
					TRACE("out of memory allocating application handler list\n");
					return;
				}
			}
		} else {
		}
		*handlerList[(*handlerCount)++] = handler;
		handler->AddReport(report);
	}

	TRACE("added %ld handlers for hid device\n", *handlerCount);
}


status_t
ApplicationHandler::Open(uint32 flags)
{
	return fDevice->Open(this, flags);
}


status_t
ApplicationHandler::Close()
{
	return fDevice->Close(this);
}


status_t
ApplicationHandler::Control(uint32 op, void *buffer, size_t length)
{
	switch (op) {
		case UIS_DEVICE_INFO:
			{
				uis_device_info *info = (uis_device_info *) buffer;
				info->usage = fUsage;
				info->reportCount = fReportHandlerCount;
				info->name = fDevice->Name();
				return B_OK;
			}

		case UIS_REPORT_INFO:
			{
				uis_report_info *info = (uis_report_info *) buffer;
				if (info->in.index >= fReportHandlerCount)
					return B_ERROR; // FIXME
				ReportHandler *report = fReportHandlers[info->in.index];
				if (report == NULL)
					return B_ERROR; // FIXME
				info->out.report = report;
				info->out.id = report->Report()->ID();
				info->out.itemCount = report->Report()->CountItems();
				return B_OK;
			}

		case UIS_ITEM_INFO:
		case UIS_READ:
		case UIS_STOP:
			{
				ReportHandler *handler = *((ReportHandler **) buffer);
				return handler->Control(op, buffer, length);
			}

		case UIS_STRING_INFO:
			{
				uis_string_info *info = (uis_string_info *) buffer;
				info->encoding = B_UNICODE_CONVERSION;
				return fDevice->GetString(info->id, info->string,
					&info->length);
			}
	}

	return B_DEV_INVALID_IOCTL;
}
