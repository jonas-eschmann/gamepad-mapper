#pragma once

#include <SDL3/SDL_joystick.h>

#include <filesystem>
#include <functional>
#include <map>
#include <string>
#include <vector>

namespace gamepad_mapper {

struct Axis {
    std::string name;
    int index = 0;
    bool inverted = false;
};

struct Button {
    std::string name;
    int index = 0;
};

struct Mapping {
    std::vector<Axis> axes;
    std::vector<Button> buttons;
};

struct State {
    std::map<std::string, float> axes;
    std::map<std::string, bool> buttons;
};

using Cancelled = std::function<bool()>;

// Same default profile location and JSON format as the Python package.
std::filesystem::path profile_path(const std::string& name = "default");

// Call on the main thread with an open joystick and SDL_INIT_JOYSTICK initialized.
// The caller owns SDL and the joystick. Errors/cancellation throw std::exception.
Mapping map(SDL_Joystick* joystick, const std::vector<std::string>& axes_names,
            const std::vector<std::string>& button_names, bool verbose = false,
            const Cancelled& cancelled = {});

Mapping load_or_map(SDL_Joystick* joystick, const std::vector<std::string>& axes_names,
                    const std::vector<std::string>& button_names, bool force = false,
                    const std::filesystem::path& file = profile_path(), bool verbose = false,
                    const Cancelled& cancelled = {});

State read_gamepad(SDL_Joystick* joystick, const Mapping& mapping);

} // namespace gamepad_mapper
