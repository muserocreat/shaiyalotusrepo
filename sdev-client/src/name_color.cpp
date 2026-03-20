#include <map>
#include <cmath>
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <util/util.h>
#include "include/main.h"
#include "include/shaiya/CCharacter.h"
#include "include/shaiya/CPlayerData.h"
#include "include/shaiya/CDataFile.h"
#include "include/shaiya/HexColor.h"
#include "include/shaiya/ItemInfo.h"
#include "include/shaiya/Static.h"
using namespace shaiya;

namespace name_color
{
    // Sistema Base: Colores por Range de Casco
    const std::map<uint16_t, HexColor> g_itemRangeToColor {
        { 1, HexColor::LightBlue }, { 2, HexColor::Blue }, { 3, HexColor::Green },
        { 4, HexColor::Yellow }, { 5, HexColor::Orange }, { 6, HexColor::Red },
        { 7, HexColor::Pink }, { 8, HexColor::Purple }, { 9, HexColor::Gray },
        { 10, HexColor::Black }
    };

    // --- EFECTOS DINÁMICOS ESTILO LOTUS ---
    
    // Smooth Rainbow (HSL Cycle)
    D3DCOLOR get_rainbow_color(float speed = 15.0f) {
        float hue = fmodf(GetTickCount() / speed, 360.0f);
        float sat = 1.0f, lit = 0.5f;
        float c = (1.0f - fabsf(2.0f * lit - 1.0f)) * sat;
        float x = c * (1.0f - fabsf(fmodf(hue / 60.0f, 2.0f) - 1.0f));
        float m = lit - c / 2.0f;
        float r = 0, g = 0, b = 0;
        if (hue < 60) { r = c; g = x; b = 0; }
        else if (hue < 120) { r = x; g = c; b = 0; }
        else if (hue < 180) { r = 0; g = c; b = x; }
        else if (hue < 240) { r = 0; g = x; b = c; }
        else if (hue < 300) { r = x; g = 0; b = c; }
        else { r = c; g = 0; b = x; }
        return (0xFE000000) | (static_cast<uint8_t>((r + m) * 255.0f) << 16) | 
                              (static_cast<uint8_t>((g + m) * 255.0f) << 8) | 
                               static_cast<uint8_t>((b + m) * 255.0f);
    }

    // Quad Rainbow (Discrete Cycle)
    D3DCOLOR get_quad_rainbow_color() {
        static const D3DCOLOR colors[4] = { 
            0xFEFF0000, // Rojo
            0xFEFFFF00, // Amarillo
            0xFE00FF00, // Verde
            0xFEFFA500  // Naranja
        };
        return colors[(GetTickCount() / 500) % 4];
    }

    inline D3DCOLOR alpha_safe(HexColor color) {
        return (std::to_underlying(color) & 0x00FFFFFF) | 0xFE000000;
    }

    // Lógica Unificada (Sin Spoofing Global)
    D3DCOLOR get_custom_name_color(CCharacter* user) {
        if (!user) return 0;

        // 1. PRIORIDAD MÁXIMA: Capas con Efectos y Colores
        auto cloakType = user->equipment.type[ItemSlot::Cloak];
        auto cloakTypeId = user->equipment.typeId[ItemSlot::Cloak];
        
        // Verificación explícita: solo procesar si hay un cloak válido
        if (cloakType && cloakType > 0 && cloakTypeId > 0) {
            uint32_t cloakId = (cloakType * 1000) + cloakTypeId;
            
            // Efectos Especiales Lotus
            if (cloakId == 24109) return get_rainbow_color(20.0f); // Staff (Slower)
            if (cloakId == 24110) return get_quad_rainbow_color(); // GM (Quad)
            if (cloakId == 24114) return get_rainbow_color(10.0f); // Creator (Faster)

            // Colores Estáticos de Títulos
            if (cloakId == 24106) return alpha_safe(HexColor::Turquoise);
            if (cloakId == 24107) return alpha_safe(HexColor::DodgerBlue);
            if (cloakId == 24108) return alpha_safe(HexColor::MediumSlateBlue);
        }

        // 2. FALLBACK: Cascos (Por valor de 'range')
        auto hType = user->equipment.type[ItemSlot::Helmet];
        auto hTypeId = user->equipment.typeId[ItemSlot::Helmet];
        
        // Verificación explícita: solo procesar si hay un casco válido
        if (hType && hType > 0 && hTypeId > 0) {
            auto itemInfo = CDataFile::GetItemInfo(hType, hTypeId);
            if (itemInfo && itemInfo->range && itemInfo->range > 0) {
                auto itRange = g_itemRangeToColor.find(itemInfo->range);
                if (itRange != g_itemRangeToColor.end()) return alpha_safe(itRange->second);
            }
        }

        return 0; // Color nativo del juego (blanco característico)
    }
}

namespace name_color 
{
    // RETORNOS
    unsigned u4537DB = 0x4537DB; 
    unsigned u453819 = 0x453819; 
    unsigned u45385B = 0x45385B; 
    unsigned u453889 = 0x453889; 
    unsigned u4538A6 = 0x4538A6; 
    unsigned u4538CD = 0x4538CD; 

    // Funciones hook individuales con sintaxis correcta
    void __declspec(naked) naked_4537D5() {
        __asm {
            pushad
            push esi
            call name_color::get_custom_name_color
            add esp, 4
            test eax, eax
            jz original_4537D5
            mov dword ptr [esp+8], eax
            popad
            sub eax, 0x4 // Instrucción reemplazada
            jmp u4537DB
        original_4537D5:
            popad
            or ebp, -0x1 // Instrucción reemplazada
            sub eax, 0x4 // Instrucción reemplazada
            jmp u4537DB
        }
    }

    void __declspec(naked) naked_453814() {
        __asm {
            pushad
            push esi
            call name_color::get_custom_name_color
            add esp, 4
            test eax, eax
            jz original_453814
            mov dword ptr [esp+8], eax
            popad
            jmp u453819
        original_453814:
            popad
            mov ebp, 0xFF0000FF
            jmp u453819
        }
    }

    void __declspec(naked) naked_453856() {
        __asm {
            pushad
            push esi
            call name_color::get_custom_name_color
            add esp, 4
            test eax, eax
            jz original_453856
            mov dword ptr [esp+8], eax
            popad
            jmp u45385B
        original_453856:
            popad
            mov ebp, 0xFF00FF00
            jmp u45385B
        }
    }

    void __declspec(naked) naked_453884() {
        __asm {
            pushad
            push esi
            call name_color::get_custom_name_color
            add esp, 4
            test eax, eax
            jz original_453884
            mov dword ptr [esp+8], eax
            popad
            jmp u453889
        original_453884:
            popad
            mov ebp, 0xFFFF0000
            jmp u453889
        }
    }

    void __declspec(naked) naked_4538A1() {
        __asm {
            pushad
            push esi
            call name_color::get_custom_name_color
            add esp, 4
            test eax, eax
            jz original_4538A1
            mov dword ptr [esp+8], eax
            popad
            jmp u4538A6
        original_4538A1:
            popad
            mov ebp, 0xFFFF0000
            jmp u4538A6
        }
    }

    void __declspec(naked) naked_4538C8() {
        __asm {
            pushad
            push esi
            call name_color::get_custom_name_color
            add esp, 4
            test eax, eax
            jz original_4538C8
            mov dword ptr [esp+8], eax
            popad
            jmp u4538CD
        original_4538C8:
            popad
            mov ebp, 0xFFFF0000
            jmp u4538CD
        }
    }
}

void hook::name_color() {
    using namespace name_color;
    util::detour((void*)0x4537D5, naked_4537D5, 6); 
    util::detour((void*)0x453814, naked_453814, 5);
    util::detour((void*)0x453856, naked_453856, 5);
    util::detour((void*)0x453884, naked_453884, 5);
    util::detour((void*)0x4538A1, naked_4538A1, 5);
    util::detour((void*)0x4538C8, naked_4538C8, 5);
}
