#include "Joystick.h"
#include <UISKit.h>

#include <usb/USB_hid.h>

#include <Looper.h>
#include <List.h>
#include <Path.h>
#include <Directory.h>
#include <String.h>
#include <Debug.h>


#define B_UIS_ITEM_EVENT '_UIE'


class JoystickLooper : public BLooper
{
public:
				JoystickLooper(BJoystick *target);

	void		MessageReceived(BMessage *message);

private:
	BJoystick *	fJoystick;
};


JoystickLooper::JoystickLooper(BJoystick *target)
	:
	BLooper(),
	fJoystick(target)
{
}


void
JoystickLooper::MessageReceived(BMessage *message)
{
	if (message->what == B_UIS_ITEM_EVENT) {
		BUISItem *item;
		float value;

		if (message->FindPointer("cookie", (void **) &item) != B_OK
				|| message->FindFloat("value", &value) != B_OK)
			return;
		switch (item->UsagePage()) {
			case HID_USAGE_PAGE_GENERIC_DESKTOP:
				{
					int32 i = fJoystick->fAxesList->IndexOf(item);
					if (i < 0)
						break;
					fJoystick->fAxesValues[i] = (int16) (value * 32768.0f
							- 0.5f);

					if (i == 0)
						fJoystick->horizontal = fJoystick->fAxesValues[0];
					if (i == 1)
						fJoystick->vertical = fJoystick->fAxesValues[1];
					break;
				}

			case HID_USAGE_PAGE_BUTTON:
				{
					int32 i = fJoystick->fButtonsList->IndexOf(item);
					if (i < 0)
						break;
					if (value > 0.5f)
						fJoystick->fButtons |= ((uint32) 1 << i);
					else
						fJoystick->fButtons &= ~((uint32) 1 << i);

					if (i == 0)
						fJoystick->button1 = fJoystick->fButtons & 1;
					if (i == 1)
						fJoystick->button2 = fJoystick->fButtons & 2;
					break;
				}
		}
	}
}


BJoystick::BJoystick()
	:
	fBeBoxMode(false),
	fCalibrationEnabled(true),
	fUISDevice(NULL),
	fDevices(new BList),
	fAxesList(new BList),
	fHatsList(new BList),
	fButtonsList(new BList),
	fButtons(0),
	fAxesValues(NULL),
	fHatsValues(NULL),
	fLooper(new JoystickLooper(this))
{
	BUISRoster roster;
	BUISDevice *device;

	// joystick devices need to have at least: 2 axes (x & y) and 1 button
	while (roster.GetNextDevice(&device) == B_OK) {
		BUISItem* item = device->FindItem(HID_USAGE_PAGE_GENERIC_DESKTOP,
			HID_USAGE_ID_X);
		if (item != NULL) {
			item = device->FindItem(HID_USAGE_PAGE_GENERIC_DESKTOP,
				HID_USAGE_ID_Y);
			if (item != NULL) {
				item = device->FindItem(HID_USAGE_PAGE_BUTTON, 1);
				if (item != NULL) {
					fDevices->AddItem(device);
					continue;
				}
			}
		}
		delete device;
			// we're supposed to free device object if we don't use it
	}

	fLooper->Run();
}


BJoystick::~BJoystick()
{
	fLooper->Lock();
	fLooper->Quit();

	Close();

	delete fAxesList;
	delete fHatsList;
	delete fButtonsList;

	while (fDevices->CountItems() > 0)
		delete (BUISDevice *) fDevices->RemoveItem(0l);
	delete fDevices;
}


status_t
BJoystick::Open(const char *devName)
{
	return Open(devName, true);
}


status_t
BJoystick::Open(const char *devName, bool enterEnhanced)
{
	if (fUISDevice)
		Close();

	if (devName == NULL)
		return B_ERROR;

	for (int32 i = 0; i < fDevices->CountItems(); i++) {
		BUISDevice *device = (BUISDevice *) fDevices->ItemAt(i);
		if (strcmp(devName, device->Path()) == 0) {
			fUISDevice = device;
			break;
		}
	}
	if (fUISDevice == NULL)
		return B_ERROR;

	fBeBoxMode = !enterEnhanced;

	BUISItem *item;

	item = fUISDevice->FindItem(HID_USAGE_PAGE_GENERIC_DESKTOP, HID_USAGE_ID_X);
	if (item) {
		fAxesList->AddItem(item);
		item->SetTarget(fLooper);
	}
	item = fUISDevice->FindItem(HID_USAGE_PAGE_GENERIC_DESKTOP, HID_USAGE_ID_Y);
	if (item) {
		fAxesList->AddItem(item);
		item->SetTarget(fLooper);
	}

	item = fUISDevice->FindItem(HID_USAGE_PAGE_BUTTON, 1);
	if (item) {
		fButtonsList->AddItem(item);
		item->SetTarget(fLooper);
	}
	item = fUISDevice->FindItem(HID_USAGE_PAGE_BUTTON, 2);
	if (item) {
		fButtonsList->AddItem(item);
		item->SetTarget(fLooper);
	}

	fAxesValues = (int16 *) malloc(CountAxes() * sizeof(int16));
	fHatsValues = (uint8 *) malloc(CountHats() * sizeof(uint8));
	memset(fAxesValues, 0, CountAxes() * sizeof(int16));
	memset(fHatsValues, 0, CountHats() * sizeof(uint8));

	return 1;
		// according to BeBook we should return positive integer on success
}


void
BJoystick::Close(void)
{
	if (fUISDevice == NULL)
		return;

	fAxesList->MakeEmpty();
	fHatsList->MakeEmpty();
	fButtonsList->MakeEmpty();

	free(fAxesValues);
	fAxesValues = NULL;
	free(fHatsValues);
	fHatsValues = NULL;

	fUISDevice = NULL;
}


int32
BJoystick::CountDevices()
{
	return fDevices->CountItems();
}


status_t
BJoystick::GetDeviceName(int32 n, char *outName, size_t bufSize)
{
	if (n >= fDevices->CountItems())
		return B_BAD_INDEX;

	const char *path = ((BUISDevice *) fDevices->ItemAt(n))->Path();
	if (strlen(path) >= bufSize)
		return B_NAME_TOO_LONG;

	strncpy(outName, path, bufSize);
	return B_OK;
}


bool
BJoystick::EnterEnhancedMode(const entry_ref *ref)
{
	fBeBoxMode = false;
	return !fBeBoxMode;
}


int32
BJoystick::CountSticks()
{
	return 1;
}


int32
BJoystick::CountAxes()
{
	return fAxesList->CountItems();
}


int32
BJoystick::CountHats()
{
	return fHatsList->CountItems();
}


int32
BJoystick::CountButtons()
{
	return fButtonsList->CountItems();
}

status_t
BJoystick::GetControllerModule(BString *out_name)
{
	return B_ERROR;
}


status_t
BJoystick::GetControllerName(BString *outName)
{
	if (fBeBoxMode)
		outName->SetTo("2-axis");
	else {
		if (fUISDevice == NULL)
			return B_ERROR;
		outName->SetTo(fUISDevice->Name());
	}
	return B_OK;
}


bool
BJoystick::IsCalibrationEnabled()
{
	return fCalibrationEnabled;
}


status_t
BJoystick::EnableCalibration(bool calibrates)
{
	fCalibrationEnabled = calibrates;
	return B_OK;
}


status_t
BJoystick::SetMaxLatency(bigtime_t max_latency)
{
	return B_OK;
}


status_t
BJoystick::GetAxisNameAt(int32 index, BString *out_name)
{
	return B_BAD_INDEX;
}


status_t
BJoystick::GetHatNameAt(int32 index, BString *out_name)
{
	return B_BAD_INDEX;
}


status_t
BJoystick::GetButtonNameAt(int32 index, BString *out_name)
{
	return B_BAD_INDEX;
}


status_t
BJoystick::GetAxisValues(int16 *outValues, int32 forStick)
{
	memcpy(outValues, fAxesValues, CountAxes() * sizeof(int16));
	return B_OK;
}


status_t
BJoystick::GetHatValues(uint8 *outHats, int32 forStick)
{
	memcpy(outHats, fHatsValues, CountHats() * sizeof(uint8));
	return B_OK;
}


uint32
BJoystick::ButtonValues(int32 for_stick)
{
	return fButtons;
}


status_t
BJoystick::Update(void)
{
	timestamp = real_time_clock_usecs();

	return B_OK;
}


void
BJoystick::Calibrate(struct _extended_joystick *reading)
{
}


void
BJoystick::ScanDevices(bool use_disabled)
{
}


status_t
BJoystick::GatherEnhanced_info(const entry_ref *ref)
{
	return B_ERROR;
}


status_t
BJoystick::SaveConfig(const entry_ref *ref)
{
	return B_ERROR;
}


//	#pragma mark - FBC protection


void BJoystick::_ReservedJoystick1() {}
void BJoystick::_ReservedJoystick2() {}
void BJoystick::_ReservedJoystick3() {}
status_t BJoystick::_Reserved_Joystick_4(void*, ...) { return B_ERROR; }
status_t BJoystick::_Reserved_Joystick_5(void*, ...) { return B_ERROR; }
status_t BJoystick::_Reserved_Joystick_6(void*, ...) { return B_ERROR; }
