"""
Script to lay out the elements of the board in a PCB. To use this:
Create a board with COUNT (i.e. 15) elements, anywhere on the PCB, e.g.:
- 15 LEDs
- 15 resistors
- 15 LED through holes
- 30 mounting holes
Save the file as piddle.kicad_pcb.backup, then run this. It will then create a
15-sided board, and arrange the elements.
python add-points.py -l 70 -x 100 -y 100 seems to work well.
"""

import argparse
import itertools
import math
import re
import typing


COUNT = 15
RESISTOR_HEADER = '(footprint "Resistor_THT:R_Axial_DIN0207_L6.3mm_D2.5mm_P10.16mm_Horizontal" (layer "F.Cu")'
PIN_HEADER = '(footprint "Connector_PinHeader_2.54mm:PinHeader_1x03_P2.54mm_Vertical" (layer "F.Cu")'
LED_HEADER = '(footprint "LED_SMD:LED_WS2812B_PLCC4_5.0x5.0mm_P3.2mm" (layer "F.Cu")'


def clamp_d(angle_d):
    """Clamp an angle to -180..180"""
    while angle_d > 180:
        angle_d -= 360
    while angle_d < -180:
        angle_d += 360
    return angle_d


def sort_lines(lines: typing.List[str]) -> typing.List[str]:
    """Put the component lines in order."""
    # The LEDs, resistors, and through holes are all out of order? So just reorder them.
    iterator = iter(lines)

    def get_block(iterator2, first_line: str) -> typing.List[str]:
        block_lines = [first_line]
        parenthesis_count = first_line.count("(") - first_line.count(")")
        while parenthesis_count > 0:
            line = next(iterator2)
            block_lines.append(line)
            parenthesis_count += line.count("(") - line.count(")")

        return block_lines

    new_lines = []
    resistors = []
    leds = []
    pin_headers = []

    resistor_index = None
    try:
        while True:
            line = next(iterator)
            if line.strip() == RESISTOR_HEADER:
                if len(resistors) == 0:
                    resistor_index = len(new_lines)
                resistors.append(get_block(iterator, line))
            elif line.strip() == PIN_HEADER:
                pin_headers.append(get_block(iterator, line))
            elif line.strip() == LED_HEADER:
                leds.append(get_block(iterator, line))
            else:
                new_lines.append(line)

    except StopIteration:
        pass

    if resistor_index is None:
        raise ValueError("No resistors found?")

    regex = re.compile(r'"R(\d+)"')
    resistors.sort(key=lambda array: int(regex.search(array[11]).groups()[0]))
    regex = re.compile(r'"J(\d+)"')
    pin_headers.sort(key=lambda array: int(regex.search(array[11]).groups()[0]))
    regex = re.compile(r'"D(\d+)"')
    leds.sort(key=lambda array: int(regex.search(array[11]).groups()[0]))

    assert COUNT == len(resistors) == len(leds) == len(pin_headers)
    #breakpoint()
    return (
        new_lines[:resistor_index]
        # I think we can put all of these right after one another
        + list(itertools.chain.from_iterable(resistors))
        + list(itertools.chain.from_iterable(leds))
        + list(itertools.chain.from_iterable(pin_headers))
        + new_lines[resistor_index:]
    )


def arrange_components(lines: typing.List[str], length: float, center_x: float, center_y: float)-> typing.Tuple[typing.List[str], bool]:
    """Arrange the components."""
    PART_R = math.radians(360 / COUNT)
    hole_count = 0
    ground_plane_count = 0
    edge_cut_count = 0
    resistor_count = 0
    pin_header_count = 0
    led_count = 0

    new_lines = []

    # Calculate the points for edge cut and ground plane
    polygon_points = []
    for i in range(COUNT):
        angle_r = PART_R * i
        x = math.sin(angle_r) * length + center_x
        y = math.cos(angle_r) * length + center_y
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
                    angle_r = PART_R * (hole_count + 1) - math.radians(angle_r_spread)
                else:
                    angle_r = PART_R * (hole_count + 1) + math.radians(angle_r_spread)

                x = math.sin(angle_r) * (length - 10) + center_x
                y = math.cos(angle_r) * (length - 10) + center_y
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
            elif line.strip() == RESISTOR_HEADER:
                new_lines.append(line)
                skip_lines(1, True, lambda l: l.strip().startswith("(tstamp"))
                skip_lines(1, False, lambda l: l.strip().startswith("(at"))
                angle_r = PART_R * (resistor_count + 1)
                resistor_length = length - 25
                x = math.sin(angle_r) * resistor_length + center_x
                y = math.cos(angle_r) * resistor_length + center_y
                angle_d = clamp_d(math.degrees(angle_r) + 90 + 180)
                new_lines.append(f"    (at {x:0.4f} {y:0.4f} {int(angle_d)})\n")
                resistor_count += 1

            # Pin headers
            elif line.strip() == PIN_HEADER:
                new_lines.append(line)
                skip_lines(1, True, lambda l: l.strip().startswith("(tstamp"))
                skip_lines(1, False, lambda l: l.strip().startswith("(at"))
                angle_r = PART_R * (pin_header_count + 1) + PART_R / 2
                # The position of the headers is one of the side pins, not the center, so we need
                # to bump it a bit more to keep it centered
                projection_angle_r = angle_r + PART_R / 8
                resistor_length = length - 20
                x = math.sin(projection_angle_r) * resistor_length + center_x
                y = math.cos(projection_angle_r) * resistor_length + center_y
                angle_d = clamp_d(math.degrees(angle_r) - 90)
                new_lines.append(f"    (at {x:0.4f} {y:0.4f} {int(angle_d)})\n")
                pin_header_count += 1

            # LEDs
            elif line.strip() == LED_HEADER:
                new_lines.append(line)
                skip_lines(1, True, lambda l: l.strip().startswith("(tstamp"))
                skip_lines(1, False, lambda l: l.strip().startswith("(at"))
                angle_r = PART_R * (led_count + 1) + PART_R / 2
                led_length = length - 25
                x = math.sin(angle_r) * led_length + center_x
                y = math.cos(angle_r) * led_length + center_y
                angle_d = clamp_d(math.degrees(angle_r) + 90)
                new_lines.append(f"    (at {x:0.4f} {y:0.4f} {int(angle_d)})\n")
                led_count += 1
            else:
                new_lines.append(line)

    except StopIteration:
        pass


    success = True

    if hole_count != COUNT * 2:
        message = f"Only processed {hole_count} holes, expected {COUNT * 2}"
        success = False
        print(message)

    type_to_count = {
        "edge_cuts": edge_cut_count,
        "ground_planes": ground_plane_count,
        "resistors": resistor_count,
        "pin_headers": pin_header_count,
        "leds": led_count,
    }
    for key, value in type_to_count.items():
        if value != COUNT:
            message = f"Only processed {value} {key}, expected {COUNT}"
            success = False
            print(message)

    if len(new_lines) != len(lines):
        message = f"Line count differs: old {len(lines)}, new {len(new_lines)}"
        success = False
        print(message)
    
    return new_lines, success


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
    
    lines = sort_lines(lines)
    lines, success = arrange_components(lines, args.length, args.center_x, args.center_y)
    if success or args.force:
        print("Writing updated file")
        with open("piddle.kicad_pcb", "w") as file:
            for line in lines:
                file.write(line)


if __name__ == "__main__":
    main()
