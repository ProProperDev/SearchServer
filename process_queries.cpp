#include "process_queries.h"
#include "search_server.h"
#include <vector>
#include <execution>

std::vector<std::vector<Document>> ProcessQueries(
    const SearchServer& search_server,
    const std::vector<std::string>& queries) {
    std::vector<std::vector<Document>> result(queries.size());
    std::transform(std::execution::par
        , queries.begin()
        , queries.end()
        , result.begin()
        ,[&search_server](const std::string_view& query) {
            return search_server.FindTopDocuments(query);
        });
    return result;
}

std::list<Document> ProcessQueriesJoined(
    const SearchServer& search_server,
    const std::vector<std::string>& queries) {
    std::list<Document> result;
    auto buffer_vector = ProcessQueries(search_server, queries);
    for(auto vector_docs : buffer_vector) {
        for(auto doc_ : vector_docs) {
            result.push_back(doc_);
        }
    }
    return result;
}