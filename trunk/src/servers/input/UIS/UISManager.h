#ifndef _UIS_MANAGER_H
#define _UIS_MANAGER_H

#include <Looper.h>
#include <List.h>
#include <Locker.h>

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

	status_t	Start();
	void		Stop();

	void		MessageReceived(BMessage *message);
	status_t	HandleMessage(BMessage *message, BMessage *reply);

	void		RemoveDevice(UISDevice *device);

	UISTarget *	FindOrAddTarget(team_id team, port_id port, int32 token);
	void		RemoveTarget(UISTarget *target);
	status_t	SendEvent(uis_item_target *itemTarget, UISReportItem *item);

private:
	static bool	_RemoveDeviceListItem(void *arg);
	void		_HandleAddRemoveDevice(BMessage *message);

	bool		fIsRunning;
	BList		fUISDeviceList;
	BLocker		fUISDeviceListLocker;
	BList		fTargetList;
	BLocker		fTargetListLocker;
};


#endif // _UIS_MANAGER_H
