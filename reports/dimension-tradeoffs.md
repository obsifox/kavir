# Dimension trade-offs

Table of dimensions that pull against each other, with claim IDs. Decisions come later as ADRs (Phase 2).

| # | Trade-off | Side A (evidence) | Side B (evidence) | Notes for Kavir |
|---|---|---|---|---|
| 1 | Performance vs memory-safety | Manual/unsafe code buys speed but produces ~70% of serious CVEs (C-104, C-105); safe Rust adds checks at no runtime cost but compile-time friction (C-601, C-112) | Unsafe escape hatches keep peak perf (C-108, C-111) | Explicit small unsafe boundary (H C-226 context) |
| 2 | Safety vs C-FFI ergonomics | C ABI is the interop substrate (C-114, C-120); interop is the recurring adoption cost (C-107) | Wrapping C safely costs abstractions (C-120) | Design FFI surface before v1 (charter H2) |
| 3 | Compile speed vs runtime code quality | Cranelift/QBE: ~10x faster codegen, ~14% slower code (C-405, C-413); 20-30% faster debug builds (C-417) | LLVM optimizes better (C-405); Go accepted SSA-backend cost for control (C-419, C-420) | Two-tier backend hypothesis (C-425) |
| 4 | Compile speed vs generics strategy | Per-instantiation generics bloat binaries/compile (C-309, S-312); NativeAOT disk impact (C-210) | Erasure/boxing costs runtime perf (C-217) | Decide compilation strategy pre-1.0 (go-generics-delay CS) |
| 5 | Soundness vs productivity (types) | TS/Python: unsound-but-free gradual typing adopted widely (C-301, C-302, C-319) | Sound gradual typing up to 100x cast cost (C-305); TS bug gains unproven (C-306) | Soundness contract decided up front (C-320) |
| 6 | GC latency vs pause-free predictability | ZGC sub-ms pauses vendor-claimed (C-613, C-211); generational GC 4x throughput (C-212) | Ownership/ARC: no pauses but lifetime friction/cycles (C-601, C-608); real-time GC suitability unverified (conflict #9) | Hybrid core hypothesis (C-623) |
| 7 | Concurrency: prevention vs ergonomics | Ownership/Send prevents races by construction (C-617) | Channels/threads simpler but races remain (C-618, C-619); async adds complexity (C-615, C-113) | Prevention + race-detector backstop (H C-623 context) |
| 8 | Portability vs hardware control | Fixed portable core avoids TSO/weak-model bugs (C-510, C-511, C-514); alignment semantics (C-512) | Bare-metal needs layout/entry/linker control (C-516, C-517) | Portable core + explicit repr/FFI layer (C-519, C-520) |
| 9 | ABI stability vs evolution freedom | Stability enables OS embedding/binary libs (C-220) | Even conservative ecosystems take managed breaks (C-518); RISC-V draft instability bites FFI (C-504) | Version ABI explicitly from day one (riscv-psabi CS) |
| 10 | Simplicity vs expressiveness (learnability) | Go/Lua omission strategy correlated with adoption (C-308, C-317); learnability gates adoption (C-112) | Ruby "natural not simple" also succeeded (S-317); omissions cost a decade retrofit (C-308) | Decide audience first (Phase 2 ADR #1) |
| 11 | Batteries-included vs minimal stdlib | Rich stdlib drives adoption (C-318 context) | Every module is permanent maintenance (C-716); Go froze APIs early (C-715) | Layered stdlib (core/alloc/std pattern, C-516 context) |
| 12 | VM portability vs AOT/deployment economics | JVM/.NET hosting is language-independent (C-201, C-203) | Startup/warmup cost real for some workloads (C-205); AOT requires closed-world (C-207) or no-emit (C-210) | Execution-model contract at v1 (C-225) |
| 13 | Registry openness vs supply-chain integrity | npm-style openness enables fast growth (C-706 context) | left-pad/event-stream incidents (C-710, C-711); checksum DBs work (C-705, C-717) | Integrity by default (C-730) |
| 14 | Breaking evolution vs compatibility debt | Editions/migrations let languages move (C-714) | Breaking resets cost years of trust (C-719, C-722); compat constraints bind forever (C-115, C-220) | Compatibility promise written early (C-731) |
