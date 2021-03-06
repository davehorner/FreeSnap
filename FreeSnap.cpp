// Copyright (c) 2008 Blue Onion Software
// All rights reserved

#include "stdafx.h"
#include <stdio.h>
#include <shellapi.h>
#include <shlwapi.h>
#include "FSnap.h"
#include "FreeSnap.h"

// ----------------------------------------------------------------------------

ATOM				MyRegisterClass(HINSTANCE hInstance);
bool				InitInstance(HINSTANCE);
LRESULT CALLBACK	WndProc(HWND, UINT, WPARAM, LPARAM);
INT_PTR CALLBACK    About(HWND window, UINT message, WPARAM wpar, LPARAM lpar);
INT_PTR CALLBACK    Help(HWND window, UINT message, WPARAM wpar, LPARAM lpar);
void                Alert(LPCTSTR text);
void                Donate();
void                ReadConfigurationFile();
bool                StringCompare(const char* left, const char* right, int length);

// ----------------------------------------------------------------------------

UINT g_close_free_snap = RegisterWindowMessage(L"CloseFreeSnap");
UINT g_help_free_snap = RegisterWindowMessage(L"HelpFreeSnap");
bool g_banner = true;
bool g_use_alternate_keys = true;
bool g_help_dialog = false;
bool g_undo = false;
bool g_task_switch = true;
std::vector<SIZE> g_sizes;
HINSTANCE g_hinstance;
const int MAX_SIZES_ARRAY = 20;

// ----------------------------------------------------------------------------

int APIENTRY WinMain(HINSTANCE hInstance,
                     HINSTANCE hPrevInstance,
                     LPSTR     lpCmdLine,
                     int     /*nCmdShow*/)
{
    g_hinstance = hInstance;

    char* next_token = NULL;
    char* token = strtok_s(lpCmdLine, " ", &next_token);

    while (token != NULL)
    {
        if (StringCompare(token, "-stop", -1))
        {
            PostMessage(HWND_BROADCAST, g_close_free_snap, 0, 0);
            return 0;
        }

        token = strtok_s(NULL, " ", &next_token);
    }

    // Insure only one instance of FreeSnap is running
    //
    if (OpenEvent(EVENT_MODIFY_STATE, FALSE, L"FreeSnap") != NULL)
        return 0;

    MyRegisterClass(hInstance);

    if (InitInstance (hInstance) == false) 
        return FALSE;

    MSG message;    
    while (GetMessage(&message, NULL, 0, 0)) 
    {
        TranslateMessage(&message);
        DispatchMessage(&message);
    }

    snap_uninstall();
    return (int)message.wParam;
}

// ----------------------------------------------------------------------------

ATOM MyRegisterClass(HINSTANCE hInstance)
{
    WNDCLASSEX wcex;
    wcex.cbSize			= sizeof(WNDCLASSEX); 
    wcex.style			= CS_HREDRAW | CS_VREDRAW;
    wcex.lpfnWndProc	= (WNDPROC)WndProc;
    wcex.cbClsExtra		= 0;
    wcex.cbWndExtra		= 0;
    wcex.hInstance		= hInstance;
    wcex.hIcon			= LoadIcon(hInstance, (LPCTSTR)IDI_FREESNAP);
    wcex.hCursor		= LoadCursor(NULL, IDC_ARROW);
    wcex.hbrBackground	= (HBRUSH)(COLOR_WINDOW+1);
    wcex.lpszMenuName	= (LPCWSTR)IDC_FREESNAP;
    wcex.lpszClassName	= L"FREESNAP";
    wcex.hIconSm		= LoadIcon(wcex.hInstance, (LPCTSTR)IDI_FREESNAP);

    return RegisterClassEx(&wcex);
}

// ----------------------------------------------------------------------------

bool InitInstance(HINSTANCE hInstance)
{
    HWND window = CreateWindow(L"FREESNAP", L"FreeSnap", 
        WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, 0, CW_USEDEFAULT,
        0, NULL, NULL, hInstance, NULL);

    if (window == NULL)
    {
        Alert(L"Create window failed");
        return false;
    }

    HWND currentWindow = GetForegroundWindow();
    ShowWindow(window, SW_HIDE);
    UpdateWindow(window);

    CreateEvent(0, 0, 0, L"FreeSnap"); // used to detect previous instance
    ReadConfigurationFile();

    if (snap_install(g_sizes, g_use_alternate_keys, g_undo, g_task_switch) == false)
    {
        Alert(L"Could not install keyboard hook");
        return false;
    }

    if (g_banner == true)
    {
        DialogBox(hInstance, (LPCWSTR)IDM_ABOUT, window, About);
        SetForegroundWindow(currentWindow);
    }

    return true;
}

// ----------------------------------------------------------------------------

bool StringCompare(const char* left, const char* right, int length)
{
    int result = CompareStringA(LOCALE_INVARIANT, NORM_IGNORECASE, left, -1, right, length);
    return result == CSTR_EQUAL;
}

// ----------------------------------------------------------------------------

void ValueToBool(char* value, bool *boolArg)
{
    if (StringCompare(value, "yes", -1) || StringCompare(value, "true", -1))
        *boolArg = true;

    else if (StringCompare(value, "no", -1) || StringCompare(value, "false", -1))
        *boolArg = false;
}

// ----------------------------------------------------------------------------

void SetOption(char* option, char* value)
{
    if (StringCompare(option, "banner", -1))
        ValueToBool(value, &g_banner);
    
    else if (StringCompare(option, "usealtkeys", -1))
        ValueToBool(value, &g_use_alternate_keys);

    else if (StringCompare(option, "undo", -1))
        ValueToBool(value, &g_undo);

    else if (StringCompare(option, "taskswitch", -1))
        ValueToBool(value, &g_task_switch);

    else if (StringCompare(option, "size", -1))
    {
        char* width;
        char* height;
        char* next_token;

        if ((width = strtok_s(value, ",", &next_token)) != NULL && 
            (height = strtok_s(NULL, "", &next_token)) != NULL)
        {
            int w = atoi(width);
            int h = atoi(height);

            if (w > 100 && h > 100)
            {
                SIZE size;
                size.cx = w;
                size.cy = h;
                g_sizes.push_back(size);
            }
        }
    }
}

// ----------------------------------------------------------------------------

void ReadConfigurationFile()
{
    wchar_t path[1024];
    GetModuleFileName(NULL, path, sizeof(path));
    int length = lstrlen(path);
    path[length - 3] = 'c';
    path[length - 2] = 'f';
    path[length - 1] = 'g';

    FILE* fp = NULL; 
    _wfopen_s(&fp, path, L"r");

    if (fp != NULL)
    {
        char buffer[1024];

        while (fgets(buffer, sizeof(buffer), fp) != NULL)
        {
            buffer[sizeof(buffer) - 1] = '\0';
            char* token = strchr(buffer, ':');

            if (token != NULL)
            {
                *token = '\0';
                char* key = buffer;
                char* value = token + 1;
                const char* white_space = "\x20\t\r\n\v\a";
                StrTrimA(key, white_space);
                StrTrimA(value, white_space);
                SetOption(key, value);
            }
        }

        fclose(fp);
    }
}

// ----------------------------------------------------------------------------

LRESULT CALLBACK WndProc(HWND window, UINT message, WPARAM wpar, LPARAM lpar)
{
    if (message == g_close_free_snap)
    {
        PostQuitMessage(0);
    }

    else if (message == g_help_free_snap)
    {
        if (g_help_dialog == false)
        {
            g_help_dialog = true;
            DialogBox(g_hinstance, (LPCWSTR)IDD_HELP, window, Help);
            g_help_dialog = false;
        }
    }

    return DefWindowProc(window, message, wpar, lpar);
}

// ----------------------------------------------------------------------------

INT_PTR CALLBACK About(HWND window, UINT message, WPARAM wpar, LPARAM lpar)
{
    switch (message)
    {
    case WM_INITDIALOG:
        SetTimer(window, 1, 2000, 0);
        return TRUE;

    case WM_TIMER:
        KillTimer(window, 1);
        EndDialog(window, 0);
        return TRUE;
    }

    return FALSE;
}

// ----------------------------------------------------------------------------

INT_PTR CALLBACK Help(HWND window, UINT message, WPARAM wpar, LPARAM lpar)
{
    switch (message)
    {
    case WM_INITDIALOG: {
        HWND title = GetDlgItem(window, IDC_TITLE);
        HFONT font = (HFONT)SendMessage(title, WM_GETFONT, 0, 0);
        LOGFONT lf;
        GetObject(font, sizeof(lf), &lf);
        lf.lfHeight -= 4;
        lf.lfWeight = FW_BOLD;
        HFONT font2 = CreateFontIndirect(&lf);
        SendMessage(title, WM_SETFONT, (WPARAM)font2, 0);

        HWND textbox = GetDlgItem(window, IDC_KEYMAP2);
        const wchar_t* standard_keys = L"Win + ↑\t\tsnap top\rWin + ↓\t\tsnap bottom\rWin + ←\t\tsnap left\rWin + →\t\tsnap right\r\rWin + Home\t\tmove to top-left corner\rWin + PgUp\t\tmove to top-right corner\rWin + End\t\tmove to bottom-left corner\rWin + PgDn\t\tmove to bottom-right corner\r\rWin + 5\t\t\tcenter window/next monitor\rWin + *\t\t\tcenter window\rWin + Plus\t\tresize up\rWin + Minus\t\tresize down\r\rWin + Shift + ↑\tgrow top\rWin + Shift + ↓\tgrow bottom\rWin + Shift + ←\tgrow left\rWin + Shift + →\tgrow right\r\rWin + Ctrl + ↑\t\tshrink top\rWin + Ctrl + ↓\t\tshrink bottom\rWin + Ctrl + ←\t\tshrink left\rWin + Ctrl + →\t\tshink right\r\rWin + .\t\t\tminimize\rWin + Enter\t\tmaximize\rWin + /\t\t\tclose\rWin + ?\t\t\thelp\rWin + TAB\t\tswitch to next task\rWin + `\t\t\tswitch to previous task\rWin+Shift+TAB\t\tswitch to previous task";
        SendMessage(textbox, WM_SETTEXT, 0, (LPARAM)standard_keys);

        textbox = GetDlgItem(window, IDC_KEYMAP);
        const wchar_t* alternate_keys = L"Win + I\t\t\tsnap top\rWin + K\t\t\tsnap bottom\rWin + J\t\t\tsnap left\rWin + L\t\t\tsnap right\r\rWin + T\t\t\tmove to top-left corner\rWin + Y\t\t\tmove to top-right corner\rWin + G\t\t\tmove to bottom-left corner\rWin + H\t\t\tmove to bottom-right corner\r\rWin + C\t\t\tcenter window/next monitor\rWin + Z\t\t\tresize up\rWin + X\t\t\tresize down\r\rWin + Shift + I\t\tgrow top\rWin + Shift + K\t\tgrow bottom\rWin + Shift + J\t\tgrow left\rWin + Shift + L\t\tgrow right\r\rWin + Ctrl + I\t\tshrink top\rWin + Ctrl + K\t\tshrink bottom\rWin + Ctrl + J\t\tshrink left\rWin + Ctrl + L\t\tshink right\r";
        SendMessage(textbox, WM_SETTEXT, 0, (LPARAM)alternate_keys);

        return TRUE; }

    case WM_COMMAND:
        switch (LOWORD(wpar))
        {
        case IDOK:
            Donate();
            EndDialog(window, 0);
            break;

        case IDCANCEL:
            EndDialog(window, 0);
            break;

        case IDC_HELPME:
            ShellExecute(NULL, L"open", L"http://blueonionsoftware.com/freesnap.aspx", NULL, NULL, SW_SHOW);
            EndDialog(window, 0);
            break;
        }
        break;
    }

    return FALSE;
}

// ----------------------------------------------------------------------------

void Alert(LPCTSTR text)
{
    LPVOID sys_msg;
    DWORD  last_error = GetLastError();

    FormatMessage( 
        FORMAT_MESSAGE_ALLOCATE_BUFFER | 
        FORMAT_MESSAGE_FROM_SYSTEM | 
        FORMAT_MESSAGE_IGNORE_INSERTS,
        NULL,
        last_error,
        MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), // Default language
        (LPTSTR)&sys_msg,
        0,
        NULL);

    LPVOID alert_msg;
    LPVOID args[] = { (LPVOID)text, (LPVOID)last_error, sys_msg };

    try
    {
        FormatMessage(
            FORMAT_MESSAGE_FROM_STRING | 
            FORMAT_MESSAGE_ALLOCATE_BUFFER |
            FORMAT_MESSAGE_ARGUMENT_ARRAY,
            "%1\n\nError:\t0x%2!x!\nText:\t%3",
            0,
            MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), // Default language
            (LPTSTR)&alert_msg,
            0,
            (va_list*)args);

        MessageBox(NULL, (LPCTSTR)alert_msg, L"FreeSnap", MB_OK | MB_ICONERROR);
    }

    catch (...)
    {
        LocalFree(alert_msg);
        LocalFree(sys_msg);
        throw;
    }

    LocalFree(alert_msg);
    LocalFree(sys_msg);
}

void Donate()
{
    ShellExecute(NULL, L"open",
        L"https://www.paypal.com/cgi-bin/webscr?cmd=_donations&business=mike%40blueonionsoftware%2ecom&no_shipping=1&cn=Leave%20a%20note&tax=0&currency_code=USD&lc=US&bn=PP%2dDonationsBF&charset=UTF%2d8",
        NULL, NULL, SW_SHOW);
}