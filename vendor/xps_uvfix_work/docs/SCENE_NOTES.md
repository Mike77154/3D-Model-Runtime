# Scene notes

## Alcance práctico

La librería trata `.scene` como un formato binario secuencial sin heap:

1. lee el header
2. emite callbacks por item y por hueso de pose
3. emite accesorios
4. emite bloques globales (cámara, luces, post, fondo, sky, ventana)

## Strings

Las strings de `.scene` usan longitud de **1 byte**.

Además, algunas herramientas toleran un byte extra `0x01` justo después de la longitud. Esta librería reproduce esa tolerancia.

## Versiones

La lectura usa compuertas por versión para:

- escala por item desde `1.8`
- glow desde `1.11`
- intensidad de luz desde `1.2`
- postprocesado desde `1.9`
- sky dome desde `1.6`
- ventana desde `1.7`
- campos extra de post/fondo en versiones `> 1.21`
- byte extra entre luces desde `1.30`

## Cosas que no se interpretan con semántica fuerte

- el `float` que aparece antes del bloque de luces se salta
- el byte y `u32` desconocidos del bloque de post se saltan

La librería los omite a propósito para no inventar semántica donde la referencia comunitaria también trata esos campos como opacos.
