// pch.h: 미리 컴파일된 헤더 파일입니다.

#ifndef PCH_H
#define PCH_H

// 프레임워크 헤더
#include "framework.h"

// 성능 최적화: 자주 사용되는 표준 라이브러리 헤더들
#include <vector>
#include <map>
#include <string>
#include <mutex>
#include <memory>
#include <algorithm>

// JSON 라이브러리
#include "json.hpp"

// EasyView 라이브러리
#include "..\include\EVStruct.h"
#include "..\Include\EVCommLib.h"
#include "..\Include\EVILib.h"

#endif //PCH_H
