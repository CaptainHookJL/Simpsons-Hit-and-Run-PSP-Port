#!/bin/bash
set -euo pipefail
export PSPDEV=/workspace/toolchains/pspdev
export PATH="$PSPDEV/bin:$PATH"
if [ ! -x "$PSPDEV/bin/psp-gcc" ]; then
  mkdir -p /workspace/toolchains
  curl -fL --retry 2 https://github.com/pspdev/pspdev/releases/download/v20261001/pspdev-ubuntu-latest-x86_64.tar.gz -o /tmp/pspdev-v20261001.tar.gz
  printf '%s\n' 'a86efe624e770005290919617859e9df24bd8fcd3e364f4587f23ec7364b0acd  /tmp/pspdev-v20261001.tar.gz' | sha256sum -c -
  tar -xzf /tmp/pspdev-v20261001.tar.gz -C /workspace/toolchains
fi
cd /workspace/Simpsons-Hit-and-Run-PSP-Port
psp-gcc --version
make -j2
gcc -std=c99 -Wall -Wextra -Werror tests/driving_test.c src/driving.c src/upstream/vehicle_steering.c -lm -o /tmp/shar-driving-test
/tmp/shar-driving-test
python3 -m unittest discover -s tests -p 'test_*.py'
gcc -std=c99 -Wall -Wextra -Werror tests/mesh_test.c src/mesh.c -lm -o /tmp/shar-mesh-test
/tmp/shar-mesh-test assets/springfield.shm
