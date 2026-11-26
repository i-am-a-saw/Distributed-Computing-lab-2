#include <iostream>
#include <vector>

#include "point.h"
#include "preparata_hong.h"

int main() {
    std::ios::sync_with_stdio(false); // Отключение синхронизации потоков ввода-вывод С++ с потоками С для ускорения работы cin/cout
    std::cin.tie(nullptr); // Отключение автоматической очистки буфера cout перед каждой операцией cin

    // Чтение количества точек
    std::size_t n;
    std::cin >> n;

    // Чтение координат
    std::vector<Point> points;
    points.reserve(n); // Выделение памяти

    for (std::size_t i = 0; i < n; ++i) {
        double x, y;
        std::cin >> x >> y;
        points.emplace_back(x, y);
    }

    // Построение выпуклой оболочки
    std::vector<Point> hull = preparataHong(points);

    // Вывод количества вершин
    std::cout << hull.size() << '\n';

    // Вывод координат вершин
    for (const Point& point : hull) {
        std::cout << point.x << ' '
                  << point.y << '\n';
    }

    return 0;
}