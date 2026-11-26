import random
import subprocess
from pathlib import Path


PROGRAM = Path(__file__).resolve().parents[1] / "build" / "convex_hull"


def cross(o, a, b):
    return ((a[0] - o[0]) * (b[1] - o[1])
            - (a[1] - o[1]) * (b[0] - o[0]))


def reference_hull(points):
    points = sorted(set(points))

    if len(points) <= 1:
        return points

    lower = []
    for p in points:
        while len(lower) >= 2 and cross(lower[-2], lower[-1], p) <= 0:
            lower.pop()
        lower.append(p)

    upper = []
    for p in reversed(points):
        while len(upper) >= 2 and cross(upper[-2], upper[-1], p) <= 0:
            upper.pop()
        upper.append(p)

    return lower[:-1] + upper[:-1]


def run_program(points):
    data = [str(len(points))]
    data.extend(f"{x} {y}" for x, y in points)

    result = subprocess.run(
        [str(PROGRAM)],
        input="\n".join(data) + "\n",
        text=True,
        capture_output=True,
        check=True,
        timeout=30
    )

    values = result.stdout.split()

    if not values:
        raise AssertionError("Программа ничего не вывела")

    count = int(values[0])

    if len(values) != 1 + 2 * count:
        raise AssertionError("Неверный формат вывода")

    hull = []
    for i in range(count):
        x = float(values[1 + 2 * i])
        y = float(values[2 + 2 * i])
        hull.append((x, y))

    return hull


def check_case(points, name):
    expected = reference_hull(points)
    actual = run_program(points)

    if len(actual) != len(set(actual)):
        raise AssertionError(f"{name}: в оболочке есть повторяющиеся вершины")

    if set(actual) != set(expected):
        raise AssertionError(
            f"{name}: неверная оболочка\n"
            f"Ожидалось: {expected}\n"
            f"Получено:  {actual}"
        )

    if len(actual) >= 3:
        area2 = sum(
            actual[i][0] * actual[(i + 1) % len(actual)][1]
            - actual[(i + 1) % len(actual)][0] * actual[i][1]
            for i in range(len(actual))
        )

        if area2 <= 0:
            raise AssertionError(f"{name}: обход не против часовой стрелки")

    print(f"[OK] {name}")


def main():
    if not PROGRAM.exists():
        raise FileNotFoundError(f"Не найден исполняемый файл: {PROGRAM}")

    # Фиксированные геометрические случаи
    cases = [
        ("Одна точка", [(0, 0)]),
        ("Две точки", [(0, 0), (5, 3)]),
        ("Треугольник", [(0, 0), (4, 0), (2, 3)]),
        ("Квадрат", [(0, 0), (0, 2), (2, 0), (2, 2), (1, 1)]),
        ("Все точки на одной прямой",
         [(0, 0), (1, 2), (2, 4), (3, 6), (4, 8)]),
        ("Одинаковые X",
         [(2, 0), (2, 1), (2, 3), (2, 5), (2, 8)]),
        ("Одинаковые Y",
         [(0, 3), (1, 3), (4, 3), (7, 3), (9, 3)]),
        ("Внутренние точки",
         [(0, 0), (0, 10), (10, 0), (10, 10),
          (5, 5), (3, 4), (7, 6), (2, 8)]),
    ]

    for name, points in cases:
        check_case(points, name)

    # Случайные наборы: точки целочисленные и уникальные
    random.seed(42)

    for size in [3, 5, 10, 50, 100, 500, 1000]:
        for test_number in range(20):
            points = set()

            while len(points) < size:
                points.add((
                    random.randint(-10000, 10000),
                    random.randint(-10000, 10000)
                ))

            check_case(
                list(points),
                f"Случайный набор N={size}, тест={test_number + 1}"
            )

    print("\nВсе тесты успешно пройдены.")


if __name__ == "__main__":
    main()