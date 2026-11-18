IF OBJECT_ID(N'dbo.Books_legacy', N'U') IS NOT NULL
   AND NOT EXISTS (SELECT 1 FROM dbo.books)
INSERT INTO dbo.books (id, title, genre, [year], price)
SELECT id, title, genre, [year], price FROM dbo.Books_legacy;
GO

IF OBJECT_ID(N'dbo.Books_legacy', N'U') IS NOT NULL
INSERT INTO dbo.authors (full_name)
SELECT DISTINCT LTRIM(RTRIM(b.author))
FROM dbo.Books_legacy b
WHERE LTRIM(RTRIM(b.author)) <> N''
  AND NOT EXISTS (SELECT 1 FROM dbo.authors a WHERE a.full_name = LTRIM(RTRIM(b.author)));
GO

IF OBJECT_ID(N'dbo.Books_legacy', N'U') IS NOT NULL
INSERT INTO dbo.book_authors (book_id, author_id, author_order)
SELECT b.id, a.id, 1
FROM dbo.Books_legacy b
JOIN dbo.authors a ON a.full_name = LTRIM(RTRIM(b.author))
WHERE b.id IN (SELECT id FROM dbo.books)
  AND NOT EXISTS (SELECT 1 FROM dbo.book_authors ba WHERE ba.book_id = b.id AND ba.author_id = a.id);
GO

IF OBJECT_ID(N'dbo.Books_legacy', N'U') IS NOT NULL
INSERT INTO dbo.book_copies (inventory, book_id, condition_code, donor_id, received_by, received_on)
SELECT N'LEG-' + CAST(b.id AS NVARCHAR(20)) + N'-' + CAST(n.n AS NVARCHAR(20)),
       b.id, 'GOOD', NULL, NULL, CAST(GETDATE() AS DATE)
FROM dbo.Books_legacy b
CROSS JOIN (SELECT TOP (100) ROW_NUMBER() OVER (ORDER BY (SELECT NULL)) AS n FROM sys.all_objects) n
WHERE n.n <= b.copies
  AND b.id IN (SELECT id FROM dbo.books)
  AND NOT EXISTS (
      SELECT 1 FROM dbo.book_copies c
      WHERE c.inventory = N'LEG-' + CAST(b.id AS NVARCHAR(20)) + N'-' + CAST(n.n AS NVARCHAR(20))
  );
GO
