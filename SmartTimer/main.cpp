#include <windows.h>
#include <string>
#include <sstream>
#include <thread>
#include <chrono>
#include <iomanip>

// �Զ�������ͼ����Ϣ�����ڴ�������ͼ��Ľ���
#define WM_TRAYICON (WM_USER + 1)
#define ID_TRAY_ABOUT 1001 // �˵���ID��������
#define ID_TRAY_EXIT 1002  // �˵���ID���˳�

// ��������
LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
void StartTimer(HWND hwnd); // ������ʱ��
void AddTrayIcon(HWND hwnd, HINSTANCE hInstance); // �������ͼ��
void RemoveTrayIcon(HWND hwnd); // �Ƴ�����ͼ��
void ShowTrayMenu(HWND hwnd); // ��ʾ���̲˵�

// ȫ��������
HFONT hFontGlobal;

// �������
int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow) {
    const wchar_t CLASS_NAME[] = L"TimerWindowClass"; // ��������

    // ���崰����
    WNDCLASS wc = {};
    wc.lpfnWndProc = WindowProc; // ���ڹ��̺���
    wc.hInstance = hInstance; // Ӧ�ó���ʵ�����
    wc.lpszClassName = CLASS_NAME; // ��������
    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1); // ʹ��ϵͳ���ڱ���ɫ
    wc.hCursor = LoadCursor(NULL, IDC_ARROW); // �����һ��

    // ע�ᴰ����
    RegisterClass(&wc);

    // ����ȫ���������
    hFontGlobal = CreateFont(
        -MulDiv(14, GetDeviceCaps(GetDC(NULL), LOGPIXELSY), 72), // ����߶�
        0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET,
        OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY,
        DEFAULT_PITCH | FF_SWISS, L"΢���ź�" // ��������
    );

    // �����ޱ߿򴰿ڣ����÷ֲ㴰����ʽ
    HWND hwnd = CreateWindowEx(
        WS_EX_LAYERED,                  // ��չ��ʽ�����÷ֲ㴰��
        CLASS_NAME,                     // ��������
        L"�򵥶�ʱ��",                  // ���ڱ���
        WS_POPUP | WS_VISIBLE,          // �ޱ߿򴰿���ʽ
        CW_USEDEFAULT, CW_USEDEFAULT,   // ���ڳ�ʼλ��
        298, 41,                        // ���ڿ�Ⱥ͸߶�
        NULL,                           // �޸�����
        NULL,                           // �޲˵�
        hInstance,                      // Ӧ�ó���ʵ�����
        NULL                            // �޸��Ӳ���
    );

    // ������ڴ���ʧ�ܣ����� 0
    if (hwnd == NULL) {
        return 0;
    }

    // ���ô���Ϊ��͸����ɫ
    COLORREF transparentColor = RGB(0, 0, 255); // ��ɫ
    BYTE transparency = 255; // ͸���ȣ�0-255��255Ϊ��ȫ��͸����
    SetLayeredWindowAttributes(hwnd, 0, transparency, LWA_ALPHA);

    // ���������ڶ���
    SetWindowPos(hwnd, HWND_TOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE);

    // �������ͼ��
    AddTrayIcon(hwnd, hInstance);

    // ��Ϣѭ��
    MSG msg = {};
    while (GetMessage(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg); // ������Ϣ
        DispatchMessage(&msg);  // �ַ���Ϣ
    }

    // ɾ��ȫ���������
    DeleteObject(hFontGlobal);

    return 0;
}

// ���ڹ��̺���
LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    // ���徲̬�ؼ������״̬����
    static HWND hEditHours, hEditMinutes, hEditSeconds, hButtonStartPause, hStaticCountdown;
    static HWND hColon0, hColon1, hColon2; // ������ʾ����ʾ���͡�:���ľ�̬�ı���
    static HWND hButtonReset = NULL; // ���㰴ť
    static int remainingTime = 0; // ʣ��ʱ�䣨�룩
    static bool isPaused = false; // �Ƿ���ͣ
    static bool isRunning = false; // �Ƿ���������
    static bool isFlashing = false;
    static bool flashColorFlag = false;
    const UINT_PTR TIMER_ID_FLASH = 100;
    static int overtimeSeconds = 0;

    // �����϶����ڵı���
    static bool isDragging = false; // �Ƿ������϶�
    static POINT dragStartPoint = {}; // �϶���ʼ��

    static int wintop_pos = 5;
    static int start_pos = 10;
    static int line_height = 30;
	static int static_width = 36;
	static int label0_width = 90;
    static int inform_width = 174;

    switch (uMsg) {
    case WM_CREATE: {
        // ������һ������ʾ����̬�ı���
        hColon0 = CreateWindow(L"STATIC", L"����ʱ����", WS_VISIBLE | WS_CHILD | SS_CENTERIMAGE,
            start_pos, wintop_pos, label0_width, line_height, hwnd, NULL, NULL, NULL);
        
        // ����Сʱ�����
        hEditHours = CreateWindow(L"EDIT", L"00", WS_VISIBLE | WS_CHILD | WS_BORDER | ES_CENTER | SS_CENTERIMAGE,
            start_pos + label0_width, wintop_pos, static_width, line_height, hwnd, NULL, NULL, NULL);

        // ������һ����:����̬�ı���
        hColon1 = CreateWindow(L"STATIC", L":", WS_VISIBLE | WS_CHILD | SS_CENTER | SS_CENTERIMAGE,
            start_pos + label0_width + static_width * 1, wintop_pos, 10, line_height, hwnd, NULL, NULL, NULL);

        // �������������
        hEditMinutes = CreateWindow(L"EDIT", L"00", WS_VISIBLE | WS_CHILD | WS_BORDER | ES_CENTER | SS_CENTERIMAGE,
            start_pos + label0_width + static_width * 1 + 10, wintop_pos, static_width, line_height, hwnd, NULL, NULL, NULL);

        // �����ڶ�����:����̬�ı���
        hColon2 = CreateWindow(L"STATIC", L":", WS_VISIBLE | WS_CHILD | SS_CENTER | SS_CENTERIMAGE,
            start_pos + label0_width + static_width * 2 + 10, wintop_pos, 10, line_height, hwnd, NULL, NULL, NULL);

        // �������������
        hEditSeconds = CreateWindow(L"EDIT", L"00", WS_VISIBLE | WS_CHILD | WS_BORDER | ES_CENTER | SS_CENTERIMAGE,
            start_pos + label0_width + static_width * 2 + 20, wintop_pos, static_width, line_height, hwnd, NULL, NULL, NULL);

        // ������ʼ/��ͣ��ť
        hButtonStartPause = CreateWindow(L"BUTTON", L"��ʼ", WS_VISIBLE | WS_CHILD,
            start_pos + label0_width + static_width * 3 + 30, wintop_pos, 50, line_height, hwnd, (HMENU)1, NULL, NULL);

        // ��������ʱ��ʾ��ǩ
        hStaticCountdown = CreateWindow(L"STATIC", L"", WS_VISIBLE | WS_CHILD | SS_CENTERIMAGE,
            start_pos, wintop_pos, inform_width, line_height, hwnd, NULL, NULL, NULL);

        // �������㰴ť����ʼ����
        hButtonReset = CreateWindow(L"BUTTON", L"����", WS_VISIBLE | WS_CHILD,
            start_pos + inform_width, wintop_pos, 50, line_height, hwnd, (HMENU)2, NULL, NULL);
        SendMessage(hButtonReset, WM_SETFONT, (WPARAM)hFontGlobal, TRUE);
        ShowWindow(hButtonReset, SW_HIDE);

        // ���ÿؼ�����
        SendMessage(hColon0, WM_SETFONT, (WPARAM)hFontGlobal, TRUE);
        SendMessage(hEditHours, WM_SETFONT, (WPARAM)hFontGlobal, TRUE);
        SendMessage(hEditMinutes, WM_SETFONT, (WPARAM)hFontGlobal, TRUE);
        SendMessage(hEditSeconds, WM_SETFONT, (WPARAM)hFontGlobal, TRUE);
        SendMessage(hColon1, WM_SETFONT, (WPARAM)hFontGlobal, TRUE);
        SendMessage(hColon2, WM_SETFONT, (WPARAM)hFontGlobal, TRUE);
        SendMessage(hButtonStartPause, WM_SETFONT, (WPARAM)hFontGlobal, TRUE);
        SendMessage(hStaticCountdown, WM_SETFONT, (WPARAM)hFontGlobal, TRUE);

        // ���õ���ʱ�༭���ȱʡֵ��0Сʱ0��30��
        SetWindowText(hEditHours, L"00");
        SetWindowText(hEditMinutes, L"10");
        SetWindowText(hEditSeconds, L"00");

        // ��ʼ��ʱ���ص���ʱ�ؼ�
        ShowWindow(hStaticCountdown, SW_HIDE);

        // ���ô����������ͱ�����ͼ��
        HICON hIcon = LoadIcon(((LPCREATESTRUCT)lParam)->hInstance, MAKEINTRESOURCE(101));
        SendMessage(hwnd, WM_SETICON, ICON_BIG, (LPARAM)hIcon);   // ������/Alt+Tab�ô�ͼ��
        SendMessage(hwnd, WM_SETICON, ICON_SMALL, (LPARAM)hIcon); // ��������Сͼ��

        break;
    }
    case WM_LBUTTONDOWN: {
        // ����������벢��¼��ʼ��
        isDragging = true;
        SetCapture(hwnd);
        dragStartPoint.x = LOWORD(lParam);
        dragStartPoint.y = HIWORD(lParam);
        break;
    }
    case WM_MOUSEMOVE: {
        if (isDragging) {
            // ��ȡ��ǰ���λ��
            POINT currentPoint;
            currentPoint.x = LOWORD(lParam);
            currentPoint.y = HIWORD(lParam);

            // ��������ƶ���ƫ����
            int offsetX = currentPoint.x - dragStartPoint.x;
            int offsetY = currentPoint.y - dragStartPoint.y;

            // ��ȡ���ڵ�ǰλ��
            RECT windowRect;
            GetWindowRect(hwnd, &windowRect);

            // ���´���λ��
            SetWindowPos(hwnd, NULL,
                windowRect.left + offsetX,
                windowRect.top + offsetY,
                0, 0,
                SWP_NOSIZE | SWP_NOZORDER);
        }
        break;
    }
    case WM_LBUTTONUP: {
        // �ͷ���겶��
        if (isDragging) {
            isDragging = false;
            ReleaseCapture();
        }
        break;
    }
    case WM_PAINT: {
        // ʹ��ϵͳĬ�ϴ��ڱ���ɫ
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hwnd, &ps);
        HBRUSH hBrush = (HBRUSH)(COLOR_WINDOW + 1); // ϵͳĬ�ϱ���ɫ
        FillRect(hdc, &ps.rcPaint, hBrush); // ��䱳��

        // ���ƺ�ɫ�߿�
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
                SetTextColor(hdcStatic, RGB(0, 0, 255)); // ��ɫ
            } else {
                SetTextColor(hdcStatic, RGB(255, 0, 0)); // ��ɫ
            }
        } else {
            SetTextColor(hdcStatic, RGB(0, 0, 0)); // Ĭ�Ϻ�ɫ
        }
        SetBkColor(hdcStatic, GetSysColor(COLOR_WINDOW));
        return (LRESULT)GetSysColorBrush(COLOR_WINDOW);
    }
    case WM_COMMAND: {
        switch (LOWORD(wParam)) {
        case ID_TRAY_ABOUT:
            // ��ʾ���ڶԻ���
            MessageBox(hwnd, L"����һ���򵥵Ķ�ʱ��Ӧ�ã�������PPT�㱨ʱͬ����ʱ����\n����汾��V1.0\n������Ա��Ҧ��\n�������ߣ�Visual Studio + Copilot", L"����", MB_OK | MB_ICONINFORMATION);
            break;
        case ID_TRAY_EXIT:
            // �˳�����
            PostMessage(hwnd, WM_CLOSE, 0, 0);
            break;
        case 1: // ����/��ͣ��ť
            if (!isRunning) {
                // ������ʱ��
                wchar_t buffer[10];
                GetWindowText(hEditHours, buffer, 10);
                int hours = _wtoi(buffer);

                GetWindowText(hEditMinutes, buffer, 10);
                int minutes = _wtoi(buffer);

                GetWindowText(hEditSeconds, buffer, 10);
                int seconds = _wtoi(buffer);

                remainingTime = hours * 3600 + minutes * 60 + seconds; // ����������
                isRunning = true;
                isPaused = false;
                SetWindowText(hButtonStartPause, L"��ͣ"); // ���°�ť�ı�
                
                // ������ʱ���߳�
                std::thread timerThread(StartTimer, hwnd);
                timerThread.detach();

                // ����ˢ�µ���ʱ��ʾ
                {
                    int showHours = hours;
                    int showMinutes = minutes;
                    int showSeconds = seconds;
                    std::wstringstream ss;
                    ss << L"ʣ��"
                       << std::setw(2) << std::setfill(L'0') << showHours << L"ʱ"
                       << std::setw(2) << std::setfill(L'0') << showMinutes << L"��"
                       << std::setw(2) << std::setfill(L'0') << showSeconds << L"��";
                    SetWindowText(hStaticCountdown, ss.str().c_str());
                }

                // ��������ؼ�����ʾ
                ShowWindow(hEditHours, SW_HIDE);
                ShowWindow(hEditMinutes, SW_HIDE);
                ShowWindow(hEditSeconds, SW_HIDE);
                ShowWindow(hColon0, SW_HIDE);
                ShowWindow(hColon1, SW_HIDE);
                ShowWindow(hColon2, SW_HIDE);

                // ��ʾ����ʱ�ؼ�
                ShowWindow(hStaticCountdown, SW_SHOW);
                ShowWindow(hButtonReset, SW_HIDE); // ����ʱ���ع��㰴ť

                KillTimer(hwnd, TIMER_ID_FLASH);
                isFlashing = false;
                overtimeSeconds = 0;
            }
            else if (isPaused) {
                // �ָ���ʱ��
                isPaused = false;
                SetWindowText(hButtonStartPause, L"��ͣ");
                ShowWindow(hButtonReset, SW_HIDE); // ����ʱ���ع��㰴ť
            }
            else {
                // ��ͣ��ʱ��
                isPaused = true;
                SetWindowText(hButtonStartPause, L"����");
                ShowWindow(hButtonReset, SW_SHOW); // ��ͣʱ��ʾ���㰴ť
            }
            break;
        case 2: // ���㰴ť
            // ֹͣ��ʱ���ָ�����ؼ�����ʾ�����ص���ʱ�ؼ��͹��㰴ť
            isRunning = false;
            isPaused = false;
            KillTimer(hwnd, 1);
            KillTimer(hwnd, TIMER_ID_FLASH); // ֹͣ��˸
            isFlashing = false;
            overtimeSeconds = 0; // ���ó�ʱ����
            SetWindowText(hButtonStartPause, L"��ʼ");
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
                // ���µ���ʱ
                remainingTime--;
                int hours = remainingTime / 3600;
                int minutes = (remainingTime % 3600) / 60;
                int seconds = remainingTime % 60;

                std::wstringstream ss;
                ss << L"ʣ��"
                   << std::setw(2) << std::setfill(L'0') << hours << L"ʱ"
                   << std::setw(2) << std::setfill(L'0') << minutes << L"��"
                   << std::setw(2) << std::setfill(L'0') << seconds << L"��";
                SetWindowText(hStaticCountdown, ss.str().c_str());
            }
            else if (remainingTime == 0) {
                // ����ʱ����
                KillTimer(hwnd, 1);
                isRunning = false;
                
                // ������˸��ʱ��
                isFlashing = true;
                flashColorFlag = false;
                overtimeSeconds = 0; // ��ʱ��������
                SetTimer(hwnd, TIMER_ID_FLASH, 1000, NULL);

                //ShowWindow(hButtonStartPause, SW_HIDE);
                ShowWindow(hButtonReset, SW_SHOW);
                EnableWindow(hButtonStartPause, FALSE);
            }
        } else if (wParam == TIMER_ID_FLASH) {
            // �л���ɫ
            flashColorFlag = !flashColorFlag;
            // �ۼӳ�ʱ��������ʾ
            overtimeSeconds++;
            int hours = overtimeSeconds / 3600;
            int minutes = (overtimeSeconds % 3600) / 60;
            int seconds = overtimeSeconds % 60;
            std::wstringstream ss;
            ss << L"��ʱ"
               << std::setw(2) << std::setfill(L'0') << hours << L"ʱ"
               << std::setw(2) << std::setfill(L'0') << minutes << L"��"
               << std::setw(2) << std::setfill(L'0') << seconds << L"��";
            SetWindowText(hStaticCountdown, ss.str().c_str());
            InvalidateRect(hStaticCountdown, NULL, TRUE);
        }
        break;
    }
    case WM_TRAYICON: {
        if (lParam == WM_RBUTTONDOWN) {
            // �Ҽ��������ͼ�꣬��ʾ�˵�
            ShowTrayMenu(hwnd);
        }
        break;
    }
    case WM_RBUTTONDOWN: {
        // �ڴ�������λ���Ҽ����������̲˵�
        ShowTrayMenu(hwnd);
        break;
    }
    case WM_SETFOCUS:
        // ȷ�����ڻ�ü������뽹��
        SetFocus(hwnd);
        break;
    case WM_KEYDOWN:
        if (wParam == VK_ESCAPE) {
            PostMessage(hwnd, WM_CLOSE, 0, 0);
        }
        break;
    case WM_DESTROY:
        // �Ƴ�����ͼ�겢�˳�����
        RemoveTrayIcon(hwnd);
        PostQuitMessage(0);
        break;
    default:
        return DefWindowProc(hwnd, uMsg, wParam, lParam);
    }
    return 0;
}

// �������ͼ��
void AddTrayIcon(HWND hwnd, HINSTANCE hInstance) {
    NOTIFYICONDATA nid = {};
    nid.cbSize = sizeof(NOTIFYICONDATA);
    nid.hWnd = hwnd;
    nid.uID = 1;
    nid.uFlags = NIF_ICON | NIF_MESSAGE | NIF_TIP;
    nid.uCallbackMessage = WM_TRAYICON;
    nid.hIcon = LoadIcon(hInstance, MAKEINTRESOURCE(101)); // 101Ϊ��ͼ����ԴID
    wcscpy_s(nid.szTip, L"��ʱ��Ӧ��");
    Shell_NotifyIcon(NIM_ADD, &nid);
}

// �Ƴ�����ͼ��
void RemoveTrayIcon(HWND hwnd) {
    NOTIFYICONDATA nid = {};
    nid.cbSize = sizeof(NOTIFYICONDATA);
    nid.hWnd = hwnd;
    nid.uID = 1;

    Shell_NotifyIcon(NIM_DELETE, &nid); // ɾ������ͼ��
}

// ��ʾ���̲˵�
void ShowTrayMenu(HWND hwnd) {
    // �����˵�
    HMENU hMenu = CreatePopupMenu();
    AppendMenu(hMenu, MF_STRING, ID_TRAY_ABOUT, L"����");
    AppendMenu(hMenu, MF_STRING, ID_TRAY_EXIT, L"�˳�");

    // ��ȡ���λ��
    POINT cursorPos;
    GetCursorPos(&cursorPos);

    // ��ʾ�˵�
    SetForegroundWindow(hwnd); // ȷ���˵���ʾ��ǰ̨
    TrackPopupMenu(hMenu, TPM_RIGHTBUTTON, cursorPos.x, cursorPos.y, 0, hwnd, NULL);

    // ���ٲ˵�
    DestroyMenu(hMenu);
}

// ������ʱ��
void StartTimer(HWND hwnd) {
    SetTimer(hwnd, 1, 1000, NULL); // ÿ�봥��һ�� WM_TIMER ��Ϣ
}
