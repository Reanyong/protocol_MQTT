// pch.h: 미리 컴파일된 헤더 파일입니다.
// 아래 나열된 파일은 한 번만 컴파일되었으며, 향후 빌드에 대한 빌드 성능을 향상합니다.
// 코드 컴파일 및 여러 코드 검색 기능을 포함하여 IntelliSense 성능에도 영향을 미칩니다.
// 그러나 여기에 나열된 파일은 빌드 간 업데이트되는 경우 모두 다시 컴파일됩니다.
// 여기에 자주 업데이트할 파일을 추가하지 마세요. 그러면 성능이 저하됩니다.

#ifndef PCH_H
#define PCH_H

// 프레임워크 헤더를 여기에 추가합니다.
#include "framework.h"

// 성능 최적화: 자주 사용되는 헤더들을 pch에 포함
#include <vector>
#include <map>
#include <string>
#include <mutex>
#include <deque>
#include <memory>
#include <algorithm>

// JSON 라이브러리
#include "json.hpp"

// Windows API 최적화
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif

#include "..\include\EVStruct.h"
#include "..\Include\EVCommLib.h"
#include "..\Include\EVILib.h"

#endif //PCH_H
