#!/usr/bin/env python3
import argparse
import json
import sys
from pathlib import Path
from typing import Dict, List
import pygame
from platformdirs import user_config_dir
import asyncio

CFG_DIR = Path(user_config_dir("gamepad-mapper"))
CFG_DIR.mkdir(parents=True, exist_ok=True)

def wait_for_axis_movement(joystick: pygame.joystick.Joystick,
                           baseline: List[float],
                           prompt: str,
                           thresh: float = 0.6,
                           used_axes: set[int] | None = None) -> Dict:
    if used_axes is None:
        used_axes = set()
    print(prompt, file=sys.stderr)
    sys.stderr.flush()
    axis_count = joystick.get_numaxes()
    while True:
        pygame.event.pump()
        # Ignore axes we've already mapped
        diffs = [
            0.0 if i in used_axes else joystick.get_axis(i) - baseline[i]
            for i in range(axis_count)
        ]
        idx, delta = max(enumerate(diffs), key=lambda x: abs(x[1]))
        if abs(delta) >= thresh:
            inverted = (joystick.get_axis(idx) < baseline[idx])
            print(f"  Detected axis {idx} {'inverted' if inverted else 'normal'}", file=sys.stderr)
            return {"index": idx, "inverted": inverted}


def wait_for_button_press(joystick: pygame.joystick.Joystick, already_taken: set, prompt: str) -> int:
    """Block until the user presses a button not in *already_taken*."""
    print(prompt, file=sys.stderr)
    sys.stderr.flush()
    while True:
        for event in pygame.event.get():
            if event.type == pygame.JOYBUTTONDOWN:
                btn = event.button
                if btn not in already_taken:
                    print(f"  Detected button {btn}", file=sys.stderr)
                    return btn


def map(joystick: pygame.joystick.Joystick, axes_names, button_names, verbose=False) -> Dict:
    axis_mapping: List[Dict] = []
    used_axes: set[int] = set()
    baseline = [joystick.get_axis(i) for i in range(joystick.get_numaxes())]

    for name in axes_names:
        cfg = wait_for_axis_movement(joystick, baseline, f"\nMove the control you want to be \"{name}\" fully " "FORWARD / RIGHT (max positive) and hold…", used_axes=used_axes)
        cfg["name"] = name
        axis_mapping.append(cfg)
        used_axes.add(cfg["index"])
        baseline = [joystick.get_axis(i) for i in range(joystick.get_numaxes())]


    taken_buttons = set()

    buttons = []
    for name in button_names:
        index = wait_for_button_press(joystick, taken_buttons, f"\nPress the first button you’d like to map for \"{name}\"…")
        buttons.append({
            "name": name,
            "index": index,
        })
        taken_buttons.add(index)

    mapping = {
        "axes": axis_mapping,
        "buttons": buttons
    }
    print("\nMapping completed!\n", file=sys.stderr)
    print(json.dumps(mapping, indent=2)) if verbose else None
    return mapping




def load_or_map(joystick, axes_names, button_names, force=False, name="default", verbose=False) -> Dict:
    mapping_file = CFG_DIR / f"{name}.json"
    if not force and mapping_file.exists():
        try:
            with mapping_file.open() as fp:
                mapping = json.load(fp)
            print(f"Loaded mapping from {mapping_file}")
            return mapping
        except Exception as exc:
            print(f"Failed to read mapping: {exc!s}. Re-mapping…")

    mapping = map(joystick, axes_names, button_names, verbose=verbose)
    with mapping_file.open("w") as fp:
        json.dump(mapping, fp, indent=2)
    print(f"Mapping saved to {mapping_file}")
    return mapping

def read_gamepad(joystick, mapping):
    pygame.event.pump()

    channels = {}
    for cfg in mapping["axes"]:
        name, idx, inv = cfg["name"], cfg["index"], cfg["inverted"]
        raw = joystick.get_axis(idx)
        if inv:
            raw = -raw
        channels[name] = raw
    
    buttons = {}
    for cfg in mapping["buttons"]:
        name, idx = cfg["name"], cfg["index"]
        buttons[name] = joystick.get_button(idx)
    
    return channels, buttons

async def test(joystick, mapping):
    while True:
        channels, buttons = read_gamepad(joystick, mapping)
        for name, value in channels.items():
            print(f"{name}: {value:.2f}", end=" ")
        print(" | ", end="")
        for name, value in buttons.items():
            print(f"{name}: {value}", end=" ")
        print()
        await asyncio.sleep(0.1)


def main():
    p = argparse.ArgumentParser(description="Gamepad Mapping")
    p.add_argument("--name", type=str, default="default", help="Name under which to store the mapping")
    p.add_argument("--map", action="store_true", help="Ignore stored mapping and run mapping")
    args = p.parse_args()

    pygame.init()
    pygame.joystick.init()
    if pygame.joystick.get_count() == 0:
        sys.exit("No game controller found.")

    joystick = pygame.joystick.Joystick(0)
    joystick.init()

    mapping = load_or_map(joystick, ["Roll", "Pitch", "Throttle", "Yaw"], ["arm"], args.map, name=args.name)

    print("Press Ctrl+C to stop the test.")
    try:
        asyncio.run(test(joystick, mapping))
    except KeyboardInterrupt:
        print("\nTest stopped by user.")
    finally:
        joystick.quit()
        pygame.quit()


if __name__ == "__main__":
    main()

