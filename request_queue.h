#pragma once

#include "search_server.h"
#include <deque>

class RequestQueue {
public:
    explicit RequestQueue(const SearchServer& search_server);
    template <typename DocumentPredicate>
    std::vector<Document> AddFindRequest(const std::string& raw_query, DocumentPredicate document_predicate) {
        QueryResult result;
        result.raw_query = raw_query;
        result.documents = search_server_->FindTopDocuments(raw_query, document_predicate);
        if (result.documents.empty())
            ++no_results_requests_count_;

        requests_.push_back(result);

        if (requests_.size() > min_in_day_) {
            if (requests_.front().documents.empty())
                --no_results_requests_count_;
            requests_.pop_front();
        }
        return result.documents;
    }

    std::vector<Document> AddFindRequest(const std::string& raw_query, DocumentStatus status);

    std::vector<Document> AddFindRequest(const std::string& raw_query);
    int GetNoResultRequests() const;
private:
    struct QueryResult {
        std::string raw_query;
        std::vector<Document> documents;
    };
    std::deque<QueryResult> requests_;
    const static int min_in_day_ = 1440;
    const SearchServer* search_server_;
    int no_results_requests_count_ = 0;
};
