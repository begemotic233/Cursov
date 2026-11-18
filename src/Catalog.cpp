#include "Catalog.h"
#include "Condition.h"
#include "Text.h"

#include <algorithm>

namespace {

bool looksLikeDate(const std::string& text) {
    if (text.size() != 10) return false;
    for (std::size_t i = 0; i < text.size(); ++i) {
        if (i == 4 || i == 7) {
            if (text[i] != '-') return false;
        } else if (text[i] < '0' || text[i] > '9') {
            return false;
        }
    }
    return true;
}

Book cleanBook(const Book& book) {
    Book clean = book;
    clean.title = trim(clean.title);
    clean.genre = trim(clean.genre);
    for (auto& author : clean.authors) author = trim(author);
    for (auto& copy : clean.copies) {
        copy.inventory = trim(copy.inventory);
        copy.condition = trim(copy.condition);
        copy.donor = trim(copy.donor);
        copy.receivedBy = trim(copy.receivedBy);
        copy.receivedOn = trim(copy.receivedOn);
    }
    return clean;
}

std::string validateCopyFields(const BookCopy& copy) {
    if (copy.inventory.empty()) return "Инвентарный номер не может быть пустым.";
    if (copy.inventory.size() > 30) return "Инвентарный номер длиннее 30 символов.";
    if (!ConditionRegistry::byCode(copy.condition)) {
        return "Неизвестное состояние экземпляра: " + copy.condition + ".";
    }
    if (!looksLikeDate(copy.receivedOn)) {
        return "Дата поступления должна быть в формате ГГГГ-ММ-ДД.";
    }
    return "";
}

// Проверяет уникальность инвентарного номера по всему каталогу.
// skipBookId < 0 — ничего не пропускать; skipInventory задан — пропустить старую запись этой копии.
std::string validateInventoryUnique(const BookCopy& copy, const std::vector<Book>& books,
                                   int skipBookId, const std::string* skipInventory) {
    for (const auto& book : books) {
        for (const auto& other : book.copies) {
            if (book.id == skipBookId) {
                if (!skipInventory) continue;
                if (equalsFold(other.inventory, *skipInventory)) continue;
            }
            if (equalsFold(other.inventory, copy.inventory)) {
                return "Инвентарный номер " + copy.inventory + " уже используется.";
            }
        }
    }
    return "";
}

}  // namespace

const Book* Catalog::findById(int id) const {
    for (const auto& book : books) {
        if (book.id == id) return &book;
    }
    return nullptr;
}

const BookCopy* Catalog::findCopy(int bookId, const std::string& inventory) const {
    const Book* book = findById(bookId);
    if (!book) return nullptr;
    const std::string key = trim(inventory);
    for (const auto& copy : book->copies) {
        if (equalsFold(copy.inventory, key)) return &copy;
    }
    return nullptr;
}

std::string Catalog::validate(const Book& book, const std::vector<Book>& existing, const int* exceptId) {
    if (trim(book.title).empty()) return "Название не может быть пустым.";
    if (book.title.size() > 255) return "Название длиннее 255 символов.";
    if (book.authors.empty()) return "Укажите хотя бы одного автора.";
    if (book.authors.size() > 10) return "Слишком много авторов (максимум 10).";
    for (const auto& author : book.authors) {
        if (trim(author).empty()) return "Имя автора не может быть пустым.";
        if (author.size() > 150) return "Имя автора длиннее 150 символов.";
    }
    for (std::size_t i = 0; i < book.authors.size(); ++i) {
        for (std::size_t j = i + 1; j < book.authors.size(); ++j) {
            if (equalsFold(book.authors[i], book.authors[j])) {
                return "Автор " + trim(book.authors[i]) + " указан дважды.";
            }
        }
    }
    if (trim(book.genre).empty()) return "Жанр не может быть пустым.";
    if (book.genre.size() > 100) return "Жанр длиннее 100 символов.";
    if (book.year <= 0) return "Год издания должен быть больше 0.";
    if (book.price < 0.0) return "Цена не может быть отрицательной.";

    for (const auto& copy : book.copies) {
        std::string error = validateCopyFields(copy);
        if (!error.empty()) return error;
    }
    for (std::size_t i = 0; i < book.copies.size(); ++i) {
        for (std::size_t j = i + 1; j < book.copies.size(); ++j) {
            if (equalsFold(book.copies[i].inventory, book.copies[j].inventory)) {
                return "Инвентарные номера не должны повторяться.";
            }
        }
    }
    const int skipBook = exceptId ? *exceptId : -1;
    for (const auto& copy : book.copies) {
        std::string error = validateInventoryUnique(copy, existing, skipBook, nullptr);
        if (!error.empty()) return error;
    }

    for (const auto& other : existing) {
        if (exceptId && other.id == *exceptId) continue;
        if (other.id == book.id) return "Книга с таким ID уже есть.";
    }
    return "";
}

std::string Catalog::add(const Book& book) {
    Book clean = cleanBook(book);
    std::string error = validate(clean, books, nullptr);
    if (!error.empty()) return error;
    books.push_back(clean);
    dirty = true;
    return "";
}

std::string Catalog::update(int originalId, const Book& book) {
    auto it = std::find_if(books.begin(), books.end(), [originalId](const Book& item) {
        return item.id == originalId;
    });
    if (it == books.end()) return "Книга с таким ID не найдена.";

    Book clean = cleanBook(book);
    std::string error = validate(clean, books, &originalId);
    if (!error.empty()) return error;
    *it = clean;
    dirty = true;
    return "";
}

bool Catalog::removeById(int id) {
    auto it = std::find_if(books.begin(), books.end(), [id](const Book& item) {
        return item.id == id;
    });
    if (it == books.end()) return false;
    books.erase(it);
    dirty = true;
    return true;
}

int Catalog::removeByTitle(const std::string& title) {
    const std::string query = trim(title);
    if (query.empty()) return 0;
    const auto oldSize = books.size();
    books.erase(std::remove_if(books.begin(), books.end(), [&query](const Book& item) {
        return equalsFold(item.title, query);
    }), books.end());
    const int removed = static_cast<int>(oldSize - books.size());
    if (removed > 0) dirty = true;
    return removed;
}

std::string Catalog::addCopy(int bookId, const BookCopy& copy) {
    auto it = std::find_if(books.begin(), books.end(), [bookId](const Book& item) {
        return item.id == bookId;
    });
    if (it == books.end()) return "Книга с таким ID не найдена.";

    BookCopy clean = copy;
    clean.inventory = trim(clean.inventory);
    clean.condition = trim(clean.condition);
    clean.donor = trim(clean.donor);
    clean.receivedBy = trim(clean.receivedBy);
    clean.receivedOn = trim(clean.receivedOn);

    std::string error = validateCopyFields(clean);
    if (!error.empty()) return error;
    error = validateInventoryUnique(clean, books, -1, nullptr);
    if (!error.empty()) return error;

    it->copies.push_back(clean);
    dirty = true;
    return "";
}

std::string Catalog::updateCopy(int bookId, const std::string& inventory, const BookCopy& copy) {
    auto it = std::find_if(books.begin(), books.end(), [bookId](const Book& item) {
        return item.id == bookId;
    });
    if (it == books.end()) return "Книга с таким ID не найдена.";

    const std::string oldInventory = trim(inventory);
    auto copyIt = std::find_if(it->copies.begin(), it->copies.end(), [&oldInventory](const BookCopy& item) {
        return equalsFold(item.inventory, oldInventory);
    });
    if (copyIt == it->copies.end()) return "Экземпляр с таким инвентарным номером не найден.";

    BookCopy clean = copy;
    clean.inventory = trim(clean.inventory);
    clean.condition = trim(clean.condition);
    clean.donor = trim(clean.donor);
    clean.receivedBy = trim(clean.receivedBy);
    clean.receivedOn = trim(clean.receivedOn);

    std::string error = validateCopyFields(clean);
    if (!error.empty()) return error;
    error = validateInventoryUnique(clean, books, bookId, &oldInventory);
    if (!error.empty()) return error;

    *copyIt = clean;
    dirty = true;
    return "";
}

bool Catalog::removeCopy(int bookId, const std::string& inventory) {
    auto it = std::find_if(books.begin(), books.end(), [bookId](const Book& item) {
        return item.id == bookId;
    });
    if (it == books.end()) return false;
    const std::string key = trim(inventory);
    auto copyIt = std::find_if(it->copies.begin(), it->copies.end(), [&key](const BookCopy& item) {
        return equalsFold(item.inventory, key);
    });
    if (copyIt == it->copies.end()) return false;
    it->copies.erase(copyIt);
    dirty = true;
    return true;
}

std::string Catalog::nextInventory(int bookId, const std::vector<BookCopy>& extra) const {
    int number = 1;
    while (true) {
        std::string candidate = "INV-" + std::to_string(bookId) + "-" + std::to_string(number);
        bool used = false;
        for (const auto& book : books) {
            for (const auto& copy : book.copies) {
                if (equalsFold(copy.inventory, candidate)) {
                    used = true;
                    break;
                }
            }
            if (used) break;
        }
        if (!used) {
            for (const auto& copy : extra) {
                if (equalsFold(copy.inventory, candidate)) {
                    used = true;
                    break;
                }
            }
        }
        if (!used) return candidate;
        ++number;
    }
}

std::vector<Book> Catalog::searchByTitle(const std::string& query) const {
    const std::string part = trim(query);
    std::vector<Book> result;
    for (const auto& book : books) {
        if (containsFold(book.title, part)) result.push_back(book);
    }
    return result;
}

std::vector<Book> Catalog::searchByAuthor(const std::string& query) const {
    const std::string part = trim(query);
    std::vector<Book> result;
    for (const auto& book : books) {
        for (const auto& author : book.authors) {
            if (containsFold(author, part)) {
                result.push_back(book);
                break;
            }
        }
    }
    return result;
}

std::vector<Book> Catalog::searchByGenre(const std::string& query) const {
    const std::string part = trim(query);
    std::vector<Book> result;
    for (const auto& book : books) {
        if (containsFold(book.genre, part)) result.push_back(book);
    }
    return result;
}

std::vector<Book> Catalog::filterByYear(int from, int to) const {
    std::vector<Book> result;
    for (const auto& book : books) {
        if (book.year >= from && book.year <= to) result.push_back(book);
    }
    return result;
}

std::vector<Book> Catalog::filterByPrice(double from, double to) const {
    std::vector<Book> result;
    for (const auto& book : books) {
        if (book.price >= from && book.price <= to) result.push_back(book);
    }
    return result;
}

void Catalog::sortByYear(bool ascending) {
    std::stable_sort(books.begin(), books.end(), [ascending](const Book& a, const Book& b) {
        if (a.year != b.year) return ascending ? a.year < b.year : a.year > b.year;
        return a.id < b.id;
    });
}

void Catalog::sortByCopies() {
    std::stable_sort(books.begin(), books.end(), [](const Book& a, const Book& b) {
        if (a.copies.size() != b.copies.size()) return a.copies.size() < b.copies.size();
        return a.id < b.id;
    });
}

void Catalog::sortByTitle() {
    std::stable_sort(books.begin(), books.end(), [](const Book& a, const Book& b) {
        if (!equalsFold(a.title, b.title)) return lessText(a.title, b.title);
        return a.id < b.id;
    });
}

void Catalog::sortByAuthor() {
    std::stable_sort(books.begin(), books.end(), [](const Book& a, const Book& b) {
        if (!equalsFold(authorsText(a), authorsText(b))) return lessText(authorsText(a), authorsText(b));
        return a.id < b.id;
    });
}

double Catalog::totalValue() const {
    double sum = 0.0;
    for (const auto& book : books) {
        for (const auto& copy : book.copies) {
            sum += book.price * conditionFactor(copy.condition);
        }
    }
    return sum;
}

int Catalog::availableCopies(const Book& book) const {
    int count = 0;
    for (const auto& copy : book.copies) {
        if (conditionAvailable(copy.condition)) ++count;
    }
    return count;
}

std::vector<Book> Catalog::needRestock() const {
    std::vector<Book> result;
    for (const auto& book : books) {
        if (availableCopies(book) <= 2) result.push_back(book);
    }
    return result;
}

std::vector<Book> Catalog::topExpensive(int count) const {
    std::vector<Book> copy = books;
    std::stable_sort(copy.begin(), copy.end(), [](const Book& a, const Book& b) {
        if (a.price != b.price) return a.price > b.price;
        return a.id < b.id;
    });
    if (static_cast<int>(copy.size()) > count) copy.resize(count);
    return copy;
}
