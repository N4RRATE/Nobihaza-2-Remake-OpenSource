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

## 键位

| 按键 | 操作 |
|---|---|
| W / A / S / D | 移动、选择菜单项目 |
| Z | 调查、确认、推进对白 |
| X | 收起武器时打开菜单；界面中取消/返回（允许时） |
| 鼠标右键 | 举起主武器；举起后短按并松开放下 |
| Shift + 鼠标右键 | 在未举武器时举起已装备的副武器 |
| 鼠标左键 | 攻击；匕首等副武器需每次重新按下 |
| 举枪时停步，短按 X 后松开 | 装填弹药 |
| 按住右键 + W / A / S / D | 原地转向 |
| 停步并持续按住右键 | 瞄准（支持该动作的武器） |
| 举枪时按住 X + W / A / S / D | 保持朝向移动 |

先在菜单装备武器，再举起攻击；主副武器切换前先放下。挣脱、起身等特殊操作按场景提示进行。当前键位固定，游戏内键位说明与自定义改键留待后续版本。

## 许可

项目自写代码采用 [MIT](LICENSE)。原作内容及第三方资源保留各自的归属与许可，详见 [第三方说明](THIRD-PARTY-NOTICES.md)。
