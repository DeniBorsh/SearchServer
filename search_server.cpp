#include "search_server.h"

using namespace std;

void SearchServer::AddDocument(int document_id, string_view document, DocumentStatus status, const vector<int>& ratings) {
    if (document_id < 0) {
        throw invalid_argument("документ с отрицательным id"s);
    }
    if (documents_.count(document_id)) {
        throw invalid_argument("документ c id ранее добавленного документа"s);
    }
    if (!IsValidWord(document)) {
        throw invalid_argument("наличие недопустимых символов"s);
    }

    const vector<string> words = Stringify(SplitIntoWordsNoStop(document));
    for (auto& word : words) {
        word_to_document_freqs_[word][document_id] += 1.0 / words.size();
    }
    for (auto& word : words) {
        document_to_word_freqs_[document_id][word] += 1.0 / words.size();
    }
    documents_.emplace(document_id, DocumentData{ ComputeAverageRating(ratings), status });
    document_ids_.emplace(document_id);
}

vector<Document> SearchServer::FindTopDocuments(string_view raw_query, DocumentStatus status) const {
    return FindTopDocuments(raw_query,
        [&status](int document_id, DocumentStatus new_status, int rating) {
            return new_status == status;
        });
}

vector<Document> SearchServer::FindTopDocuments(string_view raw_query) const {
    return FindTopDocuments(raw_query, DocumentStatus::ACTUAL);
}


vector<Document> SearchServer::FindTopDocuments(const execution::parallel_policy&, string_view raw_query, DocumentStatus status) const {
    return FindTopDocuments(execution::par, raw_query,
        [&status](int document_id, DocumentStatus new_status, int rating) {
            return new_status == status;
        });
}

vector<Document> SearchServer::FindTopDocuments(const execution::parallel_policy&, string_view raw_query) const {
    return FindTopDocuments(execution::par, raw_query, DocumentStatus::ACTUAL);
}

vector<Document> SearchServer::FindTopDocuments(const execution::sequenced_policy&, string_view raw_query, DocumentStatus status) const {
    return FindTopDocuments(execution::seq, raw_query,
        [&status](int document_id, DocumentStatus new_status, int rating) {
            return new_status == status;
        });
}

vector<Document> SearchServer::FindTopDocuments(const execution::sequenced_policy&, string_view raw_query) const {
    return FindTopDocuments(execution::seq, raw_query, DocumentStatus::ACTUAL);
}

int SearchServer::GetDocumentCount() const {
    return static_cast<int>(documents_.size());
}

const set<int>::const_iterator SearchServer::begin() const noexcept {
    return document_ids_.begin();
}

const set<int>::const_iterator SearchServer::end() const noexcept {
    return document_ids_.end();
}

tuple<vector<string_view>, DocumentStatus> SearchServer::MatchDocument(string_view raw_query, int document_id) const {
    const Query query = ParseQuery(raw_query);

    vector<string_view> matched_words;
    for (const string_view word : query.minus_words) {
        if (word_to_document_freqs_.count((string)word) == 0) {
            continue;
        }
        if (word_to_document_freqs_.at((string)word).count(document_id)) {
            matched_words.clear();
            return { {}, documents_.at(document_id).status };
        }
    }
    for (const string_view word : query.plus_words) {
        if (word_to_document_freqs_.count((string)word) == 0) {
            continue;
        }
        if (word_to_document_freqs_.at((string)word).count(document_id)) {
            matched_words.push_back(word);
        }
    }

    return { matched_words, documents_.at(document_id).status };
}

tuple<vector<string_view>, DocumentStatus> SearchServer::MatchDocument(const execution::sequenced_policy& policy, string_view raw_query, int document_id) const {
    return MatchDocument(raw_query, document_id);
}

tuple<vector<string_view>, DocumentStatus> SearchServer::MatchDocument(const execution::parallel_policy& policy, string_view raw_query, int document_id) const {
    if ((document_id < 0) || (documents_.count(document_id) == 0)) {
        throw invalid_argument("document_id out of range"s);
    }

    const Query& query = ParseQueryParallel(raw_query);
    const auto& word_freqs = document_to_word_freqs_.at(document_id);

    if (any_of(query.minus_words.begin(),
        query.minus_words.end(),
        [&word_freqs](const string_view word) {
            return word_freqs.count((string)word) > 0;
        })) {
        return { {}, documents_.at(document_id).status };
    }

    vector<string_view> matched_words;
    matched_words.reserve(query.plus_words.size());
    copy_if(query.plus_words.begin(),
        query.plus_words.end(),
        back_inserter(matched_words),
        [&word_freqs](const string_view word) {
            return word_freqs.count((string)word) > 0;
        });

    sort(policy, matched_words.begin(), matched_words.end());
    auto it = unique(matched_words.begin(), matched_words.end());
    matched_words.erase(it, matched_words.end());

    return { matched_words, documents_.at(document_id).status };
}

const map<string_view, double> SearchServer::GetWordFrequencies(int document_id) const {
    map<string_view, double> dummy;
    if (document_to_word_freqs_.count(document_id) == 0) {
        return dummy;
    }
    else {
        return StringViewFy(document_to_word_freqs_.at(document_id));
    }
}

void SearchServer::RemoveDocument(int document_id) {
    if (document_to_word_freqs_.count(document_id)) {
        map<string, double> search_element = document_to_word_freqs_[document_id];

        for (auto [key_string, value_double] : search_element) {
            word_to_document_freqs_[key_string].erase(document_id);
        }

        document_to_word_freqs_.erase(document_id);
        documents_.erase(document_id);
        document_ids_.erase(document_id);
    }
}

bool SearchServer::IsStopWord(string_view word) const {
    return stop_words_.count((string)word) > 0;
}

bool SearchServer::IsValidWord(string_view word) {
    // A valid word must not contain special characters
    return none_of(word.begin(), word.end(), [](char c) {
        return c >= '\0' && c < ' ';
        });
}

vector<string_view> SearchServer::SplitIntoWordsNoStop(string_view text) const {
    vector<string_view> words;
    for (auto word : SplitIntoWordsView(text)) {
        if (!IsValidWord(word)) {
            throw invalid_argument("наличие недопустимых символов"s);
        }
        if (!IsStopWord(word)) {
            words.push_back(word);
        }
    }
    return words;
}

int SearchServer::ComputeAverageRating(const vector<int>& ratings) {
    if (ratings.empty()) {
        return 0;
    }
    int rating_sum = 0;
    for (const auto rating : ratings) {
        rating_sum += rating;
    }
    return rating_sum / static_cast<int>(ratings.size());
}

SearchServer::QueryWord SearchServer::ParseQueryWord(string_view text) const {
    QueryWord result;

    if (text.empty()) {
        throw invalid_argument("отсутствие текста после символа «минус» в поисковом запросе"s);
    }
    bool is_minus = false;
    if (text[0] == '-') {
        is_minus = true;
        text = text.substr(1);
    }
    if (text.empty() || text[0] == '-' || !IsValidWord(text)) {
        throw invalid_argument("наличие более чем одного минуса перед словами"s);
    }

    return { text, is_minus, IsStopWord(text) };
}

SearchServer::Query SearchServer::ParseQuery(string_view text) const {
    Query result;
    for (auto word : SplitIntoWordsView(text)) {
        const QueryWord query_word = ParseQueryWord(word);
        if (!query_word.is_stop) {
            if (query_word.is_minus) {
                result.minus_words.push_back(query_word.data);
            }
            else {
                result.plus_words.push_back(query_word.data);
            }
        }
    }
    sort(result.minus_words.begin(), result.minus_words.end());
    auto itm = unique(result.minus_words.begin(), result.minus_words.end());
    result.minus_words.resize(distance(result.minus_words.begin(), itm));

    sort(result.plus_words.begin(), result.plus_words.end());
    auto itp = unique(result.plus_words.begin(), result.plus_words.end());
    result.plus_words.resize(distance(result.plus_words.begin(), itp));

    return result;
}

SearchServer::Query SearchServer::ParseQueryParallel(string_view text) const {
    Query result;
    for (auto word : SplitIntoWordsView(text)) {
        const QueryWord query_word = ParseQueryWord(word);
        if (!query_word.is_stop) {
            if (query_word.is_minus) {
                result.minus_words.push_back(query_word.data);
            }
            else {
                result.plus_words.push_back(query_word.data);
            }
        }
    }
    return result;
}