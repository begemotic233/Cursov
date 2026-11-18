#pragma once

#include <string>
#include <utility>
#include <vector>

void setupConsole();
void clearScreen();
void printLine(const std::string& text);
void print(const std::string& text);
std::string readLine(const std::string& prompt);
void waitEnter();
bool askYesNo(const std::string& prompt);

int showMenu(const std::string& title, const std::vector<std::pair<int, std::string>>& items);
