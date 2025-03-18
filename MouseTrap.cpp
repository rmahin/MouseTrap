#include <windows.h>

RECT primaryMonRect = { 0 };

DWORD WINAPI MouseConfinerThread(LPVOID lpParam) {
    while (1) {
        POINT currentPos;
        if (GetCursorPos(&currentPos)) {
            BOOL needsRepositioning = FALSE;
            POINT newPos = currentPos;

            // Check boundaries and adjust if needed
            if (currentPos.x < primaryMonRect.left) {
                newPos.x = primaryMonRect.left;
                needsRepositioning = TRUE;
            }
            else if (currentPos.x >= primaryMonRect.right) {
                newPos.x = primaryMonRect.right - 1;
                needsRepositioning = TRUE;
            }

            if (currentPos.y < primaryMonRect.top) {
                newPos.y = primaryMonRect.top;
                needsRepositioning = TRUE;
            }
            else if (currentPos.y >= primaryMonRect.bottom) {
                newPos.y = primaryMonRect.bottom - 1;
                needsRepositioning = TRUE;
            }

            // Only reposition if needed
            if (needsRepositioning) {
                SetCursorPos(newPos.x, newPos.y);
            }
        }

        // Small sleep to prevent using too much CPU
        Sleep(5);
    }

    return 0;
}

int main() {
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

    // Create thread to monitor cursor position
    HANDLE hThread = CreateThread(NULL, 0, MouseConfinerThread, NULL, 0, NULL);
    if (!hThread) return 1;

    // Create a minimal window just to handle messages
    HWND hwnd = CreateWindowEx(0, "STATIC", "MouseConfiner",
        WS_POPUP, 0, 0, 0, 0, NULL, NULL,
        GetModuleHandle(NULL), NULL);

    // Message loop
    MSG msg;
    while (GetMessage(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    // Clean up
    TerminateThread(hThread, 0);
    CloseHandle(hThread);

    return 0;
}