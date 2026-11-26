import random
import subprocess
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
PROGRAM = ROOT / "build" / "convex_hull"
INPUT = ROOT / "results" / "parallel_input.txt"
THREAD_COUNTS = [1, 2, 4, 8]
N = 100_000


def generate_points():
    random.seed(42)

    INPUT.parent.mkdir(exist_ok=True)

    with open(INPUT, "w") as f:
        f.write(f"{N}\n")

        for _ in range(N):
            x = random.randint(-1_000_000, 1_000_000)
            y = random.randint(-1_000_000, 1_000_000)
            f.write(f"{x} {y}\n")


def run_program(threads):
    import os

    env = os.environ.copy()
    env["OMP_NUM_THREADS"] = str(threads)
    env["OMP_DYNAMIC"] = "FALSE"

    result = subprocess.run(
        [str(PROGRAM)],
        stdin=open(INPUT),
        capture_output=True,
        text=True,
        env=env,
        timeout=120,
        check=True
    )

    output = result.stdout.split()

    if not output:
        raise RuntimeError(f"Нет вывода при {threads} потоках")

    count = int(output[0])

    if len(output) != 1 + 2 * count:
        raise RuntimeError(f"Некорректный вывод при {threads} потоках")

    return output


def main():
    if not PROGRAM.exists():
        raise FileNotFoundError(f"Не найдена программа: {PROGRAM}")

    print(f"Генерация {N} точек...")
    generate_points()

    reference = None

    for threads in THREAD_COUNTS:
        print(f"\nЗапуск с {threads} потоками...")

        output = run_program(threads)
        hull_size = int(output[0])

        if reference is None:
            reference = output
            print(f"Эталонный результат: {hull_size} вершин")
        elif output == reference:
            print("[OK] Результат совпадает")
        else:
            raise AssertionError(
                f"Результаты отличаются при {threads} потоках!"
            )

    print("\nВсе проверки параллельного выполнения пройдены!")


if __name__ == "__main__":
    main()
