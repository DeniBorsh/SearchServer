#include "remove_duplicates.h"

using namespace std;

bool IsDuplicate(const map<string_view, double>& first, const map<string_view, double>& second) {
	if (first.size() != second.size()) return false;
	for (auto it_1 = first.begin(), it_2 = second.begin(); it_1 != first.end(); ++it_1, ++it_2) {
		if ((*it_1).first != (*it_2).first) return false;
	}
	return true;
}

void RemoveDuplicates(SearchServer& search_server) {
	set<int> ids_to_remove;
	for (auto it = search_server.begin(); it != search_server.end();) {
		if (!ids_to_remove.count(*it)) {
			for (auto jt = ++it; jt != search_server.end(); ++jt) {
				if (IsDuplicate(search_server.GetWordFrequencies(*it), search_server.GetWordFrequencies(*jt))) {
					cout << "Found duplicate document id "s << *jt << endl;
					ids_to_remove.insert(*jt);
				}
			}
		}
	}
	for (int id : ids_to_remove) {
		search_server.RemoveDocument(id);
	}
}