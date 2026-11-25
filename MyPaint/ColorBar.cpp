/**
 * @file ColorBar.cpp
 * @brief CColorBar 구현
 */
#include "pch.h"
#include "ColorBar.h"

namespace
{
constexpr int kColumns = 10;
constexpr COLORREF kPalette[] = {
    RGB(0, 0, 0),       RGB(127, 127, 127), RGB(136, 0, 21),    RGB(237, 28, 36),   RGB(255, 127, 39),
    RGB(255, 242, 0),   RGB(34, 177, 76),   RGB(0, 162, 232),   RGB(63, 72, 204),   RGB(163, 73, 164),
    RGB(255, 255, 255), RGB(195, 195, 195), RGB(185, 122, 87),  RGB(255, 174, 201), RGB(255, 201, 14),
    RGB(239, 228, 176), RGB(181, 230, 29),  RGB(153, 217, 234), RGB(112, 146, 190), RGB(200, 191, 231),
};
constexpr int kSwatchCount = static_cast<int>(std::size(kPalette));

/** @brief 색 편집 대화 상자의 사용자 지정 색 (프로그램이 실행되는 동안 유지) */
COLORREF g_customColors[16] = {
    RGB(255, 255, 255), RGB(255, 255, 255), RGB(255, 255, 255), RGB(255, 255, 255),
    RGB(255, 255, 255), RGB(255, 255, 255), RGB(255, 255, 255), RGB(255, 255, 255),
    RGB(255, 255, 255), RGB(255, 255, 255), RGB(255, 255, 255), RGB(255, 255, 255),
    RGB(255, 255, 255), RGB(255, 255, 255), RGB(255, 255, 255), RGB(255, 255, 255),
};
} // namespace

BEGIN_MESSAGE_MAP(CColorBar, CWnd)
    ON_WM_PAINT()
    ON_WM_ERASEBKGND()
    ON_WM_LBUTTONDOWN()
    ON_WM_RBUTTONDOWN()
END_MESSAGE_MAP()

BOOL CColorBar::Create(CWnd* parent, UINT id)
{
    LPCTSTR cls = AfxRegisterWndClass(0, ::LoadCursor(nullptr, IDC_HAND), nullptr, nullptr);
    return CWnd::CreateEx(0, cls, nullptr, WS_CHILD | WS_VISIBLE, CRect(0, 0, 10, 10), parent, id);
}

void CColorBar::SetDpi(UINT dpi)
{
    m_dpi = dpi;
    if (GetSafeHwnd())
        Invalidate(FALSE);
}

// 배치: [색 1][색 2]  [팔레트 2줄 × 10칸]
//       라벨  라벨     안내 문구
CRect CColorBar::PrimaryRect() const
{
    return CRect(S(4), S(2), S(4) + S(32), S(2) + S(32));
}

CRect CColorBar::SecondaryRect() const
{
    CRect r = PrimaryRect();
    r.OffsetRect(S(40), 0);
    return r;
}

CRect CColorBar::SwatchRect(int index) const
{
    const int x0 = SecondaryRect().right + S(16);
    const int pitch = S(21);
    const int size = S(18);
    const int col = index % kColumns;
    const int row = index / kColumns;
    const int x = x0 + col * pitch;
    const int y = S(2) + row * pitch;
    return CRect(x, y, x + size, y + size);
}

int CColorBar::IdealWidth() const
{
    return SwatchRect(kColumns - 1).right + S(4);
}

int CColorBar::IdealHeight() const
{
    return S(64);
}

int CColorBar::HitTest(CPoint pt) const
{
    if (PrimaryRect().PtInRect(pt))
        return kHitPrimary;
    if (SecondaryRect().PtInRect(pt))
        return kHitSecondary;
    for (int i = 0; i < kSwatchCount; ++i)
        if (SwatchRect(i).PtInRect(pt))
            return i;
    return kHitNone;
}

BOOL CColorBar::OnEraseBkgnd(CDC*)
{
    return TRUE;
}

void CColorBar::OnPaint()
{
    CPaintDC paintDC(this);
    CRect rc;
    GetClientRect(&rc);

    CDC mem;
    mem.CreateCompatibleDC(&paintDC);
    CBitmap buffer;
    buffer.CreateCompatibleBitmap(&paintDC, rc.Width(), rc.Height());
    CBitmap* oldBitmap = mem.SelectObject(&buffer);
    mem.FillSolidRect(rc, ::GetSysColor(COLOR_BTNFACE));

    CFont* oldFont = m_font ? mem.SelectObject(m_font) : nullptr;
    mem.SetBkMode(TRANSPARENT);
    mem.SetTextColor(::GetSysColor(COLOR_BTNTEXT));
    const COLORREF frame = RGB(120, 120, 120);

    if (m_settings)
    {
        const struct
        {
            CRect rect;
            COLORREF color;
            const wchar_t* label;
        } boxes[] = {
            {PrimaryRect(), m_settings->primary, L"색 1"},
            {SecondaryRect(), m_settings->secondary, L"색 2"},
        };
        for (const auto& box : boxes)
        {
            CRect outer = box.rect;
            mem.Draw3dRect(outer, frame, frame);
            outer.DeflateRect(1, 1);
            mem.Draw3dRect(outer, RGB(255, 255, 255), RGB(255, 255, 255));
            outer.DeflateRect(1, 1);
            mem.FillSolidRect(outer, box.color);

            CRect label(box.rect.left - S(4), box.rect.bottom + S(2), box.rect.right + S(4), box.rect.bottom + S(20));
            mem.DrawText(box.label, -1, &label, DT_CENTER | DT_TOP | DT_SINGLELINE | DT_NOPREFIX);
        }
    }

    for (int i = 0; i < kSwatchCount; ++i)
    {
        const CRect r = SwatchRect(i);
        mem.FillSolidRect(r, kPalette[i]);
        mem.Draw3dRect(r, frame, frame);
    }

    const CRect first = SwatchRect(0);
    CRect hint(first.left, SwatchRect(kColumns).bottom + S(4), IdealWidth(), rc.bottom);
    mem.SetTextColor(::GetSysColor(COLOR_GRAYTEXT));
    mem.DrawText(L"왼쪽 클릭: 색 1 · 오른쪽 클릭: 색 2", -1, &hint,
                 DT_LEFT | DT_TOP | DT_SINGLELINE | DT_NOPREFIX | DT_END_ELLIPSIS);

    if (oldFont)
        mem.SelectObject(oldFont);
    paintDC.BitBlt(0, 0, rc.Width(), rc.Height(), &mem, 0, 0, SRCCOPY);
    mem.SelectObject(oldBitmap);
}

bool CColorBar::EditColor(COLORREF& color)
{
    CColorDialog dlg(color, CC_FULLOPEN | CC_RGBINIT, this);
    dlg.m_cc.lpCustColors = g_customColors;
    if (dlg.DoModal() != IDOK)
        return false;
    color = dlg.GetColor();
    return true;
}

void CColorBar::Changed()
{
    Invalidate(FALSE);
    if (onChanged)
        onChanged();
}

void CColorBar::OnLButtonDown(UINT, CPoint point)
{
    if (!m_settings)
        return;
    const int hit = HitTest(point);
    if (hit == kHitPrimary)
    {
        if (EditColor(m_settings->primary))
            Changed();
    }
    else if (hit == kHitSecondary)
    {
        if (EditColor(m_settings->secondary))
            Changed();
    }
    else if (hit >= 0)
    {
        m_settings->primary = kPalette[hit];
        Changed();
    }
}

void CColorBar::OnRButtonDown(UINT, CPoint point)
{
    if (!m_settings)
        return;
    const int hit = HitTest(point);
    if (hit == kHitPrimary || hit == kHitSecondary)
    {
        std::swap(m_settings->primary, m_settings->secondary);
        Changed();
    }
    else if (hit >= 0)
    {
        m_settings->secondary = kPalette[hit];
        Changed();
    }
}
