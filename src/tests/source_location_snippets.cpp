// Copyright (c) 2007-2009 Andrew Fischer. All rights reserved.

#include <circa.h>

namespace circa {
namespace source_location_snippets {

// In these tests we are looking at the source locations of compiled code. When
// we parse code, the parser is supposed to save the original location of every
// expression.
//
// So specifically:
//
// 1. We verify that every term has a source location attached.
// 2. We create a 2d 'canvas', where we paint the source code of each expression
//    using its saved location. While painting, we make sure that there are no
//    conflicts.

bool source_location_sanity_check(Term* term, std::string& failureReason)
{
    if (!has_source_location_defined(term)) {
        failureReason = "no source location defined";
        return false;
    }

    int colStart = term->intProp("colStart");
    int lineStart = term->intProp("lineStart");
    int colEnd = term->intProp("colEnd");
    int lineEnd = term->intProp("lineEnd");

    // make sure that the 'end' location is past the 'start' location.
    if (colEnd < colStart) {
        failureReason = "colEnd is before colStart";
        return false;
    }

    if (colEnd == colStart && lineEnd < lineStart) {
        failureReason = "lineEnd is before lineStart";
        return false;
    }
        
    if (colEnd == colStart && lineStart == lineEnd) {
        failureReason = "start and end are same location";
        return false;
    }

    return true;
}

struct SourceCodePainter
{
    struct Character {
        char c;
        Term* firstTouched;

        // these are used if there is an error:
        char c_conflict;
        Term* term_conflict;

        Character() : c(' '), firstTouched(NULL), c_conflict(' '), term_conflict(NULL) {}
    };

    typedef std::vector<Character> Line;
    std::vector<Line> m_lines;

    void initialize(int col, int row)
    {
        while ((unsigned) row >= m_lines.size())
            m_lines.push_back(Line());

        while ((unsigned) col >= m_lines[row].size())
            m_lines[row].push_back(Character());
    }


    bool paint(Term* term, std::string& failureMessage)
    {
        int colStart = term->intProp("colStart");
        int lineStart = term->intProp("lineStart");

        std::string termSource = get_term_source(term)
            + term->stringPropOptional("syntaxHints:lineEnding", "");

        bool encounteredError = false;

        for (unsigned i=0; i < termSource.length(); i++) {
            int row = lineStart;
            int col = colStart+i;
            char c = termSource[i];

            initialize(col,row);

            Character& character = m_lines[row][col];

            // if this character is untouched then paint it without problems
            if (character.firstTouched == NULL) {
                character.c = c;
                character.firstTouched = term;
            } else {
                // It's okay to repaint an character with the same thing: this
                // is expected with nested expressions. But make sure that we
                // really are repainting with the same thing.
                if (character.c != c) {
                    character.c_conflict = c;
                    character.term_conflict = term;
                    encounteredError = true;
                }
            }
        }

        if (encounteredError) {
            std::stringstream out;
            out << "Conflict while doing a source-location paint:" << std::endl;
            out << "Text so far:  " << getLine(1) << std::endl;
            out << "Conflicts:    " << getLineOfXsForConflcits(1) << std::endl;
            out << "Term ";
            out.width(7);
            out << format_global_id(term);
            out.width(0);
            out << ": " << getLineFromConflict(1) << std::endl;
            failureMessage = out.str();
            return false;
        }
        return true;
    }

    std::string getLine(int row) {
        std::stringstream out;
        for (unsigned i=0; i < m_lines[row].size(); i++)
            out << m_lines[row][i].c;
        return out.str();
    }
    std::string getLineOfXsForConflcits(int row) {
        std::stringstream out;
        for (unsigned i=0; i < m_lines[row].size(); i++) {
            if (m_lines[row][i].term_conflict != NULL)
                out << "X";
            else
                out << " ";
        }
        return out.str();
    }
    std::string getLineFromConflict(int row) {
        std::stringstream out;
        for (unsigned i=0; i < m_lines[row].size(); i++)
            out << m_lines[row][i].c_conflict;
        return out.str();
    }
    std::string getResult() {
        std::stringstream out;
        for (unsigned row=0; row < m_lines.size(); row++) {
            if (row != 0) out << "\n";
            for (unsigned col=0; col < m_lines[row].size(); col++)
                out << m_lines[row][col].c;
        }
        return out.str();
    }
};

void test_snippet(std::string const& text)
{
    Branch branch;

    parser::compile(&branch, parser::statement_list, text);

    // Check if source location is defined at all.
    for (int i=0; i < branch.length(); i++) {
        Term* term = branch[i];
        std::string failureReason;
        if (!source_location_sanity_check(term, failureReason)) {
            std::cout << "Source location test failed: " << failureReason << std::endl;
            std::cout << "<<<" << std::endl;
            std::cout << term_to_raw_string_with_properties(term);
            std::cout << ">>>" << std::endl;
            declare_current_test_failed();
            return;
        }
    }

    // Test source-code painting
    SourceCodePainter canvas;
    for (int i=0; i < branch.length(); i++) {
        std::string failureReason;
        bool success = canvas.paint(branch[i], failureReason);
        if (!success) {
            std::cout << failureReason;
            std::cout << "Final text:   " << text << std::endl;
            std::cout << "<<<" << std::endl;
            std::cout << print_branch_raw_with_properties(branch);
            std::cout << ">>>" << std::endl;
            declare_current_test_failed();
            return;
        }
    }
}

void test_simple()
{
    test_snippet("1");
    test_snippet("1 + 2");
    test_snippet("1 / 2 + 3");
}

void test_identifiers()
{
    test_snippet("a = 1");
    test_snippet("a = 1; add(a,2)");
    test_snippet("   a = 1;   add(a  ,  2)  ");
    test_snippet("hello = 1; add(2, @hello)");
}

void test_lists()
{
    test_snippet("[]");
    test_snippet("[1 2 3]");
    test_snippet("potatoes = ['potato' 'potato' 'potato']");
}

void test_blocks()
{
    test_snippet("def f() 1 2 3 end");
}

void register_tests() {
    REGISTER_TEST_CASE(source_location_snippets::test_simple);
    REGISTER_TEST_CASE(source_location_snippets::test_identifiers);
    REGISTER_TEST_CASE(source_location_snippets::test_lists);
    REGISTER_TEST_CASE(source_location_snippets::test_blocks);
}

} // namespace source_location_snippets
} // namespace circa
