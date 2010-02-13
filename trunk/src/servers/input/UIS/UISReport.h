#ifndef _UIS_REPORT_H
#define _UIS_REPORT_H

#include <Message.h>


struct _uis_report_data;
typedef _uis_report_data uis_report_data;

class UISReportItem;
class UISDevice;

class UISReport {
public:
					UISReport(int fd, UISDevice *device, uint8 type,
						uint8 index);
					~UISReport();

	status_t		InitCheck() { return fStatus; };
	UISDevice *		Device() { return fUISDevice; };
	void *			Report() { return fReport; };

	void			SetReport(uis_report_data *data);
	int32			CountItems() { return fItemsCount; };
	UISReportItem *	ItemAt(int32 index);

	status_t		SendReport(BMessage *message);

private:
	static status_t	_ReadingThreadEntry(void *arg);
	void			_ReadingThread();

	status_t		fStatus;
	int				fDevice;
	UISDevice *		fUISDevice;
	uint8			fType;
	void *			fReport;
	uint8			fId;
	thread_id		fReadingThread;
	volatile bool	fThreadActive;
	UISReportItem **	fItems;
	int32			fItemsCount;
};


#endif // _UIS_REPORT_H
