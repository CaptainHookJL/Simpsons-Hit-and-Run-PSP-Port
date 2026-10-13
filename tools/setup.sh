#!/bin/bash
set -euo pipefail
export PSPDEV=/workspace/toolchains/pspdev
export PATH="$PSPDEV/bin:$PATH"
PYTOOLS=python3
if ! python3 -c 'import PIL; assert PIL.__version__ == "12.3.0"' >/dev/null 2>&1; then
  python3 -m venv /workspace/toolchains/shar-python
  /workspace/toolchains/shar-python/bin/pip install --no-cache-dir -r /workspace/Simpsons-Hit-and-Run-PSP-Port/requirements-tools.txt
  PYTOOLS=/workspace/toolchains/shar-python/bin/python
fi
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
"$PYTOOLS" -m unittest discover -s tests -p 'test_*.py'
gcc -std=c99 -Wall -Wextra -Werror tests/mesh_test.c src/mesh.c -lm -o /tmp/shar-mesh-test
/tmp/shar-mesh-test assets/springfield.shm
gcc -std=c99 -Wall -Wextra -Werror tests/collision_test.c src/collision.c src/mesh.c src/driving.c src/upstream/vehicle_steering.c -lm -o /tmp/shar-collision-test
/tmp/shar-collision-test assets/springfield.shm
gcc -std=c99 -Wall -Wextra -Werror tests/texture_test.c src/texture.c src/mesh.c -lm -o /tmp/shar-texture-test
/tmp/shar-texture-test assets/springfield.shm assets/springfield.stx
gcc -std=c99 -Wall -Wextra -Werror tests/clip_test.c src/clip.c src/texture.c src/mesh.c -lm -o /tmp/shar-clip-test
/tmp/shar-clip-test assets/springfield.shm assets/springfield.stx
gcc -std=c99 -Wall -Wextra -Werror tests/vehicle_test.c src/clip.c src/texture.c src/mesh.c -lm -o /tmp/shar-vehicle-test
/tmp/shar-vehicle-test assets/compact.shm assets/compact.stx
gcc -std=c99 -Wall -Wextra -Werror tests/wheels_test.c src/wheels.c src/clip.c src/texture.c src/mesh.c src/collision.c src/driving.c src/upstream/vehicle_steering.c -lm -o /tmp/shar-wheels-test
/tmp/shar-wheels-test assets/compact.shm assets/compact.stx assets/compact.swh
