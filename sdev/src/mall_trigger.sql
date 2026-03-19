USE [PS_GameDefs]
GO

IF NOT EXISTS (SELECT * FROM sys.columns WHERE Name = N'ReloadMall' AND Object_ID = OBJECT_ID(N'[dbo].[ServerControl]'))
BEGIN
    ALTER TABLE [dbo].[ServerControl] ADD [ReloadMall] [int] NOT NULL DEFAULT(0)
    PRINT 'Columna [ReloadMall] añadida a [ServerControl].'
END
GO

IF EXISTS (SELECT * FROM sys.triggers WHERE object_id = OBJECT_ID(N'[dbo].[TRG_ProductList_UpdateServerControl]'))
    DROP TRIGGER [dbo].[TRG_ProductList_UpdateServerControl]
GO

CREATE TRIGGER [dbo].[TRG_ProductList_UpdateServerControl]
ON [dbo].[ProductList]
AFTER INSERT, UPDATE
AS
BEGIN
    SET NOCOUNT ON;
    UPDATE [PS_GameDefs].[dbo].[ServerControl] SET [ReloadMall] = 1;
END
GO
