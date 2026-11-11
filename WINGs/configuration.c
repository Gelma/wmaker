
#include "WINGsP.h"
#include "wconfig.h"

#include <X11/Xlocale.h>

_WINGsConfiguration WINGsConfiguration;

#define SYSTEM_FONT "sans serif"
#define BOLD_SYSTEM_FONT "sans serif:bold"
#define DEFAULT_FONT_SIZE 12

#define FLOPPY_PATH "/floppy"

/* WMGetUDStringForKey() returns a pointer into the defaults proplist, which
 * WMSynchronizeUserDefaults() releases when the defaults are reloaded: the
 * configuration must keep its own copy, and must not free the borrowed one. */
static char *copyConfigString(WMUserDefaults *defaults, const char *key)
{
	char *s = WMGetUDStringForKey(defaults, key);

	return s ? wstrdup(s) : NULL;
}

static unsigned getButtonWithName(const char *name, unsigned defaultButton)
{
	if (strncmp(name, "Button", 6) == 0 && strlen(name) == 7) {
		switch (name[6]) {
		case '1':
			return Button1;
		case '2':
			return Button2;
		case '3':
			return Button3;
		case '4':
			return Button4;
		case '5':
			return Button5;
		default:
			break;
		}
	}

	return defaultButton;
}

void W_ReadConfigurations(void)
{
	WMUserDefaults *defaults;
	Bool aaIsSet = False;

	memset(&WINGsConfiguration, 0, sizeof(_WINGsConfiguration));

	defaults = WMGetStandardUserDefaults();

	if (defaults) {
		char *buttonName;
		WMPropList *val;
		unsigned button;

		WINGsConfiguration.systemFont = copyConfigString(defaults, "SystemFont");

		WINGsConfiguration.boldSystemFont = copyConfigString(defaults, "BoldSystemFont");

		val = WMGetUDObjectForKey(defaults, "AntialiasedText");
		if (val && WMIsPLString(val) && WMGetFromPLString(val)) {
			aaIsSet = True;
			WINGsConfiguration.antialiasedText =
				WMGetUDBoolForKey(defaults, "AntialiasedText");
		}

		WINGsConfiguration.doubleClickDelay = WMGetUDIntegerForKey(defaults, "DoubleClickTime");

		WINGsConfiguration.floppyPath = copyConfigString(defaults, "FloppyPath");

		/* buttonName is borrowed from the defaults proplist: it must not be
		 * freed here (it used to be, which freed memory still owned by the
		 * database). */
		buttonName = WMGetUDStringForKey(defaults, "MouseWheelUp");
		if (buttonName) {
			button = getButtonWithName(buttonName, Button4);
		} else {
			button = Button4;
		}
		WINGsConfiguration.mouseWheelUp = button;

		buttonName = WMGetUDStringForKey(defaults, "MouseWheelDown");
		if (buttonName) {
			button = getButtonWithName(buttonName, Button5);
		} else {
			button = Button5;
		}
		WINGsConfiguration.mouseWheelDown = button;

		if (WINGsConfiguration.mouseWheelDown == WINGsConfiguration.mouseWheelUp) {
			WINGsConfiguration.mouseWheelUp = Button4;
			WINGsConfiguration.mouseWheelDown = Button5;
		}

		WINGsConfiguration.defaultFontSize = WMGetUDIntegerForKey(defaults, "DefaultFontSize");
	}

	if (!WINGsConfiguration.systemFont) {
		WINGsConfiguration.systemFont = SYSTEM_FONT;
	}
	if (!WINGsConfiguration.boldSystemFont) {
		WINGsConfiguration.boldSystemFont = BOLD_SYSTEM_FONT;
	}
	if (WINGsConfiguration.defaultFontSize == 0) {
		WINGsConfiguration.defaultFontSize = DEFAULT_FONT_SIZE;
	}
	if (!aaIsSet) {
		WINGsConfiguration.antialiasedText = True;
	}
	if (!WINGsConfiguration.floppyPath) {
		WINGsConfiguration.floppyPath = FLOPPY_PATH;
	}
	if (WINGsConfiguration.doubleClickDelay == 0) {
		WINGsConfiguration.doubleClickDelay = 250;
	}
	if (WINGsConfiguration.mouseWheelUp == 0) {
		WINGsConfiguration.mouseWheelUp = Button4;
	}
	if (WINGsConfiguration.mouseWheelDown == 0) {
		WINGsConfiguration.mouseWheelDown = Button5;
	}

}

unsigned W_getconf_mouseWheelUp(void)
{
	return WINGsConfiguration.mouseWheelUp;
}

unsigned W_getconf_mouseWheelDown(void)
{
	return WINGsConfiguration.mouseWheelDown;
}

void W_setconf_doubleClickDelay(int value)
{
	WINGsConfiguration.doubleClickDelay = value;
}
