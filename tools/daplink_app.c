#define UNICODE
#define _UNICODE
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <commctrl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#pragma comment(lib, "comctl32.lib")
#pragma comment(lib, "gdi32.lib")
#pragma comment(lib, "user32.lib")

// Window Dimensions
#define WIN_WIDTH   430
#define WIN_HEIGHT  535

// Colors (Tailwind Slate Theme)
#define COLOR_BG        RGB(15, 23, 42)    // #0F172A
#define COLOR_CARD      RGB(30, 41, 59)    // #1E293B
#define COLOR_CARD_BRD  RGB(51, 65, 85)    // #334155
#define COLOR_TEXT_PRI  RGB(248, 250, 252) // #F8FAFC
#define COLOR_TEXT_SEC  RGB(148, 163, 184) // #94A3B8
#define COLOR_CYAN      RGB(56, 189, 248)  // #38BDF8
#define COLOR_GREEN     RGB(16, 185, 129)  // #10B981
#define COLOR_YELLOW    RGB(251, 191, 36)  // #FBBF24
#define COLOR_RED       RGB(239, 68, 68)   // #EF4444
#define COLOR_BTN_BG    RGB(2, 132, 199)   // #0284C7
#define COLOR_INPUT_BG  RGB(15, 23, 42)    // #0F172A

// Control IDs
#define IDC_LBL_ROLE_VAL    101
#define IDC_LBL_LINK_VAL    102
#define IDC_LBL_RSSI_VAL    103
#define IDC_LBL_RETRIES     104
#define IDC_LBL_PKTS        105
#define IDC_LBL_PKTSIZE     106
#define IDC_LBL_CHANNEL_VAL 107
#define IDC_LBL_PROTO_VAL   108

#define IDC_EDT_PAIRCODE    201
#define IDC_CMB_CHANNEL     202
#define IDC_CMB_TXPOWER     203
#define IDC_CMB_ROLE        204
#define IDC_BTN_SAVE        205
#define IDC_LBL_MSG         206

static HBRUSH g_hBrushBg = NULL;
static HBRUSH g_hBrushCard = NULL;
static HBRUSH g_hBrushInput = NULL;
static HFONT  g_hFontRegular = NULL;
static HFONT  g_hFontBold = NULL;

static WCHAR g_baseDir[MAX_PATH] = {0};
static int   g_rssi = 0;
static BOOL  g_connected = FALSE;

// UI Controls
static HWND g_hLblLink = NULL;
static HWND g_hLblRole = NULL;
static HWND g_hLblChannel = NULL;
static HWND g_hLblRssi = NULL;
static HWND g_hLblRetries = NULL;
static HWND g_hLblPkts = NULL;
static HWND g_hLblProto = NULL;
static HWND g_hLblPktSize = NULL;
static HWND g_hEdtPairCode = NULL;
static HWND g_hCmbChannel = NULL;
static HWND g_hCmbTxPower = NULL;
static HWND g_hCmbRole = NULL;
static HWND g_hBtnSave = NULL;
static HWND g_hLblMsg = NULL;
static HWND g_hWndMain = NULL;

static void InitPaths() {
    GetModuleFileNameW(NULL, g_baseDir, MAX_PATH);
    WCHAR* p = wcsrchr(g_baseDir, L'\\');
    if (p) *p = L'\0';

    // Check if STATUS.TXT exists in current directory
    WCHAR testPath[MAX_PATH];
    wsprintfW(testPath, L"%s\\STATUS.TXT", g_baseDir);
    if (GetFileAttributesW(testPath) != INVALID_FILE_ATTRIBUTES) return;

    // Auto-scan drive letters D..Z to find DAPLink disk if launched elsewhere
    DWORD drives = GetLogicalDrives();
    for (WCHAR d = L'D'; d <= L'Z'; d++) {
        if (drives & (1 << (d - L'A'))) {
            wsprintfW(testPath, L"%c:\\STATUS.TXT", d);
            if (GetFileAttributesW(testPath) != INVALID_FILE_ATTRIBUTES) {
                char buf[128] = {0};
                FILE* f = _wfopen(testPath, L"rb");
                if (f) {
                    size_t nr = fread(buf, 1, sizeof(buf) - 1, f);
                    buf[nr] = '\0';
                    fclose(f);
                    if (strstr(buf, "DAPLink") || strstr(buf, "ROLE:")) {
                        wsprintfW(g_baseDir, L"%c:", d);
                        return;
                    }
                }
            }
        }
    }
}

static BOOL ReadUnbuffered(const WCHAR* filename, char* outBuf, DWORD maxLen) {
    WCHAR fullPath[MAX_PATH];
    wsprintfW(fullPath, L"%s\\%s", g_baseDir, filename);

    HANDLE h = CreateFileW(fullPath, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE,
                           NULL, OPEN_EXISTING, FILE_FLAG_NO_BUFFERING, NULL);
    if (h == INVALID_HANDLE_VALUE) {
        h = CreateFileW(fullPath, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE,
                        NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
        if (h == INVALID_HANDLE_VALUE) return FALSE;
    }

    static __attribute__((aligned(512))) char sectorBuf[512];
    memset(sectorBuf, 0, sizeof(sectorBuf));
    DWORD bytesRead = 0;
    BOOL ok = ReadFile(h, sectorBuf, 512, &bytesRead, NULL);
    CloseHandle(h);

    if (ok && bytesRead > 0) {
        if (bytesRead >= maxLen) bytesRead = maxLen - 1;
        memcpy(outBuf, sectorBuf, bytesRead);
        outBuf[bytesRead] = '\0';
        return TRUE;
    }
    return FALSE;
}

static void LoadConfig() {
    char buf[512] = {0};
    if (!ReadUnbuffered(L"CONFIG.TXT", buf, sizeof(buf))) return;

    char* p = buf;
    if ((unsigned char)p[0] == 0xEF && (unsigned char)p[1] == 0xBB && (unsigned char)p[2] == 0xBF) p += 3;

    char line[128];
    while (*p) {
        size_t l = 0;
        while (*p && *p != '\r' && *p != '\n' && l < sizeof(line) - 1) line[l++] = *p++;
        line[l] = '\0';
        while (*p == '\r' || *p == '\n') p++;

        if (line[0] == '#' || line[0] == '\0') continue;
        char* eq = strchr(line, '=');
        if (!eq) continue;
        *eq = '\0';
        char* key = line;
        char* val = eq + 1;

        if (_stricmp(key, "PAIR_CODE") == 0) {
            WCHAR wval[32];
            MultiByteToWideChar(CP_UTF8, 0, val, -1, wval, 32);
            SetWindowTextW(g_hEdtPairCode, wval);
        } else if (_stricmp(key, "CHANNEL") == 0) {
            int ch = atoi(val);
            if (ch == 80) SendMessage(g_hCmbChannel, CB_SETCURSEL, 0, 0);
            else if (ch == 76) SendMessage(g_hCmbChannel, CB_SETCURSEL, 1, 0);
            else if (ch == 40) SendMessage(g_hCmbChannel, CB_SETCURSEL, 2, 0);
            else if (ch == 2)  SendMessage(g_hCmbChannel, CB_SETCURSEL, 3, 0);
        } else if (_stricmp(key, "TX_POWER") == 0) {
            int tp = atoi(val);
            if (tp == 8) SendMessage(g_hCmbTxPower, CB_SETCURSEL, 0, 0);
            else if (tp == 4) SendMessage(g_hCmbTxPower, CB_SETCURSEL, 1, 0);
            else if (tp == 0) SendMessage(g_hCmbTxPower, CB_SETCURSEL, 2, 0);
            else if (tp == -4) SendMessage(g_hCmbTxPower, CB_SETCURSEL, 3, 0);
        } else if (_stricmp(key, "ROLE") == 0) {
            if (_strnicmp(val, "ROLE_A", 6) == 0 || _stricmp(val, "A") == 0 || _stricmp(val, "HOST") == 0 || _stricmp(val, "1") == 0) SendMessage(g_hCmbRole, CB_SETCURSEL, 1, 0);
            else if (_strnicmp(val, "ROLE_B", 6) == 0 || _stricmp(val, "B") == 0 || _stricmp(val, "TARGET") == 0 || _stricmp(val, "2") == 0) SendMessage(g_hCmbRole, CB_SETCURSEL, 2, 0);
            else SendMessage(g_hCmbRole, CB_SETCURSEL, 0, 0);
        }
    }
}

static void PollStatus() {
    char buf[512] = {0};
    if (!ReadUnbuffered(L"STATUS.TXT", buf, sizeof(buf))) return;

    char* p = buf;
    if ((unsigned char)p[0] == 0xEF && (unsigned char)p[1] == 0xBB && (unsigned char)p[2] == 0xBF) p += 3;

    char line[128];
    char txPkts[32] = {0}, rxPkts[32] = {0};

    while (*p) {
        size_t l = 0;
        while (*p && *p != '\r' && *p != '\n' && l < sizeof(line) - 1) line[l++] = *p++;
        line[l] = '\0';
        while (*p == '\r' || *p == '\n') p++;

        if (strstr(line, "ROLE:") || strstr(line, "角色")) {
            char* col = strchr(line, ':');
            if (col) {
                while (*(++col) == ' ') {}
                WCHAR w[64];
                MultiByteToWideChar(CP_UTF8, 0, col, -1, w, 64);
                SetWindowTextW(g_hLblRole, w);
            }
        }
        if (strstr(line, "LINK:") || strstr(line, "链路")) {
            BOOL ok = (strstr(line, "已连接") != NULL || strstr(line, "Connected") != NULL);
            g_connected = ok;
            SetWindowTextW(g_hLblLink, ok ? L"● 已连接" : L"○ 等待连接");
        }
        if (strstr(line, "CHANNEL:") || strstr(line, "信道")) {
            char* col = strchr(line, ':');
            if (col) {
                while (*(++col) == ' ') {}
                WCHAR w[64];
                MultiByteToWideChar(CP_UTF8, 0, col, -1, w, 64);
                SetWindowTextW(g_hLblChannel, w);
            }
        }
        if (strstr(line, "RSSI:") || strstr(line, "信号")) {
            char* col = strchr(line, ':');
            if (col) {
                g_rssi = atoi(col + 1);
                WCHAR w[32];
                wsprintfW(w, L"%d dBm", g_rssi);
                SetWindowTextW(g_hLblRssi, w);
            }
        }
        if (strstr(line, "TX_STAT:")) {
            char* col = strchr(line, ':');
            if (col) {
                while (*(++col) == ' ') {}
                char* paren = strchr(col, '(');
                if (paren) {
                    char* endp = strchr(paren, ')');
                    if (endp) *endp = '\0';
                    WCHAR w[64];
                    MultiByteToWideChar(CP_UTF8, 0, paren + 1, -1, w, 64);
                    SetWindowTextW(g_hLblRetries, w);
                }
                sscanf(col, "%31s", txPkts);
            }
        }
        if (strstr(line, "RX_STAT:")) {
            char* col = strchr(line, ':');
            if (col) {
                while (*(++col) == ' ') {}
                sscanf(col, "%31s", rxPkts);
            }
        }
        if (strstr(line, "PKT_SIZE:")) {
            char* col = strchr(line, ':');
            if (col) {
                while (*(++col) == ' ') {}
                WCHAR w[32];
                MultiByteToWideChar(CP_UTF8, 0, col, -1, w, 32);
                SetWindowTextW(g_hLblPktSize, w);
            }
        }
    }

    if (txPkts[0] || rxPkts[0]) {
        WCHAR w[64];
        wsprintfW(w, L"%S / %S", txPkts[0] ? txPkts : "0", rxPkts[0] ? rxPkts : "0");
        SetWindowTextW(g_hLblPkts, w);
    }

    if (g_hWndMain) {
        RECT rc = { 20, 95, 400, 135 };
        InvalidateRect(g_hWndMain, &rc, TRUE);
    }
}

static void SaveConfig() {
    WCHAR wPairCode[32];
    GetWindowTextW(g_hEdtPairCode, wPairCode, 32);
    if (wPairCode[0] == L'\0') wcscpy(wPairCode, L"123456");

    int chIdx = (int)SendMessage(g_hCmbChannel, CB_GETCURSEL, 0, 0);
    int chVal = (chIdx == 1) ? 76 : ((chIdx == 2) ? 40 : ((chIdx == 3) ? 2 : 80));

    int tpIdx = (int)SendMessage(g_hCmbTxPower, CB_GETCURSEL, 0, 0);
    int tpVal = (tpIdx == 1) ? 4 : ((tpIdx == 2) ? 0 : ((tpIdx == 3) ? -4 : 8));

    int roIdx = (int)SendMessage(g_hCmbRole, CB_GETCURSEL, 0, 0);
    const char* roVal = (roIdx == 1) ? "ROLE_A" : ((roIdx == 2) ? "ROLE_B" : "AUTO");

    char out[512];
    int len = snprintf(out, sizeof(out),
        "# ==========================================\r\n"
        "# nRF52840 Wireless DAPLink Configuration\r\n"
        "# ==========================================\r\n"
        "PAIR_CODE=%ls\r\n"
        "CHANNEL=%d\r\n"
        "TX_POWER=%d\r\n"
        "ROLE=%s\r\n",
        wPairCode, chVal, tpVal, roVal
    );

    WCHAR fullPath[MAX_PATH];
    wsprintfW(fullPath, L"%s\\CONFIG.TXT", g_baseDir);

    HANDLE h = CreateFileW(fullPath, GENERIC_WRITE, FILE_SHARE_READ, NULL,
                           CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    if (h != INVALID_HANDLE_VALUE) {
        DWORD written = 0;
        WriteFile(h, out, (DWORD)len, &written, NULL);
        FlushFileBuffers(h);
        CloseHandle(h);

        SetWindowTextW(g_hLblMsg, L"✅ 配置已成功保存至 Flash，正在自动重启...");
        ShowWindow(g_hLblMsg, SW_SHOW);
    } else {
        MessageBoxW(g_hWndMain, L"保存配置文件失败，请检查磁盘写保护！", L"错误", MB_ICONERROR);
    }
}

static BOOL CALLBACK SetChildFontProc(HWND child, LPARAM lp) {
    SendMessage(child, WM_SETFONT, (WPARAM)lp, TRUE);
    return TRUE;
}

static LRESULT CALLBACK WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
        case WM_CREATE: {
            g_hWndMain = hWnd;
            CreateWindowW(L"STATIC", L"📡 实时无线遥测 (AFH 智能跳频)", WS_CHILD | WS_VISIBLE,
                          26, 20, 220, 20, hWnd, NULL, NULL, NULL);

            g_hLblLink = CreateWindowW(L"STATIC", L"○ 检测中...", WS_CHILD | WS_VISIBLE | SS_RIGHT,
                                      280, 20, 110, 20, hWnd, (HMENU)IDC_LBL_LINK_VAL, NULL, NULL);

            // Card 1 Labels
            CreateWindowW(L"STATIC", L"当前角色", WS_CHILD | WS_VISIBLE, 26, 48, 100, 18, hWnd, NULL, NULL, NULL);
            g_hLblRole = CreateWindowW(L"STATIC", L"--", WS_CHILD | WS_VISIBLE | SS_RIGHT, 180, 48, 210, 18, hWnd, (HMENU)IDC_LBL_ROLE_VAL, NULL, NULL);

            CreateWindowW(L"STATIC", L"工作信道 (AFH)", WS_CHILD | WS_VISIBLE, 26, 72, 110, 18, hWnd, NULL, NULL, NULL);
            g_hLblChannel = CreateWindowW(L"STATIC", L"--", WS_CHILD | WS_VISIBLE | SS_RIGHT, 150, 72, 240, 18, hWnd, (HMENU)IDC_LBL_CHANNEL_VAL, NULL, NULL);

            CreateWindowW(L"STATIC", L"信号强度", WS_CHILD | WS_VISIBLE, 26, 96, 80, 18, hWnd, NULL, NULL, NULL);
            g_hLblRssi = CreateWindowW(L"STATIC", L"-- dBm", WS_CHILD | WS_VISIBLE | SS_RIGHT, 320, 96, 70, 18, hWnd, (HMENU)IDC_LBL_RSSI_VAL, NULL, NULL);

            CreateWindowW(L"STATIC", L"重发尝试 / 重发率", WS_CHILD | WS_VISIBLE, 26, 124, 130, 18, hWnd, NULL, NULL, NULL);
            g_hLblRetries = CreateWindowW(L"STATIC", L"--", WS_CHILD | WS_VISIBLE | SS_RIGHT, 180, 124, 210, 18, hWnd, (HMENU)IDC_LBL_RETRIES, NULL, NULL);

            CreateWindowW(L"STATIC", L"累计发包 / 接收", WS_CHILD | WS_VISIBLE, 26, 148, 130, 18, hWnd, NULL, NULL, NULL);
            g_hLblPkts = CreateWindowW(L"STATIC", L"--", WS_CHILD | WS_VISIBLE | SS_RIGHT, 180, 148, 210, 18, hWnd, (HMENU)IDC_LBL_PKTS, NULL, NULL);

            CreateWindowW(L"STATIC", L"DAP 协议标准", WS_CHILD | WS_VISIBLE, 26, 172, 110, 18, hWnd, NULL, NULL, NULL);
            g_hLblProto = CreateWindowW(L"STATIC", L"CMSIS-DAP v2 (WinUSB 极速)", WS_CHILD | WS_VISIBLE | SS_RIGHT, 140, 172, 250, 18, hWnd, (HMENU)IDC_LBL_PROTO_VAL, NULL, NULL);

            CreateWindowW(L"STATIC", L"单包传输上限", WS_CHILD | WS_VISIBLE, 26, 196, 130, 18, hWnd, NULL, NULL, NULL);
            g_hLblPktSize = CreateWindowW(L"STATIC", L"240 字节", WS_CHILD | WS_VISIBLE | SS_RIGHT, 180, 196, 210, 18, hWnd, (HMENU)IDC_LBL_PKTSIZE, NULL, NULL);

            // Card 2
            CreateWindowW(L"STATIC", L"⚙️ 参数配置 (修改后存至 Flash)", WS_CHILD | WS_VISIBLE,
                          26, 238, 250, 20, hWnd, NULL, NULL, NULL);

            CreateWindowW(L"STATIC", L"专属配对码 (PAIR_CODE)", WS_CHILD | WS_VISIBLE, 26, 270, 180, 20, hWnd, NULL, NULL, NULL);
            g_hEdtPairCode = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", L"123456",
                                             WS_CHILD | WS_VISIBLE | ES_RIGHT | ES_NUMBER,
                                             220, 266, 170, 24, hWnd, (HMENU)IDC_EDT_PAIRCODE, NULL, NULL);

            CreateWindowW(L"STATIC", L"基础信道 (CHANNEL)", WS_CHILD | WS_VISIBLE, 26, 304, 180, 20, hWnd, NULL, NULL, NULL);
            g_hCmbChannel = CreateWindowW(L"COMBOBOX", NULL, WS_CHILD | WS_VISIBLE | CBS_DROPDOWNLIST,
                                          220, 300, 170, 120, hWnd, (HMENU)IDC_CMB_CHANNEL, NULL, NULL);
            SendMessageW(g_hCmbChannel, CB_ADDSTRING, 0, (LPARAM)L"信道 80 (2480MHz · 推荐)");
            SendMessageW(g_hCmbChannel, CB_ADDSTRING, 0, (LPARAM)L"信道 76 (2476MHz · 净空)");
            SendMessageW(g_hCmbChannel, CB_ADDSTRING, 0, (LPARAM)L"信道 40 (2440MHz · 常用)");
            SendMessageW(g_hCmbChannel, CB_ADDSTRING, 0, (LPARAM)L"信道 2 (2402MHz)");
            SendMessageW(g_hCmbChannel, CB_SETCURSEL, 0, 0);

            CreateWindowW(L"STATIC", L"发射功率 (TX_POWER)", WS_CHILD | WS_VISIBLE, 26, 338, 180, 20, hWnd, NULL, NULL, NULL);
            g_hCmbTxPower = CreateWindowW(L"COMBOBOX", NULL, WS_CHILD | WS_VISIBLE | CBS_DROPDOWNLIST,
                                          220, 334, 170, 120, hWnd, (HMENU)IDC_CMB_TXPOWER, NULL, NULL);
            SendMessageW(g_hCmbTxPower, CB_ADDSTRING, 0, (LPARAM)L"+8 dBm (最大发射 · 推荐)");
            SendMessageW(g_hCmbTxPower, CB_ADDSTRING, 0, (LPARAM)L"+4 dBm");
            SendMessageW(g_hCmbTxPower, CB_ADDSTRING, 0, (LPARAM)L"0 dBm");
            SendMessageW(g_hCmbTxPower, CB_ADDSTRING, 0, (LPARAM)L"-4 dBm (低功耗)");
            SendMessageW(g_hCmbTxPower, CB_SETCURSEL, 0, 0);

            CreateWindowW(L"STATIC", L"运行角色 (ROLE)", WS_CHILD | WS_VISIBLE, 26, 372, 180, 20, hWnd, NULL, NULL, NULL);
            g_hCmbRole = CreateWindowW(L"COMBOBOX", NULL, WS_CHILD | WS_VISIBLE | CBS_DROPDOWNLIST,
                                       220, 368, 170, 120, hWnd, (HMENU)IDC_CMB_ROLE, NULL, NULL);
            SendMessageW(g_hCmbRole, CB_ADDSTRING, 0, (LPARAM)L"AUTO (自动协商 · 推荐)");
            SendMessageW(g_hCmbRole, CB_ADDSTRING, 0, (LPARAM)L"角色A (主动发起端 / A车)");
            SendMessageW(g_hCmbRole, CB_ADDSTRING, 0, (LPARAM)L"角色B (被动响应端 / B车)");
            SendMessageW(g_hCmbRole, CB_SETCURSEL, 0, 0);

            g_hBtnSave = CreateWindowW(L"BUTTON", L"💾 保存配置并软重启生效",
                                       WS_CHILD | WS_VISIBLE | BS_OWNERDRAW,
                                       26, 412, 364, 38, hWnd, (HMENU)IDC_BTN_SAVE, NULL, NULL);

            g_hLblMsg = CreateWindowW(L"STATIC", L"", WS_CHILD | SS_CENTER,
                                      26, 458, 364, 22, hWnd, (HMENU)IDC_LBL_MSG, NULL, NULL);

            // Set Fonts
            SendMessage(hWnd, WM_SETFONT, (WPARAM)g_hFontRegular, TRUE);
            EnumChildWindows(hWnd, SetChildFontProc, (LPARAM)g_hFontRegular);

            SetTimer(hWnd, 1, 300, NULL);
            LoadConfig();
            PollStatus();
            break;
        }

        case WM_TIMER:
            if (wParam == 1) {
                PollStatus();
            }
            break;

        case WM_COMMAND:
            if (LOWORD(wParam) == IDC_BTN_SAVE) {
                SaveConfig();
            }
            break;

        case WM_DRAWITEM: {
            LPDRAWITEMSTRUCT pDis = (LPDRAWITEMSTRUCT)lParam;
            if (pDis->CtlID == IDC_BTN_SAVE) {
                HDC hdc = pDis->hDC;
                RECT rc = pDis->rcItem;
                BOOL isPressed = (pDis->itemState & ODS_SELECTED);
                COLORREF btnCol = isPressed ? RGB(3, 105, 161) : RGB(2, 132, 199);
                HBRUSH hBr = CreateSolidBrush(btnCol);
                HPEN hPen = CreatePen(PS_SOLID, 1, RGB(56, 189, 248));
                HGDIOBJ oldBr = SelectObject(hdc, hBr);
                HGDIOBJ oldPen = SelectObject(hdc, hPen);
                RoundRect(hdc, rc.left, rc.top, rc.right, rc.bottom, 10, 10);
                SelectObject(hdc, oldBr);
                SelectObject(hdc, oldPen);
                DeleteObject(hBr);
                DeleteObject(hPen);

                SetBkMode(hdc, TRANSPARENT);
                SetTextColor(hdc, RGB(255, 255, 255));
                HGDIOBJ oldFont = SelectObject(hdc, g_hFontBold);
                WCHAR text[64];
                GetWindowTextW(pDis->hwndItem, text, 64);
                DrawTextW(hdc, text, -1, &rc, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
                SelectObject(hdc, oldFont);
                return TRUE;
            }
            break;
        }

        case WM_CTLCOLOREDIT: {
            HDC hdc = (HDC)wParam;
            SetBkColor(hdc, COLOR_INPUT_BG);
            SetTextColor(hdc, COLOR_TEXT_PRI);
            return (LRESULT)g_hBrushInput;
        }

        case WM_CTLCOLORSTATIC: {
            HDC hdc = (HDC)wParam;
            HWND hCtl = (HWND)lParam;
            SetBkMode(hdc, TRANSPARENT);

            if (hCtl == g_hLblLink) {
                SetTextColor(hdc, g_connected ? COLOR_GREEN : COLOR_YELLOW);
            } else if (hCtl == g_hLblRole || hCtl == g_hLblRssi || hCtl == g_hLblRetries ||
                       hCtl == g_hLblPkts || hCtl == g_hLblPktSize || hCtl == g_hLblChannel) {
                SetTextColor(hdc, COLOR_TEXT_PRI);
            } else if (hCtl == g_hLblProto) {
                SetTextColor(hdc, COLOR_CYAN);
            } else if (hCtl == g_hLblMsg) {
                SetTextColor(hdc, COLOR_GREEN);
            } else {
                SetTextColor(hdc, COLOR_TEXT_SEC);
            }
            return (LRESULT)g_hBrushCard;
        }

        case WM_CTLCOLORDLG:
            return (LRESULT)g_hBrushBg;

        case WM_PAINT: {
            PAINTSTRUCT ps;
            HDC hdc = BeginPaint(hWnd, &ps);

            // Draw Background
            RECT rcClient;
            GetClientRect(hWnd, &rcClient);
            FillRect(hdc, &rcClient, g_hBrushBg);

            // Card 1: 14, 10, 402, 222
            RECT rc1 = { 14, 10, 402, 222 };
            FillRect(hdc, &rc1, g_hBrushCard);
            FrameRect(hdc, &rc1, CreateSolidBrush(COLOR_CARD_BRD));

            // Card 2: 14, 228, 402, 490
            RECT rc2 = { 14, 228, 402, 490 };
            FillRect(hdc, &rc2, g_hBrushCard);
            FrameRect(hdc, &rc2, CreateSolidBrush(COLOR_CARD_BRD));

            // Draw RSSI Bar inside Card 1: from x=110 to x=310, y=100, h=10
            RECT rcBarBg = { 110, 100, 310, 110 };
            HBRUSH hBrBarBg = CreateSolidBrush(RGB(51, 65, 85));
            FillRect(hdc, &rcBarBg, hBrBarBg);
            DeleteObject(hBrBarBg);

            int pct = 0;
            if (g_rssi != 0) {
                pct = (int)((g_rssi + 100) * 1.8f);
                if (pct < 0) pct = 0;
                if (pct > 100) pct = 100;
            }
            int fillW = (200 * pct) / 100;
            if (fillW > 0) {
                RECT rcFill = { 110, 100, 110 + fillW, 110 };
                COLORREF barColor = (g_rssi >= -65) ? COLOR_GREEN : ((g_rssi >= -80) ? COLOR_YELLOW : COLOR_RED);
                HBRUSH hBrFill = CreateSolidBrush(barColor);
                FillRect(hdc, &rcFill, hBrFill);
                DeleteObject(hBrFill);
            }

            EndPaint(hWnd, &ps);
            break;
        }

        case WM_DESTROY:
            KillTimer(hWnd, 1);
            PostQuitMessage(0);
            break;

        default:
            return DefWindowProcW(hWnd, msg, wParam, lParam);
    }
    return 0;
}

int WINAPI WinMain(HINSTANCE hInst, HINSTANCE hPrev, LPSTR lpCmd, int nShow) {
    SetProcessDPIAware();
    InitCommonControls();
    InitPaths();

    g_hBrushBg = CreateSolidBrush(COLOR_BG);
    g_hBrushCard = CreateSolidBrush(COLOR_CARD);
    g_hBrushInput = CreateSolidBrush(COLOR_INPUT_BG);

    g_hFontRegular = CreateFontW(-13, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
                                 DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                                 CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI");
    g_hFontBold = CreateFontW(-13, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE,
                              DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                              CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI");

    WNDCLASSEXW wc = {0};
    wc.cbSize = sizeof(wc);
    wc.style = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInst;
    wc.hIcon = (HICON)LoadImageW(hInst, MAKEINTRESOURCEW(1), IMAGE_ICON, 32, 32, LR_DEFAULTCOLOR);
    wc.hIconSm = (HICON)LoadImageW(hInst, MAKEINTRESOURCEW(1), IMAGE_ICON, 16, 16, LR_DEFAULTCOLOR);
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    wc.hbrBackground = g_hBrushBg;
    wc.lpszClassName = L"WirelessDaplinkAppClass";
    RegisterClassExW(&wc);

    RECT rc = { 0, 0, WIN_WIDTH, WIN_HEIGHT };
    AdjustWindowRect(&rc, WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX, FALSE);

    HWND hWnd = CreateWindowExW(0, L"WirelessDaplinkAppClass",
                                L"nRF52840 无线 DAPLink 配置助手 (AFH 智能跳频)",
                                WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX | WS_VISIBLE,
                                CW_USEDEFAULT, CW_USEDEFAULT, rc.right - rc.left, rc.bottom - rc.top,
                                NULL, NULL, hInst, NULL);

    SendMessageW(hWnd, WM_SETICON, ICON_BIG, (LPARAM)wc.hIcon);
    SendMessageW(hWnd, WM_SETICON, ICON_SMALL, (LPARAM)wc.hIconSm);

    MSG msg;
    while (GetMessageW(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }

    DeleteObject(g_hBrushBg);
    DeleteObject(g_hBrushCard);
    DeleteObject(g_hBrushInput);
    DeleteObject(g_hFontRegular);
    DeleteObject(g_hFontBold);

    return (int)msg.wParam;
}
