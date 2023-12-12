#pragma once
#include "search_server.h"
#include <execution>
#include <list>

std::vector<std::vector<Document>> ProcessQueries(
    const SearchServer& search_server,
    const std::vector<std::string>& queries)
{
    std::vector<std::vector<Document>> output(queries.size());
    transform(std::execution::par_unseq, queries.cbegin(), queries.cend(), output.begin(), [&search_server](const
        std::string& query) {
            return search_server.FindTopDocuments(query);
        });
    return std::move(output);
}

std::list<Document> ProcessQueriesJoined(
    const SearchServer& search_server,
    const std::vector<std::string>& queries)
{
    std::list<Document> output;

    for (const auto& documents : ProcessQueries(search_server, queries)) {
        std::list<Document> tmp(documents.size());
        transform(std::execution::par_unseq, documents.begin(), documents.end(), tmp.begin(), [](const auto& doc) {
            return doc;
            });
        output.splice(output.end(), std::move(tmp));
    }

    return output;
}