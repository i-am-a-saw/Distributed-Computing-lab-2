import csv
from pathlib import Path

import matplotlib.pyplot as plt


ROOT = Path(__file__).resolve().parents[1]
CSV_FILE = ROOT / "results" / "benchmark_results.csv"
RESULTS_DIR = ROOT / "results"

THREAD_COUNTS = [1, 2, 4, 8]


def load_results():
    with open(CSV_FILE, newline="") as file:
        return list(csv.DictReader(file))


def plot_metric(data, metric, ylabel, title, filename, ideal=False):
    sizes = sorted({int(row["points"]) for row in data})

    plt.figure(figsize=(10, 6))

    for threads in THREAD_COUNTS:
        rows = sorted(
            [
                row for row in data
                if int(row["threads"]) == threads
            ],
            key=lambda row: int(row["points"])
        )

        x = [int(row["points"]) for row in rows]
        y = [float(row[metric]) for row in rows]

        plt.plot(
            x, y,
            marker="o",
            linewidth=2,
            label=f"{threads} поток(а)"
        )

    if ideal:
        for threads in THREAD_COUNTS:
            plt.plot(
                sizes,
                [threads] * len(sizes),
                linestyle="--",
                alpha=0.4,
                label=f"Идеальное ускорение ({threads} потоков)"
            )

    plt.xlabel("Количество входных точек")
    plt.ylabel(ylabel)
    plt.title(title)
    plt.xscale("log")
    plt.xticks(sizes, [f"{n:,}" for n in sizes])
    plt.grid(True, alpha=0.3)
    plt.legend()
    plt.tight_layout()

    output = RESULTS_DIR / filename
    plt.savefig(output, dpi=200)
    plt.close()

    print(f"График сохранён: {output}")


def main():
    if not CSV_FILE.exists():
        raise FileNotFoundError(
            f"Не найден файл результатов: {CSV_FILE}"
        )

    data = load_results()

    plot_metric(
        data,
        "mean_seconds",
        "Время выполнения, секунды",
        "Зависимость времени выполнения от размера данных",
        "benchmark_time.png"
    )

    plot_metric(
        data,
        "speedup",
        "Ускорение",
        "Ускорение параллельного алгоритма",
        "benchmark_speedup.png",
        ideal=True
    )

    plot_metric(
        data,
        "efficiency",
        "Эффективность",
        "Эффективность параллелизации",
        "benchmark_efficiency.png"
    )

    print("\nПостроение графиков завершено!")


if __name__ == "__main__":
    main()
