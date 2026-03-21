#pragma once
#include <unordered_map>
#include <shared_mutex>
#include <string>
#include <chrono>

namespace shaiya {
    struct CUser;
    struct CMob;

    #pragma pack(push, 1)
    struct BossPartyRule {
        uint32_t mobId;
        uint8_t minPartySize;
        bool requireParty;
        char message[128];
    };
    #pragma pack(pop)

    class BossPartyValidator {
    public:
        // Carga desde BD
        static void Load();
        static void CheckReloadTrigger();

        // Validación principal (capa intermedia)
        static bool ValidateDrop(uint32_t mobId, CUser* killer);
        
        // Verificaciones específicas
        static bool IsBossRequiringParty(uint32_t mobId);
        static bool HasValidParty(CUser* user, uint8_t minSize);
        
        // Logging
        static void LogBlockedDrop(uint32_t mobId, CUser* killer);
        static void Log(const std::string& message);

    private:
        static std::shared_mutex m_mutex;
        static std::unordered_map<uint32_t, BossPartyRule> m_rules;
        
        // Cooldown para mensajes (userID -> timestamp)
        static std::unordered_map<uint32_t, std::chrono::steady_clock::time_point> m_messageCooldowns;
        static std::shared_mutex m_cooldownMutex;
        static const int MESSAGE_COOLDOWN_SECONDS = 8;
        
        // Base de datos
        static void* m_hEnv;
        static void* m_hDbc;
        static std::recursive_mutex m_dbMutex;
        static bool ConnectDB();
        static void DisconnectDB();
        static void LogError(const std::string& message);
        
        static bool CanSendMessage(uint32_t userId);
    };
}
