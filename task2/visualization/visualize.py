
import argparse
import subprocess
from pathlib import Path

import matplotlib.pyplot as plt


# Пути к проекту
ROOT = Path(__file__).resolve().parents[1]
PROGRAM = ROOT / "build" / "convex_hull"
RESULTS_DIR = ROOT / "results"


def read_points(input_file):
    """Читает исходные точки из файла формата N, затем координаты."""
    with open(input_file, "r", encoding="utf-8") as file:
        values = file.read().split()

    if not values:
        raise ValueError("Входной файл пуст")

    n = int(values[0])

    if n < 1:
        raise ValueError("Количество точек должно быть положительным")

    if len(values) != 1 + 2 * n:
        raise ValueError(
            f"Некорректный формат файла: ожидалось {n} точек"
        )

    points = []

    for i in range(n):
        x = float(values[1 + 2 * i])
        y = float(values[2 + 2 * i])
        points.append((x, y))

    return points


def calculate_hull(input_file):
    """Запускает C++-программу и получает выпуклую оболочку."""
    with open(input_file, "r", encoding="utf-8") as file:
        result = subprocess.run(
            [str(PROGRAM)],
            stdin=file,
            capture_output=True,
            text=True,
            check=True,
            timeout=120
        )

    values = result.stdout.split()

    if not values:
        raise RuntimeError("C++-программа не вернула результат")

    hull_size = int(values[0])

    if len(values) != 1 + 2 * hull_size:
        raise RuntimeError("Некорректный формат вывода C++-программы")

    hull = []

    for i in range(hull_size):
        x = float(values[1 + 2 * i])
        y = float(values[2 + 2 * i])
        hull.append((x, y))

    return hull


def visualize(points, hull, output_image):
    """Строит и сохраняет изображение выпуклой оболочки."""
    x_points = [point[0] for point in points]
    y_points = [point[1] for point in points]

    plt.figure(figsize=(10, 8))

    # Все исходные точки
    plt.scatter(
        x_points,
        y_points,
        s=15,
        alpha=0.5,
        label="Исходные точки"
    )

    # Вершины выпуклой оболочки
    if hull:
        x_hull = [point[0] for point in hull]
        y_hull = [point[1] for point in hull]

        plt.scatter(
            x_hull,
            y_hull,
            s=45,
            label="Вершины оболочки"
        )

        # Граница оболочки
        if len(hull) >= 3:
            x_hull.append(x_hull[0])
            y_hull.append(y_hull[0])

            plt.plot(
                x_hull,
                y_hull,
                linewidth=2,
                label="Граница выпуклой оболочки"
            )

        elif len(hull) == 2:
            plt.plot(
                x_hull,
                y_hull,
                linewidth=2,
                label="Выпуклая оболочка"
            )

    plt.title(
        f"Convex Hull — Preparata–Hong Algorithm\n"
        f"Points: {len(points)}, Hull vertices: {len(hull)}"
    )

    plt.xlabel("X")
    plt.ylabel("Y")
    plt.grid(True, alpha=0.3)
    plt.axis("equal")
    plt.legend()
    plt.tight_layout()

    output_image.parent.mkdir(parents=True, exist_ok=True)
    plt.savefig(output_image, dpi=200)

    print(f"Изображение сохранено: {output_image}")

    plt.show()


def main():
    parser = argparse.ArgumentParser(
        description="Визуализация выпуклой оболочки Preparata–Hong"
    )

    parser.add_argument(
        "input",
        type=Path,
        help="Путь к файлу с исходными точками"
    )

    parser.add_argument(
        "-o",
        "--output",
        type=Path,
        help="Путь для сохранения изображения"
    )

    args = parser.parse_args()

    input_file = args.input.resolve()

    if not input_file.exists():
        raise FileNotFoundError(
            f"Не найден входной файл: {input_file}"
        )

    if not PROGRAM.exists():
        raise FileNotFoundError(
            f"Не найдена C++-программа: {PROGRAM}\n"
            "Сначала собери проект через CMake."
        )

    # Если путь для изображения не указан,
    # сохраняем его в results с именем входного файла.
    if args.output:
        output_image = args.output.resolve()
    else:
        output_image = RESULTS_DIR / f"{input_file.stem}_hull.png"

    print(f"Чтение точек из файла: {input_file}")
    points = read_points(input_file)

    print(f"Количество исходных точек: {len(points)}")
    print("Вычисление выпуклой оболочки...")

    hull = calculate_hull(input_file)

    print(f"Количество вершин оболочки: {len(hull)}")

    visualize(points, hull, output_image)


if __name__ == "__main__":
    main()
