#pragma once

/**
 * @file Tools.h
 * @brief 그리기 도구. 새 도구는 CTool을 상속하고 CreateTool()에 등록하면 됩니다.
 */

#include <memory>
#include "Dib.h"

class CCanvasView;

/** @brief 도구 종류. 순서는 도구 버튼 순서와 같습니다. */
enum class ToolKind
{
    Select,
    Pencil,
    Brush,
    Eraser,
    Fill,
    Picker,
    Text,
    Line,
    Rectangle,
    Ellipse,
};
constexpr int kToolCount = 10;

/** @brief 사각형·타원 그리기 방식 */
enum class ShapeStyle
{
    Outline,        ///< 윤곽선만 (색 1)
    Fill,           ///< 채우기만 (색 1)
    OutlineAndFill, ///< 윤곽선(색 1) + 채우기(색 2)
};

/** @brief 도구들이 함께 쓰는 설정. 대화 상자가 소유하고 뷰와 도구가 참조합니다. */
struct ToolSettings
{
    COLORREF primary = RGB(0, 0, 0);         ///< 색 1 (왼쪽 버튼)
    COLORREF secondary = RGB(255, 255, 255); ///< 색 2 (오른쪽 버튼, 지우개, 배경)
    int width = 3;                           ///< 선 두께 (이미지 픽셀)
    ShapeStyle shapeStyle = ShapeStyle::Outline;
    int fillTolerance = 32;                  ///< 채우기 허용 오차 (채널별 0~255)
    LOGFONTW font{};                         ///< 텍스트 글꼴. lfHeight는 이미지 픽셀 단위
};

/**
 * @brief 도구 기본 클래스.
 *
 * 좌표는 모두 이미지 픽셀 좌표입니다. 뷰가 마우스 캡처와 좌표 변환을 처리합니다.
 */
class CTool
{
public:
    explicit CTool(CCanvasView& view) : m_view(view) {}
    virtual ~CTool() = default;
    CTool(const CTool&) = delete;
    CTool& operator=(const CTool&) = delete;

    /** @brief 마우스 단추 누름. secondaryButton은 오른쪽 단추 여부 */
    virtual void OnMouseDown(CPoint /*pt*/, bool /*secondaryButton*/, UINT /*flags*/) {}
    /** @brief 마우스 이동. dragging은 단추를 누른 채 움직이는지 여부 */
    virtual void OnMouseMove(CPoint /*pt*/, UINT /*flags*/, bool /*dragging*/) {}
    /** @brief 마우스 단추 놓음 */
    virtual void OnMouseUp(CPoint /*pt*/, UINT /*flags*/) {}
    /** @brief 진행 중인 작업 확정 (도구 전환, 저장 등에서 호출) */
    virtual void Commit() {}
    /** @brief 진행 중인 작업 취소 (Esc) */
    virtual void Cancel() {}
    /** @brief 그림 위에 덧그리기 (화면 좌표). 선택 영역 테두리 등 */
    virtual void DrawOverlay(CDC& /*dc*/) {}
    /** @brief 이미지 위에서 보일 커서 */
    virtual LPCTSTR CursorId(CPoint /*pt*/) const { return IDC_CROSS; }
    /** @brief 확대 비율이나 스크롤 위치가 바뀜 */
    virtual void OnViewChanged() {}
    /** @brief 색, 두께, 글꼴 등 설정이 바뀜 */
    virtual void OnSettingsChanged() {}

protected:
    CCanvasView& m_view;
};

/** @brief 도구 생성 */
std::unique_ptr<CTool> CreateTool(ToolKind kind, CCanvasView& view);

/**
 * @brief 텍스트 도구. 클릭한 곳에 편집 상자를 띄우고, 확정하면 그림에 새깁니다.
 */
class CTextTool : public CTool
{
public:
    using CTool::CTool;

    void OnMouseDown(CPoint pt, bool secondaryButton, UINT flags) override;
    void Commit() override;
    void Cancel() override;
    void DrawOverlay(CDC& dc) override;
    LPCTSTR CursorId(CPoint) const override { return IDC_IBEAM; }
    void OnViewChanged() override;
    void OnSettingsChanged() override;

    /** @brief 편집 상자 내용이 바뀜 (크기 맞춤) */
    void OnTextChanged();
    /** @brief 편집 상자 색 지정. 해당 컨트롤이 아니면 nullptr */
    HBRUSH OnCtlColor(CDC* dc, CWnd* wnd);
    bool IsEditing() const { return m_edit.GetSafeHwnd() != nullptr; }

private:
    void Begin(CPoint pt);
    void UpdateFont();
    void Layout();
    void Close();

    CEdit m_edit;
    std::unique_ptr<CFont> m_font;
    CBrush m_backBrush;
    COLORREF m_backColor = RGB(255, 255, 255);
    CPoint m_origin;
};

/**
 * @brief 사각형 선택 도구. 선택 영역 이동, 잘라내기/복사/붙여넣기/삭제를 지원합니다.
 *
 * 선택 영역을 끌면 픽셀을 떼어 내(플로팅) 움직이고, 확정할 때 다시 붙입니다.
 */
class CSelectTool : public CTool
{
public:
    using CTool::CTool;

    void OnMouseDown(CPoint pt, bool secondaryButton, UINT flags) override;
    void OnMouseMove(CPoint pt, UINT flags, bool dragging) override;
    void OnMouseUp(CPoint pt, UINT flags) override;
    void Commit() override;
    void Cancel() override { Commit(); }
    void DrawOverlay(CDC& dc) override;
    LPCTSTR CursorId(CPoint pt) const override;

    bool HasSelection() const { return m_hasSelection; }
    CRect SelectionRect() const { return m_selection; }
    bool Copy();
    bool Cut();
    bool Paste();
    void SelectAll();
    void DeleteSelection();

private:
    enum class Drag
    {
        None,
        Marquee,
        Move,
    };

    void Lift();
    CPoint ClampToImage(CPoint pt) const;

    Drag m_drag = Drag::None;
    CPoint m_anchor;
    CPoint m_grabOffset;
    CRect m_selection;
    bool m_hasSelection = false;
    CDib m_floating; ///< 떼어 낸 픽셀 (유효하면 플로팅 상태)
};
