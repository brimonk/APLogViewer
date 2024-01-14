// Brian Chrzanowski
//
// APLogViewer - A better log viewing tool
//
// TODO
// - Wrap the global string map into an object (factory).
// - Add UI for filtering on all fields
//   - conditionals
//   - values (csv ex. PID=123,488,999)
//   - ranges (2020-01-04 - 2021-01-01) etc.
// - UI to combine LogFile streams into a single view.
// - Remove console that spawns.
// - Fix more global destruction stuff
// - MemoryMap the Input Files

#include "common.h"
#include "LogEntry.h"
#include "LogFile.h"
#include "dirent.h"

#include "imgui.h"
#include "imgui_impl_win32.h"
#include "imgui_impl_dx10.h"
#include <d3d10_1.h>
#include <d3d10.h>
#include <tchar.h>

using namespace APLogViewer;

// Data
static ID3D10Device           *g_pd3dDevice = nullptr;
static IDXGISwapChain         *g_pSwapChain = nullptr;
static UINT                    g_ResizeWidth = 0, g_ResizeHeight = 0;
static ID3D10RenderTargetView *g_mainRenderTargetView = nullptr;

// Forward declarations of helper functions
bool CreateDeviceD3D(HWND hWnd);
void CleanupDeviceD3D();
void CreateRenderTarget();
void CleanupRenderTarget();
LRESULT WINAPI WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

bool g_ReadInput = true;

std::vector<LogFile *> log_files;

u64 GetTotalLogCount()
{
    u64 total = 0;
    for (i32 i = 0; i < log_files.size(); i++) {
        total += log_files[i]->GetEntriesCount();
    }
    return total;
}

void RenderApp(std::vector<LogFile *> &files);
void RenderTable(LogFile *file, ImVec2 size);

// Main code
int main(int argc, char **argv)
{
    // Create application window
    //ImGui_ImplWin32_EnableDpiAwareness();
    WNDCLASSEXW wc = { sizeof(wc), CS_CLASSDC, WndProc, 0L, 0L, GetModuleHandle(nullptr), nullptr, nullptr, nullptr, nullptr, L"ImGui Example", nullptr };
    ::RegisterClassExW(&wc);
    HWND hwnd = ::CreateWindowW(wc.lpszClassName, L"APLogViewer", WS_OVERLAPPEDWINDOW, 100, 100, 1280, 800, nullptr, nullptr, wc.hInstance, nullptr);

    // Initialize Direct3D
    if (!CreateDeviceD3D(hwnd))
    {
        CleanupDeviceD3D();
        ::UnregisterClassW(wc.lpszClassName, wc.hInstance);
        return 1;
    }

    // Show the window
    ::ShowWindow(hwnd, SW_SHOWDEFAULT);
    ::UpdateWindow(hwnd);

    // Setup Dear ImGui context
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO &io = ImGui::GetIO(); (void)io;
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;     // Enable Keyboard Controls
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;      // Enable Gamepad Controls

    // Setup Dear ImGui style
    ImGui::StyleColorsDark();
    //ImGui::StyleColorsLight();

    // Setup Platform/Renderer backends
    ImGui_ImplWin32_Init(hwnd);
    ImGui_ImplDX10_Init(g_pd3dDevice);

    // Load Fonts
    // - If no fonts are loaded, dear imgui will use the default font. You can also load multiple fonts and use ImGui::PushFont()/PopFont() to select them.
    // - AddFontFromFileTTF() will return the ImFont* so you can store it if you need to select the font among multiple.
    // - If the file cannot be loaded, the function will return a nullptr. Please handle those errors in your application (e.g. use an assertion, or display an error and quit).
    // - The fonts will be rasterized at a given size (w/ oversampling) and stored into a texture when calling ImFontAtlas::Build()/GetTexDataAsXXXX(), which ImGui_ImplXXXX_NewFrame below will call.
    // - Use '#define IMGUI_ENABLE_FREETYPE' in your imconfig file to use Freetype for higher quality font rendering.
    // - Read 'docs/FONTS.md' for more instructions and details.
    // - Remember that in C/C++ if you want to include a backslash \ in a string literal you need to write a double backslash \\ !
    //io.Fonts->AddFontDefault();
    //io.Fonts->AddFontFromFileTTF("c:\\Windows\\Fonts\\segoeui.ttf", 18.0f);
    //io.Fonts->AddFontFromFileTTF("../../misc/fonts/DroidSans.ttf", 16.0f);
    //io.Fonts->AddFontFromFileTTF("../../misc/fonts/Roboto-Medium.ttf", 16.0f);
    //io.Fonts->AddFontFromFileTTF("../../misc/fonts/Cousine-Regular.ttf", 15.0f);
    //ImFont* font = io.Fonts->AddFontFromFileTTF("c:\\Windows\\Fonts\\ArialUni.ttf", 18.0f, nullptr, io.Fonts->GetGlyphRangesJapanese());
    //IM_ASSERT(font != nullptr);

    // Read all of the log entries
    std::vector<LogEntry> entries;
    std::string file;

    for (i32 i = 1; i < argc; i++) {
        struct stat s;
        if (stat(argv[i], &s) == 0) {
            if (s.st_mode & S_IFDIR) {
                DIR *dir;
                struct dirent *ent;
                if ((dir = opendir(argv[i])) != NULL) {
                    while ((ent = readdir(dir)) != NULL) {
                        if (ent->d_type != DT_REG)
                            continue;

                        LogFile *file = new LogFile(std::string(ent->d_name), &g_ReadInput);
                        file->Start();
                        log_files.push_back(file);
                    }
                }
            } else if (s.st_mode & S_IFREG) {
                LogFile *file = new LogFile(std::string(argv[i]), &g_ReadInput);
                file->Start();
                log_files.push_back(file);
            }
        } else {
            // TODO print error in some way
        }
    }

    // Our state
    bool show_demo_window = true;
    ImVec4 clear_color = ImVec4(0.45f, 0.55f, 0.60f, 1.00f);

    // Main loop
    bool done = false;
    while (!done)
    {
        // Poll and handle messages (inputs, window resize, etc.)
        // See the WndProc() function below for our to dispatch events to the Win32 backend.
        MSG msg;
        while (::PeekMessage(&msg, nullptr, 0U, 0U, PM_REMOVE))
        {
            ::TranslateMessage(&msg);
            ::DispatchMessage(&msg);
            if (msg.message == WM_QUIT)
                done = true;
        }
        if (done)
            break;

        // Handle window resize (we don't resize directly in the WM_SIZE handler)
        if (g_ResizeWidth != 0 && g_ResizeHeight != 0)
        {
            CleanupRenderTarget();
            g_pSwapChain->ResizeBuffers(0, g_ResizeWidth, g_ResizeHeight, DXGI_FORMAT_UNKNOWN, 0);
            g_ResizeWidth = g_ResizeHeight = 0;
            CreateRenderTarget();
        }

        // Start the Dear ImGui frame
        ImGui_ImplDX10_NewFrame();
        ImGui_ImplWin32_NewFrame();
        ImGui::NewFrame();

        // 1. Show the big demo window (Most of the sample code is in ImGui::ShowDemoWindow()! You can browse its code to learn more about Dear ImGui!).
        if (show_demo_window)
            ImGui::ShowDemoWindow(&show_demo_window);

        // 2. Show a simple window that we create ourselves. We use a Begin/End pair to create a named window.
        {
            static float f = 0.0f;
            static int counter = 0;

            RenderApp(log_files);

#if 0
            ImGui::Begin("Hello, world!");                          // Create a window called "Hello, world!" and append into it.

            ImGui::Text("This is some useful text.");               // Display some text (you can use a format strings too)
            ImGui::Checkbox("Demo Window", &show_demo_window);      // Edit bools storing our window open/close state
            ImGui::Checkbox("Another Window", &show_another_window);

            ImGui::SliderFloat("float", &f, 0.0f, 1.0f);            // Edit 1 float using a slider from 0.0f to 1.0f
            ImGui::ColorEdit3("clear color", (float *)&clear_color); // Edit 3 floats representing a color

            if (ImGui::Button("Button"))                            // Buttons return true when clicked (most widgets return true when edited/activated)
                counter++;
            ImGui::SameLine();
            ImGui::Text("counter = %d", counter);
#endif
        }

        // Rendering
        ImGui::Render();
        const float clear_color_with_alpha[4] = { clear_color.x * clear_color.w, clear_color.y * clear_color.w, clear_color.z * clear_color.w, clear_color.w };
        g_pd3dDevice->OMSetRenderTargets(1, &g_mainRenderTargetView, nullptr);
        g_pd3dDevice->ClearRenderTargetView(g_mainRenderTargetView, clear_color_with_alpha);
        ImGui_ImplDX10_RenderDrawData(ImGui::GetDrawData());

        g_pSwapChain->Present(1, 0); // Present with vsync
        //g_pSwapChain->Present(0, 0); // Present without vsync
    }

    g_ReadInput = false;

    ImGui_ImplDX10_Shutdown();
    ImGui_ImplWin32_Shutdown();
    ImGui::DestroyContext();

    CleanupDeviceD3D();
    ::DestroyWindow(hwnd);
    ::UnregisterClassW(wc.lpszClassName, wc.hInstance);

    return 0;
}

void RenderApp(std::vector<LogFile *> &files)
{
    const float TEXT_BASE_WIDTH = ImGui::CalcTextSize("A").x;
    const float TEXT_BASE_HEIGHT = ImGui::GetTextLineHeightWithSpacing();

    ImGuiIO &io = ImGui::GetIO();

    static bool use_work_area = true;

    const ImGuiViewport *viewport = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(use_work_area ? viewport->WorkPos : viewport->Pos);
    ImGui::SetNextWindowSize(use_work_area ? viewport->WorkSize : viewport->Size);

    ImGuiWindowFlags window_flags = ImGuiWindowFlags_MenuBar;

    // TODO Window names should relate to what subsection of the logs you're looking at.
    if (ImGui::Begin("APLogViewer", nullptr, window_flags)) {
        if (ImGui::BeginMenuBar()) {
            if (ImGui::BeginMenu("File")) {
                ImGui::EndMenu();
            }
            ImGui::EndMenuBar();
        }

        static int selected = -1;
        {
            // Log File Source(s)
            ImGui::BeginChild("Log Sources", ImVec2(150, 0), ImGuiChildFlags_Border | ImGuiChildFlags_ResizeX);
            
            for (i32 i = 0; i < files.size(); i++) {
                char label[128];
                snprintf(label, sizeof label, "%s", files[i]->filename.c_str());
                if (ImGui::Selectable(label, selected == i))
                    selected = i;
            }
            
            ImGui::EndChild();
        }

        ImGui::SameLine();

        {
            ImVec2 logsize = use_work_area ? viewport->WorkPos : viewport->Pos;
            logsize.x -= 150;

            ImGui::BeginGroup();
            ImGui::BeginChild("Table Stats / Filtering", ImVec2(0, TEXT_BASE_HEIGHT * 4), ImGuiChildFlags_Border | ImGuiChildFlags_ResizeY);

            if (selected >= 0) {
                static i64 records_last_frame = 0;
                i64 records_this_frame = (i64)files[selected]->GetEntriesCount();

                ImGui::Text("Records: %ld (%ld recs/frame)", records_this_frame, records_this_frame - records_last_frame);

                records_last_frame = records_this_frame;
            }
            ImGui::Text("Application average %.3f ms/frame (%.1f FPS)", 1000.0f / io.Framerate, io.Framerate);

            ImGui::EndChild();

            ImGui::BeginChild("Log Contents", ImVec2(0, 0), ImGuiChildFlags_Border | ImGuiChildFlags_ResizeY);

            if (selected >= 0) {
                RenderTable(files[selected], ImVec2(0, logsize.y));
            }

            ImGui::EndChild();

            ImGui::EndGroup();
        }
    }

    ImGui::End();
}

void RenderTable(LogFile *file, ImVec2 size)
{
    const float TEXT_BASE_WIDTH = ImGui::CalcTextSize("A").x;
    const float TEXT_BASE_HEIGHT = ImGui::GetTextLineHeightWithSpacing();

    ImGuiTableFlags flags =
        ImGuiTableFlags_NoBordersInBodyUntilResize
        | ImGuiTableFlags_RowBg
        | ImGuiTableFlags_Resizable
        | ImGuiTableFlags_Reorderable
        | ImGuiTableFlags_Hideable
        | ImGuiTableFlags_ScrollY
        | ImGuiTableFlags_ScrollX
        ;

	const int columns = 11;

    ImGui::PushID("Table");

    if (!ImGui::BeginTable("The Table", columns, flags, size)) {
        ImGui::PopID();
        return;
    }

	{
		// HEADERS
        ImGui::TableSetupColumn("Level");
        ImGui::TableSetupColumn("Timestamp");
        ImGui::TableSetupColumn("Service");
        ImGui::TableSetupColumn("Tag");
        ImGui::TableSetupColumn("Source File");
        ImGui::TableSetupColumn("Source Function");
        ImGui::TableSetupColumn("Source Line");
        ImGui::TableSetupColumn("Process ID");
        ImGui::TableSetupColumn("Thread ID");
        ImGui::TableSetupColumn("TS");
        ImGui::TableSetupColumn("Message");
		ImGui::TableHeadersRow();
	}

    ImGuiListClipper clipper;

    clipper.Begin((int)GetTotalLogCount());

    file->Lock();

    while (clipper.Step()) {
		for (i64 row = clipper.DisplayStart; row < clipper.DisplayEnd; row++) {
            // TODO we need some new UI for an entry
			LogEntry *entry = &file->entries[row];

			ImGui::TableNextColumn();
			ImGui::Text("%c", entry->level);

			ImGui::TableNextColumn();
            char timebuf[32] = { 0 };
            struct tm *tmlocal = gmtime((time_t *)&entry->date_timestamp);
            if (tmlocal) {
                strftime(timebuf, sizeof timebuf, "%Y-%m-%d %H:%M:%S", tmlocal);
                ImGui::Text(timebuf);
            } else {
                ImGui::Text("N/A");
            }

			ImGui::TableNextColumn();
            StringMap m0 = file->GetStringMap(entry->service);
            ImGui::Text("%.*s", m0.len, m0.str);

			ImGui::TableNextColumn();
            StringMap m1 = file->GetStringMap(entry->service);
            ImGui::Text("%.*s", m1.len, m1.str);

			ImGui::TableNextColumn();
            StringMap m2 = file->GetStringMap(entry->service);
            ImGui::Text("%.*s", m2.len, m2.str);

			ImGui::TableNextColumn();
            StringMap m3 = file->GetStringMap(entry->service);
            ImGui::Text("%.*s", m3.len, m3.str);

			ImGui::TableNextColumn();
            ImGui::Text("%ld", entry->source_line);

			ImGui::TableNextColumn();
            ImGui::Text("%ld", entry->process_id);

			ImGui::TableNextColumn();
            ImGui::Text("%ld", entry->thread_id);

			ImGui::TableNextColumn();
            char tsbuf[32] = { 0 };
            struct tm *tmts = gmtime((time_t *)&entry->timestamp);
            if (tmts) {
                strftime(tsbuf, sizeof tsbuf, "%Y-%m-%d %H:%M:%S", tmts);
                ImGui::Text(timebuf);
            } else {
                ImGui::Text("N/A");
            }

            ImGui::TableNextColumn();
            StringMap m4 = file->GetStringMap(entry->message);
            ImGui::Text("%.*s", m4.len, m4.str);
		}
	}

    file->Unlock();

    ImGui::EndTable();

    ImGui::PopID();
}

// Helper functions

bool CreateDeviceD3D(HWND hWnd)
{
    // Setup swap chain
    DXGI_SWAP_CHAIN_DESC sd;
    ZeroMemory(&sd, sizeof(sd));
    sd.BufferCount = 2;
    sd.BufferDesc.Width = 0;
    sd.BufferDesc.Height = 0;
    sd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    sd.BufferDesc.RefreshRate.Numerator = 60;
    sd.BufferDesc.RefreshRate.Denominator = 1;
    sd.Flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;
    sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    sd.OutputWindow = hWnd;
    sd.SampleDesc.Count = 1;
    sd.SampleDesc.Quality = 0;
    sd.Windowed = TRUE;
    sd.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;

    UINT createDeviceFlags = 0;
    //createDeviceFlags |= D3D10_CREATE_DEVICE_DEBUG;
    HRESULT res = D3D10CreateDeviceAndSwapChain(nullptr, D3D10_DRIVER_TYPE_HARDWARE, nullptr, createDeviceFlags, D3D10_SDK_VERSION, &sd, &g_pSwapChain, &g_pd3dDevice);
    if (res == DXGI_ERROR_UNSUPPORTED) // Try high-performance WARP software driver if hardware is not available.
        res = D3D10CreateDeviceAndSwapChain(nullptr, D3D10_DRIVER_TYPE_WARP, nullptr, createDeviceFlags, D3D10_SDK_VERSION, &sd, &g_pSwapChain, &g_pd3dDevice);
    if (res != S_OK)
        return false;

    CreateRenderTarget();
    return true;
}

void CleanupDeviceD3D()
{
    CleanupRenderTarget();
    if (g_pSwapChain) { g_pSwapChain->Release(); g_pSwapChain = nullptr; }
    if (g_pd3dDevice) { g_pd3dDevice->Release(); g_pd3dDevice = nullptr; }
}

void CreateRenderTarget()
{
    ID3D10Texture2D *pBackBuffer;
    g_pSwapChain->GetBuffer(0, IID_PPV_ARGS(&pBackBuffer));
    g_pd3dDevice->CreateRenderTargetView(pBackBuffer, nullptr, &g_mainRenderTargetView);
    pBackBuffer->Release();
}

void CleanupRenderTarget()
{
    if (g_mainRenderTargetView) { g_mainRenderTargetView->Release(); g_mainRenderTargetView = nullptr; }
}

// Forward declare message handler from imgui_impl_win32.cpp
extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

// Win32 message handler
// You can read the io.WantCaptureMouse, io.WantCaptureKeyboard flags to tell if dear imgui wants to use your inputs.
// - When io.WantCaptureMouse is true, do not dispatch mouse input data to your main application, or clear/overwrite your copy of the mouse data.
// - When io.WantCaptureKeyboard is true, do not dispatch keyboard input data to your main application, or clear/overwrite your copy of the keyboard data.
// Generally you may always pass all inputs to dear imgui, and hide them from your application based on those two flags.
LRESULT WINAPI WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    if (ImGui_ImplWin32_WndProcHandler(hWnd, msg, wParam, lParam))
        return true;

    switch (msg)
    {
    case WM_SIZE:
        if (wParam == SIZE_MINIMIZED)
            return 0;
        g_ResizeWidth = (UINT)LOWORD(lParam); // Queue resize
        g_ResizeHeight = (UINT)HIWORD(lParam);
        return 0;
    case WM_SYSCOMMAND:
        if ((wParam & 0xfff0) == SC_KEYMENU) // Disable ALT application menu
            return 0;
        break;
    case WM_DESTROY:
        ::PostQuitMessage(0);
        return 0;
    }
    return ::DefWindowProcW(hWnd, msg, wParam, lParam);
}
