#include "clsSysTray.h"
#include "resource.h"


clsSysTray::clsSysTray()
{
	bInTray = false;
	NotifyIconData.cbSize = sizeof(NotifyIconData);
	NotifyIconData.uID = uID = 2;
	NotifyIconData.uFlags = NIF_ICON | NIF_TIP | NIF_MESSAGE;
	NotifyIconData.uCallbackMessage = WM_TRAYNOTIFY;
	NotifyIconData.hIcon = LoadIcon(GetModuleHandle(NULL), MAKEINTRESOURCE(IDR_MAINFRAME));
	NotifyIconData.szTip[0] = '\0';
	NotifyIconData.hWnd = NULL;


}


clsSysTray::~clsSysTray()
{
}

BOOL clsSysTray::SetIcon(HICON hNewIcon)
{
	NotifyIconData.hIcon = hNewIcon;
	if (bInTray)
	{
		BOOL iRetVal;
		iRetVal = Shell_NotifyIcon(NIM_MODIFY, &NotifyIconData);
		if (iRetVal)
		{
			bInTray = true;
		}
		return iRetVal;
	}
	else
		return (1);
}

HICON clsSysTray::GetIcon()
{
	return NotifyIconData.hIcon;
}

BOOL clsSysTray::SetTipText(char *lpstrNewTipText)
{
	//strncpy(NotifyIconData.szTip, lpstrNewTipText);
	if (bInTray)
	{
		BOOL iRetVal;
		iRetVal = Shell_NotifyIcon(NIM_MODIFY, &NotifyIconData);
		if (iRetVal)
		{
			bInTray = true;
		}
		return iRetVal;
	}
	else
		return (1);
}

char *clsSysTray::GetTipText()
{
	return "test"; // NotifyIconData.szTip;
}

BOOL clsSysTray::AddIcon()
{
	BOOL iRetVal;
	NotifyIconData.hWnd = hWnd;
	NotifyIconData.uID = uID;
	iRetVal = Shell_NotifyIcon(NIM_ADD, &NotifyIconData);
	if (iRetVal)
	{
		bInTray = true;

	}
	return iRetVal;
}

BOOL clsSysTray::RemoveIcon()
{
	BOOL iRetVal;
	iRetVal = Shell_NotifyIcon(NIM_DELETE, &NotifyIconData);
	if (iRetVal)
	{
		bInTray = false;
	}
	return iRetVal;
}
