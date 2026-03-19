#include <algorithm>
#include <iostream>
#include <fstream>
#include <chrono>
#include <ctime>
#include <string>
#include <cstdio>
#include <windows.h>
#include <sql.h>
#include <sqlext.h>
#include "include/shaiya/BossPartyValidator.h"
#include "include/shaiya/CUser.h"
#include "include/shaiya/CParty.h"
#include "include/shaiya/CMob.h"
#include "include/shaiya/SConnection.h"
#include "include/shaiya/NetworkHelper.h"
#include <shaiya/include/network/game/outgoing/0100.h>

using namespace shaiya;

static std::string GetBossPartyLogPath()
{
    HMODULE hMod = NULL;
    GetModuleHandleExA(
        GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
        reinterpret_cast<LPCSTR>(&GetBossPartyLogPath),
        &hMod);

    char buffer[MAX_PATH];
    GetModuleFileNameA(hMod, buffer, MAX_PATH);
    std::string modpath = buffer;
    std::string modFolder = modpath.substr(0, modpath.find_last_of("\\/"));

    std::string dataFolder = modFolder + "\\Data";
    CreateDirectoryA(dataFolder.c_str(), NULL);

    return dataFolder + "\\BossPartyValidator.log";
}

static std::string GetBossPartyLogPathFallback()
{
    char buffer[MAX_PATH];
    GetModuleFileNameA(NULL, buffer, MAX_PATH);
    std::string exepath = buffer;
    std::string exeFolder = exepath.substr(0, exepath.find_last_of("\\/"));
    return exeFolder + "\\BossPartyValidator.log";
}

static void LogToFile(const std::string& message)
{
    std::string path = GetBossPartyLogPath();
    auto write = [&](const std::string& p) -> bool {
        FILE* f = nullptr;
        if (fopen_s(&f, p.c_str(), "a") != 0 || f == nullptr)
            return false;

        auto now = std::chrono::system_clock::now();
        std::time_t now_time = std::chrono::system_clock::to_time_t(now);
        char buf[100];
        std::tm tm_buf;
        localtime_s(&tm_buf, &now_time);
        std::strftime(buf, sizeof(buf), "[%Y-%m-%d %H:%M:%S]", &tm_buf);
        fprintf(f, "%s %s\n", buf, message.c_str());
        fclose(f);
        return true;
    };

    if (write(path))
        return;

    write(GetBossPartyLogPathFallback());
}

// Variables estáticas
std::shared_mutex BossPartyValidator::m_mutex;
std::unordered_map<uint32_t, BossPartyRule> BossPartyValidator::m_rules;
void* BossPartyValidator::m_hEnv = nullptr;
void* BossPartyValidator::m_hDbc = nullptr;
std::recursive_mutex BossPartyValidator::m_dbMutex;

// Variables para cooldown de mensajes
std::unordered_map<uint32_t, std::chrono::steady_clock::time_point> BossPartyValidator::m_messageCooldowns;
std::shared_mutex BossPartyValidator::m_cooldownMutex;

void BossPartyValidator::Init()
{
    Load();
}

void BossPartyValidator::CheckReloadTrigger() {
    using namespace std::chrono_literals;
    static std::chrono::system_clock::time_point next_check;
    static bool g_hasInitialLoad = false;

    if (!g_hasInitialLoad) {
        std::cout << "[BossPartyValidator] Iniciando conexion nativa (lotus) desde el hilo padre..." << std::endl;
        Load();
        g_hasInitialLoad = true;
        next_check = std::chrono::system_clock::now() + 10000ms;
        return;
    }

    auto now = std::chrono::system_clock::now();
    if (now < next_check) return;
    next_check = now + 10000ms;

    if (!ConnectDB()) return;

    std::lock_guard<std::recursive_mutex> lock(m_dbMutex);

    SQLHSTMT hStmt = NULL;
    if (SQLAllocHandle(SQL_HANDLE_STMT, m_hDbc, &hStmt) != SQL_SUCCESS) return;

    const char* query = "SELECT ReloadBossPartyRules FROM PS_GameDefs.dbo.ServerControl";
    if (SQL_SUCCEEDED(SQLExecDirectA(hStmt, (SQLCHAR*)query, SQL_NTS))) {
        SQLINTEGER reloadRequested = 0;
        SQLLEN len;
        if (SQL_SUCCEEDED(SQLFetch(hStmt))) {
            SQLGetData(hStmt, 1, SQL_C_LONG, &reloadRequested, 0, &len);
            if (reloadRequested == 1) {
                Log("Señal detectada en SQL. Iniciando recarga de reglas...");
                Load();
                SQLCloseCursor(hStmt); 

                const char* update = "UPDATE PS_GameDefs.dbo.ServerControl SET ReloadBossPartyRules = 0";
                SQLExecDirectA(hStmt, (SQLCHAR*)update, SQL_NTS);
            }
        }
    } else {
        LogError("La conexión de Reload Trigger falló, reiniciando driver...");
        DisconnectDB();
    }
    SQLFreeHandle(SQL_HANDLE_STMT, hStmt);
}

void BossPartyValidator::Load()
{
    if (!ConnectDB()) {
        LogError("No se pudo conectar a BD para cargar reglas de Boss Party");
        return;
    }

    std::lock_guard<std::recursive_mutex> lock(m_dbMutex);

    SQLHSTMT hStmt = NULL;
    if (SQLAllocHandle(SQL_HANDLE_STMT, m_hDbc, &hStmt) != SQL_SUCCESS) return;

    static bool isFirstLoad = true;
    
    // --- 1. SHADOW BUFFERING (Variables Temporales Seguras) ---
    std::unordered_map<uint32_t, BossPartyRule> tempRules;

    const char* query = "SELECT MobID, MinPartySize, RequireParty, Message FROM PS_GameDefs.dbo.BossPartyRequirements";
    
    SQLRETURN fetchRet = SQLExecDirectA(hStmt, (SQLCHAR*)query, SQL_NTS);

    if (!SQL_SUCCEEDED(fetchRet)) {
        LogError("La ejecución de la query de carga nativa falló.");
        SQLFreeHandle(SQL_HANDLE_STMT, hStmt);
        return;
    }

    SQLINTEGER mobId, minPartySize;
    SQLINTEGER requireParty;
    SQLCHAR message[128];
    SQLLEN lenMobId, lenMinSize, lenRequire, lenMessage;

    SQLBindCol(hStmt, 1, SQL_C_LONG, &mobId, 0, &lenMobId);
    SQLBindCol(hStmt, 2, SQL_C_LONG, &minPartySize, 0, &lenMinSize);
    SQLBindCol(hStmt, 3, SQL_C_LONG, &requireParty, 0, &lenRequire);
    SQLBindCol(hStmt, 4, SQL_C_CHAR, &message, sizeof(message), &lenMessage);

    while (SQL_SUCCEEDED(SQLFetch(hStmt))) {
        if (mobId == 0 || mobId > 30000) continue; // Seguridad: Ignorar IDs basura o fuera de rango

        BossPartyRule rule{};
        rule.mobId = (uint32_t)mobId;
        rule.minPartySize = (uint8_t)minPartySize;
        rule.requireParty = (requireParty != 0);
        strncpy_s(rule.message, sizeof(rule.message), (char*)message, _TRUNCATE);

        tempRules[rule.mobId] = rule;
    }

    SQLFreeHandle(SQL_HANDLE_STMT, hStmt);

    // --- 2. SWAP ATÓMICO (Reemplazo seguro) ---
    {
        std::unique_lock<std::shared_mutex> lock(m_mutex);
        size_t oldSize = m_rules.size();
        m_rules = std::move(tempRules);
        
        if (isFirstLoad) {
            std::cout << "[BossPartyValidator] Carga inicial completada - " << m_rules.size() << " reglas cargadas" << std::endl;
            Log("Carga inicial completada - " + std::to_string(m_rules.size()) + " reglas cargadas");
            isFirstLoad = false;
        } else {
            std::cout << "[BossPartyValidator] Recarga completada - " << m_rules.size() << " reglas (antes: " << oldSize << ")" << std::endl;
            Log("Recarga completada - " + std::to_string(m_rules.size()) + " reglas (antes: " + std::to_string(oldSize) + ")");
        }
    }
}

bool BossPartyValidator::ValidateDrop(uint32_t mobId, CUser* killer)
{
    std::string logMsg = "ValidateDrop llamado - MobID: " + std::to_string(mobId) + 
                        " Killer: " + std::string(killer ? killer->charName.data() : "NULL");
    std::cout << "[BOSS DEBUG] " << logMsg << std::endl;
    LogToFile("[BOSS DEBUG] " + logMsg);

    // Logging especial para MobID 835 durante pruebas
    if (mobId == 835) {
        std::string testMsg = "Iniciando validación - Killer: " + std::string(killer ? killer->charName.data() : "NULL");
        std::cout << "[BOSS 835 TEST] " << testMsg << std::endl;
        LogToFile("[BOSS 835 TEST] " + testMsg);
    }

    // Si no hay regla para este mob, permitir drop
    if (!IsBossRequiringParty(mobId)) {
        if (mobId == 835) {
            std::string testMsg = "Sin regla configurada - Permitiendo drop";
            std::cout << "[BOSS 835 TEST] " << testMsg << std::endl;
            LogToFile("[BOSS 835 TEST] " + testMsg);
        }
        std::string debugMsg = "No hay regla para MobID " + std::to_string(mobId) + " - Permitiendo drop";
        std::cout << "[BOSS DEBUG] " << debugMsg << std::endl;
        LogToFile("[BOSS DEBUG] " + debugMsg);
        return true;
    }

    // Obtener regla específica
    BossPartyRule rule;
    {
        std::shared_lock<std::shared_mutex> lock(m_mutex);
        auto it = m_rules.find(mobId);
        if (it == m_rules.end()) {
            if (mobId == 835) {
                std::string testMsg = "Regla no encontrada - Permitiendo drop";
                std::cout << "[BOSS 835 TEST] " << testMsg << std::endl;
                LogToFile("[BOSS 835 TEST] " + testMsg);
            }
            std::string debugMsg = "Regla no encontrada para MobID " + std::to_string(mobId) + " - Permitiendo drop";
            std::cout << "[BOSS DEBUG] " << debugMsg << std::endl;
            LogToFile("[BOSS DEBUG] " + debugMsg);
            return true; // Sin regla = permitir
        }
        rule = it->second;
    }

    std::string ruleMsg = "Regla encontrada - MobID: " + std::to_string(mobId) + 
                         " MinPartySize: " + std::to_string((int)rule.minPartySize) + 
                         " RequireParty: " + std::string(rule.requireParty ? "YES" : "NO");
    std::cout << "[BOSS DEBUG] " << ruleMsg << std::endl;
    LogToFile("[BOSS DEBUG] " + ruleMsg);

    // ESCENARIO CRÍTICO: Sin killer identificado
    if (!killer) {
        if (mobId == 835) {
            std::string testMsg = "Killer es NULL - Aplicando política estricta";
            std::cout << "[BOSS 835 TEST] " << testMsg << std::endl;
            LogToFile("[BOSS 835 TEST] " + testMsg);
        }
        
        std::string debugMsg = "Killer es NULL - Bloqueando drop por seguridad";
        std::cout << "[BOSS DEBUG] " << debugMsg << std::endl;
        LogToFile("[BOSS DEBUG] " + debugMsg);
        
        // Política: Si no podemos identificar al killer, aplicar regla estricta
        // Esto previene exploits donde players solos intentan evitar validación
        LogBlockedDrop(mobId, nullptr);
        return false; // Bloquear drops por seguridad
    }

    // Validar party del killer solo si RequireParty = true
    bool hasValidParty = true;
    if (rule.requireParty) {
        hasValidParty = HasValidParty(killer, rule.minPartySize);
    } else {
        // Si RequireParty = false, siempre permitir
        if (mobId == 835) {
            std::string testMsg = "RequireParty = FALSE - Permitiendo daño sin validar party";
            std::cout << "[BOSS 835 TEST] " << testMsg << std::endl;
            LogToFile("[BOSS 835 TEST] " + testMsg);
        }
    }
    
    if (mobId == 835) {
        int partySize = (killer && killer->party) ? killer->party->userCount : 0;
        std::string testMsg = "Killer validation - Required: " + std::to_string((int)rule.minPartySize) + 
                             " KillerParty: " + std::to_string(partySize) + 
                             " Valid: " + std::string(hasValidParty ? "YES" : "NO");
        std::cout << "[BOSS 835 TEST] " << testMsg << std::endl;
        LogToFile("[BOSS 835 TEST] " + testMsg);
        
        // Logging adicional para debugging de escenarios complejos
        if (killer->party) {
            std::string partyMsg = "Killer pertenece a party con " + std::to_string(killer->party->userCount) + " miembros";
            std::cout << "[BOSS 835 TEST] " << partyMsg << std::endl;
            LogToFile("[BOSS 835 TEST] " + partyMsg);
        } else {
            std::string noPartyMsg = "Killer NO tiene party";
            std::cout << "[BOSS 835 TEST] " << noPartyMsg << std::endl;
            LogToFile("[BOSS 835 TEST] " + noPartyMsg);
        }
    }

    std::string finalMsg = "Validación final - HasValidParty: " + std::string(hasValidParty ? "YES" : "NO");
    std::cout << "[BOSS DEBUG] " << finalMsg << std::endl;
    LogToFile("[BOSS DEBUG] " + finalMsg);

    if (!hasValidParty) {
        std::string blockMsg = "Party no válida - Bloqueando drop";
        std::cout << "[BOSS DEBUG] " << blockMsg << std::endl;
        LogToFile("[BOSS DEBUG] " + blockMsg);
        LogBlockedDrop(mobId, killer);
        
        // Enviar mensaje del sistema al jugador
        try {
            if (CanSendMessage(killer->id)) {
                GameSystemMessageOutgoing msg{};
                msg.opcode = 0x122;
                msg.isNotification = false;  // Mensaje normal
                msg.messageNumber = 2026;    // "Can't restrict character-related issues."
                
                NetworkHelper::Send(killer, &msg, sizeof(msg));
                
                std::string sendMsg = "Mensaje del sistema 2026 enviado a usuario " + std::to_string(killer->id);
                std::cout << "[BOSS DEBUG] " << sendMsg << std::endl;
                LogToFile("[BOSS DEBUG] " + sendMsg);
            } else {
                std::string cooldownMsg = "Mensaje en cooldown para usuario " + std::to_string(killer->id) + " (8s)";
                std::cout << "[BOSS DEBUG] " << cooldownMsg << std::endl;
                LogToFile("[BOSS DEBUG] " + cooldownMsg);
            }
        }
        catch (...) {
            std::string errorMsg = "Error al enviar mensaje del sistema";
            std::cout << "[BOSS ERROR] " << errorMsg << std::endl;
            LogToFile("[BOSS ERROR] " + errorMsg);
        }
        
        return false; // Bloquear drop
    }

    if (mobId == 835) {
        std::string successMsg = "Killer tiene party válida - Permitiendo drops para todos";
        std::cout << "[BOSS 835 TEST] " << successMsg << std::endl;
        LogToFile("[BOSS 835 TEST] " + successMsg);
    }

    std::string allowMsg = "Party válida - Permitiendo drop";
    std::cout << "[BOSS DEBUG] " << allowMsg << std::endl;
    LogToFile("[BOSS DEBUG] " + allowMsg);
    return true; // Permitir drop
}

bool BossPartyValidator::IsBossRequiringParty(uint32_t mobId)
{
    std::shared_lock<std::shared_mutex> lock(m_mutex);
    return m_rules.find(mobId) != m_rules.end();
}

bool BossPartyValidator::HasValidParty(CUser* user, uint8_t minSize)
{
    if (!user) {
        std::cout << "[BOSS 835 TEST] HasValidParty: User is NULL" << std::endl;
        return false;
    }
    
    if (!user->party) {
        std::cout << "[BOSS 835 TEST] HasValidParty: User has no party" << std::endl;
        return false;
    }
    
    // Contar usuarios activos (no offline/dead)
    int activeMembers = 0;
    int totalMembers = user->party->userCount;
    
    std::cout << "[BOSS 835 TEST] HasValidParty: Analyzing party with " << totalMembers << " total members" << std::endl;
    
    for (int i = 0; i < user->party->userCount && i < 30; ++i) {
        auto& partyUser = user->party->users[i];
        if (partyUser.user) {
            if (partyUser.user->status != UserStatus::Death) {
                activeMembers++;
                std::cout << "[BOSS 835 TEST] Active member: " << partyUser.user->charName.data() << std::endl;
            } else {
                std::cout << "[BOSS 835 TEST] Dead member: " << partyUser.user->charName.data() << std::endl;
            }
        } else {
            std::cout << "[BOSS 835 TEST] Empty party slot at index " << i << std::endl;
        }
    }
    
    std::cout << "[BOSS 835 TEST] HasValidParty: Active=" << activeMembers << " Required=" << (int)minSize << std::endl;
    
    return activeMembers >= minSize;
}

void BossPartyValidator::LogBlockedDrop(uint32_t mobId, CUser* killer)
{
    auto now = std::chrono::system_clock::now();
    std::time_t now_time = std::chrono::system_clock::to_time_t(now);
    
    char buf[100];
    std::tm tm_buf;
    localtime_s(&tm_buf, &now_time);
    std::strftime(buf, sizeof(buf), "[%Y-%m-%d %H:%M:%S]", &tm_buf);

    std::cout << buf << " [BossPartyValidator] Drop bloqueado - MobID: " << mobId 
              << " Player: " << (killer ? killer->charName.data() : "Unknown") 
              << " PartySize: " << (killer && killer->party ? std::to_string(killer->party->userCount) : "0") 
              << std::endl;
}

bool BossPartyValidator::ConnectDB()
{
    if (m_hDbc != nullptr) return true;

    std::cout << "[BossPartyValidator] Iniciando conexión a base de datos..." << std::endl;

    // Usar misma conexión que DynamicDropManager
    SQLHENV hEnv = NULL;
    SQLHDBC hDbc = NULL;

    std::cout << "[BossPartyValidator] Allocating environment handle..." << std::endl;
    if (SQLAllocHandle(SQL_HANDLE_ENV, SQL_NULL_HANDLE, &hEnv) != SQL_SUCCESS) {
        std::cout << "[BossPartyValidator ERROR] Failed to allocate environment handle" << std::endl;
        return false;
    }
    SQLSetEnvAttr(hEnv, SQL_ATTR_ODBC_VERSION, (SQLPOINTER)SQL_OV_ODBC3, 0);

    std::cout << "[BossPartyValidator] Allocating connection handle..." << std::endl;
    if (SQLAllocHandle(SQL_HANDLE_DBC, hEnv, &hDbc) != SQL_SUCCESS) {
        std::cout << "[BossPartyValidator ERROR] Failed to allocate connection handle" << std::endl;
        SQLFreeHandle(SQL_HANDLE_ENV, hEnv);
        return false;
    }

    std::string connStr = "DRIVER={SQL Server};SERVER=158.69.213.250;DATABASE=PS_GameDefs;UID=lotus;PWD=$2a$13$wr34crwF1vcXtwE8wDrwtunwg9cKVlZN6lJwOHwhByN.pMMNIljIK;";
    std::cout << "[BossPartyValidator] Attempting connection to: 158.69.213.250" << std::endl;
    
    SQLCHAR szConnStrOut[1024];
    SQLSMALLINT cbConnStrOut;
    
    std::cout << "[BossPartyValidator] Calling SQLDriverConnect..." << std::endl;
    SQLRETURN ret = SQLDriverConnectA(hDbc, NULL, (SQLCHAR*)connStr.c_str(), SQL_NTS, szConnStrOut, sizeof(szConnStrOut), &cbConnStrOut, SQL_DRIVER_NOPROMPT);
    
    if (!SQL_SUCCEEDED(ret)) {
        std::cout << "[BossPartyValidator ERROR] Connection failed!" << std::endl;
        
        // Obtener error detallado
        SQLCHAR sqlState[6];
        SQLINTEGER nativeError;
        SQLCHAR messageText[256];
        SQLSMALLINT textLength;
        
        if (SQLGetDiagRecA(SQL_HANDLE_DBC, hDbc, 1, sqlState, &nativeError, messageText, sizeof(messageText), &textLength) == SQL_SUCCESS) {
            std::cout << "[BossPartyValidator ERROR] SQL State: " << sqlState << std::endl;
            std::cout << "[BossPartyValidator ERROR] Native Error: " << nativeError << std::endl;
            std::cout << "[BossPartyValidator ERROR] Message: " << messageText << std::endl;
        }
        
        SQLFreeHandle(SQL_HANDLE_DBC, hDbc);
        SQLFreeHandle(SQL_HANDLE_ENV, hEnv);
        return false;
    }

    std::cout << "[BossPartyValidator] Connection successful!" << std::endl;
    m_hEnv = hEnv;
    m_hDbc = hDbc;
    return true;
}

void BossPartyValidator::DisconnectDB()
{
    std::lock_guard<std::recursive_mutex> lock(m_dbMutex);
    if (m_hDbc) {
        SQLDisconnect(m_hDbc);
        SQLFreeHandle(SQL_HANDLE_DBC, m_hDbc);
        m_hDbc = nullptr;
    }
    if (m_hEnv) {
        SQLFreeHandle(SQL_HANDLE_ENV, m_hEnv);
        m_hEnv = nullptr;
    }
}

void BossPartyValidator::Reload()
{
    Load();
}

void BossPartyValidator::SetRule(uint32_t mobId, const BossPartyRule& rule)
{
    std::unique_lock<std::shared_mutex> lock(m_mutex);
    m_rules[mobId] = rule;
}

void BossPartyValidator::RemoveRule(uint32_t mobId)
{
    std::unique_lock<std::shared_mutex> lock(m_mutex);
    m_rules.erase(mobId);
}

void BossPartyValidator::Log(const std::string& message)
{
    std::cout << "[BossPartyValidator] " << message << std::endl;
    LogToFile("[BossPartyValidator] " + message);
}

void BossPartyValidator::LogChange(uint32_t mobId, uint16_t grade, uint32_t rawRate, const std::string& action)
{
    std::string msg = action + " - MobID: " + std::to_string(mobId) + " Grade: " + std::to_string(grade) + " Rate: " + std::to_string(rawRate);
    std::cout << "[BossPartyValidator] " << msg << std::endl;
    LogToFile("[BossPartyValidator] " + msg);
}

void BossPartyValidator::LogError(const std::string& message)
{
    std::string msg = "ERROR: " + message;
    std::cout << "[BossPartyValidator ERROR] " << message << std::endl;
    LogToFile("[BossPartyValidator ERROR] " + message);
}

bool BossPartyValidator::CanSendMessage(uint32_t userId)
{
    auto now = std::chrono::steady_clock::now();
    
    std::unique_lock<std::shared_mutex> lock(m_cooldownMutex);
    
    auto it = m_messageCooldowns.find(userId);
    if (it == m_messageCooldowns.end()) {
        // Primera vez que envía mensaje
        m_messageCooldowns[userId] = now;
        return true;
    }
    
    // Verificar si pasó el cooldown
    auto timeDiff = std::chrono::duration_cast<std::chrono::seconds>(now - it->second).count();
    if (timeDiff >= MESSAGE_COOLDOWN_SECONDS) {
        m_messageCooldowns[userId] = now;
        return true;
    }
    
    return false;
}
