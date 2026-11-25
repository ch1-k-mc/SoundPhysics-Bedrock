# Notice

This project is an unofficial port of **Sound Physics Remastered** to Minecraft: Bedrock Edition, not affiliated with the authors of the original mods.

## License

Everything is under the [GNU General Public License v3.0](LICENSE), the same as the original.

## Original work

- **Sound Physics Remastered** by Max Henkel (with contributions by Saint), [GPL-3.0](https://github.com/henkelmax/sound-physics-remastered). Ported from branch `master`, commit `3d10807d95167fd81960de070121d54bb3518764` (1.5.4+26.3).
- It is a fork of **Sound Physics Fabric** by vlad2305m, which is a fork of **Sound Physics** by Sonic Ether, both under GPL-3.0.
- The sound processing, the default values and the configuration keys were ported from Java to C++.

## Changes from the original

- The OpenAL EFX filters and effect slots were replaced with custom FMOD DSPs.
- The reverb is a new implementation that takes the original's EAX reverb parameters. It does not reproduce OpenAL Soft's reverb exactly.
- Reverb sends are taken after the game's distance attenuation, so `ROOM_ROLLOFF_FACTOR` is not reproduced.
- Bedrock Edition has no block sound types. Blocks are matched to the original's sound types by their names.
- Liquids are treated as full blocks, and the collision shape of a block is treated as a single box.
- Simple Voice Chat integration, the settings screen and the debug rendering are not ported yet.
- The original's icon is not included. The icon of this project is original.

## FMOD

This project does not include FMOD or any part of the FMOD SDK. At runtime it uses the `fmod.dll` that comes with the game. FMOD is a trademark of Firelight Technologies Pty Ltd. This project is not affiliated with or endorsed by Firelight Technologies.

## Dependencies

- [LeviLamina](https://github.com/LiteLDev/LeviLamina) (LGPL-3.0), linked at runtime. Not included in this repository.

Minecraft is a trademark of Mojang Studios. This project is not affiliated with or endorsed by Mojang or Microsoft.
