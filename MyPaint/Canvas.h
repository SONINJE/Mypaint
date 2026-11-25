#pragma once

/**
 * @file Canvas.h
 * @brief 그림 데이터(비트맵)와 실행 취소 기록, 파일 입출력, 이미지 변환을 담당합니다.
 */

#include <deque>
#include <vector>
#include "Dib.h"

/**
 * @brief 확장자(점 포함 또는 제외)에 맞는 이미지 MIME 형식을 돌려줍니다.
 * @return 지원하지 않는 확장자면 nullptr
 */
const wchar_t* MimeTypeForPath(const CString& path);

/**
 * @brief 그림판 문서. 픽셀 데이터와 실행 취소/다시 실행 기록을 갖습니다.
 *
 * 픽셀을 바꾸는 쪽은 바꾸기 전에 PushUndo()를 호출합니다.
 * 변경이 없었으면 RevertLastUndo()로 기록을 되돌립니다.
 */
class CCanvas
{
public:
    /** @brief 새 그림 */
    bool New(int width, int height, COLORREF background);
    /** @brief 이미지 파일 열기 (BMP, PNG, JPEG, GIF, TIFF 등 GDI+가 읽는 형식) */
    bool Load(const CString& path, CString& error);
    /** @brief 확장자에 맞는 형식으로 저장 */
    bool Save(const CString& path, CString& error);

    CDib& Image() { return m_image; }
    const CDib& Image() const { return m_image; }
    int Width() const { return m_image.Width(); }
    int Height() const { return m_image.Height(); }
    bool Contains(CPoint pt) const { return pt.x >= 0 && pt.y >= 0 && pt.x < Width() && pt.y < Height(); }
    COLORREF GetPixel(CPoint pt) const;

    // --- 실행 취소 ---
    /** @brief 현재 상태를 실행 취소 기록에 저장 (다시 실행 기록은 지움) */
    void PushUndo();
    /** @brief 마지막 PushUndo() 상태로 되돌리고 그 기록을 버림 (작업 취소용) */
    void RevertLastUndo();
    bool CanUndo() const { return !m_undo.empty(); }
    bool CanRedo() const { return !m_redo.empty(); }
    bool Undo();
    bool Redo();
    void ClearHistory();

    // --- 이미지 작업 (호출 전에 PushUndo 필요) ---
    /** @brief 영역 채우기. tolerance는 채널별 허용 차이(0~255). 바뀐 픽셀이 있으면 true */
    bool FloodFill(CPoint pt, COLORREF color, int tolerance);
    void FlipHorizontal();
    void FlipVertical();
    bool Rotate90(bool clockwise);
    /** @brief 캔버스 크기만 바꿈 (그림은 왼쪽 위 기준으로 유지, 늘어난 부분은 배경색) */
    bool ResizeCanvas(int width, int height, COLORREF background);
    /** @brief 그림을 늘이거나 줄임 (고품질 보간) */
    bool ResizeImage(int width, int height);

    bool IsModified() const { return m_modified; }
    void SetModified(bool modified) { m_modified = modified; }

private:
    struct Snapshot
    {
        int width = 0;
        int height = 0;
        std::vector<uint32_t> pixels;
    };

    Snapshot Capture() const;
    bool Restore(const Snapshot& snapshot);
    void TrimUndo();

    CDib m_image;
    std::deque<Snapshot> m_undo;
    std::deque<Snapshot> m_redo;
    bool m_modified = false;
};
