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


bool
convert_from_uis_type(uint8 *type)
{
	switch (*type) {
		case UIS_TYPE_INPUT:
			*type = UIS_REPORT_TYPE_INPUT;
			break;
		case UIS_TYPE_OUTPUT:
			*type = UIS_REPORT_TYPE_OUTPUT;
			break;
		case UIS_TYPE_FEATURE:
			*type = UIS_REPORT_TYPE_FEATURE;
			break;
		default:
			return false;
	}

	return true;
}


uint8
convert_to_uis_type(uint8 type)
{
	switch (type) {
		case UIS_REPORT_TYPE_INPUT:
			return UIS_TYPE_INPUT;
		case UIS_REPORT_TYPE_OUTPUT:
			return UIS_TYPE_OUTPUT;
		case UIS_REPORT_TYPE_FEATURE:
			return UIS_TYPE_FEATURE;
	}

	return 0;
}


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
				if (!lock.IsLocked())
					break;

				DeviceMap::iterator found = fDeviceMap.upper_bound(id);
				if (found == fDeviceMap.end()) {
					status = B_BAD_VALUE;
					break;
				}
				return reply->AddInt32("next", found->first);
			}

		case B_UIS_FIND_DEVICE:
			{
				const char *name = NULL, *path = NULL;
				if (message->FindString("name", &name) != B_OK
						&& message->FindString("path", &path) != B_OK)
					break;

				BAutolock lock(fDeviceMapLock);
				if (!lock.IsLocked())
					break;

				for (DeviceMap::iterator it = fDeviceMap.begin();
						it != fDeviceMap.end(); it++) {
					UISDevice *device = it->second;
					if ((name != NULL && device->HasName(name))
							|| (path != NULL && device->HasPath(path)))
						return reply->AddInt32("device", it->first);
				}
				break;
			}

		case B_UIS_GET_DEVICE:
			{
				uis_device_id id;
				if (message->FindInt32("device", &id) != B_OK)
					break;

				BAutolock lock(fDeviceMapLock);
				if (!lock.IsLocked())
					break;

				UISDevice *device = _Device(id);
				if (device == NULL)
					break;

				status = reply->AddString("name", device->Name());
				if (status != B_OK)
					break;
				status = reply->AddString("path", device->Path());
				if (status != B_OK)
					break;
				status = reply->AddInt16("page", device->UsagePage());
				if (status != B_OK)
					break;
				status = reply->AddInt16("id", device->UsageId());
				if (status != B_OK)
					break;
				status = reply->AddInt32("input reports",
					device->CountReports(UIS_REPORT_TYPE_INPUT));
				if (status != B_OK)
					break;
				status = reply->AddInt32("output reports",
					device->CountReports(UIS_REPORT_TYPE_OUTPUT));
				if (status != B_OK)
					break;
				return reply->AddInt32("feature reports",
					device->CountReports(UIS_REPORT_TYPE_FEATURE));
			}

		case B_UIS_GET_REPORT:
			{
				uis_device_id id;
				int32 index;
				uint8 type;
				if (message->FindInt32("device", &id) != B_OK
						|| message->FindInt32("report", &index) != B_OK
						|| message->FindInt8("type", (int8 *) &type) != B_OK)
					break;

				BAutolock lock(fDeviceMapLock);
				if (!lock.IsLocked())
					break;

				UISDevice *device = _Device(id);
				if (device == NULL)
					break;

				for (uint8 i = 0; i < UIS_REPORT_TYPES; i++) {
					if ((type & 1 << i) != 0) {
						if (index < device->CountReports(i)) {
							type = i;
							break;
						}
						index -= device->CountReports(i);
					}
				}

				UISReport *report = device->ReportAt(type, index);
				if (report == NULL)
					break;
				status = reply->AddInt32("items", report->CountItems());
				if (status != B_OK)
					break;
				return reply->AddInt8("type", (int8) convert_to_uis_type(type));
			}

		case B_UIS_SEND_REPORT:
			{
				uis_device_id id;
				int32 index;
				uint8 type;
				if (message->FindInt32("device", &id) != B_OK
						|| message->FindInt32("report", &index) != B_OK
						|| message->FindInt8("type", (int8 *) &type) != B_OK)
					break;
				if (!convert_from_uis_type(&type))
					break;

				BAutolock lock(fDeviceMapLock);
				if (!lock.IsLocked())
					break;

				UISDevice *device = _Device(id);
				if (device == NULL)
					break;
				UISReport *report = device->ReportAt(type, index);
				if (report != NULL)
					return report->SendReport(message);
				break;
			}

		case B_UIS_GET_ITEM:
			{
				uis_device_id id;
				int32 reportIndex, itemIndex;
				uint8 type;
				if (message->FindInt32("device", &id) != B_OK
						|| message->FindInt32("report", &reportIndex) != B_OK
						|| message->FindInt8("type", (int8 *) &type) != B_OK
						|| message->FindInt32("item", &itemIndex) != B_OK)
					break;
				if (!convert_from_uis_type(&type))
					break;

				BAutolock lock(fDeviceMapLock);
				if (!lock.IsLocked())
					break;

				UISDevice *device = _Device(id);
				if (device == NULL)
					break;
				UISReport *report = device->ReportAt(type, reportIndex);
				if (report == NULL)
					break;
				UISReportItem *item = report->ItemAt(itemIndex);
				if (item == NULL)
					break;

				status = reply->AddInt16("page", item->UsagePage());
				if (status != B_OK)
					break;
				return reply->AddInt16("id", item->UsageId());
			}

		case B_UIS_FIND_ITEM:
			{
				uis_device_id id;
				uint32 usage;
				if (message->FindInt32("device", &id) != B_OK
					|| message->FindInt32("usage", (int32 *) &usage) != B_OK)
					break;

				BAutolock lock(fDeviceMapLock);
				if (!lock.IsLocked())
					break;

				uint16 usagePage = usage >> 16;
				uint16 usageId = usage & 0xffff;

				UISDevice *device = _Device(id);
				if (device == NULL)
					break;

				for (int32 ir = 0;
						ir < device->CountReports(UIS_REPORT_TYPE_INPUT);
						ir++) {
					UISReport *report = device->ReportAt(UIS_REPORT_TYPE_INPUT,
						ir);
					for (int32 ii = 0; ii < report->CountItems(); ii++) {
						UISReportItem *item = report->ItemAt(ii);
						if (item->UsagePage() == usagePage
								&& item->UsageId() == usageId) {
							status = reply->AddInt32("report", ir);
							if (status != B_OK)
								break;
							status = reply->AddInt32("item", ii);
							if (status != B_OK)
								break;
							status = reply->AddInt16("page", item->UsagePage());
							if (status != B_OK)
								break;
							status = reply->AddInt16("id", item->UsageId());
							if (status != B_OK)
								break;
							return reply->AddBool("relative",
								item->IsRelative());
						}
					}
				}
				break;
			}

		case B_UIS_ITEM_SET_TARGET:
			{
				uis_device_id id;
				int32 reportIndex, itemIndex;
				uint8 type;
				void *target;
				if (message->FindInt32("device", &id) != B_OK
						|| message->FindInt32("report", &reportIndex) != B_OK
						|| message->FindInt32("item", &itemIndex) != B_OK
						|| message->FindInt8("type", (int8 *) &type) != B_OK
						|| message->FindPointer("target", &target) != B_OK)
					break;
				if (!convert_from_uis_type(&type))
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
				if (!lock.IsLocked())
					break;

				UISDevice *device = _Device(id);
				if (device == NULL)
					break;
				UISReport *report = device->ReportAt(type, reportIndex);
				if (report == NULL)
					break;
				UISReportItem *item = report->ItemAt(itemIndex);
				if (item == NULL)
					break;

				item->SetTarget(team, port, token, cookie, &target);
				return reply->AddPointer("target", target);
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
