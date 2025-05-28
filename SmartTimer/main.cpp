#include <windows.h>
#include <string>
#include <sstream>
#include <thread>
#include <chrono>
#include <iomanip>

// 自定义托盘图标消息，用于处理托盘图标的交互
#define WM_TRAYICON (WM_USER + 1)
#define ID_TRAY_ABOUT 1001 // 菜单项ID：关于我
#define ID_TRAY_EXIT 1002  // 菜单项ID：退出

// 函数声明
LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
void StartTimer(HWND hwnd); // 启动计时器
void AddTrayIcon(HWND hwnd, HINSTANCE hInstance); // 添加托盘图标
void RemoveTrayIcon(HWND hwnd); // 移除托盘图标
void ShowTrayMenu(HWND hwnd); // 显示托盘菜单

// 全局字体句柄
HFONT hFontGlobal;

// 程序入口
int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow) {
    const wchar_t CLASS_NAME[] = L"TimerWindowClass"; // 窗口类名

    // 定义窗口类
    WNDCLASS wc = {};
    wc.lpfnWndProc = WindowProc; // 窗口过程函数
    wc.hInstance = hInstance; // 应用程序实例句柄
    wc.lpszClassName = CLASS_NAME; // 窗口类名
    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1); // 使用系统窗口背景色
    wc.hCursor = LoadCursor(NULL, IDC_ARROW); // 添加这一行

    // 注册窗口类
    RegisterClass(&wc);

    // 创建全局字体对象
    hFontGlobal = CreateFont(
        -MulDiv(14, GetDeviceCaps(GetDC(NULL), LOGPIXELSY), 72), // 字体高度
        0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET,
        OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY,
        DEFAULT_PITCH | FF_SWISS, L"微软雅黑" // 字体名称
    );

    // 创建无边框窗口，启用分层窗口样式
    HWND hwnd = CreateWindowEx(
        WS_EX_LAYERED,                  // 扩展样式，启用分层窗口
        CLASS_NAME,                     // 窗口类名
        L"简单定时器",                  // 窗口标题
        WS_POPUP | WS_VISIBLE,          // 无边框窗口样式
        CW_USEDEFAULT, CW_USEDEFAULT,   // 窗口初始位置
        298, 41,                        // 窗口宽度和高度
        NULL,                           // 无父窗口
        NULL,                           // 无菜单
        hInstance,                      // 应用程序实例句柄
        NULL                            // 无附加参数
    );

    // 如果窗口创建失败，返回 0
    if (hwnd == NULL) {
        return 0;
    }

    // 设置窗口为半透明蓝色
    COLORREF transparentColor = RGB(0, 0, 255); // 蓝色
    BYTE transparency = 255; // 透明度（0-255，255为完全不透明）
    SetLayeredWindowAttributes(hwnd, 0, transparency, LWA_ALPHA);

    // 将窗口置于顶层
    SetWindowPos(hwnd, HWND_TOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE);

    // 添加托盘图标
    AddTrayIcon(hwnd, hInstance);

    // 消息循环
    MSG msg = {};
    while (GetMessage(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg); // 翻译消息
        DispatchMessage(&msg);  // 分发消息
    }

    // 删除全局字体对象
    DeleteObject(hFontGlobal);

    return 0;
}

// 窗口过程函数
LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    // 定义静态控件句柄和状态变量
    static HWND hEditHours, hEditMinutes, hEditSeconds, hButtonStartPause, hStaticCountdown;
    static HWND hColon0, hColon1, hColon2; // 用于显示“提示”和“:”的静态文本框
    static HWND hButtonReset = NULL; // 归零按钮
    static int remainingTime = 0; // 剩余时间（秒）
    static bool isPaused = false; // 是否暂停
    static bool isRunning = false; // 是否正在运行
    static bool isFlashing = false;
    static bool flashColorFlag = false;
    const UINT_PTR TIMER_ID_FLASH = 100;
    static int overtimeSeconds = 0;

    // 用于拖动窗口的变量
    static bool isDragging = false; // 是否正在拖动
    static POINT dragStartPoint = {}; // 拖动起始点

    static int wintop_pos = 5;
    static int start_pos = 10;
    static int line_height = 30;
	static int static_width = 36;
	static int label0_width = 90;
    static int inform_width = 174;

    switch (uMsg) {
    case WM_CREATE: {
        // 创建第一个“提示”静态文本框
        hColon0 = CreateWindow(L"STATIC", L"倒计时长：", WS_VISIBLE | WS_CHILD | SS_CENTERIMAGE,
            start_pos, wintop_pos, label0_width, line_height, hwnd, NULL, NULL, NULL);
        
        // 创建小时输入框
        hEditHours = CreateWindow(L"EDIT", L"00", WS_VISIBLE | WS_CHILD | WS_BORDER | ES_CENTER | SS_CENTERIMAGE,
            start_pos + label0_width, wintop_pos, static_width, line_height, hwnd, NULL, NULL, NULL);

        // 创建第一个“:”静态文本框
        hColon1 = CreateWindow(L"STATIC", L":", WS_VISIBLE | WS_CHILD | SS_CENTER | SS_CENTERIMAGE,
            start_pos + label0_width + static_width * 1, wintop_pos, 10, line_height, hwnd, NULL, NULL, NULL);

        // 创建分钟输入框
        hEditMinutes = CreateWindow(L"EDIT", L"00", WS_VISIBLE | WS_CHILD | WS_BORDER | ES_CENTER | SS_CENTERIMAGE,
            start_pos + label0_width + static_width * 1 + 10, wintop_pos, static_width, line_height, hwnd, NULL, NULL, NULL);

        // 创建第二个“:”静态文本框
        hColon2 = CreateWindow(L"STATIC", L":", WS_VISIBLE | WS_CHILD | SS_CENTER | SS_CENTERIMAGE,
            start_pos + label0_width + static_width * 2 + 10, wintop_pos, 10, line_height, hwnd, NULL, NULL, NULL);

        // 创建秒钟输入框
        hEditSeconds = CreateWindow(L"EDIT", L"00", WS_VISIBLE | WS_CHILD | WS_BORDER | ES_CENTER | SS_CENTERIMAGE,
            start_pos + label0_width + static_width * 2 + 20, wintop_pos, static_width, line_height, hwnd, NULL, NULL, NULL);

        // 创建开始/暂停按钮
        hButtonStartPause = CreateWindow(L"BUTTON", L"开始", WS_VISIBLE | WS_CHILD,
            start_pos + label0_width + static_width * 3 + 30, wintop_pos, 50, line_height, hwnd, (HMENU)1, NULL, NULL);

        // 创建倒计时显示标签
        hStaticCountdown = CreateWindow(L"STATIC", L"", WS_VISIBLE | WS_CHILD | SS_CENTERIMAGE,
            start_pos, wintop_pos, inform_width, line_height, hwnd, NULL, NULL, NULL);

        // 创建归零按钮，初始隐藏
        hButtonReset = CreateWindow(L"BUTTON", L"归零", WS_VISIBLE | WS_CHILD,
            start_pos + inform_width, wintop_pos, 50, line_height, hwnd, (HMENU)2, NULL, NULL);
        SendMessage(hButtonReset, WM_SETFONT, (WPARAM)hFontGlobal, TRUE);
        ShowWindow(hButtonReset, SW_HIDE);

        // 设置控件字体
        SendMessage(hColon0, WM_SETFONT, (WPARAM)hFontGlobal, TRUE);
        SendMessage(hEditHours, WM_SETFONT, (WPARAM)hFontGlobal, TRUE);
        SendMessage(hEditMinutes, WM_SETFONT, (WPARAM)hFontGlobal, TRUE);
        SendMessage(hEditSeconds, WM_SETFONT, (WPARAM)hFontGlobal, TRUE);
        SendMessage(hColon1, WM_SETFONT, (WPARAM)hFontGlobal, TRUE);
        SendMessage(hColon2, WM_SETFONT, (WPARAM)hFontGlobal, TRUE);
        SendMessage(hButtonStartPause, WM_SETFONT, (WPARAM)hFontGlobal, TRUE);
        SendMessage(hStaticCountdown, WM_SETFONT, (WPARAM)hFontGlobal, TRUE);

        // 设置倒计时编辑框的缺省值：0小时0分30秒
        SetWindowText(hEditHours, L"00");
        SetWindowText(hEditMinutes, L"10");
        SetWindowText(hEditSeconds, L"00");

        // 初始化时隐藏倒计时控件
        ShowWindow(hStaticCountdown, SW_HIDE);

        // 设置窗口任务栏和标题栏图标
        HICON hIcon = LoadIcon(((LPCREATESTRUCT)lParam)->hInstance, MAKEINTRESOURCE(101));
        SendMessage(hwnd, WM_SETICON, ICON_BIG, (LPARAM)hIcon);   // 任务栏/Alt+Tab用大图标
        SendMessage(hwnd, WM_SETICON, ICON_SMALL, (LPARAM)hIcon); // 标题栏用小图标

        break;
    }
    case WM_LBUTTONDOWN: {
        // 捕获鼠标输入并记录起始点
        isDragging = true;
        SetCapture(hwnd);
        dragStartPoint.x = LOWORD(lParam);
        dragStartPoint.y = HIWORD(lParam);
        break;
    }
    case WM_MOUSEMOVE: {
        if (isDragging) {
            // 获取当前鼠标位置
            POINT currentPoint;
            currentPoint.x = LOWORD(lParam);
            currentPoint.y = HIWORD(lParam);

            // 计算鼠标移动的偏移量
            int offsetX = currentPoint.x - dragStartPoint.x;
            int offsetY = currentPoint.y - dragStartPoint.y;

            // 获取窗口当前位置
            RECT windowRect;
            GetWindowRect(hwnd, &windowRect);

            // 更新窗口位置
            SetWindowPos(hwnd, NULL,
                windowRect.left + offsetX,
                windowRect.top + offsetY,
                0, 0,
                SWP_NOSIZE | SWP_NOZORDER);
        }
        break;
    }
    case WM_LBUTTONUP: {
        // 释放鼠标捕获
        if (isDragging) {
            isDragging = false;
            ReleaseCapture();
        }
        break;
    }
    case WM_PAINT: {
        // 使用系统默认窗口背景色
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hwnd, &ps);
        HBRUSH hBrush = (HBRUSH)(COLOR_WINDOW + 1); // 系统默认背景色
        FillRect(hdc, &ps.rcPaint, hBrush); // 填充背景

        // 绘制黑色边框
        //HBRUSH hBorder = CreateSolidBrush(RGB(0, 0, 0));
        //FrameRect(hdc, &ps.rcPaint, hBorder);
        //DeleteObject(hBorder);

        EndPaint(hwnd, &ps);
        break;
    }
    case WM_CTLCOLORSTATIC: {
        HDC hdcStatic = (HDC)wParam;
        HWND hCtrl = (HWND)lParam;
        if (isFlashing && hCtrl == hStaticCountdown) {
            if (flashColorFlag) {
                SetTextColor(hdcStatic, RGB(0, 0, 255)); // 蓝色
            } else {
                SetTextColor(hdcStatic, RGB(255, 0, 0)); // 红色
            }
        } else {
            SetTextColor(hdcStatic, RGB(0, 0, 0)); // 默认黑色
        }
        SetBkColor(hdcStatic, GetSysColor(COLOR_WINDOW));
        return (LRESULT)GetSysColorBrush(COLOR_WINDOW);
    }
    case WM_COMMAND: {
        switch (LOWORD(wParam)) {
        case ID_TRAY_ABOUT:
            // 显示关于对话框
            MessageBox(hwnd, L"这是一个简单的定时器应用，可用于PPT汇报时同步计时提醒\n程序版本：V1.0\n开发人员：姚亮\n开发工具：Visual Studio + Copilot", L"关于", MB_OK | MB_ICONINFORMATION);
            break;
        case ID_TRAY_EXIT:
            // 退出程序
            PostMessage(hwnd, WM_CLOSE, 0, 0);
            break;
        case 1: // 启动/暂停按钮
            if (!isRunning) {
                // 启动计时器
                wchar_t buffer[10];
                GetWindowText(hEditHours, buffer, 10);
                int hours = _wtoi(buffer);

                GetWindowText(hEditMinutes, buffer, 10);
                int minutes = _wtoi(buffer);

                GetWindowText(hEditSeconds, buffer, 10);
                int seconds = _wtoi(buffer);

                remainingTime = hours * 3600 + minutes * 60 + seconds; // 计算总秒数
                isRunning = true;
                isPaused = false;
                SetWindowText(hButtonStartPause, L"暂停"); // 更新按钮文本
                
                // 启动计时器线程
                std::thread timerThread(StartTimer, hwnd);
                timerThread.detach();

                // 立即刷新倒计时显示
                {
                    int showHours = hours;
                    int showMinutes = minutes;
                    int showSeconds = seconds;
                    std::wstringstream ss;
                    ss << L"剩余"
                       << std::setw(2) << std::setfill(L'0') << showHours << L"时"
                       << std::setw(2) << std::setfill(L'0') << showMinutes << L"分"
                       << std::setw(2) << std::setfill(L'0') << showSeconds << L"秒";
                    SetWindowText(hStaticCountdown, ss.str().c_str());
                }

                // 隐藏输入控件和提示
                ShowWindow(hEditHours, SW_HIDE);
                ShowWindow(hEditMinutes, SW_HIDE);
                ShowWindow(hEditSeconds, SW_HIDE);
                ShowWindow(hColon0, SW_HIDE);
                ShowWindow(hColon1, SW_HIDE);
                ShowWindow(hColon2, SW_HIDE);

                // 显示倒计时控件
                ShowWindow(hStaticCountdown, SW_SHOW);
                ShowWindow(hButtonReset, SW_HIDE); // 启动时隐藏归零按钮

                KillTimer(hwnd, TIMER_ID_FLASH);
                isFlashing = false;
                overtimeSeconds = 0;
            }
            else if (isPaused) {
                // 恢复计时器
                isPaused = false;
                SetWindowText(hButtonStartPause, L"暂停");
                ShowWindow(hButtonReset, SW_HIDE); // 继续时隐藏归零按钮
            }
            else {
                // 暂停计时器
                isPaused = true;
                SetWindowText(hButtonStartPause, L"继续");
                ShowWindow(hButtonReset, SW_SHOW); // 暂停时显示归零按钮
            }
            break;
        case 2: // 归零按钮
            // 停止计时，恢复输入控件和提示，隐藏倒计时控件和归零按钮
            isRunning = false;
            isPaused = false;
            KillTimer(hwnd, 1);
            KillTimer(hwnd, TIMER_ID_FLASH); // 停止闪烁
            isFlashing = false;
            overtimeSeconds = 0; // 重置超时秒数
            SetWindowText(hButtonStartPause, L"开始");
            ShowWindow(hEditHours, SW_SHOW);
            ShowWindow(hEditMinutes, SW_SHOW);
            ShowWindow(hEditSeconds, SW_SHOW);
            ShowWindow(hColon0, SW_SHOW);
            ShowWindow(hColon1, SW_SHOW);
            ShowWindow(hColon2, SW_SHOW);
            ShowWindow(hStaticCountdown, SW_HIDE);
            ShowWindow(hButtonStartPause, SW_SHOW);
            ShowWindow(hButtonReset, SW_HIDE);
            EnableWindow(hButtonStartPause, TRUE);
            break;
        }
        break;
    }
    case WM_TIMER: {
        if (wParam == 1) {
            if (!isPaused && remainingTime > 0) {
                // 更新倒计时
                remainingTime--;
                int hours = remainingTime / 3600;
                int minutes = (remainingTime % 3600) / 60;
                int seconds = remainingTime % 60;

                std::wstringstream ss;
                ss << L"剩余"
                   << std::setw(2) << std::setfill(L'0') << hours << L"时"
                   << std::setw(2) << std::setfill(L'0') << minutes << L"分"
                   << std::setw(2) << std::setfill(L'0') << seconds << L"秒";
                SetWindowText(hStaticCountdown, ss.str().c_str());
            }
            else if (remainingTime == 0) {
                // 倒计时结束
                KillTimer(hwnd, 1);
                isRunning = false;
                
                // 启动闪烁定时器
                isFlashing = true;
                flashColorFlag = false;
                overtimeSeconds = 0; // 超时秒数清零
                SetTimer(hwnd, TIMER_ID_FLASH, 1000, NULL);

                //ShowWindow(hButtonStartPause, SW_HIDE);
                ShowWindow(hButtonReset, SW_SHOW);
                EnableWindow(hButtonStartPause, FALSE);
            }
        } else if (wParam == TIMER_ID_FLASH) {
            // 切换颜色
            flashColorFlag = !flashColorFlag;
            // 累加超时秒数并显示
            overtimeSeconds++;
            int hours = overtimeSeconds / 3600;
            int minutes = (overtimeSeconds % 3600) / 60;
            int seconds = overtimeSeconds % 60;
            std::wstringstream ss;
            ss << L"超时"
               << std::setw(2) << std::setfill(L'0') << hours << L"时"
               << std::setw(2) << std::setfill(L'0') << minutes << L"分"
               << std::setw(2) << std::setfill(L'0') << seconds << L"秒";
            SetWindowText(hStaticCountdown, ss.str().c_str());
            InvalidateRect(hStaticCountdown, NULL, TRUE);
        }
        break;
    }
    case WM_TRAYICON: {
        if (lParam == WM_RBUTTONDOWN) {
            // 右键点击托盘图标，显示菜单
            ShowTrayMenu(hwnd);
        }
        break;
    }
    case WM_RBUTTONDOWN: {
        // 在窗体任意位置右键，弹出托盘菜单
        ShowTrayMenu(hwnd);
        break;
    }
    case WM_SETFOCUS:
        // 确保窗口获得键盘输入焦点
        SetFocus(hwnd);
        break;
    case WM_KEYDOWN:
        if (wParam == VK_ESCAPE) {
            PostMessage(hwnd, WM_CLOSE, 0, 0);
        }
        break;
    case WM_DESTROY:
        // 移除托盘图标并退出程序
        RemoveTrayIcon(hwnd);
        PostQuitMessage(0);
        break;
    default:
        return DefWindowProc(hwnd, uMsg, wParam, lParam);
    }
    return 0;
}

// 添加托盘图标
void AddTrayIcon(HWND hwnd, HINSTANCE hInstance) {
    NOTIFYICONDATA nid = {};
    nid.cbSize = sizeof(NOTIFYICONDATA);
    nid.hWnd = hwnd;
    nid.uID = 1;
    nid.uFlags = NIF_ICON | NIF_MESSAGE | NIF_TIP;
    nid.uCallbackMessage = WM_TRAYICON;
    nid.hIcon = LoadIcon(hInstance, MAKEINTRESOURCE(101)); // 101为新图标资源ID
    wcscpy_s(nid.szTip, L"定时器应用");
    Shell_NotifyIcon(NIM_ADD, &nid);
}

// 移除托盘图标
void RemoveTrayIcon(HWND hwnd) {
    NOTIFYICONDATA nid = {};
    nid.cbSize = sizeof(NOTIFYICONDATA);
    nid.hWnd = hwnd;
    nid.uID = 1;

    Shell_NotifyIcon(NIM_DELETE, &nid); // 删除托盘图标
}

// 显示托盘菜单
void ShowTrayMenu(HWND hwnd) {
    // 创建菜单
    HMENU hMenu = CreatePopupMenu();
    AppendMenu(hMenu, MF_STRING, ID_TRAY_ABOUT, L"关于");
    AppendMenu(hMenu, MF_STRING, ID_TRAY_EXIT, L"退出");

    // 获取鼠标位置
    POINT cursorPos;
    GetCursorPos(&cursorPos);

    // 显示菜单
    SetForegroundWindow(hwnd); // 确保菜单显示在前台
    TrackPopupMenu(hMenu, TPM_RIGHTBUTTON, cursorPos.x, cursorPos.y, 0, hwnd, NULL);

    // 销毁菜单
    DestroyMenu(hMenu);
}

// 启动计时器
void StartTimer(HWND hwnd) {
    SetTimer(hwnd, 1, 1000, NULL); // 每秒触发一次 WM_TIMER 消息
}
