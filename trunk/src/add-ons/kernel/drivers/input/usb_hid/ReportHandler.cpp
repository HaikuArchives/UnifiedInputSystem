#include "ReportHandler.h"

#include "Driver.h"
#include "HIDDevice.h"
#include "HIDReport.h"
#include "HIDReportItem.h"

#include <stdlib.h>
#include <ring_buffer.h>

#include "uis_driver.h"


static const size_t kRingBufferSize = 512;


#ifdef TRACE
#undef TRACE
#define TRACE(a...) dprintf("\33[34musb_hid_uis:\33[0m " a)
#endif
ReportHandler::ReportHandler(HIDReport *report)
	:
	fStatus(B_NO_INIT),
	fReport(report),
	fRingBuffer(NULL)
{
	fRingBuffer = create_ring_buffer(kRingBufferSize);
	if (fRingBuffer == NULL) {
		TRACE("failed to create requested ring buffer\n");
		fStatus = B_NO_MEMORY;
		return;
	}

	fStatus = B_OK;
}


ReportHandler::~ReportHandler()
{
	if (fRingBuffer)
		delete_ring_buffer(fRingBuffer);
}


status_t
ReportHandler::Control(uint32 op, void *buffer, size_t length)
{
	switch (op) {
		case UIS_ITEM_INFO:
			{
				uis_item_info *info = (uis_item_info *) buffer;
				HIDReportItem *item = fReport->ItemAt(info->in.index);
				if (item == NULL)
					return B_ERROR; // FIXME
				info->out.item = item;
				info->out.usagePage = item->UsagePage();
				info->out.usageId = item->UsageID();
				info->out.isRelative = item->Relative();
				return B_OK;
			}

		case UIS_READ:
			{
				status_t result;

				while (_RingBufferReadable() == 0) {
					result = _ReadReport();
					if (result != B_OK)
						return result;
				}

				result = _RingBufferRead(buffer, sizeof(uis_report_data));
				if (result != B_OK)
					return result;
				return _RingBufferRead((uint8 *) buffer
						+ sizeof(uis_report_data),
					sizeof(uis_item_data)
						* ((uis_report_data *) buffer)->out.items);
			}

		case UIS_STOP:
			fReport->SetReport(B_ERROR, NULL, 0);
				// fake report for releasing
			return B_OK;
	}

	return B_DEV_INVALID_IOCTL;
}


status_t
ReportHandler::_ReadReport()
{
	status_t result = fReport->WaitForReport(B_INFINITE_TIMEOUT);
	if (result != B_OK) {
		if (fReport->Device()->IsRemoved()) {
			TRACE("device has been removed\n");
			return B_DEV_NOT_READY;
		}

		if (result == B_ERROR)
			return B_ERROR;
				// wake and exit thread

		if (result != B_INTERRUPTED) {
			// interrupts happen when other reports come in on the same
			// input as ours
			TRACE("error waiting for report: %s\n", strerror(result));
		}

		return B_OK;
			// signal that we simply want to try again
	}

	size_t size = sizeof(uis_report_data);
	uis_report_data *data = (uis_report_data *) malloc(size
			+ sizeof(uis_item_data) * fReport->CountItems());
	data->out.items = 0;

	HIDReportItem *item;
	for (uint32 i = 0; i<fReport->CountItems(); i++) {
		item = fReport->ItemAt(i);
		if (item == NULL || !item->HasData())
			continue;
		if (item->Extract() == B_OK && item->Valid()
				&& item->HasDataChanged()) {
			data->out.item[data->out.items].index = i;

			if (item->Maximum() - item->Minimum() == 1) {
				data->out.item[data->out.items++].value
					= (item->Data() == item->Maximum()) ? 1.0f : 0.0f;
			} else {
				float value = (float) item->Data() - (item->Minimum()
					+ item->Maximum()) / 2.0f;
				if (value > -1.0f && value < 1.0f)
					value = 0.0f;
				value *= 2.0f / (item->Maximum() - item->Minimum());
				data->out.item[data->out.items++].value = value;
			}
		}
	}

	fReport->DoneProcessing();
	size += sizeof(uis_item_data) * data->out.items;

	result = _RingBufferWrite(data, size);

	free(data);

	return result;
}


int32
ReportHandler::_RingBufferReadable()
{
	return ring_buffer_readable(fRingBuffer);
}


status_t
ReportHandler::_RingBufferRead(void *buffer, size_t length)
{
	ring_buffer_user_read(fRingBuffer, (uint8 *) buffer, length);
	return B_OK;
}


status_t
ReportHandler::_RingBufferWrite(const void *buffer, size_t length)
{
	ring_buffer_write(fRingBuffer, (const uint8 *) buffer, length);
	return B_OK;
}
