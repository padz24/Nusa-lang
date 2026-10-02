<!-- antislop:start -->
## antislop
For UI, copy, people, mobile layout, or code comments work, read `antislop.md` (core) and then the skill for the task:
- UI / visual: `skills/antislop-ui/SKILL.md`
- Copy & text: `skills/antislop-copywriting/SKILL.md`
- People: `skills/antislop-human/SKILL.md`
- Mobile / responsive: `skills/antislop-layoutmobile/SKILL.md`
- Code comments: `skills/antislop-code/SKILL.md`
Before starting, ask the user when antislop applies: during the work, or after it is done.
<!-- antislop:end -->

## Nusa compiler repo — agent notes

Compiled language (`.ns` → native via LLVM 21.1.8), Termux Android AArch64.
Version is single-sourced: `NUSA_VERSION_STR` in `CMakeLists.txt`. No git repo here — no commits.

### Build (always `-j2`; higher parallelism OOMs this device)

```bash
cmake -S . -B build                                   # once / after editing CMakeLists.txt
cmake --build build -j2 --target nusa test_foo        # binary + named unit tests
```

### Tests (95 via ctest; full suite takes ~30+ min — run focused)

```bash
bash compiler/tests/e2e_foo.sh "$PWD/build/nusa"     # single e2e, from repo root
./build/test_foo                                      # single unit binary
(cd build && ctest -R 'test_orc|e2e_abi')             # ctest subset
```

- e2e scripts take the binary as `$1`, are self-contained in `mktemp -d TMPDIR`, and must use `timeout` guards (hanging `.ns` kills the session). Follow the `fail`/`pass` helper pattern.
- Never delete or weaken regression tests; fix the code or narrow the change instead.
- Flaky under parallel load (`e2e_control`, fuzz oracles): rerun isolated before concluding.
- Slow suites: `e2e_verify` (fuzz), `e2e_release` (builds whole corpus), full `ctest`.

### Per-change checklist (from the MASTER PROMPT phase loop)

1. One phase/milestone at a time, then STOP and report — never auto-continue.
2. Language semantics changes require an RFC first (`rfcs/RFC-NNNN-slug.md` + README index; enforced by `e2e_rfc.sh`). Refactors, opts, and tooling don't.
3. Update `CHANGELOG.md` (top `[Unreleased: …]` section), the relevant `docs/<topic>-<ver>.md`, then `python3 scripts/gen.py` + `python3 scripts/gen.py --check` (must print `no drift`; every `docs/*.md` becomes a website page automatically).

### Layout

- `compiler/{lexer,parser,ast,semantic,codegen}` — pipeline; `compiler/mir` — isolated MIR (std-only, no LLVM includes; enforced by `e2e_mir.sh`); `compiler/{driver,lsp,pkg,fmt,bindgen,plugin,doc,incr}` — tooling.
- `runtime/rt_*.c` (single TU via `nusa_rt.c`), `std/*.ns` (32 modules), `selfhost/`, `tests/*_test.ns` (run via `nusa test`), `realworld/`, `bench/`, `examples/`, `tutorials/`.
- Build artifacts (`*.ns.ll`, `.nusa-cache/`, `*.run.out`, `build/`) are untracked — clean up strays you create; `e2e_release.sh` snapshots `.ns.ll` so don't leave new ones in the corpus.

### Writing `.ns` test programs (non-obvious rules)

- `fn main` is mandatory everywhere (even `-c` objects); `match` is statement-only with block arms (`Some(v) => { return v; },`) plus a trailing `return` for all-paths checking.
- Variant patterns are unqualified (`Some(v)`, not `E.Some(v)`); constructors are qualified (`E.Some(7)`); no turbofish (`id(x)`, never `id<i64>(x)`).
- `bool` is `i1` internally but `i32` across `extern fn` — never expose `bool` in C headers; closures are `{ptr,ptr}` pairs, not C function pointers.
- `nusa fmt` has known quirks: `a < (b)` formats to `a <(b)`; spaced generics (`f < T: Show >`) confuse the generic detector. Don't "fix" test files to match quirks outside your task.
