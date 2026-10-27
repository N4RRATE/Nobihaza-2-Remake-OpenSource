# 野生2 · C++ 重构版

通过 **Vibe Coding**，使用 **C++20 / SFML** 重构《野比大雄的生化危机2》（野生2），以还原原版剧情与玩法为目标。本仓库提供完整游戏源码，图片、音乐、地图等运行资源单独下载。

## 平台

当前支持 **macOS Apple Silicon**。Windows 版本尚未验证。

## 获取游戏

v1.0.0 将提供 **DMG 拖拽安装版**和 **ZIP 免安装轻量版**，均包含完整游戏及运行资源，仅支持 macOS arm64。发行包及源码运行资源将通过 [Releases](https://github.com/N4RRATE/Nobihaza-2-Remake-OpenSource/releases) 提供，目前尚未开放下载。

## 编译与运行

需要 C++20 编译器、CMake 3.28+、Ninja 和 Python 3；首次编译会下载依赖。资源上传完成后，在仓库根目录执行：

```sh
cmake --preset mac-release
cmake --build --preset mac-release
python3 tools/fetch_runtime.py --destination assets/runtime
./build/mac-release/bin/nobihaza assets/runtime/content.json --entry title
```

## 许可

项目自写代码采用 [MIT](LICENSE)。原作内容及第三方资源保留各自的归属与许可，详见 [第三方说明](THIRD-PARTY-NOTICES.md)。
