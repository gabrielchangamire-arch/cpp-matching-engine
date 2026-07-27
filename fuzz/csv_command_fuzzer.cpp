#include "matching_engine/command.hpp"
#include "matching_engine/csv_commands.hpp"
#include "matching_engine/matching_engine.hpp"

#include <cstddef>
#include <cstdint>
#include <stdexcept>
#include <string_view>
#include <type_traits>
#include <variant>

namespace matching_engine {
namespace {

void exercise_command(const Command& command) {
    MatchingEngine engine;
    std::visit(
        [&engine](const auto& value) {
            using Value = std::decay_t<decltype(value)>;
            if constexpr (std::is_same_v<Value, AddCommand>) {
                static_cast<void>(engine.submit_limit_order(
                    value.order_id, value.side, value.price, value.quantity));
            } else {
                static_cast<void>(engine.cancel(value.order_id));
            }
        },
        command);
}

} // namespace
} // namespace matching_engine

extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t* data,
                                      const std::size_t size) {
    if (size > 4'096) {
        return 0;
    }

    const std::string_view input =
        size == 0 ? std::string_view{}
                  : std::string_view{reinterpret_cast<const char*>(data), size};
    try {
        const auto command = matching_engine::parse_csv_command(input);
        if (command.has_value()) {
            matching_engine::exercise_command(command.value());
        }
    } catch (const std::invalid_argument&) {
        // Malformed and domain-invalid commands are expected parser inputs.
    }
    return 0;
}
