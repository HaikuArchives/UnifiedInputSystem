#include <UISKit.h>

#include <Looper.h>
#include <Message.h>

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


//	#pragma mark - BUISItem


BUISItem::BUISItem(uis_device_id device, int32 report, int32 item,
	uint16 usagePage, uint16 usageId)
	:
	fDevice(device),
	fReport(report),
	fItem(item),
	fTarget(NULL),
	fUsagePage(usagePage),
	fUsageId(usageId)
{
}


BUISItem::~BUISItem()
{
	if (fTarget != NULL)
		SetTarget(NULL);
}


status_t
BUISItem::SetTarget(BLooper *looper)
{
	return SetTarget(looper, this);
}


status_t
BUISItem::SetTarget(BLooper *looper, void *cookie)
{
	BMessage command(IS_UIS_MESSAGE), reply;
	command.AddInt32("opcode", B_UIS_ITEM_SET_TARGET);
	command.AddInt32("device", fDevice);
	command.AddInt32("report", fReport);
	command.AddInt32("item", fItem);

	if (looper != NULL) {
		command.AddInt32("team id", (int32) looper->Team());
		command.AddInt32("looper port", (int32) _get_looper_port_(looper));
		command.AddInt32("object token", _get_object_token_(looper));
		command.AddPointer("cookie", cookie);
	}

	command.AddPointer("target", fTarget);
	status_t status = _control_input_server_(&command, &reply);
	if (status != B_OK)
		return status;
	return reply.FindPointer("target", &fTarget);
}


//	#pragma mark - BUISReport


BUISReport::BUISReport(uis_device_id device, int32 report, int32 items)
	:
	fDevice(device),
	fReport(report),
	fItems(items)
{
}


BUISItem *
BUISReport::ItemAt(int32 index)
{
	BMessage command(IS_UIS_MESSAGE), reply;

	command.AddInt32("opcode", B_UIS_GET_ITEM);
	command.AddInt32("device", fDevice);
	command.AddInt32("report", fReport);
	command.AddInt32("index", index);

	if (_control_input_server_(&command, &reply) != B_OK)
		return NULL;

	uint16 page, id;
	if (reply.FindInt16("page", (int16 *) &page) != B_OK
			|| reply.FindInt16("id", (int16 *) &id) != B_OK)
		return NULL;

	return new (std::nothrow) BUISItem(fDevice, fReport, index, page, id);
}


//	#pragma mark - BUISDevice


BUISDevice::BUISDevice(uis_device_id device)
	:
	fDevice(device),
	fName(NULL),
	fUsage(0),
	fInputReports(0),
	fStatus(B_NO_INIT)
{
	BMessage command(IS_UIS_MESSAGE), reply;

	command.AddInt32("opcode", B_UIS_GET_DEVICE);
	command.AddInt32("device", device);

	if (_control_input_server_(&command, &reply) != B_OK)
		return;

	const char *name, *path;
	if (reply.FindString("name", &name) != B_OK
			|| reply.FindString("path", &path) != B_OK
			|| reply.FindInt32("usage", (int32 *) &fUsage) != B_OK
			|| reply.FindInt32("input reports", &fInputReports) != B_OK)
		return;
	fName = strdup(name);
	fPath = strdup(path);

	fStatus = B_OK;
}


BUISDevice::~BUISDevice()
{
	free(fName);
}


BUISReport *
BUISDevice::ReportAt(int32 index)
{
	BMessage command(IS_UIS_MESSAGE), reply;

	command.AddInt32("opcode", B_UIS_GET_REPORT);
	command.AddInt32("device", fDevice);
	command.AddInt32("report", index);

	if (_control_input_server_(&command, &reply) != B_OK)
		return NULL;

	int32 items;
	if (reply.FindInt32("items", &items) != B_OK)
		return NULL;

	return new (std::nothrow) BUISReport(fDevice, index, items);
}


BUISItem *
BUISDevice::FindItem(uint32 usage)
{
	BMessage command(IS_UIS_MESSAGE), reply;

	command.AddInt32("opcode", B_UIS_FIND_ITEM);
	command.AddInt32("device", fDevice);
	command.AddInt32("usage", (int32) usage);

	if (_control_input_server_(&command, &reply) != B_OK)
		return NULL;

	int32 report, item;
	uint16 page, id;
	if (reply.FindInt32("report", &report) != B_OK
			|| reply.FindInt32("item", &item) != B_OK
			|| reply.FindInt16("page", (int16 *) &page) != B_OK
			|| reply.FindInt16("id", (int16 *) &id) != B_OK)
		return NULL;

	return new (std::nothrow) BUISItem(fDevice, report, item, page, id);
}


//	#pragma mark - BUISDevice


BUISDevice *
BUISRoster::FindByName(const char *name)
{
	return NULL;
}


BUISRoster::BUISRoster()
	:
	fCookie(0),
	fTarget(NULL)
{
}


BUISRoster::~BUISRoster()
{
	StopWatching();
}


status_t
BUISRoster::GetNextDevice(BUISDevice **device)
{
	if (device == NULL)
		return B_BAD_VALUE;

	BMessage command(IS_UIS_MESSAGE), reply;

	command.AddInt32("opcode", B_UIS_NEXT_DEVICE);
	command.AddInt32("device", fCookie);

	status_t status = _control_input_server_(&command, &reply);
	if (status != B_OK)
		return status;

	uis_device_id next;
	status = reply.FindInt32("next", &next);
	if (status != B_OK)
		return status;

	*device = new (std::nothrow) BUISDevice(next);
	if (*device == NULL)
		return B_ERROR;
	status = (*device)->InitCheck();
	if (status != B_OK)
		return status;

	fCookie = next;
	return B_OK;
}


void
BUISRoster::Rewind()
{
	fCookie = 0;
}


status_t
BUISRoster::StartWatching(BLooper *looper)
{
	return B_OK;
}


void
BUISRoster::StopWatching()
{
}