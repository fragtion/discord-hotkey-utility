#include <windows.h>
#include <winhttp.h>
#include <hidsdi.h>
#include <setupapi.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#define APP_NAME    "Discord Hotkey Utility"
#define APP_VERSION "1.0.0"

// ---------------------------------------------------------------
// Config struct
// ---------------------------------------------------------------
typedef struct {
    // [general]
    int flash_ms;
    COLORREF flash_color_muted;
    COLORREF flash_color_unmuted;
    COLORREF flash_color_deafened;
    COLORREF flash_color_undeafened;
    int flash_display; // 0 = no flash, 1 = primary, 2 = second, etc.

    // [discord]
    char client_id[64];
    char client_secret[64];
    char redirect_uri[128];
    char token_file[128];
    int discord_reconnect_delay_ms;

    // [joystick]
    BOOL joystick_enabled;
    int vid;
    int pid;
    int mute_button;
    int deafen_button; // -1 = not set
    int long_press_ms;
    int joystick_reconnect_delay_ms;

    // [hotkey]
    BOOL hotkey_enabled;
    char hotkey_mute[64];
    char hotkey_deafen[64];
} Config;

Config cfg = {0};

// ---------------------------------------------------------------
// Config parsing helpers
// ---------------------------------------------------------------
void TrimWhitespace(char* s) {
    char* p = s;
    while (*p == ' ' || *p == '\t') p++;
    memmove(s, p, strlen(p) + 1);
    int len = (int)strlen(s);
    while (len > 0 && (s[len-1] == ' ' || s[len-1] == '\t' ||
                        s[len-1] == '\r' || s[len-1] == '\n'))
        s[--len] = '\0';
}

COLORREF ParseColor(const char* s) {
    const char* p = s;
    if (*p == '#') p++; // Skip optional #
    unsigned int r = 0, g = 0, b = 0;
    unsigned int hex = 0;
    sscanf(p, "%06X", &hex);
    r = (hex >> 16) & 0xFF;
    g = (hex >> 8) & 0xFF;
    b = hex & 0xFF;
    return RGB(r, g, b);
}

int ParseHex(const char* s) {
    int val = 0;
    sscanf(s, "%i", &val); // Handles 0x prefix automatically
    return val;
}

void GetConfigPath(char* out, int outSize) {
    char exePath[MAX_PATH];
    GetModuleFileNameA(NULL, exePath, MAX_PATH);
    char* last = strrchr(exePath, '\\');
    if (last) *(last + 1) = '\0';
    snprintf(out, outSize, "%sconfig.ini", exePath);
}

void GetTokenFilePath(char* out, int outSize) {
    char exePath[MAX_PATH];
    GetModuleFileNameA(NULL, exePath, MAX_PATH);
    char* last = strrchr(exePath, '\\');
    if (last) *(last + 1) = '\0';
    snprintf(out, outSize, "%s%s", exePath, cfg.token_file);
}
void WriteDefaultConfig(const char* path) {
    FILE* f = fopen(path, "w");
    if (!f) { printf("Could not create config.ini at %s\n", path); return; }
    fprintf(f,
        "; Discord Muter Hotkey Utility - Configuration File\n"
        "\n"
        "[general]\n"
        "; How long the screen flash lasts in milliseconds\n"
        "flash_ms=250\n"
        "; Flash colors (hex RGB, # prefix optional)\n"
        "flash_color_muted=FF0000\n"
        "flash_color_unmuted=00FF00\n"
        "flash_color_deafened=0000FF\n"
        "flash_color_undeafened=00C8FF\n"
        "; Which monitor to flash: 0=disabled, 1=primary, 2=second, 3=third, etc.\n"
        "flash_display=1\n"
        "\n"
        "[discord]\n"
        "; Get these from https://discord.com/developers/applications\n"
        "client_id=YOUR_CLIENT_ID\n"
        "client_secret=YOUR_CLIENT_SECRET\n"
        "; Must match exactly what is set in your Discord Developer Portal under OAuth2 > Redirects\n"
        "redirect_uri=http://localhost:7878\n"
        "; File where the OAuth2 token is cached after first login\n"
        "token_file=discord_token.txt\n"
        "; Milliseconds to wait between Discord reconnect attempts\n"
        "reconnect_delay_ms=3000\n"
        "\n"
        "[joystick]\n"
        "enabled=true\n"
        "; USB Vendor ID and Product ID of your HID device (hex). Check Device Manager.\n"
        "vid=\n"
        "pid=\n"
        "; HID report byte index for each button. Leave blank to disable.\n"
        "; Only mute_button set: short press = mute, long press = deafen\n"
        "; Only deafen_button set: short press = deafen, long press = mute\n"
        "; Both set: each button acts independently, no long press\n"
        "mute_button=\n"
        "deafen_button=\n"
        "; Milliseconds to hold button before long press fires\n"
        "long_press_ms=1000\n"
        "; Milliseconds to wait between joystick reconnect attempts\n"
        "reconnect_delay_ms=3000\n"
        "\n"
        "[hotkey]\n"
        "enabled=false\n"
        "; Format: MODIFIER+MODIFIER+KEY\n"
        "; Modifiers: CTRL, SHIFT, ALT, WIN\n"
        "; Keys: A-Z, F1-F24, SPACE, TAB, ENTER, ESC, INSERT, DELETE,\n"
        ";       HOME, END, PAGEUP, PAGEDOWN, UP, DOWN, LEFT, RIGHT, NUMPAD0-NUMPAD9\n"
        "mute=CTRL+SHIFT+F11\n"
        "deafen=CTRL+SHIFT+F12\n"
    );
    fclose(f);
    printf("Created default config.ini at %s\n", path);
    printf("Please edit config.ini and set your Discord client_id and client_secret before running.\n");
}

void LoadConfig() {
    // Defaults
    cfg.flash_ms = 250;
    cfg.flash_color_muted = RGB(255, 0, 0);
    cfg.flash_color_unmuted = RGB(0, 255, 0);
    cfg.flash_color_deafened = RGB(0, 0, 255);
    cfg.flash_color_undeafened = RGB(0, 200, 255);
    cfg.flash_display = 1;
    strncpy(cfg.client_id, "", sizeof(cfg.client_id));
    strncpy(cfg.client_secret, "", sizeof(cfg.client_secret));
    strncpy(cfg.redirect_uri, "http://localhost:7878", sizeof(cfg.redirect_uri));
    strncpy(cfg.token_file, "discord_token.txt", sizeof(cfg.token_file));
    cfg.discord_reconnect_delay_ms = 3000;
    cfg.joystick_enabled = TRUE;
    cfg.vid = 0;
    cfg.pid = 0;
    cfg.mute_button = -1;
    cfg.deafen_button = -1;
    cfg.long_press_ms = 1000;
    cfg.joystick_reconnect_delay_ms = 3000;
    cfg.hotkey_enabled = FALSE;
    strncpy(cfg.hotkey_mute, "", sizeof(cfg.hotkey_mute));
    strncpy(cfg.hotkey_deafen, "", sizeof(cfg.hotkey_deafen));

    char configPath[MAX_PATH];
    GetConfigPath(configPath, MAX_PATH);

    FILE* f = fopen(configPath, "r");
    if (!f) {
        printf("config.ini not found at %s - saving default config.\n", configPath);
        WriteDefaultConfig(configPath);
        return;
    }

    char line[256];
    char section[64] = {0};

    while (fgets(line, sizeof(line), f)) {
        TrimWhitespace(line);
        if (line[0] == ';' || line[0] == '#' || line[0] == '\0') continue;

        if (line[0] == '[') {
            char* end = strchr(line, ']');
            if (end) { *end = '\0'; strncpy(section, line + 1, sizeof(section) - 1); }
            continue;
        }

        char* eq = strchr(line, '=');
        if (!eq) continue;
        *eq = '\0';
        char key[128], val[128];
        strncpy(key, line, sizeof(key) - 1);
        strncpy(val, eq + 1, sizeof(val) - 1);
        TrimWhitespace(key);
        TrimWhitespace(val);

        if (strcmp(section, "general") == 0) {
            if (strcmp(key, "flash_ms") == 0)               cfg.flash_ms = atoi(val);
            else if (strcmp(key, "flash_color_muted") == 0)      cfg.flash_color_muted = ParseColor(val);
            else if (strcmp(key, "flash_color_unmuted") == 0)    cfg.flash_color_unmuted = ParseColor(val);
            else if (strcmp(key, "flash_color_deafened") == 0)   cfg.flash_color_deafened = ParseColor(val);
            else if (strcmp(key, "flash_color_undeafened") == 0) cfg.flash_color_undeafened = ParseColor(val);
            else if (strcmp(key, "flash_display") == 0)          cfg.flash_display = atoi(val);
        } else if (strcmp(section, "discord") == 0) {
            if (strcmp(key, "client_id") == 0)                   strncpy(cfg.client_id, val, sizeof(cfg.client_id) - 1);
            else if (strcmp(key, "client_secret") == 0)          strncpy(cfg.client_secret, val, sizeof(cfg.client_secret) - 1);
            else if (strcmp(key, "redirect_uri") == 0)           strncpy(cfg.redirect_uri, val, sizeof(cfg.redirect_uri) - 1);
            else if (strcmp(key, "token_file") == 0)             strncpy(cfg.token_file, val, sizeof(cfg.token_file) - 1);
            else if (strcmp(key, "reconnect_delay_ms") == 0)     cfg.discord_reconnect_delay_ms = atoi(val);
        } else if (strcmp(section, "joystick") == 0) {
            if (strcmp(key, "enabled") == 0)                     cfg.joystick_enabled = (strcmp(val, "true") == 0);
            else if (strcmp(key, "vid") == 0)                    cfg.vid = ParseHex(val);
            else if (strcmp(key, "pid") == 0)                    cfg.pid = ParseHex(val);
            else if (strcmp(key, "mute_button") == 0)            cfg.mute_button = strlen(val) > 0 ? atoi(val) : -1;
            else if (strcmp(key, "deafen_button") == 0)          cfg.deafen_button = strlen(val) > 0 ? atoi(val) : -1;
            else if (strcmp(key, "long_press_ms") == 0)          cfg.long_press_ms = atoi(val);
            else if (strcmp(key, "reconnect_delay_ms") == 0)     cfg.joystick_reconnect_delay_ms = atoi(val);
        } else if (strcmp(section, "hotkey") == 0) {
            if (strcmp(key, "enabled") == 0)                     cfg.hotkey_enabled = (strcmp(val, "true") == 0);
            else if (strcmp(key, "mute") == 0)                   strncpy(cfg.hotkey_mute, val, sizeof(cfg.hotkey_mute) - 1);
            else if (strcmp(key, "deafen") == 0)                 strncpy(cfg.hotkey_deafen, val, sizeof(cfg.hotkey_deafen) - 1);
        }
    }
    fclose(f);
    if (cfg.deafen_button >= 0 && cfg.deafen_button == cfg.mute_button) {
        printf("Warning: deafen_button same as mute_button, ignoring deafen_button.\n");
        cfg.deafen_button = -1;
    }
    printf("Config loaded from %s\n", configPath);
}

// ---------------------------------------------------------------
// Monitor enumeration for flash_display
// ---------------------------------------------------------------
typedef struct {
    int target;
    int current;
    RECT rect;
    BOOL found;
} MonitorEnumData;

BOOL CALLBACK MonitorEnumProc(HMONITOR hMon, HDC hdc, LPRECT lpRect, LPARAM lParam) {
    MonitorEnumData* d = (MonitorEnumData*)lParam;
    d->current++;
    if (d->current == d->target) {
        d->rect = *lpRect;
        d->found = TRUE;
        return FALSE;
    }
    return TRUE;
}

BOOL GetMonitorRect(int displayIndex, RECT* outRect) {
    MonitorEnumData d = {0};
    d.target = displayIndex; // 1-based
    EnumDisplayMonitors(NULL, NULL, MonitorEnumProc, (LPARAM)&d);
    if (d.found) { *outRect = d.rect; return TRUE; }
    // Fallback to full desktop
    GetWindowRect(GetDesktopWindow(), outRect);
    return FALSE;
}

// ---------------------------------------------------------------
// Screen flash
// ---------------------------------------------------------------
COLORREF GetContrastColor(COLORREF bg) {
    // Extract RGB components
    int r = GetRValue(bg);
    int g = GetGValue(bg);
    int b = GetBValue(bg);
    // Perceived luminance formula
    double luminance = (0.299 * r + 0.587 * g + 0.114 * b) / 255.0;
    return luminance > 0.5 ? RGB(0, 0, 0) : RGB(255, 255, 255);
}

void FlashScreen(COLORREF color, const char* text) {
    if (cfg.flash_display == 0) return;

    RECT rect;
    GetMonitorRect(cfg.flash_display, &rect);

    WNDCLASSEXA wc = {0};
    wc.cbSize = sizeof(wc);
    wc.lpfnWndProc = DefWindowProcA;
    wc.hInstance = GetModuleHandle(NULL);
    wc.hbrBackground = CreateSolidBrush(color);
    wc.lpszClassName = "DiscordMuteFlash";
    RegisterClassExA(&wc);

    HWND hFlash = CreateWindowExA(
        WS_EX_TOPMOST | WS_EX_TOOLWINDOW | WS_EX_NOACTIVATE,
        "DiscordMuteFlash", NULL,
        WS_POPUP | WS_VISIBLE,
        rect.left, rect.top,
        rect.right - rect.left,
        rect.bottom - rect.top,
        NULL, NULL, GetModuleHandle(NULL), NULL);

    if (!hFlash) { UnregisterClassA("DiscordMuteFlash", GetModuleHandle(NULL)); return; }

    // Draw background + text
    HDC hdc = GetDC(hFlash);
    RECT clientRect;
    GetClientRect(hFlash, &clientRect);

    // Fill background
    HBRUSH hBrush = CreateSolidBrush(color);
    FillRect(hdc, &clientRect, hBrush);
    DeleteObject(hBrush);

    // Draw text
    int fontSize = (clientRect.bottom - clientRect.top) / 10; // ~10% of screen height
    HFONT hFont = CreateFontA(
        fontSize, 0, 0, 0,
        FW_BOLD, FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS,
        CLIP_DEFAULT_PRECIS, ANTIALIASED_QUALITY,
        DEFAULT_PITCH | FF_SWISS, "Arial");

    HFONT hOldFont = (HFONT)SelectObject(hdc, hFont);
    SetBkMode(hdc, TRANSPARENT);
    SetTextColor(hdc, GetContrastColor(color));

    // Convert char* to wide for DrawTextW — or just use DrawTextA
    DrawTextA(hdc, text, -1, &clientRect,
        DT_CENTER | DT_VCENTER | DT_SINGLELINE);

    SelectObject(hdc, hOldFont);
    DeleteObject(hFont);
    ReleaseDC(hFlash, hdc);

    // Process messages so window paints
    MSG msg;
    DWORD start = GetTickCount();
    while (GetTickCount() - start < (DWORD)cfg.flash_ms) {
        while (PeekMessage(&msg, hFlash, 0, 0, PM_REMOVE)) {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
        Sleep(1);
    }

    DestroyWindow(hFlash);
    UnregisterClassA("DiscordMuteFlash", GetModuleHandle(NULL));
}

// ---------------------------------------------------------------
// Discord RPC framing
// ---------------------------------------------------------------
HANDLE hDiscordPipe = INVALID_HANDLE_VALUE;
BOOL isMuted = FALSE;
BOOL isDeafened = FALSE;
char savedToken[512] = {0};

void SendDiscordRPC(HANDLE hPipe, int opcode, const char* json) {
    DWORD jsonLen = (DWORD)strlen(json);
    BYTE header[8];
    header[0] = opcode & 0xFF; header[1] = (opcode >> 8) & 0xFF;
    header[2] = (opcode >> 16) & 0xFF; header[3] = (opcode >> 24) & 0xFF;
    header[4] = jsonLen & 0xFF; header[5] = (jsonLen >> 8) & 0xFF;
    header[6] = (jsonLen >> 16) & 0xFF; header[7] = (jsonLen >> 24) & 0xFF;
    DWORD bytesWritten;
    WriteFile(hPipe, header, 8, &bytesWritten, NULL);
    WriteFile(hPipe, json, jsonLen, &bytesWritten, NULL);
}

char* ReadDiscordRPC(HANDLE hPipe) {
    BYTE header[8];
    DWORD bytesRead;
    if (!ReadFile(hPipe, header, 8, &bytesRead, NULL) || bytesRead < 8) return NULL;
    DWORD payloadLen = header[4] | (header[5] << 8) | (header[6] << 16) | (header[7] << 24);
    if (payloadLen == 0 || payloadLen > 65536) return NULL;
    char* buf = (char*)malloc(payloadLen + 1);
    if (!buf) return NULL;
    if (!ReadFile(hPipe, buf, payloadLen, &bytesRead, NULL)) { free(buf); return NULL; }
    buf[bytesRead] = '\0';
    return buf;
}

BOOL ExtractJsonString(const char* json, const char* key, char* out, int outSize) {
    char search[64];
    snprintf(search, sizeof(search), "\"%s\":", key);
    const char* p = strstr(json, search);
    if (!p) return FALSE;
    p += strlen(search);
    while (*p == ' ' || *p == '\t') p++;
    if (*p != '"') return FALSE;
    p++;
    int i = 0;
    while (*p && *p != '"' && i < outSize - 1) out[i++] = *p++;
    out[i] = '\0';
    return i > 0;
}

// ---------------------------------------------------------------
// Token persistence
// ---------------------------------------------------------------
void SaveToken(const char* token) {
    char path[MAX_PATH];
    GetTokenFilePath(path, MAX_PATH);
    FILE* f = fopen(path, "w");
    if (f) { fprintf(f, "%s", token); fclose(f); printf("Token saved.\n"); }
}

BOOL LoadToken() {
    char path[MAX_PATH];
    GetTokenFilePath(path, MAX_PATH);
    FILE* f = fopen(path, "r");
    if (!f) return FALSE;
    int n = (int)fread(savedToken, 1, sizeof(savedToken) - 1, f);
    savedToken[n] = '\0';
    fclose(f);
    char* nl = strchr(savedToken, '\n');
    if (nl) *nl = '\0';
    return n > 0;
}

// ---------------------------------------------------------------
// HTTP token exchange
// ---------------------------------------------------------------
char* ExchangeCodeForToken(const char* code) {
    char postBody[1024];
    snprintf(postBody, sizeof(postBody),
        "grant_type=authorization_code&code=%s&redirect_uri=%s&client_id=%s&client_secret=%s",
        code, cfg.redirect_uri, cfg.client_id, cfg.client_secret);
    DWORD postLen = (DWORD)strlen(postBody);

    HINTERNET hSession = WinHttpOpen(L"DiscordMute/1.0",
        WINHTTP_ACCESS_TYPE_DEFAULT_PROXY, WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
    if (!hSession) return NULL;
    HINTERNET hConnect = WinHttpConnect(hSession, L"discord.com", INTERNET_DEFAULT_HTTPS_PORT, 0);
    if (!hConnect) { WinHttpCloseHandle(hSession); return NULL; }
    HINTERNET hRequest = WinHttpOpenRequest(hConnect, L"POST", L"/api/oauth2/token",
        NULL, WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES, WINHTTP_FLAG_SECURE);
    if (!hRequest) { WinHttpCloseHandle(hConnect); WinHttpCloseHandle(hSession); return NULL; }

    WinHttpSendRequest(hRequest, L"Content-Type: application/x-www-form-urlencoded",
        -1L, (LPVOID)postBody, postLen, postLen, 0);
    WinHttpReceiveResponse(hRequest, NULL);

    char* response = (char*)malloc(8192);
    if (!response) return NULL;
    DWORD totalRead = 0, bytesRead = 0, available = 0;
    while (WinHttpQueryDataAvailable(hRequest, &available) && available > 0) {
        if (totalRead + available >= 8191) break;
        WinHttpReadData(hRequest, response + totalRead, available, &bytesRead);
        totalRead += bytesRead;
    }
    response[totalRead] = '\0';
    WinHttpCloseHandle(hRequest);
    WinHttpCloseHandle(hConnect);
    WinHttpCloseHandle(hSession);
    printf("Token exchange response: %s\n", response);
    return response;
}

// ---------------------------------------------------------------
// Discord connection + auth
// ---------------------------------------------------------------
HANDLE ConnectToDiscord() {
    char pipePath[64];
    for (int i = 0; i <= 9; i++) {
        snprintf(pipePath, sizeof(pipePath), "\\\\.\\pipe\\discord-ipc-%d", i);
        HANDLE hPipe = CreateFileA(pipePath, GENERIC_READ | GENERIC_WRITE,
            0, NULL, OPEN_EXISTING, 0, NULL);
        if (hPipe != INVALID_HANDLE_VALUE) {
            printf("Connected to Discord on %s\n", pipePath);
            return hPipe;
        }
    }
    return INVALID_HANDLE_VALUE;
}

BOOL AuthenticateWithToken(HANDLE hPipe, const char* token) {
    char authJson[600];
    snprintf(authJson, sizeof(authJson),
        "{\"cmd\":\"AUTHENTICATE\",\"args\":{\"access_token\":\"%s\"},\"nonce\":\"auth-1\"}", token);
    SendDiscordRPC(hPipe, 1, authJson);
    char* resp = ReadDiscordRPC(hPipe);
    if (!resp) return FALSE;
    printf("Auth response: %s\n", resp);
    BOOL ok = !strstr(resp, "\"ERROR\"");
    free(resp);
    return ok;
}

HANDLE DiscordHandshakeAndAuth() {
    HANDLE hPipe = ConnectToDiscord();
    if (hPipe == INVALID_HANDLE_VALUE) return INVALID_HANDLE_VALUE;

    printf("Waiting for handshake...\n");
    char handshake[128];
    snprintf(handshake, sizeof(handshake), "{\"v\":1,\"client_id\":\"%s\"}", cfg.client_id);
    SendDiscordRPC(hPipe, 0, handshake);
    char* resp = ReadDiscordRPC(hPipe);
    if (!resp) { CloseHandle(hPipe); return INVALID_HANDLE_VALUE; }
    printf("Handshake: READY\n");
    free(resp);

    if (LoadToken()) {
        printf("Trying saved token...\n");
        if (AuthenticateWithToken(hPipe, savedToken)) {
            printf("Authenticated with saved token.\n");
            return hPipe;
        }
        printf("Saved token expired, re-authorizing...\n");
        char path[MAX_PATH];
        GetTokenFilePath(path, MAX_PATH);
        DeleteFileA(path);
    }

    char authReq[512];
    snprintf(authReq, sizeof(authReq),
        "{\"cmd\":\"AUTHORIZE\",\"args\":{"
        "\"client_id\":\"%s\","
        "\"scopes\":[\"rpc\",\"rpc.voice.write\"]},"
        "\"nonce\":\"authorize-1\"}", cfg.client_id);
    SendDiscordRPC(hPipe, 1, authReq);
    printf("Check Discord for an authorization popup...\n");

    resp = ReadDiscordRPC(hPipe);
    if (!resp) { CloseHandle(hPipe); return INVALID_HANDLE_VALUE; }
    printf("Authorize response: %s\n", resp);

    char code[512] = {0};
    if (!ExtractJsonString(resp, "code", code, sizeof(code))) {
        printf("ERROR: Could not extract auth code.\n");
        free(resp); CloseHandle(hPipe); return INVALID_HANDLE_VALUE;
    }
    free(resp);

    char* tokenResponse = ExchangeCodeForToken(code);
    if (!tokenResponse) { CloseHandle(hPipe); return INVALID_HANDLE_VALUE; }

    char accessToken[512] = {0};
    if (!ExtractJsonString(tokenResponse, "access_token", accessToken, sizeof(accessToken))) {
        printf("ERROR: Could not extract access_token: %s\n", tokenResponse);
        free(tokenResponse); CloseHandle(hPipe); return INVALID_HANDLE_VALUE;
    }
    free(tokenResponse);

    if (!AuthenticateWithToken(hPipe, accessToken)) {
        printf("Authentication failed.\n");
        CloseHandle(hPipe); return INVALID_HANDLE_VALUE;
    }

    SaveToken(accessToken);
    return hPipe;
}

BOOL EnsureDiscordConnected() {
    if (hDiscordPipe != INVALID_HANDLE_VALUE) {
        SendDiscordRPC(hDiscordPipe, 1,
            "{\"cmd\":\"GET_VOICE_SETTINGS\",\"args\":{},\"nonce\":\"ping\"}");
        char* resp = ReadDiscordRPC(hDiscordPipe);
        if (resp) { free(resp); return TRUE; }
        printf("Discord pipe lost, reconnecting...\n");
        CloseHandle(hDiscordPipe);
        hDiscordPipe = INVALID_HANDLE_VALUE;
    }
    hDiscordPipe = DiscordHandshakeAndAuth();
    if (hDiscordPipe == INVALID_HANDLE_VALUE) { printf("Could not reconnect to Discord.\n"); return FALSE; }
    printf("Reconnected to Discord.\n");
    return TRUE;
}

void FetchVoiceState() {
    SendDiscordRPC(hDiscordPipe, 1,
        "{\"cmd\":\"GET_VOICE_SETTINGS\",\"args\":{},\"nonce\":\"get-state\"}");
    char* resp = ReadDiscordRPC(hDiscordPipe);
    if (!resp) return;
    char* p = strstr(resp, "\"mute\":");
    if (p) { p += 7; while (*p == ' ') p++; isMuted = (strncmp(p, "true", 4) == 0); }
    p = strstr(resp, "\"deaf\":");
    if (p) { p += 7; while (*p == ' ') p++; isDeafened = (strncmp(p, "true", 4) == 0); }
    printf("State: mute=%s deaf=%s\n", isMuted ? "true" : "false", isDeafened ? "true" : "false");
    free(resp);
}

// ---------------------------------------------------------------
// Mute / deafen actions
// ---------------------------------------------------------------
void DoToggleMute() {
    if (!EnsureDiscordConnected()) return;
    FetchVoiceState();

    BOOL newMute, newDeaf;
    if (isDeafened) {
        newMute = FALSE; newDeaf = FALSE;
        printf("Action: undeafen+unmute\n");
    } else {
        newMute = !isMuted; newDeaf = FALSE;
        printf("Action: mute=%s\n", newMute ? "true" : "false");
    }

    char json[256];
    snprintf(json, sizeof(json),
        "{\"cmd\":\"SET_VOICE_SETTINGS\",\"args\":{\"mute\":%s,\"deaf\":%s},\"nonce\":\"mute-toggle\"}",
        newMute ? "true" : "false", newDeaf ? "true" : "false");
    SendDiscordRPC(hDiscordPipe, 1, json);
    char* resp = ReadDiscordRPC(hDiscordPipe);
    if (resp) { printf("Response: %s\n", resp); free(resp); }

    isMuted = newMute; isDeafened = newDeaf;
    if (isDeafened)       FlashScreen(cfg.flash_color_deafened,   "Muted + Deafened");
    else if (isMuted)     FlashScreen(cfg.flash_color_muted,      "Discord Muted");
    else                  FlashScreen(cfg.flash_color_unmuted,    "Discord Unmuted");
}

void DoToggleDeafen() {
    if (!EnsureDiscordConnected()) return;
    FetchVoiceState();

    BOOL newDeaf = !isDeafened;
    BOOL newMute = newDeaf ? TRUE : isMuted;
    printf("Action: deaf=%s mute=%s\n", newDeaf ? "true" : "false", newMute ? "true" : "false");

    char json[256];
    snprintf(json, sizeof(json),
        "{\"cmd\":\"SET_VOICE_SETTINGS\",\"args\":{\"mute\":%s,\"deaf\":%s},\"nonce\":\"deaf-toggle\"}",
        newMute ? "true" : "false", newDeaf ? "true" : "false");
    SendDiscordRPC(hDiscordPipe, 1, json);
    char* resp = ReadDiscordRPC(hDiscordPipe);
    if (resp) { printf("Response: %s\n", resp); free(resp); }

    isMuted = newMute; isDeafened = newDeaf;
    if (isDeafened)   FlashScreen(cfg.flash_color_deafened,   "Discord Deafened");
    else              FlashScreen(cfg.flash_color_undeafened, "Discord Undeafened");
}

// ---------------------------------------------------------------
// Hotkey parsing + registration
// ---------------------------------------------------------------
#define HOTKEY_ID_MUTE   1
#define HOTKEY_ID_DEAFEN 2

BOOL ParseHotkey(const char* str, UINT* modifiers, UINT* vk) {
    *modifiers = 0; *vk = 0;
    if (strlen(str) == 0) return FALSE;

    char buf[64];
    strncpy(buf, str, sizeof(buf) - 1);
    buf[sizeof(buf)-1] = '\0';

    char* token = strtok(buf, "+");
    while (token) {
        TrimWhitespace(token);
        // Convert to uppercase for comparison
        char upper[32];
        strncpy(upper, token, sizeof(upper) - 1);
        upper[sizeof(upper)-1] = '\0';
        for (int i = 0; upper[i]; i++) upper[i] = toupper((unsigned char)upper[i]);

        if      (strcmp(upper, "CTRL") == 0 || strcmp(upper, "CONTROL") == 0) *modifiers |= MOD_CONTROL;
        else if (strcmp(upper, "SHIFT") == 0)  *modifiers |= MOD_SHIFT;
        else if (strcmp(upper, "ALT") == 0)    *modifiers |= MOD_ALT;
        else if (strcmp(upper, "WIN") == 0)    *modifiers |= MOD_WIN;
        else {
            // It's the key itself
            if (strlen(upper) == 1) {
                *vk = VkKeyScanA(upper[0]) & 0xFF;
            } else if (upper[0] == 'F' && strlen(upper) <= 3) {
                int fnum = atoi(upper + 1);
                if (fnum >= 1 && fnum <= 24) *vk = VK_F1 + fnum - 1;
            } else if (strcmp(upper, "SPACE") == 0)     *vk = VK_SPACE;
            else if (strcmp(upper, "TAB") == 0)         *vk = VK_TAB;
            else if (strcmp(upper, "ENTER") == 0)       *vk = VK_RETURN;
            else if (strcmp(upper, "ESC") == 0)         *vk = VK_ESCAPE;
            else if (strcmp(upper, "INSERT") == 0)      *vk = VK_INSERT;
            else if (strcmp(upper, "DELETE") == 0)      *vk = VK_DELETE;
            else if (strcmp(upper, "HOME") == 0)        *vk = VK_HOME;
            else if (strcmp(upper, "END") == 0)         *vk = VK_END;
            else if (strcmp(upper, "PAGEUP") == 0)      *vk = VK_PRIOR;
            else if (strcmp(upper, "PAGEDOWN") == 0)    *vk = VK_NEXT;
            else if (strcmp(upper, "UP") == 0)          *vk = VK_UP;
            else if (strcmp(upper, "DOWN") == 0)        *vk = VK_DOWN;
            else if (strcmp(upper, "LEFT") == 0)        *vk = VK_LEFT;
            else if (strcmp(upper, "RIGHT") == 0)       *vk = VK_RIGHT;
            else if (strcmp(upper, "NUMPAD0") == 0)     *vk = VK_NUMPAD0;
            else if (strcmp(upper, "NUMPAD1") == 0)     *vk = VK_NUMPAD1;
            else if (strcmp(upper, "NUMPAD2") == 0)     *vk = VK_NUMPAD2;
            else if (strcmp(upper, "NUMPAD3") == 0)     *vk = VK_NUMPAD3;
            else if (strcmp(upper, "NUMPAD4") == 0)     *vk = VK_NUMPAD4;
            else if (strcmp(upper, "NUMPAD5") == 0)     *vk = VK_NUMPAD5;
            else if (strcmp(upper, "NUMPAD6") == 0)     *vk = VK_NUMPAD6;
            else if (strcmp(upper, "NUMPAD7") == 0)     *vk = VK_NUMPAD7;
            else if (strcmp(upper, "NUMPAD8") == 0)     *vk = VK_NUMPAD8;
            else if (strcmp(upper, "NUMPAD9") == 0)     *vk = VK_NUMPAD9;
            else { printf("Unknown key token: %s\n", upper); return FALSE; }
        }
        token = strtok(NULL, "+");
    }
    return *vk != 0;
}

DWORD WINAPI HotkeyThread(LPVOID lpParam) {
    UINT modMute = 0, vkMute = 0;
    UINT modDeafen = 0, vkDeafen = 0;

    BOOL hasMute   = ParseHotkey(cfg.hotkey_mute,   &modMute,   &vkMute);
    BOOL hasDeafen = ParseHotkey(cfg.hotkey_deafen, &modDeafen, &vkDeafen);

    if (hasMute && !RegisterHotKey(NULL, HOTKEY_ID_MUTE, modMute | MOD_NOREPEAT, vkMute))
        printf("Failed to register mute hotkey: %lu\n", GetLastError());
    else if (hasMute)
        printf("Mute hotkey registered: %s\n", cfg.hotkey_mute);

    if (hasDeafen && !RegisterHotKey(NULL, HOTKEY_ID_DEAFEN, modDeafen | MOD_NOREPEAT, vkDeafen))
        printf("Failed to register deafen hotkey: %lu\n", GetLastError());
    else if (hasDeafen)
        printf("Deafen hotkey registered: %s\n", cfg.hotkey_deafen);

    MSG msg;
    while (GetMessage(&msg, NULL, 0, 0)) {
        if (msg.message == WM_HOTKEY) {
            if (msg.wParam == HOTKEY_ID_MUTE)   DoToggleMute();
            if (msg.wParam == HOTKEY_ID_DEAFEN) DoToggleDeafen();
        }
    }

    if (hasMute)   UnregisterHotKey(NULL, HOTKEY_ID_MUTE);
    if (hasDeafen) UnregisterHotKey(NULL, HOTKEY_ID_DEAFEN);
    return 0;
}

// ---------------------------------------------------------------
// HID / Joystick
// ---------------------------------------------------------------
HANDLE hDevice = INVALID_HANDLE_VALUE;
BYTE lastButtonState = 0;
DWORD buttonPressTime = 0;
BOOL longPressHandled = FALSE;

void HandleHidData(BYTE* data, DWORD size) {
    BOOL hasMuteBtn   = (cfg.mute_button   >= 0 && (int)size > cfg.mute_button);
    BOOL hasDeafenBtn = (cfg.deafen_button >= 0 && (int)size > cfg.deafen_button);

    BYTE muteState   = hasMuteBtn   ? data[cfg.mute_button]   : 0;
    BYTE deafenState = hasDeafenBtn ? data[cfg.deafen_button] : 0;

    // --- Mode: both buttons set → dedicated roles, no long press ---
    if (hasMuteBtn && hasDeafenBtn) {
        static BYTE lastDeafenState = 0;
        if (deafenState == 1 && lastDeafenState != 1) DoToggleDeafen();
        lastDeafenState = deafenState;
        if (muteState == 1 && lastButtonState != 1) DoToggleMute();
        lastButtonState = muteState;
        return;
    }

    // --- Mode: only mute_button set → short=mute, long=deafen ---
    if (hasMuteBtn && !hasDeafenBtn) {
        if (muteState == 1 && lastButtonState != 1) {
            buttonPressTime = GetTickCount();
            longPressHandled = FALSE;
        } else if (muteState == 1 && lastButtonState == 1) {
            if (!longPressHandled && (GetTickCount() - buttonPressTime >= (DWORD)cfg.long_press_ms)) {
                printf("Long press (%lums) -> deafen\n", GetTickCount() - buttonPressTime);
                DoToggleDeafen();
                longPressHandled = TRUE;
            }
        } else if (muteState != 1 && lastButtonState == 1) {
            if (!longPressHandled) {
                printf("Short press -> mute\n");
                DoToggleMute();
            }
        }
        lastButtonState = muteState;
        return;
    }

    // --- Mode: only deafen_button set → short=deafen, long=mute ---
    if (!hasMuteBtn && hasDeafenBtn) {
        static BYTE lastDeafenOnlyState = 0;

        if (deafenState == 1 && lastDeafenOnlyState != 1) {
            // Button just pressed
            buttonPressTime = GetTickCount();
            longPressHandled = FALSE;
        } else if (deafenState == 1 && lastDeafenOnlyState == 1) {
            // Button held
            if (!longPressHandled && (GetTickCount() - buttonPressTime >= (DWORD)cfg.long_press_ms)) {
                printf("Long press (%lums) -> mute\n", GetTickCount() - buttonPressTime);
                DoToggleMute();
                longPressHandled = TRUE;
            }
        } else if (deafenState != 1 && lastDeafenOnlyState == 1) {
            // Button released
            if (!longPressHandled) {
                printf("Short press -> deafen\n");
                DoToggleDeafen();
            }
        }

        lastDeafenOnlyState = deafenState;
        return;
    }
}

BOOL FindAndOpenDevice() {
    GUID hidGuid;
    HidD_GetHidGuid(&hidGuid);
    HDEVINFO hDevInfo = SetupDiGetClassDevs(&hidGuid, NULL, NULL, DIGCF_PRESENT | DIGCF_DEVICEINTERFACE);
    if (hDevInfo == INVALID_HANDLE_VALUE) return FALSE;
    SP_DEVICE_INTERFACE_DATA deviceInterfaceData = {0};
    deviceInterfaceData.cbSize = sizeof(SP_DEVICE_INTERFACE_DATA);
    BOOL deviceFound = FALSE;
    for (DWORD i = 0; SetupDiEnumDeviceInterfaces(hDevInfo, NULL, &hidGuid, i, &deviceInterfaceData); i++) {
        DWORD requiredSize = 0;
        SetupDiGetDeviceInterfaceDetail(hDevInfo, &deviceInterfaceData, NULL, 0, &requiredSize, NULL);
        PSP_DEVICE_INTERFACE_DETAIL_DATA_A deviceDetail = (PSP_DEVICE_INTERFACE_DETAIL_DATA_A)malloc(requiredSize);
        if (!deviceDetail) continue;
        deviceDetail->cbSize = sizeof(SP_DEVICE_INTERFACE_DETAIL_DATA_A);
        SP_DEVINFO_DATA devInfoData = {0};
        devInfoData.cbSize = sizeof(SP_DEVINFO_DATA);
        if (!SetupDiGetDeviceInterfaceDetailA(hDevInfo, &deviceInterfaceData, deviceDetail, requiredSize, &requiredSize, &devInfoData)) {
            free(deviceDetail); continue;
        }
        hDevice = CreateFile(deviceDetail->DevicePath, GENERIC_READ | GENERIC_WRITE,
            FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, 0, NULL);
        if (hDevice == INVALID_HANDLE_VALUE) { free(deviceDetail); continue; }
        HIDD_ATTRIBUTES deviceAttributes;
        deviceAttributes.Size = sizeof(HIDD_ATTRIBUTES);
        if (HidD_GetAttributes(hDevice, &deviceAttributes)) {
            if (deviceAttributes.VendorID == cfg.vid && deviceAttributes.ProductID == cfg.pid) {
                printf("Device matched: VID=0x%04X, PID=0x%04X\n", deviceAttributes.VendorID, deviceAttributes.ProductID);
                deviceFound = TRUE;
                free(deviceDetail);
                break;
            }
        }
        CloseHandle(hDevice);
        free(deviceDetail);
    }
    SetupDiDestroyDeviceInfoList(hDevInfo);
    return deviceFound;
}

void WaitForDevice() {
    while (!FindAndOpenDevice()) {
        printf("Joystick not found, retrying in %dms...\n", cfg.joystick_reconnect_delay_ms);
        Sleep(cfg.joystick_reconnect_delay_ms);
    }
    printf("Joystick connected.\n");
}

void MonitorLoop() {
    BYTE buffer[64] = {0};
    DWORD bytesRead;
    while (1) {
        if (hDevice == INVALID_HANDLE_VALUE) WaitForDevice();
        if (ReadFile(hDevice, buffer, sizeof(buffer), &bytesRead, NULL)) {
            if (bytesRead > 0) HandleHidData(buffer, bytesRead);
        } else {
            printf("Joystick read error %lu, retrying in %dms...\n",
                GetLastError(), cfg.joystick_reconnect_delay_ms);
            CloseHandle(hDevice);
            hDevice = INVALID_HANDLE_VALUE;
            Sleep(cfg.joystick_reconnect_delay_ms);
        }
    }
}

// ---------------------------------------------------------------
// Entry point
// ---------------------------------------------------------------
int main(int argc, char* argv[]) {
    printf("%s v%s\n", APP_NAME, APP_VERSION);
    printf("----------------------------------------\n");
    LoadConfig();
    BOOL silent = FALSE;
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-s") == 0) {
            silent = TRUE;
        } else if (strcmp(argv[i], "-install") == 0) {
            char path[MAX_PATH];
            GetModuleFileNameA(NULL, path, MAX_PATH);
            char cmd[MAX_PATH + 4];
            snprintf(cmd, sizeof(cmd), "\"%s\" -s", path);
            HKEY hKey;
            if (RegOpenKeyExA(HKEY_CURRENT_USER,
                "Software\\Microsoft\\Windows\\CurrentVersion\\Run",
                0, KEY_SET_VALUE, &hKey) == ERROR_SUCCESS) {
                RegSetValueExA(hKey, "DiscordMute", 0, REG_SZ,
                    (BYTE*)cmd, (DWORD)strlen(cmd) + 1);
                RegCloseKey(hKey);
                printf("Installed to startup.\n");
            } else {
                printf("Failed to install to startup.\n");
            }
            return 0;
        } else if (strcmp(argv[i], "-uninstall") == 0) {
            HKEY hKey;
            if (RegOpenKeyExA(HKEY_CURRENT_USER,
                "Software\\Microsoft\\Windows\\CurrentVersion\\Run",
                0, KEY_SET_VALUE, &hKey) == ERROR_SUCCESS) {
                RegDeleteValueA(hKey, "DiscordMute");
                RegCloseKey(hKey);
                printf("Removed from startup.\n");
            }
            return 0;
        }
    }

    if (silent) {
        HWND hWnd = GetConsoleWindow();
        if (hWnd) ShowWindow(hWnd, SW_HIDE);
    }

    // Connect to Discord
    printf("Connecting to Discord");
    int dots = 0;
    while (hDiscordPipe == INVALID_HANDLE_VALUE) {
        hDiscordPipe = DiscordHandshakeAndAuth();
        if (hDiscordPipe == INVALID_HANDLE_VALUE) {
        printf(".");
        fflush(stdout);
        if (++dots % 5 == 0) {
            printf("\nStill trying to connect to Discord");
            fflush(stdout);
        }
        Sleep(cfg.discord_reconnect_delay_ms);
        }
    }
    printf("Connected!\n");

    // Start hotkey thread if enabled
    if (cfg.hotkey_enabled && (strlen(cfg.hotkey_mute) > 0 || strlen(cfg.hotkey_deafen) > 0)) {
        HANDLE hThread = CreateThread(NULL, 0, HotkeyThread, NULL, 0, NULL);
        if (!hThread) printf("Failed to start hotkey thread.\n");
        else printf("Hotkey thread started.\n");
    }

    // Start joystick monitor if enabled
    if (cfg.joystick_enabled && cfg.vid != 0 && cfg.pid != 0) {
        if (!silent) printf("Ready! Monitoring joystick...\n");
        MonitorLoop(); // Blocks forever
    } else {
        if (!silent) printf("Joystick disabled or not configured. Running hotkey-only mode.\n");
        // Keep main thread alive for hotkey thread
        while (1) Sleep(1000);
    }

    return 0;
}
