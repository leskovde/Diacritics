#pragma once
#include <string>
#include "LookupStructures.h"

void merge_dictionaries(const std::string&, const std::string&);

void compress_model_to_4_b(const std::string&);

mutable_offset_table generate_compressed_model(const std::string&);

mutable_offset_table load_compressed_model(const std::string& filename);

mutable_word_mapping load_word_mapping(const std::string& filename);

// NOT USED - TAKES UP TOO MUCH MEMORY
void load_trigram_model(const std::string&);
