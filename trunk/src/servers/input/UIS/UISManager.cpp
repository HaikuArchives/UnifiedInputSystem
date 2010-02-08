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
	fIsRunning(false),
	fNextDeviceId(1)
{
}


UISManager::~UISManager()
{
	Stop();

	BAutolock lock(fDeviceMapLock);
	if (lock.IsLocked()) {
		for (DeviceMap::iterator it = fDeviceMap.begin();
				it != fDeviceMap.end(); it++)
			delete it->second;
	}
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
		UISDevice *device = new (std::nothrow) UISDevice(fNextDeviceId, this,
			path);
		if (device == NULL)
			return;
		BAutolock lock(fDeviceMapLock);
		if (lock.IsLocked()) {
			try {
				fDeviceMap.insert(std::make_pair(fNextDeviceId, device));
			} catch (...) {
				return;
			}
			fNextDeviceId++;
		}
	} else if (opcode == B_ENTRY_REMOVED) {
		TRACE("please delete %s\n", path);
		BAutolock lock(fDeviceMapLock);
		if (lock.IsLocked()) {
			for (DeviceMap::iterator it = fDeviceMap.begin();
					it != fDeviceMap.end(); it++) {
				UISDevice *device = it->second;
				if (device->HasPath(path)) {
					fDeviceMap.erase(it);
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
		case B_UIS_NEXT_DEVICE:
			{
				uis_device_id id;
				if (message->FindInt32("device", &id) != B_OK)
					break;

				BAutolock lock(fDeviceMapLock);
				if (lock.IsLocked()) {
					DeviceMap::iterator found = fDeviceMap.upper_bound(id);
					if (found == fDeviceMap.end())
						return B_BAD_VALUE;
					return reply->AddInt32("next", found->first);
				}
				break;
			}

		case B_UIS_FIND_DEVICE:
			{
				const char *name = NULL, *path = NULL;
				if (message->FindString("name", &name) != B_OK
						&& message->FindString("path", &path) != B_OK)
					break;

				BAutolock lock(fDeviceMapLock);
				if (lock.IsLocked()) {
					for (DeviceMap::iterator it = fDeviceMap.begin();
							it != fDeviceMap.end(); it++) {
						UISDevice *device = it->second;
						if ((name != NULL && device->HasName(name))
								|| (path != NULL && device->HasPath(path)))
							return reply->AddInt32("device", it->first);
					}
				}
				break;
			}

		case B_UIS_COUNT_DEVICES:
			{
				BAutolock lock(fDeviceMapLock);
				if (lock.IsLocked())
					return reply->AddInt32("devices", fDeviceMap.size());
			}

		case B_UIS_GET_DEVICE:
			{
				uis_device_id id;
				if (message->FindInt32("device", &id) != B_OK)
					break;

				BAutolock lock(fDeviceMapLock);
				if (lock.IsLocked()) {
					UISDevice *device = _Device(id);
					if (device == NULL)
						break;

					status = reply->AddString("name", device->Name());
					if (status != B_OK)
						break;
					status = reply->AddString("path", device->Path());
					if (status != B_OK)
						break;
					status = reply->AddInt32("usage", device->Usage());
					if (status != B_OK)
						break;
					return reply->AddInt32("input reports",
						device->CountReports(0));
				}
				break;
			}

		case B_UIS_GET_REPORT:
			{
				uis_device_id id;
				int32 index;
				if (message->FindInt32("device", &id) != B_OK
						|| message->FindInt32("report", &index) != B_OK)
					break;

				BAutolock lock(fDeviceMapLock);
				if (lock.IsLocked()) {
					UISDevice *device = _Device(id);
					if (device == NULL)
						break;

					UISReport *report = device->ReportAt(0, index);
					if (report != NULL) {
						return reply->AddInt32("items", report->CountItems());
					}
				}
			}

		case B_UIS_GET_ITEM:
			{
				uis_device_id id;
				int32 report, index;
				if (message->FindInt32("device", &id) != B_OK
						|| message->FindInt32("report", &report) != B_OK
						|| message->FindInt32("item", &index) != B_OK)
					break;

				BAutolock lock(fDeviceMapLock);
				if (lock.IsLocked()) {
					UISDevice *device = _Device(id);
					if (device == NULL)
						break;

					UISReport *report = device->ReportAt(0, index);

					UISReportItem *item = report->ItemAt(index);
					if (item != NULL) {
						status = reply->AddInt16("page", item->UsagePage());
						if (status != B_OK)
							break;
						return reply->AddInt16("id", item->UsageId());
					}
				}
			}

		case B_UIS_FIND_ITEM:
			{
				uis_device_id id;
				uint32 usage;
				if (message->FindInt32("device", &id) != B_OK
					|| message->FindInt32("usage", (int32 *) &usage) != B_OK)
					break;

				BAutolock lock(fDeviceMapLock);
				if (lock.IsLocked()) {
					uint16 usagePage = usage >> 16;
					uint16 usageId = usage & 0xffff;

					UISDevice *device = _Device(id);
					if (device == NULL)
						break;

					for (int32 ir = 0; ir < device->CountReports(0); ir++) {
						UISReport *report = device->ReportAt(0, ir);
						for (int32 ii = 0; ii < report->CountItems(); ii++) {
							UISReportItem *item = report->ItemAt(ii);
							if (item->UsagePage() == usagePage
									&& item->UsageId() == usageId) {
								//TRACE("found item\n");
								status = reply->AddInt32("report", ir);
								if (status != B_OK)
									break;
								status = reply->AddInt32("item", ii);
								if (status != B_OK)
									break;
								status = reply->AddInt16("page",
									item->UsagePage());
								if (status != B_OK)
									break;
								return reply->AddInt16("id", item->UsageId());
							}
						}
					}
				}
			}

		case B_UIS_ITEM_SET_TARGET:
			{
				uis_device_id id;
				int32 reportIndex, itemIndex;
				void *target;
				if (message->FindInt32("device", &id) != B_OK
						|| message->FindInt32("report", &reportIndex) != B_OK
						|| message->FindInt32("item", &itemIndex) != B_OK
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

				BAutolock lock(fDeviceMapLock);
				if (lock.IsLocked()) {
					UISDevice *device = _Device(id);
					if (device == NULL)
						break;

					UISReport *report = device->ReportAt(0, reportIndex);
					UISReportItem *item = report->ItemAt(itemIndex);
					item->SetTarget(team, port, token, cookie, &target);
					return reply->AddPointer("target", target);
				}
			}
	}

	return status;
}


void
UISManager::RemoveDevice(uis_device_id id)
{
	BAutolock lock(fDeviceMapLock);
	if (lock.IsLocked()) {
		DeviceMap::iterator found = fDeviceMap.find(id);
		if (found != fDeviceMap.end()) {
			delete found->second;
			fDeviceMap.erase(found);
		}
	}
}


UISDevice *
UISManager::_Device(uis_device_id id)
{
	DeviceMap::iterator found = fDeviceMap.find(id);
	return (found != fDeviceMap.end()) ? found->second : NULL;
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
		message.AddFloat("value", item->Value());

		status_t status = target->SendMessage(&message);
		//TRACE("send msg status = %08x\n", status);
		if (status == B_BAD_PORT_ID)
			RemoveTarget(target);

		return status;		
	}

	return B_ERROR;
}
