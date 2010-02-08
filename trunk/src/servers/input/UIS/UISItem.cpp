#include "UISItem.h"
#include "UISManager.h"
#include "UISDevice.h"
#include "UISReport.h"
#include "UISTarget.h"

#include <uis_driver.h>
#include <UISProtocol.h>

#include "UIS_debug.h"


UISReportItem::UISReportItem(int fd, UISReport *report, int32 index)
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
	//TRACE("create item usage page: %04x id: %04x, relative: %s, min: %ld, "
	//	"max: %ld\n", fUsagePage, fUsageId, fIsRelative?"yes":"no",
	//	fMinimum, fMaximum);
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
UISReportItem::SetValue(float value)
{
	fValue = value;
	//TRACE("set value for item %04x %04x: %08x\n", fUsagePage, fUsageId, fValue);

	_SendEvents();
}


void
UISReportItem::_SendEvents()
{
	UISManager *manager = fUISReport->Device()->Manager();
	for (int32 i = 0; i < fItemTargetList.CountItems(); i++) {
		uis_item_target *itemTarget =
			(uis_item_target *) fItemTargetList.ItemAt(i);
		status_t status = manager->SendEvent(itemTarget, this);
		if (status == B_BAD_PORT_ID) {
			fItemTargetList.RemoveItem(i--);
			delete itemTarget;
		}
	}
}


void
UISReportItem::SetTarget(team_id team, port_id port, int32 token, void *cookie,
	void **target)
{
	//TRACE("team: %d, port: %d, token: %d, cookie: %08x, target: %08x\n",
	//	team, port, token, cookie, *target);
	uis_item_target *itemTarget = (uis_item_target *) *target;
	UISManager *manager = fUISReport->Device()->Manager();

	if (itemTarget != NULL)
		manager->RemoveTarget(itemTarget->target);

	if (team == -1 || port == -1 || token == B_NULL_TOKEN) {
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
