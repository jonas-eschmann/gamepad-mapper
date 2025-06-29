# Gamepad Mapping
```python
import sys, pygame
from gamepad_mapper import load_or_map

pygame.init()
pygame.joystick.init()
if pygame.joystick.get_count() == 0:
    print("No game controller found.")
    sys.exit(1)

joystick = pygame.joystick.Joystick(0)
joystick.init()

mapping = load_or_map(joystick, ["Roll", "Pitch", "Throttle", "Yaw"], ["arm"], force=True, name="example")
```