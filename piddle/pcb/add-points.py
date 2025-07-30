import math
import sys

count = 15
length = 70 if len(sys.argv) < 2 else float(sys.argv[1])
print(length)

points = []

for i in range(count):
    partial_angle = math.radians(360 / count)
    angle = partial_angle * i
    x = math.sin(angle) * length
    y = math.cos(angle) * length
    points.append(f"(xy {x+106} {y+114})")
    print(points[-1])

with open("piddle.kicad_pcb", "r") as file:
    lines = file.readlines()

with open("piddle.kicad_pcb", "w") as file:
    iterator = iter(lines)
    while True:
        try:
            line = next(iterator)
            if "(gr_poly" in line or "(polygon" in line:
                file.write(line)
                file.write(next(iterator)) # (pts
                for p in points:
                    file.write(p + "\n")
                while line.strip() != ")":
                    line = next(iterator)
                file.write(line)
            else:
                file.write(line)
        except StopIteration:
            break
