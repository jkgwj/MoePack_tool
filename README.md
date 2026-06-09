# MoePack

游戏资源打包/解包工具，附带轻量级解包库。

核心功能：将图片资源编码为 GPU 压缩纹理（KTX2 容器），并对非图片资源提供通用加密封包，从而平衡资源体积与加载速度。

## 功能

- **图片封包** — 解码 → KTX2 GPU 压缩（Basis Universal）→ 压缩/加密 → .moe
- **图片解包** — .moe → 解密/解压 → KTX2 验证 → 输出 .ktx2
- **非图片封包** — 原始文件 → 压缩/加密 → .moe
- **非图片解包** — .moe → 解密/解压 → 输出原始文件
- **流式加密封包** — 原始音频 → 分块加密 (Chacha20-Poly1305) → .moe
- **流式解包读取** — MoeStreamReader 逐块解密，适合音频边读边播

## 文件格式

所有 .moe 文件使用统一的 98 字节头部 (MoeHeader)，魔数为 "MOE"。

| 字段 | 大小 | 说明 |
|------|------|------|
| magic | 4B | 魔数 "MOE\0" |
| version | 8B | 版本号 "v0.2.00" |
| header_size | 4B | 头部总大小 (98) |
| ztsd_on | 1B | ZSTD 压缩标志 |
| encrypted_on | 1B | 加密标志 |
| check_data | 32B | SHA-256 完整性校验 |
| chunk_size | 4B | 分块加密块大小 (0=未分块) |
| chunk_count | 4B | 分块加密总块数 (0=未分块) |
| audio_format | 4B | 音频格式标识 |
| original_size | 4B | 原始数据大小 |
| reserved | 32B | 保留字段 |

- 多字节字段以大端序存储，通过 MoeHeader::to_big_endian() / from_big_endian() 转换
- 打包时根据 pack_ex 或 pack_ex_stream 自动选择一次性加密或分块加密
- 解包时自动读取头部标志位判断是否需要解密/解压

## 项目结构

```
MoePack/
├── MoePack/                # 封包库 + CLI 工具
│   ├── include/
│   │   ├── MoePack.h       # 封包器 API 声明（Doxygen 注释）
│   │   ├── cmd.h           # CLI 命令处理声明
│   │   ├── MoeTypes.h      # 公共枚举 (MOE_Platform, MOE_ImageFormat)
│   │   └── MoeUtils.h      # 工具函数声明（文件操作、路径处理）
│   ├── src/
│   │   ├── MoePack.cpp     # 封包器实现
│   │   ├── cmd.cpp         # CLI 命令实现 + main()
│   │   └── MoeUtils.cpp    # 工具函数实现
│   └── CMakeLists.txt
├── MoeUnpack/              # 解包库（支持头文件引入或独立编译）
│   ├── MoeHeader.h         # MOE 文件头结构 + 字节序工具 + 音频格式检测
│   ├── MoeUnpack.h         # 解包 API（#define MOE_UNPACK_IMPLEMENTATION 后引入实现）
│   ├── MoeStreamReader.h   # 流式解包读取器声明
│   ├── MoeStreamReader.cpp # 流式解包读取器实现
│   ├── MoeSharedStreamReader.h  # 共享流式读取器声明
│   ├── MoeSharedStreamReader.cpp # 共享流式读取器实现
│   ├── MoeUnpack.cpp       # 编译库入口（包含 MoeUnpack.h 实现）
│   └── CMakeLists.txt
├── libs/
│   ├── KTX-Software/       # KTX2 纹理压缩库 (git submodule)
│   └── stb/                # stb_image 图片解码 (git submodule)
├── test_tool/              # 渲染测试工具
└── README.md
```

## 工作流程

### 图片封包 (pack)
```
图片文件 (png/bmp/jpg/tga) → stb_image 解码 → KTX2 GPU 压缩 (Basis Universal)
→ ZSTD 压缩（可选）→ XSalsa20-Poly1305 加密（可选）→ 写入 .moe
```

### 图片解包 (unpack)
```
.moe 文件 → 解析 MoeHeader → 解密（可选）→ ZSTD 解压（可选）
→ KTX2 容器验证 → 输出 .ktx2 文件
```

### 非图片封包 (pack_ex)
```
原始文件 → ZSTD 压缩（可选）→ XSalsa20-Poly1305 加密（可选）→ 写入 .moe
```

### 非图片解包 (unpack_ex)
```
.moe 文件 → 解析 MoeHeader → 解密（可选）→ ZSTD 解压（可选）→ 输出原始文件
```

### 流式加密封包 (pack_ex_stream)
```
原始文件 → 检测音频格式 (WAV/FLAC/MP3/VORBIS) → 分块 Chacha20-Poly1305 加密 → 写入 .moe
```

### 流式解包 (MoeStreamReader)
```
.moe 文件 → 解析 MoeHeader → 密钥派生 → 逐块读取解密 (read_chunk) → 音频引擎边读边播
```

## 依赖

封包工具依赖：
- [stb](https://github.com/nothings/stb) — 图片解码
- [KTX-Software](https://github.com/KhronosGroup/KTX-Software) — KTX2 纹理压缩 (含 Basis Universal)
- [libsodium](https://github.com/jedisct1/libsodium) — 加密 (XSalsa20-Poly1305 / Chacha20-Poly1305 / Argon2id)
- [zstd](https://github.com/facebook/zstd) — 数据压缩

解包库依赖：
- [libsodium](https://github.com/jedisct1/libsodium)
- [zstd](https://github.com/facebook/zstd)
- [KTX-Software](https://github.com/KhronosGroup/KTX-Software)（仅 MOE_UNPACK_DEBUG=ON 时需要）

## 编译选项

| 选项 | 默认值 | 说明 |
|------|--------|------|
| MOE_BUILD_STATIC | ON | ON=静态库，OFF=动态库 |
| MOE_BUILD_PACK | OFF | ON=同时编译 MoePack（封包工具），OFF=仅编译 MoeUnpack |
| MOE_UNPACK_DEBUG | ON | ON=解包时使用 KTX 库验证数据完整性 |

默认只编译 MoeUnpack（解包库），适合直接集成到游戏引擎。封包操作使用 CLI 工具完成。

```bash
# 仅编译解包库（默认）
cmake -B build

# 编译封包工具 + 解包库（静态链接）
cmake -B build -DMOE_BUILD_PACK=ON

# 编译封包工具 + 解包库（动态链接）
cmake -B build -DMOE_BUILD_PACK=ON -DMOE_BUILD_STATIC=OFF
```

KTX-Software 和 stb 需通过 git submodule 拉取，其余依赖通过 vcpkg 管理。

## 使用

### MoeUnpack（解包库）

支持两种集成方式：头文件引入或链接编译产物。

**方式一：头文件引入**

```cpp
#define MOE_UNPACK_IMPLEMENTATION
#include "MoeUnpack.h"

int main() {
    // 图片解包（自动解密/解压 + KTX2 验证）
    MoeUnpack::unpack("path/to/file.moe", "output/");

    // 非图片解包（不做 KTX2 验证，直接返回原始数据）
    MoeUnpack::unpack_ex("path/to/audio.moe", "output/");

    // 内存版本
    int size;
    unsigned char* data = MoeUnpack::unpack("path/to/file.moe", size, "password");
    // 使用 data[0..size-1] ...
    free(data);
}
```

**方式二：链接库**

```cmake
target_link_libraries(your_target PRIVATE MoeUnpack)
```

```cpp
#include "MoeUnpack.h"
// 无需 #define MOE_UNPACK_IMPLEMENTATION
MoeUnpack::unpack("file.moe", "output/");
```

如果不需要 KTX2 完整性检查，编译时设置 `MOE_UNPACK_DEBUG=OFF`；使用头文件方式时可定义 `MOE_UNPACK_NO_KTX2_CHECK` 宏。

### MoeStreamReader（流式解包）

用于流式读取分块加密的 .moe 文件（由 pack_ex_stream 生成），适合音频引擎边读边播：

```cpp
#include "MoeStreamReader.h"

MoeStreamReader reader;
if (!reader.open("audio.moe", "password")) {
    // 打开失败，通过 reader.error() 获取错误原因
    return;
}

std::vector<uint8_t> buf(reader.header().chunk_size);
while (!reader.is_eof()) {
    size_t len = reader.read_chunk(buf.data(), buf.size());
    // 将 buf[0..len-1] 送入音频解码器播放
}
reader.close();
```

### MoePack（封包工具）

MoePack 是声明/实现分离的库，编译时通过 `MOE_BUILD_PACK=ON` 开启。目前仅支持 Windows。

```cpp
#include "MoePack.h"

MoePack packer;
packer.set_dst_platform(MOE_Platform::WINDOWS);

PackParams params;
params.ztsd_on = true;
params.ztsd_level = 15;
params.is_encryption = true;
params.encryption_key = "mykey";
packer.set_pack_params(params);

// 图片封包
packer.pack("input.png", "output.moe");

// 非图片封包
packer.pack_ex("input.mp3", "output.moe");

// 流式加密封包（大音频文件）
packer.pack_ex_stream("input.flac", "output.moe");
```

### 命令行工具

```
pack        -i <输入> -p <平台> [-o <输出>] [-z <等级>] [-k <密钥>]  图片封包
pack_ex     -i <输入> [-o <输出>] [-z <等级>] [-k <密钥>]            非图片封包
pack_ex_stream -i <输入> [-o <输出>] [-k <密钥>] [-s <块大小>]      流式加密封包
unpack      -i <输入> [-o <输出>] [-k <密钥>]                       图片解包
unpack_ex   -i <输入> [-o <输出>] [-k <密钥>]                       非图片解包
```

示例：

```bash
# 图片封包（指定平台 + 压缩 + 加密）
pack -i D:\textures -p WINDOWS -o D:\output -z 17 -k mykey

# 非图片封包（仅加密，不压缩）
pack_ex -i D:\audio\bgm.mp3 -o D:\output -k mykey

# 流式加密封包（分块加密，适合大音频）
pack_ex_stream -i D:\audio\bgm.flac -o D:\output -k mykey -s 131072

# 图片解包
unpack -i D:\output\image.moe -o D:\unpacked -k mykey

# 非图片解包
unpack_ex -i D:\output\bgm.moe -o D:\unpacked -k mykey
```

解包时自动根据文件头中的标志位判断是否需要解密/解压，无需手动指定。使用 `-h` 查看完整帮助。

## 后续计划

- ~~音频资源封包与解包~~ (已完成)
- ~~分块加密与流式解包~~ (已完成)
- 提供 GPU 纹理压缩的高级参数设置
- 支持更多加密方式
- 支持更多 GPU 压缩格式
- 跨平台封包工具支持

## 留言

这来自我的 galgame 项目资源预处理工具的升级版。我计划重构视觉小说项目并打造一个完整的视觉小说引擎，这会是一个漫长的项目。

接下来会尝试开源引擎的音频库。因个人时间精力有限，没有做完整测试，如有问题欢迎通过 issue 反馈。

本项目永远免费开源，欢迎使用与贡献。
