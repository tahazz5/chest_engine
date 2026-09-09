#pragma once
#include <array>
#include <exception>
#include <iostream>
#include <stdexcept>
#include <string>
#include <string_view>

#define CHECK(expression) do { if (!(expression)) throw std::runtime_error( \
    std::string(__FILE__) + ":" + std::to_string(__LINE__) + ": " #expression); } while (false)

namespace test {
struct Case { std::string_view name; void (*run)(); };
template<std::size_t N>
int run(int argc, char** argv, const std::array<Case, N>& tests) {
    unsigned ran = 0, failures = 0;
    for (const auto& test : tests) {
        if (argc > 1 && test.name != argv[1]) continue;
        ++ran;
        try { test.run(); std::cout << "PASS " << test.name << '\n'; }
        catch (const std::exception& error) {
            ++failures; std::cerr << "FAIL " << test.name << ": " << error.what() << '\n';
        }
    }
    if (ran == 0) std::cerr << "No matching test suite\n";
    return ran == 0 || failures != 0 ? 1 : 0;
}
} // namespace test
