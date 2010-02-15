#include <UISKit.h>

#include <Looper.h>
#include <Message.h>

#include <usb/USB_hid.h>

#include <uis_driver.h>
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


BUISItem::BUISItem(uis_device_id device, int32 report, int32 item, uint8 type,
		uint16 usagePage, uint16 usageId, bool isRelative, float value)
	:
	fDevice(device),
	fReport(report),
	fItem(item),
	fTarget(NULL),
	fType(type),
	fUsagePage(usagePage),
	fUsageId(usageId),
	fIsRelative(isRelative),
	fValue(value)
{
}


BUISItem::~BUISItem()
{
	if (fTarget != NULL)
		SetTarget(NULL);
}


status_t
BUISItem::Update()
{
	BMessage command(IS_UIS_MESSAGE), reply;

	command.AddInt32("opcode", B_UIS_ITEM_POLL_VALUE);
	command.AddInt32("device", fDevice);
	command.AddInt32("report", fReport);
	command.AddInt8("type", (int8) fType);
	command.AddInt32("item", fItem);

	status_t status = _control_input_server_(&command, &reply);
	if (status != B_OK)
		return status;
	float value;
	status = reply.FindFloat("value", &value);
	if (status != B_OK)
		return status;
	fValue = value;
	return B_OK;
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
	command.AddInt8("type", (int8) fType);
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


BUISReport::BUISReport(uis_device_id device, int32 report, int32 items,
		uint8 type)
	:
	fDevice(device),
	fReport(report),
	fItems(items),
	fType(type),
	fSendMessage(NULL)
{
}


BUISItem *
BUISReport::ItemAt(int32 index)
{
	BMessage command(IS_UIS_MESSAGE), reply;

	command.AddInt32("opcode", B_UIS_GET_ITEM);
	command.AddInt32("device", fDevice);
	command.AddInt32("report", fReport);
	command.AddInt8("type", (int8) fType);
	command.AddInt32("index", index);

	if (_control_input_server_(&command, &reply) != B_OK)
		return NULL;

	uint16 page, id;
	bool relative;
	float value;
	if (reply.FindInt16("page", (int16 *) &page) != B_OK
			|| reply.FindInt16("id", (int16 *) &id) != B_OK
			|| reply.FindBool("relative", &relative) != B_OK
			|| reply.FindFloat("value", &value) != B_OK)
		return NULL;

	return new (std::nothrow) BUISItem(fDevice, fReport, index, fType, page, id,
		relative, value);
}


status_t
BUISReport::AddItemValue(int32 index, float value)
{
	if (fType != UIS_TYPE_OUTPUT && fType != UIS_TYPE_FEATURE)
		return B_ERROR;

	if (fSendMessage == NULL) {
		fSendMessage = new BMessage(IS_UIS_MESSAGE);
		if (fSendMessage == NULL)
			return B_ERROR;
		fSendMessage->AddInt32("opcode", B_UIS_SEND_REPORT);
		fSendMessage->AddInt32("device", fDevice);
		fSendMessage->AddInt32("report", fReport);
		fSendMessage->AddInt8("type", (int8) fType);
	}

	uis_item_data data;
	data.index = index;
	data.value = value;
	return fSendMessage->AddData("data", B_RAW_TYPE, &data,
		sizeof(uis_item_data));
			// Haiku's AddData implementation doesn't do preallocation
}


status_t
BUISReport::Send()
{
	if (!fSendMessage)
		return B_ERROR;

	BMessage reply;

	status_t status = _control_input_server_(fSendMessage, &reply);
	if (status == B_OK)
		MakeEmpty();
	return status;
}


void
BUISReport::MakeEmpty()
{
	delete fSendMessage;
	fSendMessage = NULL;
}


//	#pragma mark - BUISDevice


BUISDevice::BUISDevice(uis_device_id device)
	:
	fDevice(device),
	fName(NULL),
	fPath(NULL),
	fUsagePage(0),
	fUsageId(0),
	fInputReports(0),
	fOutputReports(0),
	fFeatureReports(0),
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
			|| reply.FindInt16("page", (int16 *) &fUsagePage) != B_OK
			|| reply.FindInt16("id", (int16 *) &fUsageId) != B_OK
			|| reply.FindInt32("input reports", &fInputReports) != B_OK
			|| reply.FindInt32("output reports", &fOutputReports) != B_OK
			|| reply.FindInt32("feature reports", &fFeatureReports) != B_OK)
		return;
	fName = strdup(name);
	fPath = strdup(path);

	fStatus = B_OK;
}


BUISDevice::~BUISDevice()
{
	free(fName);
	free(fPath);
}


int32
BUISDevice::CountReports(uint8 type)
{
	int32 count = 0;

	if ((type & UIS_TYPE_INPUT) != 0)
		count += fInputReports;
	if ((type & UIS_TYPE_OUTPUT) != 0)
		count += fOutputReports;
	if ((type & UIS_TYPE_FEATURE) != 0)
		count += fFeatureReports;

	return count;
}


BUISReport *
BUISDevice::ReportAt(uint8 type, int32 index)
{
	BMessage command(IS_UIS_MESSAGE), reply;

	command.AddInt32("opcode", B_UIS_GET_REPORT);
	command.AddInt32("device", fDevice);
	command.AddInt32("report", index);
	command.AddInt8("type", (int8) type);

	if (_control_input_server_(&command, &reply) != B_OK)
		return NULL;

	int32 items;
	if (reply.FindInt32("items", &items) != B_OK
			|| reply.FindInt8("type", (int8 *) &type) != B_OK)
		return NULL;

	return new (std::nothrow) BUISReport(fDevice, index, items, type);
}


BUISItem *
BUISDevice::FindItem(uint16 usagePage, uint16 usageId)
{
	BMessage command(IS_UIS_MESSAGE), reply;

	command.AddInt32("opcode", B_UIS_FIND_ITEM);
	command.AddInt32("device", fDevice);
	command.AddInt16("page", (int16) usagePage);
	command.AddInt16("id", (int16) usageId);

	if (_control_input_server_(&command, &reply) != B_OK)
		return NULL;

	int32 report, item;
	uint16 page, id;
	bool relative;
	float value;
	if (reply.FindInt32("report", &report) != B_OK
			|| reply.FindInt32("item", &item) != B_OK
			|| reply.FindInt16("page", (int16 *) &page) != B_OK
			|| reply.FindInt16("id", (int16 *) &id) != B_OK
			|| reply.FindBool("relative", &relative) != B_OK
			|| reply.FindFloat("value", &value) != B_OK)
		return NULL;

	return new (std::nothrow) BUISItem(fDevice, report, item, UIS_TYPE_INPUT,
		page, id, relative, value);
			// found item is always of type input
}


//	#pragma mark - BUISDevice


BUISDevice *
BUISRoster::FindByName(const char *name)
{
	BMessage command(IS_UIS_MESSAGE), reply;

	command.AddInt32("opcode", B_UIS_FIND_DEVICE);
	command.AddString("name", name);

	if (_control_input_server_(&command, &reply) != B_OK)
		return NULL;

	uis_device_id deviceId;
	if (reply.FindInt32("device", &deviceId) != B_OK)
		return NULL;

	BUISDevice *device = new (std::nothrow) BUISDevice(deviceId);
	if (device != NULL && device->InitCheck() != B_OK) {
		delete device;
		device = NULL;
	}
	return device;
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
