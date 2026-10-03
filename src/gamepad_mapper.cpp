#include <gamepad_mapper/gamepad_mapper.hpp>

#include <SDL3/SDL.h>
#include <nlohmann/json.hpp>

#include <algorithm>
#include <climits>
#include <cmath>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <set>
#include <stdexcept>
#include <utility>

namespace gamepad_mapper {
namespace {
using nlohmann::json;

void check_joystick(SDL_Joystick* joystick, const Cancelled& cancelled = {}) {
    if (cancelled && cancelled()) {
        throw std::runtime_error("Mapping cancelled.");
    }
    if (!joystick || !SDL_JoystickConnected(joystick)) {
        throw std::runtime_error("Game controller disconnected.");
    }
}

std::vector<float> read_axes(SDL_Joystick* joystick, const Cancelled& cancelled) {
    SDL_PumpEvents();
    check_joystick(joystick, cancelled);
    std::vector<float> axes(SDL_GetNumJoystickAxes(joystick));
    for (std::size_t i = 0; i < axes.size(); ++i) {
        // pygame uses the same divisor, including at the asymmetric endpoints.
        axes[i] = SDL_GetJoystickAxis(joystick, static_cast<int>(i)) / 32768.0f;
    }
    return axes;
}

std::vector<float> settle_axes(SDL_Joystick* joystick, const Cancelled& cancelled) {
    auto axes = read_axes(joystick, cancelled);
    int stable_count = 0;
    const auto deadline = SDL_GetTicks() + 500;
    while (SDL_GetTicks() < deadline) {
        SDL_Delay(20);
        auto next = read_axes(joystick, cancelled);
        float max_delta = 0;
        for (std::size_t i = 0; i < axes.size(); ++i) {
            max_delta = std::max(max_delta, std::abs(next[i] - axes[i]));
        }
        stable_count = max_delta <= 0.01f ? stable_count + 1 : 0;
        axes = std::move(next);
        if (stable_count >= 3) {
            break;
        }
    }
    return axes;
}

void validate(SDL_Joystick* joystick, const Mapping& mapping) {
    check_joystick(joystick);
    for (const auto& axis : mapping.axes) {
        if (axis.index < 0 || axis.index >= SDL_GetNumJoystickAxes(joystick)) {
            throw std::runtime_error("Invalid axis index for \"" + axis.name + "\".");
        }
    }
    for (const auto& button : mapping.buttons) {
        if (button.index < 0 || button.index >= SDL_GetNumJoystickButtons(joystick)) {
            throw std::runtime_error("Invalid button index for \"" + button.name + "\".");
        }
    }
}

json to_json(const Mapping& mapping) {
    json result = {{"axes", json::array()}, {"buttons", json::array()}};
    for (const auto& axis : mapping.axes) {
        result["axes"].push_back({{"name", axis.name}, {"index", axis.index},
                                  {"inverted", axis.inverted}});
    }
    for (const auto& button : mapping.buttons) {
        result["buttons"].push_back({{"name", button.name}, {"index", button.index}});
    }
    return result;
}

Mapping from_json(const json& data) {
    if (!data.at("axes").is_array() || !data.at("buttons").is_array()) {
        throw std::runtime_error("Profile axes and buttons must be arrays.");
    }
    Mapping mapping;
    const auto index = [](const json& entry) {
        const auto& value = entry.at("index");
        if (!value.is_number_integer() || value < 0 || value > INT_MAX) {
            throw std::runtime_error("Profile indices must be nonnegative integers.");
        }
        return value.get<int>();
    };
    for (const auto& axis : data.at("axes")) {
        mapping.axes.push_back({axis.at("name").get<std::string>(), index(axis),
                                axis.at("inverted").get<bool>()});
    }
    for (const auto& button : data.at("buttons")) {
        mapping.buttons.push_back({button.at("name").get<std::string>(), index(button)});
    }
    return mapping;
}

std::filesystem::path environment_path(const char* name) {
    const char* value = std::getenv(name);
    if (!value || !*value) {
        throw std::runtime_error(std::string("Environment variable not set: ") + name);
    }
    return std::filesystem::u8path(value);
}
} // namespace

std::filesystem::path profile_path(const std::string& name) {
    if (name.empty() || name.find_first_of("/\\") != std::string::npos || name == "." || name == "..") {
        throw std::invalid_argument("Profile name must be a nonempty filename, without directories.");
    }
#ifdef _WIN32
    // platformdirs defaults the Windows app author to the app name.
    auto directory = environment_path("LOCALAPPDATA") / "gamepad-mapper";
#elif defined(__APPLE__)
    auto directory = environment_path("HOME") / "Library" / "Application Support";
#else
    const char* xdg = std::getenv("XDG_CONFIG_HOME");
    auto directory = xdg && *xdg ? std::filesystem::u8path(xdg) : environment_path("HOME") / ".config";
#endif
    return directory / "gamepad-mapper" / std::filesystem::u8path(name + ".json");
}

Mapping map(SDL_Joystick* joystick, const std::vector<std::string>& axes_names,
            const std::vector<std::string>& button_names, bool verbose,
            const Cancelled& cancelled) {
    check_joystick(joystick, cancelled);
    if (axes_names.size() > static_cast<std::size_t>(SDL_GetNumJoystickAxes(joystick)) ||
        button_names.size() > static_cast<std::size_t>(SDL_GetNumJoystickButtons(joystick))) {
        throw std::runtime_error("Game controller does not have enough axes or buttons.");
    }

    Mapping mapping;
    std::set<int> used_axes;
    auto baseline = settle_axes(joystick, cancelled);
    for (const auto& name : axes_names) {
        std::cerr << "\nMove the control you want to be \"" << name
                  << "\" fully FORWARD / RIGHT (max positive) and hold...\n";
        while (true) {
            const auto axes = read_axes(joystick, cancelled);
            int index = -1;
            float delta = 0;
            for (std::size_t i = 0; i < axes.size(); ++i) {
                const auto movement = axes[i] - baseline[i];
                if (!used_axes.count(static_cast<int>(i)) && std::abs(movement) > std::abs(delta)) {
                    index = static_cast<int>(i);
                    delta = movement;
                }
            }
            if (std::abs(delta) >= 0.6f) {
                mapping.axes.push_back({name, index, delta < 0});
                used_axes.insert(index);
                std::cerr << "  Detected axis " << index << (delta < 0 ? " inverted\n" : " normal\n");
                break;
            }
            SDL_Delay(10);
        }
        baseline = settle_axes(joystick, cancelled);
    }

    std::set<int> used_buttons;
    const auto id = SDL_GetJoystickID(joystick);
    for (const auto& name : button_names) {
        std::cerr << "\nPress the button you'd like to map for \"" << name << "\"...\n";
        int index = -1;
        while (index < 0) {
            SDL_PumpEvents();
            check_joystick(joystick, cancelled);
            SDL_Event event;
            while (SDL_PeepEvents(&event, 1, SDL_GETEVENT,
                                  SDL_EVENT_JOYSTICK_BUTTON_DOWN, SDL_EVENT_JOYSTICK_BUTTON_DOWN) > 0) {
                if (event.jbutton.which == id && !used_buttons.count(event.jbutton.button)) {
                    index = event.jbutton.button;
                    break;
                }
            }
            if (index < 0) {
                SDL_Delay(10);
            }
        }
        used_buttons.insert(index);
        mapping.buttons.push_back({name, index});
        std::cerr << "  Detected button " << index << '\n';
    }
    std::cerr << "\nMapping completed!\n\n";
    if (verbose) {
        std::cout << to_json(mapping).dump(2) << '\n';
    }
    return mapping;
}

Mapping load_or_map(SDL_Joystick* joystick, const std::vector<std::string>& axes_names,
                    const std::vector<std::string>& button_names, bool force,
                    const std::filesystem::path& file, bool verbose, const Cancelled& cancelled) {
    check_joystick(joystick, cancelled);
    std::cerr << "Gamepad mapping profile path: " << file << '\n';
    if (!force && std::filesystem::exists(file)) {
        try {
            std::cerr << "Loading gamepad mapping profile from " << file << '\n';
            std::ifstream input(file);
            const auto mapping = from_json(json::parse(input));
            validate(joystick, mapping);
            return mapping;
        } catch (const std::exception& error) {
            std::cerr << "Failed to read mapping: " << error.what() << ". Re-mapping...\n";
        }
    }
    const auto mapping = map(joystick, axes_names, button_names, verbose, cancelled);
    std::cerr << "Saving gamepad mapping profile to " << file << '\n';
    if (file.has_parent_path()) {
        std::filesystem::create_directories(file.parent_path());
    }
    std::ofstream output;
    output.exceptions(std::ios::failbit | std::ios::badbit);
    output.open(file);
    output << to_json(mapping).dump(2) << '\n';
    output.close();
    return mapping;
}

State read_gamepad(SDL_Joystick* joystick, const Mapping& mapping) {
    SDL_PumpEvents();
    validate(joystick, mapping);
    State state;
    for (const auto& axis : mapping.axes) {
        auto value = SDL_GetJoystickAxis(joystick, axis.index) / 32768.0f;
        state.axes[axis.name] = axis.inverted ? -value : value;
    }
    for (const auto& button : mapping.buttons) {
        state.buttons[button.name] = SDL_GetJoystickButton(joystick, button.index);
    }
    return state;
}

} // namespace gamepad_mapper
