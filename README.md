# MoePack_tool_
这是一个用于封包游戏资源的工具，它还带有一个轻量的解包工具
它可以把把图片资源封包成对应目标平台的gpu压缩格式（使用ktx2容器），从而平衡游戏资源体积与加载速度。
## 功能
- 封包游戏资源文件
- 解包游戏资源文件

## 工具的工作流程
### 封包流程
解码图片（stb）->压缩图片（basisu）->打包成ktx2容器（ktx2）->ztsd压缩（可选）->加密（sodium 可选）->写入封包文件（.moe）
### 解包流程
读取封包文件（.moe）->解密（sodium 可选）->ztsd解压（可选）->ktx2容器验证（ktx2）

## 依赖
封包工具依赖以下开源库：
- [stb](https://github.com/nothings/stb)
- [ktx2](https://github.com/KhronosGroup/KTX-Software)
- [sodium](https://github.com/jedisct1/libsodium)
- [ztsd](https://github.com/facebook/zstd)
- [basisu](https://github.com/BinomialLLC/basis_universal)

解包工具依赖以下开源库：
- [ktx2](https://github.com/KhronosGroup/KTX-Software)
- [sodium](https://github.com/jedisct1/libsodium)
- [ztsd](https://github.com/facebook/zstd)

## 编译
除了ktx2和stb需要拉取子模块，其他依赖均使用vcpkg管理
或许你也可以全部使用vcpkg进行管理或使用git拉取子模块


## 使用
MoePack和MoeUnpack是相互独立的项目。
这边推荐使用这个工具进行封包操作，而解包操作可以直接集成到你的项目，因为MoeUnpack其实是一个头文件库。

### 使用MoeUnpack
MoeUnpack是一个头文件库，只需要包含MoeUnpack.h和使用#define MOE_UNPACK_IMPLEMENTATION 即可。（前提是确保你的环境内已配置ztsd,ktx2,sodium）
简单示例如下：
```cpp
#define MOE_UNPACK_IMPLEMENTATION
#include "MoeUnpack.h"
int main(){
	MoeUnpack::unpack("path/to/your/file.moe","output/directory/");
	return 0;
}
```
### 使用MoePack
你可以直接使用这个工具进行封包，或者把MoePack集成到你的项目中。
这是一个声明与实现分离的库，你需要链接MoePack库文件。
或者通过设置BUILD_MOEUNPACK的值，把MoeUnpack从项目中移除。
注意：封包工具并不是跨平台的，目前只支持windows，或许在未来会支持更多平台。

### 工具使用
MoePack和MoeUnpack都提供了命令行工具，可以直接在终端中使用。
具体可以使用 -h 命令查看帮助信息。

## 后期目标

- 支持音频资源的封包与解包
- 提供gpu纹理压缩的高级参数设置
- 支持更多的加密方式
- 支持更多的GPU压缩格式

## 留言
这其实是来自我的一个galgame项目使用的资源预处理工具的升级版，虽然目前功能还比较简单，但我会持续更新它。
我打算把以前开发的视觉小说的项目进行重构，并打造出一个视觉小说引擎，或许这会是一个十分漫长的项目。
接下来会尝试开源引擎的音频库，或许这个项目的更新会比较慢。

如果你喜欢这个项目，有任何建议或意见，欢迎通过issue与我交流。
这个项目将永远免费开源，欢迎大家使用与贡献。