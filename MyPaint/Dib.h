#pragma once

/**
 * @file Dib.h
 * @brief 32비트 DIB 섹션 래퍼. GDI(메모리 DC)와 GDI+, 직접 픽셀 접근을 모두 지원합니다.
 */

#include <cstdint>
#include <vector>

/** @brief 픽셀(0xAARRGGBB)과 COLORREF(0x00BBGGRR) 사이의 변환 도우미 */
namespace px
{
/** @brief COLORREF를 불투명 픽셀 값으로 변환 */
inline uint32_t FromColor(COLORREF c)
{
    return 0xFF000000u | (uint32_t(GetRValue(c)) << 16) | (uint32_t(GetGValue(c)) << 8) | GetBValue(c);
}

/** @brief 픽셀 값을 COLORREF로 변환 (알파 무시) */
inline COLORREF ToColor(uint32_t p)
{
    return RGB((p >> 16) & 0xFF, (p >> 8) & 0xFF, p & 0xFF);
}
} // namespace px

/**
 * @brief 위에서 아래로(top-down) 저장되는 32bpp DIB 섹션.
 *
 * 메모리 DC에 항상 선택되어 있으므로 GDI 함수로 바로 그릴 수 있고,
 * Bits()로 픽셀 버퍼에 직접 접근할 수도 있습니다.
 */
class CDib
{
public:
    static constexpr int kMaxSide = 20000;              ///< 한 변의 최대 픽셀 수
    static constexpr long long kMaxPixels = 100000000;  ///< 최대 픽셀 수 (약 400MB)

    CDib() = default;
    ~CDib();
    CDib(const CDib&) = delete;
    CDib& operator=(const CDib&) = delete;

    /** @brief 지정 크기로 생성 (내용은 0으로 초기화). 실패하면 false */
    bool Create(int width, int height);
    /** @brief 지정 크기로 생성하고 색으로 채움 */
    bool Create(int width, int height, COLORREF fill);
    /** @brief 다른 DIB의 일부 영역을 복사해 생성 */
    bool CreateFromRegion(const CDib& src, CRect region);
    /** @brief 자원 해제 */
    void Destroy();
    /** @brief 내용 교환 */
    void Swap(CDib& other) noexcept;

    bool IsValid() const { return m_hBitmap != nullptr; }
    int Width() const { return m_width; }
    int Height() const { return m_height; }
    size_t PixelCount() const { return size_t(m_width) * size_t(m_height); }

    /** @brief 픽셀 버퍼. 대기 중인 GDI 작업을 먼저 반영합니다. */
    uint32_t* Bits();
    const uint32_t* Bits() const;
    /** @brief DIB가 선택된 메모리 DC */
    HDC GetDC() const { return m_hdc; }

    /** @brief 전체를 한 색으로 채움 */
    void Fill(COLORREF color);
    /** @brief 사각형 영역을 한 색으로 채움 (이미지 밖은 잘림) */
    void FillRect(CRect rect, COLORREF color);
    /** @brief 알파 채널을 모두 0xFF로 설정 (GDI 텍스트 등이 지운 알파 복구) */
    void ForceOpaque();
    /** @brief 다른 DIB를 지정 위치에 그대로 복사 (이미지 밖은 잘림) */
    void Blit(const CDib& src, CPoint dest);

private:
    HBITMAP m_hBitmap = nullptr;
    HGDIOBJ m_hOldBitmap = nullptr;
    HDC m_hdc = nullptr;
    uint32_t* m_bits = nullptr;
    int m_width = 0;
    int m_height = 0;
};
