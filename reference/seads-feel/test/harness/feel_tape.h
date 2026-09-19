#pragma once
// ===========================================================================
// THE FEEL TAPE READER -- name-indexed, and it THROWS on a missing column.
//
// The rule this file exists to enforce: a probe may not read a column that
// the tape does not contain. The previous reader returned 0.0 for any name it
// could not find, which turned "this tape predates the column" into "the
// aeroplane had zero body rate" -- and a whole night's dive verdicts were
// measured on a state nobody ever flew. A tape that cannot answer the
// question must SAY SO, loudly, at load time.
//
// Readers name columns; they never spell an index. The index comes from the
// tape's own header, checked against app::kFeelTapeColumns.
// ===========================================================================
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <stdexcept>
#include <string>
#include <vector>

#include "app/feel_tape_columns.h"
#include "app/feel_tape_fields.h"

namespace harness {

class FeelTape {
  public:
    // Required-column sets, named explicitly so a caller declares its
    // dependency and gets a hard error instead of a plausible zero.

    // Everything app::tick needs to resume mid-tape at an arbitrary tick --
    // GENERATED from SEADS_TAPE_STATE_FIELDS, never hand-listed. A hand-listed
    // set is exactly how sim::SimState::throttle/flap/gear stayed missing
    // through a fix FOR the missing-column class: the reader can only guard
    // names someone remembered to write down.
    static std::vector<std::string> seed_columns() {
        std::vector<std::string> v;
#define SEADS_TAPE_NEED_ONE(name, expr) v.push_back(#name);
        // SIM + INT + APP, but NOT SEADS_TAPE_OPT_FIELDS: those are the
        // columns a tape recorded before they existed cannot carry, and
        // feel_tape_fields.h states the narrow rule for what may live there.
        // They are still WRITTEN, still named, and still seeded when present
        // (seed() below) -- they are simply not grounds to refuse a tape of a
        // flight that predates them.
        SEADS_TAPE_SIM_FIELDS(SEADS_TAPE_NEED_ONE, SEADS_TAPE_NEED_ONE,
                              SEADS_TAPE_NEED_ONE, SEADS_TAPE_NEED_ONE)
        SEADS_TAPE_INT_FIELDS(SEADS_TAPE_NEED_ONE, SEADS_TAPE_NEED_ONE,
                              SEADS_TAPE_NEED_ONE, SEADS_TAPE_NEED_ONE)
        SEADS_TAPE_APP_FIELDS(SEADS_TAPE_NEED_ONE, SEADS_TAPE_NEED_ONE,
                              SEADS_TAPE_NEED_ONE, SEADS_TAPE_NEED_ONE)
#undef SEADS_TAPE_NEED_ONE
        return v;
    }

    // The recorded mouse -- what a replay drives with.
    static std::vector<std::string> input_columns() {
        return {"aim_dx", "aim_dy", "frame_ticks"};
    }
    // The recorded truth a replay is graded against.
    static std::vector<std::string> truth_columns() {
        return {"t", "phi", "theta", "alt", "cos_pt", "elev_gap",
                "aim_world_elev", "nose_world_elev"};
    }

    // Throws std::runtime_error if the file is unreadable or if ANY name in
    // `required` is absent from the header.
    void load(const std::string& path,
              const std::vector<std::string>& required) {
        std::FILE* f = std::fopen(path.c_str(), "rb");
        if (f == nullptr)
            throw std::runtime_error("feel tape: cannot open " + path);
        std::vector<char> line(65536);
        bool header_seen = false;
        while (std::fgets(line.data(), int(line.size()), f) != nullptr) {
            if (line[0] == '#') {
                banner_ += line.data();
                continue;
            }
            if (!header_seen) {
                header_seen = true;
                for (char* q = std::strtok(line.data(), ",\r\n"); q != nullptr;
                     q = std::strtok(nullptr, ",\r\n"))
                    names_.push_back(q);
                continue;
            }
            std::vector<double> v;
            v.reserve(names_.size());
            for (char* q = line.data(); *q != '\0';) {
                v.push_back(std::atof(q));
                char* c = std::strchr(q, ',');
                if (c == nullptr) break;
                q = c + 1;
            }
            if (v.size() + 4 < names_.size()) continue;  // torn final line
            rows_.push_back(std::move(v));
        }
        std::fclose(f);
        if (!header_seen)
            throw std::runtime_error("feel tape: no header row in " + path);

        // THE GUARD. Report every missing name at once -- a reader that dies
        // one column at a time teaches nothing about how stale the tape is.
        std::string missing;
        for (const std::string& n : required)
            if (index_of(n) < 0) missing += (missing.empty() ? "" : ", ") + n;
        if (!missing.empty())
            throw std::runtime_error(
                "feel tape " + path + " is missing required column(s): " +
                missing + "  --  it has " + std::to_string(names_.size()) +
                " columns; the current emitter writes " +
                std::to_string(app::kFeelTapeColumnCount) +
                ". This tape predates the fix; re-record it before trusting "
                "any replay seeded from it.");

        // The emitter/header divergence that started all this: the header
        // must name exactly as many fields as a data row carries.
        if (!rows_.empty() && rows_[0].size() != names_.size())
            throw std::runtime_error(
                "feel tape " + path + ": header names " +
                std::to_string(names_.size()) + " columns but a data row has " +
                std::to_string(rows_[0].size()) +
                " fields -- writer/header divergence.");
    }

    int index_of(const std::string& n) const {
        for (size_t i = 0; i < names_.size(); ++i)
            if (names_[i] == n) return int(i);
        return -1;
    }
    // Named access. Throws rather than defaulting -- always.
    double at(size_t row, const std::string& n) const {
        const int i = index_of(n);
        if (i < 0)
            throw std::runtime_error("feel tape: no such column '" + n + "'");
        if (row >= rows_.size())
            throw std::runtime_error("feel tape: row out of range");
        if (size_t(i) >= rows_[row].size())
            throw std::runtime_error("feel tape: short row for '" + n + "'");
        return rows_[row][size_t(i)];
    }
    // The value of column `n`, or `dflt` if the tape has no such column.
    // ONLY for SEADS_TAPE_OPT_FIELDS -- feel_tape_fields.h carries the rule
    // for what is allowed to be optional and why. Never reach for this to
    // silence a missing column: that is the bug this file was written for.
    double at_or(size_t row, const std::string& n, double dflt) const {
        return index_of(n) < 0 ? dflt : at(row, n);
    }
    // Restores the COMPLETE replay state into `S` from row `row`. Every
    // field of SEADS_TAPE_STATE_FIELDS, generated -- so a member added to
    // sim::SimState or control::Internal cannot be forgotten here either.
    // The OPT block reads through at_or, so a tape older than those columns
    // seeds them at their fresh-start value instead of refusing to load.
    void seed(size_t row, app::LoopState& S) const {
#define SEADS_TAPE_READ_D(name, expr) (expr) = at(row, #name);
#define SEADS_TAPE_READ_B(name, expr) (expr) = at(row, #name) > 0.5;
#define SEADS_TAPE_READ_I(name, expr) (expr) = int(at(row, #name));
#define SEADS_TAPE_READ_E(name, expr) \
    (expr) = control::CaptureState(int(at(row, #name)));
#define SEADS_TAPE_READ_OPT_D(name, expr) (expr) = at_or(row, #name, 0.0);
#define SEADS_TAPE_READ_OPT_B(name, expr) (expr) = at_or(row, #name, 0.0) > 0.5;
#define SEADS_TAPE_READ_OPT_I(name, expr) (expr) = int(at_or(row, #name, 0.0));
#define SEADS_TAPE_READ_OPT_E(name, expr) \
    (expr) = control::CaptureState(int(at_or(row, #name, 0.0)));
        SEADS_TAPE_SIM_FIELDS(SEADS_TAPE_READ_D, SEADS_TAPE_READ_B,
                              SEADS_TAPE_READ_I, SEADS_TAPE_READ_E)
        SEADS_TAPE_INT_FIELDS(SEADS_TAPE_READ_D, SEADS_TAPE_READ_B,
                              SEADS_TAPE_READ_I, SEADS_TAPE_READ_E)
        SEADS_TAPE_OPT_FIELDS(SEADS_TAPE_READ_OPT_D, SEADS_TAPE_READ_OPT_B,
                              SEADS_TAPE_READ_OPT_I, SEADS_TAPE_READ_OPT_E)
        SEADS_TAPE_APP_FIELDS(SEADS_TAPE_READ_D, SEADS_TAPE_READ_B,
                              SEADS_TAPE_READ_I, SEADS_TAPE_READ_E)
#undef SEADS_TAPE_READ_D
#undef SEADS_TAPE_READ_B
#undef SEADS_TAPE_READ_I
#undef SEADS_TAPE_READ_E
#undef SEADS_TAPE_READ_OPT_D
#undef SEADS_TAPE_READ_OPT_B
#undef SEADS_TAPE_READ_OPT_I
#undef SEADS_TAPE_READ_OPT_E
        // The quaternions are stored as components; renormalise once (a tape
        // round-trip is exact to max_digits10, but normalise anyway so a
        // hand-edited or interpolated row can never feed a non-unit rotation
        // into the plant).
        S.curr.orientation = glm::normalize(S.curr.orientation);
        S.aim.q = glm::normalize(S.aim.q);
        // `prev` is not a seed field: app::tick overwrites it from `curr` on
        // the next step, and the recorded prev IS the previous row's curr.
        S.prev = S.curr;
    }

    size_t size() const { return rows_.size(); }
    size_t columns() const { return names_.size(); }
    const std::string& banner() const { return banner_; }

    // ---------------------------------------------------------------------
    // ★★★ terrain-clip T2b (red-team P1-5) -- THE CRASH SURFACE IS A TAPE
    // FIELD. The aeroplane's collision surface is a DIAL now ([ground]
    // facet_contact, plus whether the app injected render::facet_radius_at at
    // all), and a tape flown on the drawn facet replayed against the DEM field
    // diverges by up to 66.7 m of terrain. app/main.cpp writes
    //     [config] ground: facet_contact 1.00 (injected: yes)
    // into g_config_banner, and feel_tape_open copies the whole banner in as
    // '#' comment lines -- so the field is already ON every tape written after
    // T2b, and this is the reader for it.
    //
    // BACKWARDS-READABLE BY CONSTRUCTION: a pre-T2b tape has no such line, and
    // facet_contact_or(0.0) then returns the caller's default -- absent reads
    // as 0/absent exactly as the identity-by-branch fallback in sim/ground.h
    // treats a missing injection. Nothing throws, nothing is required.
    // ---------------------------------------------------------------------
    // ★ T2c red-team P2: ANCHORED TO ITS OWN LINE. A bare
    // banner_.find("facet_contact ") would match any other banner line that
    // ever mentions the dial (a future [ground] block, a warning echo, a lane's
    // own note), and "(injected: " is not even scoped to the ground line. Both
    // now start from the "[config] ground: " anchor and read forward, so the
    // field is the one app/main.cpp stamps and nothing else.
    static constexpr const char* kGroundAnchor = "[config] ground: ";
    std::size_t ground_line_() const { return banner_.find(kGroundAnchor); }

    bool has_facet_contact() const {
        const std::size_t g = ground_line_();
        return g != std::string::npos &&
               banner_.find("facet_contact ", g) != std::string::npos;
    }
    double facet_contact_or(double dflt) const {
        const std::size_t g = ground_line_();
        if (g == std::string::npos) return dflt;
        const std::size_t k = banner_.find("facet_contact ", g);
        if (k == std::string::npos) return dflt;
        return std::atof(banner_.c_str() + k + 14);
    }
    bool facet_injected() const {
        const std::size_t g = ground_line_();
        if (g == std::string::npos) return false;
        const std::size_t k = banner_.find("(injected: ", g);
        return k != std::string::npos &&
               banner_.compare(k + 11, 3, "yes") == 0;
    }
    // Warn (never throw) when the tape was flown on a different crash surface
    // than the replay is about to use. Returns true iff they agree. `live_*`
    // are what the replay's own Environment resolves to. A pre-T2b tape is
    // treated as the field arm (0.0 / not injected), which is what it was.
    bool warn_if_facet_mismatch(double live_contact, bool live_injected,
                                const std::string& what = "replay") const {
        const double tape_c = facet_contact_or(0.0);
        const bool tape_i = has_facet_contact() ? facet_injected() : false;
        const bool eff_tape = tape_i && tape_c > 0.0;
        const bool eff_live = live_injected && live_contact > 0.0;
        // ★ T2c red-team P1-9: the banner stamps the dial at %.2f, so the
        // tape can never carry more than two decimals. A 1e-9 tolerance made
        // every tape flown at, say, 0.333 a guaranteed false MISMATCH. Half a
        // print unit is the right bar: it catches 1.00-vs-0.00 and lets
        // round-tripping through %.2f through.
        if (eff_tape == eff_live && std::fabs(tape_c - live_contact) < 5e-3)
            return true;
        std::fprintf(stderr,
                     "[feel-tape] WARNING: %s CRASH-SURFACE MISMATCH -- tape "
                     "facet_contact %.2f (injected: %s)%s vs live %.2f "
                     "(injected: %s). Terrain disagrees by up to tens of "
                     "metres on steep ground; every touchdown and every "
                     "terrain crash in this replay is suspect.\n",
                     what.c_str(), tape_c, tape_i ? "yes" : "no",
                     has_facet_contact() ? "" : " [pre-T2b tape, assumed]",
                     live_contact, live_injected ? "yes" : "no");
        return false;
    }

  private:
    std::vector<std::string> names_;
    std::vector<std::vector<double>> rows_;
    std::string banner_;
};

}  // namespace harness
