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


//	#pragma mark - BUISRoster


BUISDevice*
BUISRoster::FindByName(const char* name)
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
BUISRoster::GetNextDevice(BUISDevice** device)
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
BUISRoster::StartWatching(BLooper* looper)
{
	return B_OK;
}


void
BUISRoster::StopWatching()
{
}


//	#pragma mark - BUISDevice


BUISDevice::BUISDevice(uis_device_id device)
	:
	fDevice(device),
	fName(NULL),
	fPath(NULL),
	fUsagePage(0),
	fUsageId(0),
	fStatus(B_NO_INIT)
{
	fReports[UIS_REPORT_TYPE_INPUT] = 0;
	fReports[UIS_REPORT_TYPE_OUTPUT] = 0;
	fReports[UIS_REPORT_TYPE_FEATURE] = 0;

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
			|| reply.FindInt32("input reports",
				&fReports[UIS_REPORT_TYPE_INPUT]) != B_OK
			|| reply.FindInt32("output reports",
				&fReports[UIS_REPORT_TYPE_OUTPUT]) != B_OK
			|| reply.FindInt32("feature reports",
				&fReports[UIS_REPORT_TYPE_FEATURE]) != B_OK)
		return;
	fName = strdup(name);
	fPath = strdup(path);

	fStatus = B_OK;
}


BUISDevice::~BUISDevice()
{
	for (uint8 type = 0; type < UIS_REPORT_TYPES; type++)
		for (ReportMap::iterator it = fReportMap[type].begin();
				it != fReportMap[type].end(); it++)
			delete it->second;

	free(fName);
	free(fPath);
}


int32
BUISDevice::CountReports(uint8 type) const
{
	return type < UIS_REPORT_TYPES ? fReports[type] : -1;
}


BUISReport*
BUISDevice::ReportAt(uint8 type, int32 index)
{
	if (index < 0 || index >= CountReports(type))
		return NULL;
	ReportMap::iterator found = fReportMap[type].find(index);
	if (found != fReportMap[type].end())
		return found->second;

	BMessage command(IS_UIS_MESSAGE), reply;

	command.AddInt32("opcode", B_UIS_GET_REPORT);
	command.AddInt32("device", fDevice);
	command.AddInt32("report", index);
	command.AddInt8("type", (int8) type);

	if (_control_input_server_(&command, &reply) != B_OK)
		return NULL;

	int32 items;
	if (reply.FindInt32("items", &items) != B_OK)
		return NULL;

	BUISReport* report = new (std::nothrow)
		BUISReport(this, type, index, items);
	if (report) {
		try {
			fReportMap[type].insert(std::make_pair(index, report));
		} catch (...) {
			delete report;
			report = NULL;
		}
	}

	return report;
}


BUISItem*
BUISDevice::FindItem(uint16 usagePage, uint16 usageId, uint8 type)
{
	BMessage command(IS_UIS_MESSAGE), reply;

	command.AddInt32("opcode", B_UIS_FIND_ITEM);
	command.AddInt32("device", fDevice);
	command.AddInt8("type", (int8) type);
	command.AddInt16("page", (int16) usagePage);
	command.AddInt16("id", (int16) usageId);

	if (_control_input_server_(&command, &reply) != B_OK)
		return NULL;

	int32 reportIndex, itemIndex;
	if (reply.FindInt32("report", &reportIndex) != B_OK
			|| reply.FindInt32("item", &itemIndex) != B_OK)
		return NULL;

	BUISReport* report = ReportAt(type, reportIndex);
	if (report == NULL)
		return NULL;

	return report->ItemAt(itemIndex);
}


//	#pragma mark - BUISReport


BUISReport::BUISReport(BUISDevice* device, uint8 type, int32 index,	int32 items)
	:
	fDevice(device),
	fType(type),
	fIndex(index),
	fItems(items),
	fSendMessage(NULL)
{
}


BUISReport::~BUISReport()
{
	for (ItemMap::iterator it = fItemMap.begin(); it != fItemMap.end(); it++)
			delete it->second;
}


BUISItem*
BUISReport::ItemAt(int32 index)
{
	if (index < 0 || index >= fItems)
		return NULL;
	ItemMap::iterator found = fItemMap.find(index);
	if (found != fItemMap.end())
		return found->second;

	BMessage command(IS_UIS_MESSAGE), reply;

	command.AddInt32("opcode", B_UIS_GET_ITEM);
	command.AddInt32("device", fDevice->Device());
	command.AddInt8("type", (int8) fType);
	command.AddInt32("report", fIndex);
	command.AddInt32("item", index);

	if (_control_input_server_(&command, &reply) != B_OK)
		return NULL;

	uint16 page, id;
	bool relative;
	if (reply.FindInt16("page", (int16 *) &page) != B_OK
			|| reply.FindInt16("id", (int16 *) &id) != B_OK
			|| reply.FindBool("relative", &relative) != B_OK)
		return NULL;

	BUISItem* item = new (std::nothrow)
		BUISItem(this, index, page, id, relative);
	if (item) {
		try {
			fItemMap.insert(std::make_pair(index, item));
		} catch (...) {
			delete item;
			item = NULL;
		}
	}

	return item;
}


status_t
BUISReport::SetItemValue(int32 index, float value)
{
	if (fType != UIS_TYPE_OUTPUT && fType != UIS_TYPE_FEATURE)
		return B_ERROR;

	if (fSendMessage == NULL) {
		fSendMessage = new BMessage(IS_UIS_MESSAGE);
		if (fSendMessage == NULL)
			return B_ERROR;
		fSendMessage->AddInt32("opcode", B_UIS_SEND_REPORT);
		fSendMessage->AddInt32("device", fDevice->Device());
		fSendMessage->AddInt8("type", (int8) fType);
		fSendMessage->AddInt32("report", fIndex);
	}

	int32 count = 0;
	fSendMessage->GetInfo("data", NULL, &count);
	for (int32 i = 0; i < count; i++) {
		const void *msgData;
		ssize_t bytes;
		if (fSendMessage->FindData("data", B_RAW_TYPE, i, &msgData, &bytes)
				!= B_OK)
			continue;
		if (bytes != sizeof(uis_item_data))
			continue;
		if (((uis_item_data*) msgData)->index == index) {
			uis_item_data data;
			data.index = index;
			data.value = value;
			return fSendMessage->ReplaceData("data", B_RAW_TYPE, i, &data,
				sizeof(uis_item_data));
		}
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


//	#pragma mark - BUISItem


BUISItem::BUISItem(BUISReport* report, int32 index, uint16 usagePage,
		uint16 usageId, bool isRelative)
	:
	fReport(report),
	fIndex(index),
	fUsagePage(usagePage),
	fUsageId(usageId),
	fIsRelative(isRelative),
	fTarget(NULL)
{
}


BUISItem::~BUISItem()
{
	if (fTarget != NULL)
		SetTarget(NULL);
}


status_t
BUISItem::Value(float& value)
{
	BMessage command(IS_UIS_MESSAGE), reply;

	command.AddInt32("opcode", B_UIS_ITEM_POLL_VALUE);
	command.AddInt32("device", fReport->Device()->Device());
	command.AddInt8("type", (int8) Type());
	command.AddInt32("report", fReport->Index());
	command.AddInt32("item", fIndex);

	status_t status = _control_input_server_(&command, &reply);
	if (status != B_OK)
		return status;
	return reply.FindFloat("value", &value);
}


status_t
BUISItem::SetValue(float value)
{
	return fReport->SetItemValue(fIndex, value);
}


status_t
BUISItem::SetTarget(BLooper* looper)
{
	return SetTarget(looper, this);
}


status_t
BUISItem::SetTarget(BLooper* looper, void* cookie)
{
	BMessage command(IS_UIS_MESSAGE), reply;

	command.AddInt32("opcode", B_UIS_ITEM_SET_TARGET);
	command.AddInt32("device", fReport->Device()->Device());
	command.AddInt8("type", (int8) Type());
	command.AddInt32("report", fReport->Index());
	command.AddInt32("item", fIndex);

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
