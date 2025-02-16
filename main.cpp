/*
 * Simple JIT: a simple JIT to parse + compute recursive sequences
 */
#include <iostream>
#include <string>
#include "recurrence.h"

using namespace std;

[[noreturn]]
static void usage(const char *progname) {
        cerr << "usage: " << progname
                << " [equation [n_iter [N_0]]]"
                << endl;
        exit(1);
}

int main(int argc, char *argv[]) {
        // test with (((54 + 3) / 8) - (4 * 2)) + n
        // output: [54, 3, +, 8, /, 4, 2, *, -, n, +]
        string expr = "(((54 + 3) / 8) - (4 * 2)) + n";
        size_t nIter = 100000;
        double N_0 = 0;

        try {
                switch (argc) {
                    case 4:
                        N_0 = stod(argv[3]);
                    case 3:
                        nIter = stoul(argv[2]);
                    case 2:
                        expr = argv[1];
                    case 1:
                        break;
                    default:
                        usage(argv[0]);
                }
        } catch (invalid_argument &e) {
                cerr << e.what() << endl;
                usage(argv[0]);
        }

        
        recurrence r(expr);
//        r.jit_compile();
//        r.print_toks();
//        r.print_pf();
        cout << "with JIT: "
                << r.compute(nIter) << endl;
        fflush(nullptr);
        // cout << "without JIT: "
        //         << r.compute(nIter, false) << endl;
        return 0;
}
