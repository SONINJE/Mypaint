/**
 * @file MyPaintDlg.cpp
 * @brief 주 대화 상자 클래스 구현
 */
#include "pch.h"
#include "MyPaint.h"
#include "MyPaintDlg.h"
#include "SizeDlg.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#endif

namespace
{
constexpr UINT WM_APP_RESTORE_TOOL = WM_APP + 1;
constexpr int kDefaultWidth = 1024;
constexpr int kDefaultHeight = 768;

constexpr const wchar_t* kToolLabels[kToolCount] = {
    L"선택", L"연필", L"브러시", L"지우개", L"채우기", L"스포이트", L"텍스트", L"직선", L"사각형", L"타원",
};
constexpr int kDefaultToolWidths[kToolCount] = {1, 1, 5, 12, 1, 1, 1, 3, 3, 3};
constexpr int kWidthChoices[] = {1, 2, 3, 4, 5, 6, 8, 10, 12, 16, 20, 24, 32, 40, 60};

constexpr const wchar_t* kOpenFilter =
    L"이미지 파일 (*.png;*.jpg;*.jpeg;*.bmp;*.gif;*.tif;*.tiff)|*.png;*.jpg;*.jpeg;*.jpe;*.bmp;*.dib;*.gif;*.tif;*.tiff|"
    L"모든 파일 (*.*)|*.*||";
constexpr const wchar_t* kSaveFilter = L"PNG (*.png)|*.png|JPEG (*.jpg)|*.jpg;*.jpeg|BMP (*.bmp)|*.bmp|"
                                       L"GIF (*.gif)|*.gif|TIFF (*.tif)|*.tif;*.tiff||";
constexpr const wchar_t* kSaveExtensions[] = {L".png", L".jpg", L".bmp", L".gif", L".tif"};

bool UsesWidth(ToolKind kind)
{
    switch (kind)
    {
    case ToolKind::Pencil:
    case ToolKind::Brush:
    case ToolKind::Eraser:
    case ToolKind::Line:
    case ToolKind::Rectangle:
    case ToolKind::Ellipse:
        return true;
    default:
        return false;
    }
}

bool IsEditControlFocused()
{
    HWND focus = ::GetFocus();
    if (!focus)
        return false;
    wchar_t cls[32]{};
    ::GetClassNameW(focus, cls, static_cast<int>(std::size(cls)));
    return _wcsicmp(cls, L"Edit") == 0;
}

void SetTextIfChanged(CWnd& wnd, const CString& text)
{
    CString current;
    wnd.GetWindowText(current);
    if (current != text)
        wnd.SetWindowText(text);
}
} // namespace

BEGIN_MESSAGE_MAP(CMyPaintDlg, CDialog)
    ON_WM_SIZE()
    ON_WM_GETMINMAXINFO()
    ON_WM_INITMENUPOPUP()
    ON_WM_DROPFILES()
    ON_MESSAGE(WM_DPICHANGED, &CMyPaintDlg::OnDpiChanged)
    ON_MESSAGE(WM_APP_RESTORE_TOOL, &CMyPaintDlg::OnRestoreTool)
    ON_CONTROL_RANGE(BN_CLICKED, IDC_TOOL_FIRST, IDC_TOOL_LAST, &CMyPaintDlg::OnToolButton)
    ON_CBN_SELCHANGE(IDC_WIDTH_COMBO, &CMyPaintDlg::OnWidthChange)
    ON_CBN_SELCHANGE(IDC_STYLE_COMBO, &CMyPaintDlg::OnStyleChange)
    ON_BN_CLICKED(IDC_FONT_BUTTON, &CMyPaintDlg::OnFontButton)
    ON_COMMAND(ID_FILE_NEW, &CMyPaintDlg::OnFileNew)
    ON_COMMAND(ID_FILE_OPEN, &CMyPaintDlg::OnFileOpen)
    ON_COMMAND(ID_FILE_SAVE, &CMyPaintDlg::OnFileSave)
    ON_COMMAND(ID_FILE_SAVE_AS, &CMyPaintDlg::OnFileSaveAs)
    ON_COMMAND(ID_APP_EXIT, &CMyPaintDlg::OnAppExit)
    ON_COMMAND(ID_EDIT_UNDO, &CMyPaintDlg::OnEditUndo)
    ON_COMMAND(ID_EDIT_REDO, &CMyPaintDlg::OnEditRedo)
    ON_COMMAND(ID_EDIT_CUT, &CMyPaintDlg::OnEditCut)
    ON_COMMAND(ID_EDIT_COPY, &CMyPaintDlg::OnEditCopy)
    ON_COMMAND(ID_EDIT_PASTE, &CMyPaintDlg::OnEditPaste)
    ON_COMMAND(ID_EDIT_CLEAR, &CMyPaintDlg::OnEditClear)
    ON_COMMAND(ID_EDIT_SELECT_ALL, &CMyPaintDlg::OnEditSelectAll)
    ON_COMMAND(ID_IMAGE_RESIZE, &CMyPaintDlg::OnImageResize)
    ON_COMMAND(ID_IMAGE_CANVAS_SIZE, &CMyPaintDlg::OnImageCanvasSize)
    ON_COMMAND(ID_IMAGE_ROTATE_RIGHT, &CMyPaintDlg::OnImageRotateRight)
    ON_COMMAND(ID_IMAGE_ROTATE_LEFT, &CMyPaintDlg::OnImageRotateLeft)
    ON_COMMAND(ID_IMAGE_ROTATE_180, &CMyPaintDlg::OnImageRotate180)
    ON_COMMAND(ID_IMAGE_FLIP_H, &CMyPaintDlg::OnImageFlipH)
    ON_COMMAND(ID_IMAGE_FLIP_V, &CMyPaintDlg::OnImageFlipV)
    ON_COMMAND(ID_IMAGE_CLEAR, &CMyPaintDlg::OnImageClear)
    ON_COMMAND(ID_VIEW_ZOOM_IN, &CMyPaintDlg::OnViewZoomIn)
    ON_COMMAND(ID_VIEW_ZOOM_OUT, &CMyPaintDlg::OnViewZoomOut)
    ON_COMMAND(ID_VIEW_ZOOM_100, &CMyPaintDlg::OnViewZoom100)
    ON_COMMAND(ID_VIEW_ZOOM_FIT, &CMyPaintDlg::OnViewZoomFit)
END_MESSAGE_MAP()

CMyPaintDlg::CMyPaintDlg(CWnd* pParent) : CDialog(IDD_MYPAINT_DIALOG, pParent)
{
    m_hIcon = AfxGetApp()->LoadIcon(IDR_MAINFRAME);
    std::copy(std::begin(kDefaultToolWidths), std::end(kDefaultToolWidths), m_toolWidths.begin());

    LOGFONTW& lf = m_settings.font;
    lf.lfHeight = -21; // 16pt (96 DPI 기준)
    lf.lfWeight = FW_NORMAL;
    lf.lfCharSet = DEFAULT_CHARSET;
    wcscpy_s(lf.lfFaceName, L"Malgun Gothic");
}

void CMyPaintDlg::DoDataExchange(CDataExchange* pDX)
{
    CDialog::DoDataExchange(pDX);
}

BOOL CMyPaintDlg::OnInitDialog()
{
    CDialog::OnInitDialog();

    SetIcon(m_hIcon, TRUE);  // 큰 아이콘
    SetIcon(m_hIcon, FALSE); // 작은 아이콘

    m_dpi = ::GetDpiForWindow(m_hWnd);
    CreateUiFont();
    CreateMenuBar();
    CreateControls();

    m_canvas.New(kDefaultWidth, kDefaultHeight, RGB(255, 255, 255));

    // 시작 창 크기: 작업 영역의 80% 정도
    CRect work;
    ::SystemParametersInfo(SPI_GETWORKAREA, 0, &work, 0);
    const int w = (std::min)(S(1200), work.Width() * 9 / 10);
    const int h = (std::min)(S(860), work.Height() * 9 / 10);
    SetWindowPos(nullptr, work.left + (work.Width() - w) / 2, work.top + (work.Height() - h) / 2, w, h,
                 SWP_NOZORDER | SWP_NOACTIVATE);

    Layout();
    m_view.OnImageReplaced();
    SelectTool(ToolKind::Pencil);

    // 명령줄로 받은 파일 열기 (탐색기에서 "연결 프로그램" 또는 아이콘에 끌어 놓기)
    if (__argc > 1 && __wargv)
        OpenFile(__wargv[1]);

    UpdateTitle();
    UpdateStatus();
    m_view.SetFocus();
    return FALSE; // 포커스를 직접 지정함
}

// ---------------------------------------------------------------------------
// 화면 구성
// ---------------------------------------------------------------------------

void CMyPaintDlg::CreateUiFont()
{
    LOGFONTW lf{};
    lf.lfHeight = -MulDiv(9, static_cast<int>(m_dpi), 72);
    lf.lfWeight = FW_NORMAL;
    lf.lfCharSet = DEFAULT_CHARSET;
    lf.lfQuality = CLEARTYPE_QUALITY;
    wcscpy_s(lf.lfFaceName, L"Malgun Gothic");

    CFont font;
    font.CreateFontIndirect(&lf);

    // 새 글꼴을 컨트롤에 먼저 적용한 다음 이전 글꼴을 지웁니다.
    auto apply = [&font](CWnd& wnd) {
        if (wnd.GetSafeHwnd())
            wnd.SetFont(&font, FALSE);
    };
    for (CButton& button : m_toolButtons)
        apply(button);
    apply(m_widthLabel);
    apply(m_widthCombo);
    apply(m_styleLabel);
    apply(m_styleCombo);
    apply(m_fontButton);
    apply(m_statusPos);
    apply(m_statusSelection);
    apply(m_statusSize);
    apply(m_statusZoom);

    m_uiFont.DeleteObject();
    m_uiFont.Attach(font.Detach());
    m_colorBar.SetUiFont(&m_uiFont);
}

void CMyPaintDlg::CreateMenuBar()
{
    CMenu file;
    file.CreatePopupMenu();
    file.AppendMenu(MF_STRING, ID_FILE_NEW, L"새로 만들기(&N)\tCtrl+N");
    file.AppendMenu(MF_STRING, ID_FILE_OPEN, L"열기(&O)...\tCtrl+O");
    file.AppendMenu(MF_STRING, ID_FILE_SAVE, L"저장(&S)\tCtrl+S");
    file.AppendMenu(MF_STRING, ID_FILE_SAVE_AS, L"다른 이름으로 저장(&A)...\tCtrl+Shift+S");
    file.AppendMenu(MF_SEPARATOR);
    file.AppendMenu(MF_STRING, ID_APP_EXIT, L"끝내기(&X)\tAlt+F4");

    CMenu edit;
    edit.CreatePopupMenu();
    edit.AppendMenu(MF_STRING, ID_EDIT_UNDO, L"실행 취소(&U)\tCtrl+Z");
    edit.AppendMenu(MF_STRING, ID_EDIT_REDO, L"다시 실행(&R)\tCtrl+Y");
    edit.AppendMenu(MF_SEPARATOR);
    edit.AppendMenu(MF_STRING, ID_EDIT_CUT, L"잘라내기(&T)\tCtrl+X");
    edit.AppendMenu(MF_STRING, ID_EDIT_COPY, L"복사(&C)\tCtrl+C");
    edit.AppendMenu(MF_STRING, ID_EDIT_PASTE, L"붙여넣기(&P)\tCtrl+V");
    edit.AppendMenu(MF_STRING, ID_EDIT_CLEAR, L"선택 영역 삭제(&D)\tDel");
    edit.AppendMenu(MF_SEPARATOR);
    edit.AppendMenu(MF_STRING, ID_EDIT_SELECT_ALL, L"모두 선택(&A)\tCtrl+A");

    CMenu image;
    image.CreatePopupMenu();
    image.AppendMenu(MF_STRING, ID_IMAGE_RESIZE, L"이미지 크기 조정(&R)...\tCtrl+W");
    image.AppendMenu(MF_STRING, ID_IMAGE_CANVAS_SIZE, L"캔버스 크기(&C)...");
    image.AppendMenu(MF_SEPARATOR);
    image.AppendMenu(MF_STRING, ID_IMAGE_ROTATE_RIGHT, L"오른쪽으로 90° 회전");
    image.AppendMenu(MF_STRING, ID_IMAGE_ROTATE_LEFT, L"왼쪽으로 90° 회전");
    image.AppendMenu(MF_STRING, ID_IMAGE_ROTATE_180, L"180° 회전");
    image.AppendMenu(MF_STRING, ID_IMAGE_FLIP_H, L"좌우 대칭");
    image.AppendMenu(MF_STRING, ID_IMAGE_FLIP_V, L"상하 대칭");
    image.AppendMenu(MF_SEPARATOR);
    image.AppendMenu(MF_STRING, ID_IMAGE_CLEAR, L"모두 지우기(색 2로 채우기)");

    CMenu view;
    view.CreatePopupMenu();
    view.AppendMenu(MF_STRING, ID_VIEW_ZOOM_IN, L"확대(&I)\tCtrl++");
    view.AppendMenu(MF_STRING, ID_VIEW_ZOOM_OUT, L"축소(&O)\tCtrl+-");
    view.AppendMenu(MF_STRING, ID_VIEW_ZOOM_100, L"실제 크기(&A)\tCtrl+0");
    view.AppendMenu(MF_STRING, ID_VIEW_ZOOM_FIT, L"창에 맞추기(&F)");

    m_menu.CreateMenu();
    m_menu.AppendMenu(MF_POPUP, reinterpret_cast<UINT_PTR>(file.Detach()), L"파일(&F)");
    m_menu.AppendMenu(MF_POPUP, reinterpret_cast<UINT_PTR>(edit.Detach()), L"편집(&E)");
    m_menu.AppendMenu(MF_POPUP, reinterpret_cast<UINT_PTR>(image.Detach()), L"이미지(&I)");
    m_menu.AppendMenu(MF_POPUP, reinterpret_cast<UINT_PTR>(view.Detach()), L"보기(&V)");
    SetMenu(&m_menu);
}

void CMyPaintDlg::CreateControls()
{
    const CRect zero(0, 0, 0, 0);
    for (int i = 0; i < kToolCount; ++i)
    {
        DWORD style = WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_AUTORADIOBUTTON | BS_PUSHLIKE;
        if (i == 0)
            style |= WS_GROUP;
        m_toolButtons[i].Create(kToolLabels[i], style, zero, this, IDC_TOOL_FIRST + i);
    }

    m_widthLabel.Create(L"두께", WS_CHILD | WS_VISIBLE | SS_LEFT | SS_CENTERIMAGE, zero, this, IDC_WIDTH_LABEL);
    m_widthCombo.Create(WS_CHILD | WS_VISIBLE | WS_TABSTOP | WS_VSCROLL | CBS_DROPDOWNLIST | WS_GROUP,
                        CRect(0, 0, 10, 300), this, IDC_WIDTH_COMBO);
    for (const int width : kWidthChoices)
    {
        CString text;
        text.Format(L"%d px", width);
        const int index = m_widthCombo.AddString(text);
        m_widthCombo.SetItemData(index, static_cast<DWORD_PTR>(width));
    }

    m_styleLabel.Create(L"도형", WS_CHILD | WS_VISIBLE | SS_LEFT | SS_CENTERIMAGE, zero, this, IDC_STYLE_LABEL);
    m_styleCombo.Create(WS_CHILD | WS_VISIBLE | WS_TABSTOP | CBS_DROPDOWNLIST, CRect(0, 0, 10, 300), this,
                        IDC_STYLE_COMBO);
    m_styleCombo.AddString(L"윤곽선");
    m_styleCombo.AddString(L"채우기");
    m_styleCombo.AddString(L"윤곽선 + 채우기(색 2)");
    m_styleCombo.SetCurSel(static_cast<int>(m_settings.shapeStyle));

    m_fontButton.Create(L"글꼴", WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_PUSHBUTTON, zero, this, IDC_FONT_BUTTON);

    m_colorBar.Create(this, IDC_COLOR_BAR);
    m_colorBar.Attach(&m_settings);
    m_colorBar.SetDpi(m_dpi);
    m_colorBar.SetUiFont(&m_uiFont);
    m_colorBar.onChanged = [this]() { m_view.OnSettingsChanged(); };

    m_view.Create(this, IDC_CANVAS_VIEW);
    m_view.Attach(&m_canvas, &m_settings);
    m_view.onModified = [this]() { UpdateTitle(); };
    m_view.onStatusChanged = [this]() { UpdateStatus(); };
    m_view.onColorPicked = [this](bool finished) {
        m_colorBar.Invalidate(FALSE);
        if (finished)
            PostMessage(WM_APP_RESTORE_TOOL); // 도구 안에서 도구를 지우지 않도록 나중에 전환
    };

    const DWORD statusStyle = WS_CHILD | WS_VISIBLE | SS_LEFT | SS_CENTERIMAGE | SS_NOPREFIX | SS_ENDELLIPSIS;
    m_statusPos.Create(L"", statusStyle, zero, this, IDC_STATUS_POS);
    m_statusSelection.Create(L"", statusStyle, zero, this, IDC_STATUS_SEL);
    m_statusSize.Create(L"", statusStyle, zero, this, IDC_STATUS_SIZE);
    m_statusZoom.Create(L"", (statusStyle & ~SS_LEFT) | SS_RIGHT, zero, this, IDC_STATUS_ZOOM);

    CreateUiFont(); // 만든 컨트롤에 글꼴 적용
    UpdateFontButton();
}

void CMyPaintDlg::Layout()
{
    if (!m_view.GetSafeHwnd())
        return;

    CRect rc;
    GetClientRect(&rc);
    const int pad = S(8);
    const int gap = S(4);
    const int buttonW = S(64);
    const int buttonH = S(28);

    // 도구 버튼: 2줄 × 5칸
    for (int i = 0; i < kToolCount; ++i)
    {
        const int row = i / 5;
        const int col = i % 5;
        m_toolButtons[i].MoveWindow(pad + col * (buttonW + gap), pad + row * (buttonH + gap), buttonW, buttonH);
    }

    // 두께 / 도형 / 글꼴
    int x = pad + 5 * (buttonW + gap) + S(12);
    const int labelW = S(36);
    const int comboW = S(150);
    m_widthLabel.MoveWindow(x, pad, labelW, buttonH);
    m_widthCombo.MoveWindow(x + labelW, pad + S(2), comboW, S(300));
    m_styleLabel.MoveWindow(x, pad + buttonH + gap, labelW, buttonH);
    m_styleCombo.MoveWindow(x + labelW, pad + buttonH + gap + S(2), comboW, S(300));
    x += labelW + comboW + S(10);
    const int fontW = S(150);
    m_fontButton.MoveWindow(x, pad, fontW, buttonH * 2 + gap);
    x += fontW + S(14);

    // 색
    m_colorBar.MoveWindow(x, pad, m_colorBar.IdealWidth(), m_colorBar.IdealHeight());

    // 캔버스와 상태 표시줄
    const int toolbarH = pad * 2 + buttonH * 2 + gap;
    const int statusH = S(24);
    m_view.MoveWindow(0, toolbarH, rc.Width(), (std::max)(0, rc.Height() - toolbarH - statusH));

    const int sy = rc.bottom - statusH;
    m_statusPos.MoveWindow(pad, sy, S(140), statusH);
    m_statusSelection.MoveWindow(pad + S(150), sy, S(200), statusH);
    m_statusSize.MoveWindow(pad + S(360), sy, S(200), statusH);
    m_statusZoom.MoveWindow(rc.right - pad - S(100), sy, S(100), statusH);

    Invalidate();
}

void CMyPaintDlg::OnSize(UINT nType, int cx, int cy)
{
    CDialog::OnSize(nType, cx, cy);
    if (nType != SIZE_MINIMIZED)
        Layout();
}

void CMyPaintDlg::OnGetMinMaxInfo(MINMAXINFO* lpMMI)
{
    lpMMI->ptMinTrackSize = CPoint(S(720), S(480));
    CDialog::OnGetMinMaxInfo(lpMMI);
}

LRESULT CMyPaintDlg::OnDpiChanged(WPARAM wParam, LPARAM lParam)
{
    m_dpi = LOWORD(wParam);
    CreateUiFont();
    m_colorBar.SetDpi(m_dpi);
    const RECT* suggested = reinterpret_cast<const RECT*>(lParam);
    SetWindowPos(nullptr, suggested->left, suggested->top, suggested->right - suggested->left,
                 suggested->bottom - suggested->top, SWP_NOZORDER | SWP_NOACTIVATE);
    Layout();
    return 0;
}

// ---------------------------------------------------------------------------
// 키보드 / 마우스 휠
// ---------------------------------------------------------------------------

BOOL CMyPaintDlg::PreTranslateMessage(MSG* pMsg)
{
    // 휠은 포커스가 콤보 상자에 있어도 마우스 아래의 캔버스로 보냅니다.
    if (pMsg->message == WM_MOUSEWHEEL && m_view.GetSafeHwnd() && pMsg->hwnd != m_view.m_hWnd)
    {
        CRect viewRect;
        m_view.GetWindowRect(&viewRect);
        const CPoint cursor(static_cast<short>(LOWORD(pMsg->lParam)), static_cast<short>(HIWORD(pMsg->lParam)));
        if (viewRect.PtInRect(cursor))
        {
            m_view.SendMessage(WM_MOUSEWHEEL, pMsg->wParam, pMsg->lParam);
            return TRUE;
        }
    }

    if (pMsg->message == WM_KEYDOWN)
    {
        const UINT key = static_cast<UINT>(pMsg->wParam);
        const bool ctrl = ::GetKeyState(VK_CONTROL) < 0;
        const bool shift = ::GetKeyState(VK_SHIFT) < 0;

        if (key == VK_ESCAPE)
        {
            m_view.CancelTool();
            return TRUE; // Esc로 대화 상자가 닫히지 않게
        }

        if (IsEditControlFocused())
        {
            // 텍스트 입력 중: Ctrl+Enter로 확정, 나머지 키는 편집 상자가 처리
            if (ctrl && key == VK_RETURN)
            {
                m_view.CommitTool();
                return TRUE;
            }
            return CDialog::PreTranslateMessage(pMsg);
        }

        UINT command = 0;
        if (ctrl)
        {
            switch (key)
            {
            case 'N': command = ID_FILE_NEW; break;
            case 'O': command = ID_FILE_OPEN; break;
            case 'S': command = shift ? ID_FILE_SAVE_AS : ID_FILE_SAVE; break;
            case 'Z': command = shift ? ID_EDIT_REDO : ID_EDIT_UNDO; break;
            case 'Y': command = ID_EDIT_REDO; break;
            case 'X': command = ID_EDIT_CUT; break;
            case 'C': command = ID_EDIT_COPY; break;
            case 'V': command = ID_EDIT_PASTE; break;
            case 'A': command = ID_EDIT_SELECT_ALL; break;
            case 'W': command = ID_IMAGE_RESIZE; break;
            case VK_OEM_PLUS:
            case VK_ADD: command = ID_VIEW_ZOOM_IN; break;
            case VK_OEM_MINUS:
            case VK_SUBTRACT: command = ID_VIEW_ZOOM_OUT; break;
            case '0':
            case VK_NUMPAD0: command = ID_VIEW_ZOOM_100; break;
            default: break;
            }
        }
        else if (key == VK_DELETE)
        {
            command = ID_EDIT_CLEAR;
        }
        else if (key == VK_RETURN)
        {
            return TRUE; // 기본 단추가 눌리지 않게
        }

        if (command)
        {
            SendMessage(WM_COMMAND, command);
            return TRUE;
        }
    }
    return CDialog::PreTranslateMessage(pMsg);
}

// ---------------------------------------------------------------------------
// 도구 / 설정
// ---------------------------------------------------------------------------

void CMyPaintDlg::SelectTool(ToolKind kind)
{
    if (kind == ToolKind::Picker && m_tool != ToolKind::Picker)
        m_previousTool = m_tool;
    m_tool = kind;
    for (int i = 0; i < kToolCount; ++i)
        m_toolButtons[i].SetCheck(i == static_cast<int>(kind) ? BST_CHECKED : BST_UNCHECKED);
    m_view.SetTool(kind);
    SyncToolOptions();
    UpdateStatus();
}

void CMyPaintDlg::SyncToolOptions()
{
    const int index = static_cast<int>(m_tool);
    m_settings.width = m_toolWidths[index];
    for (int i = 0; i < m_widthCombo.GetCount(); ++i)
    {
        if (static_cast<int>(m_widthCombo.GetItemData(i)) == m_settings.width)
        {
            m_widthCombo.SetCurSel(i);
            break;
        }
    }
    m_widthCombo.EnableWindow(UsesWidth(m_tool));
    m_widthLabel.EnableWindow(UsesWidth(m_tool));
    const bool shape = m_tool == ToolKind::Rectangle || m_tool == ToolKind::Ellipse;
    m_styleCombo.EnableWindow(shape);
    m_styleLabel.EnableWindow(shape);
    m_view.OnSettingsChanged();
}

void CMyPaintDlg::OnToolButton(UINT id)
{
    SelectTool(static_cast<ToolKind>(id - IDC_TOOL_FIRST));
    m_view.SetFocus();
}

LRESULT CMyPaintDlg::OnRestoreTool(WPARAM, LPARAM)
{
    if (m_tool == ToolKind::Picker)
        SelectTool(m_previousTool);
    return 0;
}

void CMyPaintDlg::OnWidthChange()
{
    const int index = m_widthCombo.GetCurSel();
    if (index == CB_ERR)
        return;
    m_settings.width = static_cast<int>(m_widthCombo.GetItemData(index));
    m_toolWidths[static_cast<int>(m_tool)] = m_settings.width;
    m_view.OnSettingsChanged();
    m_view.SetFocus();
}

void CMyPaintDlg::OnStyleChange()
{
    const int index = m_styleCombo.GetCurSel();
    if (index == CB_ERR)
        return;
    m_settings.shapeStyle = static_cast<ShapeStyle>(index);
    m_view.SetFocus();
}

void CMyPaintDlg::OnFontButton()
{
    // 글꼴 크기는 이미지 픽셀 기준(96 DPI)으로 저장하므로 대화 상자의 DPI와 맞춰 변환합니다.
    CClientDC screen(nullptr);
    const int screenDpi = screen.GetDeviceCaps(LOGPIXELSY);
    LOGFONTW lf = m_settings.font;
    lf.lfHeight = MulDiv(lf.lfHeight, screenDpi, 96);

    CFontDialog dlg(&lf, CF_SCREENFONTS | CF_INITTOLOGFONTSTRUCT, nullptr, this);
    if (dlg.DoModal() == IDOK)
    {
        dlg.GetCurrentFont(&lf);
        lf.lfHeight = MulDiv(lf.lfHeight, 96, screenDpi);
        if (lf.lfHeight == 0)
            lf.lfHeight = -1;
        m_settings.font = lf;
        UpdateFontButton();
        m_view.OnSettingsChanged();
    }
    if (m_tool != ToolKind::Text)
        SelectTool(ToolKind::Text);
    m_view.SetFocus();
}

void CMyPaintDlg::UpdateFontButton()
{
    const LOGFONTW& lf = m_settings.font;
    const int points = MulDiv(std::abs(lf.lfHeight), 72, 96);
    CString text;
    text.Format(L"텍스트 글꼴\n%s, %dpt", lf.lfFaceName, points);
    m_fontButton.ModifyStyle(0, BS_MULTILINE);
    m_fontButton.SetWindowText(text);
}

// ---------------------------------------------------------------------------
// 제목 / 상태 표시줄 / 메뉴 상태
// ---------------------------------------------------------------------------

CString CMyPaintDlg::DocumentName() const
{
    if (m_path.IsEmpty())
        return L"제목 없음";
    const int slash = (std::max)(m_path.ReverseFind(L'\\'), m_path.ReverseFind(L'/'));
    return m_path.Mid(slash + 1);
}

void CMyPaintDlg::UpdateTitle()
{
    CString title;
    title.Format(L"%s%s - MyPaint", m_canvas.IsModified() ? L"*" : L"", DocumentName().GetString());
    SetTextIfChanged(*this, title);
}

void CMyPaintDlg::UpdateStatus()
{
    if (!m_statusPos.GetSafeHwnd())
        return;

    CString text;
    CPoint pt;
    if (m_view.CursorPosition(pt))
        text.Format(L"%d, %d px", pt.x, pt.y);
    SetTextIfChanged(m_statusPos, text);

    text.Empty();
    if (CSelectTool* select = m_view.SelectTool(); select && select->HasSelection())
    {
        const CRect sel = select->SelectionRect();
        text.Format(L"선택: %d × %d px", sel.Width(), sel.Height());
    }
    SetTextIfChanged(m_statusSelection, text);

    text.Format(L"이미지: %d × %d px", m_canvas.Width(), m_canvas.Height());
    SetTextIfChanged(m_statusSize, text);

    text.Format(L"%d%%", static_cast<int>(std::lround(m_view.Zoom() * 100)));
    SetTextIfChanged(m_statusZoom, text);
}

void CMyPaintDlg::OnInitMenuPopup(CMenu* pPopupMenu, UINT nIndex, BOOL bSysMenu)
{
    CDialog::OnInitMenuPopup(pPopupMenu, nIndex, bSysMenu);
    if (bSysMenu)
        return;

    // 대화 상자에는 메뉴 상태 자동 갱신(ON_UPDATE_COMMAND_UI)이 없어서 직접 맞춥니다.
    auto enable = [pPopupMenu](UINT id, bool on) {
        pPopupMenu->EnableMenuItem(id, MF_BYCOMMAND | (on ? MF_ENABLED : MF_GRAYED));
    };
    const bool selection = m_view.HasSelection();
    enable(ID_EDIT_UNDO, m_canvas.CanUndo());
    enable(ID_EDIT_REDO, m_canvas.CanRedo());
    enable(ID_EDIT_CUT, selection);
    enable(ID_EDIT_COPY, selection);
    enable(ID_EDIT_CLEAR, selection);
    enable(ID_EDIT_PASTE, ::IsClipboardFormatAvailable(CF_BITMAP) || ::IsClipboardFormatAvailable(CF_DIB));
}

// ---------------------------------------------------------------------------
// 파일
// ---------------------------------------------------------------------------

bool CMyPaintDlg::ConfirmDiscard()
{
    m_view.CommitTool();
    if (!m_canvas.IsModified())
        return true;

    CString message;
    message.Format(L"변경 내용을 '%s'에 저장하시겠습니까?", DocumentName().GetString());
    switch (MessageBox(message, L"MyPaint", MB_YESNOCANCEL | MB_ICONQUESTION))
    {
    case IDYES:
        return Save();
    case IDNO:
        return true;
    default:
        return false;
    }
}

bool CMyPaintDlg::OpenFile(const CString& path)
{
    m_view.CommitTool();
    CString error;
    if (!m_canvas.Load(path, error))
    {
        MessageBox(error + L"\n\n" + path, L"MyPaint", MB_ICONERROR);
        return false;
    }
    m_path = path;
    m_view.OnImageReplaced();
    UpdateTitle();
    UpdateStatus();
    return true;
}

bool CMyPaintDlg::Save()
{
    m_view.CommitTool();
    if (m_path.IsEmpty())
        return SaveAs();

    CString error;
    if (!m_canvas.Save(m_path, error))
    {
        MessageBox(error, L"MyPaint", MB_ICONERROR);
        return false;
    }
    UpdateTitle();
    return true;
}

bool CMyPaintDlg::SaveAs()
{
    m_view.CommitTool();

    CString name = DocumentName();
    if (m_path.IsEmpty())
        name += L".png";
    CFileDialog dlg(FALSE, L"png", name, OFN_OVERWRITEPROMPT | OFN_HIDEREADONLY | OFN_PATHMUSTEXIST, kSaveFilter,
                    this);

    // 지금 파일 형식에 맞는 필터를 먼저 보여 줍니다.
    if (const wchar_t* mime = MimeTypeForPath(m_path))
    {
        for (int i = 0; i < static_cast<int>(std::size(kSaveExtensions)); ++i)
        {
            if (wcscmp(MimeTypeForPath(kSaveExtensions[i]), mime) == 0)
            {
                dlg.m_ofn.nFilterIndex = static_cast<DWORD>(i + 1);
                break;
            }
        }
    }

    if (dlg.DoModal() != IDOK)
        return false;

    CString path = dlg.GetPathName();
    if (!MimeTypeForPath(path))
    {
        // 확장자가 없거나 지원하지 않으면 고른 필터의 확장자를 붙입니다.
        const DWORD filter = dlg.m_ofn.nFilterIndex;
        path += kSaveExtensions[(filter >= 1 && filter <= std::size(kSaveExtensions)) ? filter - 1 : 0];
        if (::GetFileAttributesW(path) != INVALID_FILE_ATTRIBUTES)
        {
            CString message;
            message.Format(L"'%s' 파일이 이미 있습니다.\n바꾸시겠습니까?", path.GetString());
            if (MessageBox(message, L"MyPaint", MB_YESNO | MB_ICONWARNING) != IDYES)
                return false;
        }
    }

    CString error;
    if (!m_canvas.Save(path, error))
    {
        MessageBox(error, L"MyPaint", MB_ICONERROR);
        return false;
    }
    m_path = path;
    UpdateTitle();
    return true;
}

void CMyPaintDlg::OnFileNew()
{
    if (!ConfirmDiscard())
        return;
    m_canvas.New(kDefaultWidth, kDefaultHeight, RGB(255, 255, 255));
    m_path.Empty();
    m_view.OnImageReplaced();
    UpdateTitle();
    UpdateStatus();
}

void CMyPaintDlg::OnFileOpen()
{
    if (!ConfirmDiscard())
        return;
    CFileDialog dlg(TRUE, nullptr, nullptr, OFN_FILEMUSTEXIST | OFN_HIDEREADONLY, kOpenFilter, this);
    if (dlg.DoModal() == IDOK)
        OpenFile(dlg.GetPathName());
}

void CMyPaintDlg::OnFileSave()
{
    Save();
}

void CMyPaintDlg::OnFileSaveAs()
{
    SaveAs();
}

void CMyPaintDlg::OnAppExit()
{
    PostMessage(WM_CLOSE);
}

void CMyPaintDlg::OnCancel()
{
    if (ConfirmDiscard())
        CDialog::OnCancel();
}

void CMyPaintDlg::OnDropFiles(HDROP hDropInfo)
{
    wchar_t path[MAX_PATH]{};
    const bool ok = ::DragQueryFileW(hDropInfo, 0, path, MAX_PATH) > 0;
    ::DragFinish(hDropInfo);
    if (ok && ConfirmDiscard())
        OpenFile(path);
}

// ---------------------------------------------------------------------------
// 편집
// ---------------------------------------------------------------------------

void CMyPaintDlg::OnEditUndo()
{
    m_view.CommitTool();
    if (m_canvas.Undo())
    {
        m_view.OnImageChanged();
        UpdateTitle();
    }
}

void CMyPaintDlg::OnEditRedo()
{
    m_view.CommitTool();
    if (m_canvas.Redo())
    {
        m_view.OnImageChanged();
        UpdateTitle();
    }
}

void CMyPaintDlg::OnEditCut()
{
    if (CSelectTool* select = m_view.SelectTool())
        select->Cut();
    UpdateStatus();
}

void CMyPaintDlg::OnEditCopy()
{
    if (CSelectTool* select = m_view.SelectTool())
        select->Copy();
}

void CMyPaintDlg::OnEditPaste()
{
    if (m_tool != ToolKind::Select)
        SelectTool(ToolKind::Select);
    if (CSelectTool* select = m_view.SelectTool())
    {
        if (!select->Paste())
            ::MessageBeep(MB_ICONWARNING);
    }
    UpdateTitle();
    UpdateStatus();
}

void CMyPaintDlg::OnEditClear()
{
    if (CSelectTool* select = m_view.SelectTool())
        select->DeleteSelection();
    UpdateStatus();
}

void CMyPaintDlg::OnEditSelectAll()
{
    if (m_tool != ToolKind::Select)
        SelectTool(ToolKind::Select);
    if (CSelectTool* select = m_view.SelectTool())
        select->SelectAll();
}

// ---------------------------------------------------------------------------
// 이미지
// ---------------------------------------------------------------------------

template <class Operation>
void CMyPaintDlg::ApplyImageOperation(Operation operation)
{
    m_view.CommitTool();
    m_canvas.PushUndo();
    if (operation())
    {
        m_canvas.SetModified(true);
        m_view.OnImageChanged();
        UpdateTitle();
        UpdateStatus();
    }
    else
    {
        m_canvas.RevertLastUndo();
        MessageBox(L"작업을 완료하지 못했습니다. 메모리가 부족할 수 있습니다.", L"MyPaint", MB_ICONERROR);
    }
}

void CMyPaintDlg::OnImageResize()
{
    m_view.CommitTool();
    CSizeDlg dlg(L"이미지 크기 조정", m_canvas.Width(), m_canvas.Height(), true, this);
    if (dlg.DoModal() != IDOK || (dlg.Width() == m_canvas.Width() && dlg.Height() == m_canvas.Height()))
        return;
    ApplyImageOperation([&]() { return m_canvas.ResizeImage(dlg.Width(), dlg.Height()); });
}

void CMyPaintDlg::OnImageCanvasSize()
{
    m_view.CommitTool();
    CSizeDlg dlg(L"캔버스 크기", m_canvas.Width(), m_canvas.Height(), false, this);
    if (dlg.DoModal() != IDOK || (dlg.Width() == m_canvas.Width() && dlg.Height() == m_canvas.Height()))
        return;
    ApplyImageOperation(
        [&]() { return m_canvas.ResizeCanvas(dlg.Width(), dlg.Height(), m_settings.secondary); });
}

void CMyPaintDlg::OnImageRotateRight()
{
    ApplyImageOperation([this]() { return m_canvas.Rotate90(true); });
}

void CMyPaintDlg::OnImageRotateLeft()
{
    ApplyImageOperation([this]() { return m_canvas.Rotate90(false); });
}

void CMyPaintDlg::OnImageRotate180()
{
    ApplyImageOperation([this]() {
        m_canvas.FlipHorizontal();
        m_canvas.FlipVertical();
        return true;
    });
}

void CMyPaintDlg::OnImageFlipH()
{
    ApplyImageOperation([this]() {
        m_canvas.FlipHorizontal();
        return true;
    });
}

void CMyPaintDlg::OnImageFlipV()
{
    ApplyImageOperation([this]() {
        m_canvas.FlipVertical();
        return true;
    });
}

void CMyPaintDlg::OnImageClear()
{
    ApplyImageOperation([this]() {
        m_canvas.Image().Fill(m_settings.secondary);
        return true;
    });
}

// ---------------------------------------------------------------------------
// 보기
// ---------------------------------------------------------------------------

void CMyPaintDlg::OnViewZoomIn()
{
    m_view.ZoomIn();
}

void CMyPaintDlg::OnViewZoomOut()
{
    m_view.ZoomOut();
}

void CMyPaintDlg::OnViewZoom100()
{
    m_view.ZoomActual();
}

void CMyPaintDlg::OnViewZoomFit()
{
    m_view.ZoomToFit();
}
