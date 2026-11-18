IF NOT EXISTS (SELECT 1 FROM dbo.roles WHERE name = N'Администратор')
INSERT INTO dbo.roles (name) VALUES (N'Администратор');
GO

IF NOT EXISTS (SELECT 1 FROM dbo.roles WHERE name = N'Библиотекарь')
INSERT INTO dbo.roles (name) VALUES (N'Библиотекарь');
GO

IF NOT EXISTS (SELECT 1 FROM dbo.roles WHERE name = N'Читатель')
INSERT INTO dbo.roles (name) VALUES (N'Читатель');
GO

INSERT INTO dbo.role_permissions (role_id, permission)
SELECT r.id, v.permission
FROM (VALUES
    (N'Администратор', N'catalog.add'),
    (N'Администратор', N'catalog.edit'),
    (N'Администратор', N'catalog.delete'),
    (N'Администратор', N'copies.manage'),
    (N'Администратор', N'catalog.save'),
    (N'Администратор', N'analytics.view'),
    (N'Библиотекарь',  N'catalog.add'),
    (N'Библиотекарь',  N'catalog.edit'),
    (N'Библиотекарь',  N'copies.manage'),
    (N'Библиотекарь',  N'catalog.save'),
    (N'Библиотекарь',  N'analytics.view')
) AS v(role_name, permission)
JOIN dbo.roles r ON r.name = v.role_name
WHERE NOT EXISTS (
    SELECT 1 FROM dbo.role_permissions rp
    WHERE rp.role_id = r.id AND rp.permission = v.permission
);
GO
