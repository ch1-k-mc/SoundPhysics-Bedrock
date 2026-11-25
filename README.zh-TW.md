# Sound Physics for Bedrock

[English](README.md) | [日本語](README.ja.md) | [简体中文](README.zh-CN.md)

為 Minecraft 基岩版帶來真實的聲音衰減、殘響以及方塊對聲音的吸收。這是 Max Henkel 的 Java 版模組 [Sound Physics Remastered](https://github.com/henkelmax/sound-physics-remastered)（基於 Sonic Ether 的 Sound Physics 與 vlad2305m 的 Sound Physics Fabric）的 [LeviLamina](https://github.com/LiteLDev/LeviLamina) 移植版。

> 這是非官方移植，與原模組作者、FMOD 及 Firelight Technologies 無關。

## 執行環境

|  | 版本 |
|---|---|
| Minecraft 基岩版 | 1.26.60（Windows） |
| LeviLamina | 26.60.x |

## 安裝

**lip**

```
lip install github.com/ch1-k-mc/SoundPhysics-Bedrock
```

請在 LeviLamina 用戶端的遊戲目錄中執行。

**手動**：從 [Releases](../../releases) 下載 `SoundPhysics-client-windows-x64.zip`，將 `SoundPhysics` 資料夾解壓縮到 `mods/`。

## 使用方法

安裝即可使用。世界中的聲音會被自動處理，選單音效與音樂不會改變。

## 功能

- 牆後的聲音會依中間的方塊變得悶悶的
- 依空間大小與周圍方塊產生殘響（石頭回音大，羊毛吸音）
- 穿過開口傳來的聲音會從開口的方向聽到
- 水下聲音變得悶悶的
- 隨距離產生的空氣高頻吸收
- 與原模組相同的設定項目與預設值

## 設定

目前還沒有設定畫面。請編輯 `mods/SoundPhysics/config/` 中的檔案並重新啟動遊戲。鍵名與原模組相同。

| 檔案 | 內容 |
|---|---|
| `soundphysics.properties` | 一般設定，例如 `reverb_gain`、`block_absorption`、`sound_distance_allowance` |
| `reflectivity.properties` | 方塊反射聲音的強度。鍵為聲音類型（`STONE`）、方塊 ID（`minecraft:stone`）或方塊標籤（`#minecraft:logs`） |
| `occlusion.properties` | 方塊阻擋聲音的強度，鍵相同 |
| `sound_rates.properties` | 不處理的聲音（`0`）或每 tick 的上限。預設不處理雨聲與雷聲 |

## 已知限制

- 殘響是使用與原模組相同參數的新實作，音色與原模組不完全相同
- 基岩版沒有方塊的聲音類型，因此依方塊名稱對應到原模組的聲音類型，部分方塊的數值可能不同
- 伺服器未傳送的聲音無法從更遠處聽到
- 尚無設定畫面與除錯顯示，不支援 Simple Voice Chat 連動

## 建置

需要 [xmake](https://xmake.io)、Visual Studio 2022 或更新版本以及 clang-cl。

```
xmake f -m release -y
xmake
```

輸出到 `bin/SoundPhysics/`。

## 授權

與原模組相同，採用 [GPL-3.0](LICENSE)。另請參閱 [NOTICE](NOTICE.md)。
