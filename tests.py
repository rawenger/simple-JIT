#!/usr/bin/pypy3

def r1(lastN: float = 0):
    return (((54 + 3) / n) - (4 * 2)) + lastN

def r2(lastN: float = 0):
    return (10 + n) / ((5 - (7 + (n * 6))) + 76)

if __name__ == "__main__":
    nIter = 100000
    n = 1
    # nIter = int(input("Number of iterations? "))
    for i in range(nIter):
        n = r1(n)
    print(n)