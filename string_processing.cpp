#include "string_processing.h"

std::vector<std::string_view> SplitIntoWords(const std::string_view& text) {
    std::vector<std::string_view> words;
    size_t pos = 0;
    const size_t pos_end = text.npos;
    while(true){
        size_t space = text.find(' ', pos);
        if(space == pos_end) {text.substr(pos);} 
        words.push_back(text.substr(pos, space - pos));
    
    if (space == pos_end) {
        break;
    } else {
        pos = space + 1;
    }
    }
    return words;
}