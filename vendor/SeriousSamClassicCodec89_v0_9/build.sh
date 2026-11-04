#!/bin/sh
set -eu
CC="${CC:-cc}"
mkdir -p bin
"$CC" -std=c89 -pedantic-errors -Wall -Wextra -Werror -O2 -Iinclude src/ss89.c src/ss89dump.c -o bin/ss89dump
printf '%s\n' 'Built bin/ss89dump'

"$CC" -std=c89 -pedantic-errors -Wall -Wextra -Werror -O2 -Iinclude src/ss89resources.c src/ss89resource_cli.c -o bin/ss89resource
printf "%s\n" "Built bin/ss89resource"
"$CC" -std=c89 -pedantic-errors -Wall -Wextra -Werror -O2 src/ss89gro.c -o bin/ss89gro
printf '%s\n' 'Built bin/ss89gro'
"$CC" -std=c89 -pedantic-errors -Wall -Wextra -Werror -O2 -Iinclude src/ss89skin.c src/ss89pose.c -o bin/ss89pose
printf '%s\n' 'Built bin/ss89pose'

"$CC" -std=c89 -pedantic-errors -Wall -Wextra -Werror -O2 -Iinclude src/ss89.c src/ss89resources.c src/ss89fx.c src/ss89normals.c src/ss89scene.c -o bin/ss89scene
printf "%s\n" "Built bin/ss89scene"

"$CC" -std=c89 -pedantic-errors -Wall -Wextra -Werror -O2 -Iinclude src/ss89fx.c tests/test_fx_api.c -o bin/test_fx_api

"$CC" -std=c89 -pedantic-errors -Wall -Wextra -Werror -O2 -Iinclude src/ss89fxpasses.c src/ss89env.c src/ss89settings.c src/ss89resources.c src/ss89present.c -o bin/ss89present

"${CC:-cc}" -std=c89 -pedantic-errors -Wall -Wextra -Werror -O2 -Iinclude src/ss89fxpasses.c tests/test_fxpasses.c -o bin/test_fxpasses

"${CC:-cc}" -std=c89 -pedantic-errors -Wall -Wextra -Werror -O2 -Iinclude src/ss89.c src/ss89probe.c -o bin/ss89probe

"${CC:-cc}" -std=c89 -pedantic-errors -Wall -Wextra -Werror -O2 -Iinclude src/ss89settings.c src/ss89settings_cli.c -o bin/ss89settings

"${CC:-cc}" -std=c89 -pedantic-errors -Wall -Wextra -Werror -O2 -Iinclude src/ss89env.c tests/test_env_v09.c -o bin/test_env_v09
