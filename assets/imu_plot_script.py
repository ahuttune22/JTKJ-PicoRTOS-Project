"""
AI generated plot of the IMU measurements (assets copy, not part of the hat_app build).

Reads imu_measurements.txt (the recordings made with imu_data_collection.c),
draws acc_x against acc_z for the six position recordings, shades the areas
where our rule gives a dot or a dash, and writes imu_plot.png. It also checks
that the rule from main.c gives the right symbol for every sample.

"""
import re
from pathlib import Path

import matplotlib
matplotlib.use("Agg")                      # draw to a file, no window needed
import matplotlib.pyplot as plt
from matplotlib.patches import Rectangle

HERE = Path(__file__).resolve().parent
DATA_FILE = HERE / "imu_measurements.txt"
PLOT_FILE = HERE / "imu_plot.png"

# Same thresholds as include/morse_config.h (in g).
FLAT_Z_MIN = 0.7
FLAT_X_MAX = 0.5
TILT_X_MIN = 0.7

# Recording number in the data file -> (label, expected symbol, colour, marker)
RECORDINGS = {
    1: ("Flat, still",           ".", "tab:blue",   "o"),
    2: ("Flat, wobbling",        ".", "tab:blue",   "^"),
    3: ("Tilted right, still",   "-", "#d55e00",    "o"),
    4: ("Tilted right, wobbling", "-", "#d55e00",   "^"),
    5: ("Tilted left, still",    "-", "#7b3294",    "o"),
    6: ("Tilted left, wobbling", "-", "#7b3294",    "^"),
}

ACCEL_LINE = re.compile(r"Accel: X=([-\d.]+), Y=([-\d.]+), Z=([-\d.]+)")
SECTION = re.compile(r"^--- (\d+)\. ")


def classify(ax, az):
    """Same rule as classify_position() in main.c."""
    if az > FLAT_Z_MIN and abs(ax) < FLAT_X_MAX:
        return "."
    if abs(ax) > TILT_X_MIN:
        return "-"
    return None


def read_recordings(path):
    """Return {recording number: [(ax, az), ...]}."""
    recordings = {}
    current = None
    for line in path.read_text(encoding="utf-8").splitlines():
        section = SECTION.match(line)
        if section:
            current = int(section.group(1))
            recordings[current] = []
            continue
        sample = ACCEL_LINE.search(line)
        if sample and current is not None:
            ax, _ay, az = (float(v) for v in sample.groups())
            recordings[current].append((ax, az))
    return recordings


def main():
    recordings = read_recordings(DATA_FILE)

    fig, axis = plt.subplots(figsize=(8.5, 5.7))

    # Shaded areas where the rule gives a dot (blue) or a dash (orange).
    axis.add_patch(Rectangle((-FLAT_X_MAX, FLAT_Z_MIN), 2 * FLAT_X_MAX, 1.2 - FLAT_Z_MIN,
                             color="tab:blue", alpha=0.1))
    axis.add_patch(Rectangle((TILT_X_MIN, -0.4), 1.2 - TILT_X_MIN, 1.6,
                             color="#d55e00", alpha=0.1))
    axis.add_patch(Rectangle((-1.2, -0.4), 1.2 - TILT_X_MIN, 1.6,
                             color="#d55e00", alpha=0.1))

    wrong = 0
    total = 0
    for number, (label, expected, color, marker) in RECORDINGS.items():
        samples = recordings.get(number, [])
        axis.scatter([s[0] for s in samples], [s[1] for s in samples],
                     s=18, color=color, marker=marker, alpha=0.7,
                     label=f"{label} ({len(samples)})")
        for ax_value, az_value in samples:
            total += 1
            if classify(ax_value, az_value) != expected:
                wrong += 1

    axis.set_xlim(-1.2, 1.2)
    axis.set_ylim(-0.4, 1.2)
    axis.set_xlabel("acc_x (g)")
    axis.set_ylabel("acc_z (g)")
    axis.set_title(f"{total} IMU samples vs thresholds "
                   "(blue area = dot, orange areas = dash)")
    axis.grid(alpha=0.3)
    axis.legend(loc="center", fontsize=8)
    fig.tight_layout()
    fig.savefig(PLOT_FILE, dpi=125)

    print(f"{total} samples, {wrong} classified wrong. Plot saved to {PLOT_FILE.name}")


if __name__ == "__main__":
    main()
