#include <unordered_set>
#include <chrono>
#include <ctime>
#include <string>
#include <windows.h> // Para GetModuleFileNameA
#include <sql.h>
#include <sqlext.h>
#include "include/shaiya/DynamicDrop.h"
#include "include/shaiya/CGameData.h"
#include "include/shaiya/MobInfo.h"

using namespace shaiya;

namespace shaiya {
    std::shared_mutex DynamicDropManager::m_mutex;
    std::unordered_map<uint32_t, std::array<DynamicDropEntry, 10>> DynamicDropManager::m_drops;
    std::vector<uint32_t> DynamicDropManager::m_modifiedMobs;
    void* DynamicDropManager::m_hEnv = nullptr;
    void* DynamicDropManager::m_hDbc = nullptr;
    std::recursive_mutex DynamicDropManager::m_dbMutex;

    void DynamicDropManager::DisconnectDB() {
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

    // Helper: Registra errores graves si la consola no esta visible (Servicios)
    void LogErrorToFile(const std::string& errMessage) {
        static std::string errPath;
        if (errPath.empty()) {
            char buffer[MAX_PATH];
            GetModuleFileNameA(NULL, buffer, MAX_PATH);
            std::string exepath = buffer;
            std::string exeFolder = exepath.substr(0, exepath.find_last_of("\\/"));
            errPath = exeFolder + "\\DynamicDrop_Error.log";
        }

        FILE* file;
        if (fopen_s(&file, errPath.c_str(), "a") == 0 && file != nullptr) {
            auto now = std::chrono::system_clock::now();
            std::time_t now_time = std::chrono::system_clock::to_time_t(now);
            char buf[100];
            std::tm tm_buf;
            localtime_s(&tm_buf, &now_time);
            std::strftime(buf, sizeof(buf), "[%Y-%m-%d %H:%M:%S]", &tm_buf);

            fprintf(file, "%s %s\n", buf, errMessage.c_str());
            fclose(file);
        }
    }

    bool DynamicDropManager::ConnectDB() {
        if (m_hDbc != nullptr) return true;

        SQLHENV hEnv = NULL;
        SQLHDBC hDbc = NULL;

        if (SQLAllocHandle(SQL_HANDLE_ENV, SQL_NULL_HANDLE, &hEnv) != SQL_SUCCESS) return false;
        SQLSetEnvAttr(hEnv, SQL_ATTR_ODBC_VERSION, (SQLPOINTER)SQL_OV_ODBC3, 0);

        if (SQLAllocHandle(SQL_HANDLE_DBC, hEnv, &hDbc) != SQL_SUCCESS) {
            SQLFreeHandle(SQL_HANDLE_ENV, hEnv);
            return false;
        }

        std::string connStr = "DRIVER={SQL Server};SERVER=158.69.213.250;DATABASE=PS_GameDefs;UID=lotus;PWD=$2a$13$wr34crwF1vcXtwE8wDrwtunwg9cKVlZN6lJwOHwhByN.pMMNIljIK;";
        SQLCHAR szConnStrOut[1024];
        SQLSMALLINT cbConnStrOut;
        
        SQLRETURN ret = SQLDriverConnectA(hDbc, NULL, (SQLCHAR*)connStr.c_str(), SQL_NTS, szConnStrOut, sizeof(szConnStrOut), &cbConnStrOut, SQL_DRIVER_NOPROMPT);
        if (!SQL_SUCCEEDED(ret)) {
            LogErrorToFile("Fallo al conectar a SQL Server nativamente. El driver ODBC devolvió error al intentar loguearse con 'lotus'.");
            SQLFreeHandle(SQL_HANDLE_DBC, hDbc);
            SQLFreeHandle(SQL_HANDLE_ENV, hEnv);
            return false;
        }

        m_hEnv = hEnv;
        m_hDbc = hDbc;
        return true;
    }

    void DynamicDropManager::CheckReloadTrigger() {
        using namespace std::chrono_literals;
        static std::chrono::system_clock::time_point next_check;
        static bool g_hasInitialLoad = false;

        if (!g_hasInitialLoad) {
            Log("Iniciando conexion nativa (lotus) desde el hilo padre...");
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

        const char* query = "SELECT ReloadDrops FROM PS_GameDefs.dbo.ServerControl";
        if (SQL_SUCCEEDED(SQLExecDirectA(hStmt, (SQLCHAR*)query, SQL_NTS))) {
            SQLINTEGER reloadRequested = 0;
            SQLLEN len;
            if (SQL_SUCCEEDED(SQLFetch(hStmt))) {
                SQLGetData(hStmt, 1, SQL_C_LONG, &reloadRequested, 0, &len);
                if (reloadRequested == 1) {
                    Log("Señal detectada en SQL. Iniciando recarga...");
                    Load();
                    SQLCloseCursor(hStmt); 

                    const char* update = "UPDATE PS_GameDefs.dbo.ServerControl SET ReloadDrops = 0";
                    SQLExecDirectA(hStmt, (SQLCHAR*)update, SQL_NTS);
                }
            }
        } else {
            LogErrorToFile("La conexión de Reload Trigger falló, reiniciando driver...");
            SQLFreeHandle(SQL_HANDLE_STMT, hStmt);
            DisconnectDB();
            return;
        }
        SQLFreeHandle(SQL_HANDLE_STMT, hStmt);
    }

    // Helper: Resuelve la ruta dinámica y auto-crea la carpeta Data si el servicio la requiere
    std::string GetLogFilePath() {
        static std::string cachedPath;
        if (cachedPath.empty()) {
            char buffer[MAX_PATH];
            GetModuleFileNameA(NULL, buffer, MAX_PATH);
            std::string exepath = buffer;
            std::string exeFolder = exepath.substr(0, exepath.find_last_of("\\/"));
            std::string dataFolder = exeFolder + "\\Data";
            CreateDirectoryA(dataFolder.c_str(), NULL);
            cachedPath = dataFolder + "\\DropChanged.ini";
        }
        return cachedPath;
    }

    void DynamicDropManager::Load() {
        if (!ConnectDB()) {
            LogErrorToFile("Aviso Crítico: No se pudo conectar nativamente a DB. Abandono seguro.");
            return;
        }

        std::lock_guard<std::recursive_mutex> lock(m_dbMutex);

        SQLHSTMT hStmt = NULL;
        if (SQLAllocHandle(SQL_HANDLE_STMT, m_hDbc, &hStmt) != SQL_SUCCESS) return;

        static bool isFirstLoad = true;
        
        // --- 1. SHADOW BUFFERING (Variables Temporales Seguras) ---
        std::unordered_map<uint32_t, std::array<DynamicDropEntry, 10>> tempDrops;
        std::unordered_set<uint32_t> tempModifiedSet;

        const char* query = "SELECT MobID, ItemOrder, Grade, DropRate FROM PS_GameDefs.dbo.MobItems WHERE ItemOrder < 10";
        
        SQLRETURN fetchRet = SQLExecDirectA(hStmt, (SQLCHAR*)query, SQL_NTS);

        if (!SQL_SUCCEEDED(fetchRet)) {
            LogErrorToFile("La ejecución de la query de carga nativa falló.");
            SQLFreeHandle(SQL_HANDLE_STMT, hStmt);
            return;
        }

        SQLINTEGER mobId, itemOrder, grade, rate;
        SQLLEN lenMobId, lenItemOrder, lenGrade, lenRate;

        SQLBindCol(hStmt, 1, SQL_C_LONG, &mobId, 0, &lenMobId);
        SQLBindCol(hStmt, 2, SQL_C_LONG, &itemOrder, 0, &lenItemOrder);
        SQLBindCol(hStmt, 3, SQL_C_LONG, &grade, 0, &lenGrade);
        SQLBindCol(hStmt, 4, SQL_C_LONG, &rate, 0, &lenRate);

        while (SQL_SUCCEEDED(SQLFetch(hStmt))) {
            if (mobId == 0 || mobId > 30000) continue; // Seguridad: Ignorar IDs basura o fuera de rango

            if (itemOrder >= 0 && itemOrder < 10) {
                tempDrops[mobId][itemOrder] = { (uint16_t)grade, (uint32_t)rate };
                tempModifiedSet.insert((uint32_t)mobId);
            }
        }
        SQLCloseCursor(hStmt);
        SQLFreeHandle(SQL_HANDLE_STMT, hStmt);

        // --- 2. DIFFING LOGS (Analizar qué cambió) ---
        // Extraemos una copia local de los viejos drops de forma segura (Reader Lock)
        std::unordered_map<uint32_t, std::array<DynamicDropEntry, 10>> oldDrops;
        {
            std::shared_lock<std::shared_mutex> lock(m_mutex);
            oldDrops = m_drops;
        }

        // Si es la carga inicial, reiniciamos el archivo de log e imprimimos el estandarte
        FILE* initialFile = nullptr;
        if (isFirstLoad) {
            std::string logPath = GetLogFilePath();
            errno_t err = fopen_s(&initialFile, logPath.c_str(), "w");
            if (err == 0 && initialFile != nullptr) {
                fprintf(initialFile, "Se han inicializado todos los Mobs\n\n");
            } else {
                LogErrorToFile("ERROR CRÍTICO " + std::to_string(err) + ": Incapaz de crear el archivo en: " + logPath);
            }
        }

        auto now = std::chrono::system_clock::now();
        std::time_t now_time = std::chrono::system_clock::to_time_t(now);
        char buf[100];
        std::tm tm_buf;
        localtime_s(&tm_buf, &now_time);
        std::strftime(buf, sizeof(buf), "[%Y-%m-%d %H:%M:%S]", &tm_buf);

        int changedCount = 0;
        std::vector<uint32_t> mobsToUpdate;

        for (const auto& [tmpMobId, tmpArray] : tempDrops) {
            bool mobRowNeedsUpdate = false;
            for (int i = 0; i < 10; ++i) {
                auto newGrade = tmpArray[i].grade;
                auto newRate = tmpArray[i].rate;
                if (newGrade == 0 && newRate == 0) continue; // Slots vacíos

                bool isChanged = false;
                std::string actionType = isFirstLoad ? "Loaded" : "Added";

                if (!isFirstLoad) {
                    auto oldIt = oldDrops.find(tmpMobId);
                    if (oldIt != oldDrops.end()) {
                        auto oldGrade = oldIt->second[i].grade;
                        auto oldRate = oldIt->second[i].rate;
                        if (oldGrade != newGrade || oldRate != newRate) {
                            if (oldGrade != 0 || oldRate != 0) actionType = "Changed";
                            isChanged = true;
                        }
                    } else {
                        isChanged = true;
                    }
                } else {
                    isChanged = true;
                }

                if (isChanged) {
                    mobRowNeedsUpdate = true;
                    float visualPercent = static_cast<float>(newRate) / 100.0f; // Shaiya Base Rate: 10000 = 100%
                    if (isFirstLoad && initialFile != nullptr) {
                        fprintf(initialFile, "%s\nMobID=%u\nGrade=%u\nDropRate=%u (%.2f%%)\nAction=%s\n\n", buf, tmpMobId, newGrade, newRate, visualPercent, actionType.c_str());
                    } else if (!isFirstLoad) {
                        LogChange(tmpMobId, newGrade, newRate, actionType);
                    }
                    changedCount++;
                }
            }
            if (mobRowNeedsUpdate) mobsToUpdate.push_back(tmpMobId);
        }

        if (isFirstLoad && initialFile != nullptr) {
            fclose(initialFile);
        }

        // --- 3. ATOMIC MEMORY SWAP (Inyección Selectiva y Ultra-Optimizada) ---
        // 3.1: Limpiar drops de mobs eliminados del SQL
        for (uint32_t oldMobId : m_modifiedMobs) {
            if (tempDrops.find(oldMobId) == tempDrops.end()) {
                auto info = CGameData::GetMobInfo(static_cast<int>(oldMobId));
                if (info && info->mobId == (uint16_t)oldMobId) {
                    for (int i = 0; i < 10; ++i) {
                        info->dropInfo[i].grade = 0;
                        info->dropInfo[i].rate = 0;
                    }
                }
            }
        }

        // 3.2: Inyectar SOLO mobs que realmente sufrieron cambios
        for (uint32_t targetId : mobsToUpdate) {
            auto info = CGameData::GetMobInfo(static_cast<int>(targetId));
            if (info && info->mobId == (uint16_t)targetId) {
                auto& newValues = tempDrops[targetId];
                for (int i = 0; i < 10; ++i) {
                    info->dropInfo[i].grade = newValues[i].grade;
                    info->dropInfo[i].rate = newValues[i].rate;
                }
            }
        }

        // Guardamos el Buffer local para el Differentiating del siguiente Reload
        {
            std::unique_lock<std::shared_mutex> lock(m_mutex);
            m_drops = std::move(tempDrops);
            m_modifiedMobs.assign(tempModifiedSet.begin(), tempModifiedSet.end());
        }

        isFirstLoad = false;
        Log("Carga SQL exitosa. " + std::to_string(m_modifiedMobs.size()) + " Mobs inyectados en GameData. Diferencias reportadas: " + std::to_string(changedCount));
    }

    bool DynamicDropManager::GetDrop(uint32_t mobId, uint8_t itemOrder, uint16_t& outGrade, uint32_t& outRate) {
        std::shared_lock<std::shared_mutex> lock(m_mutex);
        auto it = m_drops.find(mobId);
        if (it != m_drops.end() && itemOrder < 10) {
            outGrade = it->second[itemOrder].grade;
            outRate = it->second[itemOrder].rate;
            return true;
        }
        return false;
    }

    void DynamicDropManager::Log(const std::string& message) {
        std::string logPath = GetLogFilePath();
        FILE* file;
        errno_t err = fopen_s(&file, logPath.c_str(), "a");
        if (err == 0 && file != nullptr) {
            auto now = std::chrono::system_clock::now();
            std::time_t now_time = std::chrono::system_clock::to_time_t(now);
            
            char buf[100];
            std::tm tm_buf;
            localtime_s(&tm_buf, &now_time);
            std::strftime(buf, sizeof(buf), "[%Y-%m-%d %H:%M:%S]", &tm_buf);

            fprintf(file, "%s %s\n", buf, message.c_str());
            fclose(file);
        } else {
            LogErrorToFile("Fallo escritura de log estandar. Codigo Win32: " + std::to_string(err));
        }
    }

    void DynamicDropManager::LogChange(uint32_t mobId, uint16_t grade, uint32_t rawRate, const std::string& action) {
        std::string logPath = GetLogFilePath();
        FILE* file;
        errno_t err = fopen_s(&file, logPath.c_str(), "a");
        if (err == 0 && file != nullptr) {
            auto now = std::chrono::system_clock::now();
            std::time_t now_time = std::chrono::system_clock::to_time_t(now);
            
            char buf[100];
            std::tm tm_buf;
            localtime_s(&tm_buf, &now_time);
            std::strftime(buf, sizeof(buf), "[%Y-%m-%d %H:%M:%S]", &tm_buf);
            
            float visualPercent = static_cast<float>(rawRate) / 100.0f;

            fprintf(file, "%s\n", buf);
            fprintf(file, "MobID=%u\n", mobId);
            fprintf(file, "Grade=%u\n", grade);
            fprintf(file, "DropRate=%u (%.2f%%)\n", rawRate, visualPercent);
            fprintf(file, "Action=%s\n\n", action.c_str());
            fclose(file);
        } else {
            LogErrorToFile("Fallo guardado de MobID " + std::to_string(mobId) + ". Permiso denegado. Error: " + std::to_string(err));
        }
    }
}
