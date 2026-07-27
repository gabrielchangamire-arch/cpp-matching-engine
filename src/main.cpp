#include "matching_engine/csv_commands.hpp"
#include "matching_engine/matching_engine.hpp"

#include <fstream>
#include <iostream>

int main(const int argument_count, char* arguments[]) {
    if (argument_count != 2) {
        std::cerr << "Usage: matching_engine_cli <commands.csv>\n";
        return 1;
    }

    std::ifstream input{arguments[1]};
    if (!input) {
        std::cerr << "Unable to open command file: " << arguments[1] << '\n';
        return 1;
    }

    matching_engine::MatchingEngine engine;
    const matching_engine::CsvRunSummary summary =
        matching_engine::process_csv_commands(input, engine, std::cout, std::cerr);

    std::cout << "\nProcessed " << summary.accepted + summary.rejected
              << " commands: " << summary.accepted << " accepted, " << summary.rejected
              << " rejected\n\n"
              << engine.book();

    return summary.rejected == 0 ? 0 : 2;
}
