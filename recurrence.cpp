//
// Created by ryan on 12/16/22.
//

#include <iostream>
#include <stack>
#include <algorithm>
#include <list>

#include <unistd.h>
#include <sys/mman.h>

#include "recurrence.h"

using std::stack;

recurrence::recurrence(string formula, double init_cond)
    : eqn(std::move(formula)), N0(init_cond)
{
        // tokenize the expression for easier parsing
        for (auto it = eqn.cbegin(); it < eqn.cend(); ++it) {
                switch (*it) {
                    case '(':
                        tokens.emplace_back(LPAR, 0);
                        break;
                    case ')':
                        tokens.emplace_back(RPAR, 0);
                        break;
                    case '+':
                        tokens.emplace_back(PLUS, 0);
                        break;
                    case '-':
                        tokens.emplace_back(MINUS, 0);
                        break;
                    case '*':
                        tokens.emplace_back(TIMES, 0);
                        break;
                    case '/':
                        tokens.emplace_back(DIV, 0);
                        break;
                    case ' ':
                        continue;
                    case '0' ... '9': {
                            long num = 0;
                            while ('0' <= *it && *it <= '9') {
                                    num *= 10;
                                    num += *it - '0';
                                    ++it;
                            }
                            --it;
                            tokens.emplace_back(VAL, num);
                            break;
                    }
                    case 'n':
                    case 'N':
                        tokens.emplace_back(VAR, 0);
                        break;
                    default:
                        std::cerr << "Syntax error: unknown symbol '" << *it << "'\n";
                        exit(EXIT_FAILURE);
                }
        }

        // convert tokens to postfix notation
        // e.g. "(5 + 3) * 4 / (8 - 2)"
        // to  [5, 3, +, 4, *, 8, 2, -, /]
        // or "(5 - (4 + 2)) / (7 - 3)"
        // to [5, 4, 2, +, -, 7, 3, -, /]

        // the challenge is for chained expressions without parens;
        // e.g: "5 + 3 - 4 * 8" needs to become
        // [5, 3, +, 4, 8, *, -]. I think we need to store the "nest level"
        // of each operator, i.e: which level of brackets it occurs in, in order
        // to support these types of expressions. Maybe another time.
        // A consequence of requiring parenthesis aroun everything is we don't
        // have to parse PEMDAS either.
        stack<TOKENTYPE> ops;
        vector<token> postfix;
        postfix.reserve(tokens.size());
        int par_depth = 0;
        for (auto it = tokens.cbegin(); it < tokens.cend(); ++it) {
                // TODO: can convert to move semantics rather than using emplaces, but
                //  not necessary.
                switch (it->first) {
                        case PLUS ... DIV:
                                ops.push(it->first);
                                break;
                        case VAR:
                                postfix.emplace_back(VAR, 0);
                                break;
                        case VAL:
                                postfix.emplace_back(VAL, it->second);
                                break;
                        case LPAR:
                                ++par_depth;
                                break;
                        case RPAR:
                                --par_depth;
                                postfix.emplace_back(ops.top(), 0);
                                ops.pop();
                                break;
                }
                if (par_depth < 0) {
                        std::cerr << "***Parse error: mismatched parenthesis. "
                                     "Aborting.\n";
                        exit(EXIT_FAILURE);
                }
        }

        if (!ops.empty()) {
                postfix.emplace_back(ops.top(), 0);
                ops.pop();
        }

        pf = std::move(postfix);
}

recurrence::~recurrence() {
        if (jit_compute)
                munmap(reinterpret_cast<void *>(jit_compute), this->codesize);
}

double recurrence::compute(size_t nIter, bool use_jit) {
        double nLast = N0;
        if (use_jit) {
                if (!jit_compute)
                        this->jit_compile();
                for (int i = 0; i < nIter; i++)
                        nLast = (*jit_compute)(nLast);
                return nLast;
        }
        // use interpreter
        stack<double> calc;
        double n1, n2;
        for (int i = 0; i < nIter; i++) {
                for (const auto &tok : pf) {
                        switch (tok.first) {
                            case VAR:
                                calc.push(nLast);
                                break;
                            case VAL:
                                calc.push(tok.second);
                                break;
                            case PLUS ... DIV: {
                                n2 = calc.top(); calc.pop();
                                n1 = calc.top(); calc.pop();
                                double compute;
                                switch (tok.first) {
                                    case PLUS:
                                        compute = n1 + n2;
                                        break;
                                    case MINUS:
                                        compute = n1 - n2;
                                        break;
                                    case TIMES:
                                        compute = n1 * n2;
                                        break;
                                    case DIV:
                                        compute = n1 / n2;
                                        break;
                                }
                                calc.push(compute);
                                break;
                            }
                            default:
                                std::cerr << "Unknown error occurred. Aborting.\n";
                                exit(EXIT_FAILURE);
                        }
                }
                nLast = calc.top(); calc.pop();
                if (!calc.empty()) {
                        std::cerr << "***Error: stack not empty! Aborting.\n";
                        exit(EXIT_FAILURE);
                }
        }
        return nLast;
}

// TODO: change movsd instructions to movapd.
//  Note that movapd requires its memory operands to be 16-byte-aligned,
//  and it looks like it may not be worth the trade-off, so maybe we'll
//  only do this for XMM register copies
void recurrence::jit_compile() {
        vector<uint8_t> code {
                0x55,                           // push   %rbp
                0x48, 0x89, 0xe5,               // mov    %rsp, %rbp
                0xf2, 0x0f, 0x10, 0xd0,         // movsd  %xmm0, %xmm2

                // <GENERATED CODE GOES HERE>

//                0x66, 0x48, 0x0f, 0x7e, 0xc0, // movq   %xmm0, %rax
//                0x66, 0x48, 0x0f, 0x6e, 0xc0, // movq   %rax, %xmm0
//                0x5d,                         // pop    %rbp
//                0xc3,                         // ret
        };

        auto append_code = [&code] (std::initializer_list<uint8_t> bytes) {
                code.insert(code.end(), bytes.begin(), bytes.end());
        };

        for (const auto &tok : pf) {
                switch (tok.first) {
                    case VAR:
                        // movsd %xmm2, -0x8(%rsp)
                        append_code({0xf2, 0x0f, 0x11, 0x54, 0x24, 0xf8});

                        // sub $0x8, %rsp
                        append_code({0x48, 0x83, 0xec, 0x08});
                        break;
                    case VAL: {
                            uint64_t operand = *reinterpret_cast<const uint64_t *>(&tok.second);
                            // rather than pushing the immediate, we use the `movabs`
                            // instruction to load the full 64-bit immediate into %rax
                            // and then push %rax onto the stack
                            append_code({0x48, 0xb8});
                            for (int i = 0; i < 8; i++) {
                                    code.push_back(operand & 0xff);
                                    operand >>= 8;
                            }
                            code.push_back(0x50); // push %rax
                            break;
                    }
                    /* compute - integer version:
                     * pop      %rdx
                     * pop      %rax
                     * <op>     %rdx, %rax      ; rax = rax <op> rdx
                     * push     %rax (can skip this on final iteration)
                     *
                     * compute - double version:
                     * movsd    0x0(%rsp), %xmm1
                     * movsd    0x8(%rsp), %xmm0
                     * <op>     %xmm1, %xmm0
                     * add      $0x8, %rsp
                     * movsd    %xmm0, 0x0(%rsp)
                     */
                    case PLUS ... DIV: {
                        // movsd 0x0(%rsp), %xmm1
                        append_code({0xf2, 0x0f, 0x10, 0x0c, 0x24});
                        // movsd 0x8(%rsp), %xmm0
                        append_code({0xf2, 0x0f, 0x10, 0x44, 0x24, 0x08});

                        // <op> %xmm1, %xmm0
                        append_code({0xf2, 0x0f});
                        switch (tok.first) {
                            case PLUS:
                                code.push_back(0x58);
                                break;
                            case MINUS:
                                code.push_back(0x5c);
                                break;
                            case TIMES:
                                code.push_back(0x59);
                                break;
                            case DIV:
                                code.push_back(0x5e);
                                break;
                        }
                        code.push_back(0xc1);

                        // add $0x8, %rsp
                        append_code({0x48, 0x83, 0xc4, 0x08});

                        // movsd %xmm0, 0x0(%rsp)
                        append_code({0xf2, 0x0f, 0x11, 0x04, 0x24});
                        break;
                    }
                    default:
                        std::cerr << "Unknown error occurred. Aborting.\n";
                        exit(EXIT_FAILURE);
                }
        }

        // movq   %xmm0,%rax
        append_code({0x66, 0x48, 0x0f, 0x7e, 0xc0});

        // movq   %rax,%xmm0
        append_code({0x66, 0x48, 0x0f, 0x6e, 0xc0});

        // mov  %rbp, %rsp
        append_code({0x48, 0x89, 0xec});

        // pop    %rbp
        code.push_back(0x5d);

        // ret
        code.push_back(0xc3);

        this->codesize = code.size();
        auto *codepage = static_cast<uint8_t *>(mmap(nullptr, this->codesize,
                                                  PROT_READ | PROT_WRITE,
                                                  MAP_ANONYMOUS | MAP_PRIVATE, -1, 0));
        if (codepage == MAP_FAILED) {
                perror("Cannot allocate code page(s) for JIT");
                exit(EXIT_FAILURE);
        }

        std::copy(code.cbegin(), code.cend(), codepage);

        if (mprotect(codepage, this->codesize, PROT_READ | PROT_EXEC) < 0) {
                perror("Unable to mark JIT page as executable");
                exit(EXIT_FAILURE);
        }

        jit_compute = reinterpret_cast<double (*)(double)>(codepage);
}

// TODO: honestly easiest thing may be to just use an AST rather than a vector to store the eqn.
//  Not necessary to compute, but trying to do basic CAS manipulation on a linear container SUCKS lol

// This routine attempts to optimize the postfix format produced by the constructor, essentially
// by precomputing whatever it can.
// For example, "(((54 + 3) / n) - (4 * 2)) + n" becomes "((57 / n) - 8) + n",
// or in postfix notation, [54, 3, +, n, /, 4, 2, *, -, n, +] becomes [57, n, /, 8, -, n, +]
void recurrence::postfix_optimize() {
        // We use a pretty naive algorithm to do this: essentially just compute the expression
        // as though we are using an interpreter, and whenever we encounter a VAR token, we write
        // the top of the computation stack, that VAR, and the corresponding OP to our postfix expr
        std::list<token> new_pf{pf.begin(), pf.end()};
        vector<const token*> calc;
        vector<TOKENTYPE> ops;
        const token *n1, *n2;
        for (const auto &tok : new_pf) {
                switch (tok.first) {
                    case VAR:
                    case VAL:
                        calc.push_back(&tok);
                        break;
                    case PLUS ... DIV: {
                        n2 = calc.back(); calc.pop_back();
                        n1 = calc.back(); calc.pop_back();
                        std::initializer_list<token> tks;
                        if (n1->first == VAL && n2->first == VAL) {
                                double constant;
                                switch (tok.first) {
                                    case PLUS:
                                        constant = n1->second + n2->second;
                                        break;
                                    case MINUS:
                                        constant = n1->second - n2->second;
                                        break;
                                    case TIMES:
                                        constant = n1->second * n2->second;
                                        break;
                                    case DIV:
                                        constant = n1->second / n2->second;
                                        break;
                                }
                                tks = {{VAL, constant}};
                                calc.push_back(tks.begin());
                        } else {
                                calc.push_back(n1);
                                calc.push_back(n2);
                                ops.push_back(tok.first);
//                                if (n1->first == VAR && n2->first == VAR)
//                                        tks = {*n1, *n2, tok};
//                                else if (n1->first == VAR)
//                                        tks = {*n1, tok};
//                                else
//                                        tks = {*n2, tok};
                        }

//                        new_pf.insert(new_pf.end(), tks.begin(), tks.end());
                        break;
                    }
                    default:
                        std::cerr << "Unknown error occurred. Aborting.\n";
                        exit(EXIT_FAILURE);
                }
        }
        for (auto op :) {
                new_pf.push_back(*calc.top()); calc.pop();
                new_pf.push_back(*calc.top()); calc.pop();
                new_pf.emplace_back(ops.top(), 0); ops.pop();
        }
        print_tokens(new_pf);
        if (!calc.empty()) {
                std::cerr << "***Error: stack not empty! Aborting.\n";
                exit(EXIT_FAILURE);
        }


}

void recurrence::print_tokens(const vector<token> &tkns) {
        for (auto tok : tkns) {
                std::cout << '(';
                switch (tok.first) {
                        case PLUS:
                                std::cout << "PLUS";
                                break;
                        case MINUS:
                                std::cout << "MINUS";
                                break;
                        case TIMES:
                                std::cout << "TIMES";
                                break;
                        case DIV:
                                std::cout << "DIV";
                                break;
                        case LPAR:
                                std::cout << "LPAR";
                                break;
                        case RPAR:
                                std::cout << "RPAR";
                                break;
                        case VAR:
                                std::cout << "VAR";
                                break;
                        case VAL:
                                std::cout << "VAL";
                                break;
                }
                std::cout << ',' << tok.second << ") ";
        }
        std::cout << std::endl;
}
