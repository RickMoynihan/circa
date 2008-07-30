#ifndef CIRCA__TERM__INCLUDED
#define CIRCA__TERM__INCLUDED

#include <vector>

class Branch;
class Type;
class Term;

typedef std::vector<std::string> ErrorList;

struct TermList
{
    std::vector<Term*> items;

    TermList() { }

    // Convenience constructors
    TermList(Term* term) {
        items.push_back(term);
    }
    TermList(Term* term1, Term* term2) {
        items.push_back(term1);
        items.push_back(term2);
    }
    TermList(Term* term1, Term* term2, Term* term3) {
        items.push_back(term1);
        items.push_back(term2);
        items.push_back(term3);
    }

    int count() const;
    void append(Term* term);
    void setAt(int index, Term* term);
    void clear();
    Term* get(int index) const;
    Term* operator[](int index) const;
};

struct Term
{
    Branch* owningBranch;
    TermList inputs;
    Term* function;
    std::vector<Term*> users;

    void* value;
    Term* type;

    Term* state;

    bool needsUpdate;

    ErrorList errors;

    int globalID;

    Term();

    Type* getType() const;
    const char* toString();

    int numErrors() const;
    std::string const& getError(int index);
};

int& as_int(Term* t);
float& as_float(Term* t);
bool& as_bool(Term* t);
string& as_string(Term* t);

TermList* as_list(Term* term);

void initialize_term(Branch* kernel);

#endif
