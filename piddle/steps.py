"""Generates the frequency range buckets for notes."""
import sys

def main():
    if "-h" in sys.argv or len(sys.argv) != 3:
        print(f"Usage: {sys.argv[0]} <bucket_count> <frequency>")
        sys.exit()

    bucket_count = int(sys.argv[1])
    if bucket_count not in (2**n for n in range(20)):
        print(f"bucket_count ({bucket_count}) must be a power of 2")
        sys.exit()
    frequency = int(sys.argv[2])

    step_size = frequency / bucket_count
    lower = 0
    empty = []
    skips = []
    for index in range(bucket_count):
        # Find which notes
        upper = lower + step_size
        found = []
        for note in notes:
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
    print(f"// Generated from python3 steps.py {bucket_count} {frequency}")
    print(f"static const int SAMPLE_COUNT = {bucket_count};")
    print("static constexpr uint16_t NOTE_TO_OUTPUT_INDEX[] = {")
    print(f"  {', '.join((str(s) for s in skips))}")
    print("};")
    print(f"const int c4Index = {skips.index(c4)};")

# From https://pages.mtu.edu/~suits/notefreqs.html
notes = (
    # Skip these because they are so low
    #(16.35, "C0"),
    #(18.35, "D0"),
    #(20.60, "E0"),
    #(21.83, "F0"),
    #(24.50, "G0"),
    #(27.50, "A0"),
    #(30.87, "B0"),
    #(32.70, "C1"),
    #(36.71, "D1"),
    (41.20, "E1"),  # This is the lowest note with a 4-string bass guitar
    (43.65, "F1"),
    (49.00, "G1"),
    (55.00, "A1"),
    (61.74, "B1"),
    (65.41, "C2"),
    (73.42, "D2"),
    (82.41, "E2"),
    (87.31, "F2"),
    (98.00, "G2"),
    (110.00, "A2"),
    (123.47, "B2"),
    (130.81, "C3"),
    (146.83, "D3"),
    (164.81, "E3"),
    (174.61, "F3"),
    (196.00, "G3"),
    (220.00, "A3"),
    (246.94, "B3"),
    (261.63, "C4"),
    (293.66, "D4"),
    (329.63, "E4"),
    (349.23, "F4"),
    (392.00, "G4"),
    (440.00, "A4"),
    (493.88, "B4"),
    (523.25, "C5"),
    (587.33, "D5"),
    (659.25, "E5"),
    (698.46, "F5"),
    (783.99, "G5"),
    (880.00, "A5"),
    (987.77, "B5"),
    (1046.50, "C6"),
    (1174.66, "D6"),  # This is the highest a trumpet goes
    (1318.51, "E6"),
    (1396.91, "F6"),
    (1567.98, "G6"),
    (1760.00, "A6"),
    (1975.53, "B6"),
    (2093.00, "C7"),  # This is the highest a flute goes
    (2349.32, "D7"),
    (2637.02, "E7"),
    (2793.83, "F7"),
    (3135.96, "G7"),
    (3520.00, "A7"),
    (3951.07, "B7"),
    #(4186.01, "C8"),
    #(4698.63, "D8"),
    #(5274.04, "E8"),
    #(5587.65, "F8"),
    #(6271.93, "G8"),
    #(7040.00, "A8"),
    #(7902.13, "B8"),
)

if __name__ == "__main__":
    main()
