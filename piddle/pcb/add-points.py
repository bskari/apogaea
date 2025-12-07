import argparse
import math
import re

COUNT = 15

def main() -> None:
    """Main."""
    parser = argparse.ArgumentParser()
    parser.add_argument("--holes", action="store_true", help="Set the position of the mounting holes")
    parser.add_argument("-l", "--length", type=float, required=True, help="The length from the center")
    parser.add_argument("-x", "--center_x", type=float, required=True, help="The x coordinate of the center")
    parser.add_argument("-y", "--center_y", type=float, required=True, help="The y coordinate of the center")
    args = parser.parse_args()

    if args.holes:
        # -l 90 -x 146.1133 -y 109.5963 seemed perfect
        print(r"Processing holes - replacing {X\d} and {Y\d} with hole coordinates")
        regex = re.compile(r"{X(\d+)} {Y(\d+)}")

        with open("piddle.kicad_pcb.backup", "r") as file:
            lines = file.readlines()

        count = 0
        with open("piddle.kicad_pcb", "w") as file:
            for line in lines:
                if (match := regex.search(line)):
                    number_x, number_y = [int(i) for i in match.groups()]
                    if number_x != number_y:
                        raise ValueError(f"number_x != number_x in line: {line}")

                    angle_spread = 7
                    if number_x % 2 == 0:
                        angle = math.radians(360 / COUNT) * number_x + math.radians(360 / COUNT) / 2 - math.radians(angle_spread)
                    else:
                        angle = math.radians(360 / COUNT) * number_x + math.radians(360 / COUNT) / 2 + math.radians(angle_spread)

                    x = math.sin(angle) * args.length + args.center_x
                    y = math.cos(angle) * args.length + args.center_y
                    line = line.replace(match.group(), f"{x} {y}")
                    count += 1
                file.write(line)
        
        if count != 30:
            raise ValueError(f"Only processed {count} lines, expected 30")
                
    else:
        print("Need to specify something to do")


if __name__ == "__main__":
    main()
