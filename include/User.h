#pragma once

#include <string>

// Пользователь системы. Роль хранится в таблице roles и приходит вместе с пользователем.
struct User {
    int id = 0;
    std::string fullName;
    std::string login;
    std::string role;
};
