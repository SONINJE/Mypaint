#pragma once

/**
 * @file ColorBar.h
 * @brief 색 1/색 2 표시와 기본 팔레트
 */

#include <functional>
#include "Tools.h"

/**
 * @brief 색 선택 막대.
 *
 * - 팔레트 왼쪽 클릭: 색 1, 오른쪽 클릭: 색 2
 * - 색 1/색 2 상자 왼쪽 클릭: 색 편집 대화 상자, 오른쪽 클릭: 두 색 바꾸기
 */
class CColorBar : public CWnd
{
public:
    BOOL Create(CWnd* parent, UINT id);
    void Attach(ToolSettings* settings) { m_settings = settings; }
    void SetDpi(UINT dpi);
    void SetUiFont(CFont* font) { m_font = font; }
    int IdealWidth() const;
    int IdealHeight() const;

    std::function<void()> onChanged; ///< 색이 바뀜

protected:
    afx_msg void OnPaint();
    afx_msg BOOL OnEraseBkgnd(CDC* pDC);
    afx_msg void OnLButtonDown(UINT nFlags, CPoint point);
    afx_msg void OnRButtonDown(UINT nFlags, CPoint point);
    DECLARE_MESSAGE_MAP()

private:
    static constexpr int kHitNone = -1;
    static constexpr int kHitPrimary = -2;
    static constexpr int kHitSecondary = -3;

    int S(int value) const { return MulDiv(value, static_cast<int>(m_dpi), 96); }
    CRect PrimaryRect() const;
    CRect SecondaryRect() const;
    CRect SwatchRect(int index) const;
    int HitTest(CPoint pt) const;
    bool EditColor(COLORREF& color);
    void Changed();

    ToolSettings* m_settings = nullptr;
    CFont* m_font = nullptr;
    UINT m_dpi = 96;
};
