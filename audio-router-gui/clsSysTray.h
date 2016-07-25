#pragma once
#pragma warning(disable: 4996)

#include <windows.h>

#define WM_TRAYNOTIFY	0XA44C
#include <shellapi.h>

class clsSysTray
{
public:
	clsSysTray();
	~clsSysTray();
	BOOL SetIcon(HICON hNewIcon);
	HICON GetIcon();
	BOOL SetTipText(char *lpstrNewTipText);
	char *GetTipText();
	BOOL AddIcon();
	BOOL RemoveIcon();
	HWND hWnd;
	UINT uID;
protected:
	NOTIFYICONDATA NotifyIconData;
	bool bInTray;
};


