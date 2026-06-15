#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <objidl.h>
#include <gdiplus.h>

#include "cafeteria_renderer.h"
#include "cafeteria_simulation.h"

extern "C" __declspec(dllimport) BOOL WINAPI SetProcessDPIAware();

namespace {

cafeteria::SimulationState gState;
bool gPaused = false;

LRESULT CALLBACK WindowProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
        case WM_CREATE:
            cafeteria::BuildLayout(gState);
            cafeteria::ResetSimulation(gState);
            SetTimer(hwnd, cafeteria::kTimerId, cafeteria::kFrameMs, nullptr);
            return 0;

        case WM_TIMER:
            if (wParam == cafeteria::kTimerId && !gPaused) {
                gState.time += cafeteria::kFrameSeconds;
                cafeteria::UpdateSimulation(gState);
                InvalidateRect(hwnd, nullptr, FALSE);
            }
            return 0;

        case WM_KEYDOWN:
            if (wParam == VK_SPACE) {
                gPaused = !gPaused;
                InvalidateRect(hwnd, nullptr, FALSE);
            } else if (wParam == 'R') {
                cafeteria::ResetSimulation(gState);
                InvalidateRect(hwnd, nullptr, FALSE);
            }
            return 0;

        case WM_PAINT: {
            PAINTSTRUCT ps;
            HDC dc = BeginPaint(hwnd, &ps);
            cafeteria::RenderSimulation(hwnd, dc, gState, gPaused);
            EndPaint(hwnd, &ps);
            return 0;
        }

        case WM_DESTROY:
            KillTimer(hwnd, cafeteria::kTimerId);
            PostQuitMessage(0);
            return 0;
    }

    return DefWindowProc(hwnd, msg, wParam, lParam);
}

}  // namespace

int WINAPI WinMain(HINSTANCE instance, HINSTANCE, LPSTR, int showCmd) {
    SetProcessDPIAware();
    Gdiplus::GdiplusStartupInput gdiplusInput;
    ULONG_PTR gdiplusToken = 0;
    if (Gdiplus::GdiplusStartup(&gdiplusToken, &gdiplusInput, nullptr) != Gdiplus::Ok) {
        return 1;
    }

    const wchar_t* className = L"CafeteriaSeatFlowWindow";

    WNDCLASSW wc{};
    wc.lpfnWndProc = WindowProc;
    wc.hInstance = instance;
    wc.lpszClassName = className;
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wc.hbrBackground = static_cast<HBRUSH>(GetStockObject(WHITE_BRUSH));

    if (!RegisterClassW(&wc)) {
        return 1;
    }

    RECT rect{0, 0, cafeteria::kWindowWidth, cafeteria::kWindowHeight};
    AdjustWindowRect(&rect, WS_OVERLAPPEDWINDOW, FALSE);
    HWND hwnd = CreateWindowExW(
        0,
        className,
        L"北京交通大学就餐仿真动画",
        WS_OVERLAPPEDWINDOW & ~WS_MAXIMIZEBOX,
        CW_USEDEFAULT,
        CW_USEDEFAULT,
        rect.right - rect.left,
        rect.bottom - rect.top,
        nullptr,
        nullptr,
        instance,
        nullptr);

    if (!hwnd) {
        return 1;
    }

    ShowWindow(hwnd, showCmd);
    UpdateWindow(hwnd);

    MSG msg{};
    while (GetMessage(&msg, nullptr, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    Gdiplus::GdiplusShutdown(gdiplusToken);
    return static_cast<int>(msg.wParam);
}
