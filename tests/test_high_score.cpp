// Stage 22 high-score persistence tests (docs/test_plan.md, Stage 22).
//
// persistence/HighScore is pure stdio (SDL-free, dependency rule). Every
// case runs against a throwaway directory under /tmp; the default-path
// helper is checked for its env-var overrides only.

#include <catch2/catch_test_macros.hpp>

#include <sys/stat.h>

#include <cstdio>
#include <string>

#include "persistence/HighScore.hpp"

using galaxian::persistence::HighScoreStore;

namespace {

struct TempDir {
    std::string path;
    explicit TempDir(const std::string& name)
        : path("/tmp/galaxian_hs_" + name)
    {
        ::mkdir(path.c_str(), 0755);
    }
    ~TempDir() = default;
    std::string file(const std::string& name) const
    {
        return path + "/" + name;
    }
};

void writeFileRaw(const std::string& path, const std::string& content)
{
    FILE* f = std::fopen(path.c_str(), "wb");
    REQUIRE(f != nullptr);
    std::fwrite(content.data(), 1, content.size(), f);
    std::fclose(f);
}

bool fileExists(const std::string& path)
{
    struct stat st;
    return ::stat(path.c_str(), &st) == 0;
}

}  // namespace

TEST_CASE("high score: missing file loads as 0 without crashing",
          "[persistence]")
{
    TempDir dir("missing");
    HighScoreStore store(dir.file("nope.dat"));
    int value = -1;
    CHECK_FALSE(store.load(&value));
    CHECK(value == 0);  // out is ALWAYS set on failure
}

TEST_CASE("high score: corrupt files load as 0 without crashing",
          "[persistence]")
{
    TempDir dir("corrupt");
    const struct {
        const char* name;
        const char* content;
    } cases[] = {
        {"garbage.dat", "total garbage!!\x01\x02"},
        {"half_number.dat", "42abc\n"},    // non-numeric suffix
        {"empty.dat", ""},
        {"negative.dat", "-50\n"},
        {"huge.dat", "99999999999\n"},     // implausibly large
        {"text_first.dat", "HIGHSCORE 100\n"},
        {"mid_number_garbage.dat", "42x0\n"},
    };
    for (const auto& c : cases) {
        INFO(c.name);
        writeFileRaw(dir.file(c.name), c.content);
        HighScoreStore store(dir.file(c.name));
        int value = -1;
        CHECK_FALSE(store.load(&value));
        CHECK(value == 0);
    }

    // Lenient-tail contract: a file holding pure digits WITHOUT a trailing
    // newline (e.g. a truncated write that stopped exactly after a whole
    // number) still parses as that number -- harmless by design.
    writeFileRaw(dir.file("bare.dat"), "420");
    HighScoreStore bare(dir.file("bare.dat"));
    int v = -1;
    REQUIRE(bare.load(&v));
    CHECK(v == 420);
}

TEST_CASE("high score: save then load round-trips exactly", "[persistence]")
{
    TempDir dir("roundtrip");
    const std::string path = dir.file("highscore.dat");
    HighScoreStore store(path);

    CHECK(store.save(42000));
    int loaded = 0;
    REQUIRE(store.load(&loaded));
    CHECK(loaded == 42000);

    // Overwriting with a new best works too.
    CHECK(store.save(133700));
    REQUIRE(store.load(&loaded));
    CHECK(loaded == 133700);

    // No temp file left behind by the atomic write.
    CHECK_FALSE(fileExists(path + ".tmp"));
}

TEST_CASE("high score: save creates missing parent directories",
          "[persistence]")
{
    const std::string deep =
        "/tmp/galaxian_hs_deep/a/b/c/galaxian-clone/highscore.dat";
    std::remove(deep.c_str());
    HighScoreStore store(deep);
    CHECK(store.save(777));
    int loaded = 0;
    REQUIRE(store.load(&loaded));
    CHECK(loaded == 777);
}

TEST_CASE("high score: save refuses implausible values", "[persistence]")
{
    TempDir dir("refuse");
    const std::string path = dir.file("hs.dat");
    HighScoreStore store(path);
    CHECK(store.save(-1) == false);
    CHECK(store.save(2000000000) == false);
    int v = -1;
    CHECK_FALSE(store.load(&v));  // nothing was written
}

TEST_CASE("high score: the atomic write replaces atomically",
          "[persistence]")
{
    // Save twice in quick succession; readers must always see a complete
    // record (the rename guarantees old-or-new, never partial).
    TempDir dir("atomic");
    const std::string path = dir.file("hs.dat");
    HighScoreStore store(path);
    REQUIRE(store.save(10));
    REQUIRE(store.save(20));

    int v = 0;
    REQUIRE(store.load(&v));
    CHECK(v == 20);   // the newest complete record
    CHECK_FALSE(fileExists(path + ".tmp"));
}

TEST_CASE("high score: defaultPath honors the env override", "[persistence]")
{
    ::setenv("GALAXIAN_DATA_DIR", "/tmp/galaxian_custom_data", 1);
    const std::string p = HighScoreStore::defaultPath();
    ::unsetenv("GALAXIAN_DATA_DIR");
    CHECK(p == "/tmp/galaxian_custom_data/highscore.dat");

    // Without the override it lands under the user data home.
    const std::string dflt = HighScoreStore::defaultPath();
    CHECK(dflt.find("galaxian-clone/highscore.dat") != std::string::npos);
}
