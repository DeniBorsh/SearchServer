#pragma once
#include "search_server.h"

bool IsDuplicate(std::map<std::string_view, double>& first, std::map<std::string_view, double>& second);

void RemoveDuplicates(SearchServer& search_server);