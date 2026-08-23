#include "HighScore.hpp"

#include <sys/stat.h>
#include <sys/types.h>

#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace galaxian {
namespace persistence {

namespace {

// A stored score is plausible only within these bounds; anything else is
// treated as corruption (docs/test_plan.md Stage 22).
constexpr int kMinPlausible = 0;
constexpr int kMaxPlausible = 999999999;

bool isDigits(const char* s)
{
    if (*s == '\0') {
        return false;
    }
    for (const char* p = s; *p != '\0'; ++p) {
        if (*p < '0' || *p > '9') {
            return false;
        }
    }
    return true;
}

void makeParentDirectories(const std::string& filePath)
{
    // Create every missing directory component of `filePath` (mkdir -p).
    size_t pos = 0;
    while (true) {
        const size_t slash = filePath.find('/', pos);
        if (slash == std::string::npos) {
            break;
        }
        const std::string dir = filePath.substr(0, slash);
        if (!dir.empty()) {
            ::mkdir(dir.c_str(), 0755);  // EEXIST is fine, ignore errors
        }
        pos = slash + 1;
    }
}

}  // namespace

std::string HighScoreStore::defaultPath()
{
    const char* overrideDir = std::getenv("GALAXIAN_DATA_DIR");
    if (overrideDir != nullptr && overrideDir[0] != '\0') {
        return std::string(overrideDir) + "/highscore.dat";
    }

    const char* xdg = std::getenv("XDG_DATA_HOME");
    std::string base;
    if (xdg != nullptr && xdg[0] != '\0') {
        base = xdg;
    } else {
        const char* home = std::getenv("HOME");
        base = (home != nullptr && home[0] != '\0')
                   ? std::string(home) + "/.local/share"
                   : "/tmp";  // last resort: never crash on a weird env
    }
    return base + "/galaxian-clone/highscore.dat";
}

HighScoreStore::HighScoreStore(std::string filePath)
    : path_(std::move(filePath))
{
}

bool HighScoreStore::load(int* out) const
{
    if (out == nullptr) {
        return false;
    }
    *out = 0;

    FILE* f = std::fopen(path_.c_str(), "r");
    if (f == nullptr) {
        return false;  // missing file: perfectly fine, start at 0
    }

    char line[128];
    const bool ok = std::fgets(line, sizeof(line), f) != nullptr;
    std::fclose(f);
    if (!ok) {
        return false;  // empty file
    }

    // Strict single-integer line (leading/trailing whitespace tolerated).
    char* start = line;
    while (*start == ' ' || *start == '\t') {
        ++start;
    }
    char* end = start + std::strlen(start);
    while (end > start && (end[-1] == '\n' || end[-1] == '\r' ||
                           end[-1] == ' ' || end[-1] == '\t')) {
        --end;
    }
    *end = '\0';

    if (!isDigits(start)) {
        return false;  // garbage / truncated / negative / non-numeric
    }
    const long long value = std::atoll(start);
    if (value < kMinPlausible || value > kMaxPlausible) {
        return false;
    }
    *out = static_cast<int>(value);
    return true;
}

bool HighScoreStore::save(int score) const
{
    if (score < kMinPlausible || score > kMaxPlausible) {
        return false;  // refuse to persist implausible values
    }

    makeParentDirectories(path_);

    const std::string tmpPath = path_ + ".tmp";
    FILE* f = std::fopen(tmpPath.c_str(), "w");
    if (f == nullptr) {
        return false;
    }
    std::fprintf(f, "%d\n", score);
    std::fflush(f);
    std::fclose(f);

    // Atomic on POSIX: readers see either the old or the new complete
    // file, never a half-written one.
    if (std::rename(tmpPath.c_str(), path_.c_str()) != 0) {
        std::remove(tmpPath.c_str());
        return false;
    }
    return true;
}

}  // namespace persistence
}  // namespace galaxian
