IF NOT EXISTS (SELECT 1 FROM dbo.books)
BEGIN
    INSERT INTO dbo.authors (full_name)
    SELECT v.full_name
    FROM (VALUES
        (N'Михаил Булгаков'),
        (N'Фёдор Достоевский'),
        (N'Стивен Хокинг'),
        (N'Джон Толкин'),
        (N'Джордж Оруэлл'),
        (N'Аркадий Стругацкий'),
        (N'Борис Стругацкий')
    ) AS v(full_name)
    WHERE NOT EXISTS (SELECT 1 FROM dbo.authors a WHERE a.full_name = v.full_name);

    INSERT INTO dbo.books (id, title, genre, [year], price) VALUES
        (1, N'Мастер и Маргарита',              N'Роман',       1967,  890.00),
        (2, N'Преступление и наказание',        N'Роман',       1866,  750.50),
        (3, N'Краткая история времени',         N'Научпоп',     1988, 1200.00),
        (4, N'Хоббит',                          N'Фэнтези',     1937,  980.00),
        (5, N'1984',                            N'Антиутопия',  1949,  640.00),
        (6, N'Понедельник начинается в субботу', N'Фантастика', 1965,  700.00);

    INSERT INTO dbo.book_authors (book_id, author_id, author_order)
    SELECT b.id, a.id, v.author_order
    FROM (VALUES
        (N'Мастер и Маргарита',              N'Михаил Булгаков',   1),
        (N'Преступление и наказание',        N'Фёдор Достоевский', 1),
        (N'Краткая история времени',         N'Стивен Хокинг',     1),
        (N'Хоббит',                          N'Джон Толкин',       1),
        (N'1984',                            N'Джордж Оруэлл',     1),
        (N'Понедельник начинается в субботу', N'Аркадий Стругацкий', 1),
        (N'Понедельник начинается в субботу', N'Борис Стругацкий',   2)
    ) AS v(title, author, author_order)
    JOIN dbo.books b ON b.title = v.title
    JOIN dbo.authors a ON a.full_name = v.author;

    INSERT INTO dbo.donors (full_name, contact) VALUES
        (N'Петрова Мария',            N'+7 900 100-10-10'),
        (N'Смирнов Алексей',          N'+7 900 200-20-20'),
        (N'Иванов Пётр',              NULL),
        (N'Фонд «Книга — детям»',     N'fond@example.org');

    INSERT INTO dbo.book_copies (inventory, book_id, condition_code, donor_id, received_by, received_on)
    SELECT v.inventory, b.id, v.condition_code, d.id, u.id, v.received_on
    FROM (VALUES
        (N'INV-1-1', N'Мастер и Маргарита',              N'GOOD', N'Петрова Мария',        N'Петров Игорь',   '2025-09-01'),
        (N'INV-1-2', N'Мастер и Маргарита',              N'WORN', N'Смирнов Алексей',      N'Смирнова Ольга', '2025-09-15'),
        (N'INV-2-1', N'Преступление и наказание',        N'GOOD', N'Петрова Мария',        N'Петров Игорь',   '2025-10-02'),
        (N'INV-2-2', N'Преступление и наказание',        N'BAD',  NULL,                    N'Смирнова Ольга', '2025-10-05'),
        (N'INV-3-1', N'Краткая история времени',         N'NEW',  N'Фонд «Книга — детям»', N'Петров Игорь',   '2025-10-10'),
        (N'INV-4-1', N'Хоббит',                          N'GOOD', N'Фонд «Книга — детям»', N'Смирнова Ольга', '2025-08-20'),
        (N'INV-4-2', N'Хоббит',                          N'GOOD', N'Иванов Пётр',          N'Петров Игорь',   '2025-08-21'),
        (N'INV-5-1', N'1984',                            N'LOST', N'Иванов Пётр',          N'Петров Игорь',   '2025-07-01'),
        (N'INV-6-1', N'Понедельник начинается в субботу', N'GOOD', N'Петрова Мария',       N'Петров Игорь',   '2025-11-01'),
        (N'INV-6-2', N'Понедельник начинается в субботу', N'WORN', NULL,                    N'Смирнова Ольга', '2025-11-02')
    ) AS v(inventory, title, condition_code, donor, staff, received_on)
    JOIN dbo.books b ON b.title = v.title
    LEFT JOIN dbo.donors d ON d.full_name = v.donor
    LEFT JOIN dbo.users u ON u.full_name = v.staff;
END
GO
