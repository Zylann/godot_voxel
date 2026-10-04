# Third party libraries

Please keep categories (`##` level) listed alphabetically and matching their
respective folder names. Use two empty lines to separate categories for
readability.


## fast_noise

- Upstream: https://github.com/Auburn/FastNoiseLite
- Version: 1.1.1 (7ccfbc16eb1c932568f177d63a9ba51d89bbe516, 2024)
- License: MIT

Files extracted from upstream source:

- `Cpp/FastNoiseLite.h` as `FastNoiseLite.h`

Local modifications (marked with `<Zylann>` in the header):

- Prefixed include guard (`ZN_FASTNOISELITE_H`) to avoid conflict with Godot's copy
- Wrapped the class in `namespace fast_noise_lite`
- Added getters for cellular settings
- Uncommented / exposed private members for voxel graph extensions
- Silenced GCC `-Waggressive-loop-optimizations` around intentional integer overflow in hashing


## fast_noise_2

- Upstream: https://github.com/Auburn/FastNoise2
- Version: 0.10.0-alpha (9b75083b87d5e391d43dc0c721d8a9fa51db735a, 2023)
- License: MIT

Files extracted from upstream source:

- `v0.10.0-alpha` sources (`include/`, `src/`, `cmake/`, top-level build/docs
  files), excluding NoiseTool, tests, and CI metadata (`.github/`, `.gitignore`)

Local additions:

- `SConscript` for integration with the Godot / module build system
- `version.txt` recording the vendored version string

Additional compiler workarounds for MSVC / Clang-CL were applied after vendoring;
see git history under `thirdparty/fast_noise_2/`.


## lz4

- Upstream: https://github.com/lz4/lz4
- Version: 1.10.0 (ebb370ca83af193212df4dcbadcc5d87bc0de2f0, 2024)
- License: BSD-2-clause

Files extracted from upstream source:

- `lib/lz4.c` as `lz4.c`
- `lib/lz4.h` as `lz4.h`
- `lib/LICENSE` as `LICENSE`


## meshoptimizer

- Upstream: https://github.com/zeux/meshoptimizer
- Version: 0.25 (6daea4695c48338363b08022d2fb15deaef6ac09, 2025)
- License: MIT

Files extracted from upstream source:

- All files in `src/`
- `LICENSE.md`

Local modifications:

- Wrapped the library in an optional `zylannmeshopt` namespace via
  `MESHOPTIMIZER_ZYLANN_WRAP_LIBRARY_IN_NAMESPACE` /
  `MESHOPTIMIZER_ZYLANN_NAMESPACE_BEGIN` /
  `MESHOPTIMIZER_ZYLANN_NAMESPACE_END` to avoid symbol conflicts with Godot's
  own meshoptimizer when building as a module


## sqlite

- Upstream: https://sqlite.org/download.html (source code amalgamation)
- Version: 3.51.0 (2025)
- License: Public Domain (see https://www.sqlite.org/copyright.html)

Files extracted from upstream source:

- `sqlite3.c`
- `sqlite3.h`
- `sqlite3ext.h`
