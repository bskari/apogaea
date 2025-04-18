"""Generates C code for various things."""
import dataclasses
import math
import sys
import typing


@dataclasses.dataclass
class PixelInfo:
    strand_color: str
    strand_offset: str
    total_offset: str


debug = False

def debug_print(s: str) -> None:
    global debug
    if debug:
        print(s)


def print_luts() -> None:
    """Print the lookup tables. Returns """
    # Stage right goes from x:[0-10] and y:[4-14]
    # Central goes from x:[11-23] and y:[0-14]
    # Stage left goes from x:[24-31] and y:[4-14]
    # White GPIO17 central upper stage left
    # Blue GPIO21 stage right
    # Red GPIO4 central upper stage right
    # Green GPIO0 central lower
    # Black GPIO15 stage left
    # Formatting here: n for skip, then x,y pairs with an optional inclusive range for one of x or y
    max_expected_x = 31
    max_expected_y = 14
    formats = {
        "white": "n, n, n, n, n, (23-17:8), (17:9-14), n, (18:14-9), n, (19:9-14), n, (20:14-9), n, (21:9-14), n, (22:14-9),(23:9)",
        "blue": "n, n, (23-11:4), (10-0:3), (0:4-9), n, (1:9-4), n, (2:4-11), n, (3:11-4), n, (4:4-13), n, (5:13-4), n, (6:4-6), n, (7:6-5), (8-23:5)",
        "red": "n, n, n, n, (23-11:6), (11:7-8), n, (12:8-7), n, (13:7-14), n, (14:14-7), n, (15:7-14), n, (16:14-7), (17-23:7)",
        "green": "n, n, n, n, n, (23-12:3), n, (12-22:2), n, (22-12:1), n, (12-23:0), (23:1-2)",
        "black": "n, (24:5-7), n, (25:7-5), n, (26:5-14), n, (27:14-5), n, (28:5-11), (29:11-5), n, (30:5-9), n, (31:9-4), (30-25:4)",
    }
    expected_skips = {
        "white": 5,
        "blue": 2,
        "red": 4,
        "green": 5,
        "black": 1,
    }

    # Map of (x,y) to (color,LED)
    def get_range(start: typing.Union[str, int], end: typing.Union[str, int]):
        start = int(start)
        end = int(end)
        if start < end:
            return range(start, end + 1)
        else:
            return range(start, end - 1, -1)

    grid = {}
    def add_entry(
        x: typing.Union[str, int],
        y: typing.Union[str, int],
        color: str,  # Strand pin color
        strand_led_count: int,
        total_led_count: int,
    ) -> None:
        x = int(x)
        y = int(y)
        # total_offset will be filled in later
        value = PixelInfo(
            strand_color=color,
            strand_offset=strand_led_count,
            total_offset=total_led_count,
        )
        coordinate = (x, y)

        if not (0 <= x <= max_expected_x and 0 <= y <= max_expected_y):
            print(f"Tried to add {coordinate} for {value} LED count {strand_led_count} but out of range")
            assert 0 <= x <= max_expected_x and 0 <= y <= max_expected_y
        if coordinate not in grid:
            grid[coordinate] = value
            strand_led_count += 1
        else:
            print(f"coordinate {coordinate} for {value} already in grid {grid[coordinate]} LED count {strand_led_count}")
            assert coordinate not in grid

    color_to_count = {}
    total_led_count = 0
    def fill_grid() -> None:
        total_active_led_count = 0

        for color, format_str in formats.items():
            debug_print(f"Processing {color}")
            parts = format_str.replace(" ", "").split(",")
            strand_led_count = 0
            active_led_count = 0
            started = False
            nonlocal total_led_count
            for part in parts:
                if part == "n":
                    strand_led_count += 1
                    total_led_count += 1
                else:
                    # Sanity check
                    if not started:
                        assert(strand_led_count == expected_skips[color])
                    started = True

                    xs, ys = part[1:-1].split(":")
                    if "-" in xs:
                        start, end = xs.split("-")
                        for x in get_range(start, end):
                            add_entry(x, ys, color, strand_led_count, total_led_count)
                            active_led_count += 1
                            strand_led_count += 1
                            total_led_count += 1
                    elif "-" in ys:
                        start, end = ys.split("-")
                        for y in get_range(start, end):
                            add_entry(xs, y, color, strand_led_count, total_led_count)
                            active_led_count += 1
                            strand_led_count += 1
                            total_led_count += 1
                    else:
                        add_entry(xs, ys, color, strand_led_count, total_led_count)
                        active_led_count += 1
                        strand_led_count += 1
                        total_led_count += 1

            debug_print(f"Processed {color}, had {strand_led_count} LEDs, {active_led_count} active")
            color_to_count[color] = strand_led_count
            total_active_led_count += active_led_count

        debug_print(f"{total_active_led_count} active LEDs of {total_led_count} total")

    fill_grid()
    min_x = min(coordinate[0] for coordinate in grid)
    max_x = max(coordinate[0] for coordinate in grid)
    min_y = min(coordinate[1] for coordinate in grid)
    max_y = max(coordinate[1] for coordinate in grid)
    assert max_x == max_expected_x
    assert max_y == max_expected_y
    assert min_x == 0
    assert min_y == 0
    debug_print(f"{min_x=} {max_x=} {min_y=} {max_y=}")

    array: typing.List[typing.List[typing.Union[PixelInfo, None]]] = []
    for y in range(max_x + 1):
        array.append([None] * (max_y + 1))

    for key, value in grid.items():
        x, y = key
        assert array[x][y] is None
        array[x][y] = value

    color_to_strip = {
        "white": 0,
        "blue": 1,
        "red": 2,
        "green": 3,
        "black": 4,
    }

    if debug:
        return

    unused = "65535"
    led_count_per_strand = [
        color_to_count[i[1]] for i in
            sorted([(v, k) for k, v in color_to_strip.items()])
    ]
    print(f"""
#include <stdint.h>
const uint16_t UNUSED_LED = {unused};

#ifdef NRF52840_XXAA
#include "FastLED/src/platforms/arm/nrf52/clockless_arm_nrf52.h"
static_assert(FASTLED_NRF52_MAXIMUM_PIXELS_PER_STRING >= LED_COUNT, "You need to edit clockless_arm_nrf52.h and increase FASTLED_NRF52_MAXIMUM_PIXELS_PER_STRING");
#endif

const int LED_COLUMN_COUNT = {max_x + 1};
const int LED_ROW_COUNT = {max_y + 1};
constexpr int STRAND_TO_LED_COUNT[] = {{ {", ".join((str(i) for i in led_count_per_strand))} }};
constexpr int LINEAR_LED_INDEXES[] = {{ {", ".join(str(sum(led_count_per_strand[:i])) for i in range(0, len(led_count_per_strand)))} }};
const int LED_COUNT = {total_led_count};
const int STRAND_COUNT = {len(formats)};

// x first then y, starting at lower left corner
""")

    print(f"const uint16_t XY_TO_OFFSET[LED_COLUMN_COUNT][LED_ROW_COUNT] = {{")
    for x in range(max_x + 1):
        joined = ", ".join((unused if i is None else str(i.total_offset) for i in array[x]))
        print(f"    {{{joined}}},")
    print("};")

    return max_y, max_x


def print_precomputed_bidoulle_v3(row_count: int, column_count: int) -> None:
    """Prints the precomputed Bidoulle v3 values."""
    print("const uint8_t bidoulleV3rings[LED_COLUMN_COUNT * 2][LED_ROW_COUNT * 2] = {")

    for x in range(-column_count // 2, 3 * column_count // 2):
        sys.stdout.write("  {")
        for y in range(-row_count // 2, 3 * row_count // 2):
            xSqr = (x - column_count // 2) ** 2
            ySqr = (y - row_count // 2) ** 2
            value = round((math.sin(math.sqrt(0.1 * (xSqr + ySqr) + 1.0)) + 1.0) * 127)
            sys.stdout.write(f"{value}, ")
        print("},")
    print("};")


def main() -> None:
    global debug
    if len(sys.argv) > 1:
        debug = True

    if debug:
        print_luts()
    else:
        row_count, column_count = print_luts()
        print_precomputed_bidoulle_v3(row_count, column_count)


if __name__ == "__main__":
    main()
