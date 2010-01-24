#include "UIS.h"

#include <Autolock.h>
#include <UTF8.h>
#include <Messenger.h>

#include <PathMonitor.h>
#include <UISProtocol.h>
#include "kb_mouse_driver.h"
#include <MessengerPrivate.h>

#include <errno.h>
#include <new>
#include <stdlib.h>

using std::nothrow;

#undef TRACE
#define TRACE_IS_UIS
#ifdef TRACE_IS_UIS
#include <stdio.h>
void uis_log_print(const char *fmt, ...) {
			FILE* log = fopen("/var/log/uis_input_server.log", "a");
			char buf[1024];
			va_list ap;
			va_start(ap, fmt);
			vsprintf(buf, fmt, ap);
			va_end(ap);
			fputs(buf, log);
			fflush(log);
			fclose(log);
        }
#define TRACE(a...) uis_log_print(a)
#else
#define TRACE(a...)
#endif

static const char kMonitoredPath[] = "/dev/input/hid";
static const uint32 kReportReaderThreadPriority = B_FIRST_REAL_TIME_PRIORITY+4;


char *
UISString::StrDupTrim(const char *str)
{
	while (*str == ' ')
		str++;

	size_t length = strlen(str);
	if (length < 1)
		return NULL;
	while (length > 0 && str[length-1] == ' ')
		length--;

	return strndup(str, length);
}


UISString::UISString()
	:
	fDevice(-1),
	fId(0),
	fString(NULL),
	fIsValid(false)
{
}


UISString::~UISString()
{
	free(fString);
}


void
UISString::SetTo(int fd, uint32 id)
{
	fDevice = fd;
	fId = id;
}


const char *
UISString::String()
{
	if (fIsValid)
		return fString;

	if (fDevice == -1)
		return NULL;

	uis_string_info info;
	info.id = fId;
	info.string = NULL;
	if (ioctl(fDevice, UIS_STRING_INFO, &info) != B_OK)
		return NULL;

	info.string = new (std::nothrow) char[info.length];
	if (info.string == NULL)
		return NULL;
	if (ioctl(fDevice, UIS_STRING_INFO, &info) != B_OK) {
		delete [] info.string;
		return NULL;
	}

	int32 destLen = info.length, state = 0;
	char *dest = new (std::nothrow) char[destLen];

	if (convert_to_utf8(info.encoding, info.string, &info.length, dest,
		&destLen, &state) == B_OK) {
		dest[destLen] = 0;
		fString = StrDupTrim(dest);
		fIsValid = true;
	}

	delete [] info.string;
	delete [] dest;

	return fString;
}


UISReportItem::UISReportItem(int fd, UISReport *report, uint32 index)
	:
	fUISReport(report),
	fItem(NULL)
{
	uis_item_info itemDesc;
	itemDesc.in.report = report->Report();
	itemDesc.in.index = index;
	if (ioctl(fd, UIS_ITEM_INFO, &itemDesc) != B_OK)
		return;
	fItem = itemDesc.out.item;
	fUsagePage = itemDesc.out.usagePage;
	fUsageId = itemDesc.out.usageId;
	fIsRelative = itemDesc.out.isRelative;
	fMinimum = itemDesc.out.min;
	fMaximum = itemDesc.out.max;
	//TRACE("create item usage page: %04x id: %04x, relative: %s, min: %ld, "
	//	"max: %ld\n", fUsagePage, fUsageId, fIsRelative?"yes":"no",
	//	fMinimum, fMaximum);
	fTeamId = -1;
	fPortId = -1;
	fObjectToken = B_NULL_TOKEN;
}


UISReportItem::~UISReportItem()
{
	//TRACE("delete item usage id: 0x%02x\n", fUsageId);
}


status_t
UISReportItem::InitCheck()
{
	return (fItem == NULL) ? B_NO_INIT : B_OK;
}


void
UISReportItem::SetValue(uint32 value)
{
	fValue = value;
	//TRACE("set value for item %04x %04x: %08x\n", fUsagePage, fUsageId, fValue);

	fUISReport->Device()->Manager()->SendEvents(&fItemTargetList, this);
}


void
UISReportItem::SetTarget(team_id team, port_id port, int32 token, void *cookie,
	void **target)
{
	uis_item_target *itemTarget = (uis_item_target *) *target;
	UISManager *manager = fUISReport->Device()->Manager();

	if (itemTarget != NULL)
		manager->RemoveTarget(itemTarget->target);

	if (team == -1 || port == -1) {
		if (itemTarget != NULL && fItemTargetList.HasItem(itemTarget)) {
			fItemTargetList.RemoveItem(itemTarget);
			delete itemTarget;
			itemTarget = NULL;
		}
		return;
	}

	if (itemTarget == NULL) {
		itemTarget = new uis_item_target;
		fItemTargetList.AddItem(itemTarget);
		*target = itemTarget;
	}

	itemTarget->target = manager->FindOrAddTarget(team, port, token);
	itemTarget->cookie = cookie;
}


// --- temporary border line ---


UISReport::UISReport(int fd, UISDevice *device, uint8 type, uint8 index)
	:
	fStatus(B_NO_INIT),
	fDevice(fd),
	fUISDevice(device),
	fType(type),
	fReport(NULL),
	fId(0),
	fReadingThread(-1),
	fThreadActive(false)
{
	uis_report_info reportDesc;
	reportDesc.in.type = type;
	reportDesc.in.index = index;
	fStatus = ioctl(fd, UIS_REPORT_INFO, &reportDesc);
	if (fStatus != B_OK)
		return;
	fReport = reportDesc.out.report;
	fId = reportDesc.out.id;
	//TRACE("create report type: %d, id: %d, items: %d\n", fType, fId,
	//	reportDesc.out.itemCount);

	uint32 n = 0;
	for ( ; n<reportDesc.out.itemCount; n++) {
		UISReportItem *item = new (std::nothrow) UISReportItem(fd, this, n);
			if (item == NULL)
				break;
			if (item->InitCheck() != B_OK) {
				delete item;
				break;
			}
			fItemList.AddItem(item);
	}
	if (n < reportDesc.out.itemCount) {
		fStatus = B_NO_INIT; // FIXME
		return;
	}

	char threadName[B_OS_NAME_LENGTH];
	snprintf(threadName, B_OS_NAME_LENGTH, "uis report %08x reader",
		(unsigned int) this);
		// FIXME: fix the name of the thread
	fReadingThread = spawn_thread(_ReadingThreadEntry, threadName,
		kReportReaderThreadPriority, (void *) this);

	if (fReadingThread < B_OK) {
		fStatus = fReadingThread;
		return;
	}

	fThreadActive = true;
	fStatus = resume_thread(fReadingThread);
	if (fStatus != B_OK)
		fThreadActive = false;
}


UISReport::~UISReport()
{
	TRACE("delete report type: %d, id: %d\n", fType, fId);

	//TRACE("will stop the thread\n");
	if (fThreadActive) {
		fThreadActive = false;
			// this will eventually bring waiting thread down

		if (ioctl(fDevice, UIS_STOP, &fReport) == B_OK) {
			TRACE("wait for reading thread to quit...\n");
			wait_for_thread(fReadingThread, NULL);
				// wait only if ioctl succeeded, it'll eventually kill
				// the thread otherwise. don't have better idea for now
		}
	}

	fItemList.DoForEach(_RemoveItemListItem);
}


bool
UISReport::_RemoveItemListItem(void *arg)
{
	delete (UISReportItem *) arg;
	return false;
}


void
UISReport::SetReport(uis_report_data *data)
{
	//TRACE("has items: %d\n", data->out.items);
	for (uint32 i = 0; i<data->out.items; i++) {
		//TRACE("index of item: %d\n", data->out.item[i].index);
		UISReportItem *item = (UISReportItem *)
			fItemList.ItemAt(data->out.item[i].index); // TODO: check numbering
		item->SetValue(data->out.item[i].value);
		//TRACE("set value report %d %d %08x\n", fType, fId, i);
	}
}


UISReportItem *
UISReport::ItemAt(int32 index)
{
	if (index >= CountItems())
		return NULL;
	return (UISReportItem *) fItemList.ItemAt(index);
}


status_t
UISReport::_ReadingThreadEntry(void *arg)
{
	UISReport *report = (UISReport *) arg;
	report->_ReadingThread();
	return B_OK;
}


void
UISReport::_ReadingThread()
{
	TRACE("entering thread for report id: %d\n", fId);

	uint8 *buffer = new (std::nothrow) uint8[sizeof(uis_report_data)
			+ sizeof(uis_report_info) * CountItems()];
				// size is calculated for all possible items included
	if (buffer == NULL) {
		fThreadActive = false;
		return;
	}

	while (fThreadActive) {
		uis_report_data *data = (uis_report_data *) buffer;
		data->in.report = fReport;
		if (ioctl(fDevice, UIS_READ, data) != B_OK) {
			if (errno == B_DEV_NOT_READY) {
				delete [] buffer;
				fThreadActive = false;
				fUISDevice->Remove();
				// BONKERS !
				return;
			}

			TRACE("ioctl status = %08x\n", errno);
			fThreadActive = false;
			break;
		}

		SetReport(data);
	}

	delete [] buffer;

	TRACE("leaving thread for report id: %d\n", fId);
}


// --- temporary border line ---


UISDevice::UISDevice(UISManager *manager, const char *path)
	:
	fUISManager(manager),
	fPath(strdup(path)),
	fDevice(-1),
	fUsage(0)
{
	TRACE("create device at %s\n", path);

	fDevice = open(fPath, O_RDWR);
	if (fDevice == -1)
		return;

	uis_device_info info;
	status_t status = ioctl(fDevice, UIS_DEVICE_INFO, &info);
	if (status != B_OK)
		return;

	fUsage = info.usage;
	fName.SetTo(fDevice, info.name);
	//TRACE("usage: %08x, input report count: %d, name: %d\n", fUsage,
	//	info.reportCount, info.name);

	for (uint8 n = 0; n<info.reportCount; n++) {
		UISReport *report = new (std::nothrow) UISReport(fDevice, this, 1, n);
			// FIXME: un-fix the report type
		if (report == NULL)
			break;
		if (report->InitCheck() != B_OK) {
			delete report;
			break;
		}
		fReportList[0].AddItem(report);
	}
}


UISDevice::~UISDevice()
{
	TRACE("delete device at: %s\n", fPath);

	fReportList[0].DoForEach(_RemoveReportListItem);

	//TRACE("will close the device\n");
	if (fDevice != -1) {
		close(fDevice);
		fDevice = -1;
	}

	free(fPath);
}


bool
UISDevice::_RemoveReportListItem(void *arg)
{
	delete (UISReport *) arg;
	return false;
}


bool
UISDevice::HasPath(const char *path)
{
	return strcmp(path, fPath) == 0;
}


bool
UISDevice::HasName(const char *name)
{
	if (name == NULL)
		return false;
	const char *localName = fName.String();
	return (localName != NULL) ? strcmp(name, localName) == 0 : false;
}


uint8
UISDevice::CountReports(uint8 type)
{
	return (type < 3) ? (uint8) fReportList[type].CountItems() : 0;
}


UISReport *
UISDevice::ReportAt(uint8 type, uint8 index)
{
	if (index >= CountReports(type))
		return NULL;
	return (UISReport *) fReportList[type].ItemAt(index);
}


void
UISDevice::Remove()
{
	fUISManager->RemoveDevice(this);
}


// --- temporary border line ---


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
			for (int32 i = 0; i<fUISDeviceList.CountItems(); i++) {
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
					for (int32 i = 0; i<fUISDeviceList.CountItems(); i++) {
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
					for (int32 i = 0; i<fUISDeviceList.CountItems(); i++) {
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
				int32 token = 0;
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


uis_target *
UISManager::FindOrAddTarget(team_id team, port_id port, int32 token)
{
	uis_target *target;

	for (int32 i = 0; i<fTargetList.CountItems(); i++) {
		target = (uis_target *) fTargetList.ItemAt(i);
		if (target == NULL)
			continue;
		if (target->team == team && target->port == port
			&& target->token == token) {
			target->refCount++;
			return target;
		}
	}

	target = new uis_target;
	target->team = team;
	target->port = port;
	target->token = token;
	target->refCount = 1;
	return target;
}


void
UISManager::RemoveTarget(uis_target *target)
{
	if (!fTargetList.HasItem(target))
		return;
	if (--target->refCount == 0) {
		fTargetList.RemoveItem(target);
		delete target;
	}
}


void
UISManager::SendEvents(BList *itemTargetList, UISReportItem *item)
{
	BAutolock lock(fUISDeviceListLocker);
	if (lock.IsLocked()) {
		for (int32 i = 0; i < itemTargetList->CountItems(); i++) {
			uis_item_target *itemTarget =
				(uis_item_target *) itemTargetList->ItemAt(i);
			status_t status = _SendEvent(itemTarget, item);
			if (status == B_BAD_PORT_ID) {
				RemoveTarget(itemTarget->target);
				itemTargetList->RemoveItem(i--);
				delete itemTarget;
			}
		}
	}
}


status_t
UISManager::_SendEvent(uis_item_target *itemTarget, UISReportItem *item)
{
	uis_target *target = itemTarget->target;

	BMessenger::Private messengerPrivate(fMessenger);
	messengerPrivate.SetTo(target->team, target->port, target->token);

	BMessage message(B_UIS_ITEM_EVENT);
	message.AddPointer("cookie", itemTarget->cookie);
	message.AddInt32("value", (int32) item->Value());

	return fMessenger.SendMessage(&message);
}
