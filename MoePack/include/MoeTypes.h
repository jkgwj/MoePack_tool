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
  * @brief 源图像像素格式枚举
  * @note 用于指定 stb_image 解码后的像素格式，原在 MoeTypes.h，移至此处供 MoeUnpack 共享
  */
enum MOE_ImageFormat {
    RGB888 = 0,     ///< RGB 无透明通道 (8bit × 3)
    RGBA8888,       ///< RGBA 8位四通道 (8bit × 4)
    L8,             ///< 单通道 8 位灰度
    LA88,           ///< 8 位灰度 + 8 位 Alpha
    RGBA32F         ///< RGBA 32 位浮点数
};

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
