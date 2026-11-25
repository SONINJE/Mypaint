#pragma once

/**
 * @file pch.h
 * @brief 미리 컴파일된 헤더. 자주 바뀌지 않는 헤더를 여기에 추가합니다.
 */
#include "framework.h"

// GDI+ (Windows SDK에 포함, Windows에 기본 설치되어 있어 별도 배포가 필요 없음)
#include <gdiplus.h>
#pragma comment(lib, "gdiplus.lib")

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <deque>
#include <functional>
#include <iterator>
#include <memory>
#include <vector>
