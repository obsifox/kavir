# ADR-009: Kavir source file extension — candidates and collision research

- **Status:** ACCEPTED — user decision, 2026-10-26 (second decision
  message): **the extension stays `.k`**. Nothing is renamed. The
  collision research below was presented to the user BEFORE the
  decision; the user chose `.k` with that knowledge (informed
  acceptance — the kdb+/q `.k` collision is acknowledged, not
  overlooked).
- **Current state (FINAL per user decision):** `.k` is the source file
  extension of Kavir programs. The toolchain still does not enforce any
  extension (open-with-any-name keeps working); fixtures and docs use
  `.k`. The earlier "temporary" designation is resolved by this ADR.

## Decision (2026-10-26)

- Chosen: `.k` (status quo; zero rename churn in fixtures, docs, and
  habits).
- Explicitly rejected by the user: renaming to `.kavir`, `.kvr`, or
  `.kav`, and any migration step.
- Recorded consequence: `.k` is shared with kdb+/q's K sources and
  other formats (see research below). Editor tooling for Kavir will
  need its own grammar registration when tooling lands (M6 per
  ADR-005 v3); no action is taken now.

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

- RESOLVED (2026-10-26): the repo keeps `.k`; no rename happens.
- The syntax-finalization ADR (per the ADR ordering rule) will record
  the final grammar; `.k` is already final and independent of that
  ADR.
- README wording about the extension was added with the 2026-10-26
  partial unfreeze of README.md/STATE.md (user decision 3, second
  message): the user ordered README/STATE refreshed for M2 status and
  the temporary-syntax note, private/public lines untouched. The
  private/public question itself remains OPEN and untouched.

## Evidence references

- Kivy 2.3.1 docs (kivy.org), `.kv` requirement — fetched 2026-10-26.
- kdb+/q ecosystem tooling recognizing `.k` source (vscode-k-pro,
  code.kx.com) — fetched 2026-10-26.
- File-extension aggregators for `.kvr` (one obscure registry entry,
  no software) and no results for `.kav` / `.kavir` — fetched
  2026-10-26.
