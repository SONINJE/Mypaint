/**
 * @file Tools.cpp
 * @brief 그리기 도구 구현
 */
#include "pch.h"
#include "Resource.h"
#include "Tools.h"
#include "CanvasView.h"

namespace
{
Gdiplus::Color ToGdiplus(COLORREF c)
{
    return Gdiplus::Color(255, GetRValue(c), GetGValue(c), GetBValue(c));
}

/** @brief 캔버스 픽셀 버퍼에 직접 그리는 GDI+ Graphics (복사 없음) */
class CanvasGraphics
{
public:
    explicit CanvasGraphics(CDib& dib)
        : m_bitmap(dib.Width(), dib.Height(), dib.Width() * 4, PixelFormat32bppRGB,
                   reinterpret_cast<BYTE*>(dib.Bits())),
          m_graphics(&m_bitmap)
    {
        // 픽셀 (x, y)의 중심이 (x + 0.5, y + 0.5)가 되도록
        m_graphics.SetPixelOffsetMode(Gdiplus::PixelOffsetModeHalf);
    }
    Gdiplus::Graphics& G() { return m_graphics; }

private:
    Gdiplus::Bitmap m_bitmap;
    Gdiplus::Graphics m_graphics;
};

/** @brief 흑백 점선 사각형 (화면 좌표) */
void DrawDashedRect(CDC& dc, const CRect& rect)
{
    CPen pen(PS_DOT, 1, RGB(0, 0, 0));
    CPen* oldPen = dc.SelectObject(&pen);
    CGdiObject* oldBrush = dc.SelectStockObject(NULL_BRUSH);
    const int oldMode = dc.SetBkMode(OPAQUE);
    const COLORREF oldBk = dc.SetBkColor(RGB(255, 255, 255));
    dc.Rectangle(rect);
    dc.SetBkColor(oldBk);
    dc.SetBkMode(oldMode);
    dc.SelectObject(oldBrush);
    dc.SelectObject(oldPen);
}

// ---------------------------------------------------------------------------
// 클립보드
// ---------------------------------------------------------------------------

bool CopyDibToClipboard(HWND owner, const CDib& dib)
{
    const int w = dib.Width();
    const int h = dib.Height();
    const size_t pixelBytes = size_t(w) * h * 4;
    HGLOBAL mem = ::GlobalAlloc(GMEM_MOVEABLE, sizeof(BITMAPINFOHEADER) + pixelBytes);
    if (!mem)
        return false;

    auto* data = static_cast<BYTE*>(::GlobalLock(mem));
    if (!data)
    {
        ::GlobalFree(mem);
        return false;
    }
    BITMAPINFOHEADER header{};
    header.biSize = sizeof(header);
    header.biWidth = w;
    header.biHeight = h; // bottom-up
    header.biPlanes = 1;
    header.biBitCount = 32;
    header.biCompression = BI_RGB;
    header.biSizeImage = static_cast<DWORD>(pixelBytes);
    memcpy(data, &header, sizeof(header));

    auto* dst = reinterpret_cast<uint32_t*>(data + sizeof(header));
    const uint32_t* src = dib.Bits();
    for (int y = 0; y < h; ++y)
    {
        const uint32_t* row = src + size_t(y) * w;
        uint32_t* out = dst + size_t(h - 1 - y) * w;
        for (int x = 0; x < w; ++x)
            out[x] = row[x] | 0xFF000000u;
    }
    ::GlobalUnlock(mem);

    if (!::OpenClipboard(owner))
    {
        ::GlobalFree(mem);
        return false;
    }
    ::EmptyClipboard();
    const bool ok = ::SetClipboardData(CF_DIB, mem) != nullptr;
    if (!ok)
        ::GlobalFree(mem);
    ::CloseClipboard();
    return ok;
}

bool ReadClipboardImage(HWND owner, CDib& out)
{
    if (!::IsClipboardFormatAvailable(CF_BITMAP) && !::IsClipboardFormatAvailable(CF_DIB))
        return false;
    if (!::OpenClipboard(owner))
        return false;

    bool ok = false;
    // CF_DIB만 있어도 시스템이 CF_BITMAP을 만들어 주므로, 형식 해석은 GetDIBits에 맡깁니다.
    auto bitmap = static_cast<HBITMAP>(::GetClipboardData(CF_BITMAP));
    BITMAP info{};
    if (bitmap && ::GetObject(bitmap, sizeof(info), &info) && out.Create(info.bmWidth, std::abs(info.bmHeight)))
    {
        BITMAPINFO bi{};
        bi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
        bi.bmiHeader.biWidth = out.Width();
        bi.bmiHeader.biHeight = -out.Height();
        bi.bmiHeader.biPlanes = 1;
        bi.bmiHeader.biBitCount = 32;
        bi.bmiHeader.biCompression = BI_RGB;
        HDC screen = ::GetDC(nullptr);
        ok = ::GetDIBits(screen, bitmap, 0, out.Height(), out.Bits(), &bi, DIB_RGB_COLORS) == out.Height();
        ::ReleaseDC(nullptr, screen);
    }
    ::CloseClipboard();

    if (ok)
        out.ForceOpaque();
    else
        out.Destroy();
    return ok;
}

// ---------------------------------------------------------------------------
// 연필 / 브러시 / 지우개
// ---------------------------------------------------------------------------

class CFreehandTool : public CTool
{
public:
    enum class Mode
    {
        Pencil, ///< 계단 현상이 있는 딱딱한 선
        Brush,  ///< 부드러운(안티앨리어싱) 선
        Eraser, ///< 색 2로 사각형 지우기
    };

    CFreehandTool(CCanvasView& view, Mode mode) : CTool(view), m_mode(mode) {}

    void OnMouseDown(CPoint pt, bool secondaryButton, UINT) override
    {
        if (m_active)
            return;
        const ToolSettings& s = m_view.Settings();
        if (m_mode == Mode::Eraser)
            m_color = secondaryButton ? s.primary : s.secondary;
        else
            m_color = secondaryButton ? s.secondary : s.primary;

        m_view.BeginEdit();
        m_active = true;
        m_last = pt;
        Stroke(pt, pt);
        m_view.Redraw();
    }

    void OnMouseMove(CPoint pt, UINT, bool dragging) override
    {
        if (!m_active || !dragging || pt == m_last)
            return;
        Stroke(m_last, pt);
        m_last = pt;
        m_view.Redraw();
    }

    void OnMouseUp(CPoint pt, UINT) override
    {
        if (!m_active)
            return;
        if (pt != m_last)
            Stroke(m_last, pt);
        m_active = false;
        m_view.EndEdit();
    }

    void Commit() override
    {
        if (m_active)
        {
            m_active = false;
            m_view.EndEdit();
        }
    }

    void Cancel() override
    {
        if (m_active)
        {
            m_active = false;
            m_view.AbortEdit();
        }
    }

private:
    void Stroke(CPoint a, CPoint b)
    {
        CDib& image = m_view.Canvas().Image();
        const int width = (std::max)(1, m_view.Settings().width);
        switch (m_mode)
        {
        case Mode::Pencil:
            StrokePencil(image, a, b, width);
            break;
        case Mode::Brush:
            StrokeBrush(image, a, b, width);
            break;
        case Mode::Eraser:
            StrokeEraser(image, a, b, width);
            break;
        }
    }

    void StrokePencil(CDib& image, CPoint a, CPoint b, int width)
    {
        HDC dc = image.GetDC();
        if (width <= 1)
        {
            HPEN pen = ::CreatePen(PS_SOLID, 1, m_color);
            HGDIOBJ old = ::SelectObject(dc, pen);
            ::MoveToEx(dc, a.x, a.y, nullptr);
            ::LineTo(dc, b.x, b.y);
            ::SetPixelV(dc, b.x, b.y, m_color); // LineTo는 끝점을 칠하지 않음
            ::SelectObject(dc, old);
            ::DeleteObject(pen);
            return;
        }

        if (a == b)
        {
            HBRUSH brush = ::CreateSolidBrush(m_color);
            HGDIOBJ oldBrush = ::SelectObject(dc, brush);
            HGDIOBJ oldPen = ::SelectObject(dc, ::GetStockObject(NULL_PEN));
            const int left = a.x - width / 2;
            const int top = a.y - width / 2;
            ::Ellipse(dc, left, top, left + width + 1, top + width + 1);
            ::SelectObject(dc, oldPen);
            ::SelectObject(dc, oldBrush);
            ::DeleteObject(brush);
            return;
        }

        HPEN pen = ::CreatePen(PS_SOLID, width, m_color); // 굵은 GDI 펜은 둥근 끝
        HGDIOBJ old = ::SelectObject(dc, pen);
        ::MoveToEx(dc, a.x, a.y, nullptr);
        ::LineTo(dc, b.x, b.y);
        ::SelectObject(dc, old);
        ::DeleteObject(pen);
    }

    void StrokeBrush(CDib& image, CPoint a, CPoint b, int width)
    {
        CanvasGraphics cg(image);
        Gdiplus::Graphics& g = cg.G();
        g.SetSmoothingMode(Gdiplus::SmoothingModeAntiAlias);
        const auto w = static_cast<Gdiplus::REAL>(width);
        if (a == b)
        {
            Gdiplus::SolidBrush brush(ToGdiplus(m_color));
            g.FillEllipse(&brush, a.x + 0.5f - w / 2, a.y + 0.5f - w / 2, w, w);
            return;
        }
        Gdiplus::Pen pen(ToGdiplus(m_color), w);
        pen.SetLineCap(Gdiplus::LineCapRound, Gdiplus::LineCapRound, Gdiplus::DashCapRound);
        pen.SetLineJoin(Gdiplus::LineJoinRound);
        g.DrawLine(&pen, a.x + 0.5f, a.y + 0.5f, b.x + 0.5f, b.y + 0.5f);
    }

    void StrokeEraser(CDib& image, CPoint a, CPoint b, int width)
    {
        // 두 점 사이를 1픽셀 간격으로 따라가며 정사각형으로 지웁니다.
        const int half = width / 2;
        const int dx = b.x - a.x;
        const int dy = b.y - a.y;
        const int steps = (std::max)(std::abs(dx), std::abs(dy));
        for (int i = 0; i <= steps; ++i)
        {
            const int x = a.x + (steps ? MulDiv(dx, i, steps) : 0);
            const int y = a.y + (steps ? MulDiv(dy, i, steps) : 0);
            image.FillRect(CRect(x - half, y - half, x - half + width, y - half + width), m_color);
        }
    }

    Mode m_mode;
    bool m_active = false;
    CPoint m_last;
    COLORREF m_color = RGB(0, 0, 0);
};

// ---------------------------------------------------------------------------
// 직선 / 사각형 / 타원
// ---------------------------------------------------------------------------

class CShapeTool : public CTool
{
public:
    CShapeTool(CCanvasView& view, ToolKind kind) : CTool(view), m_kind(kind) {}

    void OnMouseDown(CPoint pt, bool secondaryButton, UINT) override
    {
        if (m_active)
            return;
        const ToolSettings& s = m_view.Settings();
        m_foreground = secondaryButton ? s.secondary : s.primary;
        m_background = secondaryButton ? s.primary : s.secondary;

        m_view.BeginEdit();
        const CDib& image = m_view.Canvas().Image();
        m_base.assign(image.Bits(), image.Bits() + image.PixelCount());
        m_start = m_end = pt;
        m_moved = false;
        m_active = true;
    }

    void OnMouseMove(CPoint pt, UINT flags, bool dragging) override
    {
        if (m_active && dragging)
            Update(pt, flags);
    }

    void OnMouseUp(CPoint pt, UINT flags) override
    {
        if (!m_active)
            return;
        Update(pt, flags);
        Finish();
    }

    void Commit() override
    {
        if (m_active)
            Finish();
    }

    void Cancel() override
    {
        if (!m_active)
            return;
        m_active = false;
        ReleaseBase();
        m_view.AbortEdit();
    }

private:
    void Update(CPoint pt, UINT flags)
    {
        const CPoint end = (flags & MK_SHIFT) ? Constrain(pt) : pt;
        if (m_moved && end == m_end)
            return;
        m_end = end;
        if (m_end != m_start)
            m_moved = true;
        if (!m_moved)
            return;

        // 누르기 직전 그림으로 되돌린 뒤 도형을 다시 그려 미리 보기를 만듭니다.
        CDib& image = m_view.Canvas().Image();
        std::copy(m_base.begin(), m_base.end(), image.Bits());
        Draw(image);
        m_view.Redraw();
    }

    /** @brief Shift: 직선은 45° 단위, 사각형/타원은 정사각형/원 */
    CPoint Constrain(CPoint pt) const
    {
        int dx = pt.x - m_start.x;
        int dy = pt.y - m_start.y;
        if (m_kind == ToolKind::Line)
        {
            if (std::abs(dx) > 2 * std::abs(dy))
                dy = 0;
            else if (std::abs(dy) > 2 * std::abs(dx))
                dx = 0;
            else
            {
                const int d = (std::max)(std::abs(dx), std::abs(dy));
                dx = dx < 0 ? -d : d;
                dy = dy < 0 ? -d : d;
            }
        }
        else
        {
            const int d = (std::max)(std::abs(dx), std::abs(dy));
            dx = dx < 0 ? -d : d;
            dy = dy < 0 ? -d : d;
        }
        return CPoint(m_start.x + dx, m_start.y + dy);
    }

    void Draw(CDib& image)
    {
        const ToolSettings& s = m_view.Settings();
        CanvasGraphics cg(image);
        Gdiplus::Graphics& g = cg.G();
        const auto w = static_cast<Gdiplus::REAL>((std::max)(1, s.width));
        Gdiplus::Pen pen(ToGdiplus(m_foreground), w);

        if (m_kind == ToolKind::Line)
        {
            g.SetSmoothingMode(Gdiplus::SmoothingModeAntiAlias);
            pen.SetLineCap(Gdiplus::LineCapRound, Gdiplus::LineCapRound, Gdiplus::DashCapRound);
            g.DrawLine(&pen, m_start.x + 0.5f, m_start.y + 0.5f, m_end.x + 0.5f, m_end.y + 0.5f);
            return;
        }

        const int l = (std::min)(m_start.x, m_end.x);
        const int t = (std::min)(m_start.y, m_end.y);
        const int r = (std::max)(m_start.x, m_end.x);
        const int b = (std::max)(m_start.y, m_end.y);
        const bool outline = s.shapeStyle != ShapeStyle::Fill;
        const bool fill = s.shapeStyle != ShapeStyle::Outline;
        Gdiplus::SolidBrush brush(ToGdiplus(s.shapeStyle == ShapeStyle::Fill ? m_foreground : m_background));
        const Gdiplus::REAL x = l + 0.5f, y = t + 0.5f;
        const auto cw = static_cast<Gdiplus::REAL>(r - l);
        const auto ch = static_cast<Gdiplus::REAL>(b - t);

        if (m_kind == ToolKind::Rectangle)
        {
            g.SetSmoothingMode(Gdiplus::SmoothingModeNone);
            pen.SetLineJoin(Gdiplus::LineJoinMiter);
            if (fill)
                g.FillRectangle(&brush, Gdiplus::REAL(l), Gdiplus::REAL(t), cw + 1, ch + 1);
            if (outline)
                g.DrawRectangle(&pen, x, y, cw, ch);
        }
        else
        {
            g.SetSmoothingMode(Gdiplus::SmoothingModeAntiAlias);
            if (fill)
                g.FillEllipse(&brush, x, y, cw, ch);
            if (outline)
                g.DrawEllipse(&pen, x, y, cw, ch);
        }
    }

    void Finish()
    {
        m_active = false;
        ReleaseBase();
        if (m_moved)
            m_view.EndEdit();
        else
            m_view.AbortEdit(); // 클릭만 하고 끌지 않음: 기록을 남기지 않음
    }

    void ReleaseBase()
    {
        m_base.clear();
        m_base.shrink_to_fit();
    }

    ToolKind m_kind;
    bool m_active = false;
    bool m_moved = false;
    CPoint m_start;
    CPoint m_end;
    COLORREF m_foreground = RGB(0, 0, 0);
    COLORREF m_background = RGB(255, 255, 255);
    std::vector<uint32_t> m_base; ///< 누르기 직전 픽셀 (미리 보기 복원용)
};

// ---------------------------------------------------------------------------
// 채우기 / 스포이트
// ---------------------------------------------------------------------------

class CFillTool : public CTool
{
public:
    using CTool::CTool;

    void OnMouseDown(CPoint pt, bool secondaryButton, UINT) override
    {
        CCanvas& canvas = m_view.Canvas();
        if (!canvas.Contains(pt))
            return;
        const ToolSettings& s = m_view.Settings();
        m_view.BeginEdit();
        if (canvas.FloodFill(pt, secondaryButton ? s.secondary : s.primary, s.fillTolerance))
            m_view.EndEdit();
        else
            m_view.AbortEdit();
    }
};

class CPickerTool : public CTool
{
public:
    using CTool::CTool;

    void OnMouseDown(CPoint pt, bool secondaryButton, UINT) override
    {
        m_secondary = secondaryButton;
        m_active = true;
        Pick(pt);
    }

    void OnMouseMove(CPoint pt, UINT, bool dragging) override
    {
        if (m_active && dragging)
            Pick(pt);
    }

    void OnMouseUp(CPoint pt, UINT) override
    {
        if (!m_active)
            return;
        m_active = false;
        Pick(pt);
        m_view.NotifyColorPicked(true); // 이전 도구로 돌아가기
    }

    void Cancel() override { m_active = false; }

private:
    void Pick(CPoint pt)
    {
        if (!m_view.Canvas().Contains(pt))
            return;
        const COLORREF color = m_view.Canvas().GetPixel(pt);
        ToolSettings& s = m_view.Settings();
        (m_secondary ? s.secondary : s.primary) = color;
        m_view.NotifyColorPicked(false);
    }

    bool m_active = false;
    bool m_secondary = false;
};
} // namespace

// ---------------------------------------------------------------------------
// 텍스트
// ---------------------------------------------------------------------------

void CTextTool::OnMouseDown(CPoint pt, bool, UINT)
{
    if (IsEditing())
    {
        Commit(); // 편집 상자 바깥을 누르면 확정
        return;
    }
    if (m_view.Canvas().Contains(pt))
        Begin(pt);
}

void CTextTool::Begin(CPoint pt)
{
    m_origin = pt;
    m_backColor = m_view.Canvas().GetPixel(pt);
    m_backBrush.DeleteObject();
    m_backBrush.CreateSolidBrush(m_backColor);

    m_edit.Create(WS_CHILD | ES_MULTILINE | ES_AUTOHSCROLL | ES_AUTOVSCROLL | ES_WANTRETURN, CRect(0, 0, 10, 10),
                  &m_view, IDC_TEXT_EDIT);
    UpdateFont();
    m_edit.SetMargins(0, 0);
    Layout();
    m_edit.ShowWindow(SW_SHOW);
    m_edit.SetFocus();
    m_view.Redraw();
}

void CTextTool::UpdateFont()
{
    LOGFONTW lf = m_view.Settings().font;
    const double scaled = lf.lfHeight * m_view.Zoom();
    lf.lfHeight = static_cast<LONG>(std::lround(scaled));
    if (lf.lfHeight == 0)
        lf.lfHeight = -1;
    lf.lfQuality = ANTIALIASED_QUALITY;

    auto font = std::make_unique<CFont>();
    font->CreateFontIndirect(&lf);
    m_edit.SetFont(font.get());
    m_font = std::move(font); // 새 글꼴을 적용한 뒤에 이전 글꼴 해제
}

void CTextTool::Layout()
{
    if (!IsEditing())
        return;

    CString text;
    m_edit.GetWindowText(text);
    if (text.IsEmpty())
        text = L" ";

    CClientDC dc(&m_edit);
    CFont* oldFont = dc.SelectObject(m_font.get());
    CRect calc(0, 0, 0, 0);
    dc.DrawText(text, &calc, DT_CALCRECT | DT_NOPREFIX | DT_EXPANDTABS);
    TEXTMETRIC tm{};
    dc.GetTextMetrics(&tm);
    dc.SelectObject(oldFont);

    // 다음 글자와 줄이 들어갈 여유를 둬서 편집 상자가 내부 스크롤하지 않게 합니다.
    const int width = (std::max)(calc.Width() + tm.tmMaxCharWidth * 2, tm.tmAveCharWidth * 8);
    const int height = calc.Height() + tm.tmHeight;
    const CPoint origin = m_view.ImageToClient(m_origin);
    m_edit.MoveWindow(origin.x, origin.y, width, height);
    m_edit.LineScroll(-m_edit.GetFirstVisibleLine(), -100000);
}

void CTextTool::OnTextChanged()
{
    Layout();
    m_view.Redraw();
}

void CTextTool::OnViewChanged()
{
    if (IsEditing())
    {
        UpdateFont();
        Layout();
    }
}

void CTextTool::OnSettingsChanged()
{
    if (IsEditing())
    {
        UpdateFont();
        Layout();
        m_edit.Invalidate();
        m_view.Redraw();
    }
}

HBRUSH CTextTool::OnCtlColor(CDC* dc, CWnd* wnd)
{
    if (!IsEditing() || wnd->GetSafeHwnd() != m_edit.GetSafeHwnd())
        return nullptr;
    dc->SetTextColor(m_view.Settings().primary);
    dc->SetBkColor(m_backColor);
    return static_cast<HBRUSH>(m_backBrush.GetSafeHandle());
}

void CTextTool::Commit()
{
    if (!IsEditing())
        return;

    CString text;
    m_edit.GetWindowText(text);
    Close();

    if (!text.IsEmpty())
    {
        const ToolSettings& s = m_view.Settings();
        m_view.BeginEdit();
        LOGFONTW lf = s.font;
        lf.lfQuality = ANTIALIASED_QUALITY;
        HFONT font = ::CreateFontIndirectW(&lf);
        HDC dc = m_view.Canvas().Image().GetDC();
        HGDIOBJ oldFont = ::SelectObject(dc, font);
        ::SetBkMode(dc, TRANSPARENT);
        ::SetTextColor(dc, s.primary);
        RECT rc{m_origin.x, m_origin.y, m_origin.x + CDib::kMaxSide, m_origin.y + CDib::kMaxSide};
        ::DrawTextW(dc, text, -1, &rc, DT_LEFT | DT_TOP | DT_NOPREFIX | DT_EXPANDTABS | DT_NOCLIP);
        ::SelectObject(dc, oldFont);
        ::DeleteObject(font);
        m_view.EndEdit();
    }
    m_view.Redraw();
}

void CTextTool::Cancel()
{
    if (IsEditing())
    {
        Close();
        m_view.Redraw();
    }
}

void CTextTool::Close()
{
    const bool hadFocus = ::GetFocus() == m_edit.GetSafeHwnd();
    m_edit.DestroyWindow();
    if (hadFocus)
        m_view.SetFocus();
}

void CTextTool::DrawOverlay(CDC& dc)
{
    if (!IsEditing())
        return;
    CRect rect;
    m_edit.GetWindowRect(&rect);
    m_view.ScreenToClient(&rect);
    rect.InflateRect(2, 2);
    DrawDashedRect(dc, rect);
}

// ---------------------------------------------------------------------------
// 선택
// ---------------------------------------------------------------------------

CPoint CSelectTool::ClampToImage(CPoint pt) const
{
    const CCanvas& canvas = m_view.Canvas();
    return CPoint((std::clamp<int>)(pt.x, 0, canvas.Width()), (std::clamp<int>)(pt.y, 0, canvas.Height()));
}

void CSelectTool::OnMouseDown(CPoint pt, bool secondaryButton, UINT)
{
    if (secondaryButton)
        return;

    if (m_hasSelection && m_selection.PtInRect(pt))
    {
        if (!m_floating.IsValid())
            Lift();
        m_drag = Drag::Move;
        m_grabOffset = pt - m_selection.TopLeft();
        return;
    }

    Commit();
    m_drag = Drag::Marquee;
    m_anchor = ClampToImage(pt);
    m_selection.SetRectEmpty();
    m_hasSelection = false;
    m_view.Redraw();
}

void CSelectTool::OnMouseMove(CPoint pt, UINT, bool dragging)
{
    if (!dragging)
        return;

    if (m_drag == Drag::Marquee)
    {
        const CPoint p = ClampToImage(pt);
        m_selection.SetRect((std::min)(m_anchor.x, p.x), (std::min)(m_anchor.y, p.y), (std::max)(m_anchor.x, p.x),
                            (std::max)(m_anchor.y, p.y));
        m_hasSelection = !m_selection.IsRectEmpty();
        m_view.Redraw();
        m_view.NotifyStatus();
    }
    else if (m_drag == Drag::Move)
    {
        const CPoint topLeft = pt - m_grabOffset;
        if (topLeft != m_selection.TopLeft())
        {
            m_selection.MoveToXY(topLeft);
            m_view.Redraw();
        }
    }
}

void CSelectTool::OnMouseUp(CPoint pt, UINT flags)
{
    OnMouseMove(pt, flags, true);
    m_drag = Drag::None;
    m_view.Redraw();
    m_view.NotifyStatus();
}

void CSelectTool::Lift()
{
    CDib& image = m_view.Canvas().Image();
    m_view.BeginEdit();
    if (!m_floating.CreateFromRegion(image, m_selection))
    {
        m_view.AbortEdit();
        return;
    }
    image.FillRect(m_selection, m_view.Settings().secondary); // 떼어 낸 자리는 색 2
    m_view.EndEdit();
}

void CSelectTool::Commit()
{
    if (m_floating.IsValid())
    {
        m_view.Canvas().Image().Blit(m_floating, m_selection.TopLeft());
        m_floating.Destroy();
        m_view.EndEdit();
    }
    if (m_hasSelection || m_drag != Drag::None)
    {
        m_hasSelection = false;
        m_selection.SetRectEmpty();
        m_drag = Drag::None;
        m_view.Redraw();
        m_view.NotifyStatus();
    }
}

void CSelectTool::DrawOverlay(CDC& dc)
{
    if (!m_hasSelection)
        return;

    CRect rect = m_view.ImageToClient(m_selection);
    if (m_floating.IsValid())
    {
        dc.SetStretchBltMode(m_view.Zoom() < 1.0 ? HALFTONE : COLORONCOLOR);
        ::SetBrushOrgEx(dc.GetSafeHdc(), 0, 0, nullptr);
        ::StretchBlt(dc.GetSafeHdc(), rect.left, rect.top, rect.Width(), rect.Height(), m_floating.GetDC(), 0, 0,
                     m_floating.Width(), m_floating.Height(), SRCCOPY);
    }
    rect.InflateRect(1, 1);
    DrawDashedRect(dc, rect);
}

LPCTSTR CSelectTool::CursorId(CPoint pt) const
{
    return (m_hasSelection && m_selection.PtInRect(pt)) ? IDC_SIZEALL : IDC_CROSS;
}

bool CSelectTool::Copy()
{
    if (!m_hasSelection)
        return false;
    if (m_floating.IsValid())
        return CopyDibToClipboard(m_view.GetSafeHwnd(), m_floating);

    CDib region;
    if (!region.CreateFromRegion(m_view.Canvas().Image(), m_selection))
        return false;
    return CopyDibToClipboard(m_view.GetSafeHwnd(), region);
}

bool CSelectTool::Cut()
{
    if (!Copy())
        return false;
    DeleteSelection();
    return true;
}

bool CSelectTool::Paste()
{
    CDib clip;
    if (!ReadClipboardImage(m_view.GetSafeHwnd(), clip))
        return false;

    Commit();
    CCanvas& canvas = m_view.Canvas();
    m_view.BeginEdit();

    // 붙여 넣을 그림이 캔버스보다 크면 캔버스를 넓힙니다.
    if (clip.Width() > canvas.Width() || clip.Height() > canvas.Height())
    {
        canvas.ResizeCanvas((std::max)(canvas.Width(), clip.Width()), (std::max)(canvas.Height(), clip.Height()),
                            m_view.Settings().secondary);
        m_view.OnImageChanged();
    }

    // 지금 보이는 영역의 왼쪽 위에 놓습니다.
    CPoint pos = m_view.VisibleImageOrigin();
    pos.x = (std::min<int>)(pos.x, canvas.Width() - clip.Width());
    pos.y = (std::min<int>)(pos.y, canvas.Height() - clip.Height());

    m_selection = CRect(pos, CSize(clip.Width(), clip.Height()));
    m_floating.Swap(clip);
    m_hasSelection = true;
    m_view.EndEdit();
    m_view.NotifyStatus();
    return true;
}

void CSelectTool::SelectAll()
{
    Commit();
    const CCanvas& canvas = m_view.Canvas();
    m_selection.SetRect(0, 0, canvas.Width(), canvas.Height());
    m_hasSelection = true;
    m_view.Redraw();
    m_view.NotifyStatus();
}

void CSelectTool::DeleteSelection()
{
    if (!m_hasSelection)
        return;

    if (m_floating.IsValid())
    {
        m_floating.Destroy(); // 떼어 낸 픽셀을 버림 (원래 자리는 이미 색 2)
        m_view.EndEdit();
    }
    else
    {
        m_view.BeginEdit();
        m_view.Canvas().Image().FillRect(m_selection, m_view.Settings().secondary);
        m_view.EndEdit();
    }
    m_hasSelection = false;
    m_selection.SetRectEmpty();
    m_view.Redraw();
    m_view.NotifyStatus();
}

// ---------------------------------------------------------------------------

std::unique_ptr<CTool> CreateTool(ToolKind kind, CCanvasView& view)
{
    switch (kind)
    {
    case ToolKind::Select:
        return std::make_unique<CSelectTool>(view);
    case ToolKind::Pencil:
        return std::make_unique<CFreehandTool>(view, CFreehandTool::Mode::Pencil);
    case ToolKind::Brush:
        return std::make_unique<CFreehandTool>(view, CFreehandTool::Mode::Brush);
    case ToolKind::Eraser:
        return std::make_unique<CFreehandTool>(view, CFreehandTool::Mode::Eraser);
    case ToolKind::Fill:
        return std::make_unique<CFillTool>(view);
    case ToolKind::Picker:
        return std::make_unique<CPickerTool>(view);
    case ToolKind::Text:
        return std::make_unique<CTextTool>(view);
    case ToolKind::Line:
    case ToolKind::Rectangle:
    case ToolKind::Ellipse:
        return std::make_unique<CShapeTool>(view, kind);
    }
    return std::make_unique<CFreehandTool>(view, CFreehandTool::Mode::Pencil);
}
