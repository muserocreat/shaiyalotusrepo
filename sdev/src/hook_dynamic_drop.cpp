#include <util/util.h>
#include "include/main.h"
#include "include/shaiya/CMob.h"
#include "include/shaiya/CWorld.h"
#include "include/shaiya/CUser.h"
#include "include/shaiya/MobInfo.h"
#include "include/shaiya/DynamicDrop.h"
#include "include/shaiya/BossPartyValidator.h"

using namespace shaiya;

namespace dynamic_drop_hook
{
    extern "C" uint32_t __cdecl OnMobHealthWrite(CMob* mob, uint32_t newHealth);
}

// Hook: write de HP del mob (CheatEngine)
// 004A1A62 - 89 85 A8 02 00 00  - mov [ebp+000002A8],eax
static unsigned u0x004A1A62_Return = 0x004A1A68;
void __declspec(naked) naked_0x004A1A62_MobHealthWrite()
{
    __asm
    {
        // Bridge: (CMob* mob = EBP, uint32_t newHealth = EAX) -> retorna health a escribir
        pushad
        push eax
        push ebp
        call dynamic_drop_hook::OnMobHealthWrite
        add esp, 8
        mov [esp + 0x1C], eax // sobrescribe EAX guardado por pushad
        popad

        // original
        mov [ebp+0x2A8], eax
        jmp u0x004A1A62_Return
    }
}

namespace dynamic_drop_hook
{
    extern "C" uint32_t __cdecl OnMobHealthWrite(CMob* mob, uint32_t newHealth)
    {
        if (!mob)
            return newHealth;

        uint32_t mobId = mob->mobId;
        if (mobId == 0)
            return newHealth;

        // Obtener atacante desde mob->target.objectId
        CUser* attacker = nullptr;
        if (mob->target.objectId) {
            attacker = CWorld::FindUser(mob->target.objectId);
        }

        // Si no hay atacante, permitir daño (fallback seguro)
        if (!attacker)
            return newHealth;

        // Solo bloquear daño (cuando HP baja), no curaciones
        uint32_t oldHealth = mob->health;
        if (newHealth >= oldHealth)
            return newHealth;

        // Validar party size - bloquear si no cumple requisitos
        bool allow = BossPartyValidator::ValidateDrop(mobId, attacker);
        if (!allow) {
            return oldHealth;
        }

        return newHealth;
    }

    // Función C++ Bridge invocada en ASM
    // esi = MobInfo*, ebx = itemOrder (0-9)
    // Extrae los valores por referencia si ese mob fue cargado en memoria.
    extern "C" void GetDynamicDrop(MobInfo* info, int itemOrder, uint16_t* outGrade, uint32_t* outRate)
    {
        if (!info) {
            *outGrade = 0;
            *outRate = 0;
            return;
        }

        uint32_t realMobId = static_cast<uint32_t>(info->mobId);
        
        // Dynamic Drop lookup
        if (!DynamicDropManager::GetDrop(realMobId, static_cast<uint8_t>(itemOrder), *outGrade, *outRate))
        {
            // Fallback: usar drops originales del servidor
            *outGrade = info->dropInfo[itemOrder].grade;
            *outRate = info->dropInfo[itemOrder].rate;
        }
    }
}

// Direcciones de retorno para el ASM
unsigned u0x48D2E1_Return = 0x48D2ED;

// Versión Cero-Corrupcion (Segura)
void __declspec(naked) naked_0x48D2E1_Safe()
{
    __asm
    {
        // Protegemos ECX si el servidor lo necesitara
        push ecx
        
        // Reservamos 8 bytes locales {grade de 4B, rate de 4B} para que C++ los llene
        sub esp, 8
        
        // Empujamos en orden inverso (CDECL)
        lea eax, [esp+4] // param4 : outRate (Address of Rate)
        push eax
        lea eax, [esp+4] // param3 : outGrade (Address of Grade) // Cuidado: [esp+4] porque push eax movio el stack 4 bytes. (Misterio de PTR Stack)
        push eax         
        push ebx         // param2 : itemOrder 
        push esi         // param1 : MobInfo*  (¡LA CLAVE! Aqui está el MobID real)
        
        call dynamic_drop_hook::GetDynamicDrop
        
        // Limpiamos los 16 bytes empujados como params a C++
        add esp, 16
        
        // Recuperamos los resultados de nuestros 8 bytes
        movzx eax, word ptr[esp] // AX = Nuevo Grade
        mov edx, [esp+4]         // EDX = Nuevo Rate (Escala 10,000)
        // Limpiamos nuestro array auxiliar de 8 bytes
        add esp, 8
        
        // Liberamos al rehen ECX
        pop ecx
        
        // Volvemos a la instrucción justa donde el server mete Grade al EBP y usa Rate para el Random
        jmp u0x48D2E1_Return
    }
}

void hook::dynamic_drop()
{
    // CMob::GenerateLoot - Desviamos exactamente al momento de pescar y evaluar Drop
    // EP6 Offset: 0x48D2E1 - Incluye validación BossParty integrada
    util::detour((void*)0x48D2E1, naked_0x48D2E1_Safe, 6);

    // Hook alternativo: bloquear daño escribiendo HP (party validation)
    util::detour((void*)0x004A1A62, naked_0x004A1A62_MobHealthWrite, 6);
    
    BossPartyValidator::Log("[HOOK INIT] dynamic_drop inicializado");
}
