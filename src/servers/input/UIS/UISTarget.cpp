#include "UISTarget.h"


UISTarget::UISTarget(team_id team, port_id port, int32 token)
	:
	BMessenger(),
	BMessenger::Private(this),
	fRefCount(1)
{
	SetTo(team, port, token);
}


bool
UISTarget::HasTarget(port_id port, int32 token)
{
	return (port == Port() && token == Token());
}


uint32
UISTarget::DecRef()
{
	if (fRefCount > 0)
		fRefCount--;
	return fRefCount;
}
