-- =====================================================
-- BOSS PARTY VALIDATOR - COMANDOS INDIVIDUALES
-- =====================================================
-- Ejecutar cada comando por separado
-- =====================================================

-- COMANDO 1: Crear tabla principal
USE PS_GameDefs;
GO

IF EXISTS (SELECT * FROM sys.objects WHERE object_id = OBJECT_ID(N'[dbo].[BossPartyRequirements]') AND type in (N'U'))
BEGIN
    DROP TABLE [dbo].[BossPartyRequirements];
END
GO

CREATE TABLE [dbo].[BossPartyRequirements] (
    [MobID] [int] NOT NULL PRIMARY KEY,
    [MinPartySize] [tinyint] NOT NULL DEFAULT 3,
    [RequireParty] [bit] NOT NULL DEFAULT 1,
    [Message] [varchar](128) NOT NULL DEFAULT 'Este boss requiere una party para recibir drops.',
    [CreatedDate] [datetime] NOT NULL DEFAULT GETDATE(),
    [ModifiedDate] [datetime] NOT NULL DEFAULT GETDATE()
);
GO

-- =====================================================

-- COMANDO 2: Verificar ServerControl
USE PS_GameDefs;
GO

IF NOT EXISTS (SELECT * FROM sys.objects WHERE object_id = OBJECT_ID(N'[dbo].[ServerControl]') AND type in (N'U'))
BEGIN
    CREATE TABLE [dbo].[ServerControl] (
        [ReloadBossPartyRules] [bit] NOT NULL DEFAULT 0,
        [ReloadDrops] [bit] NOT NULL DEFAULT 0
    );
END
ELSE
BEGIN
    IF NOT EXISTS (SELECT * FROM sys.columns WHERE object_id = OBJECT_ID(N'[dbo].[ServerControl]') AND name = 'ReloadBossPartyRules')
    BEGIN
        ALTER TABLE [dbo].[ServerControl] ADD [ReloadBossPartyRules] [bit] NOT NULL DEFAULT 0;
    END
END
GO

-- =====================================================

-- COMANDO 3: Crear vista
USE PS_GameDefs;
GO

IF EXISTS (SELECT * FROM sys.views WHERE object_id = OBJECT_ID(N'[dbo].[ActiveBossPartyRules]'))
BEGIN
    DROP VIEW [dbo].[ActiveBossPartyRules];
END
GO

CREATE VIEW [dbo].[ActiveBossPartyRules] AS
SELECT 
    [MobID],
    [MinPartySize],
    CASE WHEN [RequireParty] = 1 THEN 'SI' ELSE 'NO' END as [RequiereParty],
    [Message],
    [CreatedDate],
    [ModifiedDate]
FROM [dbo].[BossPartyRequirements]
WHERE [RequireParty] = 1
ORDER BY [MobID];
GO

-- =====================================================

-- COMANDO 4: Crear procedimiento ReloadBossPartyRules
USE PS_GameDefs;
GO

IF EXISTS (SELECT * FROM sys.objects WHERE object_id = OBJECT_ID(N'[dbo].[ReloadBossPartyRules]') AND type in (N'P'))
BEGIN
    DROP PROCEDURE [dbo].[ReloadBossPartyRules];
END
GO

CREATE PROCEDURE [dbo].[ReloadBossPartyRules]
AS
BEGIN
    SET NOCOUNT ON;
    UPDATE [dbo].[ServerControl] SET [ReloadBossPartyRules] = 1;
    PRINT 'BossPartyRules: Señal de recarga enviada al servidor';
END;
GO

-- =====================================================

-- COMANDO 5: Crear procedimiento AddBossPartyRule
USE PS_GameDefs;
GO

IF EXISTS (SELECT * FROM sys.objects WHERE object_id = OBJECT_ID(N'[dbo].[AddBossPartyRule]') AND type in (N'P'))
BEGIN
    DROP PROCEDURE [dbo].[AddBossPartyRule];
END
GO

CREATE PROCEDURE [dbo].[AddBossPartyRule]
    @MobID INT,
    @MinPartySize TINYINT = 3,
    @Message VARCHAR(128) = 'Este boss requiere una party para recibir drops.'
AS
BEGIN
    SET NOCOUNT ON;
    
    IF EXISTS (SELECT 1 FROM [dbo].[BossPartyRequirements] WHERE [MobID] = @MobID)
    BEGIN
        UPDATE [dbo].[BossPartyRequirements] 
        SET [MinPartySize] = @MinPartySize,
            [Message] = @Message,
            [ModifiedDate] = GETDATE()
        WHERE [MobID] = @MobID;
        PRINT 'Regla actualizada para MobID: ' + CAST(@MobID AS VARCHAR);
    END
    ELSE
    BEGIN
        INSERT INTO [dbo].[BossPartyRequirements] ([MobID], [MinPartySize], [Message])
        VALUES (@MobID, @MinPartySize, @Message);
        PRINT 'Nueva regla creada para MobID: ' + CAST(@MobID AS VARCHAR);
    END
END;
GO

-- =====================================================

-- COMANDO 6: Crear procedimiento RemoveBossPartyRule
USE PS_GameDefs;
GO

IF EXISTS (SELECT * FROM sys.objects WHERE object_id = OBJECT_ID(N'[dbo].[RemoveBossPartyRule]') AND type in (N'P'))
BEGIN
    DROP PROCEDURE [dbo].[RemoveBossPartyRule];
END
GO

CREATE PROCEDURE [dbo].[RemoveBossPartyRule]
    @MobID INT
AS
BEGIN
    SET NOCOUNT ON;
    
    DELETE FROM [dbo].[BossPartyRequirements] WHERE [MobID] = @MobID;
    
    IF @@ROWCOUNT > 0
        PRINT 'Regla eliminada para MobID: ' + CAST(@MobID AS VARCHAR);
    ELSE
        PRINT 'No se encontró regla para MobID: ' + CAST(@MobID AS VARCHAR);
END;
GO

-- =====================================================

-- COMANDO 7: Insertar datos iniciales
USE PS_GameDefs;
GO

DELETE FROM [dbo].[BossPartyRequirements] WHERE [MobID] = 835;
GO

INSERT INTO [dbo].[BossPartyRequirements] ([MobID], [MinPartySize], [RequireParty], [Message]) VALUES
(835, 2, 1, 'Este boss requiere una party de 2+ miembros para recibir drops.'),
(1001, 3, 1, 'Este boss requiere una party de 3+ miembros para recibir drops.'),
(1002, 3, 1, 'Necesitas estar en una party de 3+ para obtener drops de este boss.'),
(2001, 5, 1, 'Boss medio requiere party de 5+ miembros para drops.'),
(2002, 5, 1, 'Este boss solo da drops a parties de 5+ miembros.'),
(4001, 10, 1, 'World Boss requiere party de 10+ miembros para recibir drops.'),
(9999, 1, 0, 'Este boss no requiere party.');
GO

-- =====================================================

-- COMANDO 8: Verificación final
USE PS_GameDefs;
GO

SELECT 
    [MobID],
    [MinPartySize] as 'Minimo_Party',
    CASE WHEN [RequireParty] = 1 THEN 'SI' ELSE 'NO' END as 'Requiere_Party',
    LEFT([Message], 60) as 'Mensaje'
FROM [dbo].[BossPartyRequirements] 
ORDER BY [MobID];
GO
