-- =====================================================
-- BOSS PARTY VALIDATOR - INSTALADOR COMPLETO
-- =====================================================
-- Sistema para evitar soloing en bosses
-- Requiere party mínima para recibir drops
-- =====================================================

-- Usar base de datos correcta
USE PS_GameDefs;
GO

PRINT '==================================================';
PRINT 'BOSS PARTY VALIDATOR - INSTALACIÓN';
PRINT '==================================================';
PRINT '';

-- =====================================================
-- 1. CREAR TABLA PRINCIPAL
-- =====================================================
PRINT '1. Creando tabla BossPartyRequirements...';

IF EXISTS (SELECT * FROM sys.objects WHERE object_id = OBJECT_ID(N'[dbo].[BossPartyRequirements]') AND type in (N'U'))
BEGIN
    PRINT '   - La tabla ya existe, eliminando versión anterior...';
    DROP TABLE [dbo].[BossPartyRequirements];
END

CREATE TABLE [dbo].[BossPartyRequirements] (
    [MobID] [int] NOT NULL PRIMARY KEY,
    [MinPartySize] [tinyint] NOT NULL DEFAULT 3,
    [RequireParty] [bit] NOT NULL DEFAULT 1,
    [Message] [varchar](128) NOT NULL DEFAULT 'Este boss requiere una party para recibir drops.',
    [CreatedDate] [datetime] NOT NULL DEFAULT GETDATE(),
    [ModifiedDate] [datetime] NOT NULL DEFAULT GETDATE()
);

PRINT '   ✓ Tabla BossPartyRequirements creada exitosamente';
GO

-- =====================================================
-- 2. CREAR TABLA DE CONTROL
-- =====================================================
PRINT '2. Creando tabla ServerControl...';

IF EXISTS (SELECT * FROM sys.objects WHERE object_id = OBJECT_ID(N'[dbo].[ServerControl]') AND type in (N'U'))
BEGIN
    PRINT '   - La tabla ServerControl ya existe, verificando estructura...';
    
    -- Agregar columna si no existe
    IF NOT EXISTS (SELECT * FROM sys.columns WHERE object_id = OBJECT_ID(N'[dbo].[ServerControl]') AND name = 'ReloadBossPartyRules')
    BEGIN
        ALTER TABLE [dbo].[ServerControl] ADD [ReloadBossPartyRules] [bit] NOT NULL DEFAULT 0;
        PRINT '   ✓ Columna ReloadBossPartyRules agregada a ServerControl';
    END
END
ELSE
BEGIN
    CREATE TABLE [dbo].[ServerControl] (
        [ReloadBossPartyRules] [bit] NOT NULL DEFAULT 0,
        [ReloadDrops] [bit] NOT NULL DEFAULT 0
    );
    PRINT '   ✓ Tabla ServerControl creada exitosamente';
END
GO

-- =====================================================
-- 3. CREAR VISTA DE REGLAS ACTIVAS
-- =====================================================
PRINT '3. Creando vista ActiveBossPartyRules...';

IF EXISTS (SELECT * FROM sys.views WHERE object_id = OBJECT_ID(N'[dbo].[ActiveBossPartyRules]'))
BEGIN
    PRINT '   - Eliminando vista existente...';
    DROP VIEW [dbo].[ActiveBossPartyRules];
END

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

PRINT '   ✓ Vista ActiveBossPartyRules creada exitosamente';
GO

-- =====================================================
-- 4. CREAR PROCEDIMIENTOS ALMACENADOS
-- =====================================================
PRINT '4. Creando procedimientos almacenados...';

-- Procedimiento para recargar reglas
IF EXISTS (SELECT * FROM sys.objects WHERE object_id = OBJECT_ID(N'[dbo].[ReloadBossPartyRules]') AND type in (N'P'))
BEGIN
    PRINT '   - Eliminando procedimiento ReloadBossPartyRules existente...';
    DROP PROCEDURE [dbo].[ReloadBossPartyRules];
END

CREATE PROCEDURE [dbo].[ReloadBossPartyRules]
AS
BEGIN
    SET NOCOUNT ON;
    UPDATE [dbo].[ServerControl] SET [ReloadBossPartyRules] = 1;
    PRINT 'BossPartyRules: Señal de recarga enviada al servidor';
END
PRINT '   ✓ Procedimiento ReloadBossPartyRules creado';
GO

-- Procedimiento para agregar nueva regla
IF EXISTS (SELECT * FROM sys.objects WHERE object_id = OBJECT_ID(N'[dbo].[AddBossPartyRule]') AND type in (N'P'))
BEGIN
    PRINT '   - Eliminando procedimiento AddBossPartyRule existente...';
    DROP PROCEDURE [dbo].[AddBossPartyRule];
END

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
END
PRINT '   ✓ Procedimiento AddBossPartyRule creado';
GO

-- Procedimiento para eliminar regla
IF EXISTS (SELECT * FROM sys.objects WHERE object_id = OBJECT_ID(N'[dbo].[RemoveBossPartyRule]') AND type in (N'P'))
BEGIN
    PRINT '   - Eliminando procedimiento RemoveBossPartyRule existente...';
    DROP PROCEDURE [dbo].[RemoveBossPartyRule];
END

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
END
PRINT '   ✓ Procedimiento RemoveBossPartyRule creado';
GO

-- =====================================================
-- 5. INSERTAR CONFIGURACIÓN INICIAL - MOBID 835
-- =====================================================
PRINT '5. Insertando configuración inicial para MobID 835...';

-- Limpiar configuración anterior si existe
DELETE FROM [dbo].[BossPartyRequirements] WHERE [MobID] = 835;

-- Insertar configuración para prueba
INSERT INTO [dbo].[BossPartyRequirements] 
([MobID], [MinPartySize], [RequireParty], [Message]) 
VALUES 
(835, 2, 1, 'Este boss requiere una party de 2+ miembros para recibir drops.');

PRINT '   ✓ Configuración para MobID 835 insertada';
PRINT '   - Requiere: 2+ miembros en party';
PRINT '   - Mensaje personalizado activado';
GO

-- =====================================================
-- 6. INSERTAR EJEMPLOS ADICIONALES
-- =====================================================
PRINT '6. Insertando ejemplos adicionales...';

INSERT INTO [dbo].[BossPartyRequirements] ([MobID], [MinPartySize], [RequireParty], [Message]) VALUES
-- Bosses de nivel bajo (3+ miembros)
(1001, 3, 1, 'Este boss requiere una party de 3+ miembros para recibir drops.'),
(1002, 3, 1, 'Necesitas estar en una party de 3+ para obtener drops de este boss.'),

-- Bosses de nivel medio (5+ miembros)  
(2001, 5, 1, 'Boss medio requiere party de 5+ miembros para drops.'),
(2002, 5, 1, 'Este boss solo da drops a parties de 5+ miembros.'),

-- World Boss (10+ miembros)
(4001, 10, 1, 'World Boss requiere party de 10+ miembros para recibir drops.'),

-- Ejemplo de boss que NO requiere party (para testing)
(9999, 1, 0, 'Este boss no requiere party.');

PRINT '   ✓ Ejemplos adicionales insertados';
GO

-- =====================================================
-- 7. VERIFICACIÓN FINAL
-- =====================================================
PRINT '7. Verificación final de la instalación...';

PRINT '';
PRINT '==================================================';
PRINT 'REGLAS CONFIGURADAS:';
PRINT '==================================================';

SELECT 
    [MobID],
    [MinPartySize] as 'Minimo_Party',
    CASE WHEN [RequireParty] = 1 THEN 'SI' ELSE 'NO' END as 'Requiere_Party',
    LEFT([Message], 50) + '...' as 'Mensaje'
FROM [dbo].[BossPartyRequirements] 
ORDER BY [MobID];

PRINT '';
PRINT '==================================================';
PRINT 'ESTADÍSTICAS:';
PRINT '==================================================';

DECLARE @TotalRules INT, @ActiveRules INT;
SELECT @TotalRules = COUNT(*) FROM [dbo].[BossPartyRequirements];
SELECT @ActiveRules = COUNT(*) FROM [dbo].[BossPartyRequirements] WHERE [RequireParty] = 1;

PRINT 'Total de reglas configuradas: ' + CAST(@TotalRules AS VARCHAR);
PRINT 'Reglas activas (requieren party): ' + CAST(@ActiveRules AS VARCHAR);
PRINT '';
GO

-- =====================================================
-- 8. INSTRUCCIONES POST-INSTALACIÓN
-- =====================================================
PRINT '==================================================';
PRINT 'INSTALACIÓN COMPLETADA EXITOSAMENTE';
PRINT '==================================================';
PRINT '';
PRINT 'Instrucciones de uso:';
PRINT '';
PRINT '1. Para agregar una nueva regla:';
PRINT '   EXEC AddBossPartyRule @MobID = 1234, @MinPartySize = 3;';
PRINT '';
PRINT '2. Para eliminar una regla:';
PRINT '   EXEC RemoveBossPartyRule @MobID = 1234;';
PRINT '';
PRINT '3. Para recargar reglas en tiempo real:';
PRINT '   EXEC ReloadBossPartyRules;';
PRINT '';
PRINT '4. Para ver reglas activas:';
PRINT '   SELECT * FROM ActiveBossPartyRules;';
PRINT '';
PRINT '5. Configuración de prueba lista:';
PRINT '   - MobID 835 requiere 2+ miembros en party';
PRINT '   - Logs detallados activados para debugging';
PRINT '';
PRINT '==================================================';
PRINT '¡SISTEMA BOSS PARTY VALIDATOR LISTO PARA USAR!';
PRINT '==================================================';
GO

-- =====================================================
-- 9. LIMPIEZA FINAL
-- =====================================================
PRINT '9. Limpiando variables temporales...';

-- Forzar recarga inicial
UPDATE [dbo].[ServerControl] SET [ReloadBossPartyRules] = 0;

PRINT '   ✓ Limpieza completada';
PRINT '';
PRINT 'INSTALACIÓN FINALIZADA - El servidor está listo para compilar y probar';
GO
