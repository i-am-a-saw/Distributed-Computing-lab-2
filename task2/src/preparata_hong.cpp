#include "preparata_hong.h"
#include "geometry.h"

#include <algorithm>
#include <vector>
#include <cstddef>
#include <cmath>

constexpr std::size_t PARALLEL_THRESHOLD = 4096; // порог параллелизации

// 1 Геометрические вспомогательные функции

// Векторное произведение AB и AC
long double crossValue(
    const Point& a,
    const Point& b,
    const Point& c
) {
    // Вычисляем координаты векторов AB и AC и преобразуем координаты в long double для повышения точности вычислений
    long double abx = static_cast<long double>(b.x) - a.x;
    long double aby = static_cast<long double>(b.y) - a.y;

    long double acx = static_cast<long double>(c.x) - a.x;
    long double acy = static_cast<long double>(c.y) - a.y;

    return abx * acy - aby * acx;
}

// Вычисляет допустимую погрешность при сравнении векторного произведения
long double tolerance(
    const Point& a,
    const Point& b,
    const Point& c
) {
    // Снова вычисляем координаты векторов AB и AC
    long double abx = static_cast<long double>(b.x) - a.x;
    long double aby = static_cast<long double>(b.y) - a.y;

    long double acx = static_cast<long double>(c.x) - a.x;
    long double acy = static_cast<long double>(c.y) - a.y;

    // Оцениваем масштаб вычисляемого векторного произведения, используем сумму модулей двух произведений
    long double scale =
        std::abs(abx * acy) + std::abs(aby * acx);
    // Возвращаем относительный допуск - если масштаб меньше единицы, используем минимальный масштаб 1
    return 1e-18L * std::max(1.0L, scale);
}

// Определение знака векторного произведения с учетом погрешности
int crossSign(
    const Point& a,
    const Point& b,
    const Point& c
) {
    // Получаем значение векторного произведения
    long double value = crossValue(a, b, c);
    // Вычисляем допустимую погрешность
    long double eps = tolerance(a, b, c);
    // Если значение больше погрешности, то знак положительный
    if (value > eps) return 1;
    // Если значение меньше отрицательной погрешности, то знак отрицательный.
    if (value < -eps) return -1;
    // В остальных случаях считаем значение равным нулю
    return 0;
}

// Квадрат расстояния между двумя точками
long double distanceSquared(
    const Point& a,
    const Point& b
) {
    // Разность координат двух точек
    long double dx = static_cast<long double>(a.x) - b.x;
    long double dy = static_cast<long double>(a.y) - b.y;
    // Квадрат расстояния по теореме Пифагора
    return dx * dx + dy * dy;
}

// 2 Построение оболочки для вырожденных случаев
// Строит выпуклую оболочку методом монотонной цепи. Является резервным вариантом для небольших наборов точек, если поиск общих касательных не дал корректного результата
std::vector<Point> buildSmallHull(std::vector<Point> points) {
    // Сортируем точки сначала по X, затем по Y
    std::sort(points.begin(), points.end(), compareByX);

    // Удаляем повторяющиеся точки - unique перемещает дубликаты в конец, erase удаляет их
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

    // Если точек не больше двух, они уже образуют выпуклую оболочку
    if (points.size() <= 2) {
        return points;
    }

    // Векторы для нижней и верхней частей оболочки
    std::vector<Point> lower;
    std::vector<Point> upper;

    // Строим нижнюю часть оболочки
    for (const Point& p : points) {

        // Пока последние три точки не образуют нужный поворот против часовой стрелки, удаляем последнюю точку. Условие <= 0 также удаляет промежуточные коллинеарные точки
        while (lower.size() >= 2 &&
               crossSign(
                   lower[lower.size() - 2],
                   lower.back(),
                   p
               ) <= 0) {

            // Удаляем точку, которая не должна входить в оболочку
            lower.pop_back();
        }
        // Добавляем текущую точку в нижнюю часть оболочки
        lower.push_back(p);
    }

    // Строим верхнюю часть оболочки. Для этого проходим отсортированные точки в обратном порядке
    for (auto it = points.rbegin(); it != points.rend(); ++it) {

        // Проверяем направление поворота последних трёх точек
        while (upper.size() >= 2 &&
               crossSign(
                   upper[upper.size() - 2],
                   upper.back(),
                   *it
               ) <= 0) {

            // Удаляем точку, не принадлежащую верхней границе
            upper.pop_back();
        }
        // Добавляем текущую точку в верхнюю часть оболочки
        upper.push_back(*it);
    }

    // Удаляем последние точки обеих частей, поскольку они повторяются в противоположной части оболочки.
    lower.pop_back();
    upper.pop_back();

    // Объединяем нижнюю и верхнюю части в один контур
    lower.insert(
        lower.end(),
        upper.begin(),
        upper.end()
    );

    // Возвращаем итоговую выпуклую оболочку
    return lower;
}

// 3 Выбор начальных вершин для поиска касательных, т.е. другими словами самой левой нижней/верхней для правой оболочки и наборот для левой

std::size_t findExtremeVertex(
    const std::vector<Point>& hull,
    bool rightmost, // самая правая или самая левая
    bool upper
) {
    std::size_t index = 0;

    for (std::size_t i = 1; i < hull.size(); ++i) {

        bool betterX = rightmost
            ? hull[i].x > hull[index].x
            : hull[i].x < hull[index].x;

        bool sameX = hull[i].x == hull[index].x;

        // если X совпадают, сравниваем Y
        bool betterY = upper
            ? hull[i].y > hull[index].y
            : hull[i].y < hull[index].y;

        if (betterX || (sameX && betterY)) {
            index = i;
        }
    }

    return index;
}

// 4 Перемещение вершины при поиске общей касательной

bool moveTangentVertex(
    const std::vector<Point>& hull,
    std::size_t& index, // текущая вершина
    const Point& opposite, // зафиксированная точка касания на другой оболочке
    bool isLeftHull, // левая или правая оболочка
    bool upper // верхняя или нижняя касательная
) {
    if (hull.size() < 3) {
        return false;
    }

    std::size_t next = (index + 1) % hull.size(); // следующая вершина
    std::size_t prev = (index + hull.size() - 1) % hull.size(); // предыдущая вершина

    const Point& current = hull[index];

    // Проверяем соседние вершины относительно прямой, проходящей через текущие вершины двух оболочек.

    long double nextCross;
    long double prevCross;

    // проверяем насколько сильно торчат соседние точки через знаковое векторное произведение
    if (isLeftHull) {

        nextCross = crossValue(current, opposite, hull[next]);
        prevCross = crossValue(current, opposite, hull[prev]);

    } else {

        nextCross = crossValue(opposite, current, hull[next]);
        prevCross = crossValue(opposite, current, hull[prev]);
    }

    // Выбираем соседа, который сильнее нарушает условие касательной (сильнее выпирает)
    bool chooseNext;

    if (upper) {
        chooseNext = nextCross > prevCross;
    } else {
        chooseNext = nextCross < prevCross;
    }

    long double bestCross = chooseNext ? nextCross : prevCross;

    std::size_t candidate = chooseNext ? next : prev; 

    const Point& candidatePoint = hull[candidate]; // выбранный сосед

    // Если сосед находится с неправильной стороны прямой и реально торчит, перемещаем вершину к нему
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

    // Если сосед лежит на самой прямой, выбираем более удалённую вершину. Это важно при касании целого ребра оболочки
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

// 5 Проверка найденной опорной прямой / верификация как я сам ее назвал

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

        int sign = crossSign(a, b, p); // проверка через векторное произведение

        if (upper && sign > 0) return false; // над прямой есть точка/точки
        if (!upper && sign < 0) return false; // под прямой есть точка/точки
        // эти оба случая говорят о том, что прямая - не касательная
    }

    // такая же проверка в правой оболочке
    for (const Point& p : right) {

        int sign = crossSign(a, b, p);

        if (upper && sign > 0) return false;
        if (!upper && sign < 0) return false;
    }

    return true;
}

// 6 Поиск верхней или нижней общей касательной
// Ищет общую касательную к двум выпуклым оболочкам
// В результате записывает индексы точек касания. Возвращает true, если касательная найдена и проверка пройдена
bool findTangent(
    const std::vector<Point>& left,
    const std::vector<Point>& right,
    std::size_t& leftIndex,
    std::size_t& rightIndex,
    bool upper // true - ищем верхнюю, false - нижнюю
) {
    // Начинаем поиск с крайних вершин двух оболочек
    leftIndex = findExtremeVertex(left, true, upper); // ищет самую правую и самую нижнюю/верхнюю точку в левой оболочке
    rightIndex = findExtremeVertex(right, false, upper); // ищет самую левую и самую нижнюю/верхнюю точку в левой оболочке

    // Ограничиваем число итераций, чтобы избежать бесконечного цикла
    const std::size_t maxIterations =
        2 * (left.size() + right.size()) + 8; // тут с запасом

    // Повторяем перемещение вершин до стабилизации или достижения лимита
    for (std::size_t iteration = 0; iteration < maxIterations; ++iteration) {
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

        // если нет сдвигов, значит касательная стабильна и мы можем ее верифицировать
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

// 7 Добавление участка кольцевого списка
// Добавляет в результат дугу от вершины start до finish. Оболочка рассматривается как циклический список вершин

void appendArc(
    const std::vector<Point>& hull,
    std::size_t start,
    std::size_t finish,
    std::vector<Point>& result
) {
    // Начинаем обход с заданной вершины
    std::size_t index = start;

    while (true) { // бесконечный цикл с выходом через break
        // Добавляем текущую вершину в итоговую оболочку
        result.push_back(hull[index]);

        // Если достигли конечной вершины, завершаем обход
        if (index == finish) {
            break;
        }
        // Переходим к следующей вершине. Операция % обеспечивает переход от последней вершины к самой первой (чтоб не выйти за пределы диапазона)
        index = (index + 1) % hull.size();
    }
}

// 8 Объединение двух выпуклых оболочек

// Объединяет две оболочки, построенные для левой и правой частей исходного множества точек
std::vector<Point> mergeHulls(
    const std::vector<Point>& left,
    const std::vector<Point>& right
) {
    // Если левая оболочка пустая, возвращаем правую и наоборот
    if (left.empty()) return right;
    if (right.empty()) return left;

    // Если одна из оболочек является точкой или отрезком, используем последовательный алгоритм
    if (left.size() < 3 || right.size() < 3) {

        std::vector<Point> points;

        points.reserve(left.size() + right.size());

        points.insert(points.end(), left.begin(), left.end());
        points.insert(points.end(), right.begin(), right.end());

        // Строим оболочку объединённого набора точек
        return buildSmallHull(points);
    }

    // Переменные для индексов точек касания верхней касательной
    std::size_t leftTop, rightTop;
    // Переменные для индексов точек касания нижней касательной
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

    // Защита от некорректного результата поиска касательных (если хотя бы одна касательная не найдена) и снова последовательный алгоритм
    if (!upperFound || !lowerFound) {

        std::vector<Point> points;

        points.reserve(left.size() + right.size());

        points.insert(points.end(), left.begin(), left.end());
        points.insert(points.end(), right.begin(), right.end());

        return buildSmallHull(points);
    }

    // Создаём вектор для итоговой объединённой оболочки
    std::vector<Point> result;

    // Предварительно резервируем память под вершины обеих оболочек
    result.reserve(left.size() + right.size());

    // Добавляем дугу правой оболочки от точки нижней касательной до точки верхней касательной
    appendArc(
        right,
        rightBottom,
        rightTop,
        result
    );

    // Добавляем дугу левой аналогично
    appendArc(
        left,
        leftTop,
        leftBottom,
        result
    );

    return result;
}

// 9 Рекурсивное построение выпуклой оболочки

// Рекурсивно строит выпуклую оболочку для диапазона точек от begin включительно до end не включительно
std::vector<Point> buildHull(
    const std::vector<Point>& points,
    std::size_t begin,
    std::size_t end
) {
    // Вычисляем количество точек в текущем диапазоне
    const std::size_t n = end - begin;

    // Базовый случай, когда оболочка одной точки состоит из этой точки
    if (n == 1) {
        return {points[begin]};
    }

    // Базовый случай, когда оболочка двух точек это отрезок между ними
    if (n == 2) {
        return {
            points[begin],
            points[begin + 1]
        };
    }

    // Находим середину диапазона, чтобы поделить множество точек на две примерно равные части
    const std::size_t mid = begin + n / 2;

    // Векторы для хранения оболочек левой и правой частей
    std::vector<Point> leftHull;
    std::vector<Point> rightHull;

    // Параллельное построение двух оболочек
    if (n >= PARALLEL_THRESHOLD) { // условие с порогом
        // Создаём параллельную задачу для левой половины
        // shared означает, что задача записывает результат в leftHull
        // firstprivate копирует значения begin и mid для этой задачи
        #pragma omp task shared(leftHull) firstprivate(begin, mid)
        {
            leftHull = buildHull(points, begin, mid);
        }

        // Аналогично
        #pragma omp task shared(rightHull) firstprivate(mid, end)
        {
            rightHull = buildHull(points, mid, end);
        }

        // Ожидаем завершения обеих задач, прежде чем объединять их результаты
        #pragma omp taskwait

    } 
    
    else {
        // Для небольших диапазонов строим левую оболочку последовательно, а затем последовательно строим правую оболочку
        // Параллить тут мелочь объективно невыгодно 
        leftHull = buildHull(points, begin, mid);
        rightHull = buildHull(points, mid, end);
    }

    // Объединяем две полученные выпуклые оболочки
    return mergeHulls(leftHull, rightHull);
}

// 10 Основная функция алгоритма Препараты—Хонга
// Принимает исходный набор точек и возвращает его выпуклую оболочку

std::vector<Point> preparataHong(
    const std::vector<Point>& points
) {
    // Если исходное множество пустое, возвращаем пустую оболочку
    if (points.empty()) {
        return {};
    }

    // Если точек не больше двух, дополнительная обработка не нужна
    if (points.size() <= 2) {
        return points;
    }

    // Создаём копию исходного набора точек.
    std::vector<Point> sortedPoints = points;

    // Сортируем точки по X и Y для разделения множества на левую и правую части
    std::sort(
        sortedPoints.begin(),
        sortedPoints.end(),
        compareByX
    );

    // Вектор для хранения итоговой выпуклой оболочки
    std::vector<Point> result;

    // Запуск параллельной области OpenMP, внутри которой могут работать несколько потоков
    #pragma omp parallel
    {
        // Конструкция single означает, что начальную рекурсивную функцию запускает только один поток
        // В дальнейшем он создаёт задачи (task) OpenMP, которые могут быть распределены между другими потоками
        #pragma omp single
        {
            // Запускаем рекурсивное построение оболочки для всего отсортированного массива точек
            result = buildHull(
                sortedPoints,
                0,
                sortedPoints.size()
            );
        }
    }

    return result;
}
