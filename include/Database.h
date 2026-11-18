#pragma once

#include "Book.h"
#include "User.h"

#include <map>
#include <set>
#include <string>
#include <vector>

class Database {
public:
    bool connect(const std::string& iniPath, std::string& error);
    void disconnect();
    bool connected() const;
    bool load(std::vector<Book>& books, std::string& error);
    bool loadUsers(std::vector<User>& users, std::string& error);
    bool loadPermissions(int userId, std::set<std::string>& permissions, std::string& error);
    bool save(const std::vector<Book>& books, std::string& error);
    std::string target() const;

private:
    void* env_ = nullptr;
    void* dbc_ = nullptr;
    std::string server_;
    std::string database_;
    std::string sqlDir_ = ".";
    bool open_ = false;

    bool exec(const std::wstring& sql, std::string& error);
    bool ensureSchema(std::string& error);
    bool ensureId(const wchar_t* insertSql, const wchar_t* selectSql,
                  const std::string& name, int& id, std::string& error);
    bool loadUserMap(std::map<std::string, int>& ids, std::string& error);
    std::string diagnostics(short handleType, void* handle) const;
};
