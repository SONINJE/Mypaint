#pragma once

/**
 * @file CanvasView.h
 * @brief 그림을 화면에 표시하고 마우스 입력을 도구에 전달하는 자식 창
 */

#include <functional>
#include <memory>
#include "Canvas.h"
#include "Tools.h"

/**
 * @brief 캔버스 뷰. 확대/축소, 스크롤, 화면↔이미지 좌표 변환, 도구 수명 관리를 맡습니다.
 */
class CCanvasView : public CWnd
{
public:
    CCanvasView() = default;
    ~CCanvasView() override = default;

    /** @brief 창 생성 */
    BOOL Create(CWnd* parent, UINT id);
    /** @brief 표시할 문서와 도구 설정 연결 */
    void Attach(CCanvas* canvas, ToolSettings* settings);

    CCanvas& Canvas() { return *m_canvas; }
    const CCanvas& Canvas() const { return *m_canvas; }
    ToolSettings& Settings() { return *m_settings; }

    // --- 도구 ---
    void SetTool(ToolKind kind);
    ToolKind GetTool() const { return m_toolKind; }
    /** @brief 진행 중인 작업 확정 (끌기, 텍스트 입력, 떠 있는 선택 영역) */
    void CommitTool();
    /** @brief 진행 중인 작업 취소 */
    void CancelTool();
    /** @brief 선택 도구일 때만 유효 */
    CSelectTool* SelectTool();
    bool HasSelection();
    void OnSettingsChanged();

    // --- 문서 변경 알림 ---
    /** @brief 새 그림/열기: 확대 비율과 스크롤 초기화 */
    void OnImageReplaced();
    /** @brief 실행 취소·크기 변경 등으로 그림이 바뀜 */
    void OnImageChanged();

    // --- 확대/축소 ---
    double Zoom() const { return m_zoom; }
    void ZoomIn();
    void ZoomOut();
    void ZoomActual();
    void ZoomToFit();

    // --- 도구가 쓰는 함수 ---
    /** @brief 픽셀을 바꾸기 전에 호출 (실행 취소 기록) */
    void BeginEdit();
    /** @brief 픽셀을 바꾼 뒤 호출 (수정 표시, 다시 그리기) */
    void EndEdit();
    /** @brief BeginEdit 이후의 변경을 버림 */
    void AbortEdit();
    void Redraw();
    CPoint ImageToClient(CPoint pt) const;
    CRect ImageToClient(const CRect& rect) const;
    /** @brief 화면에 보이는 영역의 왼쪽 위 이미지 좌표 */
    CPoint VisibleImageOrigin() const;
    void NotifyColorPicked(bool finished);
    void NotifyStatus();

    // --- 상태 표시줄 ---
    /** @brief 마우스가 그림 위에 있으면 이미지 좌표를 돌려줌 */
    bool CursorPosition(CPoint& pt) const;

    std::function<void()> onModified;                ///< 그림이 바뀜
    std::function<void()> onStatusChanged;           ///< 커서 위치·선택·확대 비율이 바뀜
    std::function<void(bool finished)> onColorPicked; ///< 스포이트로 색을 고름

protected:
    afx_msg void OnPaint();
    afx_msg BOOL OnEraseBkgnd(CDC* pDC);
    afx_msg void OnSize(UINT nType, int cx, int cy);
    afx_msg void OnHScroll(UINT nSBCode, UINT nPos, CScrollBar* pScrollBar);
    afx_msg void OnVScroll(UINT nSBCode, UINT nPos, CScrollBar* pScrollBar);
    afx_msg BOOL OnMouseWheel(UINT nFlags, short zDelta, CPoint pt);
    afx_msg void OnLButtonDown(UINT nFlags, CPoint point);
    afx_msg void OnLButtonUp(UINT nFlags, CPoint point);
    afx_msg void OnRButtonDown(UINT nFlags, CPoint point);
    afx_msg void OnRButtonUp(UINT nFlags, CPoint point);
    afx_msg void OnMouseMove(UINT nFlags, CPoint point);
    afx_msg void OnMouseLeave();
    afx_msg BOOL OnSetCursor(CWnd* pWnd, UINT nHitTest, UINT message);
    afx_msg void OnCaptureChanged(CWnd* pWnd);
    afx_msg HBRUSH OnCtlColor(CDC* pDC, CWnd* pWnd, UINT nCtlColor);
    afx_msg void OnTextEditChange();
    DECLARE_MESSAGE_MAP()

private:
    enum class Button
    {
        None,
        Left,
        Right,
    };

    CPoint ClientToImage(CPoint pt) const;
    CPoint Origin() const;
    CSize ContentSize() const;
    void UpdateScrollBars();
    void ScrollTo(int x, int y);
    void HandleScroll(int bar, UINT code);
    void ZoomAround(double zoom, CPoint anchor);
    double NextZoomLevel(bool zoomIn) const;
    void BeginDrag(Button button, UINT flags, CPoint point);
    void EndDrag(Button button, UINT flags, CPoint point);
    void FinishDragSilently(bool cancel);
    static UINT CurrentKeyFlags();

    CCanvas* m_canvas = nullptr;
    ToolSettings* m_settings = nullptr;
    std::unique_ptr<CTool> m_tool;
    ToolKind m_toolKind = ToolKind::Pencil;

    double m_zoom = 1.0;
    CPoint m_scroll{0, 0};
    bool m_updatingScroll = false;
    int m_wheelZoomAccum = 0;

    Button m_button = Button::None;
    CPoint m_lastImagePt;
    CPoint m_cursorPt;
    bool m_cursorInside = false;
    bool m_trackingLeave = false;
};
