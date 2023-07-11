#pragma once
#include "search_server.h"
#include "remove_duplicates.h"
#include "log_duration.h"

const std::map<std::string_view, double>& TestGetWordFrequencies(SearchServer& search_server, int document_id);

void TestRemoveDocument(SearchServer& search_server, int document_id);

void TestRemoveDuplicates(SearchServer& search_server);