#pragma once
#include <iostream>
#include <set>
#include <string>
#include <vector>
#include <algorithm>
#include <numeric>
#include <map>
#include <tuple>
#include <exception>
#include <cmath>
#include "string_processing.h"
#include "document.h"
#include "log_duration.h"
#include <execution>
#include <string_view>
#include <type_traits>
#include <execution>
#include "log_duration.h"
#include "concurrent_map.h"

const int PROCESSOR_CORES = 4;
const int MAX_RESULT_DOCUMENT_COUNT = 5;
const double EPSILON = 1e-6;

class SearchServer {
public:
    template <typename StringContainer>
    SearchServer(const StringContainer& stop_words);
   
    explicit SearchServer(const std::string_view& stop_words_text);
    
    explicit SearchServer(const std::string& stop_words_text);    

    void AddDocument(int document_id, const std::string_view& document, DocumentStatus status, const std::vector<int>& ratings);

    template <typename DocumentPredicate>
    std::vector<Document> FindTopDocuments(const std::string_view& raw_query, DocumentPredicate document_predicate) const;

    template <typename DocumentPredicate>
    std::vector<Document> FindTopDocuments(const std::execution::sequenced_policy& seq_, const std::string_view& raw_query,
                                           DocumentPredicate document_predicate) const;
    
    template <typename DocumentPredicate>
    std::vector<Document> FindTopDocuments(const std::execution::parallel_policy& par_, const std::string_view& raw_query,
                                           DocumentPredicate document_predicate) const;

    std::vector<Document> FindTopDocuments(const std::string_view& raw_query, DocumentStatus status) const;

    std::vector<Document> FindTopDocuments(const std::execution::sequenced_policy& seq_, const std::string_view& raw_query,
                                           DocumentStatus status) const;

    std::vector<Document> FindTopDocuments(const std::execution::parallel_policy& par_, const std::string_view& raw_query,
                                           DocumentStatus status) const;
    
    std::vector<Document> FindTopDocuments(const std::string_view& raw_query) const;

    std::vector<Document> FindTopDocuments(const std::execution::sequenced_policy& seq_, const std::string_view& raw_query) const;

    std::vector<Document> FindTopDocuments(const std::execution::parallel_policy& par_, const std::string_view& raw_query) const;
    
    int GetDocumentCount() const;

    int GetDocumentId(int index) const;

    std::vector<int>::const_iterator begin() const;

    std::vector<int>::const_iterator end() const;

    std::tuple<std::vector<std::string_view>, DocumentStatus> MatchDocument(const std::string_view& raw_query, int document_id) const;
    
    std::tuple<std::vector<std::string_view>, DocumentStatus> MatchDocument(const std::execution::sequenced_policy& seq_,
                                                                            const std::string_view& raw_query, int document_id) const;
    
    std::tuple<std::vector<std::string_view>, DocumentStatus> MatchDocument(const std::execution::parallel_policy& par_,
                                                                            const std::string_view& raw_query, int document_id) const;

    const std::map<std::string_view, double>& GetWordFrequencies(int document_id) const;

    void RemoveDocument(int document_id);
    
    void RemoveDocument(const std::execution::sequenced_policy& seq_, int document_id);
    
    void RemoveDocument(const std::execution::parallel_policy& par_, int document_id);

private:
    struct DocumentData {
        int rating;
        DocumentStatus status;
    };
    
    const std::set<std::string, std::less<>> stop_words_;
    std::set<std::string, std::less<>> documents_words;
    std::map<std::string_view, std::map<int, double>> word_to_document_freqs_;
    std::map<int, DocumentData> documents_;
    std::vector<int> document_ids_;
    std::map<int, std::map<std::string_view, double>> document_to_word_freqs_;

    bool IsStopWord(const std::string_view& word) const;

    static bool IsValidWord(const std::string_view& word);

    std::vector<std::string_view> SplitIntoWordsNoStop(const std::string_view& text) const;

    static int ComputeAverageRating(const std::vector<int>& ratings);
    
    struct QueryWord {
        std::string_view data;
        bool is_minus;
        bool is_stop;
    };

    QueryWord ParseQueryWord(const std::string_view& text) const;
    
    struct Query {
        std::set<std::string_view> plus_words;
        std::set<std::string_view> minus_words;
    };

    Query ParseQuery(const std::string_view& text) const;

    // Existence required
    double ComputeWordInverseDocumentFreq(const std::string_view& word) const;

    template <typename DocumentPredicate>
    std::vector<Document> FindAllDocuments(const Query& query, DocumentPredicate document_predicate) const; 
    
    template <typename DocumentPredicate>
    std::vector<Document> FindAllDocuments(const std::execution::sequenced_policy& seq_, const Query& query, 
                                           DocumentPredicate document_predicate) const;

    template <typename DocumentPredicate>
    std::vector<Document> FindAllDocuments(const std::execution::parallel_policy& par_, const Query& query, 
                                           DocumentPredicate document_predicate) const;
};

void AddDocument(SearchServer& search_server, int document_id, const std::string_view& document, DocumentStatus status,
                 const std::vector<int>& ratings);

void FindTopDocuments(const SearchServer& search_server, const std::string_view& raw_query);

void MatchDocuments(const SearchServer& search_server, const std::string_view& query);

template <typename StringContainer>
SearchServer::SearchServer(const StringContainer& stop_words)
    : stop_words_(MakeUniqueNonEmptyStrings<StringContainer>(stop_words)) { // Extract non-empty stop wordsstd
    if (!all_of(stop_words_.begin(), stop_words_.end(), IsValidWord)) {
        throw std::invalid_argument("Some of stop words are invalid");
    }
}

template <typename DocumentPredicate>
std::vector<Document> SearchServer::FindTopDocuments(const std::string_view& raw_query, DocumentPredicate document_predicate) const {
    return FindTopDocuments<DocumentPredicate>(std::execution::seq, raw_query, document_predicate);
}

template <typename DocumentPredicate>
std::vector<Document> SearchServer::FindTopDocuments(const std::execution::sequenced_policy& seq_, const std::string_view& raw_query,
                                                     DocumentPredicate document_predicate) const {
    const auto query = ParseQuery(raw_query);
    auto matched_documents = FindAllDocuments(seq_, query, document_predicate);
    std::sort(matched_documents.begin(), matched_documents.end(), [](const Document& lhs, const Document& rhs) {
        if (std::abs(lhs.relevance - rhs.relevance) < EPSILON) {
            return lhs.rating > rhs.rating;
        } else {
            return lhs.relevance > rhs.relevance;
        }
    });
    if (matched_documents.size() > MAX_RESULT_DOCUMENT_COUNT) {
        matched_documents.resize(MAX_RESULT_DOCUMENT_COUNT);
    }

    return matched_documents;
}

template <typename DocumentPredicate>
std::vector<Document> SearchServer::FindTopDocuments(const std::execution::parallel_policy& par_, const std::string_view& raw_query,
                                       DocumentPredicate document_predicate) const {
    const auto query = ParseQuery(raw_query);
    auto matched_documents = FindAllDocuments(par_, query, document_predicate);

    std::sort(matched_documents.begin(), matched_documents.end(), [](const Document& lhs, const Document& rhs) {
        if (std::abs(lhs.relevance - rhs.relevance) < EPSILON) {
            return lhs.rating > rhs.rating;
        } else {
            return lhs.relevance > rhs.relevance;
        }
    });
    
    if (matched_documents.size() > MAX_RESULT_DOCUMENT_COUNT) {
        matched_documents.resize(MAX_RESULT_DOCUMENT_COUNT);
    }

    return matched_documents;
}

template <typename DocumentPredicate>
std::vector<Document> SearchServer::FindAllDocuments(const Query& query, 
                                                     DocumentPredicate document_predicate) const {
    return FindAllDocuments<DocumentPredicate>(std::execution::seq, query, document_predicate);
}
       
template <typename DocumentPredicate>
std::vector<Document> SearchServer::FindAllDocuments(const std::execution::sequenced_policy& seq_, const Query& query, 
                                                     DocumentPredicate document_predicate) const {

    std::map<int, double> document_to_relevance;
    for (const std::string_view& word : query.plus_words) {
        if (word_to_document_freqs_.count(word) == 0) {
            continue;
        }
        const double inverse_document_freq = ComputeWordInverseDocumentFreq(word);
        for (const auto [document_id, term_freq] : word_to_document_freqs_.at(word)) {
            const auto& document_data = documents_.at(document_id);
            if (document_predicate(document_id, document_data.status, document_data.rating)) {
                document_to_relevance[document_id] += term_freq * inverse_document_freq;
            }
        }
    }

    for (const std::string_view& word : query.minus_words) {
        if (word_to_document_freqs_.count(word) == 0) {
            continue;
        }
        for (const auto [document_id, _] : word_to_document_freqs_.at(word)) {
            document_to_relevance.erase(document_id);
        }
    }

    std::vector<Document> matched_documents;
    for (const auto [document_id, relevance] : document_to_relevance) {
        matched_documents.push_back({ document_id, relevance, documents_.at(document_id).rating });
    }
    return matched_documents;
}

template <typename DocumentPredicate>
std::vector<Document> SearchServer::FindAllDocuments(const std::execution::parallel_policy& par_, const Query& query,
                                                     DocumentPredicate document_predicate) const {
//Копируем set плюс-слов и set минус-слов в вектора для получения итераторов произвольного доступа
    std::vector<std::string_view> vector_plus_words;
    vector_plus_words.reserve(query.plus_words.size());
    std::vector<std::string_view> vector_minus_words;
    vector_minus_words.reserve(query.minus_words.size());
    std::copy(par_, query.plus_words.begin(), query.plus_words.end(), std::back_inserter(vector_plus_words));
    std::copy(par_, query.minus_words.begin(), query.minus_words.end(), std::back_inserter(vector_minus_words));

//Разбиваем document_to_relevance на 8 подсловарей для безопасного параллельного доступа, из расчета по 2 подсловаря на 1 ядро процессора    
    ConcurrentMap<int, double> document_to_relevance(PROCESSOR_CORES*2);
    std::for_each(par_, vector_plus_words.begin(), vector_plus_words.end(), 
                  [&document_to_relevance, &document_predicate, this](const auto& word) {
                    if (word_to_document_freqs_.count(word) == 0) {
                        return;
                    }
                    const double inverse_document_freq = ComputeWordInverseDocumentFreq(word);
                    for (const auto [document_id, term_freq] : word_to_document_freqs_.at(word)) {
                        const auto& document_data = documents_.at(document_id);
                        if (document_predicate(document_id, document_data.status, document_data.rating)) {
                            document_to_relevance[document_id].ref_to_value += term_freq * inverse_document_freq;
                        }
                    }
                });
    std::map<int, double> document_to_relevance_map = std::move(document_to_relevance.BuildOrdinaryMap());
    std::for_each(vector_minus_words.begin(), vector_minus_words.end(),
                  [&document_to_relevance_map, this](const auto& word) {
                    if (word_to_document_freqs_.count(word) == 0) {
                        return;
                    }
                    for (const auto [document_id, _] : word_to_document_freqs_.at(word)) {
                        document_to_relevance_map.erase(document_id);
                    }
                }); 
        
    std::vector<Document> matched_documents;
    for (const auto [document_id, relevance] : document_to_relevance_map) {
        matched_documents.push_back({ document_id, relevance, documents_.at(document_id).rating });
    }
    
    return matched_documents;
}