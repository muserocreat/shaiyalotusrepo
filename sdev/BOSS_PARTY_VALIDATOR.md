# Boss Party Validator - Sistema Anti-Soloing

## Descripción
Sistema separado que valida si un jugador puede recibir drops de bosses basándose en el tamaño de su party. **No interfere con el sistema de drops dinámicos existente.**

## Arquitectura
```
DynamicDropManager (existente) → BossPartyValidator (filtro) → Sistema de Drops (nativo)
```

## Características

### ✅ **Zero Impact en Sistemas Existentes**
- DynamicDropManager funciona exactamente igual
- Sistema de drops nativo sin modificaciones
- Configuración completamente separada

### ✅ **Configuración en Tiempo Real**
- Tabla SQL independiente: `PS_GameDefs.dbo.BossPartyRequirements`
- Recarga en caliente con trigger similar a DynamicDrop
- Sin necesidad de recompilar

### ✅ **Validación Inteligente**
- Solo afecta bosses configurados explícitamente
- Cuenta miembros activos (no muertos/offline)
- Mensajes personalizados por boss

## Instalación

### 1. Base de Datos
```sql
-- Ejecutar boss_party_requirements.sql
-- Configurar bosses según necesidad
```

### 2. Compilación
- Agregar archivos al proyecto sdev
- Compilar normalmente

### 3. Configuración
```sql
-- Ejemplo: Boss requiere 5+ miembros
INSERT INTO PS_GameDefs.dbo.BossPartyRequirements (MobID, MinPartySize, RequireParty, Message) 
VALUES (1001, 5, 1, 'Este boss requiere party de 5+ miembros para drops.');

-- Recargar reglas en tiempo real
EXEC PS_GameDefs.dbo.ReloadBossPartyRules;
```

## Flujo de Validación

1. **Mob muere** → Sistema normal de muerte
2. **BossPartyValidator.ValidateDrop()** → ¿Este mob requiere party?
3. **Si NO**: Continúa flujo normal (drops dinámicos + nativos)
4. **Si SÍ**: ¿Party válida?
   - ✅ Party válida → Continúa flujo normal
   - ❌ Party inválida → Bloquea drops + envía mensaje

## Ejemplos de Uso

### Boss Normal (sin validación)
```sql
-- No hay entrada en BossPartyRequirements
-- Drops funcionan normalmente
```

### Boss con Requerimiento de Party
```sql
INSERT INTO PS_GameDefs.dbo.BossPartyRequirements 
(MobID, MinPartySize, RequireParty, Message) 
VALUES (2001, 3, 1, 'Dragon Rojo requiere party de 3+ miembros');
```

### Configuración por Nivel de Boss
```sql
-- Bosses nivel 1-50: 3+ miembros
INSERT INTO PS_GameDefs.dbo.BossPartyRequirements (MobID, MinPartySize, Message)
SELECT MobID, 3, 'Boss básico requiere party de 3+' 
FROM Mobs WHERE Level BETWEEN 1 AND 50 AND IsBoss = 1;

-- Bosses nivel 51-70: 5+ miembros  
INSERT INTO PS_GameDefs.dbo.BossPartyRequirements (MobID, MinPartySize, Message)
SELECT MobID, 5, 'Boss intermedio requiere party de 5+'
FROM Mobs WHERE Level BETWEEN 51 AND 70 AND IsBoss = 1;
```

## Monitoreo

### Logs del Sistema
```
[2026-03-11 11:05:00] [BossPartyValidator] Drop bloqueado - MobID: 1001 Player: Maxi PartySize: 1
[2026-03-11 11:05:30] [BossPartyValidator] Drop permitido - MobID: 1001 Player: PartyA PartySize: 5
```

### Vista de Reglas Activas
```sql
SELECT * FROM PS_GameDefs.dbo.ActiveBossPartyRules;
```

## Ventajas de este Enfoque

1. **Separación Completa**: Lógica de validación independiente
2. **Cero Riesgo**: No afecta sistemas existentes
3. **Mantenible**: Fácil de extender/modificar
4. **Configurable**: Todo por base de datos
5. **Performance**: Validación mínima, solo cuando es necesario
6. **Debugging**: Logs detallados de bloqueos

## Próximos Pasos

1. **Testing**: Verificar hooks con IDs correctos
2. **Performance**: Monitorear impacto en rendimiento
3. **Extensión**: Agregar más validaciones (nivel, rango, etc.)
4. **UI**: Panel de configuración para GMs

Este sistema proporciona una solución robusta y segura para evitar soloing sin comprometer la estabilidad del servidor.
