#pragma once
#include <unordered_map>
#include <array>
#include <vector>
#include <string>
#include <shared_mutex>
#include <mutex>
#include <shaiya/include/common.h>

namespace shaiya {

    struct DynamicDropEntry {
        uint16_t grade = 0;
        uint32_t rate = 0;
    };

    class DynamicDropManager {
    public:
        // Carga los datos de PS_GameDefs.dbo.MobItems
        static void Load();
        static void CheckReloadTrigger();
        
        // Obtiene el drop dinámico para un mob y orden específico de forma Thread-Safe
        static bool GetDrop(uint32_t mobId, uint8_t itemOrder, uint16_t& outGrade, uint32_t& outRate);

        // Escribe en el doc DropChanged.ini
        static void Log(const std::string& message);
        
        // Escribe el log estructurado de cambios
        static void LogChange(uint32_t mobId, uint16_t grade, uint32_t rawRate, const std::string& action);

    private:
        static std::shared_mutex m_mutex; // Previene "race conditions" (Crashes)
        static std::unordered_map<uint32_t, std::array<DynamicDropEntry, 10>> m_drops;
        static std::vector<uint32_t> m_modifiedMobs;
        
        static void* m_hEnv;
        static void* m_hDbc;
        static std::recursive_mutex m_dbMutex;
        static bool ConnectDB();
        static void DisconnectDB();
    };
}
