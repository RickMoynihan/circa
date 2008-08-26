#ifndef CIRCA__TOKENIZER__INCLUDED
#define CIRCA__TOKENIZER__INCLUDED

#include "common_headers.h"

namespace circa {
namespace tokenizer {

struct TokenInstance
{
    const char * match;
    std::string text;
    int line;
    int character;

    TokenInstance() : match(NULL), line(0), character(0) {}
};

typedef std::vector<TokenInstance> TokenList;

extern const char * LPAREN;
extern const char * RPAREN;
extern const char * COMMA;
extern const char * EQUALS;
extern const char * IDENTIFIER;
extern const char * INTEGER;
extern const char * FLOAT;
extern const char * STRING;
extern const char * QUOTED_IDENTIFIER;
extern const char * WHITESPACE;
extern const char * NEWLINE;
extern const char * UNRECOGNIZED;

void tokenize(std::string const &input, TokenList &results);
TokenList tokenize(std::string const& input);

} // namespace tokenizer
} // namespace circa

#endif
