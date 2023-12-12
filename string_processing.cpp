#include "string_processing.h"

using namespace std;

vector<string> SplitIntoWords(const string& text) {
    vector<string> words;
    string word;
    for (const char c : text) {
        if (c == ' ') {
            if (!word.empty()) {
                words.push_back(word);
                word.clear();
            }
        }
        else {
            word += c;
        }
    }
    if (!word.empty()) {
        words.push_back(word);
    }

    return words;
}

vector<string_view> SplitIntoWordsView(string_view str) {
    vector<string_view> result;
    str.remove_prefix(min(str.find_first_not_of(' '), str.size()));
    const string_view::size_type pos_end = string_view::npos;

    while (!str.empty()) {
        string_view::size_type space = str.find(' ');
        result.push_back(space == pos_end ? str.substr(0, str.size()) : str.substr(0, space));
        str.remove_prefix(std::min(str.find_first_not_of(' ', space), str.size()));
    }

    return result;
}

map<string_view, double> StringViewFy(const map<string, double>& inp) {
    map<string_view, double> outp;
    for (auto& [word, freq] : inp) {
        outp.emplace(word, freq);
    }
    return move(outp);
}