# Dart's web era and the Dart 2 reset (2011–2018)

Period and context:
Launched 2011 by Google as a structured web language with a planned Chrome VM
integration; the 2015 official post pivoted away from that plan; Dart 2 (2018)
rebooted the language around client-side/Flutter development.

Problem it was meant to solve (C-ids):
The official 2015 announcement states the team decided not to integrate the
Dart VM into Chrome and to focus on compiling to JavaScript (C-718). Dart 2
then required migration of all 1.x code (C-719), a breaking reset.

What worked in practice (C-ids):
A pivot to a new killer use case: Flutter evaluated multiple languages and chose
Dart for its JIT/hot-reload + AOT combination (C-720); Google Ads (~1M lines of
Dart) publicly backed the pivot (C-718); community discussion at Dart 2 stable
shows the new adoption center was Flutter (C-719).

Costs and new problems (C-ids):
Five years of web-era momentum discarded; a breaking change with migration
burden (C-719); the HN discussion reads as "Dart is only interesting because of
Flutter" (C-719).

Valid under which conditions:
A pivot works if the sponsor is large and patient (Google), an anchor product
absorbs the migration cost, and the language is vertically integrated with the
product's runtime (C-720).

Alternatives that reach the same goal:
Interoperate with the incumbent platform instead of replacing it (the
TypeScript strategy, C-302/C-319); anchor on a different use case earlier.

Causes of limited adoption (if any), split into technical / ecosystem /
governance / timing:
technical: sound language, JIT+AOT capable (not the cause); ecosystem: required
cross-browser cooperation that never came; governance: single-sponsor
decision-making; timing: JS tooling improved fast in 2013-2015.

Counter-evidence and disagreements:
The 2015 post itself frames the pivot as listening to users, not as failure
(S-723); HN voices at the time treated Dart 2 as a fresh start (S-731). The
claim that the Chrome-VM plan caused the stall is interpretation, not
established fact (C-721, unverified).

Confidence: facts high (C-718, C-719, C-720); causal reading medium (C-721).

Open questions:
Would Dart-on-the-web have succeeded with a different embed strategy? Unknown;
no counterfactual evidence exists.

Implications for our design (as hypotheses only):
H (C-731): do not gate a new language on other vendors' platforms accepting a
new VM; pick the anchor use case early and align runtime/tooling to it; if a
breaking reset ever becomes necessary, ship the migration tooling in the same
announcement (cf. C-714 editions tooling).
