#pragma once

#include <string>
#include <iostream>
#include <vector>
#include <set>
#include <map>

std::vector<std::string> SplitIntoWords(const std::string& text);

std::vector<std::string_view> SplitIntoWordsView(std::string_view str);

std::map<std::string_view, double> StringViewFy(const std::map<std::string, double>& inp);

template <typename StringContainer>
std::set<std::string> MakeUniqueNonEmptyStrings(const StringContainer& strings) {
    std::set<std::string> non_empty_strings;
    for (auto str_view : strings) {
        std::string str{ str_view };
        if (!str.empty()) {
            non_empty_strings.emplace(move(str));
        }
    }
    return non_empty_strings;
}

template <typename StringContainer>
std::vector<std::string> Stringify(const StringContainer& strings) {
    std::vector<std::string> output;
    for (auto str_view : strings) {
        output.push_back(std::string{ str_view });
    }
    return move(output);
}
