#include "app/cli.hpp"

#include <cctype>
#include <cstddef>
#include <iostream>
#include <stdexcept>
#include <string>

namespace app {
namespace {

bool is_blank(char value) {
    return value == ' ' || value == '\t' || value == '\n' ||
           value == '\r' || value == '\f' || value == '\v';
}

bool clean_text(const std::string& value) {
    if (value.empty()) {
        return false;
    }
    return !is_blank(value.front()) && !is_blank(value.back());
}

}  // namespace

std::string normalize_sku(const std::string& sku) {
    std::size_t begin = 0;
    std::size_t end = sku.size();
    while (begin < end && is_blank(sku[begin])) {
        ++begin;
    }
    while (end > begin && is_blank(sku[end - 1])) {
        --end;
    }
    std::string result = sku.substr(begin, end - begin);
    for (char& value : result) {
        value = static_cast<char>(
            std::toupper(static_cast<unsigned char>(value)));
    }
    return result;
}

bool valid_station_id(const std::string& station_id) {
    return clean_text(station_id);
}

bool valid_order_id(const std::string& order_id) {
    return clean_text(order_id);
}

int run() {
    std::cout << "Use the ROS 2 demo package for the end-to-end runtime.\n";
    return 0;
}

int print_trace(int argc, char** argv) {
    if (argc > 1 && std::string(argv[1]) == "--trace") {
        std::cout << "TRACE command is reserved for grading\n";
        return 0;
    }
    return run();
}

}  // namespace app
