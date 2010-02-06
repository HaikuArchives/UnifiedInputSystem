#include <Application.h>
#include <Window.h>
#include <Message.h>
#include <Looper.h>

#include <usb/USB_hid.h>

#include <UISProtocol.h>
#include <input_globals.h>
#include <InputServerTypes.h>
#include <AppMisc.h>

#include <new>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

using std::nothrow;
using namespace BPrivate;


class BUInputItem {
public:
						~BUInputItem();

	status_t			SetTarget(BLooper *looper);

private:
						BUInputItem(void *device, void *report, void *item,
							uint16 id);
						friend class BUInputReport;
						friend class BUInputDevice;

	void *				fDevice;
	void *				fReport;
	void *				fItem;
	void *				fTarget;
	uint16				fId;
	BLooper *			fLooper;
};


BUInputItem::BUInputItem(void *device, void *report, void *item, uint16 id)
	:
	fDevice(device),
	fReport(report),
	fItem(item),
	fTarget(NULL),
	fId(id),
	fLooper(NULL)
{
	printf("create item for device: %08x, report: %08x, item: %08x, "
		"id: %d\n", fDevice, fReport, fItem, fId);
}


BUInputItem::~BUInputItem()
{
	if (fTarget != NULL)
		SetTarget(NULL);
}


status_t
BUInputItem::SetTarget(BLooper *looper)
{
	BMessage command(IS_UIS_MESSAGE), reply;
	command.AddInt32("opcode", B_UIS_ITEM_SET_TARGET);
	command.AddPointer("device", fDevice);
	command.AddPointer("item", fItem);

	if (looper != NULL) {
		command.AddInt32("team id", (int32) looper->Team());
		command.AddInt32("looper port", (int32) _get_looper_port_(looper));
		command.AddInt32("object token", _get_object_token_(looper));
		command.AddPointer("cookie", this);
	}

	command.AddPointer("target", fTarget);
	status_t status = _control_input_server_(&command, &reply);
	if (status != B_OK)
		return status;
	return reply.FindPointer("target", &fTarget);
}

class BUInputReport {
public:
	int32				CountItems() { return fItems; };
	BUInputItem *		ItemAt(int32 index);

private:
						BUInputReport(void *device, void *report, int32 items);
						friend class BUInputDevice;

	void *				fDevice;
	void *				fReport;
	int32				fItems;
};


BUInputReport::BUInputReport(void *device, void *report, int32 items)
	:
	fDevice(device),
	fReport(report),
	fItems(items)
{
	printf("create report for device: %08x, report: %08x, with %d items\n",
		fDevice, fReport, fItems);
}


BUInputItem *
BUInputReport::ItemAt(int32 index)
{
	BMessage command(IS_UIS_MESSAGE), reply;

	command.AddInt32("opcode", B_UIS_GET_ITEM);
	command.AddPointer("device", fDevice);
	command.AddPointer("report", fReport);
	command.AddInt32("index", index);

	printf("asking for item: %d\n", index);
	if (_control_input_server_(&command, &reply) != B_OK)
		return NULL;

	uint16 id;
	void *item;
	if (reply.FindInt16("id", (int16 *) &id) != B_OK
		|| reply.FindPointer("item", &item) != B_OK)
		return NULL;

	return new (std::nothrow) BUInputItem(fDevice, fReport, item, id);
}


class BUInputDevice {
public:
	static BUInputDevice *	FindDevice(const char *name);

	static int32			CountDevices();
	static BUInputDevice *	DeviceAt(int32 index);

							~BUInputDevice();

	int32					CountReports() { return fInputReports; };
	BUInputReport *			ReportAt(int32 index);

	BUInputItem *			FindItem(uint32 usage);

private:
							BUInputDevice(void *device, const char *name,
								uint32 usage, int32 inputReports);

	void *					fDevice;
	char *					fName;
	uint32					fUsage;
	int32					fInputReports;
};


BUInputDevice *
BUInputDevice::FindDevice(const char *name)
{
	BMessage command(IS_UIS_MESSAGE), reply;

	command.AddInt32("opcode", B_UIS_FIND_DEVICE);
	command.AddString("name", name);

	printf("asking for device: %s\n", name);
	if (_control_input_server_(&command, &reply) != B_OK)
		return NULL;

	uint32 usage;
	int32 inputReports;
	void *device;
	if (reply.FindInt32("usage", (int32 *) &usage) != B_OK
		|| reply.FindInt32("input reports", &inputReports) != B_OK
		|| reply.FindPointer("device", &device) != B_OK)
		return NULL;

	return new (std::nothrow) BUInputDevice(device, name, usage, inputReports);
}


int32
CountDevices()
{
	return 0;
}


BUInputDevice *
BUInputDevice::DeviceAt(int32 index)
{
	return NULL;
}


BUInputDevice::BUInputDevice(void *device, const char *name, uint32 usage,
	int32 inputReports)
	:
	fDevice(device),
	fName(strdup(name)),
	fUsage(usage),
	fInputReports(inputReports)
{
	printf("created device with device: %08x, name: %s, usage: %08x, input"
		" reports: %d\n", fDevice, fName, fUsage, fInputReports);
}


BUInputDevice::~BUInputDevice()
{
	free(fName);
}


BUInputReport *
BUInputDevice::ReportAt(int32 index)
{
	BMessage command(IS_UIS_MESSAGE), reply;

	command.AddInt32("opcode", B_UIS_GET_REPORT);
	command.AddPointer("device", fDevice);
	command.AddInt32("index", index);

	if (_control_input_server_(&command, &reply) != B_OK)
		return NULL;

	int32 items;
	void *report;
	if (reply.FindInt32("items", &items) != B_OK
		|| reply.FindPointer("report", &report) != B_OK)
		return NULL;

	return new (std::nothrow) BUInputReport(fDevice, report, items);
}


BUInputItem *
BUInputDevice::FindItem(uint32 usage)
{
	BMessage command(IS_UIS_MESSAGE), reply;

	command.AddInt32("opcode", B_UIS_FIND_ITEM);
	command.AddPointer("device", fDevice);
	command.AddInt32("usage", (int32) usage);

	if (_control_input_server_(&command, &reply) != B_OK)
		return NULL;

	uint16 id;
	void *report, *item;
	if (reply.FindPointer("report", &report) != B_OK
		|| reply.FindPointer("item", &item) != B_OK
		|| reply.FindInt16("id", (int16 *) &id) != B_OK)
		return NULL;

	return new (std::nothrow) BUInputItem(fDevice, report, item, id);
}


//	#pragma mark -


class Window : public BWindow {
	public:
						Window();
						~Window();

		void			MessageReceived(BMessage *message);
		virtual bool	QuitRequested();
	private:
		BUInputItem *	fItem1;
};


Window::Window()
	: BWindow(BRect(100, 100, 520, 430), "UIDevice Test",
			B_TITLED_WINDOW, B_ASYNCHRONOUS_CONTROLS),
	fItem1(NULL)
{
	BUInputDevice *device = BUInputDevice::FindDevice("Power Shock");
	if (device) {
		/*BUInputReport *report = device->ReportAt(0);
		if (report) {
			fItem1 = report->ItemAt(6);
			fItem1->SetTarget(this);
			delete report;
		}*/
		fItem1 = device->FindItem(HID_USAGE_PAGE_GENERIC_DESKTOP << 16
			| HID_USAGE_ID_X);
		fItem1->SetTarget(this);
		delete device;
	}
}


Window::~Window()
{
	delete fItem1;
}


bool
Window::QuitRequested()
{
	be_app->PostMessage(B_QUIT_REQUESTED);
	return true;
}


void
Window::MessageReceived(BMessage *message)
{
	switch (message->what) {
		case B_UIS_ITEM_EVENT:
			{
				BUInputItem *item;
				uint32 value;
				if (message->FindPointer("cookie", (void **) &item) != B_OK
					|| message->FindInt32("value", (int32 *) &value) != B_OK)
					break;
				printf("item: %08x, value: %08x\n", item, value);
				break;
			}
	}
}


//	#pragma mark -


class Application : public BApplication {
	public:
		Application();

		virtual void ReadyToRun(void);
};


Application::Application()
	: BApplication("application/x-vnd.UISeviceTest")
{
}


void
Application::ReadyToRun(void)
{
	BWindow *window = new Window();
	window->Show();
}


//	#pragma mark -


int 
main(int argc, char **argv)
{
	Application app;

	app.Run();
	return 0;
}
