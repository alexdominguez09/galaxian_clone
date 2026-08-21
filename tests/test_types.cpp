#include <catch2/catch_test_macros.hpp>

#include "core/Types.hpp"

using namespace galaxian;

TEST_CASE("Vector2 arithmetic and comparison", "[types]")
{
    const Vector2 a{1.0f, 2.0f};
    const Vector2 b{3.0f, -4.0f};

    REQUIRE(a + b == Vector2{4.0f, -2.0f});
    REQUIRE(b - a == Vector2{2.0f, -6.0f});
    REQUIRE(a * 2.0f == Vector2{2.0f, 4.0f});
    REQUIRE(Vector2{4.0f, 8.0f} / 2.0f == Vector2{2.0f, 4.0f});
    REQUIRE(Vector2{} == Vector2{0.0f, 0.0f});
    REQUIRE(Vector2{0.0f, 0.0f} != Vector2{1.0f, 0.0f});
}

TEST_CASE("Rect edges and helpers", "[types]")
{
    const Rect r{10.0f, 20.0f, 30.0f, 40.0f};

    REQUIRE(r.left() == 10.0f);
    REQUIRE(r.top() == 20.0f);
    REQUIRE(r.right() == 40.0f);
    REQUIRE(r.bottom() == 60.0f);
    REQUIRE(r.position() == Vector2{10.0f, 20.0f});
    REQUIRE(r.size() == Vector2{30.0f, 40.0f});
    REQUIRE(r == Rect{10.0f, 20.0f, 30.0f, 40.0f});
    REQUIRE(r != Rect{10.0f, 20.0f, 30.0f, 41.0f});
}

TEST_CASE("Color comparison and palette defaults", "[types]")
{
    REQUIRE(Color{1, 2, 3} == Color{1, 2, 3, 255});
    REQUIRE(Color{1, 2, 3} != Color{1, 2, 4});
    REQUIRE(Color{}.a == 255);
    REQUIRE(colors::kBlack == Color{0, 0, 0});
    REQUIRE(colors::kWhite == Color{255, 255, 255});
}
