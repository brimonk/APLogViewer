// Brian Chrzanowski
//
// APLogViewer - A better log viewing tool
// Formats supported:
//
// TODO

#include "common.h"
#include "imgui.h"
#include "imgui_impl_win32.h"
#include "imgui_impl_dx10.h"
#include <d3d10_1.h>
#include <d3d10.h>
#include <tchar.h>

// Data
static ID3D10Device *g_pd3dDevice = nullptr;
static IDXGISwapChain *g_pSwapChain = nullptr;
static UINT                     g_ResizeWidth = 0, g_ResizeHeight = 0;
static ID3D10RenderTargetView *g_mainRenderTargetView = nullptr;

// Forward declarations of helper functions
bool CreateDeviceD3D(HWND hWnd);
void CleanupDeviceD3D();
void CreateRenderTarget();
void CleanupRenderTarget();
LRESULT WINAPI WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

typedef struct LogEntry {
    char level;
    u64 date_timestamp;
    u64 service;
    u64 tag;
    u64 source_file;
    u64 source_function;
    i64 source_line;
    i64 process_id;
    i64 thread_id;
    u64 timestamp;
    u64 message_id;
} LogEntry;

std::unordered_map<u64, std::string> STRING_MAP;
std::vector<LogEntry> ENTRIES;
std::mutex GLOBAL_STATE_MUTEX;
bool g_ReadInput = true;

u64 hash(char *str)
{
    u64 hash = 5381;
    int c;
    while (c = *str++) {
        hash = ((hash << 5) + hash) + c;
    }
    return hash;
}

u64 map_upsert(std::string str)
{
    u64 h = hash((char *)str.c_str());
    GLOBAL_STATE_MUTEX.lock();
    auto search = STRING_MAP.find(h);
    if (search == STRING_MAP.end()) {
        STRING_MAP.insert({ h, str });
    }
    GLOBAL_STATE_MUTEX.unlock();
    return h;
}

std::string get_string(u64 h)
{
    GLOBAL_STATE_MUTEX.lock();
    auto search = STRING_MAP.find(h);
	std::string result = search != STRING_MAP.end() ? search->second : std::string();
    GLOBAL_STATE_MUTEX.unlock();
    return result;
}

u64 parse_timestamp(std::string timestamp)
{
    u64 ts = 0;
    int year, month, day, hour, minute, second;
    int rc = sscanf(timestamp.c_str(), "%d/%d/%d %d:%d:%d",
        &day, &month, &year, &hour, &minute, &second);
    if (rc == 6) {
        struct tm tt = { 0 };
        tt.tm_year = year - 1900;
        tt.tm_mon = month;
        tt.tm_mday = day;
        tt.tm_hour = hour;
        tt.tm_min = minute;
        tt.tm_sec = second;
        ts = mktime(&tt);
    }
    return ts;
}

std::string ReadStringWithKey(const char *str, std::string key)
{
    char tkey[32];
    snprintf(tkey, sizeof tkey, "%s=\"", key.c_str());
    const char *s = strstr(str, tkey) + strlen(tkey);
    const char *e = strchr(s, '"');
    return std::string(s, e - s);
}

int ReadIntegerWithKey(const char *str, std::string key)
{
    char tkey[32];
    snprintf(tkey, sizeof tkey, "%s=\"", key.c_str());
    const char *s = strstr(str, tkey) + strlen(tkey);
    return atoi(s);
}

void ReadAPLog(std::string path)
{
    std::ifstream file(path);
    std::string str;

    // i,11/28/2023 12:42:09,AzPubSubPerf,DefaultTag,SrcFile="" SrcFunc="" SrcLine="0" Pid="5640" Tid="2324" TS="0x01DA223B5BB194F2" String1="Setting azpubsub.kusto.log.level: 7"

    while (g_ReadInput && std::getline(file, str)) {
        // NOTE Make sure that each line ends in a '"'
        // there's probably a better way to determine if we have a partial line or not...
        if (str[str.length() - 1] != '"')
            continue;

        LogEntry log = { 0 };

        const char *s = str.c_str();

        std::string str(s);

        log.level = s[0];
        std::string date_timestamp = str.substr(2, sizeof("dd-mm-YYYY HH:MM:SS") - 1);
        log.date_timestamp = parse_timestamp(date_timestamp);

        const char *src1_ptr = s + 22;
        size_t src1_len = strchr(src1_ptr, ',') - src1_ptr;
        std::string service = str.substr(src1_ptr - s, src1_len);
        log.service = map_upsert(service);

        const char *src2_ptr = src1_ptr + src1_len + 1;
        size_t src2_len = strchr(src2_ptr, ',') - src2_ptr;
        std::string tag = str.substr(src2_ptr - s, src2_len);
        log.tag = map_upsert(tag);

        std::string source_file = ReadStringWithKey(s, "SrcFile");
        log.source_file = map_upsert(source_file);
        std::string source_function = ReadStringWithKey(s, "SrcFunc");
        log.source_function = map_upsert(source_function);
        log.source_line = ReadIntegerWithKey(s, "SrcLine");

        log.process_id = ReadIntegerWithKey(s, "Pid");
        log.thread_id = ReadIntegerWithKey(s, "Tid");

        const char *string1 = strstr(s, "String1");
        string1 += strlen("String1") + 1;

        // copy everything but the first and last quotes

        std::string message = str.substr(string1 + 1 - s, strlen(string1 + 1) - 1);
        log.message_id = map_upsert(message);

        GLOBAL_STATE_MUTEX.lock();
        ENTRIES.push_back(log);
        GLOBAL_STATE_MUTEX.unlock();
    }
}

DWORD WINAPI ReadAllFiles(LPVOID arg)
{
    std::string file = *(std::string *)arg;
    ReadAPLog(file);
    return 0;
}

void RenderTable();

// Main code
int main(int argc, char **argv)
{
    // Create application window
    //ImGui_ImplWin32_EnableDpiAwareness();
    WNDCLASSEXW wc = { sizeof(wc), CS_CLASSDC, WndProc, 0L, 0L, GetModuleHandle(nullptr), nullptr, nullptr, nullptr, nullptr, L"ImGui Example", nullptr };
    ::RegisterClassExW(&wc);
    HWND hwnd = ::CreateWindowW(wc.lpszClassName, L"Dear ImGui DirectX10 Example", WS_OVERLAPPEDWINDOW, 100, 100, 1280, 800, nullptr, nullptr, wc.hInstance, nullptr);

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
    HANDLE file_io_thread = nullptr;

    if (argc > 1) {
        file = std::string(argv[1]);
        file_io_thread = ::CreateThread(NULL, 0, ReadAllFiles, (LPVOID)&file, 0, nullptr);
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
            
            // TODO Window names should relate to what subsection of the logs you're looking at.
            if (ImGui::Begin("APLogViewer")) {
                ImGui::Text("LOG ENTRIES: %ld", ENTRIES.size());
                RenderTable();
                ImGui::Text("Application average %.3f ms/frame (%.1f FPS)", 1000.0f / io.Framerate, io.Framerate);
            }

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

            ImGui::End();
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
    WaitForSingleObject(file_io_thread, INFINITE);

    ImGui_ImplDX10_Shutdown();
    ImGui_ImplWin32_Shutdown();
    ImGui::DestroyContext();

    CleanupDeviceD3D();
    ::DestroyWindow(hwnd);
    ::UnregisterClassW(wc.lpszClassName, wc.hInstance);

    return 0;
}

void RenderTable()
{
    const float TEXT_BASE_WIDTH = ImGui::CalcTextSize("A").x;
    const float TEXT_BASE_HEIGHT = ImGui::GetTextLineHeightWithSpacing();

    ImGui::PushID("Table");

    ImGuiTableFlags flags =
        ImGuiTableFlags_NoBordersInBodyUntilResize
        | ImGuiTableFlags_RowBg
        | ImGuiTableFlags_Resizable
        | ImGuiTableFlags_Reorderable
        | ImGuiTableFlags_Hideable
        | ImGuiTableFlags_ScrollY
        | ImGuiTableFlags_ScrollX;

	const int columns = 10;

    ImVec2 outer_size = ImVec2(0.0f, TEXT_BASE_HEIGHT * 30);

    if (!ImGui::BeginTable("The Table", columns, flags, outer_size)) {
        ImGui::EndTable();
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
        ImGui::TableSetupColumn("Message");
		ImGui::TableHeadersRow();
	}

    ImGuiListClipper clipper;

    GLOBAL_STATE_MUTEX.lock();

    clipper.Begin((int)ENTRIES.size());

    while (clipper.Step()) {
		for (i64 row = clipper.DisplayStart; row < clipper.DisplayEnd; row++) {
			LogEntry *entry = &ENTRIES[row];

			ImGui::TableNextRow();
			int column = 0;

			ImGui::TableSetColumnIndex(column++);
            char levelbuf[2] = { entry->level, 0 };
			ImGui::Text(levelbuf);

            ImGui::TableSetColumnIndex(column++);
            char timebuf[32] = { 0 };
            struct tm *tmlocal = localtime((time_t *)&entry->date_timestamp);
            if (tmlocal) {
                strftime(timebuf, sizeof timebuf, "%Y-%m-%d %H:%M:%S", tmlocal);
                ImGui::Text(timebuf);
            } else {
                ImGui::Text("N/A");
            }

            ImGui::TableSetColumnIndex(column++);
            ImGui::Text(STRING_MAP[entry->service].c_str());

            ImGui::TableSetColumnIndex(column++);
            ImGui::Text(STRING_MAP[entry->tag].c_str());

            ImGui::TableSetColumnIndex(column++);
            ImGui::Text(STRING_MAP[entry->source_file].c_str());

            ImGui::TableSetColumnIndex(column++);
            ImGui::Text(STRING_MAP[entry->source_function].c_str());

            ImGui::TableSetColumnIndex(column++);
            ImGui::Text("%ld", entry->source_line);

            ImGui::TableSetColumnIndex(column++);
            ImGui::Text("%ld", entry->process_id);

            ImGui::TableSetColumnIndex(column++);
            ImGui::Text("%ld", entry->thread_id);

            ImGui::TableSetColumnIndex(column++);
            ImGui::Text(STRING_MAP[entry->message_id].c_str());
		}

	}

    GLOBAL_STATE_MUTEX.unlock();

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
