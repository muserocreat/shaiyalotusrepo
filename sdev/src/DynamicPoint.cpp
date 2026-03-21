#include <string>
#include <vector>
#include <chrono>
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <sql.h>
#include <sqlext.h>
#include <mutex>
#include <util/util.h>
#include <shaiya/include/network/dbAgent/incoming/0E00.h>
#include "include/main.h"
#include "include/shaiya/CUser.h"
#include "include/shaiya/CWorld.h"
#include "include/shaiya/DynamicPoint.h"
#include "include/shaiya/NetworkHelper.h"

using namespace shaiya;

namespace shaiya
{
    // Estado de conexion ODBC propio del modulo
    SQLHENV g_hEnvPoint = NULL;
    SQLHDBC g_hDbcPoint = NULL;
    std::recursive_mutex g_dbMutexPoint;

    void DisconnectDBPoint()
    {
        if (g_hDbcPoint) {
            SQLDisconnect(g_hDbcPoint);
            SQLFreeHandle(SQL_HANDLE_DBC, g_hDbcPoint);
            g_hDbcPoint = NULL;
        }
        if (g_hEnvPoint) {
            SQLFreeHandle(SQL_HANDLE_ENV, g_hEnvPoint);
            g_hEnvPoint = NULL;
        }
    }

    bool ConnectDBPoint()
    {
        if (g_hDbcPoint != NULL) return true;

        SQLHENV hEnv = NULL;
        SQLHDBC hDbc = NULL;

        if (SQLAllocHandle(SQL_HANDLE_ENV, SQL_NULL_HANDLE, &hEnv) != SQL_SUCCESS)
            return false;

        SQLSetEnvAttr(hEnv, SQL_ATTR_ODBC_VERSION, (SQLPOINTER)SQL_OV_ODBC3, 0);

        if (SQLAllocHandle(SQL_HANDLE_DBC, hEnv, &hDbc) != SQL_SUCCESS) {
            SQLFreeHandle(SQL_HANDLE_ENV, hEnv);
            return false;
        }

        std::string connStr = "DRIVER={SQL Server};SERVER=158.69.213.250;DATABASE=PS_GameDefs;UID=lotus;PWD=$2a$13$wr34crwF1vcXtwE8wDrwtunwg9cKVlZN6lJwOHwhByN.pMMNIljIK;";
        SQLCHAR szConnStrOut[1024];
        SQLSMALLINT cbConnStrOut;

        SQLRETURN ret = SQLDriverConnectA(hDbc, NULL, (SQLCHAR*)connStr.c_str(), SQL_NTS,
            szConnStrOut, sizeof(szConnStrOut), &cbConnStrOut, SQL_DRIVER_NOPROMPT);

        if (!SQL_SUCCEEDED(ret)) {
            SQLFreeHandle(SQL_HANDLE_DBC, hDbc);
            SQLFreeHandle(SQL_HANDLE_ENV, hEnv);
            return false;
        }

        g_hEnvPoint = hEnv;
        g_hDbcPoint = hDbc;
        return true;
    }

    void DynamicPointManager::CheckReloadTrigger()
    {
        using namespace std::chrono_literals;

        // Delay inicial identico al usado en DynamicDrop/DynamicMall:
        // Espera 15 segundos antes del primer intento de conexion SQL
        // para evitar colisiones con la CriticalSection del hilo principal.
        static std::chrono::system_clock::time_point next_check;
        static bool g_ready = false;

        if (!g_ready) {
            static auto start = std::chrono::system_clock::now();
            if (std::chrono::system_clock::now() - start < 15s)
                return;
            g_ready = true;
            next_check = std::chrono::system_clock::now();
        }

        auto now = std::chrono::system_clock::now();
        if (now < next_check) return;
        next_check = now + 3000ms;

        if (!ConnectDBPoint()) return;

        std::lock_guard<std::recursive_mutex> lock(g_dbMutexPoint);

        SQLHSTMT hStmt = NULL;
        if (SQLAllocHandle(SQL_HANDLE_STMT, g_hDbcPoint, &hStmt) != SQL_SUCCESS) return;

        std::vector<int> userUIDs;

        const char* query = "DELETE FROM PS_GameDefs.dbo.PointReloadQueue OUTPUT DELETED.UserUID";
        if (SQL_SUCCEEDED(SQLExecDirectA(hStmt, (SQLCHAR*)query, SQL_NTS))) {
            int userUID = 0;
            SQLLEN len;
            while (SQL_SUCCEEDED(SQLFetch(hStmt))) {
                SQLGetData(hStmt, 1, SQL_C_LONG, &userUID, 0, &len);
                userUIDs.push_back(userUID);
            }
        } else {
            // Conexion caida, reintentar la proxima vez
            SQLFreeHandle(SQL_HANDLE_STMT, hStmt);
            DisconnectDBPoint();
            return;
        }

        SQLFreeHandle(SQL_HANDLE_STMT, hStmt);

        for (int userUID : userUIDs) {
            CUser* user = CWorld::FindUserBill(userUID);
            if (user) {
                DBAgentPointReloadIncoming outgoing{};
                outgoing.billingId = user->billingId;
                NetworkHelper::SendDBAgent(&outgoing, sizeof(DBAgentPointReloadIncoming));
            }
        }
    }
}
