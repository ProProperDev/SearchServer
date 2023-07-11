#include "test_example_functions.h"
#include <map>
#include <string>

const std::map<std::string_view, double>& TestGetWordFrequencies(SearchServer& search_server, int document_id) {
    LOG_DURATION("GetWordFrequencies ");
   return search_server.GetWordFrequencies(document_id);
}

void TestRemoveDocument(SearchServer& search_server, int document_id) {
    LOG_DURATION("RemoveDocument ");
    search_server.RemoveDocument(document_id);
}

void TestRemoveDuplicates(SearchServer& search_server) {
    LOG_DURATION("RemoveDuplicates ");
    RemoveDuplicates(search_server);
}