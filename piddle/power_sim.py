"""Testing some parameters to see how much the solar panel and batteries can last"""

import math
import re
import sys
import time
import typing
from argparse import ArgumentDefaultsHelpFormatter, ArgumentParser
from dataclasses import dataclass
from enum import Enum

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
assert get_sunlight_percentage(5, 0, DEFAULT_STD_DEV) < 0.01
assert get_sunlight_percentage(7, 0, DEFAULT_STD_DEV) < 0.2
assert get_sunlight_percentage(12, 0, DEFAULT_STD_DEV) == 1
assert get_sunlight_percentage(11, 0, DEFAULT_STD_DEV) == get_sunlight_percentage(
    13, 0, DEFAULT_STD_DEV
)

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


def run_simulation_data(options: Options):
    """Pure computation: runs the simulation and returns all data without printing or plotting."""
    # The voltage monitor and Phonic Bloom each use about 0.5 W
    ARDUINO_W = 1.0

    battery_wh = options.max_battery_wh

    STEP = 1  # minutes per simulation tick

    day = options.start_day
    START_HOUR = 12
    END_DAY = 8 if options.start_day == 0 else 7
    # Start at minute - STEP so that the first message we print starts at 12:00
    hour = START_HOUR - 1
    minute = 60 - STEP

    previous_increasing = False
    previous_on = True
    previous_maxed = True
    previous_limited = True

    on = True
    maxed = False
    day_charge = False
    limited = False
    increasing = False

    total_minutes = 0
    battery_wh_by_minute = []
    toggle_power_times: typing.List[TogglePower] = [TogglePower(0, True, False, False)]
    annotations = []
    messages = []
    first_loop = True

    def maybe_add_annotation(h: int, m: int, bwh: float, offset: tuple) -> None:
        """Add an annotation if more than 10 minutes have passed since the previous."""
        if len(annotations) == 0 or total_minutes - annotations[-1][1][0] > 10:
            annotations.append((f"{h:02d}:{m:02d}", (total_minutes, bwh), offset))

    while day < END_DAY or hour < 18:
        minute += STEP
        if minute >= 60:
            hour += 1
            minute = 0
        if hour == 24:
            day += 1
            hour = 0

        total_minutes += STEP

        need_print = first_loop

        battery_wh_by_minute.extend([battery_wh] * STEP)
        previous_battery_wh = battery_wh

        if on and not day_charge:
            battery_wh -= options.project_w * STEP / 60
        battery_wh -= ARDUINO_W * STEP / 60
        solar_wh_increment = (
            get_sunlight_percentage(hour, minute, options.std_dev)
            * options.solar_w
            * STEP
            / 60
        )
        if options.max_charge_w is None:
            battery_wh += solar_wh_increment
            limited = False
        elif solar_wh_increment < options.max_charge_w * STEP / 60:
            battery_wh += solar_wh_increment
            limited = False
        else:
            battery_wh += options.max_charge_w * STEP / 60
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
            if (
                hour == options.day_charge_hour
                and options.day_charge_minute
                <= minute
                < options.day_charge_minute + STEP
            ):
                # Turn off until we hit the resume percent
                if battery_wh < options.resume_battery_wh or options.always_day_charge:
                    on = False
                    need_print = True
                    day_charge = True
            # We can't charge after ~6 PM, so just forcibly turn it back on
            elif day_charge and (
                hour >= 18
                or (
                    options.day_charge_until_hour is not None
                    and hour == options.day_charge_until_hour
                    and options.day_charge_until_minute
                    <= minute
                    < options.day_charge_until_minute + STEP
                )
            ):
                on = True
                day_charge = False

        if increasing != previous_increasing and not maxed:
            need_print = True
        if on != previous_on:
            need_print = True
            toggle_power_times.append(
                TogglePower(total_minutes, on, day_charge, limited)
            )
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
            toggle_power_times.append(
                TogglePower(total_minutes, on, day_charge, limited)
            )
            maybe_add_annotation(hour, minute, battery_wh, (-50, 0))

        if need_print:
            msg = f"{get_day(day)} {hour:02d}:{minute:02d}"
            pct = int(battery_wh / options.max_battery_wh * 100)
            msg += f" {battery_wh:>7.2f} Wh {pct:>3.0f}%"
            if maxed:
                msg += " maxed"
            msg += " on" if on else " off"
            if limited != previous_limited and limited:
                msg += " limited"
            if not maxed:
                msg += (" in" if increasing else " de") + "creasing"
            messages.append(msg)

        previous_increasing = increasing
        previous_on = on
        previous_maxed = maxed
        previous_limited = limited
        first_loop = False

    # Final message
    msg = f"{get_day(day)} {hour:02d}:{minute:02d}"
    pct = int(battery_wh / options.max_battery_wh * 100)
    msg += f" {battery_wh:>7.2f} Wh {pct:>3.0f}%"
    if maxed:
        msg += " maxed"
    msg += " on" if on else " off"
    if not maxed:
        msg += (" in" if increasing else " de") + "creasing"
    messages.append(msg)

    # Add one more so the zip in draw_plot plots all the line segments
    toggle_power_times.append(TogglePower(total_minutes, on, False, False))

    return (
        battery_wh_by_minute,
        toggle_power_times,
        annotations,
        messages,
        total_minutes,
        START_HOUR,
    )


def draw_plot(
    ax,
    options: Options,
    battery_wh_by_minute,
    toggle_power_times,
    annotations,
    total_minutes: int,
    START_HOUR: int,
) -> None:
    """Draw the simulation results onto the given matplotlib Axes."""
    END_DAY = 8 if options.start_day == 0 else 7

    hours_to_skip = 6
    if (END_DAY - options.start_day) * 24 // hours_to_skip > 25:
        hours_to_skip = 12
    if (END_DAY - options.start_day) * 24 // hours_to_skip > 25:
        hours_to_skip = 24
    tick_positions = [m for m in range(total_minutes) if m % (hours_to_skip * 60) == 0]
    tick_labels = [
        f"{get_day((options.start_day * 24 * 60 + START_HOUR * 60 + m) // (60 * 24))[:2]}\n{((m + 60 * START_HOUR) // 60) % 24:02d}:00"
        for m in tick_positions
    ]

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
        plot_kwargs: dict = dict(color=color)
        if label not in already_labelled:
            plot_kwargs["label"] = label
            already_labelled.add(label)
        ax.plot(
            range(start.minute, end.minute),
            battery_wh_by_minute[start.minute : end.minute],
            **plot_kwargs,
        )

    for message, position, offset in annotations:
        ax.annotate(
            message,
            position,
            textcoords="offset pixels",
            xytext=offset,
        )

    ax.legend()

    ax.set_xticks(tick_positions)
    ax.set_xticklabels(tick_labels, rotation=60)
    for pos, label in zip(tick_positions, tick_labels):
        if "00:00" in label:
            ax.axvline(x=pos, color="black", linestyle="--", linewidth=0.5)

    yticks = []
    yticks.append(
        (
            options.off_battery_wh,
            f"{int(options.off_battery_wh)} ({int(options.off_battery_wh / options.max_battery_wh * 100)}%)",
        )
    )
    yticks.append(
        (
            options.resume_battery_wh,
            f"{int(options.resume_battery_wh)} ({int(options.resume_battery_wh / options.max_battery_wh * 100)}%)",
        )
    )
    for i in range(5):
        candidate = i * options.max_battery_wh / 4
        keep = True
        blank = False
        for yt in yticks[:2]:
            # If it's exactly the same, don't keep it at all
            if math.fabs(candidate - yt[0]) < 0.0001 * options.max_battery_wh:
                keep = False
                break
            # Don't add labels but do add ticks if they're close to the off and resume ticks
            if math.fabs(candidate - yt[0]) < 0.05 * options.max_battery_wh:
                blank = True
                break

        if keep:
            if blank:
                yticks.append((candidate, ""))
            else:
                yticks.append((candidate, str(int(candidate))))

    ax.set_yticks([yt[0] for yt in yticks])
    ax.set_yticklabels([yt[1] for yt in yticks])

    ax.axhline(
        y=options.resume_battery_wh,
        color="gray",
        linestyle="--",
        linewidth=0.5,
        alpha=0.7,
    )
    ax.axhline(
        y=options.off_battery_wh, color="gray", linestyle="--", linewidth=0.5, alpha=0.7
    )
    ax.set_ylim(bottom=0)
    ax.set_xlabel("Time")
    ax.set_ylabel("Wh")

    off_p = options.off_battery_wh / options.max_battery_wh * 100
    on_p = options.resume_battery_wh / options.max_battery_wh * 100
    project_p = (options.project_w - IDLE_W) / (DEFAULT_W - IDLE_W) * 100
    title = f"batt:{options.max_battery_wh:0.0f}Wh off:{off_p:0.0f}% on:{on_p:0.0f}% solar:{options.solar_w:0.0f}W"
    if options.max_charge_w:
        title += f" max:{options.max_charge_w:0.0f}W"
    title += f" power:{project_p:0.0f}%"
    ax.set_title(title)


def run_simulation(options: Options) -> None:
    """Runs a simulation."""
    (
        battery_wh_by_minute,
        toggle_power_times,
        annotations,
        messages,
        total_minutes,
        START_HOUR,
    ) = run_simulation_data(options)

    if not has_matplot:
        return

    from matplotlib.widgets import Slider

    fig = plt.figure(figsize=(12, 7), num="Solar power simulation")

    # Main plot area — leave the bottom 24% for sliders
    ax = fig.add_axes([0.08, 0.30, 0.89, 0.62])
    draw_plot(
        ax,
        options,
        battery_wh_by_minute,
        toggle_power_times,
        annotations,
        total_minutes,
        START_HOUR,
    )

    # ── Slider initial values ────────────────────────────────────────────────
    init_brightness = (options.project_w - IDLE_W) / (DEFAULT_W - IDLE_W) * 100
    init_min_bat = options.off_battery_wh / options.max_battery_wh * 100
    init_resume_bat = options.resume_battery_wh / options.max_battery_wh * 100
    init_max_charge = (
        options.max_charge_w if options.max_charge_w is not None else 400.0
    )

    width = 0.31
    # ── Left column ──────────────────────────────────────────────────────────
    ax_battery = fig.add_axes([0.12, 0.16, width, 0.025])
    ax_solar = fig.add_axes([0.12, 0.12, width, 0.025])
    ax_max_charge = fig.add_axes([0.12, 0.08, width, 0.025])

    # ── Right column ─────────────────────────────────────────────────────────
    ax_min_bat = fig.add_axes([0.59, 0.16, width, 0.025])
    ax_resume_bat = fig.add_axes([0.59, 0.12, width, 0.025])
    ax_brightness = fig.add_axes([0.59, 0.08, width, 0.025])

    slider_battery = Slider(
        ax_battery,
        "Battery (Wh)",
        100,
        5000,
        valinit=options.max_battery_wh,
        valstep=10,
    )
    slider_solar = Slider(
        ax_solar, "Solar (W)", 0, 600, valinit=options.solar_w, valstep=5
    )
    slider_max_charge = Slider(
        ax_max_charge, "Max charge (W)", 0, 400, valinit=init_max_charge, valstep=5
    )
    slider_min_bat = Slider(
        ax_min_bat, "Min battery (%)", 1, 99, valinit=init_min_bat, valstep=1
    )
    slider_resume_bat = Slider(
        ax_resume_bat, "Resume battery (%)", 1, 99, valinit=init_resume_bat, valstep=1
    )
    slider_brightness = Slider(
        ax_brightness,
        "Brightness (%)",
        1,
        150,
        valinit=max(1.0, init_brightness),
        valstep=1,
    )

    def make_options_from_sliders() -> Options | None:
        battery = slider_battery.val
        min_bat = slider_min_bat.val
        resume_bat = slider_resume_bat.val
        # Guard the min < resume invariant
        if min_bat >= resume_bat:
            return None
        project_w = (DEFAULT_W - IDLE_W) * slider_brightness.val / 100 + IDLE_W
        return Options(
            solar_w=slider_solar.val,
            max_battery_wh=battery,
            off_battery_wh=battery * min_bat / 100,
            resume_battery_wh=battery * resume_bat / 100,
            project_w=project_w,
            std_dev=options.std_dev,
            start_day=options.start_day,
            day_charge_hour=options.day_charge_hour,
            day_charge_minute=options.day_charge_minute,
            day_charge_until_hour=options.day_charge_until_hour,
            day_charge_until_minute=options.day_charge_until_minute,
            always_day_charge=options.always_day_charge,
            max_charge_w=slider_max_charge.val if slider_max_charge.val > 0 else None,
        )

    def update(_) -> None:
        new_options = make_options_from_sliders()
        if new_options is None:
            ax.set_title("Invalid: min battery must be less than resume battery")
            fig.canvas.draw_idle()
            return
        new_bwm, new_toggles, new_ann, _, new_total, new_start_hour = (
            run_simulation_data(new_options)
        )
        ax.cla()
        draw_plot(
            ax, new_options, new_bwm, new_toggles, new_ann, new_total, new_start_hour
        )
        fig.canvas.draw_idle()

    for slider in [
        slider_battery,
        slider_solar,
        slider_max_charge,
        slider_min_bat,
        slider_resume_bat,
        slider_brightness,
    ]:
        slider.on_changed(update)

    plt.show()


# These numbers came from testing 5 LED strips. I measured 2.478A when responding to music, and
# 1.614A when idling.
# TODO: I think the test I ran used the whole strip, but the dome only turns on 80% of the strip,
# and the rest are idle. I should reduce those these measurements.
DEFAULT_A_PER_STRIP = 2.478 / 5
IDLE_A_PER_STRIP = 1.614 / 5
DEFAULT_W = DEFAULT_A_PER_STRIP * 15 * 12
IDLE_W = IDLE_A_PER_STRIP * 15 * 12


def make_parser() -> ArgumentParser:
    """Makes a parser."""
    parser = ArgumentParser(
        prog="power_sim", formatter_class=ArgumentDefaultsHelpFormatter
    )
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
        default=300 * 0.9,
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

    if namespace.day_charge is not None and not re.match(
        r"\d{1,2}:\d{2}", namespace.day_charge
    ):
        print_error(
            f"Bad day charge time: {namespace.day_charge}, should be e.g. 13:00"
        )
    if namespace.day_charge is not None:
        day_charge_hour = int(namespace.day_charge.split(":")[0])
        day_charge_minute = int(namespace.day_charge.split(":")[1])
    else:
        day_charge_hour = None
        day_charge_minute = None

    if namespace.day_charge_until is not None and not re.match(
        r"\d{1,2}:\d{2}", namespace.day_charge_until
    ):
        print_error(
            f"Bad day charge until time: {namespace.day_charge_until}, should be e.g. 13:00"
        )
    if namespace.day_charge_until is not None:
        day_charge_until_hour = int(namespace.day_charge_until.split(":")[0])
        day_charge_until_minute = int(namespace.day_charge_until.split(":")[1])
    else:
        day_charge_until_hour = None
        day_charge_until_minute = None

    if day_charge_hour is not None and day_charge_until_hour is not None:
        if day_charge_hour > day_charge_until_hour or (
            day_charge_hour == day_charge_until_hour
            and day_charge_minute >= day_charge_until_minute
        ):
            print_error(
                f"Day charge ({namespace.day_charge}) must be before until charge ({namespace.day_charge_until})"
            )

    if namespace.always_day_charge and namespace.day_charge_until is None:
        print_error(f"always-day-charge can only be used with day-charge-until")

    if namespace.min_battery < 1 or namespace.min_battery > 100:
        print_error(
            f"Bad battery percentage: {namespace.min_battery}, should be 1 < % < 100"
        )
    if namespace.resume_battery < 1 or namespace.resume_battery > 100:
        print_error(
            f"Bad battery percentage: {namespace.resume_battery}, should be 1 < % < 100"
        )
    if namespace.project_w and namespace.project_w < IDLE_W:
        # Not an error, but we should print a warning
        sys.stderr.write(
            f"Warning: project W {namespace.project_w:0.2f} is unrealistically below idle W {IDLE_W:0.2f}"
        )
    if namespace.project_w is not None and namespace.brightness != 100:
        print_error("Can only specify one of project-w and brightness")
    if namespace.min_battery >= namespace.resume_battery:
        print_error(
            f"Resume battery ({namespace.resume_battery}) needs to be less than min battery ({namespace.min_battery})"
        )
    if day_charge_hour is not None and not (6 <= day_charge_hour <= 15):
        print_error(
            f"Day charge is {namespace.day_charge} but should be between 06:00 and 16:00"
        )
    if day_charge_minute is not None and not (0 <= day_charge_minute <= 59):
        print_error(
            f"Day charge minute is {day_charge_minute} but should be in [0..59]"
        )
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
    sun_hours = sum((get_sunlight_percentage(i, 0, options.std_dev) for i in range(24)))
    print(f"simulated sun hours: {sun_hours:.2f} (max in CO is {max_solar_hours})")
    if sun_hours > max_solar_hours:
        sys.stderr.write(
            "*** Warning! Your std-dev is too high and gives unrealistically high solar hours ***\n"
        )
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
