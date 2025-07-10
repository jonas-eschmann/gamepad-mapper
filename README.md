# Gamepad Mapping
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