# Gamepad Mapping

Map joystick axes and buttons to named channels, remember the mapping in a JSON
profile, and read their current values. C++ and Python implementations are included.

## C++ library and client

Requires CMake 3.24+, a C++17 compiler, and internet access on the first configure.
CMake FetchContent downloads pinned [SDL3](https://github.com/libsdl-org/SDL/releases/tag/release-3.4.18)
and [nlohmann/json](https://github.com/nlohmann/json/releases/tag/v3.12.0) releases.

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel
./build/gamepad-mapper --name example
```

The client opens the first joystick and maps `Roll`, `Pitch`, `Throttle`, `Yaw`,
and `arm`. Start with the controls at rest, then follow the prompts, moving each
requested axis fully forward/right and holding it until the next prompt. Axis
direction is detected automatically, and each physical axis/button is used once.
Values print every 100 ms; Ctrl+C stops either mapping or live readings. Use
`--map` to replace a saved profile, or `--help` for usage. With a multi-configuration
generator such as Visual Studio, build with `--config Release` and run the client
from `build/Release/`.

The library exports `map`, `load_or_map`, `read_gamepad`, and `profile_path` in
`gamepad_mapper`. To embed it in another CMake project:

```cmake
add_subdirectory(path/to/gamepad-mapper)
target_link_libraries(your_client PRIVATE gamepad_mapper::gamepad_mapper)
```

Initialize SDL with `SDL_INIT_JOYSTICK` and open an `SDL_Joystick*` before calling
the library. Call it on the main thread; the caller owns and closes the joystick
and SDL. For a terminal client, enable `SDL_HINT_JOYSTICK_ALLOW_BACKGROUND_EVENTS`.
See [src/main.cpp](src/main.cpp) for a complete client.

```cpp
#include <gamepad_mapper/gamepad_mapper.hpp>

// joystick is an open SDL_Joystick*.
auto mapping = gamepad_mapper::load_or_map(
    joystick, {"Roll", "Pitch", "Throttle", "Yaw"}, {"arm"},
    false, gamepad_mapper::profile_path("example"));
auto [axes, buttons] = gamepad_mapper::read_gamepad(joystick, mapping);
float roll = axes.at("Roll");
bool armed = buttons.at("arm");
```

Axes use the same signed normalization as pygame (`raw / 32768.0`), with the
stored inversion applied; buttons are booleans. Mapping waits for stable axis
readings between prompts. Errors throw exceptions. `map` and `load_or_map` accept
an optional final cancellation callback; returning `true` aborts without saving.
`load_or_map` accepts a custom profile file path and remaps missing, unreadable,
or invalid profiles. An existing valid profile is loaded as stored, regardless
of the supplied channel names, matching Python behavior.

Both implementations use the same JSON schema and default profile paths:

- macOS: `~/Library/Application Support/gamepad-mapper/<name>.json`
- Linux: `${XDG_CONFIG_HOME:-~/.config}/gamepad-mapper/<name>.json`
- Windows: `%LOCALAPPDATA%/gamepad-mapper/gamepad-mapper/<name>.json`

Profiles store raw joystick indices, so remap if your controller or SDL driver
reports a different axis/button order.

The client is built by default for standalone builds. Set
`GAMEPAD_MAPPER_BUILD_CLIENT=OFF` for just the library. Optional integration tests
use SDL virtual joysticks and temporary profiles; no controller is needed:

```sh
cmake -S . -B build -DGAMEPAD_MAPPER_BUILD_TESTS=ON
cmake --build build --parallel
ctest --test-dir build --output-on-failure
```

## Python

```python
import pygame
from gamepad_mapper import load_or_map, read_gamepad

pygame.init()
pygame.joystick.init()
assert pygame.joystick.get_count() > 0, "No game controller found."

joystick = pygame.joystick.Joystick(0)
joystick.init()
mapping = load_or_map(joystick, ["Roll", "Pitch", "Throttle", "Yaw"], ["arm"], force=True, name="example")
axes, buttons = read_gamepad(joystick, mapping)
print(axes)
print(buttons)
```

Output
```
{'Roll': 0.796051025390625, 'Pitch': 0.62353515625, 'Throttle': -0.17645263671875, 'Yaw': -0.741180419921875}
{'arm': 1}
```
