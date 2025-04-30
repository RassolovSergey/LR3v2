#define _CRT_SECURE_NO_WARNINGS  // Для использования стандартных функций CRT без предупреждений

#include <windows.h>    // Основные функции WinAPI
#include <tchar.h>      // Макросы для Unicode/ANSI
#include <shellapi.h>   // ShellExecute
#include <stdio.h>      // Стандартный ввод-вывод C
#include <stdlib.h>     // Стандартные утилиты C
#include <string.h>     // C-строковые функции
#include <string>       // std::string
#include <sstream>      // std::istringstream, std::ostringstream
#include <vector>       // std::vector
#include <fstream>      // std::ifstream, std::ofstream
#include <iostream>     // std::cout
#include <chrono>       // std::chrono для замера времени
#include <ctime>        // time()

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

// Прототипы функций для сохраения информации в config.txt
bool SaveConfig_Method1();  // Метод 1: память (MMAP)
bool SaveConfig_Method2();  // Метод 2: C stdio
bool SaveConfig_Method3();  // Метод 3: C++ fstream
bool SaveConfig_Method4();  // Метод 4: WinAPI

// Прототип функции парсинга файла config.txt
bool ParseConfigContent(const std::string& content);

// Прототипы функций для бенчмаркинга
void CreateDataFile();          // Создание файла

void ReadDataFile_Method1();    // Метод 1
void ReadDataFile_Method2();    // Метод 2
void ReadDataFile_Method3();    // Метод 3
void ReadDataFile_Method4();    // Метод 4

void BenchmarkDataFile();       // Бенчмарк чтения файла

// Прототип оконной процедуры
LRESULT CALLBACK WindowProcedure(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam);


// ==========================================
// == ФУНКЦИЯ: Парсинг содержимого конфига ==
// ==========================================
bool ParseConfigContent(const std::string& content) {
    std::istringstream iss(content);
    std::string line;
    while (std::getline(iss, line)) {
        if (line.empty() || line[0] == '#') continue;
        auto pos = line.find('=');
        if (pos == std::string::npos) continue;
        std::string key = line.substr(0, pos);
        std::string val = line.substr(pos + 1);
        if (key == "gridSize") gridSize = atoi(val.c_str());
        else if (key == "windowWidth") windowWidth = atoi(val.c_str());
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
    if (gridSize < 1) gridSize = 1;
    if (gridSize > MAX_GRID) gridSize = MAX_GRID;
    return true;
}


// =============================================
// == МЕТОД 1: Отображение файла в память (MMAP) ==
// =============================================
bool LoadConfig_Method1() {
    // Открываем файл с именем configFileName для чтения
    HANDLE hFile = CreateFileA(
        configFileName,       // имя файла
        GENERIC_READ,         // режим: только чтение
        FILE_SHARE_READ,      // разрешаем другим процессам читать файл параллельно
        NULL,                 // атрибуты безопасности по умолчанию
        OPEN_EXISTING,        // открываем только существующий файл
        FILE_ATTRIBUTE_NORMAL,// обычный файл без специальных атрибутов
        NULL                  // шаблонный дескриптор не используется
    );
    // Если не удалось открыть файл — выходим с ошибкой
    if (hFile == INVALID_HANDLE_VALUE)
        return false;

    // Создаём объект отображения файла в память (read-only)
    HANDLE hMap = CreateFileMappingA(
        hFile,                // дескриптор открытого файла
        NULL,                 // атрибуты безопасности по умолчанию
        PAGE_READONLY,        // отображение только для чтения
        0, 0,                 // отображаем весь файл (размер = фактический размер файла)
        NULL                  // имя мапинга не требуется
    );
    // Если не удалось создать отображение — закрываем файл и выходим
    if (!hMap) {
        CloseHandle(hFile);
        return false;
    }

    // Мапим (присоединяем) отображение файла в адресное пространство процесса
    char* pData = (char*)MapViewOfFile(
        hMap,                 // объект отображения
        FILE_MAP_READ,        // доступ: чтение
        0, 0, 0               // отображаем весь файл
    );
    // Если мапинг не удался — освобождаем все дескрипторы и выходим
    if (!pData) {
        CloseHandle(hMap);
        CloseHandle(hFile);
        return false;
    }

    // Конструируем std::string из данных в памяти.
    // Предполагается, что в конце данных есть '\0'
    std::string content(pData);

    // Очищаем мапинг и закрываем дескрипторы
    UnmapViewOfFile(pData);
    CloseHandle(hMap);
    CloseHandle(hFile);

    // Передаём прочитанный контент в функцию парсинга конфига
    return ParseConfigContent(content);
}
// =============================================
// == МЕТОД 1: Сохранение через MMAP            ==
// =============================================
bool SaveConfig_Method1() {
    std::ostringstream oss;
    oss << "gridSize=" << gridSize << "\n"
        << "windowWidth=" << windowWidth << "\n"
        << "windowHeight=" << windowHeight << "\n"
        << "bgColor="
        << static_cast<int>(GetRValue(bgColor)) << " "
        << static_cast<int>(GetGValue(bgColor)) << " "
        << static_cast<int>(GetBValue(bgColor)) << "\n"
        << "gridColor="
        << static_cast<int>(GetRValue(gridColor)) << " "
        << static_cast<int>(GetGValue(gridColor)) << " "
        << static_cast<int>(GetBValue(gridColor)) << "\n";
    std::string data = oss.str();
    DWORD size = static_cast<DWORD>(data.size());

    HANDLE hFile = CreateFileA(
        configFileName,
        GENERIC_READ | GENERIC_WRITE,
        0, nullptr,
        CREATE_ALWAYS,
        FILE_ATTRIBUTE_NORMAL,
        nullptr
    );
    if (hFile == INVALID_HANDLE_VALUE) {
        std::cerr << "[SaveConfig1] CreateFile failed: " << GetLastError() << std::endl;
        return false;
    }
    // Устанавливаем размер файла
    if (SetFilePointer(hFile, size, nullptr, FILE_BEGIN) == INVALID_SET_FILE_POINTER ||
        !SetEndOfFile(hFile)) {
        std::cerr << "[SaveConfig1] SetEndOfFile failed: " << GetLastError() << std::endl;
        CloseHandle(hFile);
        return false;
    }
    HANDLE hMap = CreateFileMappingA(
        hFile,
        nullptr,
        PAGE_READWRITE,
        0,
        size,
        nullptr
    );
    if (!hMap) {
        std::cerr << "[SaveConfig1] CreateFileMapping failed: " << GetLastError() << std::endl;
        CloseHandle(hFile);
        return false;
    }
    LPVOID view = MapViewOfFile(
        hMap,
        FILE_MAP_WRITE,
        0, 0,
        size
    );
    if (!view) {
        std::cerr << "[SaveConfig1] MapViewOfFile failed: " << GetLastError() << std::endl;
        CloseHandle(hMap);
        CloseHandle(hFile);
        return false;
    }
    memcpy(view, data.c_str(), size);
    UnmapViewOfFile(view);
    CloseHandle(hMap);
    CloseHandle(hFile);
    return true;
}



// =============================================
// == МЕТОД 2: C stdio (fopen/fread/fwrite...)  ==
// =============================================
bool SaveConfig_Method2() {
    FILE* f = nullptr;
    errno_t err = fopen_s(&f, configFileName, "wb");
    if (err || !f) {
        char errBuf[128];
        strerror_s(errBuf, sizeof(errBuf), err);
        std::cerr << "[SaveConfig2] fopen failed: " << errBuf << std::endl;
        return false;
    }
    std::ostringstream oss;
    oss << "gridSize=" << gridSize << "\n"
        << "windowWidth=" << windowWidth << "\n"
        << "windowHeight=" << windowHeight << "\n"
        << "bgColor="
        << static_cast<int>(GetRValue(bgColor)) << " "
        << static_cast<int>(GetGValue(bgColor)) << " "
        << static_cast<int>(GetBValue(bgColor)) << "\n"
        << "gridColor="
        << static_cast<int>(GetRValue(gridColor)) << " "
        << static_cast<int>(GetGValue(gridColor)) << " "
        << static_cast<int>(GetBValue(gridColor)) << "\n";
    std::string data = oss.str();
    size_t written = fwrite(data.c_str(), 1, data.size(), f);
    if (written != data.size()) {
        std::cerr << "[SaveConfig2] fwrite wrote " << written << " of " << data.size() << std::endl;
        fclose(f);
        return false;
    }
    fclose(f);
    return true;
}

bool LoadConfig_Method2() {
    FILE* f = nullptr;
    errno_t err = fopen_s(&f, configFileName, "rb");
    if (err || !f) {
        char errBuf[128];
        strerror_s(errBuf, sizeof(errBuf), err);
        std::cerr << "[LoadConfig2] fopen failed: " << errBuf << std::endl;
        return false;
    }
    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    fseek(f, 0, SEEK_SET);
    std::string content;
    content.resize(size);
    size_t read = fread(&content[0], 1, size, f);
    fclose(f);
    if (read != static_cast<size_t>(size)) {
        std::cerr << "[LoadConfig2] fread read " << read << " of " << size << std::endl;
        return false;
    }
    return ParseConfigContent(content);
}

// =====================================
// == МЕТОД 3: C++ потоки (fstream)     ==
// =====================================
bool SaveConfig_Method3() {
    std::ofstream ofs(configFileName);
    if (!ofs.is_open()) {
        std::cerr << "[SaveConfig3] ofstream open failed" << std::endl;
        return false;
    }
    ofs << "gridSize=" << gridSize << "\n"
        << "windowWidth=" << windowWidth << "\n"
        << "windowHeight=" << windowHeight << "\n"
        << "bgColor="
        << static_cast<int>(GetRValue(bgColor)) << " "
        << static_cast<int>(GetGValue(bgColor)) << " "
        << static_cast<int>(GetBValue(bgColor)) << "\n"
        << "gridColor="
        << static_cast<int>(GetRValue(gridColor)) << " "
        << static_cast<int>(GetGValue(gridColor)) << " "
        << static_cast<int>(GetBValue(gridColor)) << "\n";
    ofs.close();
    return true;
}


bool LoadConfig_Method3() {
    std::ifstream ifs(configFileName);
    if (!ifs.is_open()) {
        std::cerr << "[LoadConfig3] ifstream open failed" << std::endl;
        return false;
    }
    std::ostringstream oss;
    oss << ifs.rdbuf();
    ifs.close();
    return ParseConfigContent(oss.str());
}


// ===============================================================
// == МЕТОД 4: WinAPI низкоуровневое чтение/запись (LoadConfig) ==
// ===============================================================
bool LoadConfig_Method4() {
    // 1) Открываем файл с именем configFileName для чтения (read-only)
    HANDLE hFile = CreateFileA(
        configFileName,         // путь к файлу
        GENERIC_READ,           // доступ: только чтение
        FILE_SHARE_READ,        // разрешаем другим процессам читать параллельно
        NULL,                   // атрибуты безопасности по умолчанию
        OPEN_EXISTING,          // открываем только если файл существует
        FILE_ATTRIBUTE_NORMAL,  // обычный файл
        NULL                    // шаблон не используется
    );
    // Проверяем успешность открытия файла
    if (hFile == INVALID_HANDLE_VALUE)
        return false;

    // 2) Получаем размер файла в байтах
    DWORD fileSize = GetFileSize(hFile, NULL);
    // Создаём буфер на fileSize+1 байт для данных плюс завершающий '\0'
    std::vector<char> buffer(fileSize + 1);

    // 3) Читаем содержимое файла в буфер
    DWORD readBytes = 0;
    if (!ReadFile(
        hFile,              // дескриптор файла
        buffer.data(),      // указатель на начало буфера
        fileSize,           // число байт для чтения
        &readBytes,         // фактически прочитанные байты
        NULL                // без overlapped I/O
    ))
    {
        // При ошибке чтения — закрываем файл и выходим с false
        CloseHandle(hFile);
        return false;
    }

    // 4) Добавляем нулевой символ для безопасного создания строки
    buffer[readBytes] = '\0';

    // 5) Освобождаем дескриптор файла
    CloseHandle(hFile);

    // 6) Преобразуем содержимое буфера в std::string и парсим
    return ParseConfigContent(std::string(buffer.data()));
}
// =============================================
// == МЕТОД 4: WinAPI низкоуровневое сохранение ==
// =============================================
bool SaveConfig_Method4() {
    std::ostringstream oss;
    oss << "gridSize=" << gridSize << "\n"
        << "windowWidth=" << windowWidth << "\n"
        << "windowHeight=" << windowHeight << "\n"
        << "bgColor="
        << static_cast<int>(GetRValue(bgColor)) << " "
        << static_cast<int>(GetGValue(bgColor)) << " "
        << static_cast<int>(GetBValue(bgColor)) << "\n"
        << "gridColor="
        << static_cast<int>(GetRValue(gridColor)) << " "
        << static_cast<int>(GetGValue(gridColor)) << " "
        << static_cast<int>(GetBValue(gridColor)) << "\n";
    std::string data = oss.str();
    DWORD size = static_cast<DWORD>(data.size());

    HANDLE hFile = CreateFileA(
        configFileName,
        GENERIC_WRITE,
        0, nullptr,
        CREATE_ALWAYS,
        FILE_ATTRIBUTE_NORMAL,
        nullptr
    );
    if (hFile == INVALID_HANDLE_VALUE) {
        std::cerr << "[SaveConfig4] CreateFile failed: " << GetLastError() << std::endl;
        return false;
    }
    DWORD written = 0;
    if (!WriteFile(hFile, data.c_str(), size, &written, nullptr) || written != size) {
        std::cerr << "[SaveConfig4] WriteFile failed/wrote " << written << " of " << size
            << " Error: " << GetLastError() << std::endl;
        CloseHandle(hFile);
        return false;
    }
    CloseHandle(hFile);
    return true;
}

// ====================================================
// == БЕНЧМАРК ИЗ FҰாЦINE DATA FILE (1 МБ) ==
// ====================================================

// ===============================================
// == ФУНКЦИЯ: Создание бинарного файла данных   ==
// ===============================================
void CreateDataFile() {
    // Указатель на файл для записи
    FILE* f = nullptr;
    // Открываем (или создаём) файл dataFileName в бинарном режиме для записи ("wb")
    fopen_s(&f, dataFileName, "wb");
    // Если не удалось открыть файл — выходим
    if (!f)
        return;

    // Формируем буфер из 1024 нулевых байт
    std::vector<char> buf(1024, 0);

    // Записываем в файл 1024 блока по 1024 байта (итого ~1 МБ)
    for (int i = 0; i < 1024; ++i) {
        // fwrite(ptr, size_of_element, count, file)
        fwrite(buf.data(), 1, buf.size(), f);
    }

    // Закрываем файл после завершения записи
    fclose(f);
}


// =========================================================
// == ФУНКЦИЯ: Чтение бинарного файла через MMAP (Method1) ==
// =========================================================
void ReadDataFile_Method1() {
    // 1) Открываем файл dataFileName для чтения (read-only)
    HANDLE hFile = CreateFileA(
        dataFileName,         // путь к файлу
        GENERIC_READ,         // доступ: только чтение
        FILE_SHARE_READ,      // разрешаем другим процессам читать параллельно
        NULL,                 // атрибуты безопасности по умолчанию
        OPEN_EXISTING,        // открываем только если файл существует
        FILE_ATTRIBUTE_NORMAL,// обычный файл
        NULL                  // шаблонный дескриптор не используется
    );

    // 2) Создаём объект отображения файла в память (read-only)
    HANDLE hMap = CreateFileMappingA(
        hFile,                // дескриптор открытого файла
        NULL,                 // атрибуты безопасности по умолчанию
        PAGE_READONLY,        // отображение только для чтения
        0, 0,                 // отображаем весь файл
        NULL                  // имя мапинга не требуется
    );

    // 3) Мапим (присоединяем) отображение в адресное пространство процесса
    char* p = reinterpret_cast<char*>(
        MapViewOfFile(
            hMap,             // объект отображения
            FILE_MAP_READ,    // доступ: чтение
            0, 0, 0           // отображаем весь файл
        )
        );

    // 4) Демонстрация чтения: читаем первый и последний байт
    // volatile гарантирует, что чтение не будет оптимизировано компилятором
    volatile char a = p[0];                     // первый байт файла
    volatile char b = p[1024 * 1024 - 1];       // последний байт (1 MB - 1)

    // 5) Очищаем отображение и закрываем дескрипторы
    UnmapViewOfFile(p);   // отвязываем область памяти
    CloseHandle(hMap);    // закрываем объект мапинга
    CloseHandle(hFile);   // закрываем дескриптор файла
}


// ==============================================
// == ФУНКЦИЯ: Чтение бинарного файла (stdio)    ==
// ==============================================
void ReadDataFile_Method2() {
    // Указатель на файл для чтения
    FILE* f = nullptr;
    // Открываем файл в бинарном режиме для чтения ("rb")
    fopen_s(&f, dataFileName, "rb");
    // Примечание: здесь нет проверки успешности открытия файла (f == nullptr),
    // что в реальном коде стоит добавить, чтобы избежать UB при неудаче.

    // Создаём буфер размером 1 МБ (1024*1024 байт)
    std::vector<char> buf(1024 * 1024);

    // Читаем данные из файла в буфер:
    // fread(ptr, size_of_element, count, file)
    // вернёт фактическое число прочитанных элементов (bytes)
    fread(buf.data(), 1, buf.size(), f);

    // Закрываем файл после завершения чтения
    fclose(f);
}


// ======================================================
// == ФУНКЦИЯ: Чтение бинарного файла через ifstream   ==
// ======================================================
void ReadDataFile_Method3() {
    // Открываем файл dataFileName в бинарном режиме для чтения
    std::ifstream ifs(
        dataFileName,         // имя файла
        std::ios::binary       // режим: бинарный ввод
    );
    // Проверяем, удалось ли открыть файл (можно добавить обработку ошибки)
    if (!ifs.is_open())
        return;

    // Создаём буфер размером 1 МБ (1024*1024 байт)
    std::vector<char> buf(1024 * 1024);

    // Читаем ровно buf.size() байт из файла в буфер
    // read(ptr, count) — читает count байт в указанный буфер
    ifs.read(buf.data(), buf.size());

    // Закрываем файл (можно неявно при разрушении ifs, но закрываем явно)
    ifs.close();

    // При необходимости далее можно обрабатывать данные из buf
}


// ================================================================
// == МЕТОД 4: WinAPI низкоуровневое чтение бинарного файла      ==
// ================================================================
void ReadDataFile_Method4() {
    // 1) Открываем файл dataFileName для чтения (read-only)
    HANDLE hFile = CreateFileA(
        dataFileName,          // путь к файлу
        GENERIC_READ,          // доступ: только чтение
        FILE_SHARE_READ,       // разрешаем другим процессам читать параллельно
        NULL,                  // атрибуты безопасности по умолчанию
        OPEN_EXISTING,         // открываем только если файл существует
        FILE_ATTRIBUTE_NORMAL, // обычный файл
        NULL                   // шаблонный дескриптор не используется
    );
    // Если файл не открылся — выходим
    if (hFile == INVALID_HANDLE_VALUE)
        return;

    // 2) Получаем размер файла через GetFileSizeEx
    LARGE_INTEGER size;
    if (!GetFileSizeEx(hFile, &size)) {
        CloseHandle(hFile);
        return;
    }

    // 3) Выделяем буфер нужного размера (size.QuadPart байт)
    std::vector<char> buf(static_cast<size_t>(size.QuadPart));

    // 4) Считываем данные из файла в буфер
    // readBytes — фактическое число прочитанных байтов
    DWORD readBytes = 0;
    ReadFile(
        hFile,                    // дескриптор файла
        buf.data(),               // указатель на буфер
        static_cast<DWORD>(size.QuadPart), // число байт для чтения
        &readBytes,               // записанное число прочитанных байт
        NULL                      // без overlapped I/O
    );

    // 5) Закрываем дескриптор файла
    CloseHandle(hFile);

    // При необходимости далее можно работать с данными в buf[0..readBytes-1]
}


// =================================================================
// == ФУНКЦИЯ: Бенчмарк чтения 1 МБ файла разными методами        ==
// =================================================================
void BenchmarkDataFile() {
    // 1) Генерируем тестовый файл размером ~1 МБ
    CreateDataFile();

    // 2) Устанавливаем кодировку консоли UTF‑8 для корректного вывода русских символов
    SetConsoleCP(CP_UTF8);
    SetConsoleOutputCP(CP_UTF8);

    // 3) Выводим заголовок бенчмарка
    std::cout << u8"=== Бенчмарк чтения 1 МБ файла, 10 итераций для каждого метода ===\n";

    // 4) Псевдоним для высокоточного таймера
    using clk = std::chrono::high_resolution_clock;

    // 5) Перебираем четыре метода чтения
    for (int method = 1; method <= 4; ++method) {
        double total_ms = 0;  // аккумулируем общее время для метода

        // 6) Повторяем 10 прогонов для каждого метода
        for (int i = 1; i <= 10; ++i) {
            // Засекаем время начала
            auto t0 = clk::now();

            // Вызываем соответствующий метод чтения
            switch (method) {
            case 1: ReadDataFile_Method1(); break;
            case 2: ReadDataFile_Method2(); break;
            case 3: ReadDataFile_Method3(); break;
            case 4: ReadDataFile_Method4(); break;
            }

            // Засекаем время окончания
            auto t1 = clk::now();
            // Вычисляем миллисекунды, прошедшие между t0 и t1
            double ms = std::chrono::duration<double, std::milli>(t1 - t0).count();
            total_ms += ms;  // добавляем к общему времени

            // 7) Выводим время данного прогона
            std::cout << u8"Метод " << method
                << u8", итерация " << i
                << u8": " << ms << u8" ms\n";
        }

        // 8) После 10 прогонов выводим среднее время для метода
        std::cout << u8"Среднее время для метода " << method << u8": "
            << (total_ms / 10.0) << u8" ms за 10 прогонов\n\n";
    }
}


// ===================================
// == ОКОННАЯ ПРОЦЕДУРА И ОТРИСОВКА ==
// ===================================
LRESULT CALLBACK WindowProcedure(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam) {
    switch (message) {
    case WM_LBUTTONDOWN:
    case WM_RBUTTONDOWN: {
        RECT rc;
        GetClientRect(hwnd, &rc);
        int cellW = rc.right / gridSize;
        int cellH = rc.bottom / gridSize;
        int x = LOWORD(lParam);
        int y = HIWORD(lParam);
        int col = x / cellW;
        int row = y / cellH;
        if (row >= 0 && row < gridSize && col >= 0 && col < gridSize) {
            grid[row][col] = (message == WM_LBUTTONDOWN) ? 1 : 2;
            InvalidateRect(hwnd, NULL, FALSE);
        }
        return 0;
    }
    case WM_KEYDOWN:
        if (wParam == VK_ESCAPE
            || (wParam == 'Q' && (GetKeyState(VK_CONTROL) & 0x8000))) {
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
    case WM_SIZE:
        windowWidth = LOWORD(lParam);
        windowHeight = HIWORD(lParam);
        return 0;
    case WM_MOUSEWHEEL: {
        static int shift = 0;
        shift += (GET_WHEEL_DELTA_WPARAM(wParam) > 0) ? 15 : -15;
        if (shift < 0) shift += 256;
        gridColor = RGB((shift * 3) % 256, (shift * 5) % 256, (shift * 7) % 256);
        InvalidateRect(hwnd, NULL, FALSE);
        return 0;
    }
    case WM_PAINT: {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hwnd, &ps);
        RECT rc;
        GetClientRect(hwnd, &rc);
        HBRUSH hBg = CreateSolidBrush(bgColor);
        FillRect(hdc, &rc, hBg);
        DeleteObject(hBg);

        int cw = rc.right / gridSize;
        int ch = rc.bottom / gridSize;
        HPEN hGridPen = CreatePen(PS_SOLID, 1, gridColor);
        HPEN hPenOld = (HPEN)SelectObject(hdc, hGridPen);
        for (int i = 0; i <= gridSize; ++i) {
            MoveToEx(hdc, i * cw, 0, NULL);
            LineTo(hdc, i * cw, rc.bottom);
            MoveToEx(hdc, 0, i * ch, NULL);
            LineTo(hdc, rc.right, i * ch);
        }
        SelectObject(hdc, hPenOld);
        DeleteObject(hGridPen);

        HPEN   hMarkPen = CreatePen(PS_SOLID, 2, RGB(0, 255, 0));
        HBRUSH hMarkBr = CreateSolidBrush(RGB(255, 255, 0));
        hPenOld = (HPEN)SelectObject(hdc, hMarkPen);
        HBRUSH hBrOld = (HBRUSH)SelectObject(hdc, hMarkBr);

        for (int r = 0; r < gridSize; ++r) {
            for (int c = 0; c < gridSize; ++c) {
                int x0 = c * cw;
                int y0 = r * ch;
                if (grid[r][c] == 1) {
                    Ellipse(hdc, x0 + 5, y0 + 5, x0 + cw - 5, y0 + ch - 5);
                }
                else if (grid[r][c] == 2) {
                    MoveToEx(hdc, x0 + 5, y0 + 5, NULL);
                    LineTo(hdc, x0 + cw - 5, y0 + ch - 5);
                    MoveToEx(hdc, x0 + cw - 5, y0 + 5, NULL);
                    LineTo(hdc, x0 + 5, y0 + ch - 5);
                }
            }
        }
        SelectObject(hdc, hPenOld);
        SelectObject(hdc, hBrOld);
        DeleteObject(hMarkPen);
        DeleteObject(hMarkBr);

        EndPaint(hwnd, &ps);
        return 0;
    }
    case WM_DESTROY:
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

// ==========
// == MAIN ==
// ==========
int _tmain(int argc, _TCHAR* argv[]) {
    srand(static_cast<unsigned>(time(NULL)));
    SetConsoleCP(CP_UTF8);
    SetConsoleOutputCP(CP_UTF8);

    int argSize = -1;
    for (int i = 1; i < argc; ++i) {
        if (_tcscmp(argv[i], _T("-m")) == 0 && i + 1 < argc) {
            configMethod = _ttoi(argv[++i]);
            if (configMethod < 1 || configMethod > 4)
                configMethod = 2;
        }
        else {
            argSize = _ttoi(argv[i]);
        }
    }

    bool ok = false;
    switch (configMethod) {
    case 1: ok = LoadConfig_Method1(); break;
    case 2: ok = LoadConfig_Method2(); break;
    case 3: ok = LoadConfig_Method3(); break;
    case 4: ok = LoadConfig_Method4(); break;
    }

    if (argSize > 0 && argSize <= MAX_GRID) {
        gridSize = argSize;
    }

    BenchmarkDataFile();

    WNDCLASS wc = { 0 };
    wc.style = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc = WindowProcedure;
    wc.hInstance = GetModuleHandle(NULL);
    wc.lpszClassName = _T("Win32SampleApp");
    wc.hbrBackground = CreateSolidBrush(bgColor);
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    wc.hIcon = LoadIcon(NULL, IDI_APPLICATION);
    RegisterClass(&wc);

    HWND hwnd = CreateWindow(
        wc.lpszClassName,
        _T("Win32SampleWindow"),
        WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT, CW_USEDEFAULT,
        windowWidth, windowHeight,
        NULL, NULL,
        wc.hInstance,
        NULL
    );

    ShowWindow(hwnd, SW_SHOW);
    MSG msg;
    while (GetMessage(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
    return 0;
}
