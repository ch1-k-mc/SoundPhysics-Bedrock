# Sound Physics for Bedrock

[English](README.md) | [日本語](README.ja.md) | [繁體中文](README.zh-TW.md)

为 Minecraft 基岩版带来真实的声音衰减、混响以及方块对声音的吸收。这是 Max Henkel 的 Java 版模组 [Sound Physics Remastered](https://github.com/henkelmax/sound-physics-remastered)（基于 Sonic Ether 的 Sound Physics 和 vlad2305m 的 Sound Physics Fabric）的 [LeviLamina](https://github.com/LiteLDev/LeviLamina) 移植版。

> 这是非官方移植，与原模组作者、FMOD 及 Firelight Technologies 无关。

## 运行环境

|  | 版本 |
|---|---|
| Minecraft 基岩版 | 1.26.60（Windows） |
| LeviLamina | 26.60.x |

## 安装

**lip**

```
lip install github.com/ch1-k-mc/SoundPhysics-Bedrock
```

请在 LeviLamina 客户端的游戏目录中运行。

**手动**：从 [Releases](../../releases) 下载 `SoundPhysics-client-windows-x64.zip`，将 `SoundPhysics` 文件夹解压到 `mods/`。

## 使用方法

安装即可使用。世界中的声音会被自动处理，菜单音效和音乐不会改变。

## 功能

- 墙后的声音会根据中间的方块变得沉闷
- 根据空间大小和周围方块产生混响（石头回声大，羊毛吸音）
- 穿过开口传来的声音会从开口的方向听到
- 水下声音变得沉闷
- 随距离产生的空气高频吸收
- 与原模组相同的设置项和默认值

## 设置

目前还没有设置界面。请编辑 `mods/SoundPhysics/config/` 中的文件并重新启动游戏。键名与原模组相同。

| 文件 | 内容 |
|---|---|
| `soundphysics.properties` | 常规设置，例如 `reverb_gain`、`block_absorption`、`sound_distance_allowance` |
| `reflectivity.properties` | 方块反射声音的强度。键为声音类型（`STONE`）、方块 ID（`minecraft:stone`）或方块标签（`#minecraft:logs`） |
| `occlusion.properties` | 方块阻挡声音的强度，键相同 |
| `sound_rates.properties` | 不处理的声音（`0`）或每 tick 的上限。默认不处理雨声和雷声 |

## 已知限制

- 混响是使用与原模组相同参数的新实现，音色与原模组不完全相同
- 基岩版没有方块的声音类型，因此根据方块名称对应到原模组的声音类型，部分方块的数值可能不同
- 服务器未发送的声音无法从更远处听到
- 暂无设置界面和调试显示，不支持 Simple Voice Chat 联动

## 构建

需要 [xmake](https://xmake.io)、Visual Studio 2022 或更高版本以及 clang-cl。

```
xmake f -m release -y
xmake
```

输出到 `bin/SoundPhysics/`。

## 许可证

与原模组相同，采用 [GPL-3.0](LICENSE)。另请参阅 [NOTICE](NOTICE.md)。
