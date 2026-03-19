-- =====================================================
-- UNINSTALLER - BOSS PARTY VALIDATOR
-- =====================================================
-- Ejecutar solo si necesitas desinstalar completamente
-- =====================================================

USE PS_GameDefs;
GO

PRINT '==================================================';
PRINT 'BOSS PARTY VALIDATOR - DESINSTALACIÓN';
PRINT '==================================================';
PRINT '';

PRINT 'ADVERTENCIA: Esto eliminará TODA la configuración';
PRINT 'de BossPartyValidator incluyendo MobID 835';
PRINT '';
PRINT 'Presiona Ctrl+C para cancelar o espera 5 segundos...';
-- WAITFOR DELAY '00:00:05';
PRINT '';

-- Eliminar procedimientos almacenados
PRINT '1. Eliminando procedimientos almacenados...';

IF EXISTS (SELECT * FROM sys.objects WHERE object_id = OBJECT_ID(N'[dbo].[ReloadBossPartyRules]') AND type in (N'P'))
BEGIN
    DROP PROCEDURE [dbo].[ReloadBossPartyRules];
    PRINT '   ✓ Procedimiento ReloadBossPartyRules eliminado';
END

IF EXISTS (SELECT * FROM sys.objects WHERE object_id = OBJECT_ID(N'[dbo].[AddBossPartyRule]') AND type in (N'P'))
BEGIN
    DROP PROCEDURE [dbo].[AddBossPartyRule];
    PRINT '   ✓ Procedimiento AddBossPartyRule eliminado';
END

IF EXISTS (SELECT * FROM sys.objects WHERE object_id = OBJECT_ID(N'[dbo].[RemoveBossPartyRule]') AND type in (N'P'))
BEGIN
    DROP PROCEDURE [dbo].[RemoveBossPartyRule];
    PRINT '   ✓ Procedimiento RemoveBossPartyRule eliminado';
END
GO

-- Eliminar vista
PRINT '2. Eliminando vistas...';

IF EXISTS (SELECT * FROM sys.views WHERE object_id = OBJECT_ID(N'[dbo].[ActiveBossPartyRules]'))
BEGIN
    DROP VIEW [dbo].[ActiveBossPartyRules];
    PRINT '   ✓ Vista ActiveBossPartyRules eliminada';
END
GO

-- Eliminar tabla principal
PRINT '3. Eliminando tablas...';

IF EXISTS (SELECT * FROM sys.objects WHERE object_id = OBJECT_ID(N'[dbo].[BossPartyRequirements]') AND type in (N'U'))
BEGIN
    PRINT '   - Eliminando tabla BossPartyRequirements...';
    DROP TABLE [dbo].[BossPartyRequirements];
    PRINT '   ✓ Tabla BossPartyRequirements eliminada';
END

-- NOTA: No eliminamos ServerControl porque puede ser usada por otros sistemas
GO

PRINT '';
PRINT '==================================================';
PRINT 'DESINSTALACIÓN COMPLETADA';
PRINT '==================================================';
PRINT '';
PRINT 'BossPartyValidator ha sido completamente removido';
PRINT 'del sistema de base de datos.';
PRINT '';
PRINT 'Para reinstalar, ejecuta install_boss_party_validator.sql';
GO
