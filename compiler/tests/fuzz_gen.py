#!/usr/bin/env python3
# Fuzz generator berseed v2.22.0 (Phase 71, RFC-0010 slice 1).
# Stdlib saja. Menghasilkan program .ns bounded-terminating
# (loop countdown literal, tanpa input, tanpa rekursi) + manifest
# "<file> ok|bad" ke stdout. Defect (20%): var takdeklarasi /
# mismatch i64-vs-string; oracle ada di e2e_verify.sh.
# Variabel dilacak tipenya (i64/bool) agar program ok-tagged
# valid secara tipe.
import random
import sys


def gen_i64(rng, ints, depth):
    pool = [str(rng.randint(0, 50))] + list(ints)
    if depth < 2 and rng.random() < 0.6:
        return "(%s %s %s)" % (gen_i64(rng, ints, depth + 1),
                               rng.choice(["+", "-", "*"]),
                               gen_i64(rng, ints, depth + 1))
    return rng.choice(pool)


def gen_bool(rng, ints, bools, depth):
    if bools and rng.random() < 0.4:
        return rng.choice(list(bools))
    return "(%s %s %s)" % (gen_i64(rng, ints, depth + 1),
                           rng.choice(["<", ">", "==", "!="]),
                           gen_i64(rng, ints, depth + 1))


def gen_block(rng, ints, bools, counter, allow_while, frozen=()):
    # readable = assignable + const beku; assignment hanya ke assignable.
    read = list(ints) + list(frozen)
    lines = []
    for _ in range(rng.randint(1, 4)):
        r = rng.random()
        if r < 0.3 or (not read and not bools):
            if rng.random() < 0.7 or not bools:
                v = "v%d" % counter[0]
                counter[0] += 1
                lines.append("let %s = %s;" % (v, gen_i64(rng, read, 0)))
                ints.append(v)
            else:
                v = "b%d" % counter[0]
                counter[0] += 1
                lines.append("let %s = %s;" % (v, gen_bool(rng, read, bools, 0)))
                bools.append(v)
        elif r < 0.45 and ints:
            lines.append("%s = %s;" % (rng.choice(list(ints)),
                                       gen_i64(rng, read, 0)))
        elif r < 0.6:
            pool = read + list(bools)
            lines.append("print(%s);" % (rng.choice(pool) if pool
                                         else rng.randint(0, 9)))
        elif r < 0.78:
            t = gen_block(rng, list(ints), list(bools), counter, False, frozen)
            lines.append("if %s {" % gen_bool(rng, read, bools, 0))
            lines += ["    " + x for x in t]
            if rng.random() < 0.5:
                e = gen_block(rng, list(ints), list(bools), counter, False,
                              frozen)
                lines.append("} else {")
                lines += ["    " + x for x in e]
            lines.append("}")
        elif allow_while:
            c = "_c%d" % counter[0]
            counter[0] += 1
            lines.append("let %s = %d;" % (c, rng.randint(1, 4)))
            # c MASUK frozen (bukan assignable): body tak boleh
            # meng-clobber countdown (non-terminasi, temuan Phase 80).
            body = gen_block(rng, list(ints), list(bools),
                             counter, False, frozen + [c])
            body.append("%s = %s - 1;" % (c, c))
            lines.append("while %s > 0 {" % c)
            lines += ["    " + x for x in body]
            lines.append("}")
        else:
            lines.append("print(%s);" % gen_i64(rng, read, 0))
    return lines


def gen_program(rng, idx):
    bad = rng.random() < 0.2
    ints, bools, counter = [], [], [0]
    frozen = []
    lines = []
    if rng.random() < 0.4:
        lines.append("const K%d: i64 = %d;" % (idx, rng.randint(1, 99)))
        frozen.append("K%d" % idx)
    helper = rng.random() < 0.4
    if helper:
        lines.append("fn h%d(p: i64, q: i64) -> i64 {" % idx)
        lines.append("    return (p + q * 2);")
        lines.append("}")
    lines.append("fn main() {")
    for x in gen_block(rng, ints, bools, counter, True, frozen):
        lines.append("    " + x)
    read = list(ints) + frozen
    if helper and read:
        lines.append("    print(h%d(%s, %s));" % (
            idx, gen_i64(rng, read, 0), gen_i64(rng, read, 0)))
    pool = read + list(bools)
    if pool and rng.random() < 0.7:
        lines.append("    print(%s);" % rng.choice(pool))
    if bad:
        if rng.random() < 0.5:
            lines.append("    print(undeclared_%d);" % idx)
        else:
            lines.append('    let bad%d: i64 = "boom";' % idx)
            lines.append("    print(bad%d);" % idx)
    lines.append("}")
    lines.append("")
    return "\n".join(lines), bad


def main():
    seed = int(sys.argv[1]) if len(sys.argv) > 1 else 71
    count = int(sys.argv[2]) if len(sys.argv) > 2 else 40
    outdir = sys.argv[3] if len(sys.argv) > 3 else "."
    rng = random.Random(seed)
    for i in range(count):
        src, bad = gen_program(rng, i)
        name = "prog_%03d.ns" % i
        with open("%s/%s" % (outdir, name), "w") as f:
            f.write(src)
        print("%s %s" % (name, "bad" if bad else "ok"))


if __name__ == "__main__":
    main()
