# ADR-009: Kavir source file extension — candidates and collision research

- **Status:** PROPOSED — decision RESERVED to the user (2026-10-26).
  The user explicitly reserved this choice ("until my choice, do not
  treat the temporary extension as final"). This ADR only records the
  researched options.
- **Current state (temporary, NOT final):** the toolchain and fixtures
  carry the `.k` suffix from M1 as a filename convention. Nothing in
  the compiler enforces an extension. This carry-over is explicitly
  temporary and MUST NOT be read as a language decision.

## Why `.k` and `.kv` are indeed problematic (user intuition, confirmed)

- **`.kv`** — REQUIRED by the Kivy GUI framework: "A Kivy language file
  must have .kv as filename extension" (Kivy 2.3.1 documentation,
  kivy.org/doc/stable/api-kivy.lang.html). Broad Python-GUI mindshare,
  editor tooling registered for it (VS Code extensions). Collision is
  real and loud.
- **`.k`** — source-file extension of the K language under kdb+/q (Kx
  Systems): k and q source files are recognized as such by editor
  tooling (e.g. the vscode-k-pro extension analyzes "q and k source
  files"), and `.k` is additionally used by KiCad legacy metadata and
  various numeric formats. A terse one-letter extension in an
  established database-language ecosystem — collision risk and zero
  discoverability ("grep for .k files" is useless).

## Candidates (2 + 1), with research findings

### 1. `.kavir` (researched; no collision found)
- Search results: completely unclaimed; unambiguous; maximally
  discoverable (the name IS the language); trivially greppable.
- Cost: longer to type; unusual length for a source extension.
- Risk: essentially none today.

### 2. `.kvr` (researched; effectively unclaimed)
- Search results: only an obscure Windows-registry entry ("Binary Data
  by Unknown Developer") on a file-extension aggregator; no software is
  known to claim it. Short and available.
- Cost/risk: less discoverable; a three-letter coinage can be re-claimed
  by some future tool; "KVR" is also a well-known audio-community site
  name (KVR Audio), so web searches for the extension surface noise.

### 3. `.kav` (researched; no hard collision found, soft concerns)
- Search results: no established software association surfaced for
  `.kav` as a file extension.
- Soft concern: "KAV" is a very common abbreviation for Kaspersky
  Anti-Virus; AV products historically used K-branded quarantine/
  database suffixes. No hard collision was found, but the abbreviation
  invites confusion and mis-grepping.

## Coordinator's recommendation (non-binding)

`.kavir` for uniqueness and discoverability, `.kvr` if brevity wins.
The coordinator does NOT recommend `.k`, `.kv`, or `.kav`.

## Consequences

- Until the user decides, the repo keeps the temporary `.k` naming;
  the finalizing change is mechanical (rename fixtures, update usage
  strings) and additive.
- The syntax-finalization ADR (per the ADR ordering rule) is the
  natural place to record the user's extension decision together with
  the final grammar.
- README wording about the extension is PENDING and intentionally
  absent: README.md is frozen by user decision 3 (2026-10-26) until the
  user announces the private/public decision. The lines to add then are
  tracked in the M2 report.

## Evidence references

- Kivy 2.3.1 docs (kivy.org), `.kv` requirement — fetched 2026-10-26.
- kdb+/q ecosystem tooling recognizing `.k` source (vscode-k-pro,
  code.kx.com) — fetched 2026-10-26.
- File-extension aggregators for `.kvr` (one obscure registry entry,
  no software) and no results for `.kav` / `.kavir` — fetched
  2026-10-26.
