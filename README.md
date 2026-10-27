# Nobihaza 2 Remake

这是通过 **Vibe Coding** 方式，使用 **C++20 / SFML** 重构的《野比大雄的生化危机2》（**野生2**）。本仓库包含当前完整的游戏实现、七份预制内容 include、原文测试基准、开发目标和测试，无需私有内容输入即可编译 `nobihaza` 游戏。图片、音乐、地图、字体等预制运行资源通过公开 Release 单独分发，可用一条命令恢复。

项目实现和文档适用 [LICENSE](LICENSE) 中限定范围的 MIT 许可。原游戏及翻译内容、生成的事件程序、原文测试基准、角色、音乐、图片、字体、依赖和商标保留各自的归属与条款，详见 [THIRD-PARTY-NOTICES.md](THIRD-PARTY-NOTICES.md)。公开这些文件不会将原作改授 MIT 许可，也不代表已建立完整的第三方授权链。

## 编译

需要支持 C++20 的编译器、CMake >=3.28、Ninja 和 Python 3。除非提供匹配的本地 FetchContent 源码目录覆盖设置，CMake 会下载固定版本的 SFML 3.1.0、nlohmann/json 3.12.0 及其依赖。本仓库不包含依赖缓存。

macOS Apple Silicon：

```sh
cmake -S . -B build/local -G Ninja -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_OSX_ARCHITECTURES=arm64 -DCMAKE_OSX_DEPLOYMENT_TARGET=13.0 \
  -DNOBIHAZA_BUILD_TESTS=ON
cmake --build build/local --target nobihaza
```

也可使用 `cmake --preset mac-release` 和 `cmake --build --preset mac-release` 编译游戏。首房间实验和工具链探针仍作为独立目标保留。Windows 预设沿用 MSVC/Ninja 路线，需要在 x64 开发者命令行中运行；本次源码发布尚未验证 Windows。macOS 13.0 是部署目标，不代表已在所有支持的系统版本上测试。当前源码构建已在 Apple Silicon 上检查；这不是已签名或公证的应用发行包。

## 恢复运行资源并启动

在仓库根目录运行：

```sh
python3 tools/fetch_runtime.py --destination assets/runtime
./build/local/bin/nobihaza assets/runtime/content.json --entry title
```

运行资源解压后约 1.74 GB，下载的 ZIP 分卷合计约 1.11 GB；拼接和解压还需要额外临时磁盘空间。[runtime-manifest.json](runtime-manifest.json) 固定了 Release 下载地址、全部 17 个分卷的大小与 SHA-256、拼接后 ZIP 的摘要，以及解压后全部 6,198 个文件的摘要（包括运行资源和许可说明）。`tools/fetch_runtime.py` 只使用 Python 标准库。已通过校验的分卷缓存在 `.runtime-downloads` 中，中断后可复用。工具会拒绝被修改的数据、不安全的 ZIP 路径、链接、缺失或多余的文件，以及已存在的目标目录。再次恢复时请选一个新目录；工具不会覆盖已有运行资源或存档目录。若缓存分卷损坏，会明确报告需要删除后重试的文件路径。

显式传入 `content.json` 的启动方式不依赖 `.app` 的资源布局。打包后的 Mac `.app` 也可使用自身的 `Contents/Resources/runtime/content.json`，但本源码构建不会自动创建该资源布局。存档沿用当前平台的默认保存位置；可传入 `--save-dir /path/of/your/choice`，为试玩指定独立存档目录。

## 检查与内容准备

```sh
python3 tests/test_fetch_runtime.py
python3 tests/test_prepare_public_source.py
python3 tests/test_public_source_lower.py
cmake --build build/local --target room_map_checks g7_score_rules
./build/local/room_map_checks
./build/local/g7_score_rules
```

原有 CMake 生产目标与开发、测试目标均保留。合成数据规则检查不需要原作素材。历史集成检查仍在 CMake 中引用原有的私有证据、场景和存档测试数据；这些证据目录及真实用户存档不公开，因此直接运行整套 `ctest` 不是受支持的独立验收方式。需要运行资源的检查，可使用恢复后的资源包，并按相应测试源码规定的参数单独执行。原文测试基准包含在 `tests/fixtures/original-text.json` 中，其固定摘要会在配置阶段校验；使用这些基准的检查不会默默接受缺失或被修改的数据。

七份已审定的 `.inc` 文件已足以支持正常编译。`tools/public_source_lower.py` 和 `tools/public-compile-contract.json` 保留现有的可选固定输入再生成边界，并不构成完整的原始 RPG Maker 工程导入器。本仓库不包含运行资源制作工具、原始作者工程、私有参考与证据历史、正式存档或凭据。`PUBLIC-SOURCE-MANIFEST.json` 记录已审定导出的来源与摘要；导出工具供维护者配合原准备目录使用，不能代替游戏构建命令。

项目采用 Vibe Coding 开发方式，包含 Codex 辅助实现。源码构建成功、运行资源恢复校验通过，不能代替完整玩法验收、Windows 测试或公证发行。
