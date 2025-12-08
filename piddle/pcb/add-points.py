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
PART = math.radians(360 / COUNT)

def main() -> None:
    """Main."""
    parser = argparse.ArgumentParser()
    parser.add_argument("-l", "--length", type=float, required=True, help="The length from the center")
    parser.add_argument("-x", "--center_x", type=float, required=True, help="The x coordinate of the center")
    parser.add_argument("-y", "--center_y", type=float, required=True, help="The y coordinate of the center")
    args = parser.parse_args()

    with open("piddle.kicad_pcb.backup", "r") as file:
        lines = file.readlines()

    hole_count = 0
    ground_plane_count = 0
    edge_cut_count = 0

    new_lines = []

    # Calculate the points for edge cut and ground plane
    polygon_points = []
    for i in range(COUNT):
        angle = PART * i
        x = math.sin(angle) * args.length + args.center_x
        y = math.cos(angle) * args.length + args.center_y
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
            if '(footprint "MountingHole:MountingHole_3.2mm_M3" (layer "F.Cu")' in line:
                new_lines.append(line)
                skip_lines(1, True, lambda l: l.strip().startswith("(tstamp"))

                skip_lines(1, False, lambda l: l.strip().startswith("(at"))
                angle_spread = 4
                if hole_count % 2 == 0:
                    angle = PART * hole_count - math.radians(angle_spread)
                else:
                    angle = PART * hole_count + math.radians(angle_spread)

                x = math.sin(angle) * (args.length - 10) + args.center_x
                y = math.cos(angle) * (args.length - 10) + args.center_y
                new_lines.append(f"    (at {x:0.4f} {y:0.4f})\n")
                hole_count += 1

            # Ground plane
            elif '(zone (net 1) (net_name "GND") (layers "F&B.Cu")' in line:
                new_lines.append(line)
                # Skip the lines until we get to the polygon points
                skip_lines(5, True, lambda l: "(xy" not in l)
                # Skip the points
                skip_lines(COUNT, False, lambda l: "(xy" in l)
                for point in polygon_points:
                    new_lines.append(f"        (xy {point[0]:0.4f} {point[1]:0.4f})\n")
                    ground_plane_count += 1

            # Edge cut
            elif "(gr_poly" in line:
                new_lines.append(line)
                skip_lines(1, True, lambda l: "(pts" in l)
                # Skip the points
                skip_lines(COUNT, False, lambda l: "(xy" in l)
                for point in polygon_points:
                    new_lines.append(f"      (xy {point[0]:0.4f} {point[1]:0.4f})\n")
                    edge_cut_count += 1

            else:
                new_lines.append(line)

    except StopIteration:
        pass

    if hole_count != COUNT * 2:
        raise ValueError(f"Only processed {hole_count} holes, expected {COUNT * 2}")
    if edge_cut_count != COUNT:
        raise ValueError(f"Only processed {edge_cut_count} edge cuts, expected {COUNT}")
    if ground_plane_count != COUNT:
        raise ValueError(f"Only processed {ground_plane_count} ground plane edges, expected {COUNT}")
    if len(new_lines) != len(lines):
        message = f"Line count differs: old {len(lines)}, new {len(new_lines)}"
        print(message)
        #raise ValueError(message)

    print("Writing updated file")
    with open("piddle.kicad_pcb", "w") as file:
        for line in new_lines:
            file.write(line)


if __name__ == "__main__":
    main()
