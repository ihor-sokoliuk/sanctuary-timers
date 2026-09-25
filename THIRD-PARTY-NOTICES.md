# Third-party notices

The Zig-built Windows executable statically links compiler runtime and standard library components. Their notices are included in `licenses/`:

- LLVM libc++: `licenses/libcxx.txt` (Apache 2.0 with LLVM exceptions and applicable legacy notices).
- LLVM libc++abi: `licenses/libcxxabi.txt` (Apache 2.0 with LLVM exceptions and applicable legacy notices).
- MinGW-w64 runtime: `licenses/mingw-w64.txt` (individual runtime notices).
- Zig compiler runtime: `licenses/zig.txt` (MIT and included notices).

The compiler itself is not bundled with the application. Windows system libraries are provided by the operating system. No code or artwork from other Diablo overlay projects is included. The application icon and event glyphs are drawn specifically for this project.

The executable embeds unmodified PT Serif regular and bold by ParaType Ltd. under the SIL Open Font License 1.1. Its license is embedded as a resource and included as `licenses/PT-Serif-OFL.txt` in binary distributions. Original font files, license and source revision are in `resources/fonts/` in the source repository.

Timing data is obtained from the public Helltides.com website feed. This notice does not grant rights to Blizzard trademarks or game assets.
