"""Generates the frequency range buckets for notes."""
import argparse
import sys

def main():
    parser = argparse.ArgumentParser(
        prog="Steps",
        description="Generate note steps",
    )
    parser.add_argument("bucket_count", type=int)
    parser.add_argument("sample_frequency", type=float)
    parser.add_argument("-v", "--verbose", action="store_true")
    parser.add_argument("-s", "--sharps", action="store_true", help="include half step notes")
    args = parser.parse_args()

    bucket_count = args.bucket_count
    if bucket_count not in (2**n for n in range(20)):
        print(f"bucket_count ({bucket_count}) must be a power of 2")
        sys.exit()
    frequency = args.sample_frequency

    step_size = frequency / bucket_count
    lower = 0
    empty = []
    skips = []
    for index in range(bucket_count):
        # Find which notes
        upper = lower + step_size
        found = []
        for note in notes:
            if not args.sharps and "#" in note[1]:
                continue
            if lower <= note[0] <= upper:
                found.append(note[1])

        if upper < notes[0][0]:
            print(f"Bucket {index} has no notes")

        if len(found) == 0:
            empty.append(index)
        else:
            if len(empty) > 0:
                #print(f"Buckets {' '.join((str(b) for b in empty))} were empty")
                empty = []
            skips.append(index)
            print(f"Bucket {index} {lower:0.1f}-{upper:0.1f} has {' '.join(found)}")
            if "C4" in found:
                c4 = index

        lower = upper
    print("Remaining buckets are empty because they're above note range")
    print()
    print(f"// Generated from python3 steps.py {bucket_count} {frequency} {'-s' if args.sharps else ''}")
    print(f"static const int SAMPLE_COUNT = {bucket_count};")
    print("static constexpr uint16_t NOTE_TO_OUTPUT_INDEX[] = {")
    print(f"  {', '.join((str(s) for s in skips))}")
    print("};")
    print(f"const int c4Index = {skips.index(c4)};")

# From https://pages.mtu.edu/~suits/notefreqs.html
notes = (
    # Skip these because they are so low
    #(16.35, "C0"),
    #(17.32, "C#0/Db0"),
    #(18.35, "D0"),
    #(19.45, "D#0/Eb0"),
    #(20.60, "E0"),
    #(21.83, "F0"),
    #(23.12, "F#0/Gb0"),
    #(24.50, "G0"),
    #(25.96, "G#0/Ab0"),
    #(27.50, "A0"),
    #(29.14, "A#0/Bb0"),
    #(30.87, "B0"),
    #(32.70, "C1"),
    #(34.65, "C#1/Db1"),
    #(36.71, "D1"),
    #(38.89, "D#1/Eb1"),
    (41.20, "E1"),  # This is the lowest note with a 4-string bass guitar
    (43.65, "F1"),
    (46.25, "F#1/Gb1"),
    (49.00, "G1"),
    (51.91, "G#1/Ab1"),
    (55.00, "A1"),
    (58.27, "A#1/Bb1"),
    (61.74, "B1"),
    (65.41, "C2"),
    (69.30, "C#2/Db2"),
    (73.42, "D2"),
    (77.78, "D#2/Eb2"),
    (82.41, "E2"),
    (87.31, "F2"),
    (92.50, "F#2/Gb2"),
    (98.00, "G2"),
    (103.83, "G#2/Ab2"),
    (110.00, "A2"),
    (116.54, "A#2/Bb2"),
    (123.47, "B2"),
    (130.81, "C3"),
    (138.59, "C#3/Db3"),
    (146.83, "D3"),
    (155.56, "D#3/Eb3"),
    (164.81, "E3"),
    (174.61, "F3"),
    (185.00, "F#3/Gb3"),
    (196.00, "G3"),
    (207.65, "G#3/Ab3"),
    (220.00, "A3"),
    (233.08, "A#3/Bb3"),
    (246.94, "B3"),
    (261.63, "C4"),
    (277.18, "C#4/Db4"),
    (293.66, "D4"),
    (311.13, "D#4/Eb4"),
    (329.63, "E4"),
    (349.23, "F4"),
    (369.99, "F#4/Gb4"),
    (392.00, "G4"),
    (415.30, "G#4/Ab4"),
    (440.00, "A4"),
    (466.16, "A#4/Bb4"),
    (493.88, "B4"),
    (523.25, "C5"),
    (554.37, "C#5/Db5"),
    (587.33, "D5"),
    (622.25, "D#5/Eb5"),
    (659.25, "E5"),
    (698.46, "F5"),
    (739.99, "F#5/Gb5"),
    (783.99, "G5"),
    (830.61, "G#5/Ab5"),
    (880.00, "A5"),
    (932.33, "A#5/Bb5"),
    (987.77, "B5"),
    (1046.50, "C6"),
    (1108.73, "C#6/Db6"),
    (1174.66, "D6"),  # This is the highest a trumpet goes
    (1244.51, "D#6/Eb6"),
    (1318.51, "E6"),
    (1396.91, "F6"),
    (1479.98, "F#6/Gb6"),
    (1567.98, "G6"),
    (1661.22, "G#6/Ab6"),
    (1760.00, "A6"),
    (1864.66, "A#6/Bb6"),
    (1975.53, "B6"),
    (2093.00, "C7"),  # This is the highest a flute goes
    (2217.46, "C#7/Db7"),
    (2349.32, "D7"),
    (2489.02, "D#7/Eb7"),
    (2637.02, "E7"),
    (2793.83, "F7"),
    (2959.96, "F#7/Gb7"),
    (3135.96, "G7"),
    (3322.44, "G#7/Ab7"),
    (3520.00, "A7"),
    (3729.31, "A#7/Bb7"),
    (3951.07, "B7"),
    #(4186.01, "C8"),
    #(4434.92, "C#8/Db8"),
    #(4698.63, "D8"),
    #(4978.03, "D#8/Eb8"),
    #(5274.04, "E8"),
    #(5587.65, "F8"),
    #(5919.91, "F#8/Gb8"),
    #(6271.93, "G8"),
    #(6644.88, "G#8/Ab8"),
    #(7040.00, "A8"),
    #(7458.62, "A#8/Bb8"),
    #(7902.13, "B8"),
)

if __name__ == "__main__":
    main()
