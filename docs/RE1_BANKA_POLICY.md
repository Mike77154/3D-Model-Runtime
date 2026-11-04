# RE1 Bank A runtime policy

`RE1codec v0.3-mvref` preserves both native animation banks. The runtime does
not reinterpret or discard Bank A bytes, but it deliberately does **not**
automatically bind Bank A PartList nodes to visible TMD objects.

Reason: across the validated enemy corpus Bank A is frequently a common
15-part hierarchy even when Bank B/TMD describe a non-humanoid creature. An
ordinal `BankA part N -> TMD object N` mapping visibly deforms dogs, snakes,
spiders and other enemies.

Runtime rules:

- Bank B is the only automatic rigid hierarchy for RE1 model pieces.
- Bank B clips are enumerated first and are playable.
- Bank A clips remain enumerable/inspectable and carry
  `MODELRT_ANIM_UNBOUND`.
- Selecting a Bank A clip leaves the visible model in the Bank B bind pose; it
  does not emit/apply Bank A hierarchy transforms.
- If Bank B is absent, the runtime does not synthesize a hierarchy from Bank A.
- A future explicit mapper may bind Bank A, but that must be opt-in and
  data-driven, never a humanoid fallback.

Regression: `tests/re1codec_fixture.emd` deliberately stores a +100 centi Bank
A pivot and a zero Bank B pivot. `tests/re1codec_regression.c` verifies that
selecting the Bank A clip does not move the rendered triangle.
