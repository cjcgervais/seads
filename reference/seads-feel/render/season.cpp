#include "render/season.h"

#include <array>
#include <cassert>
#include <cctype>

namespace render {

namespace {
// Index-aligned with the Season enum (Winter=0..Autumn=3).
constexpr std::array<const char*, kSeasonCount> kNames = {"winter", "spring",
                                                          "summer", "autumn"};
}  // namespace

const char* season_name(Season s) {
    const int i = static_cast<int>(s);
    // A forged/corrupted enum should trip in this assert-live repo, not silently
    // render as WINTER (P2-2). The fallback keeps release semantics total.
    assert(i >= 0 && i < kSeasonCount);
    return (i >= 0 && i < kSeasonCount) ? kNames[static_cast<size_t>(i)]
                                        : "winter";
}

bool season_from_string(const std::string& s, Season& out) {
    // Trim surrounding whitespace + lowercase (env/config are hand-typed).
    size_t a = 0, b = s.size();
    while (a < b && std::isspace(static_cast<unsigned char>(s[a]))) ++a;
    while (b > a && std::isspace(static_cast<unsigned char>(s[b - 1]))) --b;
    std::string t = s.substr(a, b - a);
    for (char& c : t) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    for (int i = 0; i < kSeasonCount; ++i) {
        if (t == kNames[static_cast<size_t>(i)]) {
            out = static_cast<Season>(i);
            return true;
        }
    }
    return false;
}

}  // namespace render
