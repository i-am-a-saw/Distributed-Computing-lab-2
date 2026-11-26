#ifndef GEOMETRY_H
#define GEOMETRY_H

#include "point.h"

// Векторное произведение векторов AB и AC
double crossProduct(const Point& a, const Point& b, const Point& c);

// Определение ориентации трёх точек:
// > 0 — против часовой стрелки
// < 0 — по часовой стрелке
// = 0 — точки лежат на одной прямой
double orientation(const Point& a, const Point& b, const Point& c);

// Проверка, лежит ли точка p выше прямой ab
bool isAbove(const Point& a, const Point& b, const Point& p);

// Проверка, лежит ли точка p ниже прямой ab
bool isBelow(const Point& a, const Point& b, const Point& p);

// Сравнение точек по координате X
bool compareByX(const Point& a, const Point& b);

#endif // GEOMETRY_H