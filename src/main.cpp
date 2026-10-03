#include <gamepad_mapper/gamepad_mapper.hpp>

#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>

#include <csignal>
#include <iomanip>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>

namespace {
volatile std::sig_atomic_t stopped = 0;
void stop(int) { stopped = 1; }

struct SDLSession {
    SDLSession() {
        SDL_SetHint(SDL_HINT_JOYSTICK_ALLOW_BACKGROUND_EVENTS, "1");
        SDL_SetHint(SDL_HINT_NO_SIGNAL_HANDLERS, "1");
        if (!SDL_Init(SDL_INIT_JOYSTICK)) {
            throw std::runtime_error(SDL_GetError());
        }
    }
    ~SDLSession() { SDL_Quit(); }
};
} // namespace

int main(int argc, char** argv) {
    try {
        std::string name = "default";
        bool force = false;
        for (int i = 1; i < argc; ++i) {
            const std::string arg = argv[i];
            if (arg == "--help" || arg == "-h") {
                std::cout << "Usage: gamepad-mapper [--name PROFILE] [--map]\n"
                             "Map Roll, Pitch, Throttle, Yaw and arm on the first joystick.\n"
                             "  --name PROFILE  Profile name (default: default)\n"
                             "  --map           Ignore the saved profile and map again\n";
                return 0;
            }
            if (arg == "--map") {
                force = true;
            } else if (arg == "--name" && i + 1 < argc) {
                name = argv[++i];
            } else if (arg.rfind("--name=", 0) == 0) {
                name = arg.substr(7);
            } else {
                throw std::invalid_argument("Unknown or incomplete argument: " + arg);
            }
        }

        const auto file = gamepad_mapper::profile_path(name);
        SDLSession session;
        std::signal(SIGINT, stop);
        std::signal(SIGTERM, stop);
        int count = 0;
        auto* ids = SDL_GetJoysticks(&count);
        const auto id = ids && count > 0 ? ids[0] : 0;
        SDL_free(ids);
        if (!id) {
            throw std::runtime_error("No game controller found.");
        }
        std::unique_ptr<SDL_Joystick, decltype(&SDL_CloseJoystick)> joystick(
            SDL_OpenJoystick(id), SDL_CloseJoystick);
        if (!joystick) {
            throw std::runtime_error(SDL_GetError());
        }
        const auto mapping = gamepad_mapper::load_or_map(
            joystick.get(), {"Roll", "Pitch", "Throttle", "Yaw"}, {"arm"},
            force, file, false, [] { return stopped != 0; });

        std::cout << "Press Ctrl+C to stop the test.\n" << std::fixed << std::setprecision(2);
        while (!stopped) {
            const auto state = gamepad_mapper::read_gamepad(joystick.get(), mapping);
            for (const auto& axis : mapping.axes) {
                std::cout << axis.name << ": " << state.axes.at(axis.name) << ' ';
            }
            std::cout << " | ";
            for (const auto& button : mapping.buttons) {
                std::cout << button.name << ": " << state.buttons.at(button.name) << ' ';
            }
            std::cout << std::endl;
            SDL_Event event;
            while (SDL_PollEvent(&event)) {
                if (event.type == SDL_EVENT_QUIT) stopped = 1;
            }
            SDL_Delay(100);
        }
        std::cout << "\nTest stopped by user.\n";
        return 0;
    } catch (const std::exception& error) {
        if (stopped) {
            std::cout << "\nTest stopped by user.\n";
            return 0;
        }
        std::cerr << error.what() << '\n';
        return 1;
    }
}
