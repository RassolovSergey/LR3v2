/*
 * Пример Win32 приложения с поддержкой конфигурационного файла и бенчмаркингом
 *
 * Реализовано:
 *   1) Загрузка и сохранение настроек приложения четырьмя методами:
 *      1. Память (CreateFileMapping / MapViewOfFile)
 *      2. Стандартное C (fopen/fread/fwrite/fclose)
 *      3. C++ потоки (ifstream/ofstream)
 *      4. Низкоуровневые WinAPI функции (CreateFile/ReadFile/WriteFile)
 *   2) Выбор метода обработки конфига через аргумент командной строки (-m N)
 *   3) Переопределение размера сетки через аргумент командной строки
 *   4) Создание файла данных размером 1 МБ и бенчмаркинг чтения (10 итераций)
 *   5) Отрисовка и взаимодействие с графической сеткой (лабораторная работа 2)
 */

#include <windows.h>    // Основные функции WinAPI
#include <tchar.h>      // Макросы для Unicode/ANSI
#include <stdio.h>      // Стандартный ввод-вывод C
#include <stdlib.h>     // Стандартные утилиты C
#include <string.h>     // C-строковые функции
#include <string>       // std::string
#include <sstream>      // std::istringstream, std::ostringstream
#include <vector>       // std::vector
#include <fstream>      // std::ifstream, std::ofstream
#include <iostream>     // std::cout
#include <chrono>       // std::chrono для замера времени

 // =========================
 // == ГЛОБАЛЬНЫЕ ПЕРЕМЕННЫЕ ==
 // =========================
const int MAX_GRID = 30;                // Максимальный размер сетки
int grid[MAX_GRID][MAX_GRID] = { {0} }; // Состояние клеток: 0 – пусто, 1 – круг, 2 – крест
int gridSize = 10;                      // Размер сетки (по умолчанию 10)
int windowWidth = 320;                  // Ширина окна
int windowHeight = 240;                 // Высота окна
COLORREF bgColor = RGB(0, 0, 255);      // Цвет фона (синий)
COLORREF gridColor = RGB(255, 0, 0);    // Цвет сетки (красный)
int configMethod = 2;                   // Метод работы с конфигом по умолчанию (2)

// Имена файлов конфигурации и данных
const char* configFileName = "config.txt";
const char* dataFileName = "data.bin";

// Прототипы функций для работы с конфигом
bool LoadConfig_Method1();  // Метод 1: память (MMAP)
bool LoadConfig_Method2();  // Метод 2: C stdio
bool LoadConfig_Method3();  // Метод 3: C++ fstream
bool LoadConfig_Method4();  // Метод 4: WinAPI
bool SaveConfig_Method1();
bool SaveConfig_Method2();
bool SaveConfig_Method3();
bool SaveConfig_Method4();
bool ParseConfigContent(const std::string& content);

// Прототипы функций для бенчмаркинга
void CreateDataFile();
void ReadDataFile_Method1();
void ReadDataFile_Method2();
void ReadDataFile_Method3();
void ReadDataFile_Method4();
void BenchmarkDataFile();

// Прототип оконной процедуры
LRESULT CALLBACK WindowProcedure(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam);

// ==========================================
// == ФУНКЦИЯ: Парсинг содержимого конфига ==
// ==========================================
bool ParseConfigContent(const std::string& content) {
    std::istringstream iss(content);
    std::string line;
    // Читаем файл построчно
    while (std::getline(iss, line)) {
        // Пропускаем пустые строки и комментарии
        if (line.empty() || line[0] == '#') continue;
        // Ищем разделитель '='
        size_t pos = line.find('=');
        if (pos == std::string::npos) continue;
        std::string key = line.substr(0, pos);
        std::string val = line.substr(pos + 1);
        // Сопоставляем ключи и присваиваем значения
        if (key == "gridSize")      gridSize = atoi(val.c_str());
        else if (key == "windowWidth")  windowWidth = atoi(val.c_str());
        else if (key == "windowHeight") windowHeight = atoi(val.c_str());
        else if (key == "bgColor") {
            int r, g, b;
            sscanf_s(val.c_str(), "%d %d %d", &r, &g, &b);
            bgColor = RGB(r, g, b);
        }
        else if (key == "gridColor") {
            int r, g, b;
            sscanf_s(val.c_str(), "%d %d %d", &r, &g, &b);
            gridColor = RGB(r, g, b);
        }
    }
    return true;
}

// =============================================
// == МЕТОД 1: Отображение файла в память (MMAP) ==
// =============================================
bool LoadConfig_Method1() {
    // Открываем файл для чтения
    HANDLE hFile = CreateFileA(configFileName,
        GENERIC_READ, FILE_SHARE_READ,
        NULL, OPEN_EXISTING,
        FILE_ATTRIBUTE_NORMAL, NULL);
    if (hFile == INVALID_HANDLE_VALUE) return false;
    // Создаём файловое отображение
    HANDLE hMap = CreateFileMappingA(hFile, NULL,
        PAGE_READONLY, 0, 0, NULL);
    if (!hMap) { CloseHandle(hFile); return false; }
    // Мапим файл в память
    char* pData = (char*)MapViewOfFile(hMap,
        FILE_MAP_READ, 0, 0, 0);
    if (!pData) { CloseHandle(hMap); CloseHandle(hFile); return false; }
    // Читаем содержимое и парсим
    std::string content(pData);
    UnmapViewOfFile(pData);
    CloseHandle(hMap);
    CloseHandle(hFile);
    return ParseConfigContent(content);
}

bool SaveConfig_Method1() {
    // Собираем конфиг в строку
    std::ostringstream oss;
    oss << "gridSize=" << gridSize << "\n";
    oss << "windowWidth=" << windowWidth << "\n";
    oss << "windowHeight=" << windowHeight << "\n";
    oss << "bgColor=" << GetRValue(bgColor) << " "
        << GetGValue(bgColor) << " "
        << GetBValue(bgColor) << "\n";
    oss << "gridColor=" << GetRValue(gridColor) << " "
        << GetGValue(gridColor) << " "
        << GetBValue(gridColor) << "\n";
    std::string data = oss.str();
    DWORD size = (DWORD)data.size();
    // Создаём/перезаписываем файл
    HANDLE hFile = CreateFileA(configFileName,
        GENERIC_WRITE, 0, NULL,
        CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    if (hFile == INVALID_HANDLE_VALUE) return false;
    // Создаём отображение нужного размера
    HANDLE hMap = CreateFileMappingA(hFile, NULL,
        PAGE_READWRITE, 0, size, NULL);
    if (!hMap) { CloseHandle(hFile); return false; }
    // Копируем данные в память
    char* pData = (char*)MapViewOfFile(hMap,
        FILE_MAP_WRITE, 0, 0, size);
    if (!pData) { CloseHandle(hMap); CloseHandle(hFile); return false; }
    memcpy(pData, data.c_str(), size);
    FlushViewOfFile(pData, size);
    UnmapViewOfFile(pData);
    CloseHandle(hMap);
    CloseHandle(hFile);
    return true;
}

// ==================================
// == МЕТОД 2: Стандартное C (stdio) ==
// ==================================
bool LoadConfig_Method2() {
    FILE* file = nullptr;
    fopen_s(&file, configFileName, "r");
    if (!file) return false;
    std::string content;
    char buf[256];
    // Читаем построчно через fgets
    while (fgets(buf, sizeof(buf), file)) {
        content += buf;
    }
    fclose(file);
    return ParseConfigContent(content);
}

bool SaveConfig_Method2() {
    FILE* file = nullptr;
    fopen_s(&file, configFileName, "w");
    if (!file) return false;
    // Записываем через fprintf
    fprintf(file, "gridSize=%d\n", gridSize);
    fprintf(file, "windowWidth=%d\n", windowWidth);
    fprintf(file, "windowHeight=%d\n", windowHeight);
    fprintf(file, "bgColor=%d %d %d\n",
        GetRValue(bgColor), GetGValue(bgColor), GetBValue(bgColor));
    fprintf(file, "gridColor=%d %d %d\n",
        GetRValue(gridColor), GetGValue(gridColor), GetBValue(gridColor));
    fclose(file);
    return true;
}

// =====================================
// == МЕТОД 3: C++ потоки (fstream) ==
// =====================================
bool LoadConfig_Method3() {
    std::ifstream ifs(configFileName);
    if (!ifs.is_open()) return false;
    std::ostringstream oss;
    oss << ifs.rdbuf();
    ifs.close();
    return ParseConfigContent(oss.str());
}

bool SaveConfig_Method3() {
    std::ofstream ofs(configFileName);
    if (!ofs.is_open()) return false;
    ofs << "gridSize=" << gridSize << "\n";
    ofs << "windowWidth=" << windowWidth << "\n";
    ofs << "windowHeight=" << windowHeight << "\n";
    ofs << "bgColor=" << GetRValue(bgColor) << " "
        << GetGValue(bgColor) << " "
        << GetBValue(bgColor) << "\n";
    ofs << "gridColor=" << GetRValue(gridColor) << " "
        << GetGValue(gridColor) << " "
        << GetBValue(gridColor) << "\n";
    ofs.close();
    return true;
}

// ================================================
// == МЕТОД 4: WinAPI низкоуровневое чтение/запись ==
// ================================================
bool LoadConfig_Method4() {
    HANDLE hFile = CreateFileA(configFileName,
        GENERIC_READ, FILE_SHARE_READ,
        NULL, OPEN_EXISTING,
        FILE_ATTRIBUTE_NORMAL, NULL);
    if (hFile == INVALID_HANDLE_VALUE) return false;
    DWORD fileSize = GetFileSize(hFile, NULL);
    std::vector<char> buffer(fileSize + 1);
    DWORD readBytes = 0;
    if (!ReadFile(hFile, buffer.data(), fileSize, &readBytes, NULL)) {
        CloseHandle(hFile); return false;
    }
    buffer[readBytes] = '\0';
    CloseHandle(hFile);
    return ParseConfigContent(std::string(buffer.data()));
}

bool SaveConfig_Method4() {
    std::ostringstream oss;
    oss << "gridSize=" << gridSize << "\n";
    oss << "windowWidth=" << windowWidth << "\n";
    oss << "windowHeight=" << windowHeight << "\n";
    oss << "bgColor=" << GetRValue(bgColor) << " "
        << GetGValue(bgColor) << " "
        << GetBValue(bgColor) << "\n";
    oss << "gridColor=" << GetRValue(gridColor) << " "
        << GetGValue(gridColor) << " "
        << GetBValue(gridColor) << "\n";
    std::string data = oss.str();
    DWORD size = (DWORD)data.size();
    HANDLE hFile = CreateFileA(configFileName,
        GENERIC_WRITE, 0, NULL,
        CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    if (hFile == INVALID_HANDLE_VALUE) return false;
    DWORD written;
    WriteFile(hFile, data.c_str(), size, &written, NULL);
    CloseHandle(hFile);
    return (written == size);
}

// ====================================================
// == БЕНЧМАРК ИЗ FҰாЦINE DATA FILE (1 МБ) ==
// ====================================================

void CreateDataFile() {
    FILE* f = nullptr;
    fopen_s(&f, dataFileName, "wb");
    if (!f) return;
    std::vector<char> buf(1024, 0);
    for (int i = 0; i < 1024; ++i) fwrite(buf.data(), 1, buf.size(), f);
    fclose(f);
}

void ReadDataFile_Method1() {
    HANDLE hFile = CreateFileA(dataFileName,
        GENERIC_READ, FILE_SHARE_READ,
        NULL, OPEN_EXISTING,
        FILE_ATTRIBUTE_NORMAL, NULL);
    HANDLE hMap = CreateFileMappingA(hFile, NULL,
        PAGE_READONLY, 0, 0, NULL);
    char* p = (char*)MapViewOfFile(hMap,
        FILE_MAP_READ, 0, 0, 0);
    volatile char a = p[0];
    volatile char b = p[1024 * 1024 - 1];
    UnmapViewOfFile(p);
    CloseHandle(hMap);
    CloseHandle(hFile);
}

void ReadDataFile_Method2() {
    FILE* f = nullptr;
    fopen_s(&f, dataFileName, "rb");
    std::vector<char> buf(1024 * 1024);
    fread(buf.data(), 1, buf.size(), f);
    fclose(f);
}

void ReadDataFile_Method3() {
    std::ifstream ifs(dataFileName, std::ios::binary);
    std::vector<char> buf(1024 * 1024);
    ifs.read(buf.data(), buf.size());
    ifs.close();
}

void ReadDataFile_Method4() {
    HANDLE hFile = CreateFileA(dataFileName,
        GENERIC_READ, FILE_SHARE_READ,
        NULL, OPEN_EXISTING,
        FILE_ATTRIBUTE_NORMAL, NULL);
    LARGE_INTEGER size;
    GetFileSizeEx(hFile, &size);
    std::vector<char> buf((size_t)size.QuadPart);
    DWORD readBytes;
    ReadFile(hFile, buf.data(), (DWORD)size.QuadPart, &readBytes, NULL);
    CloseHandle(hFile);
}

void BenchmarkDataFile() {
    CreateDataFile();
    // Устанавливаем кодировку консоли в UTF-8
    SetConsoleCP(CP_UTF8);
    SetConsoleOutputCP(CP_UTF8);
    std::cout << u8"=== Бенчмарк чтения 1 МБ файла, 10 итераций для каждого метода ===\n";
    using clk = std::chrono::high_resolution_clock;
    for (int method = 1; method <= 4; ++method) {
        double total_ms = 0;
        for (int i = 1; i <= 10; ++i) {
            auto t0 = clk::now();
            switch (method) {
            case 1: ReadDataFile_Method1(); break;
            case 2: ReadDataFile_Method2(); break;
            case 3: ReadDataFile_Method3(); break;
            case 4: ReadDataFile_Method4(); break;
            }
            auto t1 = clk::now();
            double ms = std::chrono::duration<double, std::milli>(t1 - t0).count();
            total_ms += ms;
            std::cout << u8"Метод " << method << u8", итерация " << i << u8": " << ms << u8" ms\n";
        }
        std::cout << u8"Среднее время для метода " << method << u8": "
            << (total_ms / 10.0) << u8" ms за 10 прогонов\n\n";
    }
}

// =================================
// == ОКОННАЯ ПРОЦЕДУРА И ОТРИСОВКА ==
// =================================
LRESULT CALLBACK WindowProcedure(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam) {
    switch (message) {
    case WM_LBUTTONDOWN:
    case WM_RBUTTONDOWN: {
        RECT rc; GetClientRect(hwnd, &rc);
        int cellW = rc.right / gridSize;
        int cellH = rc.bottom / gridSize;
        int x = LOWORD(lParam), y = HIWORD(lParam);
        int col = x / cellW, row = y / cellH;
        if (row >= 0 && row < gridSize && col >= 0 && col < gridSize) {
            grid[row][col] = (message == WM_LBUTTONDOWN) ? 1 : 2;
            InvalidateRect(hwnd, NULL, FALSE);
        }
        return 0;
    }
    case WM_KEYDOWN:
        if (wParam == VK_ESCAPE || (wParam == 'Q' && (GetKeyState(VK_CONTROL) & 0x8000))) {
            PostMessage(hwnd, WM_CLOSE, 0, 0);
        }
        else if (wParam == VK_RETURN) {
            bgColor = RGB(rand() % 256, rand() % 256, rand() % 256);
            HBRUSH hBr = CreateSolidBrush(bgColor);
            SetClassLongPtr(hwnd, GCLP_HBRBACKGROUND, (LONG_PTR)hBr);
            InvalidateRect(hwnd, NULL, TRUE);
        }
        else if (wParam == 'C' && (GetKeyState(VK_SHIFT) & 0x8000)) {
            ShellExecute(NULL, _T("open"), _T("notepad.exe"), NULL, NULL, SW_SHOWNORMAL);
        }
        return 0;
    case WM_MOUSEWHEEL: {
        static int shift = 0;
        shift += GET_WHEEL_DELTA_WPARAM(wParam) > 0 ? 15 : -15;
        if (shift < 0) shift += 256;
        gridColor = RGB((shift * 3) % 256, (shift * 5) % 256, (shift * 7) % 256);
        InvalidateRect(hwnd, NULL, FALSE);
        return 0;
    }
    case WM_PAINT: {
        PAINTSTRUCT ps; HDC hdc = BeginPaint(hwnd, &ps);
        RECT rc; GetClientRect(hwnd, &rc);
        // Рисуем фон
        HBRUSH hBg = CreateSolidBrush(bgColor);
        FillRect(hdc, &rc, hBg);
        DeleteObject(hBg);
        // Рисуем сетку
        int cw = rc.right / gridSize;
        int ch = rc.bottom / gridSize;
        HPEN hPenOld;
        HPEN hGridPen = CreatePen(PS_SOLID, 1, gridColor);
        hPenOld = (HPEN)SelectObject(hdc, hGridPen);
        for (int i = 0; i <= gridSize; ++i) {
            MoveToEx(hdc, i * cw, 0, NULL); LineTo(hdc, i * cw, rc.bottom);
            MoveToEx(hdc, 0, i * ch, NULL); LineTo(hdc, rc.right, i * ch);
        }
        SelectObject(hdc, hPenOld); DeleteObject(hGridPen);
        // Рисуем фигуры
        HPEN hMarkPen = CreatePen(PS_SOLID, 2, RGB(0, 255, 0));
        HBRUSH hMarkBr = CreateSolidBrush(RGB(255, 255, 0));
        hPenOld = (HPEN)SelectObject(hdc, hMarkPen);
        HBRUSH hBrOld = (HBRUSH)SelectObject(hdc, hMarkBr);
        for (int r = 0; r < gridSize; ++r) for (int c = 0; c < gridSize; ++c) {
            int x0 = c * cw, y0 = r * ch;
            if (grid[r][c] == 1) Ellipse(hdc, x0 + 5, y0 + 5, x0 + cw - 5, y0 + ch - 5);
            else if (grid[r][c] == 2) {
                MoveToEx(hdc, x0 + 5, y0 + 5, NULL); LineTo(hdc, x0 + cw - 5, y0 + ch - 5);
                MoveToEx(hdc, x0 + cw - 5, y0 + 5, NULL); LineTo(hdc, x0 + 5, y0 + ch - 5);
            }
        }
        SelectObject(hdc, hPenOld); SelectObject(hdc, hBrOld);
        DeleteObject(hMarkPen); DeleteObject(hMarkBr);
        EndPaint(hwnd, &ps);
        return 0;
    }
    case WM_DESTROY:
        // Сохраняем конфигурацию перед выходом
        switch (configMethod) {
        case 1: SaveConfig_Method1(); break;
        case 2: SaveConfig_Method2(); break;
        case 3: SaveConfig_Method3(); break;
        case 4: SaveConfig_Method4(); break;
        }
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProc(hwnd, message, wParam, lParam);
}

// ===================
// == ENTRY POINT ==
// ===================
int main(int argc, char** argv) {
    // Инициализация генератора случайных чисел и консоли
    srand((unsigned)time(NULL));
    SetConsoleCP(CP_UTF8);
    SetConsoleOutputCP(CP_UTF8);

    // Разбор аргументов командной строки: -m N и gridSize
    int argSize = -1;
    for (int i = 1; i < argc; ++i) {
        if (strcmp(argv[i], "-m") == 0 && i + 1 < argc) {
            configMethod = atoi(argv[++i]);
            if (configMethod < 1 || configMethod>4) configMethod = 2;
        }
        else argSize = atoi(argv[i]);
    }
    // Загрузка конфигурации выбранным методом
    bool ok = false;
    switch (configMethod) {
    case 1: ok = LoadConfig_Method1(); break;
    case 2: ok = LoadConfig_Method2(); break;
    case 3: ok = LoadConfig_Method3(); break;
    case 4: ok = LoadConfig_Method4(); break;
    }
    // При отсутствии файла используем значения по умолчанию
    if (!ok) {}
    // Переопределение размера сетки из аргумента
    if (argSize > 0 && argSize <= MAX_GRID) gridSize = argSize;

    // Выполняем бенчмарк чтения файла (задача 3)
    BenchmarkDataFile();

    // Регистрация и создание окна
    WNDCLASS wc = { 0 };
    wc.style = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc = WindowProcedure;
    wc.hInstance = GetModuleHandle(NULL);
    wc.lpszClassName = _T("Win32SampleApp");
    wc.hbrBackground = CreateSolidBrush(bgColor);
    if (!RegisterClass(&wc)) return 0;

    HWND hwnd = CreateWindowA("Win32SampleApp", "Win32SampleWindow",
        WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT,
        windowWidth, windowHeight, NULL, NULL,
        wc.hInstance, NULL);
    ShowWindow(hwnd, SW_SHOW);

    // Цикл сообщений
    MSG msg;
    while (GetMessage(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
    return 0;
}
