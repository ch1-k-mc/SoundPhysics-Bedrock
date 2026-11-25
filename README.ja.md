# Sound Physics for Bedrock

[English](README.md) | [简体中文](README.zh-CN.md) | [繁體中文](README.zh-TW.md)

Minecraft 統合版で、ブロックによる音の遮蔽・残響・吸収を再現します。Max Henkel 氏の Java 版 Mod [Sound Physics Remastered](https://github.com/henkelmax/sound-physics-remastered)（Sonic Ether 氏の Sound Physics、vlad2305m 氏の Sound Physics Fabric が元）の [LeviLamina](https://github.com/LiteLDev/LeviLamina) 移植です。

> 非公式の移植です。本家の作者、FMOD、Firelight Technologies とは関係ありません。

## 動作環境

|  | 版 |
|---|---|
| Minecraft 統合版 | 1.26.60（Windows） |
| LeviLamina | 26.60.x |

## 導入

**lip**

```
lip install github.com/ch1-k-mc/SoundPhysics-Bedrock
```

LeviLamina のクライアントのゲームフォルダで実行してください。

**手動**: [Releases](../../releases) から `SoundPhysics-client-windows-x64.zip` をダウンロードし、`SoundPhysics` フォルダを `mods/` に展開してください。

## 使い方

入れるだけで動きます。ワールドの中の音が自動で処理されます。メニューの音と音楽は変わりません。

## 機能

- 壁の向こうの音が、間にあるブロックに応じてこもって聞こえる
- 空間の広さと周りのブロックに応じた残響（石はよく響き、羊毛は吸う）
- 開口部を通ってきた音は、開口部の方向から聞こえる
- 水中では音がこもる
- 距離に応じた空気による高音の吸収
- 本家と同じ設定項目と既定値

## 設定

設定画面はまだありません。`mods/SoundPhysics/config/` のファイルを編集し、ゲームを起動し直してください。キーは本家と同じです。

| ファイル | 内容 |
|---|---|
| `soundphysics.properties` | 全体の設定。例: `reverb_gain`、`block_absorption`、`sound_distance_allowance` |
| `reflectivity.properties` | ブロックが音を反射する強さ。キーは音の種類（`STONE`）、ブロック ID（`minecraft:stone`）、ブロックのタグ（`#minecraft:logs`） |
| `occlusion.properties` | ブロックが音をさえぎる強さ。キーは同じ |
| `sound_rates.properties` | 処理しない音（`0`）や、tick ごとの上限。雨と雷は既定で処理しない |

## 既知の制限

- 残響は本家と同じパラメータを受ける新しい実装なので、音色は本家と同じではありません
- 統合版にはブロックの音の種類が無いため、ブロック名から本家の音の種類に当てはめています。一部のブロックは値が異なることがあります
- サーバーが送らない音は、遠くからでも聞こえるようにはなりません
- 設定画面とデバッグ表示はまだありません。Simple Voice Chat との連携はありません

## ビルド

[xmake](https://xmake.io)、Visual Studio 2022 以降、clang-cl が必要です。

```
xmake f -m release -y
xmake
```

`bin/SoundPhysics/` に出力されます。

## ライセンス

本家と同じ [GPL-3.0](LICENSE) です。[NOTICE](NOTICE.md) も参照してください。
