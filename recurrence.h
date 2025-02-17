//
// Created by ryan on 12/16/22.
//

#pragma once

#include <string>
#include <vector>
#include <utility>

using std::string, std::vector;

class recurrence {
    enum TOKENTYPES {
        PLUS,
        MINUS,
        TIMES,
        DIV,
        LPAR,
        RPAR,
        VAR,
        VAL
    };
    using token = std::pair<TOKENTYPES, double>;
    string eqn;
    vector<token> tokens;
    vector<token> pf;
    double N0; // initial recurrence value
    double (*jit_compute) (double nLast, size_t nIter /* = 1*/) = nullptr;
    size_t codesize = 0;

    static void print_tokens(const vector<token> &tkns);
public:
    explicit recurrence(string formula, double init_cond = 0);
    ~recurrence();

    double compute(size_t nIter, bool use_jit= true);

    void postfix_optimize();

    void jit_compile();

    void print_toks() { print_tokens(tokens); }
    void print_pf() { print_tokens(pf); }
};
