#include <util/util.h>
#include <iostream>
#include <fstream>
#include <chrono>
#include <ctime>
#include <iomanip>
#include <cstdio>
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

static std::string GetHookLogPath()
{
    HMODULE hMod = NULL;
    GetModuleHandleExA(
        GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
        reinterpret_cast<LPCSTR>(&GetHookLogPath),
        &hMod);

    char buffer[MAX_PATH];
    GetModuleFileNameA(hMod, buffer, MAX_PATH);
    std::string modpath = buffer;
    std::string modFolder = modpath.substr(0, modpath.find_last_of("\\/"));

    std::string dataFolder = modFolder + "\\Data";
    CreateDirectoryA(dataFolder.c_str(), NULL);

    return dataFolder + "\\HookDynamicDrop.log";
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

static std::string GetHookLogPathFallback()
{
    char buffer[MAX_PATH];
    GetModuleFileNameA(NULL, buffer, MAX_PATH);
    std::string exepath = buffer;
    std::string exeFolder = exepath.substr(0, exepath.find_last_of("\\/"));
    return exeFolder + "\\HookDynamicDrop.log";
}

static void LogHookToFile(const std::string& message)
{
    std::string path = GetHookLogPath();
    FILE* file = nullptr;
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

    write(GetHookLogPathFallback());
}

namespace dynamic_drop_hook
{
    static volatile long g_nakedHitCount = 0;

    extern "C" void __cdecl OnNakedHookHit()
    {
        // Loguear solo las primeras veces para evitar spam
        long n = InterlockedIncrement(&g_nakedHitCount);
        if (n <= 20) {
            LogHookToFile("[HOOK HIT] naked_0x48D2E1_Safe ejecutado. count=" + std::to_string(n));
        }
    }

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
        
        // Validación BossParty
        CUser* killer = nullptr; // TODO: Obtener killer si es posible
        if (!BossPartyValidator::ValidateDrop(realMobId, killer)) {
            *outGrade = 0;
            *outRate = 0;
            return;
        }
        
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
        // Confirmación de ejecución del hook (log a archivo)
        pushad
        call dynamic_drop_hook::OnNakedHookHit
        popad

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
    BossPartyValidator::Init();
    
    // CMob::GenerateLoot - Desviamos exactamente al momento de pescar y evaluar Drop
    // EP6 Offset: 0x48D2E1 - Incluye validación BossParty integrada
    util::detour((void*)0x48D2E1, naked_0x48D2E1_Safe, 6);

    // Hook alternativo: bloquear daño escribiendo HP (party validation)
    util::detour((void*)0x004A1A62, naked_0x004A1A62_MobHealthWrite, 6);
    
    LogHookToFile("[HOOK INIT] dynamic_drop + BossPartyValidator inicializados");
}
