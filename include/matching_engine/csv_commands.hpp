#pragma once

#include "matching_engine/command.hpp"
#include "matching_engine/matching_engine.hpp"

#include <cstddef>
#include <iosfwd>
#include <optional>
#include <string_view>

namespace matching_engine {

struct CsvRunSummary {
    std::size_t accepted;
    std::size_t rejected;

    bool operator==(const CsvRunSummary&) const = default;
};

[[nodiscard]] std::optional<Command> parse_csv_command(std::string_view line);

[[nodiscard]] CsvRunSummary process_csv_commands(std::istream& input,
                                                 MatchingEngine& engine,
                                                 std::ostream& output,
                                                 std::ostream& errors);

} // namespace matching_engine
