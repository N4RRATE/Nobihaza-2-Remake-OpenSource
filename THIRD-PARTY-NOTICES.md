# Source and runtime attribution

The handwritten project implementation and documentation are covered by the scoped project MIT license. That grant excludes original game/translation expression, generated content programs, text goldens, game characters, trademarks, graphics, maps, audio, fonts and third-party dependencies. Their publication here does not relicense them or assert that every third-party permission has been established.

The original-expression files in this source tree are:

- `src/game/dekisugi_program.inc`
- `src/game/dora_program.inc`
- `src/game/cash_program.inc`
- `src/game/warehouse_g4_program.inc`
- `src/game/final_boss_program.inc`
- `tools/g3-route-doors.inc`
- `tools/g3-access-cold-room.inc`
- `tests/fixtures/original-text.json`

These are prepared from original game/event or translation content and retain the respective original authors' rights. They are included to preserve the current implementation's actual behavior and test expectations; they are not original handwritten engine code granted under MIT.

CMake fetches SFML 3.1.0 (commit `8835b6b955c96fa1ad1278a8dae6f05c4143effc`, zlib/png license) and nlohmann/json 3.12.0 (MIT). Their sources supply their own copyright/license texts. SFML dependencies may include FreeType, HarfBuzz, SheenBidi, Ogg, Vorbis, FLAC and embedded headers under separate licenses. Their source/build caches are not bundled here. Preserve the actual dependency notices when distributing linked binaries.

The runtime Release includes its actual `licenses/` directory, original retained notices and resource inventory. Preserve that directory when copying runtime files. Original music and recordings remain separate works. Noto CJK fonts use the SIL Open Font License; soundfonts and other bundled components retain their own included terms. A soundfont license does not grant rights in the original compositions or recordings. Read the actual retained notices rather than treating this short index as a replacement for them.
