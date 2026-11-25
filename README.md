# Sound Physics for Bedrock

[日本語](README.ja.md) | [简体中文](README.zh-CN.md) | [繁體中文](README.zh-TW.md)

Realistic sound attenuation, reverberation and absorption through blocks for Minecraft: Bedrock Edition. A [LeviLamina](https://github.com/LiteLDev/LeviLamina) port of the Java Edition mod [Sound Physics Remastered](https://github.com/henkelmax/sound-physics-remastered) by Max Henkel, based on Sound Physics by Sonic Ether and Sound Physics Fabric by vlad2305m.

> This is an unofficial port, not affiliated with the authors of the original mods, FMOD or Firelight Technologies.

## Requirements

|  | Version |
|---|---|
| Minecraft: Bedrock Edition | 1.26.60 (Windows) |
| LeviLamina | 26.60.x |

## Installation

**lip**

```
lip install github.com/ch1-k-mc/SoundPhysics-Bedrock
```

Run it in the game directory of your LeviLamina client.

**Manual**: download `SoundPhysics-client-windows-x64.zip` from [Releases](../../releases) and extract the `SoundPhysics` folder into `mods/`.

## Usage

Install it and play. Sounds in the world are processed automatically; menu sounds and music are not changed.

## Features

- Sounds behind walls are muffled, depending on the blocks in between
- Reverb that depends on the size of the space and the blocks around you (stone echoes, wool absorbs)
- Sounds that come through an opening are heard from the direction of the opening
- Muffled sounds underwater
- Air absorption of high frequencies over distance
- The same settings and default values as the original

## Settings

There is no settings screen yet. Edit the files in `mods/SoundPhysics/config/` and restart the game. The keys are the same as in the original mod.

| File | Contents |
|---|---|
| `soundphysics.properties` | General settings, for example `reverb_gain`, `block_absorption`, `sound_distance_allowance` |
| `reflectivity.properties` | How strongly blocks reflect sound. Keys are sound types (`STONE`), block IDs (`minecraft:stone`) or block tags (`#minecraft:logs`) |
| `occlusion.properties` | How strongly blocks muffle sound, with the same keys |
| `sound_rates.properties` | Sounds to skip (`0`) or limit per tick. Rain and thunder are skipped by default |

## Known limitations

- The reverb is a new implementation that takes the same parameters as the original, so it does not sound exactly the same
- Bedrock Edition has no sound types for blocks. Blocks are matched to the original's sound types by their names, so a few blocks may have different values
- Sounds the server does not send cannot be heard from farther away
- No settings screen and no debug rendering yet. Simple Voice Chat integration is not available

## Building

Requires [xmake](https://xmake.io), Visual Studio 2022 or later, and clang-cl.

```
xmake f -m release -y
xmake
```

The mod is output to `bin/SoundPhysics/`.

## License

[GPL-3.0](LICENSE), the same as the original. See [NOTICE](NOTICE.md).
