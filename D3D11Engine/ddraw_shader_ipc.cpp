#include "ddraw_shader_ipc.h"
#include <mutex>
#include <windows.h>

static std::mutex g_shaderMtx;
static std::wstring g_pendingShader;
static std::atomic_bool g_hasPending{false};

static std::mutex g_overrideMtx;
static std::string g_psOverride;

std::atomic<int> g_blendDirection{+1};

static std::string W2U(const std::wstring& w) {
    if (w.empty()) return {};
    int n = WideCharToMultiByte(CP_UTF8, 0, w.data(), (int)w.size(), nullptr, 0, nullptr, nullptr);
    std::string s(n, '\0');
    WideCharToMultiByte(CP_UTF8, 0, w.data(), (int)w.size(), s.data(), n, nullptr, nullptr);
    return s;
}

bool DX11Shader_HasPSOverride() {
    std::lock_guard<std::mutex> lk(g_overrideMtx);
    return !g_psOverride.empty();
}

const std::string& DX11Shader_GetPSOverride() {
    return g_psOverride;
}

void DX11Shader_ClearOverride() {
    std::lock_guard<std::mutex> lk(g_overrideMtx);
    g_psOverride.clear();
}

void ApplyShaderByName(const std::wstring& wname) {
    
    if (wname.empty()) return;

    size_t pos = wname.find(L':');
    std::wstring shaderPart = wname;
    std::wstring dirPart;
    if (pos != std::wstring::npos) {
        shaderPart = wname.substr(0, pos);
        dirPart    = wname.substr(pos + 1);
    }

    if (!dirPart.empty()) {

        if (_wcsicmp(dirPart.c_str(), L"UP") == 0)
            g_blendDirection.store(+1, std::memory_order_release);
            
        else if (_wcsicmp(dirPart.c_str(), L"DOWN") == 0)
            g_blendDirection.store(-1, std::memory_order_release);
    }

    if (shaderPart == L"__CLEAR__" || shaderPart == L"default") {
        DX11Shader_ClearOverride();
        return;
    }
    std::string name = W2U(shaderPart);
    { std::lock_guard<std::mutex> lk(g_overrideMtx); g_psOverride = std::move(name); }
}

void DX11_ApplyPendingShaderIfAny() {
    if (g_hasPending.exchange(false, std::memory_order_acq_rel)) {
        std::wstring name;
        { std::lock_guard<std::mutex> lk(g_shaderMtx); name = g_pendingShader; }
        ApplyShaderByName(name);
    }
}

static std::wstring PipeName() {
    return L"\\\\.\\pipe\\DX11Shader";
}

static DWORD WINAPI ShaderPipeThread(LPVOID) {
    for (;;) {
        HANDLE hPipe = CreateNamedPipeW(
            PipeName().c_str(),
            PIPE_ACCESS_DUPLEX,
            PIPE_TYPE_MESSAGE | PIPE_READMODE_MESSAGE | PIPE_WAIT,
            1, 1024, 1024, 0, nullptr);

        if (hPipe == INVALID_HANDLE_VALUE) {
            Sleep(500);
            continue;
        }

        BOOL ok = ConnectNamedPipe(hPipe, nullptr) ? TRUE
                 : (GetLastError() == ERROR_PIPE_CONNECTED);

        if (ok) {
            wchar_t buf[512];
            DWORD read = 0;
            if (ReadFile(hPipe, buf, sizeof(buf) - sizeof(wchar_t), &read, nullptr)) {
                buf[read / sizeof(wchar_t)] = L'\0';
                {
                    std::lock_guard<std::mutex> lk(g_shaderMtx);
                    g_pendingShader = buf;
                    g_hasPending.store(true, std::memory_order_release);
                }
                const wchar_t* reply = L"OK";
                DWORD written = 0;
                WriteFile(hPipe, reply, (DWORD)((wcslen(reply) + 1) * sizeof(wchar_t)), &written, nullptr);
            }
        }

        FlushFileBuffers(hPipe);
        DisconnectNamedPipe(hPipe);
        CloseHandle(hPipe);
    }
    return 0;
}

static HANDLE g_pipeThread = nullptr;

extern "C" void DX11_InitRemoteControl() {
    if (g_pipeThread) return;
    g_pipeThread = CreateThread(nullptr, 0, ShaderPipeThread, nullptr, 0, nullptr);
}
