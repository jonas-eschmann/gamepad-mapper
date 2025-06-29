import sys, pygame
from calibration import load_or_calibrate

pygame.init()
pygame.joystick.init()
if pygame.joystick.get_count() == 0:
    print("No game controller found.")
    sys.exit(1)

joystick = pygame.joystick.Joystick(0)
joystick.init()

mapping = load_or_calibrate(joystick, ["Roll", "Pitch", "Throttle", "Yaw"], ["arm"], force_calibration=True, name="example")