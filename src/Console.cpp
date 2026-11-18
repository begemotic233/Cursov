#include "Console.h"

#include <windows.h>

#include <string>

namespace {

std::wstring utf8ToWide(const std::string& s) {
    if (s.empty()) return L"";
    int count = MultiByteToWideChar(CP_UTF8, 0, s.data(), static_cast<int>(s.size()), nullptr, 0);
    if (count <= 0) return L"";
    std::wstring wide(count, L'\0');
    MultiByteToWideChar(CP_UTF8, 0, s.data(), static_cast<int>(s.size()), wide.data(), count);
    return wide;
}

std::string wideToUtf8(const std::wstring& s) {
    if (s.empty()) return "";
    int count = WideCharToMultiByte(CP_UTF8, 0, s.data(), static_cast<int>(s.size()), nullptr, 0, nullptr, nullptr);
    if (count <= 0) return "";
    std::string out(count, '\0');
    WideCharToMultiByte(CP_UTF8, 0, s.data(), static_cast<int>(s.size()), out.data(), count, nullptr, nullptr);
    return out;
}

void setLineMode() {
    HANDLE input = GetStdHandle(STD_INPUT_HANDLE);
    SetConsoleMode(input, ENABLE_LINE_INPUT | ENABLE_ECHO_INPUT | ENABLE_PROCESSED_INPUT | ENABLE_EXTENDED_FLAGS);
}

void setNavMode() {
    HANDLE input = GetStdHandle(STD_INPUT_HANDLE);
    SetConsoleMode(input, ENABLE_EXTENDED_FLAGS | ENABLE_PROCESSED_INPUT);
}

enum Key { KeyNone, KeyUp, KeyDown, KeyEnter, KeyEsc, KeyDigit };

struct Press {
    Key key = KeyNone;
    int digit = -1;
};

Press readPress() {
    HANDLE input = GetStdHandle(STD_INPUT_HANDLE);
    while (true) {
        INPUT_RECORD record;
        DWORD read = 0;
        if (!ReadConsoleInputW(input, &record, 1, &read) || read == 0) return {};
        if (record.EventType != KEY_EVENT || !record.Event.KeyEvent.bKeyDown) continue;
        WORD vk = record.Event.KeyEvent.wVirtualKeyCode;
        wchar_t ch = record.Event.KeyEvent.uChar.UnicodeChar;
        if (vk == VK_UP) return {KeyUp, -1};
        if (vk == VK_DOWN) return {KeyDown, -1};
        if (vk == VK_RETURN) return {KeyEnter, -1};
        if (vk == VK_ESCAPE) return {KeyEsc, -1};
        if (ch >= L'0' && ch <= L'9') return {KeyDigit, static_cast<int>(ch - L'0')};
    }
}

}  // namespace

void setupConsole() {
    SetConsoleOutputCP(CP_UTF8);
    SetConsoleCP(CP_UTF8);
    SetConsoleTitleW(L"Каталог библиотеки");

    HANDLE output = GetStdHandle(STD_OUTPUT_HANDLE);
    CONSOLE_FONT_INFOEX font{};
    font.cbSize = sizeof(font);
    font.dwFontSize.Y = 18;
    font.FontWeight = FW_NORMAL;
    wcscpy_s(font.FaceName, L"Consolas");
    SetCurrentConsoleFontEx(output, FALSE, &font);
    setLineMode();
}

void clearScreen() {
    HANDLE output = GetStdHandle(STD_OUTPUT_HANDLE);
    CONSOLE_SCREEN_BUFFER_INFO info{};
    if (!GetConsoleScreenBufferInfo(output, &info)) return;
    DWORD cells = static_cast<DWORD>(info.dwSize.X) * static_cast<DWORD>(info.dwSize.Y);
    DWORD written = 0;
    COORD home{0, 0};
    FillConsoleOutputCharacterW(output, L' ', cells, home, &written);
    FillConsoleOutputAttribute(output, info.wAttributes, cells, home, &written);
    SetConsoleCursorPosition(output, home);
}

void print(const std::string& text) {
    std::wstring wide = utf8ToWide(text);
    HANDLE output = GetStdHandle(STD_OUTPUT_HANDLE);
    DWORD written = 0;
    if (!WriteConsoleW(output, wide.c_str(), static_cast<DWORD>(wide.size()), &written, nullptr)) {
        DWORD bytes = 0;
        WriteFile(output, text.data(), static_cast<DWORD>(text.size()), &bytes, nullptr);
    }
}

void printLine(const std::string& text) {
    print(text);
    print("\n");
}

std::string readLine(const std::string& prompt) {
    setLineMode();
    print(prompt);
    HANDLE input = GetStdHandle(STD_INPUT_HANDLE);
    wchar_t buffer[1024];
    DWORD read = 0;
    if (!ReadConsoleW(input, buffer, 1023, &read, nullptr)) return "";
    while (read > 0 && (buffer[read - 1] == L'\n' || buffer[read - 1] == L'\r')) --read;
    buffer[read] = L'\0';
    return wideToUtf8(std::wstring(buffer, read));
}

void waitEnter() {
    readLine("\nEnter — вернуться в меню...");
}

bool askYesNo(const std::string& prompt) {
    while (true) {
        std::string answer = readLine(prompt);
        if (answer == "д" || answer == "Д" || answer == "y" || answer == "Y" || answer == "да" || answer == "Да") return true;
        if (answer == "н" || answer == "Н" || answer == "n" || answer == "N" || answer == "нет" || answer == "Нет") return false;
        printLine("Введите д или н.");
    }
}

int showMenu(const std::string& title, const std::vector<std::pair<int, std::string>>& items) {
    if (items.empty()) return 0;
    int index = 0;
    setNavMode();
    while (true) {
        clearScreen();
        printLine(title);
        printLine("");
        for (int i = 0; i < static_cast<int>(items.size()); ++i) {
            print(i == index ? "> " : "  ");
            printLine(std::to_string(items[i].first) + ". " + items[i].second);
        }
        printLine("");
        printLine("Вверх/вниз — по кругу, Enter — выбрать, цифра — сразу, Esc — назад.");

        Press press = readPress();
        if (press.key == KeyUp) {
            index = (index + static_cast<int>(items.size()) - 1) % static_cast<int>(items.size());
        } else if (press.key == KeyDown) {
            index = (index + 1) % static_cast<int>(items.size());
        } else if (press.key == KeyEnter) {
            setLineMode();
            return items[index].first;
        } else if (press.key == KeyEsc) {
            setLineMode();
            return 0;
        } else if (press.key == KeyDigit) {
            for (const auto& item : items) {
                if (item.first == press.digit) {
                    setLineMode();
                    return press.digit;
                }
            }
        }
    }
}
