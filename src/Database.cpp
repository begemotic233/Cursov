#include "Database.h"
#include "Text.h"

#include <windows.h>
#include <sql.h>
#include <sqlext.h>

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <fstream>
#include <map>
#include <sstream>
#include <vector>

namespace {

std::wstring utf8ToWide(const std::string& s) {
    if (s.empty()) return L"";
    int count = MultiByteToWideChar(CP_UTF8, 0, s.data(), static_cast<int>(s.size()), nullptr, 0);
    if (count <= 0) return L"";
    std::wstring wide(count, L'\0');
    MultiByteToWideChar(CP_UTF8, 0, s.data(), static_cast<int>(s.size()), wide.data(), count);
    return wide;
}

std::string wideToUtf8(const wchar_t* s, int length) {
    if (!s || length == 0) return "";
    int count = WideCharToMultiByte(CP_UTF8, 0, s, length, nullptr, 0, nullptr, nullptr);
    if (count <= 0) return "";
    std::string out(count, '\0');
    WideCharToMultiByte(CP_UTF8, 0, s, length, out.data(), count, nullptr, nullptr);
    return out;
}

std::string wideToUtf8(const std::wstring& s) {
    return wideToUtf8(s.c_str(), static_cast<int>(s.size()));
}

std::string columnText(const SQLWCHAR* buffer, SQLLEN indicator) {
    if (indicator <= 0) return "";
    return wideToUtf8(buffer, static_cast<int>(indicator / sizeof(SQLWCHAR)));
}

std::map<std::string, std::string> readIni(const std::string& path) {
    std::map<std::string, std::string> values;
    std::ifstream in(path);
    if (!in) return values;
    std::string line;
    bool first = true;
    while (std::getline(in, line)) {
        if (first) {
            first = false;
            if (line.size() >= 3 &&
                static_cast<unsigned char>(line[0]) == 0xEF &&
                static_cast<unsigned char>(line[1]) == 0xBB &&
                static_cast<unsigned char>(line[2]) == 0xBF) {
                line.erase(0, 3);
            }
        }
        line = trim(line);
        if (line.empty() || line[0] == '#' || line[0] == ';') continue;
        const auto pos = line.find('=');
        if (pos == std::string::npos) continue;
        values[trim(line.substr(0, pos))] = trim(line.substr(pos + 1));
    }
    return values;
}

bool yes(const std::string& value) {
    std::string folded = foldCase(value);
    return folded == "yes" || folded == "true" || folded == "1" || folded == "да";
}

bool safeName(const std::string& name) {
    if (name.empty() || name.size() > 64) return false;
    for (unsigned char ch : name) {
        if (!((ch >= 'A' && ch <= 'Z') || (ch >= 'a' && ch <= 'z') ||
              (ch >= '0' && ch <= '9') || ch == '_')) {
            return false;
        }
    }
    return true;
}

std::wstring pickDriver(SQLHENV env) {
    const wchar_t* preferred[] = {
        L"ODBC Driver 18 for SQL Server",
        L"ODBC Driver 17 for SQL Server",
        L"SQL Server Native Client 11.0",
        L"SQL Server"
    };
    std::vector<std::wstring> installed;
    SQLUSMALLINT direction = SQL_FETCH_FIRST;
    while (true) {
        SQLWCHAR name[512];
        SQLWCHAR attrs[512];
        SQLSMALLINT nameLen = 0;
        SQLSMALLINT attrLen = 0;
        SQLRETURN rc = SQLDriversW(env, direction, name, 512, &nameLen, attrs, 512, &attrLen);
        if (rc != SQL_SUCCESS && rc != SQL_SUCCESS_WITH_INFO) break;
        installed.emplace_back(name);
        direction = SQL_FETCH_NEXT;
    }
    for (const wchar_t* want : preferred) {
        for (const auto& have : installed) {
            if (have == want) return have;
        }
    }
    for (const auto& have : installed) {
        if (have.find(L"SQL Server") != std::wstring::npos) return have;
    }
    return L"";
}

std::wstring connectionString(const std::wstring& driver, const std::wstring& server,
                              const std::wstring& database, bool trusted,
                              const std::wstring& user, const std::wstring& password) {
    std::wstringstream ss;
    ss << L"DRIVER={" << driver << L"};SERVER=" << server << L";DATABASE=" << database << L";";
    if (trusted) {
        ss << L"Trusted_Connection=Yes;";
    } else {
        ss << L"UID=" << user << L";PWD=" << password << L";";
    }
    if (driver.find(L"ODBC Driver") != std::wstring::npos) {
        ss << L"Encrypt=no;TrustServerCertificate=yes;";
    }
    return ss.str();
}

void bindText(SQLHSTMT stmt, SQLUSMALLINT index, const std::wstring& text, SQLLEN& indicator) {
    indicator = SQL_NTS;
    SQLBindParameter(stmt, index, SQL_PARAM_INPUT, SQL_C_WCHAR, SQL_WVARCHAR,
                     static_cast<SQLULEN>(text.size() > 0 ? text.size() : 1), 0,
                     const_cast<wchar_t*>(text.c_str()), 0, &indicator);
}

void bindInt(SQLHSTMT stmt, SQLUSMALLINT index, void* value, SQLLEN& indicator) {
    SQLBindParameter(stmt, index, SQL_PARAM_INPUT, SQL_C_SLONG, SQL_INTEGER, 0, 0, value, 0, &indicator);
}

std::string readFile(const std::string& path) {
    std::ifstream in(path, std::ios::binary);
    if (!in) return "";
    std::ostringstream buffer;
    buffer << in.rdbuf();
    return buffer.str();
}

std::vector<std::string> splitBatches(const std::string& text) {
    std::vector<std::string> batches;
    std::istringstream lines(text);
    std::string line;
    std::string current;
    while (std::getline(lines, line)) {
        if (foldCase(trim(line)) == "go") {
            if (!trim(current).empty()) batches.push_back(current);
            current.clear();
        } else {
            current += line;
            current += '\n';
        }
    }
    if (!trim(current).empty()) batches.push_back(current);
    return batches;
}

}  // namespace

std::string Database::diagnostics(short handleType, void* handle) const {
    std::string text;
    for (SQLSMALLINT i = 1; i <= 5; ++i) {
        SQLWCHAR state[6] = {};
        SQLWCHAR message[512] = {};
        SQLINTEGER native = 0;
        SQLSMALLINT length = 0;
        SQLRETURN rc = SQLGetDiagRecW(handleType, handle, i, state, &native, message, 511, &length);
        if (rc != SQL_SUCCESS && rc != SQL_SUCCESS_WITH_INFO) break;
        if (!text.empty()) text += " ";
        text += wideToUtf8(message);
    }
    if (text.empty()) text = "Неизвестная ошибка ODBC.";
    return text;
}

bool Database::exec(const std::wstring& sql, std::string& error) {
    SQLHSTMT stmt = SQL_NULL_HSTMT;
    if (!SQL_SUCCEEDED(SQLAllocHandle(SQL_HANDLE_STMT, static_cast<SQLHDBC>(dbc_), &stmt))) {
        error = diagnostics(SQL_HANDLE_DBC, dbc_);
        return false;
    }
    SQLRETURN rc = SQLExecDirectW(stmt, const_cast<SQLWCHAR*>(sql.c_str()), SQL_NTS);
    if (!SQL_SUCCEEDED(rc)) {
        error = diagnostics(SQL_HANDLE_STMT, stmt);
        SQLFreeHandle(SQL_HANDLE_STMT, stmt);
        return false;
    }
    SQLFreeHandle(SQL_HANDLE_STMT, stmt);
    return true;
}

bool Database::connect(const std::string& iniPath, std::string& error) {
    disconnect();
    auto ini = readIni(iniPath);
    server_ = ini.count("server") && !ini["server"].empty() ? ini["server"] : "localhost";
    database_ = ini.count("database") && !ini["database"].empty() ? ini["database"] : "Library";

    sqlDir_ = ".";
    const auto slash = iniPath.find_last_of("/\\");
    if (slash != std::string::npos) {
        sqlDir_ = slash == 0 ? iniPath.substr(0, 1) : iniPath.substr(0, slash);
    }

    const bool trusted = !ini.count("trusted") || yes(ini["trusted"]);
    const std::string user = ini.count("user") ? ini["user"] : "";
    const std::string password = ini.count("password") ? ini["password"] : "";

    if (!safeName(database_)) {
        error = "Имя базы в db.ini может содержать только латинские буквы, цифры и _.";
        return false;
    }

    if (!SQL_SUCCEEDED(SQLAllocHandle(SQL_HANDLE_ENV, SQL_NULL_HANDLE, reinterpret_cast<SQLHENV*>(&env_)))) {
        error = "Не удалось создать среду ODBC.";
        env_ = nullptr;
        return false;
    }
    SQLSetEnvAttr(static_cast<SQLHENV>(env_), SQL_ATTR_ODBC_VERSION, reinterpret_cast<SQLPOINTER>(SQL_OV_ODBC3), 0);

    std::wstring driver = pickDriver(static_cast<SQLHENV>(env_));
    if (driver.empty()) {
        error = "Не найден драйвер ODBC для SQL Server. Установите ODBC Driver 17 или 18.";
        disconnect();
        return false;
    }

    if (!SQL_SUCCEEDED(SQLAllocHandle(SQL_HANDLE_DBC, static_cast<SQLHENV>(env_), reinterpret_cast<SQLHDBC*>(&dbc_)))) {
        error = diagnostics(SQL_HANDLE_ENV, env_);
        disconnect();
        return false;
    }

    SQLSetConnectAttr(static_cast<SQLHDBC>(dbc_), SQL_LOGIN_TIMEOUT, reinterpret_cast<SQLPOINTER>(static_cast<SQLULEN>(5)), 0);

    std::wstring master = connectionString(driver, utf8ToWide(server_), L"master", trusted,
                                           utf8ToWide(user), utf8ToWide(password));
    SQLWCHAR out[1024];
    SQLSMALLINT outLen = 0;
    SQLRETURN rc = SQLDriverConnectW(static_cast<SQLHDBC>(dbc_), nullptr,
                                     const_cast<SQLWCHAR*>(master.c_str()), SQL_NTS,
                                     out, 1024, &outLen, SQL_DRIVER_NOPROMPT);
    if (!SQL_SUCCEEDED(rc)) {
        error = "Нет соединения с SQL Server (" + server_ + "). " + diagnostics(SQL_HANDLE_DBC, dbc_);
        disconnect();
        return false;
    }

    std::wstring createDb = L"IF DB_ID(N'" + utf8ToWide(database_) + L"') IS NULL CREATE DATABASE [" + utf8ToWide(database_) + L"]";
    if (!exec(createDb, error)) {
        disconnect();
        return false;
    }
    SQLDisconnect(static_cast<SQLHDBC>(dbc_));

    std::wstring catalog = connectionString(driver, utf8ToWide(server_), utf8ToWide(database_), trusted,
                                            utf8ToWide(user), utf8ToWide(password));
    rc = SQLDriverConnectW(static_cast<SQLHDBC>(dbc_), nullptr,
                           const_cast<SQLWCHAR*>(catalog.c_str()), SQL_NTS,
                           out, 1024, &outLen, SQL_DRIVER_NOPROMPT);
    if (!SQL_SUCCEEDED(rc)) {
        error = "База создана, но открыть её не удалось. " + diagnostics(SQL_HANDLE_DBC, dbc_);
        disconnect();
        return false;
    }

    if (!ensureSchema(error)) {
        disconnect();
        return false;
    }

    open_ = true;
    error.clear();
    return true;
}

bool Database::ensureSchema(std::string& error) {
    namespace fs = std::filesystem;
    const fs::path dir = fs::path(sqlDir_) / "sql";

    std::error_code ec;
    if (!fs::is_directory(dir, ec)) {
        error = "Не найдена папка SQL-скриптов: " + dir.string();
        return false;
    }

    std::vector<fs::path> files;
    for (const auto& entry : fs::directory_iterator(dir, ec)) {
        if (!entry.is_regular_file()) continue;
        const std::string name = entry.path().filename().string();
        if (name.size() < 3 ||
            !std::isdigit(static_cast<unsigned char>(name[0])) ||
            !std::isdigit(static_cast<unsigned char>(name[1]))) {
            continue;
        }
        if (entry.path().extension() != ".sql") continue;
        files.push_back(entry.path());
    }
    if (ec) {
        error = "Не удалось прочитать папку " + dir.string() + ": " + ec.message();
        return false;
    }
    if (files.empty()) {
        error = "В папке " + dir.string() + " нет SQL-скриптов с числовым префиксом.";
        return false;
    }

    std::sort(files.begin(), files.end());
    for (const auto& file : files) {
        std::string text = readFile(file.string());
        if (text.size() >= 3 &&
            static_cast<unsigned char>(text[0]) == 0xEF &&
            static_cast<unsigned char>(text[1]) == 0xBB &&
            static_cast<unsigned char>(text[2]) == 0xBF) {
            text.erase(0, 3);
        }
        for (const auto& batch : splitBatches(text)) {
            std::string execError;
            if (!exec(utf8ToWide(batch), execError)) {
                error = "Ошибка в " + file.filename().string() + ": " + execError;
                return false;
            }
        }
    }
    return true;
}

bool Database::ensureId(const wchar_t* insertSql, const wchar_t* selectSql,
                        const std::string& name, int& id, std::string& error) {
    std::wstring wide = utf8ToWide(name);

    SQLHSTMT stmt = SQL_NULL_HSTMT;
    if (!SQL_SUCCEEDED(SQLAllocHandle(SQL_HANDLE_STMT, static_cast<SQLHDBC>(dbc_), &stmt))) {
        error = diagnostics(SQL_HANDLE_DBC, dbc_);
        return false;
    }
    SQLLEN firstInd = 0;
    SQLLEN secondInd = 0;
    bindText(stmt, 1, wide, firstInd);
    bindText(stmt, 2, wide, secondInd);
    if (!SQL_SUCCEEDED(SQLExecDirectW(stmt, const_cast<SQLWCHAR*>(insertSql), SQL_NTS))) {
        error = diagnostics(SQL_HANDLE_STMT, stmt);
        SQLFreeHandle(SQL_HANDLE_STMT, stmt);
        return false;
    }
    SQLFreeHandle(SQL_HANDLE_STMT, stmt);

    stmt = SQL_NULL_HSTMT;
    if (!SQL_SUCCEEDED(SQLAllocHandle(SQL_HANDLE_STMT, static_cast<SQLHDBC>(dbc_), &stmt))) {
        error = diagnostics(SQL_HANDLE_DBC, dbc_);
        return false;
    }
    SQLLEN selectInd = 0;
    bindText(stmt, 1, wide, selectInd);
    if (!SQL_SUCCEEDED(SQLExecDirectW(stmt, const_cast<SQLWCHAR*>(selectSql), SQL_NTS))) {
        error = diagnostics(SQL_HANDLE_STMT, stmt);
        SQLFreeHandle(SQL_HANDLE_STMT, stmt);
        return false;
    }
    SQLINTEGER value = 0;
    SQLLEN valueInd = 0;
    SQLBindCol(stmt, 1, SQL_C_SLONG, &value, 0, &valueInd);
    if (!SQL_SUCCEEDED(SQLFetch(stmt))) {
        error = diagnostics(SQL_HANDLE_STMT, stmt);
        SQLFreeHandle(SQL_HANDLE_STMT, stmt);
        return false;
    }
    SQLFreeHandle(SQL_HANDLE_STMT, stmt);
    id = value;
    return true;
}

bool Database::loadUserMap(std::map<std::string, int>& ids, std::string& error) {
    SQLHSTMT stmt = SQL_NULL_HSTMT;
    if (!SQL_SUCCEEDED(SQLAllocHandle(SQL_HANDLE_STMT, static_cast<SQLHDBC>(dbc_), &stmt))) {
        error = diagnostics(SQL_HANDLE_DBC, dbc_);
        return false;
    }
    const wchar_t* query = L"SELECT id, full_name FROM dbo.users";
    if (!SQL_SUCCEEDED(SQLExecDirectW(stmt, const_cast<SQLWCHAR*>(query), SQL_NTS))) {
        error = diagnostics(SQL_HANDLE_STMT, stmt);
        SQLFreeHandle(SQL_HANDLE_STMT, stmt);
        return false;
    }
    SQLINTEGER id = 0;
    SQLWCHAR name[256];
    SQLLEN idInd = 0;
    SQLLEN nameInd = 0;
    SQLBindCol(stmt, 1, SQL_C_SLONG, &id, 0, &idInd);
    SQLBindCol(stmt, 2, SQL_C_WCHAR, name, sizeof(name), &nameInd);
    while (true) {
        SQLRETURN rc = SQLFetch(stmt);
        if (rc == SQL_NO_DATA) break;
        if (!SQL_SUCCEEDED(rc)) {
            error = diagnostics(SQL_HANDLE_STMT, stmt);
            SQLFreeHandle(SQL_HANDLE_STMT, stmt);
            return false;
        }
        ids[foldCase(columnText(name, nameInd))] = id;
    }
    SQLFreeHandle(SQL_HANDLE_STMT, stmt);
    return true;
}

void Database::disconnect() {
    if (dbc_) {
        SQLDisconnect(static_cast<SQLHDBC>(dbc_));
        SQLFreeHandle(SQL_HANDLE_DBC, static_cast<SQLHDBC>(dbc_));
        dbc_ = nullptr;
    }
    if (env_) {
        SQLFreeHandle(SQL_HANDLE_ENV, static_cast<SQLHENV>(env_));
        env_ = nullptr;
    }
    open_ = false;
}

bool Database::connected() const {
    return open_;
}

std::string Database::target() const {
    return server_ + " / " + database_;
}

bool Database::loadUsers(std::vector<User>& users, std::string& error) {
    if (!open_) {
        error = "Нет соединения с SQL Server.";
        return false;
    }
    SQLHSTMT stmt = SQL_NULL_HSTMT;
    if (!SQL_SUCCEEDED(SQLAllocHandle(SQL_HANDLE_STMT, static_cast<SQLHDBC>(dbc_), &stmt))) {
        error = diagnostics(SQL_HANDLE_DBC, dbc_);
        return false;
    }
    const wchar_t* query =
        L"SELECT u.id, u.full_name, u.login, r.name FROM dbo.users u "
        L"JOIN dbo.roles r ON r.id = u.role_id ORDER BY u.id";
    if (!SQL_SUCCEEDED(SQLExecDirectW(stmt, const_cast<SQLWCHAR*>(query), SQL_NTS))) {
        error = diagnostics(SQL_HANDLE_STMT, stmt);
        SQLFreeHandle(SQL_HANDLE_STMT, stmt);
        return false;
    }

    SQLINTEGER id = 0;
    SQLWCHAR fullName[256];
    SQLWCHAR login[64];
    SQLWCHAR role[64];
    SQLLEN idInd = 0, fullNameInd = 0, loginInd = 0, roleInd = 0;
    SQLBindCol(stmt, 1, SQL_C_SLONG, &id, 0, &idInd);
    SQLBindCol(stmt, 2, SQL_C_WCHAR, fullName, sizeof(fullName), &fullNameInd);
    SQLBindCol(stmt, 3, SQL_C_WCHAR, login, sizeof(login), &loginInd);
    SQLBindCol(stmt, 4, SQL_C_WCHAR, role, sizeof(role), &roleInd);

    std::vector<User> loaded;
    while (true) {
        SQLRETURN rc = SQLFetch(stmt);
        if (rc == SQL_NO_DATA) break;
        if (!SQL_SUCCEEDED(rc)) {
            error = diagnostics(SQL_HANDLE_STMT, stmt);
            SQLFreeHandle(SQL_HANDLE_STMT, stmt);
            return false;
        }
        User user;
        user.id = id;
        user.fullName = columnText(fullName, fullNameInd);
        user.login = columnText(login, loginInd);
        user.role = columnText(role, roleInd);
        loaded.push_back(user);
    }
    SQLFreeHandle(SQL_HANDLE_STMT, stmt);
    users.swap(loaded);
    return true;
}

bool Database::loadPermissions(int userId, std::set<std::string>& permissions, std::string& error) {
    permissions.clear();
    if (!open_) {
        error = "Нет соединения с SQL Server.";
        return false;
    }
    SQLHSTMT stmt = SQL_NULL_HSTMT;
    if (!SQL_SUCCEEDED(SQLAllocHandle(SQL_HANDLE_STMT, static_cast<SQLHDBC>(dbc_), &stmt))) {
        error = diagnostics(SQL_HANDLE_DBC, dbc_);
        return false;
    }
    const wchar_t* query =
        L"SELECT rp.permission FROM dbo.role_permissions rp "
        L"JOIN dbo.users u ON u.role_id = rp.role_id WHERE u.id = ?";
    if (!SQL_SUCCEEDED(SQLPrepareW(stmt, const_cast<SQLWCHAR*>(query), SQL_NTS))) {
        error = diagnostics(SQL_HANDLE_STMT, stmt);
        SQLFreeHandle(SQL_HANDLE_STMT, stmt);
        return false;
    }
    SQLINTEGER id = userId;
    SQLLEN idInd = 0;
    bindInt(stmt, 1, &id, idInd);
    if (!SQL_SUCCEEDED(SQLExecute(stmt))) {
        error = diagnostics(SQL_HANDLE_STMT, stmt);
        SQLFreeHandle(SQL_HANDLE_STMT, stmt);
        return false;
    }

    SQLWCHAR permission[64];
    SQLLEN permissionInd = 0;
    SQLBindCol(stmt, 1, SQL_C_WCHAR, permission, sizeof(permission), &permissionInd);

    std::set<std::string> loaded;
    while (true) {
        SQLRETURN rc = SQLFetch(stmt);
        if (rc == SQL_NO_DATA) break;
        if (!SQL_SUCCEEDED(rc)) {
            error = diagnostics(SQL_HANDLE_STMT, stmt);
            SQLFreeHandle(SQL_HANDLE_STMT, stmt);
            return false;
        }
        loaded.insert(columnText(permission, permissionInd));
    }
    SQLFreeHandle(SQL_HANDLE_STMT, stmt);
    permissions.swap(loaded);
    return true;
}

bool Database::load(std::vector<Book>& books, std::string& error) {
    if (!open_) {
        error = "Нет соединения с SQL Server.";
        return false;
    }

    std::vector<Book> loaded;
    std::map<int, std::size_t> index;

    SQLHSTMT stmt = SQL_NULL_HSTMT;
    if (!SQL_SUCCEEDED(SQLAllocHandle(SQL_HANDLE_STMT, static_cast<SQLHDBC>(dbc_), &stmt))) {
        error = diagnostics(SQL_HANDLE_DBC, dbc_);
        return false;
    }
    const wchar_t* bookQuery = L"SELECT id, title, genre, [year], price FROM dbo.books ORDER BY id";
    if (!SQL_SUCCEEDED(SQLExecDirectW(stmt, const_cast<SQLWCHAR*>(bookQuery), SQL_NTS))) {
        error = diagnostics(SQL_HANDLE_STMT, stmt);
        SQLFreeHandle(SQL_HANDLE_STMT, stmt);
        return false;
    }

    SQLINTEGER id = 0;
    SQLINTEGER year = 0;
    SQLDOUBLE price = 0.0;
    SQLWCHAR title[256];
    SQLWCHAR genre[128];
    SQLLEN idInd = 0, yearInd = 0, priceInd = 0, titleInd = 0, genreInd = 0;
    SQLBindCol(stmt, 1, SQL_C_SLONG, &id, 0, &idInd);
    SQLBindCol(stmt, 2, SQL_C_WCHAR, title, sizeof(title), &titleInd);
    SQLBindCol(stmt, 3, SQL_C_WCHAR, genre, sizeof(genre), &genreInd);
    SQLBindCol(stmt, 4, SQL_C_SLONG, &year, 0, &yearInd);
    SQLBindCol(stmt, 5, SQL_C_DOUBLE, &price, 0, &priceInd);

    while (true) {
        SQLRETURN rc = SQLFetch(stmt);
        if (rc == SQL_NO_DATA) break;
        if (!SQL_SUCCEEDED(rc)) {
            error = diagnostics(SQL_HANDLE_STMT, stmt);
            SQLFreeHandle(SQL_HANDLE_STMT, stmt);
            return false;
        }
        Book book;
        book.id = id;
        book.title = columnText(title, titleInd);
        book.genre = columnText(genre, genreInd);
        book.year = year;
        book.price = price;
        index[book.id] = loaded.size();
        loaded.push_back(book);
    }
    SQLFreeHandle(SQL_HANDLE_STMT, stmt);

    // Авторы (связь многие-ко-многим).
    stmt = SQL_NULL_HSTMT;
    if (!SQL_SUCCEEDED(SQLAllocHandle(SQL_HANDLE_STMT, static_cast<SQLHDBC>(dbc_), &stmt))) {
        error = diagnostics(SQL_HANDLE_DBC, dbc_);
        return false;
    }
    const wchar_t* authorQuery =
        L"SELECT ba.book_id, a.full_name FROM dbo.book_authors ba "
        L"JOIN dbo.authors a ON a.id = ba.author_id ORDER BY ba.book_id, ba.author_order";
    if (!SQL_SUCCEEDED(SQLExecDirectW(stmt, const_cast<SQLWCHAR*>(authorQuery), SQL_NTS))) {
        error = diagnostics(SQL_HANDLE_STMT, stmt);
        SQLFreeHandle(SQL_HANDLE_STMT, stmt);
        return false;
    }
    SQLINTEGER authorBookId = 0;
    SQLWCHAR authorName[256];
    SQLLEN authorBookIdInd = 0, authorNameInd = 0;
    SQLBindCol(stmt, 1, SQL_C_SLONG, &authorBookId, 0, &authorBookIdInd);
    SQLBindCol(stmt, 2, SQL_C_WCHAR, authorName, sizeof(authorName), &authorNameInd);
    while (true) {
        SQLRETURN rc = SQLFetch(stmt);
        if (rc == SQL_NO_DATA) break;
        if (!SQL_SUCCEEDED(rc)) {
            error = diagnostics(SQL_HANDLE_STMT, stmt);
            SQLFreeHandle(SQL_HANDLE_STMT, stmt);
            return false;
        }
        auto it = index.find(authorBookId);
        if (it != index.end()) {
            loaded[it->second].authors.push_back(columnText(authorName, authorNameInd));
        }
    }
    SQLFreeHandle(SQL_HANDLE_STMT, stmt);

    // Экземпляры: состояние, кто принёс, кто принял.
    stmt = SQL_NULL_HSTMT;
    if (!SQL_SUCCEEDED(SQLAllocHandle(SQL_HANDLE_STMT, static_cast<SQLHDBC>(dbc_), &stmt))) {
        error = diagnostics(SQL_HANDLE_DBC, dbc_);
        return false;
    }
    const wchar_t* copyQuery =
        L"SELECT c.book_id, c.inventory, c.condition_code, ISNULL(d.full_name, N''), "
        L"ISNULL(u.full_name, N''), CONVERT(NVARCHAR(10), c.received_on, 23) "
        L"FROM dbo.book_copies c "
        L"LEFT JOIN dbo.donors d ON d.id = c.donor_id "
        L"LEFT JOIN dbo.users u ON u.id = c.received_by "
        L"ORDER BY c.book_id, c.inventory";
    if (!SQL_SUCCEEDED(SQLExecDirectW(stmt, const_cast<SQLWCHAR*>(copyQuery), SQL_NTS))) {
        error = diagnostics(SQL_HANDLE_STMT, stmt);
        SQLFreeHandle(SQL_HANDLE_STMT, stmt);
        return false;
    }
    SQLINTEGER copyBookId = 0;
    SQLWCHAR inventory[64];
    SQLWCHAR condition[32];
    SQLWCHAR donor[256];
    SQLWCHAR staff[256];
    SQLWCHAR receivedOn[64];
    SQLLEN copyBookIdInd = 0, inventoryInd = 0, conditionInd = 0;
    SQLLEN donorInd = 0, staffInd = 0, receivedOnInd = 0;
    SQLBindCol(stmt, 1, SQL_C_SLONG, &copyBookId, 0, &copyBookIdInd);
    SQLBindCol(stmt, 2, SQL_C_WCHAR, inventory, sizeof(inventory), &inventoryInd);
    SQLBindCol(stmt, 3, SQL_C_WCHAR, condition, sizeof(condition), &conditionInd);
    SQLBindCol(stmt, 4, SQL_C_WCHAR, donor, sizeof(donor), &donorInd);
    SQLBindCol(stmt, 5, SQL_C_WCHAR, staff, sizeof(staff), &staffInd);
    SQLBindCol(stmt, 6, SQL_C_WCHAR, receivedOn, sizeof(receivedOn), &receivedOnInd);
    while (true) {
        SQLRETURN rc = SQLFetch(stmt);
        if (rc == SQL_NO_DATA) break;
        if (!SQL_SUCCEEDED(rc)) {
            error = diagnostics(SQL_HANDLE_STMT, stmt);
            SQLFreeHandle(SQL_HANDLE_STMT, stmt);
            return false;
        }
        auto it = index.find(copyBookId);
        if (it == index.end()) continue;
        BookCopy copy;
        copy.inventory = columnText(inventory, inventoryInd);
        copy.condition = columnText(condition, conditionInd);
        copy.donor = columnText(donor, donorInd);
        copy.receivedBy = columnText(staff, staffInd);
        copy.receivedOn = columnText(receivedOn, receivedOnInd);
        loaded[it->second].copies.push_back(copy);
    }
    SQLFreeHandle(SQL_HANDLE_STMT, stmt);

    books.swap(loaded);
    return true;
}

bool Database::save(const std::vector<Book>& books, std::string& error) {
    if (!open_) {
        error = "Нет соединения с SQL Server.";
        return false;
    }
    SQLHDBC dbc = static_cast<SQLHDBC>(dbc_);
    if (!SQL_SUCCEEDED(SQLSetConnectAttr(dbc, SQL_ATTR_AUTOCOMMIT, reinterpret_cast<SQLPOINTER>(SQL_AUTOCOMMIT_OFF), 0))) {
        error = diagnostics(SQL_HANDLE_DBC, dbc);
        return false;
    }

    bool ok = true;
    std::map<std::string, int> userIds;
    if (ok) ok = loadUserMap(userIds, error);

    if (ok && !exec(L"DELETE FROM dbo.book_copies", error)) ok = false;
    if (ok && !exec(L"DELETE FROM dbo.book_authors", error)) ok = false;
    if (ok && !exec(L"DELETE FROM dbo.books", error)) ok = false;
    if (ok && !exec(L"DELETE FROM dbo.donors", error)) ok = false;

    // Книги.
    SQLHSTMT bookStmt = SQL_NULL_HSTMT;
    if (ok && !SQL_SUCCEEDED(SQLAllocHandle(SQL_HANDLE_STMT, dbc, &bookStmt))) {
        error = diagnostics(SQL_HANDLE_DBC, dbc);
        ok = false;
    }
    const wchar_t* insertBook = L"INSERT INTO dbo.books (id, title, genre, [year], price) VALUES (?, ?, ?, ?, ?)";
    if (ok && !SQL_SUCCEEDED(SQLPrepareW(bookStmt, const_cast<SQLWCHAR*>(insertBook), SQL_NTS))) {
        error = diagnostics(SQL_HANDLE_STMT, bookStmt);
        ok = false;
    }
    for (const auto& book : books) {
        if (!ok) break;
        SQLINTEGER bookId = book.id;
        SQLINTEGER bookYear = book.year;
        SQLDOUBLE bookPrice = book.price;
        std::wstring bookTitle = utf8ToWide(book.title);
        std::wstring bookGenre = utf8ToWide(book.genre);
        SQLLEN bookIdInd = 0, bookYearInd = 0, bookPriceInd = 0;
        SQLLEN bookTitleInd = 0, bookGenreInd = 0;
        SQLFreeStmt(bookStmt, SQL_RESET_PARAMS);
        bindInt(bookStmt, 1, &bookId, bookIdInd);
        bindText(bookStmt, 2, bookTitle, bookTitleInd);
        bindText(bookStmt, 3, bookGenre, bookGenreInd);
        bindInt(bookStmt, 4, &bookYear, bookYearInd);
        SQLBindParameter(bookStmt, 5, SQL_PARAM_INPUT, SQL_C_DOUBLE, SQL_DECIMAL, 12, 2,
                         &bookPrice, 0, &bookPriceInd);
        if (!SQL_SUCCEEDED(SQLExecute(bookStmt))) {
            error = diagnostics(SQL_HANDLE_STMT, bookStmt);
            ok = false;
        }
    }
    if (bookStmt) SQLFreeHandle(SQL_HANDLE_STMT, bookStmt);

    // Авторы и связи с книгами.
    std::map<std::string, int> authorIds;
    SQLHSTMT linkStmt = SQL_NULL_HSTMT;
    if (ok && !SQL_SUCCEEDED(SQLAllocHandle(SQL_HANDLE_STMT, dbc, &linkStmt))) {
        error = diagnostics(SQL_HANDLE_DBC, dbc);
        ok = false;
    }
    const wchar_t* insertLink = L"INSERT INTO dbo.book_authors (book_id, author_id, author_order) VALUES (?, ?, ?)";
    if (ok && !SQL_SUCCEEDED(SQLPrepareW(linkStmt, const_cast<SQLWCHAR*>(insertLink), SQL_NTS))) {
        error = diagnostics(SQL_HANDLE_STMT, linkStmt);
        ok = false;
    }
    if (ok) {
        const wchar_t* authorInsertSql =
            L"IF NOT EXISTS (SELECT 1 FROM dbo.authors WHERE full_name = ?) INSERT INTO dbo.authors (full_name) VALUES (?)";
        const wchar_t* authorSelectSql = L"SELECT id FROM dbo.authors WHERE full_name = ?";
        for (const auto& book : books) {
            if (!ok) break;
            int order = 0;
            for (const auto& author : book.authors) {
                if (!ok) break;
                ++order;
                const std::string key = foldCase(author);
                int authorId = 0;
                auto cached = authorIds.find(key);
                if (cached == authorIds.end()) {
                    if (!ensureId(authorInsertSql, authorSelectSql, author, authorId, error)) {
                        ok = false;
                        break;
                    }
                    authorIds[key] = authorId;
                } else {
                    authorId = cached->second;
                }
                SQLINTEGER bookId = book.id;
                SQLINTEGER authorValue = authorId;
                SQLINTEGER authorOrder = order;
                SQLLEN bookIdInd = 0, authorValueInd = 0, authorOrderInd = 0;
                SQLFreeStmt(linkStmt, SQL_RESET_PARAMS);
                bindInt(linkStmt, 1, &bookId, bookIdInd);
                bindInt(linkStmt, 2, &authorValue, authorValueInd);
                bindInt(linkStmt, 3, &authorOrder, authorOrderInd);
                if (!SQL_SUCCEEDED(SQLExecute(linkStmt))) {
                    error = diagnostics(SQL_HANDLE_STMT, linkStmt);
                    ok = false;
                }
            }
        }
    }
    if (linkStmt) SQLFreeHandle(SQL_HANDLE_STMT, linkStmt);

    // Экземпляры: кто принёс (donors), кто принял (users), состояние.
    std::map<std::string, int> donorIds;
    SQLHSTMT copyStmt = SQL_NULL_HSTMT;
    if (ok && !SQL_SUCCEEDED(SQLAllocHandle(SQL_HANDLE_STMT, dbc, &copyStmt))) {
        error = diagnostics(SQL_HANDLE_DBC, dbc);
        ok = false;
    }
    const wchar_t* insertCopy =
        L"INSERT INTO dbo.book_copies (inventory, book_id, condition_code, donor_id, received_by, received_on) "
        L"VALUES (?, ?, ?, ?, ?, ?)";
    if (ok && !SQL_SUCCEEDED(SQLPrepareW(copyStmt, const_cast<SQLWCHAR*>(insertCopy), SQL_NTS))) {
        error = diagnostics(SQL_HANDLE_STMT, copyStmt);
        ok = false;
    }
    if (ok) {
        const wchar_t* donorInsertSql =
            L"IF NOT EXISTS (SELECT 1 FROM dbo.donors WHERE full_name = ?) INSERT INTO dbo.donors (full_name) VALUES (?)";
        const wchar_t* donorSelectSql = L"SELECT id FROM dbo.donors WHERE full_name = ?";
        for (const auto& book : books) {
            if (!ok) break;
            for (const auto& copy : book.copies) {
                if (!ok) break;
                int donorId = 0;
                SQLLEN donorInd = SQL_NULL_DATA;
                if (!copy.donor.empty()) {
                    const std::string key = foldCase(copy.donor);
                    auto cached = donorIds.find(key);
                    if (cached == donorIds.end()) {
                        if (!ensureId(donorInsertSql, donorSelectSql, copy.donor, donorId, error)) {
                            ok = false;
                            break;
                        }
                        donorIds[key] = donorId;
                    } else {
                        donorId = cached->second;
                    }
                    donorInd = 0;
                }
                int staffId = 0;
                SQLLEN staffInd = SQL_NULL_DATA;
                if (!copy.receivedBy.empty()) {
                    auto found = userIds.find(foldCase(copy.receivedBy));
                    if (found != userIds.end()) {
                        staffId = found->second;
                        staffInd = 0;
                    }
                }
                SQLINTEGER bookId = book.id;
                SQLINTEGER donorValue = donorId;
                SQLINTEGER staffValue = staffId;
                std::wstring inventory = utf8ToWide(copy.inventory);
                std::wstring condition = utf8ToWide(copy.condition);
                std::wstring receivedOn = utf8ToWide(copy.receivedOn);
                SQLLEN inventoryInd = 0, bookIdInd = 0, conditionInd = 0;
                SQLLEN receivedOnInd = 0;
                SQLFreeStmt(copyStmt, SQL_RESET_PARAMS);
                bindText(copyStmt, 1, inventory, inventoryInd);
                bindInt(copyStmt, 2, &bookId, bookIdInd);
                bindText(copyStmt, 3, condition, conditionInd);
                bindInt(copyStmt, 4, &donorValue, donorInd);
                bindInt(copyStmt, 5, &staffValue, staffInd);
                bindText(copyStmt, 6, receivedOn, receivedOnInd);
                if (!SQL_SUCCEEDED(SQLExecute(copyStmt))) {
                    error = diagnostics(SQL_HANDLE_STMT, copyStmt);
                    ok = false;
                }
            }
        }
    }
    if (copyStmt) SQLFreeHandle(SQL_HANDLE_STMT, copyStmt);

    SQLEndTran(SQL_HANDLE_DBC, dbc, ok ? SQL_COMMIT : SQL_ROLLBACK);
    SQLSetConnectAttr(dbc, SQL_ATTR_AUTOCOMMIT, reinterpret_cast<SQLPOINTER>(SQL_AUTOCOMMIT_ON), 0);
    return ok;
}
