import csv
import os
import random
import statistics
import subprocess
import time
from pathlib import Path


# Пути к файлам проекта
ROOT = Path(__file__).resolve().parents[1]
PROGRAM = ROOT / "build" / "convex_hull"
RESULTS_DIR = ROOT / "results"

INPUT_FILE = RESULTS_DIR / "benchmark_input.txt"
CSV_FILE = RESULTS_DIR / "benchmark_results.csv"

# Параметры тестирования
SIZES = [10_000, 50_000, 100_000, 500_000, 1_000_000]
THREAD_COUNTS = [1, 2, 4, 8]
REPEATS = 3
SEED = 42


def generate_points(n, seed):
    """Генерирует случайные точки и записывает их в файл."""
    rng = random.Random(seed)

    with open(INPUT_FILE, "w") as file:
        file.write(f"{n}\n")

        for _ in range(n):
            x = rng.randint(-1_000_000, 1_000_000)
            y = rng.randint(-1_000_000, 1_000_000)
            file.write(f"{x} {y}\n")


def run_program(threads):
    """Запускает C++-программу и измеряет время выполнения."""
    env = os.environ.copy()
    env["OMP_NUM_THREADS"] = str(threads)
    env["OMP_DYNAMIC"] = "FALSE"

    start_time = time.perf_counter()

    with open(INPUT_FILE, "r") as input_file:
        subprocess.run(
            [str(PROGRAM)],
            stdin=input_file,
            stdout=subprocess.DEVNULL,
            stderr=subprocess.PIPE,
            text=True,
            env=env,
            timeout=600,
            check=True
        )

    return time.perf_counter() - start_time


def benchmark():
    """Проводит измерения для разных размеров данных и потоков."""
    all_results = []

    for n in SIZES:
        print(f"\n{'=' * 50}")
        print(f"Количество точек: {n:,}")
        print(f"{'=' * 50}")

        print("Генерация входных данных...")
        generate_points(n, SEED + n)

        times = {}

        for threads in THREAD_COUNTS:
            measurements = []

            for repeat in range(REPEATS):
                print(
                    f"Потоков: {threads} | "
                    f"Запуск: {repeat + 1}/{REPEATS}",
                    end=" | ",
                    flush=True
                )

                elapsed = run_program(threads)
                measurements.append(elapsed)

                print(f"{elapsed:.4f} сек.")

            times[threads] = measurements

        # Среднее время выполнения при одном потоке
        baseline = statistics.mean(times[1])

        for threads in THREAD_COUNTS:
            mean_time = statistics.mean(times[threads])
            min_time = min(times[threads])

            speedup = baseline / mean_time
            efficiency = speedup / threads

            all_results.append({
                "points": n,
                "threads": threads,
                "mean_seconds": round(mean_time, 6),
                "min_seconds": round(min_time, 6),
                "speedup": round(speedup, 4),
                "efficiency": round(efficiency, 4),
                "repeats": REPEATS
            })

        print("\nРезультаты для N =", f"{n:,}")
        print(f"{'Потоки':<10}{'Среднее время':<20}{'Ускорение':<15}")

        for threads in THREAD_COUNTS:
            row = next(
                r for r in all_results
                if r["points"] == n and r["threads"] == threads
            )

            print(
                f"{threads:<10}"
                f"{row['mean_seconds']:<20.4f}"
                f"{row['speedup']:<15.2f}"
            )

    return all_results


def save_results(results):
    """Сохраняет результаты измерений в CSV."""
    fieldnames = [
        "points",
        "threads",
        "mean_seconds",
        "min_seconds",
        "speedup",
        "efficiency",
        "repeats"
    ]

    with open(CSV_FILE, "w", newline="") as file:
        writer = csv.DictWriter(file, fieldnames=fieldnames)
        writer.writeheader()
        writer.writerows(results)

    print(f"\nРезультаты сохранены: {CSV_FILE}")


def main():
    if not PROGRAM.exists():
        raise FileNotFoundError(
            f"Не найдена программа: {PROGRAM}\n"
            "Сначала собери проект через CMake."
        )

    RESULTS_DIR.mkdir(parents=True, exist_ok=True)

    print("ТЕСТИРОВАНИЕ ПРОИЗВОДИТЕЛЬНОСТИ")
    print(f"Размеры наборов: {SIZES}")
    print(f"Количество потоков: {THREAD_COUNTS}")
    print(f"Повторов на комбинацию: {REPEATS}")

    results = benchmark()
    save_results(results)

    print("\nТестирование завершено!")


if __name__ == "__main__":
    main()
