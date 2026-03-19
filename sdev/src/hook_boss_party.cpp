#include <util/util.h>
#include "include/main.h"
#include "include/shaiya/BossPartyValidator.h"
#include "include/shaiya/CUser.h"
#include "include/shaiya/CMob.h"
#include "include/shaiya/MobInfo.h"
using namespace shaiya;

namespace boss_party_hook
{
    /// <summary>
    /// Hook principal que intercepta la generación de drops
    /// Se integra con DynamicDropManager en la misma dirección 0x48D2E1
    /// </summary>
    extern "C" void GetBossPartyValidatedDrop(MobInfo* info, int itemOrder, uint16_t* outGrade, uint32_t* outRate)
    {
        if (!info) {
            *outGrade = 0;
            *outRate = 0;
            return;
        }

        uint32_t mobId = static_cast<uint32_t>(info->mobId);
        
        // Obtener killer actual (necesitamos encontrar cómo obtenerlo desde MobInfo)
        // Por ahora, validamos sin killer - esto se puede mejorar
        CUser* killer = nullptr; // TODO: Encontrar cómo obtener el killer desde MobInfo
        
        // Validar usando BossPartyValidator
        if (!BossPartyValidator::ValidateDrop(mobId, killer)) {
            // Drop bloqueado
            *outGrade = 0;
            *outRate = 0;
            return;
        }
        
        // Si pasa validación, usar DynamicDropManager normal
        if (!DynamicDropManager::GetDrop(mobId, static_cast<uint8_t>(itemOrder), *outGrade, *outRate))
        {
            // Fallback a drops originales
            *outGrade = info->dropInfo[itemOrder].grade;
            *outRate = info->dropInfo[itemOrder].rate;
        }
    }
}

// Usar la misma dirección que DynamicDropManager (probada y funcionando)
unsigned u0x48D2E1_Return = 0x48D2ED;

void __declspec(naked) naked_boss_party_drop_hook()
{
    __asm
    {
        // Protegemos ECX
        push ecx
        
        // Reservamos 8 bytes locales para grade/rate
        sub esp, 8
        
        // Parámetros para nuestra función (orden inverso - CDECL)
        lea eax, [esp+4]      // outRate
        push eax
        lea eax, [esp+4]      // outGrade
        push eax
        push ebx              // itemOrder
        push esi              // MobInfo*
        
        call boss_party_hook::GetBossPartyValidatedDrop
        
        // Limpiar parámetros
        add esp, 16
        
        // Recuperar resultados
        movzx eax, word ptr[esp]  // Nuevo Grade
        mov edx, [esp+4]          // Nuevo Rate
        
        // Limpiar stack local
        add esp, 8
        
        // Restaurar ECX
        pop ecx
        
        // Continuar con flujo normal
        jmp u0x48D2E1_Return
    }
}

void hook::boss_party_validator()
{
    // Inicializar nuestro sistema separado
    BossPartyValidator::Init();
    
    // Hook en la misma dirección que DynamicDrop (0x48D2E1)
    // NOTA: Esto puede conflictuar con DynamicDropManager
    // Solución: Modificar DynamicDropManager para incluir validación
    util::detour((void*)0x48D2E1, naked_boss_party_drop_hook, 6);
}
