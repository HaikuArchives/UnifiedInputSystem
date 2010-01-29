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
		case B_UIS_GET_DEVICES:
			{
				BAutolock lock(fUISDeviceListLocker);
				if (lock.IsLocked()) {
					UISDevice *device;
					for (int32 i = 0; i < fUISDeviceList.CountItems(); i++) {
						device = (UISDevice *) fUISDeviceList.ItemAt(i);

						BMessage msg('Ihdf');
						status = msg.AddInt32("usage", device->Usage());
						if (status != B_OK)
							break;
						status = msg.AddInt8("input reports",
							device->CountReports(0));
						if (status != B_OK)
							break;
						status = msg.AddString("name", device->Name());
						if (status != B_OK)
							break;
						status = msg.AddPointer("device", device);
						if (status != B_OK)
							break;

						status = reply->AddMessage("uis device", &msg);
						if (status != B_OK)
							break;
					}
				}
				break;
			}

		case B_UIS_FIND_DEVICE:
			{
				const char *name;
				if (message->FindString("name", &name) != B_OK)
					break;

				BAutolock lock(fUISDeviceListLocker);
				if (lock.IsLocked()) {
					UISDevice *device;
					for (int32 i = 0; i < fUISDeviceList.CountItems(); i++) {
						device = (UISDevice *) fUISDeviceList.ItemAt(i);
						//TRACE("cmp %s, %s\n", name, device->Name());
						if (device->HasName(name)) {
							status = reply->AddInt32("usage",
								device->Usage());
							if (status != B_OK)
								break;
							status = reply->AddInt8("input reports",
								device->CountReports(0));
							if (status != B_OK)
								break;
							return reply->AddPointer("device", device);
						}
					}
				}
				break;
			}

		case B_UIS_GET_REPORT:
			{
				UISDevice *device;
				uint8 index;
				if (message->FindPointer("device", (void **) &device) != B_OK
						|| message->FindInt8("index", (int8 *) &index) != B_OK)
					break;

				BAutolock lock(fUISDeviceListLocker);
				if (lock.IsLocked() && fUISDeviceList.HasItem(device)) {
					UISReport *report = device->ReportAt(0, index);
					if (report != NULL) {
						status = reply->AddInt32("items", report->CountItems());
						if (status != B_OK)
							break;
						return reply->AddPointer("report", report);
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
						status = reply->AddInt16("id", item->Id());
						if (status != B_OK)
							break;
						return reply->AddPointer("item", item);
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

				team_id team = -1;
				port_id port = -1;
				int32 token = B_NULL_TOKEN;
				void *cookie = NULL;
				message->FindInt32("team id", (int32 *) &team);
				message->FindInt32("looper port", (int32 *) &port);
				message->FindInt32("object token", &token);
				message->FindPointer("cookie", &cookie);

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
		if (status == B_BAD_PORT_ID)
			RemoveTarget(target);

		return status;		
	}

	return B_ERROR;
}
