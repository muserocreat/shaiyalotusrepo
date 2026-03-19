#pragma warning(disable: 4996)
#include <cstring>
#include <string>
#include <util/util.h>
#include <shaiya/include/network/game/incoming/1100.h>
#include "include/main.h"
#include "include/shaiya/CCharacter.h"
#include "include/shaiya/CNetwork.h"
#include "include/shaiya/CPlayerData.h"
#include "include/shaiya/CWorldMgr.h"
#include "include/shaiya/Static.h"

using namespace shaiya;

namespace target
{
    unsigned lastTargetId = 0;
    DWORD lastTargetTime = 0;

    /**
     * @brief Función hookeada que se ejecuta cada vez que el jugador selecciona un objetivo.
     * Si el jugador es Boss de la Unión, anuncia automáticamente el objetivo al grupo.
     */
    void hook_set_target(unsigned targetId)
    {
        // 1. Evitar redundancia (si es el mismo objetivo que ya teníamos)
        if (targetId == lastTargetId)
            return;

        lastTargetId = targetId;

        // 2. VALIDACIÓN DE LIDERAZGO
        // Verificamos si somos un usuario de Unión y si nuestro ID coincide con el ID del Boss (líder)
        if (g_pPlayerData->isUnionUser && g_pPlayerData->unionInfo.bossId == g_pPlayerData->charId)
        {
            // --- FILTRO: MAPAS DE PvP EN SHAIYA LOTUS ---
            // Solo queremos avisos en mapas de combate
            uint16_t currentMap = g_pPlayerData->mapId;
            switch (currentMap)
            {
                case 0:  // Proelium
                case 11: // Cantabilian
                case 45: // Deep Desert 1
                case 46: // Deep Desert 2
                case 47: // Stable Erde
                case 58: // Kanos Illu
                case 60: // Valdemar Regnum (Naked Event?)
                case 67: // Mapas de guerra custom
                    break;
                default:
                    return; // Ignorar si no estamos en zona de guerra
            }

            // --- FILTRO: OBJETIVOS VÁLIDOS ---
            // Buscamos si el objetivo es un jugador en nuestro mundo (memoria local)
            CCharacter* target = CWorldMgr::FindUser(targetId);
            
            // Solo anunciamos si el objetivo es un jugador real (no monstruos/NPCs)
            if (!target)
                return;

            // No nos anunciamos a nosotros mismos ni a aliados de nuestra facción
            if (target->country == g_pPlayerData->country)
                return; 

            // Debounce (Anti-spam): Máximo un aviso cada 500ms
            DWORD now = GetTickCount();
            if (now - lastTargetTime < 500)
                return;

            lastTargetTime = now;

            // 3. CONSTRUCCIÓN Y ENVÍO DEL MENSAJE
            std::string targetName(target->charName.data());
            std::string msg = ">>> ATTACK: " + targetName + " <<<";

            // Enviamos un paquete de chat de Unión (0x1112) que el servidor retransmitirá
            GameChatUnionIncoming packet{};
            packet.opcode = 0x1112;
            packet.messageLength = static_cast<uint8_t>(msg.length() + 1);
            std::strncpy(packet.message.data(), msg.c_str(), sizeof(packet.message));

            // CNetwork::Send envía el paquete directamente al servidor ps_game
            CNetwork::Send(&packet, 3 + packet.messageLength);
        }
    }
}

/**
 * HOOK PARA EL CLIENTE (game.exe)
 * Basado en los hallazgos de Lotus: Dirección 0x44CF14
 * Instrucción original: mov [0x7ABBDC], edx (6 bytes)
 */
unsigned u0x44CF1A = 0x44CF1A; // Dirección de retorno
void __declspec(naked) naked_0x44CF14()
{
    __asm
    {
        // Ejecutamos la instrucción original que sobreescribimos
        mov ds:[0x7ABBDC], edx

        pushad
        push edx // Pasamos el el CharID que está en EDX
        call target::hook_set_target
        add esp, 4
        popad

        jmp u0x44CF1A // Volvemos al flujo normal del cliente
    }
}

namespace hook
{
    void target()
    {
        // Instalamos el detour de 6 bytes en el cliente
        util::detour((void*)0x44CF14, naked_0x44CF14, 6);
    }
}
