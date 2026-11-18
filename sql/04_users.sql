IF NOT EXISTS (SELECT 1 FROM dbo.users WHERE login = N'admin')
INSERT INTO dbo.users (full_name, login, role_id) VALUES
    (N'Иванова Анна', N'admin', (SELECT id FROM dbo.roles WHERE name = N'Администратор'));
GO

IF NOT EXISTS (SELECT 1 FROM dbo.users WHERE login = N'librarian')
INSERT INTO dbo.users (full_name, login, role_id) VALUES
    (N'Петров Игорь', N'librarian', (SELECT id FROM dbo.roles WHERE name = N'Библиотекарь'));
GO

IF NOT EXISTS (SELECT 1 FROM dbo.users WHERE login = N'librarian2')
INSERT INTO dbo.users (full_name, login, role_id) VALUES
    (N'Смирнова Ольга', N'librarian2', (SELECT id FROM dbo.roles WHERE name = N'Библиотекарь'));
GO

IF NOT EXISTS (SELECT 1 FROM dbo.users WHERE login = N'reader')
INSERT INTO dbo.users (full_name, login, role_id) VALUES
    (N'Кузнецов Павел', N'reader', (SELECT id FROM dbo.roles WHERE name = N'Читатель'));
GO
