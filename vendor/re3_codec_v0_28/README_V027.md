# RE3 Codec v0.27 — Auditoría de bancos de daño y trazas del EXE

**Alcance:** versión diagnóstica, sin sustitución automática de mapas de huesos.

## Nuevos componentes
- `re3c_damage_rig_status()` distingue ausencia, jerarquía heredada compatible y banco sin asociación demostrada.
- `re3emd_damage_contract` valida las keys de daño pero no intenta aplicar un mapa desconocido.
- `test_damage_contract` comprueba que el banco de 15 canales de Grave Digger no se marque como compatible con 17 joints.
- Corrección de una línea rota en `build_mingw.bat`: antes se mezclaban dos invocaciones de compilación en una sola.

## Ejemplos

```bash
make test re3emd_damage_contract
./re3emd_damage_contract EM30.EMD
./re3emd_damage_contract EM34.EMD
```

## Qué NO demuestra
- No se ha encontrado una correspondencia original 15→16/17 validada con Ghidra.
- No hay nueva renderización certificada de los daños de Grave Digger.
- Los remapeos opt-in de v0.26 siguen **EXPERIMENTALES**, sin modificar su semántica.
- Ghidra 12.0.4 funcionó directamente, mientras el puente REA exige una versión 12.1+.

Incluye código ISO C89, sin asignación dinámica de memoria, con el mismo backend de v0.26.
