#include <windows.h>

#include "platform.h"

// This is the game Layer
#include "game/game.cpp"

#include "assets/assets.cpp"

// This is the rendering Layer
#include "renderer/vk_renderer.cpp"

// This is the Input Layer
#include "input.cpp"

global_variable bool running = true;
global_variable HWND window;
global_variable InputState input;

LRESULT CALLBACK platform_window_callback(HWND window, UINT msg, WPARAM wParam, LPARAM lParam)
{
    switch (msg)
    {

    case WM_DROPFILES:
    {
        HDROP drop = (HDROP)wParam;

        uint32_t fileCount = DragQueryFileA(drop, INVALID_IDX, 0, 0);

        for (uint32_t i = 0; i < fileCount; i++)
        {
            uint32_t fileNameLength = DragQueryFileA(drop, i, 0, 0);

            char filePath[500] = {};

            DragQueryFileA(drop, i, filePath, fileNameLength + 1);

            // Convert File to DDS and copy into Assets folder
            {
                char command[300] = {};

                sprintf(command, "lib\\texconv.exe -y -m 1 -f R8G8B8A8_UNORM \"%s\" -o assets/textures",
                        filePath);

                CAKEZ_ASSERT(!system(command), "Failed to import Asset: %s", filePath);
            }
        }
    }
    break;

    case WM_KEYDOWN:
    case WM_KEYUP:
    {
        bool isDown = msg == WM_KEYDOWN ? true : false;
        uint32_t keyID = INVALID_IDX;

        switch ((int)wParam)
        {
        case 'A':
            keyID = A_KEY;
            break;

        case 'D':
            keyID = D_KEY;
            break;

        case 'S':
            keyID = S_KEY;
            break;

        case 'W':
            keyID = W_KEY;
            break;
        }

        if (keyID < KEY_COUNT)
        {
            // if (input.keys[keyID].isDown != isDown)
            // {
            input.keys[keyID].halfTransitionCount++;
            input.keys[keyID].isDown = isDown;
            // }
        }
    }
    break;

    case WM_CLOSE:
        running = false;
        break;
    }

    return DefWindowProcA(window, msg, wParam, lParam);
}

bool platform_create_window()
{
    HINSTANCE instance = GetModuleHandleA(0);

    WNDCLASS wc = {};
    wc.lpfnWndProc = platform_window_callback;
    wc.hInstance = instance;
    wc.lpszClassName = "vulkan_engine_class";
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);

    if (!RegisterClassA(&wc))
    {
        MessageBoxA(0, "Failed registering window class", "Error", MB_ICONEXCLAMATION | MB_OK);
        return false;
    }

    window = CreateWindowExA(
        WS_EX_APPWINDOW,
        "vulkan_engine_class",
        "Pong",
        WS_THICKFRAME | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX | WS_MAXIMIZEBOX | WS_OVERLAPPED,
        100, 100, 1000, 720, 0, 0, instance, 0);

    if (window == 0)
    {
        MessageBoxA(0, "Failed creating window", "Error", MB_ICONEXCLAMATION | MB_OK);
        return false;
    }

#ifdef DEBUG
    DragAcceptFiles(window, true);
#endif

    ShowWindow(window, SW_SHOW);

    return true;
}

void platform_update_window(HWND window)
{
    MSG msg;

    while (PeekMessageA(&msg, window, 0, 0, PM_REMOVE))
    {
        TranslateMessage(&msg);
        DispatchMessageA(&msg);
    }
}

int main()
{
    VkContext vkcontext = {};
    GameState gameState = {};

    if (!platform_create_window())
    {
        CAKEZ_FATAL("Failed to open a Window");
        return -1;
    }

    if (!init_game(&gameState))
    {
        CAKEZ_FATAL("Failed to initialize Game");
        return -1;
    }

    if (!vk_init(&vkcontext, window))
    {
        CAKEZ_FATAL("Failed to initialize Vulkan");
        return -1;
    }

    while (running)
    {
        // Clear out transition count
        {
            for (uint32_t i = 0; i < KEY_COUNT; i++)
            {
                input.keys[i].halfTransitionCount = 0;
            }
        }

        platform_update_window(window);
        update_game(&gameState, &input);
        if (!vk_render(&vkcontext, &gameState))
        {
            CAKEZ_FATAL("Failed to render Vulkan");
            return -1;
        }
    }

    return 0;
}

// ########################################################################
//                   Implementation of exposed platform functions
// ########################################################################

void platform_get_window_size(uint32_t *width, uint32_t *heigth)
{
    RECT rect;
    GetClientRect(window, &rect);

    *width = rect.right - rect.left;
    *heigth = rect.bottom - rect.top;
}

char *platform_read_file(char *path, uint32_t *length)
{
    char *result = 0;

    // This opens the file
    HANDLE file = CreateFileA(path, GENERIC_READ, FILE_SHARE_READ, 0, OPEN_EXISTING, 0, 0);

    if (file != INVALID_HANDLE_VALUE)
    {
        LARGE_INTEGER size;
        if (GetFileSizeEx(file, &size))
        {
            *length = size.QuadPart;
            //TODO: Suballocte from main allocation
            result = new char[*length];

            DWORD bytesRead;
            if (ReadFile(file, result, *length, &bytesRead, 0) &&
                bytesRead == *length)
            {
                // TODO: What to do here?
                // Success
            }
            else
            {
                CAKEZ_ASSERT(0, "Failed to read file: %s", path);
                CAKEZ_ERROR("Failed to read file: %s", path);
            }
        }
        else
        {
            CAKEZ_ASSERT(0, "Failed to get size of file: %s", path);
            CAKEZ_ERROR("Failed to get size of file: %s", path);
        }
    }
    else
    {
        CAKEZ_ASSERT(0, "Failed to open file: %s", path);
        CAKEZ_ERROR("Failed to open file: %s", path);
    }

    return result;
}

void platform_log(const char *msg, TextColor color)
{
    HANDLE consoleHandle = GetStdHandle(STD_OUTPUT_HANDLE);

    uint32_t colorBits = 0;

    switch (color)
    {
    case TEXT_COLOR_WHITE:
        colorBits = FOREGROUND_BLUE | FOREGROUND_GREEN | FOREGROUND_RED;
        break;

    case TEXT_COLOR_GREEN:
        colorBits = FOREGROUND_GREEN;
        break;

    case TEXT_COLOR_YELLOW:
        colorBits = FOREGROUND_GREEN | FOREGROUND_RED;
        break;

    case TEXT_COLOR_RED:
        colorBits = FOREGROUND_RED;
        break;

    case TEXT_COLOR_LIGHT_RED:
        colorBits = FOREGROUND_RED | FOREGROUND_INTENSITY;
        break;
    }

    SetConsoleTextAttribute(consoleHandle, colorBits);

#ifdef DEBUG
    OutputDebugStringA(msg);
#endif

    WriteConsoleA(consoleHandle, msg, strlen(msg), 0, 0);
}