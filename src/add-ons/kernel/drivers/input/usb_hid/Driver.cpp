/*
	Driver for USB Human Interface Devices.
	Copyright (C) 2008-2009 Michael Lotz <mmlr@mlotz.ch>
	Distributed under the terms of the MIT license.
 */

#include "DeviceList.h"
#include "Driver.h"
#include "HIDDevice.h"
#include "HIDReport.h"
#include "ApplicationHandler.h"

#include <lock.h>
#include <new>
#include <stdio.h>
#include <string.h>


int32 api_version = B_CUR_DRIVER_API_VERSION;
usb_module_info *gUSBModule = NULL;
DeviceList *gDeviceList = NULL;
static int32 sParentCookie = 0;
static mutex sDriverLock;


// #pragma mark - notify hooks


status_t
usb_hid_device_added(usb_device device, void **cookie)
{
	TRACE("device_added()\n");
	const usb_device_descriptor *deviceDescriptor
		= gUSBModule->get_device_descriptor(device);

	TRACE("vendor id: 0x%04x; product id: 0x%04x\n",
		deviceDescriptor->vendor_id, deviceDescriptor->product_id);

	// wacom devices are handled by the dedicated wacom driver
	if (deviceDescriptor->vendor_id == USB_VENDOR_WACOM)
		return B_ERROR;

	const usb_configuration_info *config
		= gUSBModule->get_nth_configuration(device, USB_DEFAULT_CONFIGURATION);
	if (config == NULL) {
		TRACE_ALWAYS("cannot get default configuration\n");
		return B_ERROR;
	}

	// ensure default configuration is set
	status_t result = gUSBModule->set_configuration(device, config);
	if (result != B_OK) {
		TRACE_ALWAYS("set_configuration() failed 0x%08lx\n", result);
		return result;
	}

	// refresh config
	config = gUSBModule->get_configuration(device);
	if (config == NULL) {
		TRACE_ALWAYS("cannot get current configuration\n");
		return B_ERROR;
	}

	bool devicesFound = false;
	int32 parentCookie = atomic_add(&sParentCookie, 1);
	for (size_t i = 0; i < config->interface_count; i++) {
		const usb_interface_info *interface = config->interface[i].active;
		uint8 interfaceClass = interface->descr->interface_class;
		TRACE("interface %lu: class: %u; subclass: %u; protocol: %u\n",
			i, interfaceClass, interface->descr->interface_subclass,
			interface->descr->interface_protocol);

		if (interfaceClass == USB_INTERFACE_CLASS_HID) {
			mutex_lock(&sDriverLock);
			HIDDevice *hidDevice
				= new(std::nothrow) HIDDevice(device, config, i);

			if (hidDevice != NULL && hidDevice->InitCheck() == B_OK) {
				hidDevice->SetParentCookie(parentCookie);

				for (uint32 i = 0;; i++) {
					ApplicationHandler *handler
						= hidDevice->ApplicationHandlerAt(i);
					if (handler == NULL)
						break;

					// As devices can be un- and replugged at will, we cannot
					// simply rely on a device count. If there is just one
					// keyboard, this does not mean that it uses the 0 name.
					// There might have been two keyboards and the one using 0
					// might have been unplugged. So we just generate names
					// until we find one that is not currently in use.
					int32 index = 0;
					char pathBuffer[128];
					static const char kBasePath[] = "input/hid/usb/";
					while (true) {
						sprintf(pathBuffer, "%s%ld", kBasePath, index++);
						if (gDeviceList->FindDevice(pathBuffer) == NULL) {
							// this name is still free, use it
							handler->SetPublishPath(strdup(pathBuffer));
							break;
						}
					}

					gDeviceList->AddDevice(handler->PublishPath(), handler);
					devicesFound = true;
				}
			} else
				delete hidDevice;

			mutex_unlock(&sDriverLock);
		}
	}

	if (!devicesFound)
		return B_ERROR;

	*cookie = (void *)parentCookie;
	return B_OK;
}


status_t
usb_hid_device_removed(void *cookie)
{
	mutex_lock(&sDriverLock);
	int32 parentCookie = (int32)cookie;
	TRACE("device_removed(%ld)\n", parentCookie);

	for (int32 i = 0; i < gDeviceList->CountDevices(); i++) {
		ApplicationHandler *handler
			= (ApplicationHandler *)gDeviceList->DeviceAt(i);
		if (!handler)
			continue;

		HIDDevice *device = handler->Device();
		if (device->ParentCookie() != parentCookie)
			continue;

		// remove all the handlers from the list even if they are opened
		// this avoids publishing them in the following publish devices call
		// if device is removed, handlers will be deleted together with device
		// in free hook
		for (uint32 i = 0;; i++) {
			handler = device->ApplicationHandlerAt(i);
			if (handler == NULL)
				break;

			gDeviceList->RemoveDevice(NULL, handler);
		}

		if (device->IsOpen()) {
			// this handler's device belongs to the one removed
			// the device and it's handlers will be deleted in the free hook
			device->Removed();
			break;
		}

		delete device;
		break;
	}

	mutex_unlock(&sDriverLock);
	return B_OK;
}


// #pragma mark - driver hooks


static status_t
usb_hid_open(const char *name, uint32 flags, void **cookie)
{
	TRACE("open(%s, %lu, %p)\n", name, flags, cookie);
	mutex_lock(&sDriverLock);

	ApplicationHandler *handler
		= (ApplicationHandler *)gDeviceList->FindDevice(name);
	if (handler == NULL) {
		mutex_unlock(&sDriverLock);
		return B_ENTRY_NOT_FOUND;
	}

	status_t result = handler->Open(flags);
	*cookie = handler;
	mutex_unlock(&sDriverLock);
	return result;
}


static status_t
usb_hid_read(void *cookie, off_t position, void *buffer, size_t *numBytes)
{
	TRACE_ALWAYS("read on hid device\n");
	*numBytes = 0;
	return B_ERROR;
}


static status_t
usb_hid_write(void *cookie, off_t position, const void *buffer,
	size_t *numBytes)
{
	TRACE_ALWAYS("write on hid device\n");
	*numBytes = 0;
	return B_ERROR;
}


static status_t
usb_hid_control(void *cookie, uint32 op, void *buffer, size_t length)
{
	TRACE("control(%p, %lu, %p, %lu)\n", cookie, op, buffer, length);
	ApplicationHandler *handler = (ApplicationHandler *) cookie;
	return handler->Control(op, buffer, length);
}


static status_t
usb_hid_close(void *cookie)
{
	TRACE("close(%p)\n", cookie);
	ApplicationHandler *handler = (ApplicationHandler *) cookie;
	return handler->Close();
}


static status_t
usb_hid_free(void *cookie)
{
	TRACE("free(%p)\n", cookie);

	status_t status = B_OK;
	mutex_lock(&sDriverLock);

	HIDDevice *device = ((ApplicationHandler *) cookie)->Device();
	if (device->IsOpen()) {
		// another handler of this device is still open so we can't free it
		status = B_ERROR;
	} else if (device->IsRemoved()) {
		// the parent device is removed already and none of its handlers are
		// open anymore and not on the list also so we can free it here
		delete device;
	}

	mutex_unlock(&sDriverLock);
	return B_OK;
}


//	#pragma mark - driver API


status_t
init_hardware()
{
	TRACE("init_hardware() " __DATE__ " " __TIME__ "\n");
	return B_OK;
}


status_t
init_driver()
{
	TRACE("init_driver() " __DATE__ " " __TIME__ "\n");
	if (get_module(B_USB_MODULE_NAME, (module_info **)&gUSBModule) != B_OK)
		return B_ERROR;

	gDeviceList = new(std::nothrow) DeviceList();
	if (gDeviceList == NULL) {
		put_module(B_USB_MODULE_NAME);
		return B_NO_MEMORY;
	}

	mutex_init(&sDriverLock, "usb hid driver lock");

	static usb_notify_hooks notifyHooks = {
		&usb_hid_device_added,
		&usb_hid_device_removed
	};

	static usb_support_descriptor supportDescriptor = {
		USB_INTERFACE_CLASS_HID, 0, 0, 0, 0
	};

	gUSBModule->register_driver(DRIVER_NAME, &supportDescriptor, 1, NULL);
	gUSBModule->install_notify(DRIVER_NAME, &notifyHooks);
	TRACE("init_driver() OK\n");
	return B_OK;
}


void
uninit_driver()
{
	TRACE("uninit_driver()\n");
	gUSBModule->uninstall_notify(DRIVER_NAME);
	put_module(B_USB_MODULE_NAME);
	delete gDeviceList;
	gDeviceList = NULL;
	mutex_destroy(&sDriverLock);
}


const char **
publish_devices()
{
	TRACE("publish_devices()\n");
	const char **publishList = gDeviceList->PublishDevices();

	int32 index = 0;
	while (publishList[index] != NULL) {
		TRACE("publishing %s\n", publishList[index]);
		index++;
	}

	return publishList;
}


device_hooks *
find_device(const char *name)
{
	static device_hooks hooks = {
		usb_hid_open,
		usb_hid_close,
		usb_hid_free,
		usb_hid_control,
		usb_hid_read,
		usb_hid_write,
		NULL,				/* select */
		NULL				/* deselect */
	};

	TRACE("find_device(%s)\n", name);
	if (gDeviceList->FindDevice(name) == NULL) {
		TRACE_ALWAYS("didn't find device %s\n", name);
		return NULL;
	}

	return &hooks;
}
