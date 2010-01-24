#ifndef _APPLICATION_HANDLER_H
#define _APPLICATION_HANDLER_H

#include <SupportDefs.h>

class HIDDevice;
class HIDReport;
class ReportHandler;

class ApplicationHandler {
public:
						ApplicationHandler(HIDDevice *device, uint32 usage);
						~ApplicationHandler();

	void				AddReport(HIDReport *report);

	HIDDevice *			Device() { return fDevice; };
	uint32				Usage() { return fUsage; };

	void				SetPublishPath(char *publishPath);
	const char *		PublishPath() { return fPublishPath; };

	static void			AddHandlers(HIDDevice *device,
							ApplicationHandler ***handlerList,
							uint32 *handlerCount);

	status_t			Open(uint32 flags);
	status_t			Close();

	status_t			Control(uint32 op, void *buffer, size_t length);

private:
	HIDDevice *			fDevice;
	uint32				fUsage;
	char *				fPublishPath;
	char *				fName;
	uint8				fReportHandlerCount;
	ReportHandler **	fReportHandlers;
};

#endif // _APPLICATION_HANDLER_H
