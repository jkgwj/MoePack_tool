/*
 * Copyright 2026 jkgwj
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
#pragma once
#include "MoeHeader.h"

/**
 * @brief 目标平台枚举
 * @note 用于指定 GPU 纹理压缩的目标平台，不同平台使用不同的压缩格式
 */
enum MOE_Platform {
    WINDOWS,    ///< Windows 平台 (BC7)
    LINUX,      ///< Linux 平台 (ETC2)
    MACOS,      ///< macOS 平台 (ASTC 8x8)
    IOS,        ///< iOS 平台 (ASTC 6x6)
    ANDROID,    ///< Android 平台 (ETC2)
    NOP         ///< 未指定平台
};
