#include <unordered_set>
#include <chrono>
#include <ctime>
#include <string>
#include <windows.h>
#include <sql.h>
#include <sqlext.h>
#include "include/shaiya/DynamicMall.h"
#include "include/shaiya/CGameData.h"
#include "include/shaiya/ProductInfo.h"

using namespace shaiya;

namespace shaiya {
    std::shared_mutex DynamicMallManager::m_mutex;
    std::unordered_map<std::string, DynamicMallEntry> DynamicMallManager::m_products;
    std::vector<std::string> DynamicMallManager::m_modifiedProducts;
    void* DynamicMallManager::m_hEnv = nullptr;
    void* DynamicMallManager::m_hDbc = nullptr;
    std::recursive_mutex DynamicMallManager::m_dbMutex;

    void DynamicMallManager::DisconnectDB() {
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

    // Helper: Registra errores graves
    void LogMallErrorToFile(const std::string& errMessage) {
        static std::string errPath;
        if (errPath.empty()) {
            char buffer[MAX_PATH];
            GetModuleFileNameA(NULL, buffer, MAX_PATH);
            std::string exepath = buffer;
            std::string exeFolder = exepath.substr(0, exepath.find_last_of("\\/"));
            errPath = exeFolder + "\\DynamicMall_Error.log";
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

    bool DynamicMallManager::ConnectDB() {
        std::lock_guard<std::recursive_mutex> lock(m_dbMutex);
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
            LogMallErrorToFile("Fallo al conectar a SQL Server nativamente para Mall.");
            SQLFreeHandle(SQL_HANDLE_DBC, hDbc);
            SQLFreeHandle(SQL_HANDLE_ENV, hEnv);
            return false;
        }

        m_hEnv = hEnv;
        m_hDbc = hDbc;
        return true;
    }

    void DynamicMallManager::CheckReloadTrigger() {
        using namespace std::chrono_literals;
        static std::chrono::system_clock::time_point next_check;
        static bool g_hasInitialLoad = false;

        if (!g_hasInitialLoad) {
            Log("Iniciando modulo de Tienda en Vivo...");
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

        const char* query = "SELECT ReloadMall FROM PS_GameDefs.dbo.ServerControl";
        if (SQL_SUCCEEDED(SQLExecDirectA(hStmt, (SQLCHAR*)query, SQL_NTS))) {
            SQLINTEGER reloadRequested = 0;
            SQLLEN len;
            if (SQL_SUCCEEDED(SQLFetch(hStmt))) {
                SQLGetData(hStmt, 1, SQL_C_LONG, &reloadRequested, 0, &len);
                if (reloadRequested == 1) {
                    Log("Señal detectada en Mall SQL. Iniciando recarga de ofertas...");
                    Load();
                    SQLCloseCursor(hStmt); 

                    const char* update = "UPDATE PS_GameDefs.dbo.ServerControl SET ReloadMall = 0";
                    SQLExecDirectA(hStmt, (SQLCHAR*)update, SQL_NTS);
                }
            }
        } else {
            LogMallErrorToFile("La conexión de Mall Trigger falló, reiniciando driver...");
            SQLFreeHandle(SQL_HANDLE_STMT, hStmt);
            DisconnectDB();
            return;
        }
        SQLFreeHandle(SQL_HANDLE_STMT, hStmt);
    }

    std::string GetMallLogFilePath() {
        static std::string cachedPath;
        if (cachedPath.empty()) {
            char buffer[MAX_PATH];
            GetModuleFileNameA(NULL, buffer, MAX_PATH);
            std::string exepath = buffer;
            std::string exeFolder = exepath.substr(0, exepath.find_last_of("\\/"));
            std::string dataFolder = exeFolder + "\\Data";
            CreateDirectoryA(dataFolder.c_str(), NULL);
            cachedPath = dataFolder + "\\MallChanged.ini";
        }
        return cachedPath;
    }

    // Funcion auxiliar para limpiar espacios muertos del ProductCode que extrae el ODBC
    std::string TrimSpaces(const char* str) {
        std::string s(str);
        s.erase(s.find_last_not_of(" \n\r\t") + 1);
        return s;
    }

    void DynamicMallManager::Load() {
        if (!ConnectDB()) {
            LogMallErrorToFile("Aviso Crítico: No se pudo conectar a DB para Mall.");
            return;
        }

        std::lock_guard<std::recursive_mutex> lock(m_dbMutex);

        SQLHSTMT hStmt = NULL;
        if (SQLAllocHandle(SQL_HANDLE_STMT, m_hDbc, &hStmt) != SQL_SUCCESS) return;

        static bool isFirstLoad = true;
        
        // Shadow Buffer
        std::unordered_map<std::string, DynamicMallEntry> tempProducts;
        std::unordered_set<std::string> tempModifiedSet;

        // Armamos el super Query
        // Solicitamos el Codigo, Precio, y los 24 Slots de Item y Cantidad
        std::string query = "SELECT ProductCode, BuyCost";
        for (int i = 1; i <= 24; ++i) {
            query += ", ItemID" + std::to_string(i) + ", ItemCount" + std::to_string(i);
        }
        query += " FROM PS_GameDefs.dbo.ProductList";

        SQLRETURN fetchRet = SQLExecDirectA(hStmt, (SQLCHAR*)query.c_str(), SQL_NTS);

        if (!SQL_SUCCEEDED(fetchRet)) {
            LogMallErrorToFile("La ejecución de la query del Mall falló.");
            SQLFreeHandle(SQL_HANDLE_STMT, hStmt);
            return;
        }

        // Bindeado de variables. 1 string + 1 int + 48 ints = 50 columnas totales.
        SQLCHAR productCodeRaw[21];
        SQLINTEGER buyCost;
        SQLINTEGER itemIdsArr[24];
        SQLINTEGER itemCountsArr[24];
        
        SQLLEN lenCode, lenCost;
        SQLLEN lenItemIds[24], lenItemCounts[24];

        SQLBindCol(hStmt, 1, SQL_C_CHAR, productCodeRaw, sizeof(productCodeRaw), &lenCode);
        SQLBindCol(hStmt, 2, SQL_C_LONG, &buyCost, 0, &lenCost);
        
        for (int i = 0; i < 24; ++i) {
            SQLBindCol(hStmt, 3 + (i*2), SQL_C_LONG, &itemIdsArr[i], 0, &lenItemIds[i]);
            SQLBindCol(hStmt, 4 + (i*2), SQL_C_LONG, &itemCountsArr[i], 0, &lenItemCounts[i]);
        }

        while (SQL_SUCCEEDED(SQLFetch(hStmt))) {
            std::string codeStr = TrimSpaces((const char*)productCodeRaw);
            if (codeStr.empty()) continue; 

            DynamicMallEntry entry;
            entry.purchasePoints = (uint32_t)buyCost;
            
            for (int i = 0; i < 24; ++i) {
                // Manejo de nulos de SQL
                if (lenItemIds[i] == SQL_NULL_DATA) entry.itemIds[i] = 0; 
                else entry.itemIds[i] = (uint32_t)itemIdsArr[i];

                if (lenItemCounts[i] == SQL_NULL_DATA) entry.itemCounts[i] = 0;
                else entry.itemCounts[i] = (uint8_t)itemCountsArr[i];
            }

            tempProducts[codeStr] = entry;
            tempModifiedSet.insert(codeStr);
        }
        SQLCloseCursor(hStmt);
        SQLFreeHandle(SQL_HANDLE_STMT, hStmt);

        // --- Analizar y Loguear Diferencias ---
        std::unordered_map<std::string, DynamicMallEntry> oldProducts;
        {
            std::shared_lock<std::shared_mutex> lockRead(m_mutex);
            oldProducts = m_products;
        }

        FILE* initialFile = nullptr;
        if (isFirstLoad) {
            std::string logPath = GetMallLogFilePath();
            errno_t err = fopen_s(&initialFile, logPath.c_str(), "w");
            if (err == 0 && initialFile != nullptr) {
                fprintf(initialFile, "=== Inicializacion de Inventario Dinamico (Item Mall) ===\n\n");
            } else {
                LogMallErrorToFile("ERROR CRÍTICO: Incapaz de crear el archivo en: " + logPath);
            }
        }

        auto now = std::chrono::system_clock::now();
        std::time_t now_time = std::chrono::system_clock::to_time_t(now);
        char buf[100];
        std::tm tm_buf;
        localtime_s(&tm_buf, &now_time);
        std::strftime(buf, sizeof(buf), "[%Y-%m-%d %H:%M:%S]", &tm_buf);

        int changedCount = 0;
        std::vector<std::string> productsToUpdate;

        for (const auto& [code, newEntry] : tempProducts) {
            bool isChanged = false;
            std::string actionType = isFirstLoad ? "Loaded" : "Added";

            if (!isFirstLoad) {
                auto oldIt = oldProducts.find(code);
                if (oldIt != oldProducts.end()) {
                    if (oldIt->second.purchasePoints != newEntry.purchasePoints) {
                        actionType = "Price_Changed";
                        isChanged = true;
                    } else {
                        // Revisar si cambió un ID o la Cantidad dentro de la bolsa de 24
                        for (int i=0; i<24; ++i) {
                            if (oldIt->second.itemIds[i] != newEntry.itemIds[i] || oldIt->second.itemCounts[i] != newEntry.itemCounts[i]) {
                                actionType = "Content_Changed";
                                isChanged = true;
                                break;
                            }
                        }
                    }
                } else {
                    isChanged = true;
                }
            } else {
                isChanged = true;
            }

            if (isChanged) {
                productsToUpdate.push_back(code);
                if (isFirstLoad && initialFile != nullptr) {
                    fprintf(initialFile, "%s\nProducto=%s\nNuevo Precio=%u AP\nAction=%s\n\n", buf, code.c_str(), newEntry.purchasePoints, actionType.c_str());
                } else if (!isFirstLoad) {
                    LogChange(code, newEntry.purchasePoints, actionType);
                }
                changedCount++;
            }
        }

        if (isFirstLoad && initialFile != nullptr) fclose(initialFile);

        // --- MEMORY HOT-SWAP (Actualizacion de Punteros Oficiales en Shaiya RAM) ---

        for (const std::string& targetCode : productsToUpdate) {
            auto info = CGameData::GetProductInfo(targetCode.c_str());
            if (info) {
                auto& newValues = tempProducts[targetCode];
                info->purchasePoints = newValues.purchasePoints;
                for (int i = 0; i < 24; ++i) {
                    info->itemIds[i] = newValues.itemIds[i];
                    info->itemCounts[i] = newValues.itemCounts[i];
                }
            } else {
                if (!isFirstLoad) {
                    LogMallErrorToFile("Aviso: El código " + targetCode + " no se encontró en la memoria RAM (No mapeado en inicio).");
                }
            }
        }

        {
            std::unique_lock<std::shared_mutex> writeLock(m_mutex);
            m_products = std::move(tempProducts);
            m_modifiedProducts.assign(tempModifiedSet.begin(), tempModifiedSet.end());
        }

        isFirstLoad = false;
        if(changedCount > 0) Log("Mall Sync listo. " + std::to_string(m_modifiedProducts.size()) + " Paquetes mapeados. Novedades inyectadas: " + std::to_string(changedCount));
    }

    void DynamicMallManager::Log(const std::string& message) {
        std::string logPath = GetMallLogFilePath();
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
        }
    }

    void DynamicMallManager::LogChange(const std::string& productCode, uint32_t buyCost, const std::string& action) {
        std::string logPath = GetMallLogFilePath();
        FILE* file;
        errno_t err = fopen_s(&file, logPath.c_str(), "a");
        if (err == 0 && file != nullptr) {
            auto now = std::chrono::system_clock::now();
            std::time_t now_time = std::chrono::system_clock::to_time_t(now);
            char buf[100];
            std::tm tm_buf;
            localtime_s(&tm_buf, &now_time);
            std::strftime(buf, sizeof(buf), "[%Y-%m-%d %H:%M:%S]", &tm_buf);
            
            fprintf(file, "%s\n", buf);
            fprintf(file, "Producto=%s\n", productCode.c_str());
            fprintf(file, "Nuevo Precio=%u AP\n", buyCost);
            fprintf(file, "Action=%s\n\n", action.c_str());
            fclose(file);
        }
    }
}
