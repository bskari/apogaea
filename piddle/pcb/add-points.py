"""
Script to lay out the elements of the board in a PCB. To use this:
Create a board with COUNT (i.e. 15) elements, anywhere on the PCB, e.g.:
- 15 LEDs
- 15 resistors
- 15 LED through holes
- 30 mounting holes
Save the file as piddle.kicad_pcb.backup, then run this. It will then create a
15-sided board, and arrange the elements.
"""

import argparse
import math

COUNT = 15
PART_R = math.radians(360 / COUNT)


def clamp_d(angle_d):
    while angle_d > 180:
        angle_d -= 360
    while angle_d < -180:
        angle_d += 360
    return angle_d


def main() -> None:
    """Main."""
    parser = argparse.ArgumentParser()
    parser.add_argument("-l", "--length", type=float, required=True, help="The length from the center")
    parser.add_argument("-x", "--center_x", type=float, required=True, help="The x coordinate of the center")
    parser.add_argument("-y", "--center_y", type=float, required=True, help="The y coordinate of the center")
    parser.add_argument("-f", "--force", default=False, action="store_true", help="Write out even if there are errors")
    args = parser.parse_args()

    with open("piddle.kicad_pcb.backup", "r") as file:
        lines = file.readlines()

    hole_count = 0
    ground_plane_count = 0
    edge_cut_count = 0
    resistor_count = 0

    new_lines = []

    # Calculate the points for edge cut and ground plane
    polygon_points = []
    for i in range(COUNT):
        angle_r = PART_R * i
        x = math.sin(angle_r) * args.length + args.center_x
        y = math.cos(angle_r) * args.length + args.center_y
        polygon_points.append((x, y))

    iterator = iter(lines)
    line = None

    def skip_lines(count: int, append: bool, assertion = None) -> None:
        nonlocal line
        for _ in range(count):
            line = next(iterator)
            if assertion is not None:
                assert assertion(line), line
            if append:
                new_lines.append(line)

    try:
        while True:
            line = next(iterator)
            # Mounting holes
            if line.strip() == '(footprint "MountingHole:MountingHole_3.2mm_M3" (layer "F.Cu")':
                new_lines.append(line)
                skip_lines(1, True, lambda l: l.strip().startswith("(tstamp"))

                skip_lines(1, False, lambda l: l.strip().startswith("(at"))
                angle_r_spread = 4
                if hole_count % 2 == 0:
                    angle_r = PART_R * hole_count - math.radians(angle_r_spread)
                else:
                    angle_r = PART_R * hole_count + math.radians(angle_r_spread)

                x = math.sin(angle_r) * (args.length - 10) + args.center_x
                y = math.cos(angle_r) * (args.length - 10) + args.center_y
                new_lines.append(f"    (at {x:0.4f} {y:0.4f})\n")
                hole_count += 1

            # Ground plane
            elif line.strip() == '(zone (net 1) (net_name "GND") (layers "F&B.Cu") (tstamp ada41c68-0de9-470a-8df8-3f4c66fa4ce9) (hatch edge 0.5)':
                new_lines.append(line)
                # Skip the lines until we get to the polygon points
                skip_lines(5, True, lambda l: "(xy" not in l)
                # Skip the points
                skip_lines(COUNT, False, lambda l: "(xy" in l)
                for point in polygon_points:
                    new_lines.append(f"        (xy {point[0]:0.4f} {point[1]:0.4f})\n")
                    ground_plane_count += 1

            # Edge cut
            elif line.strip() == "(gr_poly":
                new_lines.append(line)
                skip_lines(1, True, lambda l: "(pts" in l)
                # Skip the points
                skip_lines(COUNT, False, lambda l: "(xy" in l)
                for point in polygon_points:
                    new_lines.append(f"      (xy {point[0]:0.4f} {point[1]:0.4f})\n")
                    edge_cut_count += 1

            # Resistors
            elif line.strip() == '(footprint "Resistor_THT:R_Axial_DIN0207_L6.3mm_D2.5mm_P10.16mm_Horizontal" (layer "F.Cu")':
                new_lines.append(line)
                skip_lines(1, True, lambda l: l.strip().startswith("(tstamp"))
                skip_lines(1, False, lambda l: l.strip().startswith("(at"))
                angle_r = PART_R * resistor_count
                length = args.length - 25
                x = math.sin(angle_r) * length + args.center_x
                y = math.cos(angle_r) * length + args.center_y
                angle_d = clamp_d(math.degrees(angle_r) + 90)
                new_lines.append(f"    (at {x:0.4f} {y:0.4f} {int(angle_d)})\n")
                resistor_count += 1

            else:
                new_lines.append(line)

    except StopIteration:
        pass

    if hole_count != COUNT * 2:
        message = f"Only processed {hole_count} holes, expected {COUNT * 2}"
        if args.force:
            print(message)
        else:
            raise ValueError(message)

    type_to_count = {
        "edge_cuts": edge_cut_count,
        "ground_planes": ground_plane_count,
        "resistors": resistor_count,
    }
    for key, value in type_to_count.items():
        if value != COUNT:
            message = f"Only processed {value} {key}, expected {COUNT}"
            if args.force:
                print(message)
            else:
                raise ValueError(message)

    if len(new_lines) != len(lines):
        message = f"Line count differs: old {len(lines)}, new {len(new_lines)}"
        if args.force:
            print(message)
        else:
            raise ValueError(message)

    print("Writing updated file")
    with open("piddle.kicad_pcb", "w") as file:
        for line in new_lines:
            file.write(line)


if __name__ == "__main__":
    main()
