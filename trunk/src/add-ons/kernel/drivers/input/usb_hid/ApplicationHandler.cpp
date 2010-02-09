#include "ApplicationHandler.h"

#include "Driver.h"
#include "HIDDevice.h"
#include "HIDReport.h"
#include "ReportHandler.h"

#include <UTF8.h>

#include <new>
#include <stdlib.h>


#ifdef TRACE
#undef TRACE
#define TRACE(a...) dprintf("\33[34musb_hid_uis:\33[0m " a)
#endif
ApplicationHandler::ApplicationHandler(HIDDevice *device, uint32 usage)
	:
	fDevice(device),
	fUsage(usage),
	fPublishPath(NULL)
{
	fReportHandlers[UIS_REPORT_TYPE_INPUT] = NULL;
	fReportHandlers[UIS_REPORT_TYPE_OUTPUT] = NULL;
	fReportHandlers[UIS_REPORT_TYPE_FEATURE] = NULL;
	fReportHandlerCount[UIS_REPORT_TYPE_INPUT] = 0;
	fReportHandlerCount[UIS_REPORT_TYPE_OUTPUT] = 0;
	fReportHandlerCount[UIS_REPORT_TYPE_FEATURE] = 0;
}


ApplicationHandler::~ApplicationHandler()
{
	for (uint8 i = 0; i < UIS_REPORT_TYPES; i++) {
		while (fReportHandlerCount[i]--)
			delete fReportHandlers[i][fReportHandlerCount[i]];

		free(fReportHandlers[i]);
	}

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

	uint8 type = report->TypeId();

	ReportHandler **reportHandlers = (ReportHandler **)
		realloc(fReportHandlers[type], sizeof(ReportHandler *)
		* fReportHandlerCount[type] + 1);
	if (reportHandlers == NULL) {
		TRACE("no memory for report handlers list\n");
		delete handler;
		return;
	}
	fReportHandlers[type] = reportHandlers;
	fReportHandlers[type][fReportHandlerCount[type]++] = handler;
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
	for (uint8 i = 0; i < parser->CountReports(HID_REPORT_TYPE_ANY); i++) {
		HIDReport *report = parser->ReportAt(HID_REPORT_TYPE_ANY, i);
		if (report == NULL)
			continue;

		ApplicationHandler *handler = NULL;
		for (uint8 ai = 0; ai < *handlerCount; ai++) {
			if ((*handlerList)[ai]->Usage() == report->ApplicationUsage()) {
				handler = *handlerList[ai];
				break;
			}
		}

		if (handler == NULL) {
			handler = new(std::nothrow) ApplicationHandler(device,
					report->ApplicationUsage());
			if (handler == NULL)
				continue;

			ApplicationHandler **handlers = (ApplicationHandler **)
				realloc(*handlerList, sizeof(ApplicationHandler *)
					* *handlerCount + 1);
			if (handlers == NULL) {
				TRACE("out of memory allocating application handler list\n");
				return;
			}

			*handlerList = handlers;
			*handlerList[(*handlerCount)++] = handler;
		}

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
				for (uint8 i = 0; i < UIS_REPORT_TYPES; i++)
					info->reportCount[i] = fReportHandlerCount[i];
				info->name = fDevice->Name();
				return B_OK;
			}

		case UIS_REPORT_INFO:
			{
				uis_report_info *info = (uis_report_info *) buffer;
				if (info->in.index >= fReportHandlerCount[info->in.type])
					return B_ERROR; // FIXME
				ReportHandler *report
					= fReportHandlers[info->in.type][info->in.index];
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
