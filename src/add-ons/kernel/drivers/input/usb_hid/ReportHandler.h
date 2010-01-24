#ifndef _REPORT_HANDLER_H
#define _REPORT_HANDLER_H

#include <SupportDefs.h>

class HIDReport;

class ReportHandler {
public:
							ReportHandler(HIDReport *report);
							~ReportHandler();

	status_t				InitCheck() { return fStatus; };

	HIDReport *				Report() { return fReport; };

	status_t				Control(uint32 op, void *buffer, size_t length);

private:
	status_t				_ReadReport();

	int32					_RingBufferReadable();
	status_t				_RingBufferRead(void *buffer, size_t length);
	status_t				_RingBufferWrite(const void *buffer, size_t length);

	status_t				fStatus;
	HIDReport *				fReport;
	struct ring_buffer *	fRingBuffer;
};

#endif // _REPORT_HANDLER_H
