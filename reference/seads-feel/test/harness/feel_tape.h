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
#include <cstdio>
#include <cstring>
#include <stdexcept>
#include <string>
#include <vector>

#include "app/feel_tape_columns.h"

namespace harness {

class FeelTape {
  public:
    // Required-column sets, named explicitly so a caller declares its
    // dependency and gets a hard error instead of a plausible zero.

    // Everything app::tick needs to resume mid-tape at an arbitrary tick.
    static std::vector<std::string> seed_columns() {
        return {"px",  "py",  "pz",  "vx",  "vy",  "vz",
                "wx",  "wy",  "wz",  "qw",  "qx",  "qy",
                "qz",  "vhx", "vhy", "vhz", "aqw", "aqx",
                "aqy", "aqz", "int_roll_latch", "int_elev_latch",
                "int_integx", "int_integy", "int_integz",
                "int_aoa_filtered", "int_capture", "int_ballistic",
                "int_deadzoned", "int_pursuit", "int_rest_time",
                "int_inv_rest", "int_righting", "held_bank", "hand_rest"};
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
    size_t size() const { return rows_.size(); }
    size_t columns() const { return names_.size(); }
    const std::string& banner() const { return banner_; }

  private:
    std::vector<std::string> names_;
    std::vector<std::vector<double>> rows_;
    std::string banner_;
};

}  // namespace harness
