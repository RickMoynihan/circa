#ifndef CIRCA__TERM__INCLUDED
#define CIRCA__TERM__INCLUDED

#include "common_headers.h"
#include "term_list.h"

struct Branch;
struct Type;

typedef std::vector<std::string> ErrorList;

struct TermData
{
    Branch* owningBranch;
    TermList inputs;
    Term function;
    std::vector<Term> users;

    void* value;
    Term type;

    Term state;

    bool needsUpdate;

    ErrorList errors;

    int globalID;

    TermData();

    Type* getType() const;
    const char* toString();

    int numErrors() const;
    std::string const& getError(int index);
};

int& as_int(Term t);
float& as_float(Term t);
bool& as_bool(Term t);
string& as_string(Term t);

TermList* as_list(Term term);

void initialize_term(Branch* kernel);

#endif
