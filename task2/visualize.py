#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
Скрипт визуализации для лабораторной работы:
Параллельный алгоритм Препараты-Хонга (OpenMP)

Генерирует:
1. convex_hull_demo.png - демонстрация корректности построения выпуклой оболочки (точки + оболочка CCW)
2. plot_time_vs_threads.png - графики времени выполнения T(p) от числа потоков
3. plot_speedup.png - графики ускорения S(p) = T(1) / T(p)
4. plot_efficiency.png - графики эффективности E(p) = S(p) / p
5. plot_algorithms_comparison.png - сравнение быстродействия трех алгоритмов от N
6. plot_distributions.png - зависимость времени от распределения входных данных (квадрат, круг, гаусс)
"""

import sys
import os
import subprocess
import csv
import math
import numpy as np
import matplotlib.pyplot as plt
import matplotlib.patches as patches

# Настройка шрифтов и стиля графиков
plt.rcParams['font.sans-serif'] = ['DejaVu Sans', 'Arial', 'Helvetica']
plt.rcParams['axes.edgecolor'] = '#333333'
plt.rcParams['axes.linewidth'] = 1.0
plt.rcParams['grid.alpha'] = 0.4
plt.rcParams['grid.linestyle'] = '--'

OUTPUT_DIR = os.path.dirname(os.path.abspath(__file__))

def generate_convex_hull_visualization(binary_path):
    """
    Генерирует изображение выпуклой оболочки для демонстрации корректности.
    """
    print("Генерация визуализации корректности выпуклой оболочки...")
    np.random.seed(42)
    # Генерируем 60 случайных точек
    pts_x = np.random.uniform(-40, 40, 60)
    pts_y = np.random.uniform(-40, 40, 60)
    
    # Запускаем бинарник для построения выпуклой оболочки
    input_data = f"{len(pts_x)}\n" + "\n".join(f"{x:.4f} {y:.4f}" for x, y in zip(pts_x, pts_y)) + "\n"
    proc = subprocess.run([binary_path, "4"], input=input_data, text=True, capture_output=True)
    lines = proc.stdout.strip().split("\n")
    k = int(lines[0])
    hull = []
    for line in lines[1:]:
        x, y = map(float, line.split())
        hull.append((x, y))
    
    # Строим график
    fig, ax = plt.subplots(figsize=(9, 8), dpi=300)
    
    # Отображаем исходные внутренние точки
    ax.scatter(pts_x, pts_y, color="#3498db", s=50, alpha=0.7, edgecolors="none", label="Исходные точки (N = 60)", zorder=3)
    
    # Замыкаем контур оболочки для отрисовки
    hull_x = [p[0] for p in hull] + [hull[0][0]]
    hull_y = [p[1] for p in hull] + [hull[0][1]]
    
    # Заливка выпуклой оболочки
    ax.fill(hull_x, hull_y, color="#2ecc71", alpha=0.15, label="Выпуклая оболочка (Conv)", zorder=2)
    
    # Линии ребер выпуклой оболочки
    ax.plot(hull_x, hull_y, color="#27ae60", linewidth=2.5, linestyle="-", label="Ребра оболочки (обход CCW)", zorder=4)
    
    # Вершины выпуклой оболочки
    ax.scatter([p[0] for p in hull], [p[1] for p in hull], color="#e74c3c", s=100, edgecolors="#c0392b", linewidth=1.5,
               label=f"Вершины оболочки (K = {k})", zorder=5)
    
    # Стрелки направления обхода против часовой стрелки
    for i in range(len(hull)):
        p1 = hull[i]
        p2 = hull[(i + 1) % len(hull)]
        dx = p2[0] - p1[0]
        dy = p2[1] - p1[1]
        mid_x = p1[0] + dx * 0.5
        mid_y = p1[1] + dy * 0.5
        ax.annotate('', xy=(p1[0] + dx * 0.55, p1[1] + dy * 0.55),
                    xytext=(mid_x, mid_y),
                    arrowprops=dict(arrowstyle="->", color="#e67e22", lw=2.0),
                    zorder=6)
        # Номера вершин
        ax.text(p1[0] * 1.07, p1[1] * 1.07, f"{i+1}", fontsize=11, fontweight='bold', color="#8e44ad", zorder=7)

    ax.set_title("Параллельный алгоритм Препараты-Хонга: Выпуклая оболочка\n"
                 "(Обход вершин против часовой стрелки, CCW)", fontsize=13, fontweight='bold', pad=15)
    ax.set_xlabel("Координата X", fontsize=11)
    ax.set_ylabel("Координата Y", fontsize=11)
    ax.grid(True, linestyle="--", alpha=0.5)
    ax.legend(loc="upper right", framealpha=0.9, fontsize=10)
    
    out_file = os.path.join(OUTPUT_DIR, "convex_hull_demo.png")
    fig.tight_layout()
    fig.savefig(out_file)
    plt.close(fig)
    print(f"Сохранено: {out_file}")

def load_benchmark_data(csv_file):
    """
    Загружает данные бенчмарка из CSV файла.
    """
    data = []
    with open(csv_file, 'r', encoding='utf-8') as f:
        reader = csv.DictReader(f)
        for row in reader:
            data.append({
                'algorithm': row['algorithm'],
                'n': int(row['n']),
                'threads': int(row['threads']),
                'distribution': row['distribution'],
                'time_ms': float(row['time_ms']),
                'hull_size': int(row['hull_size'])
            })
    return data

def plot_time_speedup_efficiency(data):
    """
    Строит графики времени выполнения, ускорения и эффективности для OpenMP.
    """
    print("Генерация графиков T(p), S(p), E(p)...")
    
    # Фильтруем данные только для PreparataHong_OpenMP с распределением square
    omp_data = [d for d in data if d['algorithm'] == 'PreparataHong_OpenMP' and d['distribution'] == 'square']
    
    # Уникальные значения N
    n_values = sorted(list(set(d['n'] for d in omp_data)))
    
    # 1. График времени выполнения T(p)
    fig, ax = plt.subplots(figsize=(10, 6), dpi=300)
    colors = plt.cm.viridis(np.linspace(0.1, 0.9, len(n_values)))
    
    for n_val, color in zip(n_values, colors):
        subset = [d for d in omp_data if d['n'] == n_val]
        subset = sorted(subset, key=lambda x: x['threads'])
        threads = [d['threads'] for d in subset]
        times = [d['time_ms'] for d in subset]
        ax.plot(threads, times, marker='o', linewidth=2, markersize=6, color=color, label=f"N = {n_val:,}")
        
    ax.set_title("Время выполнения параллельного алгоритма Препараты-Хонга $T(p)$\n(Распределение: квадрат)",
                 fontsize=13, fontweight='bold', pad=15)
    ax.set_xlabel("Число потоков OpenMP ($p$)", fontsize=11)
    ax.set_ylabel("Время выполнения (мс, лог. шкала)", fontsize=11)
    ax.set_yscale('log')
    ax.set_xticks(sorted(list(set(d['threads'] for d in omp_data))))
    ax.grid(True, which="both", linestyle="--", alpha=0.5)
    ax.legend(title="Размер выборки N", loc="upper right")
    fig.tight_layout()
    time_file = os.path.join(OUTPUT_DIR, "plot_time_vs_threads.png")
    fig.savefig(time_file)
    plt.close(fig)
    print(f"Сохранено: {time_file}")

    # 2. График ускорения S(p) = T(1) / T(p)
    fig, ax = plt.subplots(figsize=(10, 6), dpi=300)
    large_n_values = [n for n in n_values if n >= 100000] # Берем значимые объемы
    colors = plt.cm.tab10(np.linspace(0, 1, len(large_n_values)))
    
    all_threads = sorted(list(set(d['threads'] for d in omp_data)))
    # Идеальное ускорение
    ax.plot(all_threads, all_threads, 'k--', linewidth=2, label="Линейное (идеальное) ускорение $S=p$")
    
    for n_val, color in zip(large_n_values, colors):
        subset = [d for d in omp_data if d['n'] == n_val]
        subset = sorted(subset, key=lambda x: x['threads'])
        t1 = [d['time_ms'] for d in subset if d['threads'] == 1][0]
        threads = [d['threads'] for d in subset]
        speedup = [t1 / d['time_ms'] for d in subset]
        ax.plot(threads, speedup, marker='s', linewidth=2, markersize=7, color=color, label=f"N = {n_val:,}")
        
    ax.set_title("Ускорение параллельного алгоритма Препараты-Хонга $S(p) = T(1) / T(p)$\n(Распределение: квадрат)",
                 fontsize=13, fontweight='bold', pad=15)
    ax.set_xlabel("Число потоков OpenMP ($p$)", fontsize=11)
    ax.set_ylabel("Ускорение $S(p)$", fontsize=11)
    ax.set_xticks(all_threads)
    ax.grid(True, linestyle="--", alpha=0.5)
    ax.legend(loc="upper left")
    fig.tight_layout()
    speedup_file = os.path.join(OUTPUT_DIR, "plot_speedup.png")
    fig.savefig(speedup_file)
    plt.close(fig)
    print(f"Сохранено: {speedup_file}")

    # 3. График эффективности E(p) = S(p) / p
    fig, ax = plt.subplots(figsize=(10, 6), dpi=300)
    ax.axhline(1.0, color='k', linestyle='--', linewidth=2, label="Идеальная эффективность $E=1.0$ (100%)")
    
    for n_val, color in zip(large_n_values, colors):
        subset = [d for d in omp_data if d['n'] == n_val]
        subset = sorted(subset, key=lambda x: x['threads'])
        t1 = [d['time_ms'] for d in subset if d['threads'] == 1][0]
        threads = [d['threads'] for d in subset]
        efficiency = [(t1 / d['time_ms']) / d['threads'] for d in subset]
        ax.plot(threads, efficiency, marker='^', linewidth=2, markersize=7, color=color, label=f"N = {n_val:,}")
        
    ax.set_title("Эффективность параллелизации $E(p) = S(p) / p$\n(Распределение: квадрат)",
                 fontsize=13, fontweight='bold', pad=15)
    ax.set_xlabel("Число потоков OpenMP ($p$)", fontsize=11)
    ax.set_ylabel("Эффективность $E(p)$", fontsize=11)
    ax.set_xticks(all_threads)
    ax.set_ylim(0.0, 1.3)
    ax.grid(True, linestyle="--", alpha=0.5)
    ax.legend(loc="lower left")
    fig.tight_layout()
    eff_file = os.path.join(OUTPUT_DIR, "plot_efficiency.png")
    fig.savefig(eff_file)
    plt.close(fig)
    print(f"Сохранено: {eff_file}")

def plot_algorithms_comparison(data):
    """
    Строит график сравнения трех алгоритмов:
    1. Эндрю (Monotone Chain)
    2. Последовательный Препарата-Хонг
    3. Параллельный Препарата-Хонг (OpenMP)
    """
    print("Генерация графика сравнения 3 алгоритмов...")
    square_data = [d for d in data if d['distribution'] == 'square']
    
    n_values = sorted(list(set(d['n'] for d in square_data)))
    
    andrew_times = []
    seq_ph_times = []
    par_ph_times = []
    
    # Для параллельного берем лучший результат (например, max потоков = 8 или 10)
    max_th = max(d['threads'] for d in square_data if d['algorithm'] == 'PreparataHong_OpenMP')
    
    for n in n_values:
        a_t = [d['time_ms'] for d in square_data if d['algorithm'] == 'Andrew_MonotoneChain' and d['n'] == n][0]
        s_t = [d['time_ms'] for d in square_data if d['algorithm'] == 'PreparataHong_Sequential' and d['n'] == n][0]
        p_t = [d['time_ms'] for d in square_data if d['algorithm'] == 'PreparataHong_OpenMP' and d['n'] == n and d['threads'] == max_th][0]
        andrew_times.append(a_t)
        seq_ph_times.append(s_t)
        par_ph_times.append(p_t)
        
    fig, ax = plt.subplots(figsize=(10, 6), dpi=300)
    
    ax.plot(n_values, andrew_times, marker='o', linewidth=2.5, markersize=7, color="#e74c3c", label="[1] Алгоритм Эндрю (Monotone Chain)")
    ax.plot(n_values, seq_ph_times, marker='s', linewidth=2.5, markersize=7, color="#3498db", label="[2] Препарата-Хонг (последовательный)")
    ax.plot(n_values, par_ph_times, marker='^', linewidth=2.5, markersize=7, color="#2ecc71", label=f"[3] Препарата-Хонг (OpenMP, {max_th} потоков)")
    
    ax.set_title("Сравнение быстродействия трех алгоритмов построения выпуклой оболочки", fontsize=13, fontweight='bold', pad=15)
    ax.set_xlabel("Количество входных точек $N$", fontsize=11)
    ax.set_ylabel("Время выполнения (мс)", fontsize=11)
    ax.set_xscale('log')
    ax.set_yscale('log')
    ax.grid(True, which="both", linestyle="--", alpha=0.5)
    ax.legend(fontsize=10)
    
    fig.tight_layout()
    out_file = os.path.join(OUTPUT_DIR, "plot_algorithms_comparison.png")
    fig.savefig(out_file)
    plt.close(fig)
    print(f"Сохранено: {out_file}")

def plot_distributions_comparison(data):
    """
    Сравнивает время выполнения и размер оболочки в зависимости от распределения данных (квадрат, круг, гаусс).
    """
    print("Генерация графика зависимости от распределения данных...")
    
    fig, (ax1, ax2) = plt.subplots(1, 2, figsize=(14, 6), dpi=300)
    
    dist_names = {"square": "Квадрат", "circle": "Круг", "gaussian": "Гаусс (нормальное)"}
    colors = {"square": "#3498db", "circle": "#e67e22", "gaussian": "#9b59b6"}
    
    max_th = max(d['threads'] for d in data if d['algorithm'] == 'PreparataHong_OpenMP')
    n_values = sorted(list(set(d['n'] for d in data)))
    
    for dist in ["square", "circle", "gaussian"]:
        times = []
        sizes = []
        for n in n_values:
            matches = [d for d in data if d['algorithm'] == 'PreparataHong_OpenMP' and d['distribution'] == dist and d['n'] == n and d['threads'] == max_th]
            if matches:
                times.append(matches[0]['time_ms'])
                sizes.append(matches[0]['hull_size'])
            else:
                times.append(None)
                sizes.append(None)
                
        ax1.plot(n_values, times, marker='o', linewidth=2, color=colors[dist], label=dist_names[dist])
        ax2.plot(n_values, sizes, marker='s', linewidth=2, color=colors[dist], label=dist_names[dist])
        
    ax1.set_title("Время работы OpenMP Препараты-Хонга от типа данных", fontsize=11, fontweight='bold')
    ax1.set_xlabel("Размер $N$", fontsize=10)
    ax1.set_ylabel("Время выполнения (мс)", fontsize=10)
    ax1.set_xscale('log')
    ax1.set_yscale('log')
    ax1.grid(True, which="both", linestyle="--", alpha=0.5)
    ax1.legend()
    
    ax2.set_title("Число вершин оболочки $K$ от распределения данных", fontsize=11, fontweight='bold')
    ax2.set_xlabel("Размер $N$", fontsize=10)
    ax2.set_ylabel("Число вершин выпуклой оболочки $K$", fontsize=10)
    ax2.set_xscale('log')
    ax2.grid(True, which="both", linestyle="--", alpha=0.5)
    ax2.legend()
    
    fig.tight_layout()
    out_file = os.path.join(OUTPUT_DIR, "plot_distributions.png")
    fig.savefig(out_file)
    plt.close(fig)
    print(f"Сохранено: {out_file}")

def main():
    binary_path = os.path.join(OUTPUT_DIR, "preparata_hong")
    csv_file = os.path.join(OUTPUT_DIR, "benchmark_results.csv")
    
    if os.path.exists(binary_path):
        generate_convex_hull_visualization(binary_path)
    else:
        print(f"Предупреждение: бинарник {binary_path} не найден для демо оболочки.")
        
    if os.path.exists(csv_file):
        data = load_benchmark_data(csv_file)
        plot_time_speedup_efficiency(data)
        plot_algorithms_comparison(data)
        plot_distributions_comparison(data)
    else:
        print(f"Ошибка: файл результатов {csv_file} не найден!")

if __name__ == "__main__":
    main()
