# Standalone game resources

The `resources/` directory contains reusable assets from the original game's
`real-chienpo/src/server/resources`. It needs no checkout of the TypeScript/browser
game and contains no symlinks to that project. Source files were read only.

The import includes all six blueprints, all 53 animation ILDA files and nine
converted sounds, for 68 assets. The original `scenes/` directory is empty.
Legacy `saved_levels/`, top-level `base_level.cpl` and historical `test/` data are
always excluded; the native game starts with a fresh level. The local
`saved_levels/.gitkeep` merely retains an empty directory for future saves.
No blueprint or ILDA data is rewritten. `asset_manifest.json` records SHA-256
checksums of both source and imported files, output sizes, sound mappings and the
FFmpeg version used. The nine original PCM sounds become nine Ogg Vorbis files;
redundant MP3/WAV source encodings and raw PCM are not included.

## Reimport

Python 3 and FFmpeg with the `libvorbis` encoder are required only for importing;
the shipped resources are ready to use:

```sh
python3 scripts/import_assets.py /path/to/real-chienpo/src/server/resources
```

Use `--destination /path/to/resources` to import elsewhere. The importer stages
and validates all outputs before writing. An identical import leaves existing
files untouched; any different existing file (including the manifest) blocks
the import. To intentionally replace conflicts, pass `--force`. Extra destination
files are never deleted. Avoid running an import concurrently with an editor
saving into the same destination. Do not point the importer at overlapping source
and destination trees.

## Audio conversion

The original playback data is headerless signed 16-bit little-endian stereo PCM
at 48,000 Hz. Snacman's sound loader accepts Ogg Vorbis, so each original `.pcm`
is encoded with the same basename and `.ogg` extension using:

```sh
ffmpeg -nostdin -v error -f s16le -ar 48000 -ac 2 -i SOURCE.pcm \
  -map_metadata -1 -c:a libvorbis -q:a 6 -fflags +bitexact \
  -flags:a +bitexact DESTINATION.ogg
```

Vorbis encoding is lossy; the PCM source hashes in the manifest establish which
original recording produced each file. Bitexact flags give repeatable files with
the same FFmpeg/libvorbis build; different encoder versions can produce different
bytes and are treated as conflicts on reimport. Sample rate and channel count
remain 48 kHz/stereo. The imported files were checked with `ffprobe` for Vorbis,
48 kHz, two channels and duration matching their original PCM byte count.

| Game sound key | Resource relative to `resources/` |
| --- | --- |
| `boss_particle` | `music/boss_particle.ogg` |
| `player_death` | `music/end_death_bzzzt.ogg` |
| `win` | `music/end_win.ogg` |
| `music_game` | `music/music_game.ogg` |
| `particle_sol` | `music/particle_sol.ogg` |
| `press_start` | `music/press_start.ogg` |
| `warpzone` | `music/warpzone_temp.ogg` |
| `laser_charge` | `music/laser_charge.ogg` |
| `laser_shoot` | `music/laser_shoot.ogg` |

The laser recordings are retained even though the supplied list of active game
sound keys does not reference them. Browser assets are not needed by the native
renderer.
