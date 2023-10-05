#include "video-ffmpeg/canvas.h"
#include "vh/mem.h"

#define WIN32_LEAN_AND_MEAN
#include <Windows.h>

struct canvas
{
    HWND hWnd;
};

/* this is the main message handler for the program */
LRESULT CALLBACK WindowProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    /* sort through and find what code to run for the message given */
    switch (message)
    {
        /* this message is read when the window is closed */
        case WM_DESTROY:
            /* close the application entirely */
            PostQuitMessage(0);
            return 0;
    }

    /* Handle any messages the switch statement didn't */
    return DefWindowProc(hWnd, message, wParam, lParam);
}

struct canvas*
canvas_create(void)
{
    HINSTANCE hInstance;
    WNDCLASSEX wc;

    struct canvas* canvas = mem_alloc(sizeof * canvas);
    if (canvas == NULL)
        goto alloc_canvas_failed;

    hInstance = GetModuleHandle(NULL);

    ZeroMemory(&wc, sizeof(WNDCLASSEX));
    wc.cbSize = sizeof(WNDCLASSEX);
    wc.style = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc = WindowProc;
    wc.hInstance = hInstance;
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)COLOR_WINDOW;
    wc.lpszClassName = "GPUCanvas1";
    if (RegisterClassEx(&wc) == 0)
    {
        log_err("Failed to register window class\n");
        goto register_class_failed;
    }

    canvas->hWnd = CreateWindowExA(0,
        "GPUCanvas1",              /* name of the window class */
        "Our First Windowed Program",   /* title of the window */
        WS_OVERLAPPEDWINDOW,       /* window style */
        300,                       /* x-position of the window */
        300,                       /* y-position of the window */
        500,                       /* width of the window */
        400,                       /* height of the window */
        NULL,                      /* we have no parent window, NULL */
        NULL,                      /* we aren't using menus, NULL */
        hInstance,                 /* application handle */
        NULL);                     /* used with multiple windows, NULL */
    if (canvas->hWnd == NULL)
    {
        log_err("Failed to create window\n");
        goto create_window_failed;
    }

    ShowWindow(canvas->hWnd, SW_SHOW);
    return canvas;

    create_window_failed  : UnregisterClass("GPUCanvas1", hInstance);
    register_class_failed : mem_free(canvas);
    alloc_canvas_failed   : return NULL;
}

void
canvas_destroy(struct canvas* canvas)
{
    HINSTANCE hInstance = GetModuleHandle(NULL);
    DestroyWindow(canvas->hWnd);
    UnregisterClass("GPUCanvas1", hInstance);
}

void*
canvas_get_native_window_handle(struct canvas* canvas)
{
    return canvas->hWnd;
}

void
canvas_main_loop(struct canvas* canvas)
{
    MSG msg;

    while (GetMessage(&msg, NULL, 0, 0))
    {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
}
