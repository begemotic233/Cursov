#pragma once

#include <string>
#include <vector>

// Экземпляр книги: у каждой физической копии своё состояние и свой даритель.
struct BookCopy {
    std::string inventory;    // инвентарный номер
    std::string condition;    // код состояния: NEW, GOOD, WORN, BAD, LOST
    std::string donor;        // кто принёс книгу
    std::string receivedBy;   // кто из сотрудников принял
    std::string receivedOn;   // дата поступления, ГГГГ-ММ-ДД
};

struct Book {
    int id = 0;
    std::string title;
    std::vector<std::string> authors;
    std::string genre;
    int year = 0;
    double price = 0.0;
    std::vector<BookCopy> copies;
};

inline std::string joinAuthors(const std::vector<std::string>& authors) {
    std::string text;
    for (const auto& author : authors) {
        if (!text.empty()) text += ", ";
        text += author;
    }
    return text;
}

inline std::string authorsText(const Book& book) {
    return joinAuthors(book.authors);
}
