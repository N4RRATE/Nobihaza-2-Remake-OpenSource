# 源码与运行资源归属

项目手写实现和文档适用限定范围的项目 MIT 许可。该许可不包含原游戏及翻译表达、生成的内容程序、原文测试基准、游戏角色、商标、图片、地图、音频、字体和第三方依赖。在这里公开这些内容，不会改变其许可，也不代表已取得全部第三方授权。

本源码树中包含原作表达的文件如下：

- `src/game/dekisugi_program.inc`
- `src/game/dora_program.inc`
- `src/game/cash_program.inc`
- `src/game/warehouse_g4_program.inc`
- `src/game/final_boss_program.inc`
- `tools/g3-route-doors.inc`
- `tools/g3-access-cold-room.inc`
- `tests/fixtures/original-text.json`

这些文件由原游戏、事件或翻译内容准备而来，保留相应原作者的权利。收录它们是为了保留当前实现的实际行为和测试预期；它们不属于按 MIT 许可授予的原创手写引擎代码。

CMake 会获取 SFML 3.1.0（commit `8835b6b955c96fa1ad1278a8dae6f05c4143effc`，zlib/png 许可）和 nlohmann/json 3.12.0（MIT）。它们的源码包含各自的版权和许可文本。SFML 的依赖可能包括 FreeType、HarfBuzz、SheenBidi、Ogg、Vorbis、FLAC 及采用其他许可的内嵌头文件。本仓库不包含这些依赖的源码或构建缓存。分发链接了这些依赖的二进制文件时，应保留实际适用的依赖声明。

运行资源 Release 包含实际的 `licenses/` 目录、保留的原始声明和资源清单。复制运行资源时，请一并保留该目录。原作音乐和录音仍是独立作品。Noto CJK 字体使用 SIL Open Font License；音色库和其他随包组件保留各自附带的条款。音色库许可不授予原作乐曲或录音的权利。请阅读实际保留的声明，不要用这份简要索引代替它们。
