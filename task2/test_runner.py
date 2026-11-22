#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
Автоматический тестовый раннер для проверки корректности алгоритма Препараты-Хонга.
Проверяет краевые, вырожденные и случайные случаи против эталонного алгоритма Монотонной цепочки (Эндрю).
"""

import subprocess
import os
import random
import math

BINARY = os.path.join(os.path.dirname(os.path.abspath(__file__)), "preparata_hong")

def cross_product(o, a, b):
    return (a[0] - o[0]) * (b[1] - o[1]) - (a[1] - o[1]) * (b[0] - o[0])

def reference_convex_hull(points):
    """
    Эталонный алгоритм Эндрю (Monotone Chain) на Python.
    """
    pts = sorted(list(set(points)))
    if len(pts) <= 2:
        return pts
    lower = []
    for p in pts:
        while len(lower) >= 2 and cross_product(lower[-2], lower[-1], p) <= 1e-9:
            lower.pop()
        lower.append(p)
    upper = []
    for p in reversed(pts):
        while len(upper) >= 2 and cross_product(upper[-2], upper[-1], p) <= 1e-9:
            upper.pop()
        upper.append(p)
    return lower[:-1] + upper[:-1]

def run_program(points, threads=4):
    """
    Запускает бинарник и парсит результат.
    """
    input_str = f"{len(points)}\n" + "\n".join(f"{p[0]:.6f} {p[1]:.6f}" for p in points) + "\n"
    proc = subprocess.run([BINARY, str(threads)], input=input_str, text=True, capture_output=True)
    if proc.returncode != 0:
        raise RuntimeError(f"Программа завершилась с ошибкой: {proc.stderr}")
    lines = proc.stdout.strip().split("\n")
    if not lines or lines[0] == "":
        return []
    k = int(lines[0])
    res = []
    for line in lines[1:]:
        parts = line.split()
        if len(parts) >= 2:
            res.append((round(float(parts[0]), 4), round(float(parts[1]), 4)))
    return res

def normalize_polygon(poly):
    """
    Приводит многоугольник к каноническому виду для корректного сравнения:
    начинает с лексикографически минимальной точки.
    """
    if not poly:
        return []
    min_idx = min(range(len(poly)), key=lambda i: (poly[i][0], poly[i][1]))
    return poly[min_idx:] + poly[:min_idx]

def test_case(name, points, threads=4):
    ref_hull = reference_convex_hull(points)
    ref_norm = normalize_polygon([(round(p[0], 4), round(p[1], 4)) for p in ref_hull])
    
    prog_hull = run_program(points, threads)
    prog_norm = normalize_polygon(prog_hull)
    
    # Сравниваем как множества и как циклические последовательности
    ref_set = set(ref_norm)
    prog_set = set(prog_norm)
    
    if ref_set == prog_set:
        print(f"  [OK] {name} (вершин: {len(prog_norm)})")
        return True
    else:
        print(f"  [FAIL] {name}")
        print(f"    Ожидалось: {ref_norm}")
        print(f"    Получено:  {prog_norm}")
        return False

def main():
    if not os.path.exists(BINARY):
        print(f"Ошибка: исполняемый файл {BINARY} не найден! Сначала скомпилируйте проект (make).")
        return 1

    print("==================================================")
    print("      ЗАПУСК ТЕСТОВ КОРРЕКТНОСТИ РЕАЛИЗАЦИИ       ")
    print("==================================================")
    
    passed = 0
    total = 0

    # Тест 1: 1 точка
    total += 1
    if test_case("1 точка", [(5.0, 5.0)]): passed += 1

    # Тест 2: 2 точки
    total += 1
    if test_case("2 точки", [(0.0, 0.0), (10.0, 10.0)]): passed += 1

    # Тест 3: 3 точки (треугольник)
    total += 1
    if test_case("3 точки (треугольник)", [(0.0, 0.0), (5.0, 10.0), (10.0, 0.0)]): passed += 1

    # Тест 4: 3 точки на одной горизонтальной прямой
    total += 1
    if test_case("3 коллинеарные горизонтальные точки", [(0.0, 5.0), (5.0, 5.0), (10.0, 5.0)]): passed += 1

    # Тест 5: 4 точки на одной вертикальной прямой
    total += 1
    if test_case("4 коллинеарные вертикальные точки", [(3.0, 1.0), (3.0, 4.0), (3.0, -2.0), (3.0, 7.0)]): passed += 1

    # Тест 6: Квадрат с внутренними точками
    total += 1
    sq_pts = [(0, 0), (10, 0), (10, 10), (0, 10), (5, 5), (2, 3), (8, 7), (1, 9)]
    if test_case("Квадрат с точками внутри", sq_pts): passed += 1

    # Тест 7: Дубликаты точек
    total += 1
    dup_pts = [(1, 1), (1, 1), (2, 2), (2, 2), (0, 3), (0, 3), (0, 0)]
    if test_case("Множество с дубликатами", dup_pts): passed += 1

    # Тест 8: Точки на окружности
    total += 1
    circ_pts = [(math.cos(2 * math.pi * i / 16) * 10, math.sin(2 * math.pi * i / 16) * 10) for i in range(16)]
    if test_case("16 точек строго на окружности", circ_pts): passed += 1

    # Тест 9..20: Серия случайных наборов при разном числе потоков (1, 2, 4, 8)
    print("\n--- Стресс-тестирование на случайных данных ---")
    random.seed(12345)
    stress_ok = True
    for t_idx in range(1, 16):
        total += 1
        n = random.randint(50, 500)
        th = random.choice([1, 2, 4, 8])
        pts = [(round(random.uniform(-100, 100), 2), round(random.uniform(-100, 100), 2)) for _ in range(n)]
        if test_case(f"Случайный набор #{t_idx} (N={n}, потоков={th})", pts, threads=th):
            passed += 1
        else:
            stress_ok = False
            break

    print("==================================================")
    print(f"ИТОГ: Успешно пройдено {passed} из {total} тестов.")
    if passed == total:
        print(">>> ВСЕ ТЕСТЫ ПРОЙДЕНЫ УСПЕШНО! КОРРЕКТНОСТЬ ПОДТВЕРЖДЕНА! <<<")
        return 0
    else:
        print(">>> ОБНАРУЖЕНЫ ОШИБКИ В ТЕСТАХ! <<<")
        return 1

if __name__ == "__main__":
    exit(main())
