#include <gamepad_mapper/gamepad_mapper.hpp>

#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>

#include <fstream>
#include <iostream>
#include <stdexcept>

namespace gm = gamepad_mapper;

namespace {
void require(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}

template<class Function>
void require_error(Function function, const char* message) {
    try {
        function();
    } catch (const std::exception&) {
        return;
    }
    throw std::runtime_error(message);
}

struct Script {
    SDL_Joystick* joystick = nullptr;
    Uint64 start = 0;
    bool running = false;
    bool foreign_sent = false;
};

void SDLCALL update(void* userdata) {
    auto& script = *static_cast<Script*>(userdata);
    if (!script.running) return;
    const auto elapsed = SDL_GetTicks() - script.start;
    SDL_SetJoystickVirtualAxis(script.joystick, 0,
                              elapsed < 180 ? 0 : elapsed < 350 ? -32768 : 32767);
    SDL_SetJoystickVirtualAxis(script.joystick, 1, elapsed < 500 ? 0 : 32767);
    // A different controller's press must not claim button 0.
    if (elapsed >= 650 && !script.foreign_sent) {
        SDL_Event event{};
        event.type = SDL_EVENT_JOYSTICK_BUTTON_DOWN;
        event.jbutton.which = SDL_GetJoystickID(script.joystick) + 100000;
        event.jbutton.button = 0;
        event.jbutton.down = true;
        SDL_PushEvent(&event);
        script.foreign_sent = true;
    }
    // Press button 1 twice; the second mapping must wait for unused button 0.
    SDL_SetJoystickVirtualButton(script.joystick, 1,
                                 (elapsed >= 700 && elapsed < 800) || elapsed >= 900);
    SDL_SetJoystickVirtualButton(script.joystick, 0, elapsed >= 1000);
}
} // namespace

int main(int, char**) {
    std::filesystem::path directory;
    SDL_Joystick* joystick = nullptr;
    SDL_JoystickID id = 0;
    Script script;
    int result = 0;
    try {
        SDL_SetHint(SDL_HINT_JOYSTICK_ALLOW_BACKGROUND_EVENTS, "1");
        require(SDL_Init(SDL_INIT_JOYSTICK), SDL_GetError());
        SDL_VirtualJoystickDesc description{};
        SDL_INIT_INTERFACE(&description);
        description.naxes = 4;
        description.nbuttons = 2;
        description.name = "gamepad-mapper test";
        description.userdata = &script;
        description.Update = update;
        id = SDL_AttachVirtualJoystick(&description);
        require(id != 0, SDL_GetError());
        joystick = SDL_OpenJoystick(id);
        require(joystick != nullptr, SDL_GetError());
        script.joystick = joystick;

        directory = std::filesystem::temp_directory_path() /
                    ("gamepad-mapper-test-" + std::to_string(SDL_GetPerformanceCounter()));
        require(std::filesystem::create_directory(directory), "Cannot create temporary directory");
        const auto file = directory / "nested" / "profile.json";
        script.start = SDL_GetTicks();
        script.running = true;
        const auto mapping = gm::load_or_map(joystick, {"Roll", "Pitch"}, {"arm", "mode"},
            false, file, false, [&] { return SDL_GetTicks() - script.start > 3000; });
        script.running = false;
        require(mapping.axes.size() == 2 && mapping.buttons.size() == 2, "Missing controls");
        require(mapping.axes[0].index == 0 && mapping.axes[0].inverted, "Inversion not detected");
        require(mapping.axes[1].index == 1 && !mapping.axes[1].inverted, "Axis reused or inverted");
        require(mapping.buttons[0].index == 1 && mapping.buttons[1].index == 0,
                "Foreign controller or repeated button was mapped");
        require(std::filesystem::exists(file), "Profile was not saved");

        // Loading must return the saved profile without asking for new input.
        const auto loaded = gm::load_or_map(joystick, {}, {}, false, file);
        require(loaded.axes.at(0).name == "Roll" && loaded.axes.at(0).inverted &&
                loaded.buttons.at(0).index == 1, "Profile did not round-trip");

        // A fixture in the Python package's exact schema, with reordered indices.
        std::ofstream(file) << R"({"axes":[
            {"name":"Roll","index":2,"inverted":false},
            {"name":"Pitch","index":0,"inverted":true},
            {"name":"Throttle","index":3,"inverted":false},
            {"name":"Yaw","index":1,"inverted":false}],
            "buttons":[{"name":"arm","index":1}]})";
        const auto python_mapping = gm::load_or_map(joystick, {}, {}, false, file);
        require(SDL_SetJoystickVirtualAxis(joystick, 0, -32768), SDL_GetError());
        require(SDL_SetJoystickVirtualAxis(joystick, 1, 32767), SDL_GetError());
        require(SDL_SetJoystickVirtualAxis(joystick, 2, 16384), SDL_GetError());
        require(SDL_SetJoystickVirtualAxis(joystick, 3, -8192), SDL_GetError());
        require(SDL_SetJoystickVirtualButton(joystick, 1, true), SDL_GetError());
        const auto state = gm::read_gamepad(joystick, python_mapping);
        require(state.axes.at("Roll") == 0.5f && state.axes.at("Pitch") == 1.0f &&
                state.axes.at("Throttle") == -0.25f && state.axes.at("Yaw") == 32767 / 32768.0f,
                "Axis scaling, inversion or name lookup differs from Python");
        require(state.buttons.at("arm"), "Pressed button not read");
        SDL_SetJoystickVirtualButton(joystick, 1, false);
        require(!gm::read_gamepad(joystick, python_mapping).buttons.at("arm"), "Release not read");

        const auto forced = gm::load_or_map(joystick, {}, {}, true, file);
        require(forced.axes.empty() && forced.buttons.empty(), "Force did not replace profile");
        for (const auto* bad : {"{broken", R"({"axes":{},"buttons":[]})",
             R"({"axes":[{"name":"bad","index":4,"inverted":false}],"buttons":[]})",
             R"({"axes":[],"buttons":[{"name":"bad","index":-1}]})",
             R"({"axes":[],"buttons":[{"name":"bad","index":0.5}]})"}) {
            std::ofstream(file) << bad;
            const auto remapped = gm::load_or_map(joystick, {}, {}, false, file);
            require(remapped.axes.empty() && remapped.buttons.empty(), "Invalid profile not remapped");
        }

        require_error([&] { gm::map(joystick, {"a", "b", "c", "d", "e"}, {}); },
                      "Impossible mapping did not fail");
        const auto cancelled_file = directory / "cancelled.json";
        const auto start = SDL_GetTicks();
        require_error([&] {
            gm::load_or_map(joystick, {"Roll"}, {}, false, cancelled_file, false,
                           [&] { return SDL_GetTicks() - start > 150; });
        }, "Cancellation did not stop mapping");
        require(!std::filesystem::exists(cancelled_file), "Cancellation saved a partial profile");
        require_error([&] { gm::read_gamepad(joystick, {{{"bad", -1, false}}, {}}); },
                      "Invalid axis index accepted");
        require_error([&] { gm::profile_path("../escape"); }, "Invalid profile name accepted");
        require(SDL_DetachVirtualJoystick(id), SDL_GetError());
        id = 0;
        require_error([&] { gm::read_gamepad(joystick, mapping); }, "Disconnect not detected");
        std::cout << "Virtual joystick and profile tests passed.\n";
    } catch (const std::exception& error) {
        std::cerr << "Test failed: " << error.what() << '\n';
        result = 1;
    }
    if (joystick) SDL_CloseJoystick(joystick);
    if (id) SDL_DetachVirtualJoystick(id);
    SDL_Quit();
    if (!directory.empty()) std::filesystem::remove_all(directory);
    return result;
}
