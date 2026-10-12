#!/usr/bin/env python3
"""Import standalone game data, converting raw PCM to Ogg Vorbis."""

import argparse
import hashlib
import json
from pathlib import Path
import shutil
import subprocess
import tempfile


SOUND_KEYS = {
    "boss_particle": "boss_particle",
    "player_death": "end_death_bzzzt",
    "win": "end_win",
    "music_game": "music_game",
    "particle_sol": "particle_sol",
    "press_start": "press_start",
    "warpzone": "warpzone_temp",
    "laser_charge": "laser_charge",
    "laser_shoot": "laser_shoot",
}
IMPORT_DIRECTORIES = {"animations", "blueprints", "music", "scenes"}


def digest(path):
    value = hashlib.sha256()
    with path.open("rb") as stream:
        for chunk in iter(lambda: stream.read(1024 * 1024), b""):
            value.update(chunk)
    return value.hexdigest()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("source", type=Path, help="original src/server/resources directory")
    parser.add_argument("--destination", type=Path,
                        default=Path(__file__).resolve().parents[1] / "resources")
    parser.add_argument("--force", action="store_true", help="replace conflicting destination files")
    args = parser.parse_args()
    source, destination = args.source.resolve(), args.destination.resolve()
    if source == destination or source in destination.parents or destination in source.parents:
        parser.error("source and destination must be separate directory trees")
    if not (source / "animations").is_dir() or not (source / "music").is_dir():
        parser.error("source must contain animations/ and music/")
    if not shutil.which("ffmpeg"):
        parser.error("ffmpeg with libvorbis support is required")
    # Legacy levels and historical tests are intentionally excluded: the native
    # game starts with a fresh level and keeps only reusable original assets.
    files = sorted(path for path in source.rglob("*") if path.is_file()
                   and path.relative_to(source).parts[0] in IMPORT_DIRECTORIES)
    if any(path.is_symlink() for path in source.rglob("*")):
        parser.error("source symlinks are not supported")
    manifest = {
        "format_version": 1,
        "provenance": "real-chienpo/src/server/resources",
        "included_directories": sorted(IMPORT_DIRECTORIES),
        "excluded_legacy_data": ["base_level.cpl", "saved_levels/", "test/"],
        "audio_conversion": {
            "input": "signed 16-bit little-endian PCM, 48000 Hz, stereo",
            "output": "Ogg Vorbis, quality 6, 48000 Hz, stereo",
            "ffmpeg_version": subprocess.check_output(
                ["ffmpeg", "-version"], text=True).splitlines()[0],
            "command": ["ffmpeg", "-nostdin", "-v", "error", "-f", "s16le",
                        "-ar", "48000", "-ac", "2", "-i", "SOURCE.pcm", "-map_metadata", "-1",
                        "-c:a", "libvorbis", "-q:a", "6", "-fflags", "+bitexact",
                        "-flags:a", "+bitexact", "DESTINATION.ogg"],
        },
        "sounds": {key: "music/" + name + ".ogg" for key, name in SOUND_KEYS.items()},
        "files": [],
    }
    # Stage before checking conflicts, so an unsuccessful conversion changes nothing.
    with tempfile.TemporaryDirectory(prefix="chienpo-assets-") as temporary:
        staged = Path(temporary)
        for path in files:
            relative = path.relative_to(source)
            is_music = relative.parts[0] == "music"
            if is_music and path.suffix.lower() != ".pcm":
                continue  # MP3/WAV are redundant source encodings, not runtime inputs.
            output_relative = relative.with_suffix(".ogg") if is_music else relative
            output = staged / output_relative
            output.parent.mkdir(parents=True, exist_ok=True)
            source_hash = digest(path)
            if is_music:
                command = manifest["audio_conversion"]["command"][:]
                command[command.index("SOURCE.pcm")] = str(path)
                command[command.index("DESTINATION.ogg")] = str(output)
                subprocess.run(command, check=True)
            else:
                shutil.copyfile(path, output)
            if digest(path) != source_hash:
                raise RuntimeError("source changed during import: " + str(path))
            manifest["files"].append({
                "source": relative.as_posix(), "source_sha256": source_hash,
                "path": output_relative.as_posix(), "sha256": digest(output),
                "bytes": output.stat().st_size,
            })
        for filename in manifest["sounds"].values():
            if not (staged / filename).is_file():
                raise RuntimeError("missing required sound: " + filename)
        (staged / "asset_manifest.json").write_text(
            json.dumps(manifest, indent=2, sort_keys=True) + "\n", encoding="utf-8")
        staged_files = sorted(path for path in staged.rglob("*") if path.is_file())
        for path in staged_files:
            target = destination / path.relative_to(staged)
            for parent in [target, *target.parents]:
                if parent == destination.parent:
                    break
                if parent.is_symlink():
                    raise RuntimeError("refusing destination symlink: " + str(parent))
            if target.exists() and (not target.is_file() or digest(target) != digest(path)):
                if not args.force or not target.is_file():
                    raise RuntimeError("destination conflict (use --force to replace files): " + str(target))
        # Keep empty scene directories; do not delete extra user-created destination files.
        for directory in sorted(path for path in source.rglob("*") if path.is_dir()):
            if directory.relative_to(source).parts[0] in IMPORT_DIRECTORIES:
                (destination / directory.relative_to(source)).mkdir(parents=True, exist_ok=True)
        for path in staged_files:
            target = destination / path.relative_to(staged)
            target.parent.mkdir(parents=True, exist_ok=True)
            if not target.exists() or digest(target) != digest(path):
                shutil.copyfile(path, target)
    print("Imported {} assets into {}".format(len(manifest["files"]), destination))


if __name__ == "__main__":
    main()
