#include "matching_engine/csv_commands.hpp"

#include <charconv>
#include <istream>
#include <ostream>
#include <stdexcept>
#include <string>
#include <system_error>
#include <type_traits>
#include <vector>

namespace matching_engine {
namespace {

std::string_view trim(std::string_view value) {
    const auto first = value.find_first_not_of(" \t\r\n");
    if (first == std::string_view::npos) {
        return {};
    }
    const auto last = value.find_last_not_of(" \t\r\n");
    return value.substr(first, last - first + 1);
}

std::vector<std::string_view> split_fields(const std::string_view line) {
    std::vector<std::string_view> fields;
    std::size_t field_begin = 0;

    while (field_begin <= line.size()) {
        const std::size_t comma = line.find(',', field_begin);
        if (comma == std::string_view::npos) {
            fields.push_back(trim(line.substr(field_begin)));
            break;
        }
        fields.push_back(trim(line.substr(field_begin, comma - field_begin)));
        field_begin = comma + 1;
    }

    return fields;
}

template <typename Integer>
Integer parse_integer(const std::string_view field, const std::string_view name) {
    Integer result{};
    const auto [end, error] =
        std::from_chars(field.data(), field.data() + field.size(), result);
    if (field.empty() || error != std::errc{} || end != field.data() + field.size()) {
        throw std::invalid_argument("invalid " + std::string{name} + ": '" +
                                    std::string{field} + "'");
    }
    return result;
}

Side parse_side(const std::string_view field) {
    if (field == "BUY") {
        return Side::buy;
    }
    if (field == "SELL") {
        return Side::sell;
    }
    throw std::invalid_argument("side must be BUY or SELL");
}

} // namespace

std::optional<Command> parse_csv_command(const std::string_view line) {
    const std::string_view content = trim(line);
    if (content.empty() || content.front() == '#') {
        return std::nullopt;
    }

    const auto fields = split_fields(content);
    if (fields.front() == "ADD") {
        if (fields.size() != 5) {
            throw std::invalid_argument(
                "ADD requires: ADD,order_id,side,price_ticks,quantity");
        }
        return AddCommand{parse_integer<OrderId>(fields[1], "order ID"),
                          parse_side(fields[2]),
                          parse_integer<Price>(fields[3], "price"),
                          parse_integer<Quantity>(fields[4], "quantity")};
    }

    if (fields.front() == "CANCEL") {
        if (fields.size() != 2) {
            throw std::invalid_argument("CANCEL requires: CANCEL,order_id");
        }
        return CancelCommand{parse_integer<OrderId>(fields[1], "order ID")};
    }

    throw std::invalid_argument("command must be ADD or CANCEL");
}

CsvRunSummary process_csv_commands(std::istream& input, MatchingEngine& engine,
                                   std::ostream& output, std::ostream& errors) {
    CsvRunSummary summary{0, 0};
    std::string line;
    std::size_t line_number = 0;

    while (std::getline(input, line)) {
        ++line_number;
        try {
            const auto command = parse_csv_command(line);
            if (!command.has_value()) {
                continue;
            }

            std::visit(
                [&](const auto& value) {
                    using Value = std::decay_t<decltype(value)>;
                    if constexpr (std::is_same_v<Value, AddCommand>) {
                        const auto trades = engine.submit_limit_order(
                            value.order_id, value.side, value.price, value.quantity);
                        output << "ACCEPTED line=" << line_number
                               << " ADD id=" << value.order_id << '\n';
                        for (const Trade& trade : trades) {
                            output << "TRADE line=" << line_number << ' ' << trade
                                   << '\n';
                        }
                    } else {
                        if (!engine.cancel(value.order_id)) {
                            throw std::invalid_argument(
                                "cannot cancel unknown active order ID");
                        }
                        output << "ACCEPTED line=" << line_number
                               << " CANCEL id=" << value.order_id << '\n';
                    }
                },
                *command);
            ++summary.accepted;
        } catch (const std::exception& error) {
            ++summary.rejected;
            errors << "REJECTED line=" << line_number << ": " << error.what() << '\n';
        }
    }

    return summary;
}

} // namespace matching_engine
