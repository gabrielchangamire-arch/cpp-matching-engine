#include "matching_engine/command.hpp"
#include "matching_engine/csv_commands.hpp"
#include "matching_engine/matching_engine.hpp"

#include <gtest/gtest.h>
#include <sstream>
#include <stdexcept>
#include <string>
#include <variant>

namespace matching_engine {
namespace {

TEST(CsvCommandTest, ParsesBuyAddCommand) {
    const auto command = parse_csv_command("ADD,42,BUY,10125,50");

    ASSERT_TRUE(command.has_value());
    // GoogleTest's fatal assertion guarantees the optional is populated.
    // NOLINTNEXTLINE(bugprone-unchecked-optional-access)
    EXPECT_EQ(std::get<AddCommand>(command.value()),
              (AddCommand{42, Side::buy, 10'125, 50}));
}

TEST(CsvCommandTest, ParsesSellAddCommandWithWhitespace) {
    const auto command = parse_csv_command(" ADD , 7 , SELL , 9900 , 3 ");

    ASSERT_TRUE(command.has_value());
    // GoogleTest's fatal assertion guarantees the optional is populated.
    // NOLINTNEXTLINE(bugprone-unchecked-optional-access)
    EXPECT_EQ(std::get<AddCommand>(command.value()),
              (AddCommand{7, Side::sell, 9'900, 3}));
}

TEST(CsvCommandTest, ParsesCancelCommand) {
    const auto command = parse_csv_command("CANCEL,42");

    ASSERT_TRUE(command.has_value());
    // GoogleTest's fatal assertion guarantees the optional is populated.
    // NOLINTNEXTLINE(bugprone-unchecked-optional-access)
    EXPECT_EQ(std::get<CancelCommand>(command.value()), (CancelCommand{42}));
}

TEST(CsvCommandTest, IgnoresBlankAndCommentLines) {
    EXPECT_FALSE(parse_csv_command("   ").has_value());
    EXPECT_FALSE(parse_csv_command("  # comment").has_value());
}

TEST(CsvCommandTest, RejectsUnknownCommand) {
    EXPECT_THROW(static_cast<void>(parse_csv_command("REPLACE,1")),
                 std::invalid_argument);
}

TEST(CsvCommandTest, RejectsWrongFieldCount) {
    EXPECT_THROW(static_cast<void>(parse_csv_command("ADD,1,BUY,100")),
                 std::invalid_argument);
    EXPECT_THROW(static_cast<void>(parse_csv_command("CANCEL,1,extra")),
                 std::invalid_argument);
}

TEST(CsvCommandTest, RejectsInvalidSide) {
    EXPECT_THROW(static_cast<void>(parse_csv_command("ADD,1,HOLD,100,1")),
                 std::invalid_argument);
}

TEST(CsvCommandTest, RejectsMalformedAndOutOfRangeIntegers) {
    EXPECT_THROW(static_cast<void>(parse_csv_command("ADD,nope,BUY,100,1")),
                 std::invalid_argument);
    EXPECT_THROW(static_cast<void>(parse_csv_command("ADD,1,BUY,100x,1")),
                 std::invalid_argument);
    EXPECT_THROW(
        static_cast<void>(parse_csv_command("ADD,18446744073709551616,BUY,100,1")),
        std::invalid_argument);
}

TEST(CsvCommandTest, ProcessesTradesAndCancellation) {
    std::istringstream input{"ADD,1,SELL,10100,5\n"
                             "ADD,2,BUY,10100,3\n"
                             "CANCEL,1\n"};
    std::ostringstream output;
    std::ostringstream errors;
    MatchingEngine engine;

    const auto summary = process_csv_commands(input, engine, output, errors);

    EXPECT_EQ(summary, (CsvRunSummary{3, 0}));
    EXPECT_TRUE(errors.str().empty());
    EXPECT_NE(output.str().find("TRADE line=2 Trade{id=1"), std::string::npos);
    EXPECT_FALSE(engine.book().best_ask().has_value());
}

TEST(CsvCommandTest, ReportsRejectedLineAndContinues) {
    std::istringstream input{"ADD,1,BUY,0,5\n"
                             "CANCEL,999\n"
                             "ADD,2,BUY,100,3\n"};
    std::ostringstream output;
    std::ostringstream errors;
    MatchingEngine engine;

    const auto summary = process_csv_commands(input, engine, output, errors);

    EXPECT_EQ(summary, (CsvRunSummary{1, 2}));
    EXPECT_NE(errors.str().find("REJECTED line=1"), std::string::npos);
    EXPECT_NE(errors.str().find("REJECTED line=2"), std::string::npos);
    EXPECT_EQ(engine.book().best_bid(), (BookLevel{100, 3, 1}));
}

} // namespace
} // namespace matching_engine
