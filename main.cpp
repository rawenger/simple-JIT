/*
 * Simple JIT: a simple JIT to parse + compute recursive sequences
 */
#include <iostream>
#include <string>
#include "recurrence.h"


// upper bound to compare our optimization to GCC's
#pragma GCC optimize ("O3")
// leave the overhead of a function call to make it as accurate as possible
[[gnu::noinline]]
double r1(double lastN = 0) {
        return (((54 + 3) / lastN) - (4 * 2)) + lastN;
}

int main() {
        std::string expr = "(((54 + 3) / n) - (4 * 2)) + n";
        int nIter = 10000000;
        double lastN = 1;
        std::cout << "Enter expression: \n";
//        std::cout << expr << std::endl;
        // test with (((54 + 3) / 8) - (4 * 2)) + n
        // output: [54, 3, +, 8, /, 4, 2, *, -, n, +]
//        std::getline(std::cin, expr);
        std::cout << "Number of iterations?\n";
//        std::cin >> nIter;
        recurrence r(expr, lastN);
        r.jit_compile();
//        r.print_toks();
//        r.print_pf();
        std::cout << r.compute(nIter) << std::endl;
//        fflush(nullptr);
//        for (int i = 0; i < nIter; i++)
//                lastN = r1(lastN);
//        std::cout << lastN << std::endl;
//        std::cout << r.compute(nIter, false) << std::endl;
        return 0;
}
