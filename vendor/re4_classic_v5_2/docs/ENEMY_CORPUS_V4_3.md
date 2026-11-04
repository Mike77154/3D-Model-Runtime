# RE4 PC 2007 — universal enemy coverage audit (v4.3)

## What was actually done

- `examples/re4_enemy_universal_audit.py` scans every family with an `emXX`/`plXX` path in the supplied xfile.dat and/or a corresponding package in em.dat. No game assets are packaged with this code.
- `re4_motion_catalog` is executed on **every archive with FCV**, checking structural validity and counts for rotation, root, IK, other channels.
- Every FCV gets a SHA-256 digest; duplicate file content is linked across archives without assuming any particular mesh or skeleton was intended.
- `examples/re4_enemy_fk_smoke.py` selects up to four non-empty real clips per PMD family and evaluates FK using the C89 `re4_character_probe`. A pass only demonstrates sampled rotations.
- `examples/re4_enemy_alternate_pmd_probe.py` investigates PMDs whose selected base has no matching rotations; a valid bone match on a HEAD does **not** certify a complete body or skinning.
- `re4_bestiaire_validate_set()` now rejects missing/wrong **named-set** candidates for U-3, Saddler spider, two Mendez phases, Novistador variants, base Regenerador, base Iron Maiden, armor A/B and Gigante A/B. It does not certify original state machine / game scene assembly.

## Results on supplied em.dat + xfile.dat

- 107 resource families, 66 families with PMD, 100 families with FCV.
- 11,098 FCV, 11,082 structurally valid and 16 empty, zero parser failures; 8,683 SEQ entries.
- 40 families with FCV and no same-name PMD; 5,644 byte-equal clip matches with PMD-bearing families. Those matches **must not** be treated as verified model assignments.
- First-FCV FK samples: 56/66 model families with linked rotations, four with unmatched channels, four lacking same-family animation package, two with no nonempty FCV.
- Alternates found for em1e (head-only pieces), em31 (em3101), em3c (em3c00a); pl02 still unmatched against its same-family PMDs.
- Head Plaga A/B/C models and trigger/morph model state are unresolved; do not silently use em2500 (locomotor Plaga) as substitute.

## Explicit limits

No claim of all enemies working visually: IK controllers, morphs, animated destruction, particles, wing/state selections, creature weapons, facial rigs, provenance of actual in-scene models, full-frame mesh/UV/weights validation, PS2/GC platform parity, and Win32 runtime integration remain unresolved. A valid FCV or PMD header is NOT proof of a correct character.

## How to repeat

```
make -j4 all && make check
python3 examples/re4_enemy_universal_audit.py --xfile /path/xfile.dat --em /path/em.dat --out reports
python3 examples/re4_enemy_fk_smoke.py --xfile /path/xfile.dat --em /path/em.dat --out reports
python3 examples/re4_enemy_alternate_pmd_probe.py --xfile /path/xfile.dat --em /path/em.dat --out reports
```

References: JADERLINK Classic RE4 2007 PMD directory mapping and enemy descriptions: https://jaderlink.blogspot.com/2025/10/re4-2007-modding-tutorialsguide-index.html and https://jaderlink.blogspot.com/2025/01/re4-descricao-dos-arquivos.html. These describe naming and *candidate* model parts; all code in this ZIP is independently written. Image and model rights remain with their owners.

## Named-set part linkage tests

After changing the Saddler-spider candidate master from leg-only em3100 to head em3101, the C89 part-link smoke test found direct links for the two additional components. Armadura A (em3c00 + em3c01) still has no direct bone link and needs special attachment logic. The tests inspect bind retarget candidates, NOT definitive animated geometry or game-canonical assemblies.
