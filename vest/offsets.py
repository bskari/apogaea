# Generates C code for the array offsets
# The generated offsets
led_counts = [6, 7, 10, 11, 12, 12, 6, 6, 7, 11, 11, 11, 11, 11]
array = []
count = 0
for x in range(len(led_counts)):
    array.append([])
    for y in range(led_counts[x]):
        array[-1].append(str(count))
        count += 1
    if len(array) % 2 == 0:
        array[-1] = list(reversed(array[-1]))
    for y in range(led_counts[x], max(led_counts)):
        array[-1].append("UNUSED_LED")

for x in range(len(led_counts) - 1, -1, -1):
    array.append([])
    for y in range(led_counts[x]):
        array[-1].append(str(count))
        count += 1
    if len(array) % 2 == 0:
        array[-1] = list(reversed(array[-1]))
    for y in range(led_counts[x], max(led_counts)):
        array[-1].append("UNUSED_LED")

print("const int UNUSED_LED = -1;")
print(f"const int LED_COUNT = {sum(led_counts) * 2};")
print(f"const int LED_COLUMN_COUNT = 2 * {len(led_counts)};")
print(f"const int LED_ROW_COUNT = {max(led_counts)};")
print("// x first then y, starting at lower left corner")
print(f"const int16_t LED_STRIPS[2 * {len(led_counts)}][{max(led_counts)}] = {{")
for x in range(len(array)):
    items = [array[x][y] for y in range(len(array[x]))]
    print(f"    {{{', '.join(items)}}},")
print("};")