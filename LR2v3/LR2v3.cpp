/*
 * Зависимости:
 *  gdi32       — для графики (например, кисти)
 *  kernel32    — базовые функции Windows (неявно подключается)
 *  user32      — функции работы с окнами, сообщениями и т.д.
 *  comctl32    — для расширенных элементов управления (здесь не используется)
 */

#include <stdio.h>       // стандартный ввод/вывод
#include <stdlib.h>      // стандартные функции языка С
#include <tchar.h>       // поддержка символов Unicode/ANSI через макросы
#include <windows.h>     // основной заголовочный файл Windows API
#include <time.h>

#define MAX_GRID 30
#define KEY_SHIFTED     0x8000     // используется для проверки, удерживается ли клавиша Shift
#define KEY_TOGGLED     0x0001     // используется для проверки, включён ли режим (например, CapsLock)


 /* Имя класса и заголовок окна */
const TCHAR szWinClass[] = _T("Win32SampleApp");    // имя класса окна
const TCHAR szWinName[] = _T("Win32SampleWindow");  // заголовок окна


// Глобальные переменные
HWND hwnd;         // дескриптор главного окна
HBRUSH hBrush;     // текущая кисть для закраски фона окна
HPEN hPen;         // ручка для сетки

int grid[MAX_GRID][MAX_GRID] = { 0 }; // состояние клеток: 0 - пусто, 1 - круг, 2 - крест
int gridSize = 10; // По умолчанию 10x10, может быть переопределено аргументом
int windowWidth = 320;
int windowHeight = 240;
COLORREF bgColor = RGB(0, 0, 255);     // ЦВЕТ ФОНА
COLORREF gridColor = RGB(255, 0, 0);   // ЦВЕТ СЕТКИ

// === ЗАГРУЗКА КОНФИГА ИЗ ФАЙЛА ===
void LoadConfig()
{
    FILE* file = NULL;
    fopen_s(&file, "config.txt", "r");
    if (!file) return;

    char key[32];
    while (fscanf_s(file, "%31[^=]=%*[ ]", key, (unsigned)_countof(key)) == 1)
    {
        if (strcmp(key, "gridSize") == 0)
            fscanf_s(file, "%d\n", &gridSize);
        else if (strcmp(key, "windowWidth") == 0)
            fscanf_s(file, "%d\n", &windowWidth);
        else if (strcmp(key, "windowHeight") == 0)
            fscanf_s(file, "%d\n", &windowHeight);
        else if (strcmp(key, "bgColor") == 0)
        {
            int r, g, b;
            fscanf_s(file, "%d %d %d\n", &r, &g, &b);
            bgColor = RGB(r, g, b);
        }
        else if (strcmp(key, "gridColor") == 0)
        {
            int r, g, b;
            fscanf_s(file, "%d %d %d\n", &r, &g, &b);
            gridColor = RGB(r, g, b);
        }
        else
            fscanf_s(file, "%*[^\n]\n"); // пропускаем строку
    }

    fclose(file);
}


// === СОХРАНЕНИЕ КОНФИГА В ФАЙЛ ===
void SaveConfig()
{
    FILE* file = NULL;
    fopen_s(&file, "config.txt", "w");
    if (!file) return;

    fprintf(file, "gridSize=%d\n", gridSize);
    fprintf(file, "windowWidth=%d\n", windowWidth);
    fprintf(file, "windowHeight=%d\n", windowHeight);
    fprintf(file, "bgColor=%d %d %d\n", GetRValue(bgColor), GetGValue(bgColor), GetBValue(bgColor));
    fprintf(file, "gridColor=%d %d %d\n", GetRValue(gridColor), GetGValue(gridColor), GetBValue(gridColor));

    fclose(file);
}

/* Оконная процедура */
LRESULT CALLBACK WindowProcedure(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    switch (message)
    {
    case WM_LBUTTONDOWN:
    case WM_RBUTTONDOWN:
    {
        RECT rect;
        GetClientRect(hwnd, &rect);

        int width = rect.right;
        int height = rect.bottom;
        int cellWidth = width / gridSize;
        int cellHeight = height / gridSize;

        int x = LOWORD(lParam);
        int y = HIWORD(lParam);
        int col = x / cellWidth;
        int row = y / cellHeight;

        if (row >= 0 && row < gridSize && col >= 0 && col < gridSize)
        {
            grid[row][col] = (message == WM_LBUTTONDOWN) ? 1 : 2;
            InvalidateRect(hwnd, NULL, FALSE);
        }
        return 0;
    }
    // === ОБРАБОТКА КЛАВИШ ===
    case WM_KEYDOWN:
    {
        if (wParam == VK_ESCAPE || (wParam == 'Q' && (GetKeyState(VK_CONTROL) & 0x8000)))
        {
            PostMessage(hwnd, WM_CLOSE, 0, 0); // безопасно закрываем окно
        }
        else if (wParam == VK_RETURN)
        {
            // Случайный цвет фона
            int r = rand() % 256;
            int g = rand() % 256;
            int b = rand() % 256;

            bgColor = RGB(r, g, b);
            DeleteObject(hBrush);
            hBrush = CreateSolidBrush(RGB(r, g, b));

            InvalidateRect(hwnd, NULL, TRUE);
        }
        else if (wParam == 'C' && (GetKeyState(VK_SHIFT) & 0x8000))
        {
            // Запустить блокнот
            ShellExecute(NULL, _T("open"), _T("notepad.exe"), NULL, NULL, SW_SHOWNORMAL);
        }
        return 0;
    }
    // === ПРОКРУТКА КОЛЕСА МЫШИ: СМЕНА ЦВЕТА СЕТКИ ===
    case WM_MOUSEWHEEL:
    {
        static int colorShift = 0;
        colorShift += GET_WHEEL_DELTA_WPARAM(wParam) > 0 ? 15 : -15;
        if (colorShift < 0) colorShift += 256;

        int r = (colorShift * 3) % 256;
        int g = (colorShift * 5) % 256;
        int b = (colorShift * 7) % 256;

        gridColor = RGB(r, g, b);
        DeleteObject(hPen);
        hPen = CreatePen(PS_SOLID, 1, gridColor);
        InvalidateRect(hwnd, NULL, FALSE);
        return 0;
    }

    case WM_PAINT:
    {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hwnd, &ps);
        RECT rect;
        GetClientRect(hwnd, &rect);
        FillRect(hdc, &rect, hBrush); // синий фон

        int width = rect.right;
        int height = rect.bottom;
        int cellWidth = width / gridSize;
        int cellHeight = height / gridSize;

        // Сетка
        HPEN oldPen = (HPEN)SelectObject(hdc, hPen);
        for (int i = 0; i <= gridSize; ++i)
        {
            MoveToEx(hdc, i * cellWidth, 0, NULL);
            LineTo(hdc, i * cellWidth, height);

            MoveToEx(hdc, 0, i * cellHeight, NULL);
            LineTo(hdc, width, i * cellHeight);
        }
        SelectObject(hdc, oldPen);

        // Фигуры
        HBRUSH yellowBrush = CreateSolidBrush(RGB(255, 255, 0));
        HPEN greenPen = CreatePen(PS_SOLID, 2, RGB(0, 255, 0));
        HPEN oldPen2 = (HPEN)SelectObject(hdc, greenPen);
        HBRUSH oldBrush = (HBRUSH)SelectObject(hdc, yellowBrush);

        for (int row = 0; row < gridSize; ++row)
        {
            for (int col = 0; col < gridSize; ++col)
            {
                int x = col * cellWidth;
                int y = row * cellHeight;

                if (grid[row][col] == 1) // круг
                {
                    Ellipse(hdc, x + 5, y + 5, x + cellWidth - 5, y + cellHeight - 5);
                }
                else if (grid[row][col] == 2) // крест
                {
                    MoveToEx(hdc, x + 5, y + 5, NULL);
                    LineTo(hdc, x + cellWidth - 5, y + cellHeight - 5);

                    MoveToEx(hdc, x + cellWidth - 5, y + 5, NULL);
                    LineTo(hdc, x + 5, y + cellHeight - 5);
                }
            }
        }

        SelectObject(hdc, oldPen2);
        SelectObject(hdc, oldBrush);
        DeleteObject(yellowBrush);
        DeleteObject(greenPen);

        EndPaint(hwnd, &ps);
        return 0;
    }
    // === СОХРАНЕНИЕ КОНФИГА ПРИ ЗАКРЫТИИ ===
    case WM_DESTROY:
        // === ПОЛУЧАЕМ АКТУАЛЬНЫЙ РАЗМЕР ОКНА ===
        RECT rect;
        GetWindowRect(hwnd, &rect);
        windowWidth = rect.right - rect.left;
        windowHeight = rect.bottom - rect.top;

        SaveConfig(); // СОХРАНЯЕМ АКТУАЛЬНЫЕ ДАННЫЕ
        PostQuitMessage(0);
        return 0;
    }

    return DefWindowProc(hwnd, message, wParam, lParam);
}

/* Точка входа в программу */
// === ОСНОВНАЯ ТОЧКА ВХОДА ===
int main(int argc, char** argv)
{
    SetConsoleOutputCP(65001);
    srand((unsigned int)time(NULL)); // ИНИЦИАЛИЗАЦИЯ СЛУЧАЙНОСТИ

    LoadConfig(); // ЗАГРУЗКА КОНФИГА

    // ПРИОРИТЕТ КОМАНДНОЙ СТРОКИ
    if (argc >= 2)
    {
        int n = atoi(argv[1]);
        if (n >= 1 && n <= MAX_GRID)
            gridSize = n;
        else
            printf("Ошибка: введите число от 1 до 30. Используется значение из конфигурации.\n");
    }

    MSG message;
    WNDCLASS wincl = { 0 };
    wincl.style = CS_HREDRAW | CS_VREDRAW;
    wincl.lpfnWndProc = WindowProcedure;
    wincl.hInstance = GetModuleHandle(NULL);
    wincl.lpszClassName = szWinClass;
    wincl.hbrBackground = NULL;

    hBrush = CreateSolidBrush(bgColor);        // ИСПОЛЬЗУЕМ ЦВЕТ ИЗ КОНФИГА
    hPen = CreatePen(PS_SOLID, 1, gridColor);  // ИСПОЛЬЗУЕМ ЦВЕТ ИЗ КОНФИГА

    if (!RegisterClass(&wincl)) return 0;

    hwnd = CreateWindow(
        szWinClass, szWinName,
        WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT, CW_USEDEFAULT,
        windowWidth, windowHeight,
        HWND_DESKTOP, NULL,
        wincl.hInstance, NULL
    );

    ShowWindow(hwnd, SW_SHOW);

    while (GetMessage(&message, NULL, 0, 0))
    {
        TranslateMessage(&message);
        DispatchMessage(&message);
    }

    // ОСВОБОЖДЕНИЕ РЕСУРСОВ
    DestroyWindow(hwnd);
    UnregisterClass(szWinClass, wincl.hInstance);
    DeleteObject(hBrush);
    DeleteObject(hPen);

    return 0;
}