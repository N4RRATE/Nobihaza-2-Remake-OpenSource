# Nobihaza 2 Remake

A C++20/SFML learning remake of the original second Nobihaza game. This repository includes the complete current game implementation, its seven prepared content includes, original-text test goldens, development targets and tests. It builds the `nobihaza` game without private content inputs. The prepared runtime, including images, music, maps and fonts, is distributed separately through the public Release and restored with one command.

Project implementation and documentation have the scoped MIT license described in [LICENSE](LICENSE). Original game/translation content, generated event programs, text goldens, characters, music, images, fonts, dependencies and trademarks retain their separate ownership and terms; see [THIRD-PARTY-NOTICES.md](THIRD-PARTY-NOTICES.md). Publishing these files does not make those original works MIT-licensed or establish a complete chain of third-party permissions.

## Build

Requirements: a C++20 compiler, CMake >=3.28, Ninja and Python 3. CMake downloads the pinned SFML 3.1.0 and nlohmann/json 3.12.0 sources and their dependencies unless matching local FetchContent source overrides are supplied. Dependency caches are not part of this repository.

macOS Apple Silicon:

```sh
cmake -S . -B build/local -G Ninja -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_OSX_ARCHITECTURES=arm64 -DCMAKE_OSX_DEPLOYMENT_TARGET=13.0 \
  -DNOBIHAZA_BUILD_TESTS=ON
cmake --build build/local --target nobihaza
```

Alternatively, `cmake --preset mac-release` and `cmake --build --preset mac-release` build the game. The first-room experiment and toolchain probe remain separate targets. The Windows presets retain the MSVC/Ninja route; use an x64 developer shell. Windows has not been validated for this source release. macOS 13.0 is the deployment target, not a claim of testing on every supported OS. The current source build was checked on Apple Silicon; this is not a signed/notarized application distribution.

## Restore runtime and play

From the repository root:

```sh
python3 tools/fetch_runtime.py --destination assets/runtime
./build/local/bin/nobihaza assets/runtime/content.json --entry title
```

The runtime is about 1.74 GB after extraction. Downloaded ZIP parts total about 1.11 GB; allow additional temporary disk space for assembly and extraction. [runtime-manifest.json](runtime-manifest.json) fixes the Release URLs, all 17 part sizes and SHA-256 values, the combined ZIP digest, and all 6,198 extracted file digests (runtime plus license notices). `tools/fetch_runtime.py` uses only Python's standard library. Verified parts are cached in `.runtime-downloads` and reused after interruptions. It rejects changed data, unsafe ZIP paths, links, missing/extra files and an existing destination. Choose a new destination to restore again; it never overwrites an existing runtime or save directory. A corrupt cached part is reported explicitly with the path to remove before retrying.

The explicit `content.json` argument works independently of `.app` resource layout. A packaged Mac `.app` may instead use its own `Contents/Resources/runtime/content.json`; this source build does not automatically create that runtime layout. Saves use the platform's existing default save location. Pass `--save-dir /path/of/your/choice` to keep a separate playtest save directory.

## Checks and content preparation

```sh
python3 tests/test_fetch_runtime.py
python3 tests/test_prepare_public_source.py
python3 tests/test_public_source_lower.py
cmake --build build/local --target room_map_checks g7_score_rules
./build/local/room_map_checks
./build/local/g7_score_rules
```

The original CMake production and development/test targets are retained. Synthetic rule checks need no original assets. Historical integration checks still name their original private evidence/scene/save fixtures in CMake; those evidence directories and real user saves are deliberately not published, so a blanket `ctest` is not a supported standalone acceptance command. Checks requiring runtime data can be invoked explicitly with the restored package and the arguments stated in their source. Original-text test goldens are included at `tests/fixtures/original-text.json`, with their fixed digest checked at configure time; a missing/changed golden is not silently accepted by its consumers.

The seven reviewed `.inc` files already support normal compilation. `tools/public_source_lower.py` and `tools/public-compile-contract.json` preserve the existing optional fixed-input regeneration boundary; they are not a complete importer of a raw RPG Maker project. Runtime preparation utilities, original authoring projects, private reference/evidence history, formal saves and credentials are outside this repository. `PUBLIC-SOURCE-MANIFEST.json` records the reviewed export provenance and hashes; the export utility is used by the maintainers with the original preparation tree, not as a replacement game build command.

The project contains learner-written and Codex-assisted work. It does not claim the learner independently implemented or mastered every module. A successful source build and verified runtime restoration do not replace complete gameplay acceptance, Windows testing or a notarized release.
