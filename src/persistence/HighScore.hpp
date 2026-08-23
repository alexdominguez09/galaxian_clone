#pragma once

#include <string>

namespace galaxian {
namespace persistence {

// High-score persistence (docs/game_spec.md §13, docs/architecture.md
// §3.11, Stage 22): a tiny text record stored under the user's data
// directory, loaded at boot and saved atomically (temp file + rename).
//
// Robustness rules:
//   * missing file / unreadable -> treated as 0, never a crash
//   * corrupt content (garbage, truncated, non-numeric, negative,
//     implausibly large) -> treated as 0, never a crash
//   * save() writes to `<path>.tmp` first, then renames over the target
//     (atomic on POSIX), creating parent directories when needed
class HighScoreStore {
public:
    // `$GALAXIAN_DATA_DIR/highscore.dat` if the env var is set (hermetic
    // tests), else `${XDG_DATA_HOME:-$HOME/.local/share}/galaxian-clone/
    // highscore.dat`.
    static std::string defaultPath();

    explicit HighScoreStore(std::string filePath);

    // Reads the stored value. Returns true only when a valid record was
    // found; `out` is set to 0 on any failure.
    bool load(int* out) const;

    // Atomically writes `score`. Creates missing parent directories.
    bool save(int score) const;

    const std::string& path() const { return path_; }

private:
    std::string path_;
};

}  // namespace persistence
}  // namespace galaxian
