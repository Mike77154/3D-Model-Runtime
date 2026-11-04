# RE1codec v0.2 changes

- Accept full 20-byte EMD directories with A+B animation banks.
- Accept 20-byte B-only EMD directories whose first two offsets are zero.
- Accept compact 16-byte B-only EMD directory observed in `em1032.emd`.
- Missing animation banks are represented as absent zero-size sections instead of a format error.
- New bank APIs: `re1c_bank_available()` and `re1c_get_preferred_bank()`.
- `re1c_stats` now separates TMD mesh-object count from A/B EMR animation-part counts.
- New `RE1C_ERR_NOT_PRESENT` result for optional banks/TIM.
- Add `.EMW` weapon-container support (`re1c_open_emw`).
- Add standalone `.TMD` support (`re1c_open_tmd`).
- Add format auto-detection (`re1c_open_model`).
- Embedded TIM is optional for EMW/TMD; callers can attach an external texture provider.
- Inspector auto-detects EMD/EMW/TMD and reports absent banks/textures.
- Added corpus audit script and format-variant documentation.
- Supplied corpus regression: 67/67 EMD + 32/32 EMW + 7/7 TMD = 106/106 validated.
- EM1111 Golden Master unchanged: 283734 canonical bytes, FNV-1a32 C2104AE0.
