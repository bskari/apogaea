"""Testing some parameters to see how much the solar panel and batteries can last"""
from argparse import ArgumentParser, ArgumentDefaultsHelpFormatter
from dataclasses import dataclass
from enum import Enum
import math
import re
import sys
import typing

has_matplot = False
try:
    import matplotlib.pyplot as plt
    has_matplot = True
except:
    pass


def get_sunlight_percentage(hour: int, minute: int, std_dev: float) -> float:
    """Returns the percent of solar energy the solar panels produce at a time of day."""
    # I made up this std_dev, see
    # https://joshuatauberer.medium.com/solar-numbers-7a7f28d51897
    # for a real curve. It's not perfectly normal, and I fudged the
    # std_dev so that it matches the number of solar hours in southern
    # Colorado in the summer

    mean = 12
    value_at_mean = 1.0

    def probability_density_function(value, mean_, std_dev_) -> float:
        part1 = 1.0 / (std_dev_ * math.sqrt(2.0 * math.pi))
        part2 = math.pow(math.e, -0.5 * (((value - mean_) / std_dev_) ** 2.0))
        return part1 * part2
    pdf = probability_density_function

    pdf_at_x = pdf(hour + minute / 60, mean, std_dev)
    pdf_at_mean = pdf(mean, mean, std_dev)

    scaled_value = (pdf_at_x / pdf_at_mean) * value_at_mean
    # There's a minimum amount before the solar panel starts the photovoltaic process
    if scaled_value < 0.05:
        return 0.0
    return scaled_value


DEFAULT_STD_DEV = 2.3
assert get_sunlight_percentage(5, 0, DEFAULT_STD_DEV) < .01
assert get_sunlight_percentage(7, 0, DEFAULT_STD_DEV) < .2
assert get_sunlight_percentage(12, 0, DEFAULT_STD_DEV) == 1
assert get_sunlight_percentage(11, 0, DEFAULT_STD_DEV) == get_sunlight_percentage(13, 0, DEFAULT_STD_DEV)

DAYS = ("Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat")
def get_day(index: int) -> str:
    return DAYS[(index + 700) % 7]

@dataclass
class Options:
    solar_w: float
    max_charge_w: float | None
    max_battery_wh: float
    off_battery_wh: float
    resume_battery_wh: float
    project_w: float
    std_dev: float
    start_day: int
    day_charge_hour: int | None
    day_charge_minute: int | None
    day_charge_until_hour: int | None
    day_charge_until_minute: int | None
    always_day_charge: bool

@dataclass
class TogglePower:
    minute: int
    on: bool
    day_charging: bool
    # Limited by the charge limit of the charge controller
    limited: bool


def run_simulation(options: Options) -> None:
    """Runs a simulation."""
    # The voltage monitor and Phonic Bloom each use about 0.5 W
    ARDUINO_W = 1.0

    battery_wh = options.max_battery_wh

    day = options.start_day
    START_HOUR = 12
    END_DAY = 8 if options.start_day == 0 else 7
    # Start at minute - 1 so that the first message we print starts at 12:00
    hour = START_HOUR - 1
    minute = 59

    previous_increasing = False
    previous_on = True
    previous_maxed = True
    previous_limited = True

    def format_message() -> None:
        message = f"{get_day(day)} {hour:02d}:{minute:02d}"
        truncated_percent = int(battery_wh / options.max_battery_wh * 100)
        message += f" {battery_wh:>7.2f} Wh {truncated_percent:>3.0f}%"
        if maxed:
            message += " maxed"
        message += (" on" if on else " off")
        if limited != previous_limited:
            if limited:
                message += " limited"
        if not maxed:
            message += (" in" if increasing else " de") + "creasing"
        return message

    on = True
    maxed = False
    day_charge = False
    limited = False

    total_minutes = 0
    battery_wh_by_minute = []
    toggle_power_times: typing.List[TogglePower] = [TogglePower(0, True, False, False)]
    annotations = []
    first_loop = True

    def maybe_add_annotation(hour: int, minute: int, battery_wh: float, offset: float) -> None:
        """Add an annotation if more than 10 minutes have passed since the previous."""
        if len(annotations) == 0 or total_minutes - annotations[-1][1][0] > 10:
            formatted_time = f"{hour:02d}:{minute:02d}"
            annotations.append((
                formatted_time,
                (total_minutes, battery_wh),
                offset,
            ))

    while day < END_DAY or hour < 18:
        minute += 1
        if minute == 60:
            hour += 1
            minute = 0
        if hour == 24:
            day += 1
            hour = 0

        total_minutes += 1

        need_print = first_loop

        battery_wh_by_minute.append(battery_wh)
        previous_battery_wh = battery_wh

        if on and not day_charge:
            battery_wh -= options.project_w / 60
        battery_wh -= ARDUINO_W / 60
        solar_wh_increment = get_sunlight_percentage(hour, minute, options.std_dev) * options.solar_w / 60
        if options.max_charge_w is None:
            battery_wh += solar_wh_increment
            limited = False
        elif solar_wh_increment < options.max_charge_w / 60:
            battery_wh += solar_wh_increment
            limited = False
        else:
            battery_wh += options.max_charge_w / 60
            limited = True

        maxed = False
        if battery_wh < options.off_battery_wh:
            on = False
        elif battery_wh > options.max_battery_wh:
            battery_wh = options.max_battery_wh
            maxed = True

        if battery_wh > options.resume_battery_wh and not options.always_day_charge:
            on = True
            day_charge = False

        increasing = battery_wh > previous_battery_wh

        if options.day_charge_hour is not None:
            if hour == options.day_charge_hour and minute == options.day_charge_minute:
                # Turn off until we hit the resume percent
                if battery_wh < options.resume_battery_wh or options.always_day_charge:
                    on = False
                    need_print = True
                    day_charge = True
            # We can't charge after ~6 PM, so just forcibly turn it back on
            elif day_charge and (
                hour >= 18 or (
                    options.day_charge_until_hour is not None
                    and hour == options.day_charge_until_hour
                    and minute == options.day_charge_until_minute
                )
            ):
                on = True
                day_charge = False

        if increasing != previous_increasing and not maxed:
            need_print = True
        if on != previous_on:
            need_print = True
            toggle_power_times.append(TogglePower(total_minutes, on, day_charge, limited))
            if limited:
                offset = (-50, 0)
            elif on:
                offset = (10, 0)
            else:
                offset = (-50, 0)
            maybe_add_annotation(hour, minute, battery_wh, offset)
        if maxed != previous_maxed:
            need_print = True
            offset = (-50, 0) if maxed else (10, 0)
            maybe_add_annotation(hour, minute, battery_wh, offset)
        if limited != previous_limited:
            need_print = True
            toggle_power_times.append(TogglePower(total_minutes, on, day_charge, limited))
            offset = (-50, 0)
            maybe_add_annotation(hour, minute, battery_wh, offset)

        if need_print:
            print(format_message())

        previous_increasing = increasing
        previous_on = on
        previous_maxed = maxed
        previous_limited = limited
        first_loop = False

    print(format_message())

    # Add one more so the zip below plots all the line segments
    toggle_power_times.append(TogglePower(total_minutes, on, False, False))

    if has_matplot:
        plt.figure(figsize=(10, 5), num="Solar power simulation")
        hours_to_skip = 6
        if (END_DAY - options.start_day) * 24 // hours_to_skip > 25:
            hours_to_skip = 12
        if (END_DAY - options.start_day) * 24 // hours_to_skip > 25:
            hours_to_skip = 24
        tick_positions = [m for m in range(total_minutes) if m % (hours_to_skip * 60) == 0]
        tick_labels = [f"{get_day((options.start_day * 24 * 60 + START_HOUR * 60 + m) // (60 * 24))[:2]}\n{((m + 60 * START_HOUR) // 60) % 24:02d}:00" for m in tick_positions]

        already_labelled = set()
        for start, end in zip(toggle_power_times[:-1], toggle_power_times[1:]):
            if start.day_charging:
                if start.limited:
                    color = "darkblue"
                    label = "off, day charge, limited"
                else:
                    color = "blue"
                    label = "off, day charge"
            elif start.limited:
                if start.on:
                    color = "red"
                    label = "on, limited"
                else:
                    color = "purple"
                    label = "off, limited"
            elif start.on:
                color = "orange"
                label = "on"
            else:
                color = "black"
                label = "off"
            if label in already_labelled:
                plt.plot(
                    range(start.minute, end.minute),
                    battery_wh_by_minute[start.minute:end.minute],
                    color=color,
                )
            else:
                plt.plot(
                    range(start.minute, end.minute),
                    battery_wh_by_minute[start.minute:end.minute],
                    color=color,
                    label=label,
                )
                already_labelled.add(label)

        for message, position, offset in annotations:
            plt.annotate(
                message,
                position,
                textcoords="offset pixels",
                xytext=offset,
            )

        plt.legend()

        # Set custom ticks
        plt.xticks(tick_positions, tick_labels, rotation=60)
        for pos, label in zip(tick_positions, tick_labels):
            if "00:00" in label:
                plt.axvline(x=pos, color="black", linestyle="--", linewidth=0.5)

        yticks = []
        yticks.append((
            options.off_battery_wh,
            f"{int(options.off_battery_wh)} ({int(options.off_battery_wh / options.max_battery_wh * 100)}%)"
        ))
        yticks.append((
            options.resume_battery_wh,
            f"{int(options.resume_battery_wh)} ({int(options.resume_battery_wh / options.max_battery_wh * 100)}%)"
        ))
        for i in range(5):
            candidate = i * options.max_battery_wh / 4
            keep = True
            blank = False
            for yt in yticks[:2]:
                # If it's exactly the same, don't keep it at all
                if math.fabs(candidate - yt[0]) < .0001 * options.max_battery_wh:
                    keep = False
                    break
                # Don't add labels but to add ticks if they're close to the off and resume ticks
                if math.fabs(candidate - yt[0]) < .05 * options.max_battery_wh:
                    blank = True
                    break

            if keep:
                if blank:
                    yticks.append((candidate, ""))
                else:
                    yticks.append((candidate, (str(int(candidate)))))

        plt.yticks([yt[0] for yt in yticks], [yt[1] for yt in yticks])

        plt.axhline(y=options.resume_battery_wh, color="gray", linestyle="--", linewidth=0.5, alpha=0.7)
        plt.axhline(y=options.off_battery_wh, color="gray", linestyle="--", linewidth=0.5, alpha=0.7)
        plt.ylim(bottom=0)
        plt.xlabel("Time")
        plt.ylabel("Wh")
        off_p = options.off_battery_wh / options.max_battery_wh * 100
        on_p = options.resume_battery_wh / options.max_battery_wh * 100
        project_p = (options.project_w - IDLE_W) / (DEFAULT_W - IDLE_W) * 100
        title = f"batt:{options.max_battery_wh:0.0f}Wh off:{off_p:0.0f}% on:{on_p:0.0f}% solar:{options.solar_w:0.0f}W"
        if options.max_charge_w:
            title += f" max:{options.max_charge_w:0.0f}W"
        title += f" power:{project_p:0.0f}%"
        plt.title(title)
        mng = plt.get_current_fig_manager()
        mng.resize(*mng.window.maxsize())
        plt.show()


DEFAULT_W = 2.478 * 2 * 12
IDLE_W = 1.614 * 2 * 12
def make_parser() -> ArgumentParser:
    """Makes a parser."""
    parser = ArgumentParser(prog="power_sim", formatter_class=ArgumentDefaultsHelpFormatter)
    parser.add_argument(
        "--battery-wh",
        "-b",
        type=float,
        help="The max battery capacity in Wh. 100 Ah @ 12.8 V = 1280 Wh.",
        default=12.8 * 100 * 2,
    )
    parser.add_argument(
        "--solar-w",
        "-s",
        type=float,
        help="The solar power in W. I have 300 W, but because of Colorado's latitude, they'll likely only produce ~90%% of their rated power.",
        # 90% because we're not at the equator
        default=300 * .9,
    )
    parser.add_argument(
        "--max-charge-w",
        help="""Max charge in W. If more power comes from the solar panels, it will be dropped.
        MPPT 100/20 max is 145W, but in real world I saw ~138. MPPT 100/20 max is 290W.""",
        type=float,
        # For the MPPT 75/10, the max rate is 145W, but I usually only saw
        # ~138. For the MPPT 100/20, the max rate is 290W, so maybe 276 in
        # real world settings.
        default=138,
    )
    parser.add_argument(
        "--min-battery",
        "-m",
        type=float,
        help="How low the battery should go before it shuts off, in percent.",
        default=25,
    )
    parser.add_argument(
        "--resume-battery",
        "-r",
        type=float,
        help="How high the battery must go before it turns back on, in percent.",
        default=40,
    )
    parser.add_argument(
        "--brightness",
        "-p",
        type=float,
        default=100,
        help="Brightness in percent to run the LEDs at. Either this or -w may be specified, but not both. You can run more than 100, because my 'max' estimate is based on responding to one song, and other songs may light up more LEDs.",
    )
    parser.add_argument(
        "--project-w",
        "-w",
        type=float,
        help=f"The number of Watts the project uses. From testing, it uses {DEFAULT_W:0.0f} W at full brightness while responding to music and {IDLE_W:0.0f} W at idle.",
        default=None,
    )
    parser.add_argument(
        "--start-day",
        "-d",
        type=int,
        help="Start day for the simulation. 0=Sunday (i.e. for BM), 3=Wednesday (i.e. for Apogaea).",
        default=3,
    )
    parser.add_argument(
        "--std-dev",
        type=float,
        help="Standard deviation for solar radiation curve.",
        default=DEFAULT_STD_DEV,
    )
    parser.add_argument(
        "--day-charge",
        help="Instead of shutting off when it gets low, shut off in the morning at the given time and charge during the day",
        type=str,
        default=None,
    )
    parser.add_argument(
        "--day-charge-until",
        help="When to shut off the day charge",
        type=str,
        default=None,
    )
    parser.add_argument(
        "--always-day-charge",
        help="Always day charge, even if the battery is above the resume percentage",
        action="store_true",
    )
    return parser


if __name__ == "__main__":
    parser = make_parser()
    namespace = parser.parse_args()

    def print_error(error: str) -> None:
        sys.stderr.write("Error: " + error + "\n")
        sys.stderr.flush()
        sys.exit()

    if namespace.day_charge is not None and not re.match(r"\d{1,2}:\d{2}", namespace.day_charge):
        print_error(f"Bad day charge time: {namespace.day_charge}, should be e.g. 13:00")
    if namespace.day_charge is not None:
        day_charge_hour = int(namespace.day_charge.split(":")[0])
        day_charge_minute = int(namespace.day_charge.split(":")[1])
    else:
        day_charge_hour = None
        day_charge_minute = None

    if namespace.day_charge_until is not None and not re.match(r"\d{1,2}:\d{2}", namespace.day_charge_until):
        print_error(f"Bad day charge until time: {namespace.day_charge_until}, should be e.g. 13:00")
    if namespace.day_charge_until is not None:
        day_charge_until_hour = int(namespace.day_charge_until.split(":")[0])
        day_charge_until_minute = int(namespace.day_charge_until.split(":")[1])
    else:
        day_charge_until_hour = None
        day_charge_until_minute = None

    if day_charge_hour is not None and day_charge_until_hour is not None:
        if (
            day_charge_hour > day_charge_until_hour or (
                day_charge_hour == day_charge_until_hour
                and day_charge_minute >= day_charge_until_minute
            )
        ):
            print_error(f"Day charge ({namespace.day_charge}) must be before until charge ({namespace.day_charge_until})")

    if namespace.always_day_charge and namespace.day_charge_until is None:
        print_error(f"always-day-charge can only be used with day-charge-until")

    if namespace.min_battery < 1 or namespace.min_battery > 100:
        print_error(f"Bad battery percentage: {namespace.min_battery}, should be 1 < % < 100")
    if namespace.resume_battery < 1 or namespace.resume_battery > 100:
        print_error(f"Bad battery percentage: {namespace.resume_battery}, should be 1 < % < 100")
    if namespace.project_w and namespace.project_w < IDLE_W:
        # Not an error, but we should print a warning
        sys.stderr.write(f"Warning: project W {namespace.project_w:0.2f} is unrealistically below idle W {IDLE_W:0.2f}")
    if namespace.project_w is not None and namespace.brightness != 100:
        print_error("Can only specify one of project-w and brightness")
    if namespace.min_battery >= namespace.resume_battery:
        print_error(f"Resume battery ({namespace.resume_battery}) needs to be less than min battery ({namespace.min_battery})")
    if day_charge_hour is not None and not (6 <= day_charge_hour <= 15):
        print_error(f"Day charge is {namespace.day_charge} but should be between 06:00 and 16:00")
    if day_charge_minute is not None and not (0 <= day_charge_minute <= 59):
        print_error(f"Day charge minute is {day_charge_minute} but should be in [0..59]")
    # Over 100 brightness is okay, because my "max" is based on 1 measurement from
    # responding to a song, and other songs might make more LEDs light up
    if namespace.brightness < 2:  # Check < 2 in case someone enters .5 instead of 50
        print_error(f"Brightness too low: {namespace.brightness}")

    if namespace.project_w is not None:
        project_w = namespace.project_w
    else:
        project_w = (DEFAULT_W - IDLE_W) * namespace.brightness / 100 + IDLE_W

    # Avoid floating point errors
    if namespace.resume_battery == 100:
        namespace.resume_battery = 99.999

    options = Options(
        solar_w=namespace.solar_w,
        max_battery_wh=namespace.battery_wh,
        off_battery_wh=namespace.battery_wh * namespace.min_battery / 100,
        resume_battery_wh=namespace.battery_wh * namespace.resume_battery / 100,
        project_w=project_w,
        std_dev=namespace.std_dev,
        start_day=namespace.start_day,
        day_charge_hour=day_charge_hour,
        day_charge_minute=day_charge_minute,
        day_charge_until_hour=day_charge_until_hour,
        day_charge_until_minute=day_charge_until_minute,
        always_day_charge=namespace.always_day_charge,
        max_charge_w=namespace.max_charge_w,
    )

    # https://www.turbinegenerator.org/solar/colorado/ claims that southern
    # Colorado's peak summer sun hours per day is 5.72
    max_solar_hours = 5.72
    sun_hours = sum((
        get_sunlight_percentage(i, 0, options.std_dev) for i in range(24)
    ))
    print(f"simulated sun hours: {sun_hours:.2f} (max in CO is {max_solar_hours})")
    if sun_hours > max_solar_hours:
        sys.stderr.write("*** Warning! Your std-dev is too high and gives unrealistically high solar hours ***\n")
        sys.stderr.flush()
    print("Running simulation with:")
    print(f"- Battery capacity: {options.max_battery_wh:0.0f} Wh")
    percent = options.off_battery_wh / options.max_battery_wh * 100
    print(f"- Off battery: {percent:0.0f}% / {options.off_battery_wh:0.0f} Wh")
    percent = options.resume_battery_wh / options.max_battery_wh * 100
    print(f"- Resume battery: {percent:0.0f}% / {options.resume_battery_wh:0.0f} Wh")
    if day_charge_hour is not None:
        print(f"- Charging during the day starting at {namespace.day_charge}")
    print(f"- Solar power: {options.solar_w:0.0f} W")
    print(f"- Max charging speed: {options.max_charge_w:0.0f} W")
    print(f"- Solar power std dev: {options.std_dev:0.2f}")
    percent = (options.project_w - IDLE_W) / (DEFAULT_W - IDLE_W) * 100
    print(f"- Project power: {percent:0.0f}% brightness / {options.project_w:0.2f} W")
    print(f"- Start day: {get_day(options.start_day)}")
    run_simulation(options)
