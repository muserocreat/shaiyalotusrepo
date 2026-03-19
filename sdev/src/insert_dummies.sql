USE [PS_GameDefs];
GO

SET NOCOUNT ON;
DECLARE @i INT = 1;
DECLARE @ProductCode VARCHAR(21);
DECLARE @ProductName VARCHAR(51);

PRINT 'Iniciando creacion de 50 Slots de Reserva en ProductList (Hot-Swap Dummies)...';

WHILE @i <= 50
BEGIN
    -- Genera ProductCodes como RES001, RES002, ..., RES050
    SET @ProductCode = 'RES' + RIGHT('00' + CAST(@i AS VARCHAR(3)), 3);
    SET @ProductName = 'Espacio Vacio ' + CAST(@i AS VARCHAR(3));

    IF NOT EXISTS (SELECT 1 FROM [dbo].[ProductList] WHERE [ProductCode] = @ProductCode)
    BEGIN
        INSERT INTO [dbo].[ProductList] (
            [ProductName], [ProductCode], [BuyCost],
            [ItemID1], [ItemCount1], [ItemID2], [ItemCount2], [ItemID3], [ItemCount3],
            [ItemID4], [ItemCount4], [ItemID5], [ItemCount5], [ItemID6], [ItemCount6],
            [ItemID7], [ItemCount7], [ItemID8], [ItemCount8], [ItemID9], [ItemCount9],
            [ItemID10], [ItemCount10], [ItemID11], [ItemCount11], [ItemID12], [ItemCount12],
            [ItemID13], [ItemCount13], [ItemID14], [ItemCount14], [ItemID15], [ItemCount15],
            [ItemID16], [ItemCount16], [ItemID17], [ItemCount17], [ItemID18], [ItemCount18],
            [ItemID19], [ItemCount19], [ItemID20], [ItemCount20], [ItemID21], [ItemCount21],
            [ItemID22], [ItemCount22], [ItemID23], [ItemCount23], [ItemID24], [ItemCount24]
        ) VALUES (
            @ProductName, @ProductCode, 999999,
            25069, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
            0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
            0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
            0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
        );
    END
    SET @i = @i + 1;
END

PRINT 'Â¡50 Slots de reserva creados exitosamente!';
GO
