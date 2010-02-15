#ifndef _UIS_MANAGER_H
#define _UIS_MANAGER_H

#include <map>

#include <Looper.h>
#include <List.h>
#include <Locker.h>
#include <UISKit.h>

struct _uis_item_target;
typedef _uis_item_target uis_item_target;

class UISTarget;
class UISReportItem;
class UISReport;
class UISDevice;


class UISManager : public BLooper {
public:
					UISManager();
					~UISManager();

	status_t		Start();
	void			Stop();

	void			MessageReceived(BMessage *message);
	status_t		HandleMessage(BMessage *message, BMessage *reply);

	void			RemoveDevice(uis_device_id id);

	UISTarget *		FindOrAddTarget(team_id team, port_id port, int32 token);
	void			RemoveTarget(UISTarget *target);
	status_t		SendEvent(uis_item_target *itemTarget, UISReportItem *item);

private:
	void			_RecursiveScan(const char *directory);
	void			_AddDevice(const char *path);
	void			_HandleAddDevice(BMessage *message);
	UISDevice *		_Device(uis_device_id id);

	bool			fIsRunning;

	typedef std::map<uis_device_id, UISDevice *> DeviceMap;
	DeviceMap		fDeviceMap;
	uis_device_id	fNextDeviceId;
	int32			fFreeDeviceIds;
	BLocker			fDeviceMapLock;

	BList			fTargetList;
	BLocker			fTargetListLocker;
};


#endif // _UIS_MANAGER_H
