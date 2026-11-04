@echo off
if not exist build mkdir build
gcc -std=c89 -pedantic-errors -Wall -Wextra -Werror -O2 -Iinclude -c src\re1codec.c -o build\re1codec.o || exit /b 1
gcc -std=c89 -pedantic-errors -Wall -Wextra -Werror -O2 -Iinclude tools\re1codec_inspect.c build\re1codec.o -o build\re1codec_inspect.exe || exit /b 1
gcc -std=c89 -pedantic-errors -Wall -Wextra -Werror -O2 -Iinclude tools\re1codec_mv_audit.c build\re1codec.o -o build\re1codec_mv_audit.exe || exit /b 1
gcc -std=c89 -pedantic-errors -Wall -Wextra -Werror -O2 -Iinclude tests\test_em1111.c build\re1codec.o -o build\test_em1111.exe || exit /b 1
gcc -std=c89 -pedantic-errors -Wall -Wextra -Werror -O2 -Iinclude tests\test_ft4_packet.c build\re1codec.o -o build\test_ft4_packet.exe || exit /b 1
echo RE1codec build OK
