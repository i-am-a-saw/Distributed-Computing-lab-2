#include "geometry.h"

// Векторное произведение векторов AB и AC
double crossProduct(const Point& a, const Point& b, const Point& c) {
    return (b.x - a.x) * (c.y - a.y)
         - (b.y - a.y) * (c.x - a.x);
}

// Определение ориентации трёх точек
double orientation(const Point& a, const Point& b, const Point& c) {
    return crossProduct(a, b, c);
}

// Проверка, лежит ли точка p выше прямой ab
bool isAbove(const Point& a, const Point& b, const Point& p) {
    return crossProduct(a, b, p) > 0;
}

// Проверка, лежит ли точка p ниже прямой ab
bool isBelow(const Point& a, const Point& b, const Point& p) {
    return crossProduct(a, b, p) < 0;
}

// Сравнение точек по координате X
bool compareByX(const Point& a, const Point& b) {
    if (a.x != b.x) {
        return a.x < b.x;
    }

    return a.y < b.y;
}