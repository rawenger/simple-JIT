//
// Created by ryan on 12/16/22.
//

#include <iostream>
#include <stack>
#include <algorithm>

#include <unistd.h>
#include <sys/mman.h>

#include "recurrence.h"

#if defined(__x86_64__)
#   define x86_64   1
#   define arm64    0
#elif defined(__aarch64__)
#   define x86_64   0
#   define arm64    1
#elif
#   error "only 64-bit ARM and x86 are supported."
#endif


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
    stack<TOKENTYPES> ops;
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
          case LPAR:
            ++par_depth;
            break;
          case RPAR:
            --par_depth;
            postfix.emplace_back(ops.top(), 0);
            ops.pop();
            break;
          case VAR:
            postfix.emplace_back(VAR, 0);
            break;
          case VAL:
            postfix.emplace_back(VAL, it->second);
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

// arm codegen embeds loop in JITted code, x86 does not
#if x86_64
        for (int i = 0; i < nIter; i++)
            nLast = jit_compute(nLast, 1);
#elif arm64
        nLast = jit_compute(nLast, nIter);
#endif
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

#if x86_64
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

        // 0x66, 0x48, 0x0f, 0x7e, 0xc0, // movq   %xmm0, %rax
        // 0x66, 0x48, 0x0f, 0x6e, 0xc0, // movq   %rax, %xmm0
        // 0x5d,                         // pop    %rbp
        // 0xc3,                         // ret
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

    jit_compute = reinterpret_cast<double (*)(double, size_t)>(codepage);
}


#elif arm64
void recurrence::jit_compile()
{
    /* we will be called as though our prototype is
     *      double (double N0, size_t n_iter);
     *
     * - N0 will be in d0, n_iter in x0
     * - return value also in d0
     * - d1 & d2 are scratch FP registers used for the actual
     *  computation
     *
     * note that aarch64 macOS will pagefault upon accessing a
     * non-16 byte aligned stack, so we have to adhere to that.
     */
    vector<uint32_t> code {
        0xa9bf7bfd,     // stp lr, fp, [sp, #-16]!
        0xb5000060,     // cbnz x0, 1f (+3)
        0xa8c17bfd,     // ldp lr, fp, [sp], #16
        0xd65f03c0,     // ret
                        // 1:
        0x6dbf0be1,     // stp d1, d2, [sp, #-16]!
        0xaa0003e9,     // mov  x9, x0

        // loop_iter:
        // calculate number of instructions between here
        // and the branch at the end so we know what operand
        // to give as the branch target
        // 

        // <GENERATED CODE GOES HERE>


        // 0xd1000529,      // sub x9, x9, #1
        // 0xb5 + 19-bits + 0b01001 // cbnz  x9, <# instructions back to loop start>

        // 0x6cc10be1,      // ldp d1, d2, [sp], #16
        // 0xa8c17bfd,      // ldp lr, fp, [sp], #16
        // 0xd65f03c0,      // ret
    };

    // number of instructions that make up the computation loop
    // need this so we know how far back to branch
    ssize_t loop_size = 0;
    auto append_code = [&code, &loop_size] 
        (std::initializer_list<uint32_t> instrs)
    {
        loop_size += instrs.size();
        code.insert(code.end(), std::move(instrs));
    };

    for (const auto &tok : pf) {
        switch (tok.first) {
          case VAR:
            append_code({
                0xfc1f0fe0,     // str d0, [sp, #-16]!
            });
            break;

          case VAL: {
            struct mov_instr {
                uint32_t Rd: 5,
                         imm: 16,
                         sh: 2,
                         opc: 9;
            };
            uint64_t imms = *reinterpret_cast<const uint64_t *>(&tok.second);
            if (imms == 0) {
                append_code({
                    0xf81f0fff, // str xzr, [sp, #-16]!
                });
                break;
            }

            // grab operand as 4 16-bit immediates, load into x10, then save to stack
            bool zeroed = false; // keep track of whether we've zeroed x10 
                                 // already or not
            for (unsigned i = 0; i < 64; i += 16) {
                // movk x10, imm16, lsl #i
                //  <OR>
                // movz x10, imm16, lsl #i
                uint16_t imm16 = (imms >> i) & 0xffff;
                if (!imm16)
                    continue;

                mov_instr instr {
                    .opc = 0,
                    .sh = i / 16,
                    .imm = imm16,
                    .Rd = 10
                };
                if (zeroed) {
                    instr.opc = 0b111100101;
                } else {
                    instr.opc = 0b110100101;
                    zeroed = true;
                }
                append_code({
                    *reinterpret_cast<uint32_t *>(&instr)
                });
            }
            append_code({
                0xf81f0fea, // str x10, [sp, #-16]!
            });
            break;
          }
            /* compute - double version:
             * 1:
             *  
             *
             * ldp      q2, q1, [sp], #32
             * <op>     d1, d1, d2
             * str      d1, [sp, #-16]!
             */
          case PLUS ... DIV: {
            append_code({
                // use q2 and q1 here since they're 16-bytes rather than 8
                // which allows us to do a packed load
                0xacc107e2,     // ldp q2, q1, [sp], #32
            });

            struct {
                uint32_t Rd: 5,
                         Rn: 5,
                         opc: 6,
                         Rm: 5,
                         one: 1,
                         ftype: 2, // 1 for double word
                         misc: 8; // 0b00011110
            } __attribute__((packed)) fpop {
                .misc = 0x1e,
                .ftype = 0b01,
                .one = 0b01,
                .Rm = 2,
                .Rn = 1,
                .opc = 0,
                .Rd = 1,
            };

            // <op> d1, d1, d2
            switch (tok.first) {
              case PLUS:
                fpop.opc = 0b001010;
                break;
              case MINUS:
                fpop.opc = 0b001110;
                break;
              case TIMES:
                fpop.opc = 0b000010;
                break;
              case DIV:
                fpop.opc = 0b000110;
                break;
            }
            
            append_code({
                *reinterpret_cast<uint32_t *>(&fpop),
                0xfc1f0fe1,     // str d1, [sp, #-16]!
            });
            break;
          }
          default:
            std::cerr << "Unknown error occurred. Aborting.\n";
            exit(EXIT_FAILURE);
        }
    }

    append_code({
        0xfc4107e0,      // ldr d0, [sp], #16
        0xd1000529,      // sub x9, x9, #1
    });

    // create our loop branch instruction
    // 0xb5 + 19-bits + 0b01001  -- cbnz  x9, <# instructions back to loop start>
    
    // I prefer bitfield structs over the below. fight me.
    // uint32_t cbnz = (0xb5U << 24) 
    //     | ((cbnz_offset << 5) & ~(0xff << 24))
    //     | 9;
    
    int cbnz_offset = -1 * loop_size;
    struct {
        int32_t Rt: 5, // remember little-endian
                offset: 19,
                opcode: 8;
    } cbnz_instr = {
        .opcode = 0xb5,
        .offset = cbnz_offset,
        .Rt     = 9,
    };

    // don't care about loop_size anymore so ok to use this
    append_code({
        *reinterpret_cast<uint32_t *>(&cbnz_instr),
        0x6cc10be1,      // ldp d1, d2, [sp], #16
        0xa8c17bfd,      // ldp lr, fp, [sp], #16
        0xd65f03c0,      // ret
    });

    this->codesize = code.size() * sizeof (uint32_t);
    uint32_t *codepage = static_cast<uint32_t *>(mmap(nullptr, this->codesize,
                                              PROT_READ | PROT_WRITE,
                                              MAP_ANONYMOUS | MAP_PRIVATE, -1, 0));
    if (codepage == MAP_FAILED) {
        perror("Cannot allocate code page(s) for JIT");
        exit(EXIT_FAILURE);
    }

    // memcpy(codepage, code.data(), codesize);
    std::copy(code.cbegin(), code.cend(), codepage);

    if (mprotect(codepage, this->codesize, PROT_READ | PROT_EXEC) < 0) {
        perror("Unable to mark JIT page as executable");
        exit(EXIT_FAILURE);
    }

    jit_compute = reinterpret_cast<double (*)(double, size_t)>(codepage);
}
#endif

void recurrence::postfix_optimize() {
        // STUB
        return;
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
