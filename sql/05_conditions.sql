INSERT INTO dbo.conditions (code, title, value_factor, available_for_loan)
SELECT v.code, v.title, v.value_factor, v.available_for_loan
FROM (VALUES
    ('NEW',  N'Новое',    1.00, 1),
    ('GOOD', N'Хорошее',  0.85, 1),
    ('WORN', N'Потёртое', 0.60, 1),
    ('BAD',  N'Ветхое',   0.30, 1),
    ('LOST', N'Утеряно',  0.00, 0)
) AS v(code, title, value_factor, available_for_loan)
WHERE NOT EXISTS (SELECT 1 FROM dbo.conditions c WHERE c.code = v.code);
GO
