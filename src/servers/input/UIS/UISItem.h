#ifndef _UIS_ITEM_H
#define _UIS_ITEM_H

#include <List.h>
#include <Messenger.h>


class UISTarget;
class UISReport;


typedef struct _uis_item_target {
	UISTarget *	target;
	void *		cookie;
} uis_item_target;


class UISReportItem {
public:
				UISReportItem(int fd, UISReport *report, uint32 index);
				~UISReportItem();

	status_t	InitCheck();
	void		SetValue(uint32 value);

	uint16		UsagePage() { return fUsagePage; };
	uint16		UsageId() { return fUsageId; };
	bool		IsRelative() { return fIsRelative; };
	uint32		Minimum() { return fMinimum; };
	uint32		Maximum() { return fMaximum; };
	uint32		Value() { return fValue; };

	void		SetTarget(team_id team, port_id port, int32 token, void *cookie,
					void **target);

private:
	void		_SendEvents();

	UISReport *	fUISReport;
	void *		fItem;
	uint16		fUsagePage;
	uint16		fUsageId;
	bool		fIsRelative;
	uint32		fMinimum;
	uint32		fMaximum;
	uint32		fValue;
	BList		fItemTargetList;

	team_id		fTeamId;
	port_id		fPortId;
	int32		fObjectToken;
};


#endif // _UIS_ITEM_H
