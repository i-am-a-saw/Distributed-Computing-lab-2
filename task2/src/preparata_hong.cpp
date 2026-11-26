#include "preparata_hong.h"
#include "geometry.h"

#include <algorithm>
#include <vector>
#include <cstddef>
#include <cmath>

constexpr std::size_t PARALLEL_THRESHOLD = 4096;

// --------------------------------------------------
// Геометрические вспомогательные функции
// --------------------------------------------------

// Векторное произведение AB и AC
long double crossValue(
    const Point& a,
    const Point& b,
    const Point& c
) {
    long double abx = static_cast<long double>(b.x) - a.x;
    long double aby = static_cast<long double>(b.y) - a.y;
    long double acx = static_cast<long double>(c.x) - a.x;
    long double acy = static_cast<long double>(c.y) - a.y;

    return abx * acy - aby * acx;
}

// Допуск для сравнения геометрических величин
long double tolerance(
    const Point& a,
    const Point& b,
    const Point& c
) {
    long double abx = static_cast<long double>(b.x) - a.x;
    long double aby = static_cast<long double>(b.y) - a.y;
    long double acx = static_cast<long double>(c.x) - a.x;
    long double acy = static_cast<long double>(c.y) - a.y;

    long double scale =
        std::abs(abx * acy) + std::abs(aby * acx);

    return 1e-18L * std::max(1.0L, scale);
}

// Определение знака векторного произведения
int crossSign(
    const Point& a,
    const Point& b,
    const Point& c
) {
    long double value = crossValue(a, b, c);
    long double eps = tolerance(a, b, c);

    if (value > eps) return 1;
    if (value < -eps) return -1;

    return 0;
}

// Квадрат расстояния между точками
long double distanceSquared(
    const Point& a,
    const Point& b
) {
    long double dx = static_cast<long double>(a.x) - b.x;
    long double dy = static_cast<long double>(a.y) - b.y;

    return dx * dx + dy * dy;
}

// --------------------------------------------------
// Построение оболочки для вырожденных случаев
// --------------------------------------------------

std::vector<Point> buildSmallHull(std::vector<Point> points) {

    std::sort(points.begin(), points.end(), compareByX);

    points.erase(
        std::unique(
            points.begin(),
            points.end(),
            [](const Point& a, const Point& b) {
                return a.x == b.x && a.y == b.y;
            }
        ),
        points.end()
    );

    if (points.size() <= 2) {
        return points;
    }

    std::vector<Point> lower;
    std::vector<Point> upper;

    for (const Point& p : points) {

        while (lower.size() >= 2 &&
               crossSign(
                   lower[lower.size() - 2],
                   lower.back(),
                   p
               ) <= 0) {

            lower.pop_back();
        }

        lower.push_back(p);
    }

    for (auto it = points.rbegin(); it != points.rend(); ++it) {

        while (upper.size() >= 2 &&
               crossSign(
                   upper[upper.size() - 2],
                   upper.back(),
                   *it
               ) <= 0) {

            upper.pop_back();
        }

        upper.push_back(*it);
    }

    lower.pop_back();
    upper.pop_back();

    lower.insert(
        lower.end(),
        upper.begin(),
        upper.end()
    );

    return lower;
}

// --------------------------------------------------
// Выбор начальных вершин для поиска касательных
// --------------------------------------------------

std::size_t findExtremeVertex(
    const std::vector<Point>& hull,
    bool rightmost,
    bool upper
) {
    std::size_t index = 0;

    for (std::size_t i = 1; i < hull.size(); ++i) {

        bool betterX = rightmost
            ? hull[i].x > hull[index].x
            : hull[i].x < hull[index].x;

        bool sameX = hull[i].x == hull[index].x;

        bool betterY = upper
            ? hull[i].y > hull[index].y
            : hull[i].y < hull[index].y;

        if (betterX || (sameX && betterY)) {
            index = i;
        }
    }

    return index;
}

// --------------------------------------------------
// Перемещение вершины при поиске общей касательной
// --------------------------------------------------

bool moveTangentVertex(
    const std::vector<Point>& hull,
    std::size_t& index,
    const Point& opposite,
    bool isLeftHull,
    bool upper
) {
    if (hull.size() < 3) {
        return false;
    }

    std::size_t next = (index + 1) % hull.size();
    std::size_t prev = (index + hull.size() - 1) % hull.size();

    const Point& current = hull[index];

    // Проверяем соседние вершины относительно прямой,
    // проходящей через текущие вершины двух оболочек.

    long double nextCross;
    long double prevCross;

    if (isLeftHull) {

        nextCross = crossValue(current, opposite, hull[next]);
        prevCross = crossValue(current, opposite, hull[prev]);

    } else {

        nextCross = crossValue(opposite, current, hull[next]);
        prevCross = crossValue(opposite, current, hull[prev]);
    }

    // Выбираем соседа, который сильнее нарушает условие касательной.
    bool chooseNext;

    if (upper) {
        chooseNext = nextCross > prevCross;
    } else {
        chooseNext = nextCross < prevCross;
    }

    long double bestCross = chooseNext ? nextCross : prevCross;

    std::size_t candidate = chooseNext ? next : prev;

    const Point& candidatePoint = hull[candidate];

    // Если сосед находится с неправильной стороны прямой,
    // перемещаем вершину к нему.
    int sign = crossSign(
        isLeftHull ? current : opposite,
        isLeftHull ? opposite : current,
        candidatePoint
    );

    if (upper && sign > 0) {
        index = candidate;
        return true;
    }

    if (!upper && sign < 0) {
        index = candidate;
        return true;
    }

    // Если сосед лежит на самой прямой, выбираем более удалённую
    // вершину. Это важно при касании целого ребра оболочки.
    if (sign == 0) {

        long double currentDistance =
            distanceSquared(current, opposite);

        long double candidateDistance =
            distanceSquared(candidatePoint, opposite);

        if (candidateDistance > currentDistance) {
            index = candidate;
            return true;
        }
    }

    return false;
}

// --------------------------------------------------
// Проверка найденной опорной прямой
// --------------------------------------------------

bool verifyTangent(
    const std::vector<Point>& left,
    const std::vector<Point>& right,
    std::size_t leftIndex,
    std::size_t rightIndex,
    bool upper
) {
    const Point& a = left[leftIndex];
    const Point& b = right[rightIndex];

    for (const Point& p : left) {

        int sign = crossSign(a, b, p);

        if (upper && sign > 0) return false;
        if (!upper && sign < 0) return false;
    }

    for (const Point& p : right) {

        int sign = crossSign(a, b, p);

        if (upper && sign > 0) return false;
        if (!upper && sign < 0) return false;
    }

    return true;
}

// --------------------------------------------------
// Поиск верхней или нижней общей касательной
// --------------------------------------------------

bool findTangent(
    const std::vector<Point>& left,
    const std::vector<Point>& right,
    std::size_t& leftIndex,
    std::size_t& rightIndex,
    bool upper
) {
    leftIndex = findExtremeVertex(left, true, upper);
    rightIndex = findExtremeVertex(right, false, upper);

    const std::size_t maxIterations =
        2 * (left.size() + right.size()) + 8;

    for (std::size_t iteration = 0;
         iteration < maxIterations;
         ++iteration) {

        bool changed = false;

        // Перемещение вершины левой оболочки
        for (std::size_t step = 0; step < left.size(); ++step) {

            if (!moveTangentVertex(
                    left,
                    leftIndex,
                    right[rightIndex],
                    true,
                    upper)) {
                break;
            }

            changed = true;
        }

        // Перемещение вершины правой оболочки
        for (std::size_t step = 0; step < right.size(); ++step) {

            if (!moveTangentVertex(
                    right,
                    rightIndex,
                    left[leftIndex],
                    false,
                    upper)) {
                break;
            }

            changed = true;
        }

        if (!changed) {

            return verifyTangent(
                left,
                right,
                leftIndex,
                rightIndex,
                upper
            );
        }
    }

    return false;
}

// --------------------------------------------------
// Добавление участка кольцевого списка
// --------------------------------------------------

void appendArc(
    const std::vector<Point>& hull,
    std::size_t start,
    std::size_t finish,
    std::vector<Point>& result
) {
    std::size_t index = start;

    while (true) {

        result.push_back(hull[index]);

        if (index == finish) {
            break;
        }

        index = (index + 1) % hull.size();
    }
}

// --------------------------------------------------
// Объединение двух выпуклых оболочек
// --------------------------------------------------

std::vector<Point> mergeHulls(
    const std::vector<Point>& left,
    const std::vector<Point>& right
) {
    if (left.empty()) return right;
    if (right.empty()) return left;

    // Если одна из оболочек является точкой или отрезком,
    // используем обработку вырожденного случая.
    if (left.size() < 3 || right.size() < 3) {

        std::vector<Point> points;

        points.reserve(left.size() + right.size());

        points.insert(points.end(), left.begin(), left.end());
        points.insert(points.end(), right.begin(), right.end());

        return buildSmallHull(points);
    }

    std::size_t leftTop, rightTop;
    std::size_t leftBottom, rightBottom;

    // Поиск верхней общей касательной
    bool upperFound = findTangent(
        left,
        right,
        leftTop,
        rightTop,
        true
    );

    // Поиск нижней общей касательной
    bool lowerFound = findTangent(
        left,
        right,
        leftBottom,
        rightBottom,
        false
    );

    // Защита от некорректного результата поиска касательных
    if (!upperFound || !lowerFound) {

        std::vector<Point> points;

        points.reserve(left.size() + right.size());

        points.insert(points.end(), left.begin(), left.end());
        points.insert(points.end(), right.begin(), right.end());

        return buildSmallHull(points);
    }

    std::vector<Point> result;

    result.reserve(left.size() + right.size());

    // Обход правой оболочки:
    // от нижней касательной к верхней
    appendArc(
        right,
        rightBottom,
        rightTop,
        result
    );

    // Обход левой оболочки:
    // от верхней касательной к нижней
    appendArc(
        left,
        leftTop,
        leftBottom,
        result
    );

    return result;
}

// --------------------------------------------------
// Рекурсивное построение выпуклой оболочки
// --------------------------------------------------

std::vector<Point> buildHull(
    const std::vector<Point>& points,
    std::size_t begin,
    std::size_t end
) {
    const std::size_t n = end - begin;

    if (n == 1) {
        return {points[begin]};
    }

    if (n == 2) {
        return {
            points[begin],
            points[begin + 1]
        };
    }

    // Разделение множества на две равные части
    const std::size_t mid = begin + n / 2;

    std::vector<Point> leftHull;
    std::vector<Point> rightHull;

    // Параллельное построение двух оболочек
    if (n >= PARALLEL_THRESHOLD) {

        #pragma omp task shared(leftHull) firstprivate(begin, mid)
        {
            leftHull = buildHull(points, begin, mid);
        }

        #pragma omp task shared(rightHull) firstprivate(mid, end)
        {
            rightHull = buildHull(points, mid, end);
        }

        #pragma omp taskwait

    } else {

        leftHull = buildHull(points, begin, mid);
        rightHull = buildHull(points, mid, end);
    }

    return mergeHulls(leftHull, rightHull);
}

// --------------------------------------------------
// Основная функция алгоритма Препараты—Хонга
// --------------------------------------------------

std::vector<Point> preparataHong(
    const std::vector<Point>& points
) {
    if (points.empty()) {
        return {};
    }

    if (points.size() <= 2) {
        return points;
    }

    // Сортировка исходных точек по X и Y
    std::vector<Point> sortedPoints = points;

    std::sort(
        sortedPoints.begin(),
        sortedPoints.end(),
        compareByX
    );

    std::vector<Point> result;

    // Запуск параллельной области OpenMP
    #pragma omp parallel
    {
        #pragma omp single
        {
            result = buildHull(
                sortedPoints,
                0,
                sortedPoints.size()
            );
        }
    }

    return result;
}
