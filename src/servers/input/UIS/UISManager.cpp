#include "UISManager.h"
#include "UISDevice.h"
#include "UISReport.h"
#include "UISItem.h"
#include "UISTarget.h"

#include <Autolock.h>

#include <PathMonitor.h>
#include <uis_driver.h>
#include <UISProtocol.h>

#include <new>

using std::nothrow;

#include "UIS_debug.h"


static const char kMonitoredPath[] = "/dev/input/hid";


UISManager::UISManager()
	: BLooper("uis manager"),
	fIsRunning(false)
{
}


UISManager::~UISManager()
{
	Stop();

	BAutolock lock(fUISDeviceListLocker);
	if (lock.IsLocked())
		fUISDeviceList.DoForEach(_RemoveDeviceListItem);
}


bool
UISManager::_RemoveDeviceListItem(void *arg)
{
	delete (UISDevice *) arg;
	return false;
}


status_t
UISManager::Start()
{
	if (fIsRunning)
		return B_OK;

	status_t status = BPathMonitor::StartWatching(kMonitoredPath,
		B_ENTRY_CREATED | B_ENTRY_REMOVED | B_WATCH_FILES_ONLY
			| B_WATCH_RECURSIVELY, this);
	TRACE("start watching status: %d\n", status);
	fIsRunning = (status == B_OK);
	return status;
}


void
UISManager::Stop()
{
	if (fIsRunning) {
		BPathMonitor::StopWatching(kMonitoredPath, this);
		fIsRunning = false;
	}
}


void
UISManager::MessageReceived(BMessage *message)
{
	switch (message->what) {
		case B_PATH_MONITOR:
			_HandleAddRemoveDevice(message);
			break;
	}
}


void
UISManager::_HandleAddRemoveDevice(BMessage *message)
{
	int32 opcode;
	if (message->FindInt32("opcode", &opcode) != B_OK)
		return;

	const char *path;
	if (message->FindString("path", &path) != B_OK) {
		TRACE("B_PATH_MONITOR message without path\n");
		return;
	}

	if (opcode == B_ENTRY_CREATED) {
		TRACE("please create %s\n", path);
		UISDevice *device = new (std::nothrow) UISDevice(this, path);
		if (device == NULL)
			return;
		BAutolock lock(fUISDeviceListLocker);
		if (lock.IsLocked())
			fUISDeviceList.AddItem(device);
	} else if (opcode == B_ENTRY_REMOVED) {
		TRACE("please delete %s\n", path);
		BAutolock lock(fUISDeviceListLocker);
		if (lock.IsLocked()) {
			UISDevice *device;
			for (int32 i = 0; i < fUISDeviceList.CountItems(); i++) {
				device = (UISDevice *) fUISDeviceList.ItemAt(i);
				if (device->HasPath(path)) {
					fUISDeviceList.RemoveItem(i);
					delete device;
					break;
				}
			}
		}
	}
}


status_t
UISManager::HandleMessage(BMessage *message, BMessage *reply)
{
	status_t status = B_ERROR;

	int32 opcode;
	if (message->FindInt32("opcode", &opcode) != B_OK)
		return status;

	switch (opcode) {
		case B_UIS_FIND_DEVICE:
			{
				const char *name = NULL, *path = NULL;
				if (message->FindString("name", &name) != B_OK
						&& message->FindString("path", &path) != B_OK)
					break;

				BAutolock lock(fUISDeviceListLocker);
				if (lock.IsLocked()) {
					UISDevice *device;
					for (int32 i = 0; i < fUISDeviceList.CountItems(); i++) {
						device = (UISDevice *) fUISDeviceList.ItemAt(i);
						if ((name != NULL && device->HasName(name))
								|| (path != NULL && device->HasPath(path))) {
							status = reply->AddPointer("device", device);
							if (status != B_OK)
								break;
							status = reply->AddString("name", device->Name());
							if (status != B_OK)
								break;
							status = reply->AddInt32("usage",
								device->Usage());
							if (status != B_OK)
								break;
							return reply->AddInt32("input reports",
								device->CountReports(0));
						}
					}
				}
				break;
			}

		case B_UIS_COUNT_DEVICES:
			{
				BAutolock lock(fUISDeviceListLocker);
				if (lock.IsLocked())
					return reply->AddInt32("devices",
						fUISDeviceList.CountItems());
			}

		case B_UIS_GET_DEVICE:
			{
				int32 index;
				if (message->FindInt32("index", &index) != B_OK)
					break;

				BAutolock lock(fUISDeviceListLocker);
				if (lock.IsLocked()) {
					UISDevice *device =
						(UISDevice *) fUISDeviceList.ItemAt(index);
					if (device) {
						status = reply->AddPointer("device", device);
						if (status != B_OK)
							break;
						status = reply->AddString("name", device->Name());
						if (status != B_OK)
							break;
						status = reply->AddInt32("usage", device->Usage());
						if (status != B_OK)
							break;
						return reply->AddInt32("input reports",
							device->CountReports(0));
					}
				}
				break;
			}

		case B_UIS_GET_DEVICE_PATH:
			{
				int32 index;
				if (message->FindInt32("index", &index) != B_OK)
					break;

				BAutolock lock(fUISDeviceListLocker);
				if (lock.IsLocked()) {
					UISDevice *device =
						(UISDevice *) fUISDeviceList.ItemAt(index);
					if (device)
						return reply->AddString("path", device->Path());
					return B_BAD_INDEX;
				}
				break;
			}

		case B_UIS_GET_REPORT:
			{
				UISDevice *device;
				int32 index;
				if (message->FindPointer("device", (void **) &device) != B_OK
						|| message->FindInt32("index", &index) != B_OK)
					break;

				BAutolock lock(fUISDeviceListLocker);
				if (lock.IsLocked() && fUISDeviceList.HasItem(device)) {
					UISReport *report = device->ReportAt(0, index);
					if (report != NULL) {
						status = reply->AddPointer("report", report);
						if (status != B_OK)
							break;
						return reply->AddInt32("items", report->CountItems());
					}
				}
			}

		case B_UIS_GET_ITEM:
			{
				UISDevice *device;
				UISReport *report;
				int32 index;
				if (message->FindPointer("device", (void **) &device) != B_OK
					|| message->FindPointer("report", (void **) &report) != B_OK
					|| message->FindInt32("index", &index) != B_OK)
					break;

				BAutolock lock(fUISDeviceListLocker);
				if (lock.IsLocked() && fUISDeviceList.HasItem(device)) {
					UISReportItem *item = report->ItemAt(index);
					if (item != NULL) {
						status = reply->AddPointer("item", item);
						if (status != B_OK)
							break;
						status = reply->AddInt16("page", item->UsagePage());
						if (status != B_OK)
							break;
						status = reply->AddInt16("id", item->UsageId());
						if (status != B_OK)
							break;
						status = reply->AddInt32("min", item->Minimum());
						if (status != B_OK)
							break;
						return reply->AddInt32("max", item->Maximum());
					}
				}
			}

		case B_UIS_FIND_ITEM:
			{
				UISDevice *device;
				uint32 usage;
				if (message->FindPointer("device", (void **) &device) != B_OK
					|| message->FindInt32("usage", (int32 *) &usage) != B_OK)
					break;

				BAutolock lock(fUISDeviceListLocker);
				if (lock.IsLocked() && fUISDeviceList.HasItem(device)) {
					uint16 usagePage = usage >> 16;
					uint16 usageId = usage & 0xffff;

					for (int32 ir = 0; ir < device->CountReports(0); ir++) {
						UISReport *report = device->ReportAt(0, ir);
						for (int32 ii = 0; ii < report->CountItems(); ii++) {
							UISReportItem *item = report->ItemAt(ii);
							if (item->UsagePage() == usagePage
									&& item->UsageId() == usageId) {
								//TRACE("found item\n");
								status = reply->AddPointer("report", report);
								if (status != B_OK)
									break;
								status = reply->AddPointer("item", item);
								if (status != B_OK)
									break;
								status = reply->AddInt16("page",
									item->UsagePage());
								if (status != B_OK)
									break;
								status = reply->AddInt16("id", item->UsageId());
								if (status != B_OK)
									break;
								status = reply->AddInt32("min",
									item->Minimum());
								if (status != B_OK)
									break;
								return reply->AddInt32("max", item->Maximum());
							}
						}
					}
				}
			}

		case B_UIS_ITEM_SET_TARGET:
			{
				UISDevice *device;
				UISReportItem *item;
				void *target;
				if (message->FindPointer("device", (void **) &device) != B_OK
					|| message->FindPointer("item", (void **) &item) != B_OK
					|| message->FindPointer("target", &target) != B_OK)
					break;

				team_id team;
				if (message->FindInt32("team id", (int32 *) &team) != B_OK)
					team = -1;
				port_id port;
				if (message->FindInt32("looper port", (int32 *) &port) != B_OK)
					port = -1;
				int32 token;
				if (message->FindInt32("object token", &token) != B_OK)
					token = B_NULL_TOKEN;
				void *cookie;
				if (message->FindPointer("cookie", &cookie) != B_OK)
					cookie = NULL;

				BAutolock lock(fUISDeviceListLocker);
				if (lock.IsLocked() && fUISDeviceList.HasItem(device)) {
					item->SetTarget(team, port, token, cookie, &target);
					return reply->AddPointer("target", target);
				}
			}
	}

	return status;
}


void
UISManager::RemoveDevice(UISDevice *device)
{
	BAutolock lock(fUISDeviceListLocker);
	if (lock.IsLocked())
		if (fUISDeviceList.RemoveItem(device))
			delete device;
}


UISTarget *
UISManager::FindOrAddTarget(team_id team, port_id port, int32 token)
{
	BAutolock lock(fTargetListLocker);
	if (!lock.IsLocked())
		return NULL;

	UISTarget *target;

	for (int32 i = 0; i < fTargetList.CountItems(); i++) {
		target = (UISTarget *) fTargetList.ItemAt(i);
		if (target->HasTarget(port, token)) {
			target->IncRef();
			return target;
		}
	}

	target = new (std::nothrow) UISTarget(team, port, token);
	if (target != NULL)
		fTargetList.AddItem(target);

	return target;
}


void
UISManager::RemoveTarget(UISTarget *target)
{
	BAutolock lock(fTargetListLocker);
	if (lock.IsLocked() && fTargetList.HasItem(target)) {
		if (target->DecRef() == 0) {
			fTargetList.RemoveItem(target);
			delete target;
		}
	}
}


status_t
UISManager::SendEvent(uis_item_target *itemTarget, UISReportItem *item)
{
	UISTarget *target = itemTarget->target;

	BAutolock lock(fTargetListLocker);
	if (lock.IsLocked() && fTargetList.HasItem(target)) {
		BMessage message(B_UIS_ITEM_EVENT);
		message.AddPointer("cookie", itemTarget->cookie);
		message.AddInt32("value", (int32) item->Value());

		status_t status = target->SendMessage(&message);
		//TRACE("send msg status = %08x\n", status);
		if (status == B_BAD_PORT_ID)
			RemoveTarget(target);

		return status;		
	}

	return B_ERROR;
}
