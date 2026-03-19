# Configuración de Prueba - MobID 835

## 🎯 Objetivo
Probar el sistema BossPartyValidator con el MobID 835 requiriendo mínimo 2 miembros en party.

## 📋 Pasos de Configuración

### 1. Ejecutar Script SQL
```sql
-- Ejecutar: mobid_835_test.sql
-- Esto configurará el MobID 835 para requerir 2+ miembros
```

### 2. Compilar el Servidor
```bash
# Compilar sdev con los nuevos archivos
# BossPartyValidator.cpp debe incluirse en el proyecto
```

### 3. Escenario de Prueba

#### ✅ **Test 1: Solo Player (Drop Bloqueado)**
1. Un jugador solo mata al MobID 835
2. **Expected**: No drops + mensaje "Este boss requiere una party de 2+ miembros para recibir drops."
3. **Console Log**: 
   ```
   [BOSS 835 TEST] Iniciando validación - Player: NombrePlayer
   [BOSS 835 TEST] HasValidParty: User has no party
   [BOSS 835 TEST] Drop bloqueado
   ```

#### ✅ **Test 2: Party de 2 (Drop Permitido)**
1. Party con 2 jugadores mata al MobID 835
2. **Expected**: Drops normales + sin mensaje de bloqueo
3. **Console Log**:
   ```
   [BOSS 835 TEST] Iniciando validación - Player: NombrePlayer
   [BOSS 835 TEST] HasValidParty: Analyzing party with 2 total members
   [BOSS 835 TEST] Active member: Player1
   [BOSS 835 TEST] Active member: Player2
   [BOSS 835 TEST] HasValidParty: Active=2 Required=2
   [BOSS 835 TEST] Party válida - Permitiendo drop
   ```

#### ✅ **Test 3: Party con 1 muerto (Drop Bloqueado)**
1. Party de 2, pero 1 está muerto
2. **Expected**: No drops
3. **Console Log**:
   ```
   [BOSS 835 TEST] Active member: Player1
   [BOSS 835 TEST] Dead member: Player2
   [BOSS 835 TEST] HasValidParty: Active=1 Required=2
   [BOSS 835 TEST] Drop bloqueado
   ```

## 🔍 Logs Esperados

### Drop Bloqueado
```
[2026-03-11 11:08:00] [BossPartyValidator] Cargadas 1 reglas de Boss Party
[BOSS 835 TEST] Iniciando validación - Player: Maxi
[BOSS 835 TEST] HasValidParty: User has no party
[2026-03-11 11:08:01] [BossPartyValidator] Drop bloqueado - MobID: 835 Player: Maxi PartySize: 0
```

### Drop Permitido
```
[BOSS 835 TEST] Iniciando validación - Player: Maxi
[BOSS 835 TEST] HasValidParty: Analyzing party with 2 total members
[BOSS 835 TEST] Active member: Maxi
[BOSS 835 TEST] Active member: Player2
[BOSS 835 TEST] HasValidParty: Active=2 Required=2
[BOSS 835 TEST] Party válida - Permitiendo drop
```

## ⚠️ Notas Importantes

1. **Hooks**: Los hooks en `hook_boss_party.cpp` necesitan las direcciones correctas de memoria
2. **Testing**: Probar primero con GM mode para facilitar
3. **Logs**: Los logs de prueba solo aparecen para MobID 835
4. **Performance**: Los logs detallados se pueden remover después de testing

## 🚀 Siguiente Paso

Una vez que los hooks estén implementados con las direcciones correctas, el sistema debería funcionar automáticamente para el MobID 835.

## 📊 Verificación

```sql
-- Verificar configuración actual
SELECT * FROM PS_GameDefs.dbo.ActiveBossPartyRules WHERE MobID = 835;
```

**Resultado esperado:**
```
MobID | MinPartySize | RequiereParty | Message
835   | 2            | SI           | Este boss requiere una party de 2+ miembros para recibir drops.
```
