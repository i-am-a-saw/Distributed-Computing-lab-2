# Makefile для сборки лабораторной работы по OpenMP
# Поддерживает компиляцию как на Linux (g++ / clang++), так и на macOS (Homebrew libomp)

# Определение компилятора C++
CXX ?= g++
CXXFLAGS = -O3 -std=c++17 -Wall

# Определение операционной системы
UNAME_S := $(shell uname -s)

# Настройки для macOS
ifeq ($(UNAME_S),Darwin)
    # Путь к libomp из Homebrew на Apple Silicon или Intel
    BREW_PREFIX := $(shell brew --prefix libomp 2>/dev/null || echo "/opt/homebrew/opt/libomp")
    OMP_FLAGS = -Xpreprocessor -fopenmp -I$(BREW_PREFIX)/include -L$(BREW_PREFIX)/lib -lomp
else
    # Настройки для Linux
    OMP_FLAGS = -fopenmp
endif

TARGET = preparata_hong

.PHONY: all clean run benchmark visualize test help

all: $(TARGET)

$(TARGET): preparata_hong.cpp
	$(CXX) $(CXXFLAGS) preparata_hong.cpp -o $(TARGET) $(OMP_FLAGS)
	@echo "Сборка успешно завершена: ./$(TARGET)"

# Запуск программы на тестовом вводе
run: $(TARGET)
	@echo "Запуск на input_sample.txt:"
	./$(TARGET) < input_sample.txt

# Запуск полного набора бенчмарков
benchmark: $(TARGET)
	@echo "Запуск бенчмарков (замеры времени, ускорения и эффективности)..."
	./$(TARGET) --benchmark benchmark_results.csv

# Генерация графиков визуализации
visualize:
	@echo "Построение графиков и визуализации корректности..."
	uv run --with matplotlib --with numpy python3 visualize.py || python3 visualize.py

# Запуск юнит-тестов корректности
test: $(TARGET)
	@echo "Запуск автоматических тестов корректности..."
	python3 test_runner.py

clean:
	rm -f $(TARGET)
	rm -f benchmark_results.csv
	rm -f *.png
	rm -rf *.dSYM
	@echo "Очистка завершена."

help:
	@echo "Доступные команды:"
	@echo "  make            - Собрать программу preparata_hong"
	@echo "  make run        - Запустить программу с примером input_sample.txt"
	@echo "  make benchmark  - Провести серию бенчмарков (CSV)"
	@echo "  make visualize  - Построить графики и визуализацию (PNG)"
	@echo "  make test       - Прогнать стресс-тесты корректности"
	@echo "  make clean      - Удалить бинарные файлы и графики"
