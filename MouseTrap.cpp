#include <windows.h>

// Global variables
RECT primaryMonRect = { 0 };
HWND g_hwnd = NULL;
HANDLE g_hThread = NULL;
volatile BOOL g_bRunning = TRUE;
volatile BOOL g_bConfineEnabled = TRUE;
DWORD g_dwThreadId = 0;

// Define constants for shell notification
#define WM_USER_SHELLICON (WM_USER + 1)
#define ID_TRAY_EXIT      1001
#define ID_TRAY_TOGGLE    1002
#define NIM_ADD       0x00000000
#define NIM_MODIFY    0x00000001
#define NIM_DELETE    0x00000002
#define NIF_MESSAGE   0x00000001
#define NIF_ICON      0x00000002
#define NIF_TIP       0x00000004

// Define NOTIFYICONDATA structure manually
typedef struct {
    DWORD cbSize;
    HWND hWnd;
    UINT uID;
    UINT uFlags;
    UINT uCallbackMessage;
    HICON hIcon;
    CHAR szTip[64];
} MY_NOTIFYICONDATA;

// Function declarations
LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
DWORD WINAPI MouseConfinerThread(LPVOID lpParam);
void RegisterToggleHotkey(HWND hwnd);
void CreateTrayIcon(HWND hwnd);
void ToggleConfinement(HWND hwnd);
void RemoveTrayIcon(HWND hwnd);
BOOL Shell_NotifyIcon(DWORD dwMessage, MY_NOTIFYICONDATA* lpData);
BOOL StartConfinerThread();
void StopConfinerThread();

// strcpy for strings implementation
void my_strcpy(char* dest, const char* src) {
    while (*src) {
        *dest++ = *src++;
    }
    *dest = '\0';
}

// Threaded mouse confinement
DWORD WINAPI MouseConfinerThread(LPVOID lpParam) {
    // Apply initial boundary
    ClipCursor(&primaryMonRect);

    // Keep applying the boundary while running
    while (g_bRunning) {
        ClipCursor(&primaryMonRect);
        Sleep(100);
    }

    // Remove restriction before exiting
    ClipCursor(NULL);
    return 0;
}

// Start the confiner thread
BOOL StartConfinerThread() {
    // Set the running flag
    g_bRunning = TRUE;

    // Create thread to monitor cursor position
    g_hThread = CreateThread(NULL, 0, MouseConfinerThread, NULL, 0, &g_dwThreadId);
    if (!g_hThread) {
        return FALSE;
    }

    return TRUE;
}

// Stop the confiner thread
void StopConfinerThread() {
    if (g_hThread) {
        // Signal thread to exit
        g_bRunning = FALSE;

        // Give the thread time to exit gracefully
        WaitForSingleObject(g_hThread, 1000);

        // Clean up the handle
        CloseHandle(g_hThread);
        g_hThread = NULL;

        // Make sure the cursor is free
        ClipCursor(NULL);
    }
}

// Register the window class
ATOM RegisterAppWindowClass(HINSTANCE hInstance) {
    WNDCLASSEX wcex;

    wcex.cbSize = sizeof(WNDCLASSEX);
    wcex.style = 0;
    wcex.lpfnWndProc = WindowProc;
    wcex.cbClsExtra = 0;
    wcex.cbWndExtra = 0;
    wcex.hInstance = hInstance;
    wcex.hIcon = LoadIcon(NULL, IDI_APPLICATION);
    wcex.hCursor = LoadCursor(NULL, IDC_ARROW);
    wcex.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    wcex.lpszMenuName = NULL;
    wcex.lpszClassName = "MouseConfinerClass";
    wcex.hIconSm = LoadIcon(NULL, IDI_APPLICATION);

    return RegisterClassEx(&wcex);
}

// Create system tray icon
void CreateTrayIcon(HWND hwnd) {
    MY_NOTIFYICONDATA nid;
    nid.cbSize = sizeof(MY_NOTIFYICONDATA);
    nid.hWnd = hwnd;
    nid.uID = 1;
    nid.uFlags = NIF_ICON | NIF_MESSAGE | NIF_TIP;
    nid.uCallbackMessage = WM_USER_SHELLICON;
    nid.hIcon = LoadIcon(NULL, IDI_APPLICATION);

    // Use our safe string copy
    my_strcpy(nid.szTip, "MouseTrap (Enabled)");

    Shell_NotifyIcon(NIM_ADD, &nid);
}

// Remove tray icon
void RemoveTrayIcon(HWND hwnd) {
    MY_NOTIFYICONDATA nid;
    nid.cbSize = sizeof(MY_NOTIFYICONDATA);
    nid.hWnd = hwnd;
    nid.uID = 1;
    Shell_NotifyIcon(NIM_DELETE, &nid);
}

// Toggle confinement state
void ToggleConfinement(HWND hwnd) {
    g_bConfineEnabled = !g_bConfineEnabled;

    // Update tray icon tooltip
    MY_NOTIFYICONDATA nid;
    nid.cbSize = sizeof(MY_NOTIFYICONDATA);
    nid.hWnd = hwnd;
    nid.uID = 1;
    nid.uFlags = NIF_TIP;

    if (g_bConfineEnabled) {
        my_strcpy(nid.szTip, "MouseTrap (Enabled)");

        // Start the thread when enabled
        StartConfinerThread();
    }
    else {
        my_strcpy(nid.szTip, "MouseTrap (Disabled)");

        // Stop the thread when disabled
        StopConfinerThread();
    }

    Shell_NotifyIcon(NIM_MODIFY, &nid);
}

// Window procedure
LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    switch (uMsg) {
    case WM_HOTKEY:
        if (wParam == 1) {
            ToggleConfinement(hwnd);
        }
        return 0;

    case WM_USER_SHELLICON:
        if (lParam == WM_LBUTTONUP) {
            ToggleConfinement(hwnd);
        }
        return 0;

    case WM_DESTROY:
        // Remove tray icon
        RemoveTrayIcon(hwnd);

        // Stop the thread if it's running
        StopConfinerThread();

        PostQuitMessage(0);
        return 0;
    }

    return DefWindowProc(hwnd, uMsg, wParam, lParam);
}

// Register hotkey
void RegisterToggleHotkey(HWND hwnd) {
    // Register Ctrl+Alt+C as the toggle hotkey
    RegisterHotKey(hwnd, 1, MOD_CONTROL | MOD_ALT, 'C');
}

// Entry point - must be exactly "main" since that's what the makefile specifies
int main() {
    HINSTANCE hInstance = GetModuleHandle(NULL);

    // Get primary monitor dimensions
    POINT ptZero = { 0, 0 };
    HMONITOR hMonitor = MonitorFromPoint(ptZero, MONITOR_DEFAULTTOPRIMARY);
    MONITORINFO mi = { sizeof(MONITORINFO) };
    if (GetMonitorInfo(hMonitor, &mi)) {
        primaryMonRect = mi.rcMonitor;
    }
    else {
        // Fallback to basic dimensions
        primaryMonRect.left = 0;
        primaryMonRect.top = 0;
        primaryMonRect.right = GetSystemMetrics(SM_CXSCREEN);
        primaryMonRect.bottom = GetSystemMetrics(SM_CYSCREEN);
    }

    // Register window class
    if (!RegisterAppWindowClass(hInstance)) {
        return 1;
    }

    // Create window (invisible)
    g_hwnd = CreateWindowEx(
        0,
        "MouseConfinerClass",
        "MouseTrap",
        WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT, CW_USEDEFAULT,
        400, 300,
        NULL, NULL, hInstance, NULL
    );

    if (!g_hwnd) {
        return 1;
    }

    // Create tray icon
    CreateTrayIcon(g_hwnd);

    // Register hotkey
    RegisterToggleHotkey(g_hwnd);

    // Start the confiner thread initially (since we start enabled)
    if (!StartConfinerThread()) {
        DestroyWindow(g_hwnd);
        return 1;
    }

    // Message loop
    MSG msg;
    while (GetMessage(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    return (int)msg.wParam;
}

// Our own implementation of Shell_NotifyIcon for linking purposes
BOOL Shell_NotifyIcon(DWORD dwMessage, MY_NOTIFYICONDATA* lpData) {
    // Get the function address from shell32.dll
    static BOOL(WINAPI * pfnShell_NotifyIconA)(DWORD, void*) = NULL;

    if (!pfnShell_NotifyIconA) {
        HMODULE hShell32 = LoadLibrary("shell32.dll");
        if (hShell32) {
            pfnShell_NotifyIconA = (BOOL(WINAPI*)(DWORD, void*))GetProcAddress(hShell32, "Shell_NotifyIconA");
        }

        if (!pfnShell_NotifyIconA) {
            return FALSE;
        }
    }

    return pfnShell_NotifyIconA(dwMessage, lpData);
}
