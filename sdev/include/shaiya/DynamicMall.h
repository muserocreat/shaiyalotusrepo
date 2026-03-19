#pragma once
#include <unordered_map>
#include <array>
#include <vector>
#include <string>
#include <shared_mutex>
#include <mutex>
#include <shaiya/include/common.h>
#include "include/shaiya/ProductInfo.h"

namespace shaiya {

    struct DynamicMallEntry {
        uint32_t purchasePoints = 0;
        std::array<uint32_t, 24> itemIds = {0};
        std::array<uint8_t, 24> itemCounts = {0};
    };

    class DynamicMallManager {
    public:
        // Carga los datos de PS_GameDefs.dbo.ProductList
        static void Load();
        static void CheckReloadTrigger();
        
        // Verifica si un producto tiene datos dinámicos cargados (Opcional para Debug)
        static bool HasDynamicProduct(const std::string& productCode);

        // Retorna la lista de Códigos modificados para el log
        static const std::vector<std::string>& GetModifiedProductList();

        // Escribe en el doc MallChanged.ini
        static void Log(const std::string& message);
        
        // Escribe el log estructurado de cambios
        static void LogChange(const std::string& productCode, uint32_t buyCost, const std::string& action);

    private:
        static std::shared_mutex m_mutex;
        static std::unordered_map<std::string, DynamicMallEntry> m_products;
        static std::vector<std::string> m_modifiedProducts;
        
        static void* m_hEnv;
        static void* m_hDbc;
        static std::recursive_mutex m_dbMutex;
        static bool ConnectDB();
        static void DisconnectDB();
    };
}
