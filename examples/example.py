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