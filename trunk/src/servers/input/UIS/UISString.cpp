#include "UISString.h"

#include <OS.h>
#include <UTF8.h>

#include <UISProtocol.h>
#include "kb_mouse_driver.h"

#include <stdlib.h>
#include <string.h>
#include <new>

#include "UIS_debug.h"


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
