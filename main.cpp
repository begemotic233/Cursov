#include "Catalog.h"
#include "Condition.h"
#include "Console.h"
#include "Database.h"
#include "Text.h"
#include "User.h"

#include <ctime>
#include <iomanip>
#include <set>
#include <sstream>
#include <string>
#include <vector>

namespace {

constexpr const char* kPermAddBook = "catalog.add";
constexpr const char* kPermEditBook = "catalog.edit";
constexpr const char* kPermDeleteBook = "catalog.delete";
constexpr const char* kPermCopies = "copies.manage";
constexpr const char* kPermSave = "catalog.save";
constexpr const char* kPermAnalytics = "analytics.view";

bool can(const std::set<std::string>& permissions, const char* name) {
    return permissions.count(name) != 0;
}

std::string money(double value) {
    std::ostringstream out;
    out.setf(std::ios::fixed);
    out << std::setprecision(2) << value;
    return out.str();
}

bool parseInt(const std::string& text, int& value) {
    std::string s = trim(text);
    if (s.empty()) return false;
    try {
        std::size_t used = 0;
        long parsed = std::stol(s, &used);
        if (used != s.size()) return false;
        value = static_cast<int>(parsed);
        return true;
    } catch (...) {
        return false;
    }
}

bool parsePrice(const std::string& text, double& value) {
    std::string s = trim(text);
    for (char& ch : s) {
        if (ch == ',') ch = '.';
    }
    if (s.empty()) return false;
    try {
        std::size_t used = 0;
        value = std::stod(s, &used);
        return used == s.size();
    } catch (...) {
        return false;
    }
}

std::string todayText() {
    std::time_t now = std::time(nullptr);
    std::tm* local = std::localtime(&now);
    if (!local) return "1970-01-01";
    char buffer[16] = {};
    std::strftime(buffer, sizeof(buffer), "%Y-%m-%d", local);
    return buffer;
}

void printBook(const Book& book) {
    printLine("[" + std::to_string(book.id) + "] " + book.title +
              " — " + authorsText(book) +
              ", " + book.genre +
              ", " + std::to_string(book.year) +
              ", экз.: " + std::to_string(book.copies.size()) +
              ", цена: " + money(book.price));
}

void printBookDetails(const Catalog& catalog, const Book& book) {
    printLine("[" + std::to_string(book.id) + "] " + book.title);
    printLine("Авторы: " + (book.authors.empty() ? std::string("не указаны") : authorsText(book)));
    printLine("Жанр: " + book.genre + ", год: " + std::to_string(book.year) +
              ", цена: " + money(book.price));
    printLine("Экземпляры: " + std::to_string(book.copies.size()) +
              ", доступно: " + std::to_string(catalog.availableCopies(book)));
    if (book.copies.empty()) {
        printLine("  экземпляров нет");
        return;
    }
    for (const auto& copy : book.copies) {
        printLine("  " + copy.inventory + " — " + conditionTitle(copy.condition) +
                  ", принёс: " + (copy.donor.empty() ? "неизвестно" : copy.donor) +
                  ", принял: " + (copy.receivedBy.empty() ? "не указан" : copy.receivedBy) +
                  ", " + copy.receivedOn);
    }
}

void printBooks(const std::vector<Book>& books, const std::string& emptyText) {
    if (books.empty()) {
        printLine(emptyText);
        return;
    }
    for (const auto& book : books) printBook(book);
}

std::string header(const Database& db, const Catalog& catalog, const User* user) {
    std::string text = "Каталог библиотеки";
    if (user) {
        text += "  |  " + user->fullName + " (" + user->role + ")";
    } else if (!db.connected()) {
        text += "  |  локальный режим, без ролей";
    }
    if (db.connected()) text += "  |  SQL Server: " + db.target();
    else text += "  |  SQL Server: нет соединения";
    if (catalog.dirty) text += "  |  есть несохранённые изменения";
    return text;
}

int chooseUser(const std::vector<User>& users) {
    std::vector<std::pair<int, std::string>> items;
    for (int i = 0; i < static_cast<int>(users.size()); ++i) {
        items.push_back({i + 1, users[i].fullName + " — " + users[i].role + " (" + users[i].login + ")"});
    }
    items.push_back({0, "Выход"});
    return showMenu("Вход в систему: выберите пользователя", items);
}

std::vector<std::string> askAuthors(const std::vector<std::string>* current) {
    std::string hint = current ? joinAuthors(*current) : "";
    std::string entered = readLine(current
        ? "Авторы (через запятую) [" + hint + "], Enter — оставить: "
        : "Авторы (через запятую): ");
    if (entered.empty() && current) return *current;
    return splitList(entered, ',');
}

Book askBook(const Book* current) {
    Book book = current ? *current : Book{};
    while (true) {
        std::string idText = readLine(current
            ? "ID [" + std::to_string(current->id) + "], Enter — оставить: "
            : "ID: ");
        if (idText.empty() && current) break;
        int id = 0;
        if (parseInt(idText, id)) {
            book.id = id;
            break;
        }
        printLine("ID должен быть целым числом.");
    }

    auto askText = [&](const std::string& label, std::string& field) {
        std::string entered = readLine(current ? label + " [" + field + "], Enter — оставить: " : label + ": ");
        if (!entered.empty() || !current) field = entered;
    };
    askText("Название", book.title);
    book.authors = askAuthors(current ? &current->authors : nullptr);
    askText("Жанр", book.genre);

    while (true) {
        std::string yearText = readLine(current
            ? "Год [" + std::to_string(current->year) + "], Enter — оставить: "
            : "Год издания: ");
        if (yearText.empty() && current) break;
        int year = 0;
        if (parseInt(yearText, year)) {
            book.year = year;
            break;
        }
        printLine("Год должен быть целым числом.");
    }

    while (true) {
        std::string priceText = readLine(current
            ? "Цена [" + money(current->price) + "], Enter — оставить: "
            : "Цена: ");
        if (priceText.empty() && current) break;
        double price = 0.0;
        if (parsePrice(priceText, price)) {
            book.price = price;
            break;
        }
        printLine("Цена должна быть числом.");
    }
    return book;
}

const ConditionPolicy* chooseCondition(const std::string& current) {
    const auto& all = ConditionRegistry::all();
    std::vector<std::pair<int, std::string>> items;
    for (int i = 0; i < static_cast<int>(all.size()); ++i) {
        items.push_back({i + 1, all[i]->title() + " (" + all[i]->code() + ")"});
    }
    const ConditionPolicy* currentPolicy = ConditionRegistry::byCode(current);
    std::string title = "Состояние экземпляра";
    if (currentPolicy) title += " (сейчас: " + currentPolicy->title() + ")";
    int choice = showMenu(title, items);
    if (choice == 0) return currentPolicy;
    return all[choice - 1];
}

std::string chooseStaff(const std::vector<User>& users, const std::string& current) {
    if (users.empty()) {
        std::string prompt = current.empty()
            ? "Кто принял книгу (Enter — не указывать): "
            : "Кто принял [" + current + "], Enter — оставить: ";
        std::string entered = readLine(prompt);
        return entered.empty() ? current : entered;
    }
    std::vector<std::pair<int, std::string>> items;
    for (int i = 0; i < static_cast<int>(users.size()); ++i) {
        items.push_back({i + 1, users[i].fullName + " (" + users[i].role + ")"});
    }
    int choice = showMenu("Кто принял книгу" + (current.empty() ? "" : " (сейчас: " + current + ")"), items);
    if (choice == 0) return current;
    return users[choice - 1].fullName;
}

BookCopy askCopy(const Catalog& catalog, const Book& book, const std::vector<User>& users, const BookCopy* current) {
    BookCopy copy;
    copy.condition = ConditionRegistry::defaultCode();
    copy.receivedOn = todayText();
    if (current) copy = *current;

    const std::string suggested = current ? copy.inventory : catalog.nextInventory(book.id, book.copies);
    std::string inventoryText = readLine("Инвентарный номер [" + suggested + "], Enter — оставить: ");
    copy.inventory = inventoryText.empty() ? suggested : inventoryText;

    const ConditionPolicy* policy = chooseCondition(copy.condition);
    if (policy) copy.condition = policy->code();

    std::string donorPrompt = copy.donor.empty()
        ? "Кто принёс книгу (Enter — неизвестно): "
        : "Кто принёс [" + copy.donor + "], Enter — оставить: ";
    std::string donorText = readLine(donorPrompt);
    if (!donorText.empty()) copy.donor = donorText;

    copy.receivedBy = chooseStaff(users, copy.receivedBy);

    std::string dateDefault = copy.receivedOn.empty() ? todayText() : copy.receivedOn;
    std::string dateText = readLine("Дата поступления [" + dateDefault + "], Enter — оставить: ");
    copy.receivedOn = dateText.empty() ? dateDefault : dateText;
    return copy;
}

void addBook(Catalog& catalog, const std::vector<User>& users) {
    clearScreen();
    printLine("Новая книга. Поля: ID, название, авторы, жанр, год, цена.");
    printLine("");
    Book book = askBook(nullptr);
    if (askYesNo("Добавить экземпляры сразу? (д/н): ")) {
        int count = 0;
        while (true) {
            if (parseInt(readLine("Сколько экземпляров: "), count) && count > 0) break;
            printLine("Введите целое число больше 0.");
        }
        BookCopy sample;
        const ConditionPolicy* policy = chooseCondition(ConditionRegistry::defaultCode());
        sample.condition = policy ? policy->code() : ConditionRegistry::defaultCode();
        sample.donor = readLine("Кто принёс книгу (Enter — неизвестно): ");
        sample.receivedBy = chooseStaff(users, "");
        std::string dateDefault = todayText();
        std::string dateText = readLine("Дата поступления [" + dateDefault + "], Enter — оставить: ");
        sample.receivedOn = dateText.empty() ? dateDefault : dateText;
        for (int i = 0; i < count; ++i) {
            BookCopy copy = sample;
            copy.inventory = catalog.nextInventory(book.id, book.copies);
            book.copies.push_back(copy);
        }
    }
    std::string error = catalog.add(book);
    printLine(error.empty() ? "Книга добавлена." : error);
    waitEnter();
}

void editBook(Catalog& catalog) {
    clearScreen();
    printLine("Редактирование. Пустой ввод оставляет поле без изменений.");
    printLine("");
    int id = 0;
    if (!parseInt(readLine("ID книги: "), id)) {
        printLine("ID должен быть целым числом.");
        waitEnter();
        return;
    }
    const Book* found = catalog.findById(id);
    if (!found) {
        printLine("Книга с таким ID не найдена.");
        waitEnter();
        return;
    }
    printLine("");
    printBook(*found);
    printLine("");
    Book edited = askBook(found);
    std::string error = catalog.update(id, edited);
    printLine(error.empty() ? "Запись обновлена." : error);
    waitEnter();
}

void deleteBook(Catalog& catalog) {
    while (true) {
        int choice = showMenu("Удаление книги", {
            {1, "По ID"},
            {2, "По названию"},
            {0, "Назад"}
        });
        if (choice == 0) return;
        clearScreen();
        if (choice == 1) {
            int id = 0;
            if (!parseInt(readLine("ID: "), id)) {
                printLine("ID должен быть целым числом.");
            } else if (catalog.removeById(id)) {
                printLine("Книга удалена.");
            } else {
                printLine("Книга с таким ID не найдена.");
            }
        } else {
            int removed = catalog.removeByTitle(readLine("Название: "));
            if (removed == 0) printLine("Книга с таким названием не найдена.");
            else printLine("Удалено записей: " + std::to_string(removed) + ".");
        }
        waitEnter();
    }
}

void showAll(const Catalog& catalog) {
    clearScreen();
    printLine("Все книги");
    printLine("");
    printBooks(catalog.books, "Каталог пуст.");
    waitEnter();
}

void searchMenu(const Catalog& catalog) {
    while (true) {
        int choice = showMenu("Поиск и фильтрация", {
            {1, "По названию (часть строки, без учёта регистра)"},
            {2, "По автору (любой из авторов)"},
            {3, "По жанру"},
            {4, "По диапазону года издания"},
            {5, "По диапазону цены"},
            {0, "Назад"}
        });
        if (choice == 0) return;
        clearScreen();
        if (choice == 1) {
            auto found = catalog.searchByTitle(readLine("Часть названия: "));
            printBooks(found, "Ничего не найдено.");
        } else if (choice == 2) {
            auto found = catalog.searchByAuthor(readLine("Автор: "));
            printBooks(found, "Ничего не найдено.");
        } else if (choice == 3) {
            auto found = catalog.searchByGenre(readLine("Жанр: "));
            printBooks(found, "Ничего не найдено.");
        } else if (choice == 4) {
            int from = 0;
            int to = 0;
            if (!parseInt(readLine("Год от: "), from) || !parseInt(readLine("Год до: "), to)) {
                printLine("Границы диапазона должны быть целыми числами.");
            } else if (from > to) {
                printLine("Начало диапазона больше конца.");
            } else {
                printBooks(catalog.filterByYear(from, to), "Ничего не найдено.");
            }
        } else {
            double from = 0.0;
            double to = 0.0;
            if (!parsePrice(readLine("Цена от: "), from) || !parsePrice(readLine("Цена до: "), to)) {
                printLine("Границы диапазона должны быть числами.");
            } else if (from > to) {
                printLine("Начало диапазона больше конца.");
            } else {
                printBooks(catalog.filterByPrice(from, to), "Ничего не найдено.");
            }
        }
        waitEnter();
    }
}

void sortMenu(Catalog& catalog) {
    while (true) {
        int choice = showMenu("Сортировка каталога", {
            {1, "По году издания (возрастание)"},
            {2, "По году издания (убывание)"},
            {3, "По количеству экземпляров"},
            {4, "По алфавиту (название)"},
            {5, "По алфавиту (автор)"},
            {0, "Назад"}
        });
        if (choice == 0) return;
        clearScreen();
        if (catalog.books.empty()) {
            printLine("Каталог пуст.");
            waitEnter();
            continue;
        }
        if (choice == 1) catalog.sortByYear(true);
        else if (choice == 2) catalog.sortByYear(false);
        else if (choice == 3) catalog.sortByCopies();
        else if (choice == 4) catalog.sortByTitle();
        else catalog.sortByAuthor();
        printLine("Порядок каталога изменён.");
        printLine("");
        printBooks(catalog.books, "Каталог пуст.");
        waitEnter();
    }
}

void copiesMenu(Catalog& catalog, const std::vector<User>& users) {
    while (true) {
        int choice = showMenu("Экземпляры книги (состояние, кто принёс)", {
            {1, "Показать экземпляры книги"},
            {2, "Добавить экземпляр"},
            {3, "Изменить состояние / данные экземпляра"},
            {4, "Удалить экземпляр"},
            {0, "Назад"}
        });
        if (choice == 0) return;
        clearScreen();

        int bookId = 0;
        if (!parseInt(readLine("ID книги: "), bookId)) {
            printLine("ID должен быть целым числом.");
            waitEnter();
            continue;
        }
        const Book* book = catalog.findById(bookId);
        if (!book) {
            printLine("Книга с таким ID не найдена.");
            waitEnter();
            continue;
        }

        if (choice == 1) {
            printBookDetails(catalog, *book);
        } else if (choice == 2) {
            BookCopy copy = askCopy(catalog, *book, users, nullptr);
            std::string error = catalog.addCopy(bookId, copy);
            printLine(error.empty() ? "Экземпляр добавлен." : error);
        } else if (choice == 3) {
            printBookDetails(catalog, *book);
            std::string inventory = readLine("Инвентарный номер: ");
            const BookCopy* found = catalog.findCopy(bookId, inventory);
            if (!found) {
                printLine("Экземпляр с таким номером не найден.");
                waitEnter();
                continue;
            }
            BookCopy edited = askCopy(catalog, *book, users, found);
            std::string error = catalog.updateCopy(bookId, inventory, edited);
            printLine(error.empty() ? "Экземпляр обновлён." : error);
        } else {
            printBookDetails(catalog, *book);
            std::string inventory = readLine("Инвентарный номер для удаления: ");
            if (askYesNo("Удалить экземпляр " + inventory + "? (д/н): ")) {
                printLine(catalog.removeCopy(bookId, inventory) ? "Экземпляр удалён." : "Экземпляр не найден.");
            } else {
                printLine("Удаление отменено.");
            }
        }
        waitEnter();
    }
}

void analyticsMenu(const Catalog& catalog) {
    while (true) {
        int choice = showMenu("Аналитика", {
            {1, "Стоимость фонда (с учётом состояния)"},
            {2, "Книги к пополнению (доступных 2 и меньше)"},
            {3, "Топ-3 самых дорогих"},
            {4, "Сводка по состояниям экземпляров"},
            {0, "Назад"}
        });
        if (choice == 0) return;
        clearScreen();
        if (choice == 1) {
            printLine("Стоимость фонда: " + money(catalog.totalValue()));
            printLine("Цена умножается на коэффициент состояния каждого экземпляра.");
        } else if (choice == 2) {
            printBooks(catalog.needRestock(), "Нет книг с доступным остатком 2 и меньше.");
        } else if (choice == 3) {
            printBooks(catalog.topExpensive(3), "Каталог пуст.");
        } else {
            for (const ConditionPolicy* policy : ConditionRegistry::all()) {
                int count = 0;
                for (const auto& book : catalog.books) {
                    for (const auto& copy : book.copies) {
                        if (ConditionRegistry::byCode(copy.condition) == policy) ++count;
                    }
                }
                printLine(policy->title() + " (" + policy->code() + "): " + std::to_string(count) + " экз.");
            }
        }
        waitEnter();
    }
}

bool saveChanges(Database& db, Catalog& catalog, bool loadedFromServer) {
    clearScreen();
    if (!catalog.dirty) {
        printLine("Нет изменений для сохранения.");
        waitEnter();
        return true;
    }
    if (!loadedFromServer) {
        printLine("При запуске данные из SQL Server не загружались.");
        printLine("Сохранение заменит содержимое таблиц текущим каталогом.");
        if (!askYesNo("Продолжить? (д/н): ")) {
            printLine("Сохранение отменено.");
            waitEnter();
            return false;
        }
    }
    std::string error;
    if (!db.connected() && !db.connect("db.ini", error)) {
        printLine(error);
        waitEnter();
        return false;
    }
    if (!db.save(catalog.books, error)) {
        printLine("Сохранить не удалось. " + error);
        waitEnter();
        return false;
    }
    catalog.dirty = false;
    printLine("Изменения записаны в SQL Server (" + db.target() + ").");
    waitEnter();
    return true;
}

}  // namespace

int main() {
    setupConsole();

    Database database;
    Catalog catalog;
    std::vector<User> users;
    std::set<std::string> permissions;
    const User* activeUser = nullptr;
    bool loadedFromServer = false;
    std::string error;
    if (database.connect("db.ini", error)) {
        std::string userError;
        if (!database.loadUsers(users, userError)) {
            clearScreen();
            printLine("Не удалось прочитать пользователей.");
            printLine(userError);
            waitEnter();
        }
        if (database.load(catalog.books, error)) {
            loadedFromServer = true;
        } else {
            clearScreen();
            printLine("Соединение есть, но прочитать каталог не удалось.");
            printLine(error);
            waitEnter();
        }
        if (!users.empty()) {
            int login = chooseUser(users);
            if (login == 0) {
                database.disconnect();
                return 0;
            }
            activeUser = &users[login - 1];
            std::string permissionError;
            if (!database.loadPermissions(activeUser->id, permissions, permissionError)) {
                clearScreen();
                printLine("Не удалось загрузить права роли.");
                printLine(permissionError);
                printLine("Будет доступен только просмотр каталога.");
                waitEnter();
            }
        }
    } else {
        clearScreen();
        printLine(error);
        printLine("Можно работать с каталогом в памяти и сохранить его командой меню, когда SQL Server будет доступен.");
        printLine("Параметры подключения: файл db.ini.");
        waitEnter();
        permissions = {kPermAddBook, kPermEditBook, kPermDeleteBook, kPermCopies, kPermSave, kPermAnalytics};
    }

    while (true) {
        std::vector<std::pair<int, std::string>> items;
        if (can(permissions, kPermAddBook)) items.push_back({1, "Добавить книгу"});
        if (can(permissions, kPermEditBook)) items.push_back({2, "Редактировать книгу"});
        if (can(permissions, kPermDeleteBook)) items.push_back({3, "Удалить книгу"});
        items.push_back({4, "Показать все книги"});
        items.push_back({5, "Поиск и фильтрация"});
        items.push_back({6, "Сортировка"});
        if (can(permissions, kPermCopies)) items.push_back({7, "Экземпляры книги"});
        if (can(permissions, kPermAnalytics)) items.push_back({8, "Аналитика"});
        if (can(permissions, kPermSave)) items.push_back({9, "Сохранить изменения"});
        items.push_back({0, "Выход"});

        int choice = showMenu(header(database, catalog, activeUser), items);

        if (choice == 1) addBook(catalog, users);
        else if (choice == 2) editBook(catalog);
        else if (choice == 3) deleteBook(catalog);
        else if (choice == 4) showAll(catalog);
        else if (choice == 5) searchMenu(catalog);
        else if (choice == 6) sortMenu(catalog);
        else if (choice == 7) copiesMenu(catalog, users);
        else if (choice == 8) analyticsMenu(catalog);
        else if (choice == 9) {
            if (saveChanges(database, catalog, loadedFromServer)) loadedFromServer = database.connected();
        } else if (choice == 0) {
            if (catalog.dirty && can(permissions, kPermSave)) {
                clearScreen();
                printLine("Есть несохранённые изменения.");
                if (askYesNo("Сохранить перед выходом? (д/н): ")) {
                    saveChanges(database, catalog, loadedFromServer);
                    if (catalog.dirty) continue;
                }
            }
            break;
        }
    }

    database.disconnect();
    return 0;
}
