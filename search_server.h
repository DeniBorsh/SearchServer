#pragma once

#include <map>
#include <algorithm>
#include "read_input_functions.h"
#include "string_processing.h"
#include "document.h"
#include <execution>
#include "concurent_map.h"

const int MAX_RESULT_DOCUMENT_COUNT = 5;

class SearchServer {
public:
    template <typename StringContainer>
    explicit SearchServer(const StringContainer& stop_words);

    explicit SearchServer(const std::string& stop_words_text)
        : SearchServer(SplitIntoWords(stop_words_text))
    {
    }

    explicit SearchServer(std::string_view stop_words_text)
        : SearchServer(SplitIntoWordsView(stop_words_text))
    {
    }

    void AddDocument(int document_id, std::string_view document, DocumentStatus status, const  std::vector<int>& ratings);

    template <typename DocumentPredicate>
    std::vector<Document> FindTopDocuments(std::string_view raw_query, DocumentPredicate document_predicate) const;
    std::vector<Document> FindTopDocuments(std::string_view raw_query, DocumentStatus status) const;
    std::vector<Document> FindTopDocuments(std::string_view raw_query) const;

    template <typename DocumentPredicate>
    std::vector<Document> FindTopDocuments(const std::execution::parallel_policy&, std::string_view raw_query, DocumentPredicate document_predicate) const;
    std::vector<Document> FindTopDocuments(const std::execution::parallel_policy&, std::string_view raw_query, DocumentStatus status) const;
    std::vector<Document> FindTopDocuments(const std::execution::parallel_policy&, std::string_view raw_query) const;

    template <typename DocumentPredicate>
    std::vector<Document> FindTopDocuments(const std::execution::sequenced_policy&, std::string_view raw_query, DocumentPredicate document_predicate) const;
    std::vector<Document> FindTopDocuments(const std::execution::sequenced_policy&, std::string_view raw_query, DocumentStatus status) const;
    std::vector<Document> FindTopDocuments(const std::execution::sequenced_policy&, std::string_view raw_query) const;

    int GetDocumentCount() const;

    const std::set<int>::const_iterator begin() const noexcept;
    const std::set<int>::const_iterator end() const noexcept;

    std::tuple<std::vector<std::string_view>, DocumentStatus> MatchDocument(std::string_view raw_query, int document_id) const;
    std::tuple<std::vector<std::string_view>, DocumentStatus> MatchDocument(const std::execution::sequenced_policy& policy, std::string_view raw_query, int document_id) const;
    std::tuple<std::vector<std::string_view>, DocumentStatus> MatchDocument(const std::execution::parallel_policy& policy, std::string_view raw_query, int document_id) const;

    const std::map<std::string_view, double> GetWordFrequencies(int document_id) const;

    template <typename ExecutionPolicy>
    void RemoveDocument(ExecutionPolicy&& policy, int document_id);
    void RemoveDocument(int document_id);

private:
    struct DocumentData {
        int rating;
        DocumentStatus status;
    };

    const std::set<std::string> stop_words_;
    std::map<std::string, std::map<int, double>> word_to_document_freqs_;
    std::map<int, std::map<std::string, double>> document_to_word_freqs_;
    std::map<int, DocumentData> documents_;
    std::set<int> document_ids_;

    bool IsStopWord(std::string_view word) const;

    static bool IsValidWord(std::string_view word);

    std::vector<std::string_view> SplitIntoWordsNoStop(std::string_view text) const;

    static int ComputeAverageRating(const std::vector<int>& ratings);

    struct QueryWord {
        std::string_view data;
        bool is_minus;
        bool is_stop;
    };

    QueryWord ParseQueryWord(std::string_view text) const;

    struct Query {
        std::vector<std::string_view> plus_words;
        std::vector<std::string_view> minus_words;
    };

    Query ParseQuery(std::string_view text) const;
    Query ParseQueryParallel(std::string_view text) const;

    template<typename DocumentPredicate>
    std::vector<Document> FindAllDocuments(const Query& query, DocumentPredicate predicate) const;

    template<typename DocumentPredicate>
    std::vector<Document> FindAllDocuments(const std::execution::parallel_policy&, const Query& query, DocumentPredicate predicate) const;

    template<typename DocumentPredicate>
    std::vector<Document> FindAllDocuments(const std::execution::sequenced_policy&, const Query& query, DocumentPredicate predicate) const;
};

template <typename StringContainer>
SearchServer::SearchServer(const StringContainer& stop_words)
    : stop_words_(MakeUniqueNonEmptyStrings(stop_words))
{
    if (!all_of(stop_words_.begin(), stop_words_.end(), IsValidWord)) {
        throw invalid_argument("слово содержит специальный символ"s);
    }
}

template <typename DocumentPredicate>
std::vector<Document> SearchServer::FindTopDocuments(std::string_view raw_query, DocumentPredicate document_predicate) const {
    const Query query = ParseQuery(raw_query);
    auto matched_documents = FindAllDocuments(query, document_predicate);

    sort(matched_documents.begin(), matched_documents.end(),
        [](const Document& lhs, const Document& rhs) {
            const double EPSILON = 1e-6;
            if (abs(lhs.relevance - rhs.relevance) < EPSILON) {
                return lhs.rating > rhs.rating;
            }
            else {
                return lhs.relevance > rhs.relevance;
            }
        });
    if (matched_documents.size() > MAX_RESULT_DOCUMENT_COUNT) {
        matched_documents.resize(MAX_RESULT_DOCUMENT_COUNT);
    }

    return matched_documents;
}

template <typename DocumentPredicate>
std::vector<Document> SearchServer::FindTopDocuments(const std::execution::parallel_policy&, std::string_view raw_query, DocumentPredicate document_predicate) const {
    const Query query = ParseQuery(raw_query);
    auto matched_documents = FindAllDocuments(query, document_predicate);

    sort(execution::par, matched_documents.begin(), matched_documents.end(),
        [](const Document& lhs, const Document& rhs) {
            const double EPSILON = 1e-6;
            if (abs(lhs.relevance - rhs.relevance) < EPSILON) {
                return lhs.rating > rhs.rating;
            }
            else {
                return lhs.relevance > rhs.relevance;
            }
        });
    if (matched_documents.size() > MAX_RESULT_DOCUMENT_COUNT) {
        matched_documents.resize(MAX_RESULT_DOCUMENT_COUNT);
    }

    return matched_documents;
}

template <typename DocumentPredicate>
std::vector<Document> SearchServer::FindTopDocuments(const std::execution::sequenced_policy&, std::string_view raw_query, DocumentPredicate document_predicate) const {
    return FindTopDocuments(raw_query, document_predicate);
}

template<typename DocumentPredicate>
std::vector<Document> SearchServer::FindAllDocuments(const Query& query, DocumentPredicate predicate) const {
    std::map<int, double> document_to_relevance;
    for (auto word : query.plus_words) {
        if (word_to_document_freqs_.count((std::string)word) == 0) {
            continue;
        }
        for (const auto& [document_id, term_freq] : word_to_document_freqs_.at((std::string)word)) {
            const DocumentData documents_data = documents_.at(document_id);
            if (predicate(document_id, documents_data.status, documents_data.rating)) {
                const double IDF = log(GetDocumentCount() * 1.0 / word_to_document_freqs_.at((std::string)word).size());
                document_to_relevance[document_id] += IDF * term_freq;
            }
        }
    }
    for (auto word : query.minus_words) {
        if (word_to_document_freqs_.count((std::string)word) == 0) {
            continue;
        }
        for (const auto& [document_id, term_freq] : word_to_document_freqs_.at((std::string)word)) {
            document_to_relevance.erase(document_id);
        }
    }

    std::vector<Document> matched_documents;
    for (const auto [document_id, relevance] : document_to_relevance) {
        matched_documents.push_back({ document_id, relevance, documents_.at(document_id).rating });
    }
    return matched_documents;
}

template<typename DocumentPredicate>
std::vector<Document> SearchServer::FindAllDocuments(const std::execution::parallel_policy&, const Query& query, DocumentPredicate predicate) const {
    const size_t BUCKET_SIZE = 6;
    Concurrentstd::map<int, double> document_to_relevance(BUCKET_SIZE);

    for_each(execution::par, query.plus_words.begin(), query.plus_words.end(),
        [this, &document_to_relevance]
        (std::string_view word) {
            if (word_to_document_freqs_.count((std::string)word)) {
                for (const auto& [document_id, term_freq] : word_to_document_freqs_.at((std::string)word)) {
                    const DocumentData documents_data = documents_.at(document_id);
                    if (predicate(document_id, documents_data.status, documents_data.rating)) {
                        const double IDF = log(GetDocumentCount() * 1.0 / word_to_document_freqs_.at((std::string)word).size());
                        document_to_relevance[document_id] += IDF * term_freq;
                    }
                }
            }
        }
    );

    for_each(execution::par, query.minus_words.begin(), query.minus_words.end(),
        [this, &document_to_relevance]
        (std::string_view word) {
            if (word_to_document_freqs_.count((std::string)word)) {
                for (const auto& [document_id, _] : word_to_document_freqs_.at((std::string)word)) {
                    document_to_relevance[document_id % BUCKET_SIZE].erase(document_id);
                }
            }
        }
    );

    std::map<int, double> document_to_relevance_reduced = document_to_relevance.BuildOrdinarystd::map();
    std::vector<Document> matched_documents(document_to_relevance_reduced.size());
    for (const auto [document_id, relevance] : document_to_relevance_reduced) {
        matched_documents.push_back({ document_id, relevance, documents_.at(document_id).rating });
    }
    return matched_documents;

}

template<typename DocumentPredicate>
std::vector<Document> SearchServer::FindAllDocuments(const std::execution::sequenced_policy&, const Query& query, DocumentPredicate predicate) const {
    return FindAllDocuments(query, predicate);
}

template <typename ExecutionPolicy>
void SearchServer::RemoveDocument(ExecutionPolicy&& policy, int document_id) {
    if (document_to_word_freqs_.count(document_id)) {
        const std::map<std::string, double>& word_freqs = document_to_word_freqs_.at(document_id);
        std::vector<std::string*> words(word_freqs.size());

        transform(
            policy,
            word_freqs.begin(), word_freqs.end(),
            words.begin(),
            [](const auto& item) { return &item.first; }
        );

        for_each(policy, words.begin(), words.end(), [this, document_id](std::string* item) {
            word_to_document_freqs_.at(*item).erase(document_id);
            });

        document_to_word_freqs_.erase(document_id);
        documents_.erase(document_id);
        document_ids_.erase(document_id);
    }
}