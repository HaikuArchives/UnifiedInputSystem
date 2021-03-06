/*
	Driver for USB Human Interface Devices.
	Copyright (C) 2008 Michael Lotz <mmlr@mlotz.ch>
	Distributed under the terms of the MIT license.
*/
#ifndef USB_HID_DEVICE_H
#define USB_HID_DEVICE_H

#include "HIDParser.h"

#include <USB3.h>

class ApplicationHandler;

class HIDDevice {
public:
								HIDDevice(usb_device device,
									const usb_configuration_info *config,
									size_t interfaceIndex);
								~HIDDevice();

		void					SetParentCookie(int32 cookie);
		int32					ParentCookie() { return fParentCookie; };

		status_t				InitCheck() { return fStatus; };

		bool					IsOpen() { return fOpenCount > 0; };
		status_t				Open(ApplicationHandler *handler, uint32 flags);
		status_t				Close(ApplicationHandler *handler);

		void					Removed();
		bool					IsRemoved() { return fRemoved; };

		status_t				MaybeScheduleTransfer();

		status_t				SendReport(HIDReport *report);

		HIDParser *				Parser() { return &fParser; };
		ApplicationHandler *	ApplicationHandlerAt(uint32 index);

		// only to be used for the kernel debugger information
		usb_pipe				InterruptPipe() { return fInterruptPipe; };

		status_t				GetString(uint8 index, char *string,
									int32 *length);

		uint8					Name() { return fName; };

private:
static	void					_TransferCallback(void *cookie,
									status_t status, void *data,
									size_t actualLength);

		status_t				fStatus;
		usb_device				fDevice;
		usb_pipe				fInterruptPipe;
		size_t					fInterfaceIndex;

		int32					fTransferScheduled;
		size_t					fTransferBufferSize;
		uint8 *					fTransferBuffer;

		int32					fParentCookie;
		int32					fOpenCount;
		bool					fRemoved;

		HIDParser				fParser;

		uint32					fApplicationHandlerCount;
		ApplicationHandler **	fApplicationHandlers;

		uint16					fLangId;

		uint8					fName;
};

#endif // USB_HID_DEVICE_H
