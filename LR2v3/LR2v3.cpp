/*
 * Sample Win32 Application with Config and Benchmarking
 *
 * Implements:
 *  - Config load/save via methods 1-4 (memory mapping, C stdio, C++ fstream, WinAPI)
 *  - Command-line method selection (-m N) and gridSize override
 *  - Benchmarking reading a 1MB data file via all four methods (10 iterations each)
 *  - Interactive grid window (Lab 2 functionality)
 */

#include <windows.h>
#include <tchar.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <string>
#include <sstream>
#include <vector>
#include <fstream>
#include <iostream>
#include <chrono>

 // === GLOBALS ===
const int MAX_GRID = 30;
int grid[MAX_GRID][MAX_GRID] = { {0} };
int gridSize = 10;
int windowWidth = 320;
int windowHeight = 240;
COLORREF bgColor = RGB(0, 0, 255);
COLORREF gridColor = RGB(255, 0, 0);
int configMethod = 2; // default method for config load/save

// Config file name
const char* configFileName = "config.txt";

// Forward declarations: config methods
bool LoadConfig_Method1();
bool LoadConfig_Method2();
bool LoadConfig_Method3();
bool LoadConfig_Method4();
bool SaveConfig_Method1();
bool SaveConfig_Method2();
bool SaveConfig_Method3();
bool SaveConfig_Method4();
bool ParseConfigContent(const std::string& content);

// Data file benchmarking
const char* dataFileName = "data.bin";
void CreateDataFile();
void ReadDataFile_Method1();
void ReadDataFile_Method2();
void ReadDataFile_Method3();
void ReadDataFile_Method4();
void BenchmarkDataFile();

// === CONFIG PARSING ===
bool ParseConfigContent(const std::string& content) {
    std::istringstream iss(content);
    std::string line;
    while (std::getline(iss, line)) {
        if (line.empty() || line[0] == '#') continue;
        size_t pos = line.find('=');
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
    return true;
}

// === METHOD 1: MEMORY MAPPING ===
bool LoadConfig_Method1() {
    HANDLE hFile = CreateFileA(configFileName, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (hFile == INVALID_HANDLE_VALUE) return false;
    HANDLE hMap = CreateFileMappingA(hFile, NULL, PAGE_READONLY, 0, 0, NULL);
    if (!hMap) { CloseHandle(hFile); return false; }
    char* pData = (char*)MapViewOfFile(hMap, FILE_MAP_READ, 0, 0, 0);
    if (!pData) { CloseHandle(hMap); CloseHandle(hFile); return false; }
    std::string content(pData);
    UnmapViewOfFile(pData);
    CloseHandle(hMap);
    CloseHandle(hFile);
    return ParseConfigContent(content);
}

bool SaveConfig_Method1() {
    std::ostringstream oss;
    oss << "gridSize=" << gridSize << "\n";
    oss << "windowWidth=" << windowWidth << "\n";
    oss << "windowHeight=" << windowHeight << "\n";
    oss << "bgColor=" << GetRValue(bgColor) << " " << GetGValue(bgColor) << " " << GetBValue(bgColor) << "\n";
    oss << "gridColor=" << GetRValue(gridColor) << " " << GetGValue(gridColor) << " " << GetBValue(gridColor) << "\n";
    std::string data = oss.str();
    DWORD size = (DWORD)data.size();

    HANDLE hFile = CreateFileA(configFileName, GENERIC_READ | GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    if (hFile == INVALID_HANDLE_VALUE) return false;
    HANDLE hMap = CreateFileMappingA(hFile, NULL, PAGE_READWRITE, 0, size, NULL);
    if (!hMap) { CloseHandle(hFile); return false; }
    char* pData = (char*)MapViewOfFile(hMap, FILE_MAP_WRITE, 0, 0, size);
    if (!pData) { CloseHandle(hMap); CloseHandle(hFile); return false; }
    memcpy(pData, data.c_str(), size);
    FlushViewOfFile(pData, size);
    UnmapViewOfFile(pData);
    CloseHandle(hMap);
    CloseHandle(hFile);
    return true;
}

// === METHOD 2: C stdio ===
bool LoadConfig_Method2() {
    FILE* file = NULL;
    fopen_s(&file, configFileName, "r");
    if (!file) return false;
    std::string content;
    char buf[256];
    while (fgets(buf, sizeof(buf), file)) {
        content += buf;
    }
    fclose(file);
    return ParseConfigContent(content);
}

bool SaveConfig_Method2() {
    FILE* file = NULL;
    fopen_s(&file, configFileName, "w");
    if (!file) return false;
    fprintf(file, "gridSize=%d\n", gridSize);
    fprintf(file, "windowWidth=%d\n", windowWidth);
    fprintf(file, "windowHeight=%d\n", windowHeight);
    fprintf(file, "bgColor=%d %d %d\n", GetRValue(bgColor), GetGValue(bgColor), GetBValue(bgColor));
    fprintf(file, "gridColor=%d %d %d\n", GetRValue(gridColor), GetGValue(gridColor), GetBValue(gridColor));
    fclose(file);
    return true;
}

// === METHOD 3: C++ fstream ===
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
    ofs << "bgColor=" << GetRValue(bgColor) << " " << GetGValue(bgColor) << " " << GetBValue(bgColor) << "\n";
    ofs << "gridColor=" << GetRValue(gridColor) << " " << GetGValue(gridColor) << " " << GetBValue(gridColor) << "\n";
    ofs.close();
    return true;
}

// === METHOD 4: WinAPI low-level ===
bool LoadConfig_Method4() {
    HANDLE hFile = CreateFileA(configFileName, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (hFile == INVALID_HANDLE_VALUE) return false;
    DWORD fileSize = GetFileSize(hFile, NULL);
    std::vector<char> buffer(fileSize + 1);
    DWORD read = 0;
    if (!ReadFile(hFile, buffer.data(), fileSize, &read, NULL)) { CloseHandle(hFile); return false; }
    buffer[read] = '\0';
    CloseHandle(hFile);
    return ParseConfigContent(std::string(buffer.data()));
}

bool SaveConfig_Method4() {
    std::ostringstream oss;
    oss << "gridSize=" << gridSize << "\n";
    oss << "windowWidth=" << windowWidth << "\n";
    oss << "windowHeight=" << windowHeight << "\n";
    oss << "bgColor=" << GetRValue(bgColor) << " " << GetGValue(bgColor) << " " << GetBValue(bgColor) << "\n";
    oss << "gridColor=" << GetRValue(gridColor) << " " << GetGValue(gridColor) << " " << GetBValue(gridColor) << "\n";
    std::string data = oss.str();
    DWORD size = (DWORD)data.size();
    HANDLE hFile = CreateFileA(configFileName, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    if (hFile == INVALID_HANDLE_VALUE) return false;
    DWORD written;
    WriteFile(hFile, data.c_str(), size, &written, NULL);
    CloseHandle(hFile);
    return written == size;
}

// === DATA FILE BENCHMARKS ===
void CreateDataFile() {
    // Use method 2 (fwrite) to create a 1MB file
    FILE* f = nullptr;
    fopen_s(&f, dataFileName, "wb");
    if (!f) return;
    const size_t chunk = 1024;
    std::vector<char> buffer(chunk, 0);
    for (int i = 0; i < 1024; ++i) fwrite(buffer.data(), 1, chunk, f);
    fclose(f);
}

void ReadDataFile_Method1() {
    HANDLE hFile = CreateFileA(dataFileName, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    HANDLE hMap = CreateFileMappingA(hFile, NULL, PAGE_READONLY, 0, 0, NULL);
    char* p = (char*)MapViewOfFile(hMap, FILE_MAP_READ, 0, 0, 0);
    volatile char c1 = p[0]; volatile char c2 = p[1024 * 1024 - 1];
    UnmapViewOfFile(p);
    CloseHandle(hMap);
    CloseHandle(hFile);
}

void ReadDataFile_Method2() {
    FILE* f = nullptr;
    fopen_s(&f, dataFileName, "rb");

    const size_t size = 1024 * 1024;
    std::vector<char> buf(size);
    fread(buf.data(), 1, size, f);
    fclose(f);
}

void ReadDataFile_Method3() {
    std::ifstream ifs(dataFileName, std::ios::binary);
    std::vector<char> buf(1024 * 1024);
    ifs.read(buf.data(), buf.size());
    ifs.close();
}

void ReadDataFile_Method4() {
    HANDLE hFile = CreateFileA(dataFileName, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    LARGE_INTEGER size;
    GetFileSizeEx(hFile, &size);
    std::vector<char> buf((size_t)size.QuadPart);
    DWORD read;
    ReadFile(hFile, buf.data(), (DWORD)size.QuadPart, &read, NULL);
    CloseHandle(hFile);
}

void BenchmarkDataFile() {
    CreateDataFile();
    std::cout << "Benchmarking data file read (1 MB) over 10 iterations...\n";
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
            std::cout << "Method " << method << ", Iteration " << i << ": " << ms << " ms\n";
        }
        std::cout << "Method " << method << " average: " << (total_ms / 10.0) << " ms over 10 runs\n\n";
    }
}

// === WINDOW PROCEDURE ===
LRESULT CALLBACK WindowProcedure(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam) {
    switch (message) {
    case WM_LBUTTONDOWN:
    case WM_RBUTTONDOWN: {
        RECT rect; GetClientRect(hwnd, &rect);
        int cw = rect.right / gridSize, ch = rect.bottom / gridSize;
        int x = LOWORD(lParam), y = HIWORD(lParam);
        int col = x / cw, row = y / ch;
        if (row >= 0 && row < gridSize && col >= 0 && col < gridSize) {
            grid[row][col] = (message == WM_LBUTTONDOWN) ? 1 : 2;
            InvalidateRect(hwnd, NULL, FALSE);
        }
        return 0;
    }
    case WM_KEYDOWN:
        if (wParam == VK_ESCAPE || (wParam == 'Q' && (GetKeyState(VK_CONTROL) & 0x8000))) PostMessage(hwnd, WM_CLOSE, 0, 0);
        else if (wParam == VK_RETURN) {
            int r = rand() % 256, g = rand() % 256, b = rand() % 256;
            bgColor = RGB(r, g, b);
            HBRUSH hBrush = CreateSolidBrush(bgColor);
            SetClassLongPtr(hwnd, GCLP_HBRBACKGROUND, (LONG_PTR)hBrush);
            InvalidateRect(hwnd, NULL, TRUE);
        }
        else if (wParam == 'C' && (GetKeyState(VK_SHIFT) & 0x8000)) ShellExecute(NULL, _T("open"), _T("notepad.exe"), NULL, NULL, SW_SHOWNORMAL);
        return 0;
    case WM_MOUSEWHEEL: {
        static int shift = 0;
        shift += GET_WHEEL_DELTA_WPARAM(wParam) > 0 ? 15 : -15;
        if (shift < 0) shift += 256;
        gridColor = RGB((shift * 3) % 256, (shift * 5) % 256, (shift * 7) % 256);
        HPEN hPen = CreatePen(PS_SOLID, 1, gridColor);
        SetClassLongPtr(hwnd, GCLP_HCURSOR, (LONG_PTR)hPen);
        InvalidateRect(hwnd, NULL, FALSE);
        return 0;
    }
    case WM_PAINT: {
        PAINTSTRUCT ps; HDC hdc = BeginPaint(hwnd, &ps);
        RECT rc; GetClientRect(hwnd, &rc);
        HBRUSH hB = CreateSolidBrush(bgColor);
        FillRect(hdc, &rc, hB);
        DeleteObject(hB);
        int cw = rc.right / gridSize, ch = rc.bottom / gridSize;
        HPEN pen = CreatePen(PS_SOLID, 1, gridColor), oldPen = (HPEN)SelectObject(hdc, pen);
        for (int i = 0;i <= gridSize;i++) {
            MoveToEx(hdc, i * cw, 0, NULL); LineTo(hdc, i * cw, rc.bottom);
            MoveToEx(hdc, 0, i * ch, NULL); LineTo(hdc, rc.right, i * ch);
        }
        SelectObject(hdc, oldPen); DeleteObject(pen);
        HPEN gpen = CreatePen(PS_SOLID, 2, RGB(0, 255, 0)); HBRUSH yb = CreateSolidBrush(RGB(255, 255, 0));
        oldPen = (HPEN)SelectObject(hdc, gpen); HBRUSH oldB = (HBRUSH)SelectObject(hdc, yb);
        for (int r = 0;r < gridSize;r++) for (int c = 0;c < gridSize;c++) {
            int x = c * cw, y = r * ch;
            if (grid[r][c] == 1) Ellipse(hdc, x + 5, y + 5, x + cw - 5, y + ch - 5);
            else if (grid[r][c] == 2) {
                MoveToEx(hdc, x + 5, y + 5, NULL);
                LineTo(hdc, x + cw - 5, y + ch - 5);
                MoveToEx(hdc, x + cw - 5, y + 5, NULL);
                LineTo(hdc, x + 5, y + ch - 5);
            }
        }
        SelectObject(hdc, oldPen); SelectObject(hdc, oldB);
        DeleteObject(gpen); DeleteObject(yb);
        EndPaint(hwnd, &ps);
        return 0;
    }
    case WM_DESTROY: {
        // Save config
        switch (configMethod) {
        case 1: SaveConfig_Method1(); break;
        case 2: SaveConfig_Method2(); break;
        case 3: SaveConfig_Method3(); break;
        case 4: SaveConfig_Method4(); break;
        }
        PostQuitMessage(0);
        return 0;
    }
    }
    return DefWindowProc(hwnd, message, wParam, lParam);
}

int main(int argc, char** argv) {
    srand((unsigned)time(NULL));
    // Parse command line
    int argSize = -1;
    for (int i = 1; i < argc; ++i) {
        if (strcmp(argv[i], "-m") == 0 && i + 1 < argc) {
            configMethod = atoi(argv[++i]);
            if (configMethod < 1 || configMethod > 4) configMethod = 2;
        }
        else {
            argSize = atoi(argv[i]);
        }
    }
    // Load config
    bool ok = false;
    switch (configMethod) {
    case 1: ok = LoadConfig_Method1(); break;
    case 2: ok = LoadConfig_Method2(); break;
    case 3: ok = LoadConfig_Method3(); break;
    case 4: ok = LoadConfig_Method4(); break;
    }
    if (!ok) {
        // defaults already set
    }
    if (argSize > 0 && argSize <= MAX_GRID) gridSize = argSize;

    // Task 3: Benchmark
    BenchmarkDataFile();

    // Initialize window class
    WNDCLASS wc = { 0 };
    wc.style = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc = WindowProcedure;
    wc.hInstance = GetModuleHandle(NULL);
    wc.lpszClassName = _T("Win32SampleApp");
    wc.hbrBackground = CreateSolidBrush(bgColor);
    if (!RegisterClass(&wc)) return 0;

    HWND hwnd = CreateWindowA("Win32SampleApp", "Win32SampleWindow",
        WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT,
        windowWidth, windowHeight, NULL, NULL, wc.hInstance, NULL);
    ShowWindow(hwnd, SW_SHOW);

    MSG msg;
    while (GetMessage(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
    return 0;
}
