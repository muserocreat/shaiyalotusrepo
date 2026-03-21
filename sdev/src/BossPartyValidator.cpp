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
        Log("Iniciando conexion nativa desde el hilo padre...");
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
        SQLFreeHandle(SQL_HANDLE_STMT, hStmt);
        DisconnectDB();
        return;
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
            Log("Carga inicial completada - " + std::to_string(m_rules.size()) + " reglas cargadas");
            isFirstLoad = false;
        } else {
            Log("Recarga completada - " + std::to_string(m_rules.size()) + " reglas (antes: " + std::to_string(oldSize) + ")");
        }
    }
}

bool BossPartyValidator::ValidateDrop(uint32_t mobId, CUser* killer)
{
    // Si no hay regla para este mob, permitir drop
    if (!IsBossRequiringParty(mobId))
        return true;

    // Obtener regla específica
    BossPartyRule rule;
    {
        std::shared_lock<std::shared_mutex> lock(m_mutex);
        auto it = m_rules.find(mobId);
        if (it == m_rules.end())
            return true; // Sin regla = permitir
        rule = it->second;
    }

    // ESCENARIO CRÍTICO: Sin killer identificado
    if (!killer) {
        // Política: Si no podemos identificar al killer, aplicar regla estricta
        // Esto previene exploits donde players solos intentan evitar validación
        LogBlockedDrop(mobId, nullptr);
        return false; // Bloquear drops por seguridad
    }

    // Validar party del killer solo si RequireParty = true
    bool hasValidParty = true;
    if (rule.requireParty) {
        hasValidParty = HasValidParty(killer, rule.minPartySize);
    }

    if (!hasValidParty) {
        LogBlockedDrop(mobId, killer);
        
        // Enviar mensaje del sistema al jugador
        try {
            if (CanSendMessage(killer->id)) {
                GameSystemMessageOutgoing msg{};
                msg.opcode = 0x122;
                msg.isNotification = false;  // Mensaje normal
                msg.messageNumber = 2026;    // "Can't restrict character-related issues."
                
                NetworkHelper::Send(killer, &msg, sizeof(msg));
            }
        }
        catch (...) {
            LogError("Error al enviar mensaje del sistema");
        }
        
        return false; // Bloquear drop
    }

    return true; // Permitir drop
}

bool BossPartyValidator::IsBossRequiringParty(uint32_t mobId)
{
    std::shared_lock<std::shared_mutex> lock(m_mutex);
    return m_rules.find(mobId) != m_rules.end();
}

bool BossPartyValidator::HasValidParty(CUser* user, uint8_t minSize)
{
    if (!user)
        return false;
    
    if (!user->party)
        return false;
    
    // Contar usuarios activos (no offline/dead)
    int activeMembers = 0;
    
    for (int i = 0; i < user->party->userCount && i < 30; ++i) {
        auto& partyUser = user->party->users[i];
        if (partyUser.user && partyUser.user->status != UserStatus::Death) {
            activeMembers++;
        }
    }
    
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

    std::string msg = std::string(buf) + " [BossPartyValidator] Drop bloqueado - MobID: " + std::to_string(mobId)
        + " Player: " + (killer ? killer->charName.data() : "Unknown")
        + " PartySize: " + (killer && killer->party ? std::to_string(killer->party->userCount) : "0");
    LogToFile(msg);
}

bool BossPartyValidator::ConnectDB()
{
    if (m_hDbc != nullptr) return true;

    SQLHENV hEnv = NULL;
    SQLHDBC hDbc = NULL;

    if (SQLAllocHandle(SQL_HANDLE_ENV, SQL_NULL_HANDLE, &hEnv) != SQL_SUCCESS) {
        LogError("Failed to allocate environment handle");
        return false;
    }
    SQLSetEnvAttr(hEnv, SQL_ATTR_ODBC_VERSION, (SQLPOINTER)SQL_OV_ODBC3, 0);

    if (SQLAllocHandle(SQL_HANDLE_DBC, hEnv, &hDbc) != SQL_SUCCESS) {
        LogError("Failed to allocate connection handle");
        SQLFreeHandle(SQL_HANDLE_ENV, hEnv);
        return false;
    }

    std::string connStr = "DRIVER={SQL Server};SERVER=158.69.213.250;DATABASE=PS_GameDefs;UID=lotus;PWD=$2a$13$wr34crwF1vcXtwE8wDrwtunwg9cKVlZN6lJwOHwhByN.pMMNIljIK;";
    
    SQLCHAR szConnStrOut[1024];
    SQLSMALLINT cbConnStrOut;
    
    SQLRETURN ret = SQLDriverConnectA(hDbc, NULL, (SQLCHAR*)connStr.c_str(), SQL_NTS, szConnStrOut, sizeof(szConnStrOut), &cbConnStrOut, SQL_DRIVER_NOPROMPT);
    
    if (!SQL_SUCCEEDED(ret)) {
        SQLCHAR sqlState[6];
        SQLINTEGER nativeError;
        SQLCHAR messageText[256];
        SQLSMALLINT textLength;
        
        if (SQLGetDiagRecA(SQL_HANDLE_DBC, hDbc, 1, sqlState, &nativeError, messageText, sizeof(messageText), &textLength) == SQL_SUCCESS) {
            LogError("Connection failed - SQL State: " + std::string((char*)sqlState) + " Error: " + std::string((char*)messageText));
        } else {
            LogError("Connection failed - Unknown error");
        }
        
        SQLFreeHandle(SQL_HANDLE_DBC, hDbc);
        SQLFreeHandle(SQL_HANDLE_ENV, hEnv);
        return false;
    }

    Log("Conexión a base de datos exitosa");
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
    LogToFile("[BossPartyValidator] " + message);
}

void BossPartyValidator::LogChange(uint32_t mobId, uint16_t grade, uint32_t rawRate, const std::string& action)
{
    std::string msg = action + " - MobID: " + std::to_string(mobId) + " Grade: " + std::to_string(grade) + " Rate: " + std::to_string(rawRate);
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
