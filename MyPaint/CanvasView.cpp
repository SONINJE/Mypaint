/**
 * @file CanvasView.cpp
 * @brief CCanvasView 구현
 */
#include "pch.h"
#include "Resource.h"
#include "CanvasView.h"

namespace
{
constexpr int kMargin = 16;                           ///< 그림 둘레 여백 (화면 픽셀)
constexpr int kLineScroll = 40;                       ///< 화살표·휠 한 칸 스크롤 양
constexpr COLORREF kBackground = RGB(204, 212, 226);  ///< 작업 영역 배경
constexpr COLORREF kBorder = RGB(150, 158, 172);      ///< 그림 테두리
constexpr double kZoomLevels[] = {0.05, 0.1, 0.125, 0.25, 1.0 / 3, 0.5, 2.0 / 3, 1.0, 1.5, 2.0, 3.0,
                                  4.0,  5.0, 6.0,   8.0,  12.0,    16.0, 24.0,    32.0};
constexpr double kMinZoom = kZoomLevels[0];
constexpr double kMaxZoom = kZoomLevels[std::size(kZoomLevels) - 1];
} // namespace

BEGIN_MESSAGE_MAP(CCanvasView, CWnd)
    ON_WM_PAINT()
    ON_WM_ERASEBKGND()
    ON_WM_SIZE()
    ON_WM_HSCROLL()
    ON_WM_VSCROLL()
    ON_WM_MOUSEWHEEL()
    ON_WM_LBUTTONDOWN()
    ON_WM_LBUTTONUP()
    ON_WM_RBUTTONDOWN()
    ON_WM_RBUTTONUP()
    ON_WM_MOUSEMOVE()
    ON_WM_MOUSELEAVE()
    ON_WM_SETCURSOR()
    ON_WM_CAPTURECHANGED()
    ON_WM_CTLCOLOR()
    ON_EN_CHANGE(IDC_TEXT_EDIT, &CCanvasView::OnTextEditChange)
END_MESSAGE_MAP()

BOOL CCanvasView::Create(CWnd* parent, UINT id)
{
    LPCTSTR cls = AfxRegisterWndClass(0, ::LoadCursor(nullptr, IDC_ARROW), nullptr, nullptr);
    return CWnd::CreateEx(0, cls, nullptr, WS_CHILD | WS_VISIBLE | WS_HSCROLL | WS_VSCROLL | WS_CLIPCHILDREN,
                          CRect(0, 0, 10, 10), parent, id);
}

void CCanvasView::Attach(CCanvas* canvas, ToolSettings* settings)
{
    m_canvas = canvas;
    m_settings = settings;
    m_tool = CreateTool(m_toolKind, *this);
}

// ---------------------------------------------------------------------------
// 도구
// ---------------------------------------------------------------------------

void CCanvasView::SetTool(ToolKind kind)
{
    CommitTool();
    m_tool = CreateTool(kind, *this);
    m_toolKind = kind;
    Redraw();
    NotifyStatus();
}

void CCanvasView::CommitTool()
{
    FinishDragSilently(false);
    if (m_tool)
        m_tool->Commit();
    NotifyStatus();
}

void CCanvasView::CancelTool()
{
    FinishDragSilently(true);
    if (m_tool)
        m_tool->Cancel();
    NotifyStatus();
}

void CCanvasView::FinishDragSilently(bool cancel)
{
    if (m_button == Button::None)
        return;
    m_button = Button::None; // OnCaptureChanged가 다시 처리하지 않도록 먼저 지움
    ::ReleaseCapture();
    if (m_tool)
    {
        if (cancel)
            m_tool->Cancel();
        else
            m_tool->OnMouseUp(m_lastImagePt, CurrentKeyFlags());
    }
}

CSelectTool* CCanvasView::SelectTool()
{
    return m_toolKind == ToolKind::Select ? static_cast<CSelectTool*>(m_tool.get()) : nullptr;
}

bool CCanvasView::HasSelection()
{
    CSelectTool* tool = SelectTool();
    return tool && tool->HasSelection();
}

void CCanvasView::OnSettingsChanged()
{
    if (m_tool)
        m_tool->OnSettingsChanged();
}

// ---------------------------------------------------------------------------
// 문서 변경
// ---------------------------------------------------------------------------

void CCanvasView::OnImageReplaced()
{
    m_scroll = CPoint(0, 0);
    m_zoom = 1.0;
    CRect rc;
    GetClientRect(&rc);
    if (m_canvas->Width() + 2 * kMargin > rc.Width() || m_canvas->Height() + 2 * kMargin > rc.Height())
        ZoomToFit();
    else
        OnImageChanged();
}

void CCanvasView::OnImageChanged()
{
    UpdateScrollBars();
    if (m_tool)
        m_tool->OnViewChanged();
    Redraw();
    NotifyStatus();
}

// ---------------------------------------------------------------------------
// 좌표 변환
// ---------------------------------------------------------------------------

CSize CCanvasView::ContentSize() const
{
    if (!m_canvas)
        return CSize(0, 0);
    return CSize((std::max)(1, static_cast<int>(std::floor(m_canvas->Width() * m_zoom))),
                 (std::max)(1, static_cast<int>(std::floor(m_canvas->Height() * m_zoom))));
}

CPoint CCanvasView::Origin() const
{
    CRect rc;
    GetClientRect(&rc);
    const CSize content = ContentSize();
    // 그림이 창보다 작으면 가운데에 놓습니다.
    const int x = (content.cx + 2 * kMargin <= rc.Width()) ? (rc.Width() - content.cx) / 2 : kMargin - m_scroll.x;
    const int y = (content.cy + 2 * kMargin <= rc.Height()) ? (rc.Height() - content.cy) / 2 : kMargin - m_scroll.y;
    return CPoint(x, y);
}

CPoint CCanvasView::ClientToImage(CPoint pt) const
{
    const CPoint o = Origin();
    return CPoint(static_cast<int>(std::floor((pt.x - o.x) / m_zoom)),
                  static_cast<int>(std::floor((pt.y - o.y) / m_zoom)));
}

CPoint CCanvasView::ImageToClient(CPoint pt) const
{
    const CPoint o = Origin();
    return CPoint(o.x + static_cast<int>(std::floor(pt.x * m_zoom)), o.y + static_cast<int>(std::floor(pt.y * m_zoom)));
}

CRect CCanvasView::ImageToClient(const CRect& rect) const
{
    return CRect(ImageToClient(rect.TopLeft()), ImageToClient(rect.BottomRight()));
}

CPoint CCanvasView::VisibleImageOrigin() const
{
    const CPoint pt = ClientToImage(CPoint(0, 0));
    return CPoint((std::clamp<int>)(pt.x, 0, (std::max)(0, m_canvas->Width() - 1)),
                  (std::clamp<int>)(pt.y, 0, (std::max)(0, m_canvas->Height() - 1)));
}

bool CCanvasView::CursorPosition(CPoint& pt) const
{
    if (!m_cursorInside)
        return false;
    pt = m_cursorPt;
    return true;
}

// ---------------------------------------------------------------------------
// 도구 지원
// ---------------------------------------------------------------------------

void CCanvasView::BeginEdit()
{
    m_canvas->PushUndo();
}

void CCanvasView::EndEdit()
{
    m_canvas->SetModified(true);
    Redraw();
    if (onModified)
        onModified();
}

void CCanvasView::AbortEdit()
{
    m_canvas->RevertLastUndo();
    Redraw();
    if (onModified)
        onModified();
}

void CCanvasView::Redraw()
{
    if (GetSafeHwnd())
        Invalidate(FALSE);
}

void CCanvasView::NotifyColorPicked(bool finished)
{
    if (onColorPicked)
        onColorPicked(finished);
}

void CCanvasView::NotifyStatus()
{
    if (onStatusChanged)
        onStatusChanged();
}

// ---------------------------------------------------------------------------
// 그리기
// ---------------------------------------------------------------------------

BOOL CCanvasView::OnEraseBkgnd(CDC*)
{
    return TRUE; // OnPaint에서 더블 버퍼로 전부 그림
}

void CCanvasView::OnPaint()
{
    CPaintDC paintDC(this);
    CRect rc;
    GetClientRect(&rc);
    if (rc.IsRectEmpty())
        return;

    CDC mem;
    mem.CreateCompatibleDC(&paintDC);
    CBitmap buffer;
    buffer.CreateCompatibleBitmap(&paintDC, rc.Width(), rc.Height());
    CBitmap* oldBitmap = mem.SelectObject(&buffer);
    mem.FillSolidRect(rc, kBackground);

    if (m_canvas && m_canvas->Image().IsValid())
    {
        const CDib& image = m_canvas->Image();
        const CPoint o = Origin();
        const CRect dest = ImageToClient(CRect(0, 0, image.Width(), image.Height()));

        CRect border = dest;
        border.InflateRect(1, 1);
        mem.Draw3dRect(border, kBorder, kBorder);

        // 화면에 보이는 부분만 원본에서 잘라 확대/축소해 그립니다.
        CRect visible;
        if (visible.IntersectRect(dest, rc))
        {
            const int sx0 = (std::max)(0, static_cast<int>(std::floor((visible.left - o.x) / m_zoom)));
            const int sy0 = (std::max)(0, static_cast<int>(std::floor((visible.top - o.y) / m_zoom)));
            const int sx1 = (std::min)(image.Width(), static_cast<int>(std::ceil((visible.right - o.x) / m_zoom)));
            const int sy1 = (std::min)(image.Height(), static_cast<int>(std::ceil((visible.bottom - o.y) / m_zoom)));
            if (sx1 > sx0 && sy1 > sy0)
            {
                const CRect d = ImageToClient(CRect(sx0, sy0, sx1, sy1));
                mem.SetStretchBltMode(m_zoom < 1.0 ? HALFTONE : COLORONCOLOR);
                ::SetBrushOrgEx(mem.GetSafeHdc(), 0, 0, nullptr);
                ::StretchBlt(mem.GetSafeHdc(), d.left, d.top, d.Width(), d.Height(), image.GetDC(), sx0, sy0,
                             sx1 - sx0, sy1 - sy0, SRCCOPY);
            }
        }

        if (m_tool)
            m_tool->DrawOverlay(mem);
    }

    paintDC.BitBlt(0, 0, rc.Width(), rc.Height(), &mem, 0, 0, SRCCOPY);
    mem.SelectObject(oldBitmap);
}

// ---------------------------------------------------------------------------
// 스크롤 / 확대
// ---------------------------------------------------------------------------

void CCanvasView::UpdateScrollBars()
{
    if (m_updatingScroll || !GetSafeHwnd() || !m_canvas)
        return;
    m_updatingScroll = true;

    // 스크롤 막대가 나타나거나 사라지면 클라이언트 크기가 바뀌므로 두 번 맞춥니다.
    for (int pass = 0; pass < 2; ++pass)
    {
        CRect rc;
        GetClientRect(&rc);
        const CSize content = ContentSize();
        const int totalW = content.cx + 2 * kMargin;
        const int totalH = content.cy + 2 * kMargin;
        m_scroll.x = (std::clamp<int>)(m_scroll.x, 0, (std::max)(0, totalW - rc.Width()));
        m_scroll.y = (std::clamp<int>)(m_scroll.y, 0, (std::max)(0, totalH - rc.Height()));

        SCROLLINFO si{};
        si.cbSize = sizeof(si);
        si.fMask = SIF_RANGE | SIF_PAGE | SIF_POS;
        si.nMin = 0;
        si.nMax = totalW - 1;
        si.nPage = static_cast<UINT>((std::max)(0, rc.Width()));
        si.nPos = m_scroll.x;
        SetScrollInfo(SB_HORZ, &si, TRUE);

        si.nMax = totalH - 1;
        si.nPage = static_cast<UINT>((std::max)(0, rc.Height()));
        si.nPos = m_scroll.y;
        SetScrollInfo(SB_VERT, &si, TRUE);
    }
    m_updatingScroll = false;
}

void CCanvasView::ScrollTo(int x, int y)
{
    CRect rc;
    GetClientRect(&rc);
    const CSize content = ContentSize();
    x = (std::clamp<int>)(x, 0, (std::max<int>)(0, content.cx + 2 * kMargin - rc.Width()));
    y = (std::clamp<int>)(y, 0, (std::max<int>)(0, content.cy + 2 * kMargin - rc.Height()));
    if (x == m_scroll.x && y == m_scroll.y)
        return;

    m_scroll = CPoint(x, y);
    SetScrollPos(SB_HORZ, x);
    SetScrollPos(SB_VERT, y);
    if (m_tool)
        m_tool->OnViewChanged();
    Redraw();
}

void CCanvasView::HandleScroll(int bar, UINT code)
{
    SCROLLINFO si{};
    si.cbSize = sizeof(si);
    si.fMask = SIF_ALL;
    GetScrollInfo(bar, &si);

    int pos = (bar == SB_HORZ) ? m_scroll.x : m_scroll.y;
    switch (code)
    {
    case SB_LINEUP:
        pos -= kLineScroll;
        break;
    case SB_LINEDOWN:
        pos += kLineScroll;
        break;
    case SB_PAGEUP:
        pos -= static_cast<int>(si.nPage);
        break;
    case SB_PAGEDOWN:
        pos += static_cast<int>(si.nPage);
        break;
    case SB_THUMBTRACK:
    case SB_THUMBPOSITION:
        pos = si.nTrackPos;
        break;
    case SB_TOP:
        pos = 0;
        break;
    case SB_BOTTOM:
        pos = si.nMax;
        break;
    default:
        return;
    }

    if (bar == SB_HORZ)
        ScrollTo(pos, m_scroll.y);
    else
        ScrollTo(m_scroll.x, pos);
}

void CCanvasView::OnHScroll(UINT nSBCode, UINT, CScrollBar*)
{
    HandleScroll(SB_HORZ, nSBCode);
}

void CCanvasView::OnVScroll(UINT nSBCode, UINT, CScrollBar*)
{
    HandleScroll(SB_VERT, nSBCode);
}

void CCanvasView::OnSize(UINT nType, int cx, int cy)
{
    CWnd::OnSize(nType, cx, cy);
    UpdateScrollBars();
    if (m_tool)
        m_tool->OnViewChanged();
    Redraw();
}

BOOL CCanvasView::OnMouseWheel(UINT nFlags, short zDelta, CPoint pt)
{
    ScreenToClient(&pt);
    if (nFlags & MK_CONTROL)
    {
        // 정밀 터치패드는 작은 값을 여러 번 보내므로 한 칸(120)이 모일 때마다 확대합니다.
        m_wheelZoomAccum += zDelta;
        while (std::abs(m_wheelZoomAccum) >= WHEEL_DELTA)
        {
            const bool zoomIn = m_wheelZoomAccum > 0;
            m_wheelZoomAccum += zoomIn ? -WHEEL_DELTA : WHEEL_DELTA;
            ZoomAround(NextZoomLevel(zoomIn), pt);
        }
        return TRUE;
    }

    const int amount = -MulDiv(zDelta, kLineScroll * 3, WHEEL_DELTA);
    if (nFlags & MK_SHIFT)
        ScrollTo(m_scroll.x + amount, m_scroll.y);
    else
        ScrollTo(m_scroll.x, m_scroll.y + amount);
    return TRUE;
}

double CCanvasView::NextZoomLevel(bool zoomIn) const
{
    if (zoomIn)
    {
        for (const double level : kZoomLevels)
            if (level > m_zoom * 1.001)
                return level;
        return kMaxZoom;
    }
    for (auto it = std::rbegin(kZoomLevels); it != std::rend(kZoomLevels); ++it)
        if (*it < m_zoom * 0.999)
            return *it;
    return kMinZoom;
}

void CCanvasView::ZoomAround(double zoom, CPoint anchor)
{
    zoom = (std::clamp<double>)(zoom, kMinZoom, kMaxZoom);
    if (std::abs(zoom - m_zoom) < 1e-9)
        return;

    // 확대 전후에 anchor 아래 이미지 지점이 그대로 있도록 스크롤을 맞춥니다.
    const CPoint o = Origin();
    const double ix = (anchor.x - o.x) / m_zoom;
    const double iy = (anchor.y - o.y) / m_zoom;
    m_zoom = zoom;
    m_scroll.x = kMargin - (anchor.x - static_cast<int>(std::lround(ix * m_zoom)));
    m_scroll.y = kMargin - (anchor.y - static_cast<int>(std::lround(iy * m_zoom)));
    OnImageChanged();
}

void CCanvasView::ZoomIn()
{
    CRect rc;
    GetClientRect(&rc);
    ZoomAround(NextZoomLevel(true), rc.CenterPoint());
}

void CCanvasView::ZoomOut()
{
    CRect rc;
    GetClientRect(&rc);
    ZoomAround(NextZoomLevel(false), rc.CenterPoint());
}

void CCanvasView::ZoomActual()
{
    CRect rc;
    GetClientRect(&rc);
    ZoomAround(1.0, rc.CenterPoint());
}

void CCanvasView::ZoomToFit()
{
    CRect rc;
    GetClientRect(&rc);
    const double zx = double(rc.Width() - 2 * kMargin) / m_canvas->Width();
    const double zy = double(rc.Height() - 2 * kMargin) / m_canvas->Height();
    m_zoom = (std::clamp<double>)((std::min)({zx, zy, 1.0}), kMinZoom, kMaxZoom);
    m_scroll = CPoint(0, 0);
    OnImageChanged();
}

// ---------------------------------------------------------------------------
// 마우스
// ---------------------------------------------------------------------------

UINT CCanvasView::CurrentKeyFlags()
{
    UINT flags = 0;
    if (::GetKeyState(VK_SHIFT) < 0)
        flags |= MK_SHIFT;
    if (::GetKeyState(VK_CONTROL) < 0)
        flags |= MK_CONTROL;
    return flags;
}

void CCanvasView::BeginDrag(Button button, UINT flags, CPoint point)
{
    if (!m_tool || !m_canvas || m_button != Button::None)
        return;
    SetFocus();
    m_button = button;
    SetCapture();
    m_lastImagePt = ClientToImage(point);
    m_tool->OnMouseDown(m_lastImagePt, button == Button::Right, flags);
    NotifyStatus();
}

void CCanvasView::EndDrag(Button button, UINT flags, CPoint point)
{
    if (m_button != button)
        return;
    m_button = Button::None;
    ::ReleaseCapture();
    m_lastImagePt = ClientToImage(point);
    if (m_tool)
        m_tool->OnMouseUp(m_lastImagePt, flags);
    NotifyStatus();
}

void CCanvasView::OnLButtonDown(UINT nFlags, CPoint point)
{
    BeginDrag(Button::Left, nFlags, point);
}

void CCanvasView::OnLButtonUp(UINT nFlags, CPoint point)
{
    EndDrag(Button::Left, nFlags, point);
}

void CCanvasView::OnRButtonDown(UINT nFlags, CPoint point)
{
    BeginDrag(Button::Right, nFlags, point);
}

void CCanvasView::OnRButtonUp(UINT nFlags, CPoint point)
{
    EndDrag(Button::Right, nFlags, point);
}

void CCanvasView::OnMouseMove(UINT nFlags, CPoint point)
{
    if (!m_trackingLeave)
    {
        TRACKMOUSEEVENT tme{};
        tme.cbSize = sizeof(tme);
        tme.dwFlags = TME_LEAVE;
        tme.hwndTrack = m_hWnd;
        m_trackingLeave = ::TrackMouseEvent(&tme) != FALSE;
    }

    if (!m_canvas)
        return;
    const CPoint img = ClientToImage(point);
    m_cursorPt = img;
    m_cursorInside = m_canvas->Contains(img);

    const bool dragging = m_button != Button::None;
    if (m_tool)
        m_tool->OnMouseMove(img, nFlags, dragging);
    if (dragging)
        m_lastImagePt = img;
    NotifyStatus();
}

void CCanvasView::OnMouseLeave()
{
    m_trackingLeave = false;
    m_cursorInside = false;
    NotifyStatus();
}

void CCanvasView::OnCaptureChanged(CWnd* pWnd)
{
    // 끄는 도중 다른 창이 캡처를 가져가면(Alt+Tab 등) 그 자리에서 작업을 확정합니다.
    if (m_button != Button::None)
    {
        m_button = Button::None;
        if (m_tool)
            m_tool->OnMouseUp(m_lastImagePt, CurrentKeyFlags());
        NotifyStatus();
    }
    CWnd::OnCaptureChanged(pWnd);
}

BOOL CCanvasView::OnSetCursor(CWnd* pWnd, UINT nHitTest, UINT message)
{
    if (pWnd == this && nHitTest == HTCLIENT && m_tool && m_canvas)
    {
        CPoint pt;
        ::GetCursorPos(&pt);
        ScreenToClient(&pt);
        const CRect imageRect = ImageToClient(CRect(0, 0, m_canvas->Width(), m_canvas->Height()));
        if (imageRect.PtInRect(pt) || m_button != Button::None)
            ::SetCursor(::LoadCursor(nullptr, m_tool->CursorId(ClientToImage(pt))));
        else
            ::SetCursor(::LoadCursor(nullptr, IDC_ARROW));
        return TRUE;
    }
    return CWnd::OnSetCursor(pWnd, nHitTest, message);
}

// ---------------------------------------------------------------------------
// 텍스트 편집 상자
// ---------------------------------------------------------------------------

HBRUSH CCanvasView::OnCtlColor(CDC* pDC, CWnd* pWnd, UINT nCtlColor)
{
    if (m_toolKind == ToolKind::Text && m_tool)
    {
        if (HBRUSH brush = static_cast<CTextTool*>(m_tool.get())->OnCtlColor(pDC, pWnd))
            return brush;
    }
    return CWnd::OnCtlColor(pDC, pWnd, nCtlColor);
}

void CCanvasView::OnTextEditChange()
{
    if (m_toolKind == ToolKind::Text && m_tool)
        static_cast<CTextTool*>(m_tool.get())->OnTextChanged();
}
