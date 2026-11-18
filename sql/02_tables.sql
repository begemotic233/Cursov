IF OBJECT_ID(N'dbo.Books', N'U') IS NOT NULL
   AND COL_LENGTH(N'dbo.Books', N'author') IS NOT NULL
    EXEC sp_rename N'dbo.Books', N'Books_legacy';
GO

IF OBJECT_ID(N'dbo.roles', N'U') IS NULL
CREATE TABLE dbo.roles (
    id   INT IDENTITY(1,1) NOT NULL PRIMARY KEY,
    name NVARCHAR(50)      NOT NULL UNIQUE
);
GO

IF OBJECT_ID(N'dbo.role_permissions', N'U') IS NULL
CREATE TABLE dbo.role_permissions (
    role_id    INT          NOT NULL REFERENCES dbo.roles(id) ON DELETE CASCADE,
    permission NVARCHAR(50) NOT NULL,
    CONSTRAINT pk_role_permissions PRIMARY KEY (role_id, permission)
);
GO

IF OBJECT_ID(N'dbo.users', N'U') IS NULL
CREATE TABLE dbo.users (
    id        INT IDENTITY(1,1) NOT NULL PRIMARY KEY,
    full_name NVARCHAR(150)     NOT NULL,
    login     NVARCHAR(50)      NOT NULL UNIQUE,
    role_id   INT               NOT NULL REFERENCES dbo.roles(id)
);
GO

IF OBJECT_ID(N'dbo.authors', N'U') IS NULL
CREATE TABLE dbo.authors (
    id        INT IDENTITY(1,1) NOT NULL PRIMARY KEY,
    full_name NVARCHAR(150)     NOT NULL UNIQUE
);
GO

IF OBJECT_ID(N'dbo.books', N'U') IS NULL
CREATE TABLE dbo.books (
    id     INT            NOT NULL PRIMARY KEY,
    title  NVARCHAR(255)  NOT NULL,
    genre  NVARCHAR(100)  NOT NULL,
    [year] INT            NOT NULL,
    price  DECIMAL(12, 2) NOT NULL
);
GO

IF OBJECT_ID(N'dbo.book_authors', N'U') IS NULL
CREATE TABLE dbo.book_authors (
    book_id      INT NOT NULL REFERENCES dbo.books(id) ON DELETE CASCADE,
    author_id    INT NOT NULL REFERENCES dbo.authors(id),
    author_order INT NOT NULL,
    CONSTRAINT pk_book_authors PRIMARY KEY (book_id, author_id)
);
GO

IF OBJECT_ID(N'dbo.conditions', N'U') IS NULL
CREATE TABLE dbo.conditions (
    code                VARCHAR(10)   NOT NULL PRIMARY KEY,
    title               NVARCHAR(50)  NOT NULL,
    value_factor        DECIMAL(4, 2) NOT NULL,
    available_for_loan  BIT           NOT NULL
);
GO

IF OBJECT_ID(N'dbo.donors', N'U') IS NULL
CREATE TABLE dbo.donors (
    id        INT IDENTITY(1,1) NOT NULL PRIMARY KEY,
    full_name NVARCHAR(150)     NOT NULL UNIQUE,
    contact   NVARCHAR(100)     NULL
);
GO

IF OBJECT_ID(N'dbo.book_copies', N'U') IS NULL
CREATE TABLE dbo.book_copies (
    inventory      NVARCHAR(30) NOT NULL PRIMARY KEY,
    book_id        INT          NOT NULL REFERENCES dbo.books(id) ON DELETE CASCADE,
    condition_code VARCHAR(10)  NOT NULL REFERENCES dbo.conditions(code),
    donor_id       INT          NULL REFERENCES dbo.donors(id),
    received_by    INT          NULL REFERENCES dbo.users(id),
    received_on    DATE         NOT NULL
);
GO
