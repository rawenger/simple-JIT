/*
 * Simple JIT: a simple JIT to parse + compute recursive sequences
 */
#include <iostream>
#include <string>
#include "recurrence.h"

int main() {
        std::string expr = "(((54 + 3) / 8) - (4 * 2)) + n";
        int nIter = 100000;
        std::cout << "Enter expression: \n";
//        std::cout << expr << std::endl;
        // test with (((54 + 3) / 8) - (4 * 2)) + n
        // output: [54, 3, +, 8, /, 4, 2, *, -, n, +]
        std::getline(std::cin, expr);
        std::cout << "Number of iterations?\n";
        std::cin >> nIter;
        recurrence r(expr);
//        r.jit_compile();
//        r.print_toks();
//        r.print_pf();
        std::cout << r.compute(nIter) << std::endl;
        fflush(nullptr);
        std::cout << r.compute(nIter, false) << std::endl;
        return 0;
}
