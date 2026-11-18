#pragma once

#include "Book.h"

#include <string>
#include <vector>

class Catalog {
public:
    std::vector<Book> books;
    bool dirty = false;

    const Book* findById(int id) const;
    const BookCopy* findCopy(int bookId, const std::string& inventory) const;

    std::string add(const Book& book);
    std::string update(int originalId, const Book& book);
    bool removeById(int id);
    int removeByTitle(const std::string& title);

    std::string addCopy(int bookId, const BookCopy& copy);
    std::string updateCopy(int bookId, const std::string& inventory, const BookCopy& copy);
    bool removeCopy(int bookId, const std::string& inventory);
    std::string nextInventory(int bookId, const std::vector<BookCopy>& extra) const;

    std::vector<Book> searchByTitle(const std::string& query) const;
    std::vector<Book> searchByAuthor(const std::string& query) const;
    std::vector<Book> searchByGenre(const std::string& query) const;
    std::vector<Book> filterByYear(int from, int to) const;
    std::vector<Book> filterByPrice(double from, double to) const;

    void sortByYear(bool ascending);
    void sortByCopies();
    void sortByTitle();
    void sortByAuthor();

    double totalValue() const;
    int availableCopies(const Book& book) const;
    std::vector<Book> needRestock() const;
    std::vector<Book> topExpensive(int count) const;

    static std::string validate(const Book& book, const std::vector<Book>& existing, const int* exceptId);
};
