#ifndef PREPARATA_HONG_H
#define PREPARATA_HONG_H

#include <vector>
#include "point.h"

// Построение выпуклой оболочки алгоритмом Препараты—Хонга
// points - исходное множество точек
// Возвращает вершины выпуклой оболочки в порядке обхода против часовой стрелки
std::vector<Point> preparataHong(const std::vector<Point>& points);

#endif // PREPARATA_HONG_H