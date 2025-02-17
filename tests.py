#!/usr/bin/env pypy3

def r1(n: float = 0):
    return (((54 + 3) / n) - (4 * 2)) + n

def r2(n: float = 0):
    return (10 + n) / ((5 - (7 + (n * 6))) + 76)

def r3(n: float = 0):
    return (((54 + n) / (3 * n)) - (4 * 2))


def main():
    nIter = 10000000000
    n = 1
    # nIter = int(input("Number of iterations? "))
    for i in range(nIter):
        n = r3(n)
    print(n)


if __name__ == "__main__":
    main()