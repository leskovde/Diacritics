#pragma once
#include <fstream>

class word_mapping;
void parse_corpus_in_range(const std::string&, size_t, size_t);

void parse_corpus(std::wifstream&, std::wofstream&);

size_t count_lines(std::wifstream&);

void create_trigram_model(const std::string&, word_mapping&, bool);
