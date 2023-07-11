#include "request_queue.h"

RequestQueue::RequestQueue(const SearchServer& search_server)
: search_server_(search_server)
, no_results_requests_(0)
, current_time_(0) {
}

void RequestQueue::AddNewRequest(int count_results) {
    ++current_time_;
    while (!requests_.empty() && min_in_day_ <= current_time_ - requests_.front().timestamp) {
        if (0 == requests_.front().results) {
            --no_results_requests_;
        }
        requests_.pop_front();
    }
    requests_.push_back({ current_time_, count_results });
    if (0 == count_results) {
        ++no_results_requests_;
    }
}

std::vector<Document> RequestQueue::AddFindRequest(const std::string& raw_query, DocumentStatus status) {
    const auto result = search_server_.FindTopDocuments(raw_query, status);
    AddNewRequest(result.size());
    return result;
}
std::vector<Document> RequestQueue::AddFindRequest(const std::string& raw_query) {
    return RequestQueue::AddFindRequest(raw_query, DocumentStatus::ACTUAL);
}

int RequestQueue::GetNoResultRequests() const {
    return no_results_requests_;
}