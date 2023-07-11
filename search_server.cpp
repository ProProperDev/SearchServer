#include <iterator>
#include "search_server.h"
#include "read_input_functions.h"
#include "test_example_functions.h"
#include <execution>
#include <tuple>
#include <cassert>
#include <numeric>

SearchServer::SearchServer(const std::string& stop_words_text)
    : SearchServer::SearchServer(SplitIntoWords(stop_words_text)) {
}

SearchServer::SearchServer(const std::string_view& stop_words_text)
    : SearchServer::SearchServer(SplitIntoWords(stop_words_text)) {
}

void SearchServer::AddDocument(int document_id, const std::string_view& document, DocumentStatus status, const std::vector<int>& ratings) {
    if ((document_id < 0) || (documents_.count(document_id) > 0)) {
        throw std::invalid_argument("Invalid document_id");
    }
    const auto words = SplitIntoWordsNoStop(document);

    const double inv_word_count = 1.0 / words.size();
    for (const std::string_view& word : words) {
        documents_words.insert(static_cast<std::string>(word));
        auto word_iter = std::find( documents_words.begin(), documents_words.end(), word);
        const std::string_view word_view{*(word_iter)};
        word_to_document_freqs_[word_view][document_id] += inv_word_count;
        document_to_word_freqs_[document_id][word_view] += inv_word_count;
    }
    documents_.emplace(document_id, DocumentData{ComputeAverageRating(ratings), status});
    document_ids_.push_back(document_id);
}    

std::vector<Document> SearchServer::FindTopDocuments(const std::string_view& raw_query, DocumentStatus status) const {
    return FindTopDocuments(std::execution::seq, raw_query, status);
}

std::vector<Document> SearchServer::FindTopDocuments(const std::execution::sequenced_policy& seq_, const std::string_view& raw_query,
                                                     DocumentStatus status) const {
    return FindTopDocuments(seq_, raw_query, [status](int document_id, DocumentStatus document_status, int rating) {
                            return document_status == status;
                        });
}

std::vector<Document> SearchServer::FindTopDocuments(const std::execution::parallel_policy& par_, const std::string_view& raw_query,
                                                     DocumentStatus status) const {
    return FindTopDocuments(par_, raw_query, [status](int document_id, DocumentStatus document_status, int rating) {
                            return document_status == status;
                        });
}

std::vector<Document> SearchServer::FindTopDocuments(const std::string_view& raw_query) const {
    return FindTopDocuments(std::execution::seq, raw_query);
}

std::vector<Document> SearchServer::FindTopDocuments(const std::execution::sequenced_policy& seq_, const std::string_view& raw_query) const {
    return FindTopDocuments(seq_, raw_query, DocumentStatus::ACTUAL);
}

std::vector<Document> SearchServer::FindTopDocuments(const std::execution::parallel_policy& par_, const std::string_view& raw_query) const {
    return FindTopDocuments(par_, raw_query, DocumentStatus::ACTUAL);
}

int SearchServer::GetDocumentCount() const {
    return documents_.size();
}

int SearchServer::GetDocumentId(int index) const {
    return document_ids_.at(index);
}

std::vector<int>::const_iterator SearchServer::begin() const {
    return this->document_ids_.begin();
}

std::vector<int>::const_iterator SearchServer::end() const {
    return this->document_ids_.end();
}

const std::map<std::string_view, double>& SearchServer::GetWordFrequencies(int document_id) const {
    if(document_to_word_freqs_.count(document_id)>0) {
        return document_to_word_freqs_.at(document_id);
    }
    return (*document_to_word_freqs_.end()).second;
}

void SearchServer::RemoveDocument(int document_id) {
    RemoveDocument(std::execution::seq, document_id);
}

void SearchServer::RemoveDocument(const std::execution::sequenced_policy& seq_, int document_id) {
    if(std::find(seq_, document_ids_.begin(), document_ids_.end(), document_id)==document_ids_.end()) {
        return;
    }
    //Удаляем документ с данным id из всех контейнеров
    for(const auto [word, TF] : GetWordFrequencies(document_id)){
        word_to_document_freqs_[word].erase(document_id);
        if(word_to_document_freqs_[word].empty()) {
            word_to_document_freqs_.erase(word);
        }
    }
    document_to_word_freqs_.erase(document_id);
    documents_.erase(document_id);
    document_ids_.erase(std::find(seq_, document_ids_.begin(),document_ids_.end(),document_id));   
}

void SearchServer::RemoveDocument(const std::execution::parallel_policy& par_, int document_id) {
    if(std::find(document_ids_.begin(), document_ids_.end(), document_id)==document_ids_.end()) {
        return;
    }

    std::vector<std::pair<std::string_view, double>> delete_(document_to_word_freqs_.at(document_id).size());
    //Удаляем документ с данным id из всех контейнеров
    std::copy(make_move_iterator(document_to_word_freqs_[document_id].begin()),
              make_move_iterator(document_to_word_freqs_[document_id].end()),
              delete_.begin());

    std::for_each(par_, delete_.begin(), delete_.end(),
                    [&document_id, this](auto& word_ptr) {
                        word_to_document_freqs_[word_ptr.first].erase(document_id);
                        if(word_to_document_freqs_[word_ptr.first].empty()) {
                            word_to_document_freqs_.erase(word_ptr.first);
                        }
                    });
    
    documents_.erase(document_id);
    document_ids_.erase(std::find(document_ids_.begin(), document_ids_.end(), document_id));
    document_to_word_freqs_.erase(document_id);
}

std::tuple<std::vector<std::string_view>, DocumentStatus> SearchServer::MatchDocument(
    const std::string_view& raw_query,
    int document_id) const {
    return MatchDocument(std::execution::seq, raw_query, document_id);
}

std::tuple<std::vector<std::string_view>, DocumentStatus> SearchServer::MatchDocument(
    const std::execution::sequenced_policy& seq_,
    const std::string_view& raw_query,
    int document_id) const {
  
  if (!IsValidWord(raw_query)) {
    throw std::invalid_argument("Invalid query");
  }

  if (std::count(seq_, document_ids_.begin(), document_ids_.end(), document_id)==0) {
    throw std::out_of_range("Out_of_range_id");
  }

  const auto query = ParseQuery(raw_query);
  std::vector<std::string_view> matched_words;
  for (const std::string_view& word : query.plus_words) {
    if (document_to_word_freqs_.at(document_id).count(word) == 0) {
        continue;
    }
    if (document_to_word_freqs_.at(document_id).count(word)) {
        matched_words.push_back(word);
    }
  }
        
  for (const std::string_view& word : query.minus_words) {
    if (document_to_word_freqs_.at(document_id).count(word) == 0) {
        continue;
    }
    if (document_to_word_freqs_.at(document_id).count(word)) {
        matched_words.clear();
        break;
    }
  }

  return {matched_words, SearchServer::documents_.at(document_id).status};    
}

std::tuple<std::vector<std::string_view>, DocumentStatus> SearchServer::MatchDocument(
    const std::execution::parallel_policy& par_,
    const std::string_view& raw_query,
    int document_id) const {
  
  if (!IsValidWord(raw_query)) {
    throw std::invalid_argument("Invalid query");
  }
    
  if(std::count( par_, document_ids_.begin(), document_ids_.end(), document_id)==0) {
    throw std::out_of_range("Out_of_range_id");
  }
    
  std::vector<std::string_view> matched_words;
  std::vector<std::string_view> vector_plus;
  std::vector<std::string_view> vector_minus;
    
  for (const auto& word : SplitIntoWords(raw_query)) {
    const auto parse_word = ParseQueryWord(word);
    if (!parse_word.is_stop) {
        if (parse_word.is_minus) {
            vector_minus.push_back(move(parse_word.data));
        } else {
            vector_plus.push_back(move(parse_word.data));
        }
    }
  }

  bool match_minus_words = std::any_of(par_, vector_minus.begin(), vector_minus.end(),
                                        [&](const std::string_view& word) {
                                            return document_to_word_freqs_.at(document_id).count(word)>0;
                                        });
    
  if (match_minus_words) {
    return {matched_words, SearchServer::documents_.at(document_id).status};
  }
    
  auto predicat = [&](const auto& word) {
    return document_to_word_freqs_.at(document_id).count(word)>0 && std::count(matched_words.begin(), matched_words.end(), word)==0;
  };
    
  std::copy_if(par_, vector_plus.begin(), vector_plus.end(), std::back_inserter(matched_words), predicat);
    
  return {matched_words, SearchServer::documents_.at(document_id).status};
}

bool SearchServer::IsStopWord(const std::string_view& word) const {
    return stop_words_.count(word) > 0;
}

bool SearchServer::IsValidWord(const std::string_view& word) {
// A valid word must not contain special characters
    return std::none_of(word.begin(), word.end(), [](char c) {
        return c >= '\0' && c < ' ';
    });
}

std::vector<std::string_view> SearchServer::SplitIntoWordsNoStop(const std::string_view& text) const {
    std::vector<std::string_view> words;
    for (const std::string_view& word : SplitIntoWords(text)) {
        if (!IsValidWord(word)) {
            throw std::invalid_argument("Word " + static_cast<std::string>(word) + " is invalid");
        }
        
        if (!IsStopWord(word)) {
            words.push_back(word);
        }
    }
    return words;
}

int SearchServer::ComputeAverageRating(const std::vector<int>& ratings) {
    if (ratings.empty()) {
        return 0;
    }
    int rating_sum = accumulate(ratings.begin(), ratings.end(), 0);
    return rating_sum / static_cast<int>(ratings.size());
}
 
SearchServer::QueryWord SearchServer::ParseQueryWord(const std::string_view& text) const {
    if (text.empty()) {
        throw std::invalid_argument("Query word is empty");
    }

    std::string_view word = text;
    bool is_minus = false;
    if (word[0] == '-') {
        is_minus = true;
        word = word.substr(1);
    }

    if (word.empty() || word[0] == '-' || !IsValidWord(word)) {
        throw std::invalid_argument("Query word " + static_cast<std::string>(text) + " is invalid");
    }
    
    return {word, is_minus, IsStopWord(word)};
}

   
SearchServer::Query SearchServer::ParseQuery(const std::string_view& text) const {
    Query result;
    for (const std::string_view& word : SplitIntoWords(text)) {
        const auto query_word = ParseQueryWord(word);
        if (!query_word.is_stop) {
            if (query_word.is_minus) {
                result.minus_words.insert(query_word.data);
            } else {
                result.plus_words.insert(query_word.data);
            }
        }
    }
    return result;
}

// Existence required
double SearchServer::ComputeWordInverseDocumentFreq(const std::string_view& word) const {
    return std::log(GetDocumentCount() * 1.0 / word_to_document_freqs_.at(word).size());
}   

    
void AddDocument(SearchServer& search_server, int document_id, const std::string_view& document, DocumentStatus status,
                 const std::vector<int>& ratings) {
    try {
        search_server.AddDocument(document_id, document, status, ratings);
    } catch (const std::exception& e) {
        std::cout << "Ошибка добавления документа " << document_id << ": " << e.what() << std::endl;
    }
}

void FindTopDocuments(const SearchServer& search_server, const std::string_view& raw_query) {
    std::cout << "Результаты поиска по запросу: " << raw_query << std::endl;
    try {
        for (const Document& document : search_server.FindTopDocuments(raw_query)) {
            PrintDocument(document);
        }
    } catch (const std::exception& e) {
        std::cout << "Ошибка поиска: " << e.what() << std::endl;
    }
}

void MatchDocuments(const SearchServer& search_server, const std::string_view& query) {
    try {
        std::cout << "Матчинг документов по запросу: " << query << std::endl;
        const int document_count = search_server.GetDocumentCount();
        for (int index = 0; index < document_count; ++index) {
            const int document_id = search_server.GetDocumentId(index);
            const auto [words, status] = search_server.MatchDocument(query, document_id);
            PrintMatchDocumentResult(document_id, words, status);
        }
    } catch (const std::exception& e) {
        std::cout << "Ошибка матчинга документов на запрос " << query << ": " << e.what() << std::endl;
    }
}
