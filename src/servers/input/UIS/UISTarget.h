#ifndef _UIS_TARGET_H
#define _UIS_TARGET_H

#include <Messenger.h>
#include <MessengerPrivate.h>

using namespace BPrivate;


class UISTarget : public BMessenger, public BMessenger::Private
{
public:
			UISTarget(team_id team, port_id port, int32 token);

	bool	HasTarget(port_id port, int32 token);

	void	IncRef() { fRefCount++; };
	uint32	DecRef();

private:
	uint32	fRefCount;
};


#endif // _UIS_TARGET_H
