#ifndef _UIS_STRING_H
#define _UIS_STRING_H

#include <SupportDefs.h>


class UISString {
public:
	static char *	StrDupTrim(const char *str);

					UISString();
					~UISString();

	void			SetTo(int fd, uint32 id);
	const char *	String();

private:
	int				fDevice;
	uint32			fId;
	char *			fString;
	bool			fIsValid;
};


#endif // _UIS_STRING_H
