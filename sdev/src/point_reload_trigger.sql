USE [PS_GameDefs];
GO

IF NOT EXISTS (SELECT * FROM sys.objects WHERE object_id = OBJECT_ID(N'[dbo].[PointReloadQueue]') AND type in (N'U'))
BEGIN
    CREATE TABLE [dbo].[PointReloadQueue](
        [UserUID] [int] NOT NULL,
        CONSTRAINT [PK_PointReloadQueue] PRIMARY KEY CLUSTERED ([UserUID] ASC)
    )
    PRINT 'Tabla [PointReloadQueue] creada en PS_GameDefs.'
END
GO

USE [PS_UserData];
GO

IF EXISTS (SELECT * FROM sys.triggers WHERE object_id = OBJECT_ID(N'[dbo].[TRG_UsersMaster_UpdatePoints]'))
    DROP TRIGGER [dbo].[TRG_UsersMaster_UpdatePoints];
GO

CREATE TRIGGER [dbo].[TRG_UsersMaster_UpdatePoints]
ON [dbo].[Users_Master]
AFTER UPDATE
AS
BEGIN
    SET NOCOUNT ON;

    IF UPDATE(Point)
    BEGIN
        INSERT INTO PS_GameDefs.dbo.PointReloadQueue (UserUID)
        SELECT i.UserUID
        FROM inserted i
        INNER JOIN deleted d ON i.UserUID = d.UserUID
        WHERE i.Point > d.Point
          AND NOT EXISTS (
              SELECT 1 FROM PS_GameDefs.dbo.PointReloadQueue q WHERE q.UserUID = i.UserUID
          );
    END
END
GO
PRINT 'Trigger [TRG_UsersMaster_UpdatePoints] instalado en PS_UserData.'
GO
