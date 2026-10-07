# SCT Cross Keygen Templates

**IMPORTANT NOTICE**:

There is no warez stuff in this repo. These templates are only for fun and
just to share art and music.

This repos contains keygen templates for all major os platforms and
architectures. There are not pure C or asm. Instead this time I have used
[raylib][02], [raygui][03], [dear imgui][05], sld2 libraries.

Also full xm playing support from [libxmp][06].

The artifacts are bloat! But still I just want to ride the wave!

One more thing. I have cross compile development toolchain for my
Linux aarch64 machine. So that I can built this templates from my linux
machine to linux and windows machines.

Maybe later I will test on macOS.

## Toolchains

* [x86_64-linux-gnu cross-compilation tool-chain for Arch Linux ARM (Aarch64 host)][07]
* [mingw-w64-toolchain cross-compilation tool-chain for Arch Linux ARM (Aarch64 host)][08]
* [Arch Linux ARM packages which are not available through official repos][09]

## Building

If you have the toolchains above you can build for linux and windows
machines. Just run `build.sh`. By default it strips the artifact binaries.

## Features

* Opacity
* XM Music
* GIF
* Dark/Light mode

Testes on these platforms

| OS        | Arch      | Building | Status     |
| --------- | --------- | -------- | ---------- |
| Linux     | `x86_64`  | built    | not tested |
| Linux     | `x86_32`  | built    | not tested |
| Linux     | `aarch64` | built    | tested     |
| Windows   | `x86_32`  | built    | tested     |
| Windows   | `x86_64`  | built    | tested     |

## Resources

* [raylib Homepage][01]
* [Dear imGui Homepage][04]

## Author

* Blue DeviL // SCT

## License

SCT-PL

[01]: https://www.raylib.com/
[02]: https://github.com/raysan5/raylib
[03]: https://github.com/raysan5/raygui
[04]: https://www.dearimgui.com/
[05]: https://github.com/ocornut/imgui
[06]: https://github.com/libxmp/libxmp
[07]: https://github.com/blue-devil/x86_64-linux-gnu
[08]: https://github.com/blue-devil/mingw-w64-toolchain
[09]: https://github.com/blue-devil/archlinux-arm-packages
