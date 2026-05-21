# MoePack_tool
这是一个用于封包游戏资源的工具，它还带有一个轻量的解包工具
它可以把图片资源封包成对应目标平台的gpu压缩格式（使用ktx2容器），从而平衡游戏资源体积与加载速度。
对于非图片资源（MP3/FLAC/OGG等音频文件、或其他二进制文件），提供 pack_ex / unpack_ex 进行加密封包。

## 功能
- 封包图片资源（解码→KTX2 GPU压缩→加密/压缩→.moe）
- 解包图片资源（.moe→解密/解压→KTX2验证→输出）
- 封包非图片资源（原始文件→加密/压缩→.moe）
- 解包非图片资源（.moe→解密/解压→输出原始文件）

## 工具的工作流程
### 图片封包流程(pack)
解码图片（stb）->压缩图片（basisu）->打包成ktx2容器（ktx2）->ztsd压缩（可选）->加密（sodium 可选）->写入封包文件（.moe）

### 图片解包流程(unpack)
读取封包文件（.moe）->解密（sodium 可选）->ztsd解压（可选）->ktx2容器验证（ktx2）->输出.ktx2文件

### 非图片封包流程(pack_ex)
读取原始文件 -> ztsd压缩（可选）->加密（sodium 可选）->添加MOE文件头->写入封包文件（.moe）

### 非图片解包流程(unpack_ex)
读取封包文件（.moe）->解密（sodium 可选）->ztsd解压（可选）->输出原始文件（无扩展名）

## 依赖
封包工具依赖以下开源库：
- [stb](https://github.com/nothings/stb)
- [ktx2](https://github.com/KhronosGroup/KTX-Software)
- [sodium](https://github.com/jedisct1/libsodium)
- [ztsd](https://github.com/facebook/zstd)
- [basisu](https://github.com/BinomialLLC/basis_universal)

解包工具依赖以下开源库：
- [ktx2](https://github.com/KhronosGroup/KTX-Software)（仅在 MOE_UNPACK_CHECK_DATA 时需要）
- [sodium](https://github.com/jedisct1/libsodium)
- [ztsd](https://github.com/facebook/zstd)

## 编译选项

| 选项 | 默认值 | 说明 |
|------|--------|------|
| MOE_BUILD_STATIC | ON | ON=静态库，OFF=动态库 |
| MOE_BUILD_PACK | OFF | ON=同时编译封包库和封包工具，OFF=仅编译解包库 |
| MOE_UNPACK_DEBUG | ON | ON=解包时使用ktx库验证数据完整性 |

默认情况下只编译 MoeUnpack（解包库），适合直接集成到游戏引擎中使用。
封包操作可使用封包工具完成，无需在引擎中链接封包库。

示例：
```bash
# 仅编译解包库（默认）
cmake -B build

# 编译封包工具+解包库（静态链接）
cmake -B build -DMOE_BUILD_PACK=ON

# 编译封包工具+解包库（动态链接）
cmake -B build -DMOE_BUILD_PACK=ON -DMOE_BUILD_STATIC=OFF
```

除了ktx2和stb需要拉取子模块，其他依赖均使用vcpkg管理。
或许你也可以全部使用vcpkg进行管理或使用git拉取子模块。

## 测试
这里提供了一个测试工具，位于test_tool目录下，用它打开你解包出来的文件会进行渲染，用于测试文件是否正常


## 使用
MoePack和MoeUnpack是相互独立的项目。
这边推荐使用这个工具进行封包操作，而解包操作可以直接集成到你的项目，因为MoeUnpack其实是一个头文件库。

### 使用MoeUnpack
MoeUnpack是一个头文件库，只需要包含MoeUnpack.h和使用#define MOE_UNPACK_IMPLEMENTATION 即可。（前提是确保你的环境内已配置ztsd,ktx2,sodium）
如果你不需要检查ktx2容器的完整性，cmake使用MOE_UNPACK_DEBUG=OFF。
当然，直接使用头文件的话，不要设置MOE_UNPACK_NO_KTX2_CHECK宏。
- 简单示例如下：
```cpp
#define MOE_UNPACK_IMPLEMENTATION
#include "MoeUnpack.h"
int main(){
    // 解包图片资源（会进行KTX2验证）
    MoeUnpack::unpack("path/to/your/file.moe","output/directory/");

    // 解包非图片资源（不做KTX2验证，直接返回原始数据）
    MoeUnpack::unpack_ex("path/to/your/audio.moe","output/directory/");

    return 0;
}
```

### 使用MoePack
你可以直接使用这个工具进行封包，或者把MoePack集成到你的项目中。
这是一个声明与实现分离的库，你需要链接MoePack库文件。
编译时通过 MOE_BUILD_PACK=ON 开启封包模块。
注意：封包工具并不是跨平台的，目前只支持windows，或许在未来会支持更多平台。

### 工具使用
MoePack提供了命令行工具，可以直接在终端中使用。
支持以下命令：

- **pack** - 图片资源封包（需要 -p，默认压缩等级15）
- **pack_ex** - 非图片资源封包（无需 -p，默认不压缩仅加密）
- **unpack** - 图片资源解包
- **unpack_ex** - 非图片资源解包

解包时会自动根据文件头中的标志位判断是否需要解密/解压，无需手动指定。

具体可以使用 -h 命令查看帮助信息。

示例：
```bash
# 图片封包（默认压缩）
pack -i D:\textures -p WINDOWS -o D:\output -z 17 -k mykey

# 非图片封包（默认不压缩，仅加密）
pack_ex -i D:\audio\bgm.mp3 -o D:\output -k mykey

# 图片解包
unpack -i D:\output\image.moe -o D:\unpacked -k mykey

# 非图片解包
unpack_ex -i D:\output\bgm.moe -o D:\unpacked -k mykey
```

## 后期目标

- ~~支持音频资源的封包与解包~~ (已完成: pack_ex / unpack_ex)
- 提供gpu纹理压缩的高级参数设置
- 支持更多的加密方式
- 支持更多的GPU压缩格式

## 留言
这其实是来自我的一个galgame项目使用的资源预处理工具的升级版，虽然目前功能还比较简单，但我会持续更新它。
我打算把以前开发的视觉小说的项目进行重构，并打造出一个视觉小说引擎，或许这会是一个十分漫长的项目。
接下来会尝试开源引擎的音频库，或许这个项目的更新会比较慢。
我因为个人时间精力有限，没有做完整的测试，如果有任何问题其反馈，我会尽力处理的。

如果你喜欢这个项目，有任何建议或意见，欢迎通过issue与我交流。
这个项目将永远免费开源，欢迎大家使用与贡献。