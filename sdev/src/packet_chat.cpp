#include <util/util.h>
#include "include/main.h"
#include "include/shaiya/CUser.h"
#include "include/shaiya/CParty.h"
#include "include/shaiya/CWorld.h"
#include "include/shaiya/CZone.h"
#include "include/shaiya/CMob.h"
#include "include/shaiya/MobInfo.h"
#include <shaiya/include/network/game/incoming/1100.h>
#include <shaiya/include/network/game/outgoing/1100.h>
#include <string>
#include <cstring>

using namespace shaiya;

namespace packet_chat
{
    /**
     * @brief Manejador de Chat mediante Hook de Tabla de Dispatcher (Registro EBX).
     * Solución de precisión para Shaiya Lotus (Core EP6.4 Modificado).
     */
    bool handle_chat(CUser* user, shaiya::GameChatNormalIncoming* packet)
    {
        // 1. BLINDAJE DE MEMORIA
        if ((uintptr_t)user < 0x100000 || (uintptr_t)packet < 0x100000)
            return false;

        // Solo procesar si el usuario está físicamente en el mapa
        if (user->where != UserWhere::Default)
            return false;

        // 2. VALIDACIÓN DE MENSAJE (En Lotus, el mensaje está en EBX + 0x2 o similar)
        // Nuestra estructura GameChatNormalIncoming ya mapea esto correctamente.
        if (packet->messageLength == 0 || packet->messageLength > 128)
            return false;

        std::string input(packet->message.data());

        // --- SISTEMA DE AVISOS DE RAID (!!) ---
        if (input.starts_with("!!") && user->party)
        {
            // Solo el líder de la party/raid puede avisar
            if (!CParty::IsPartyBoss(user->party, user))
                return false;

            std::string content = input.substr(2);
            std::string announcement;

            if (content == "target")
            {
                // Offsets de Target estables para Lotus EP6.4
                int32_t targetType = *(int32_t*)((char*)user + 0x332);
                uint32_t targetId = *(uint32_t*)((char*)user + 0x336);

                if (targetType == 1) // Jugador
                {
                    CUser* targetUser = CWorld::FindUser(targetId);
                    if (targetUser)
                        announcement = "[RN] >>> TARGET: " + std::string(targetUser->charName.data()) + " <<<";
                }
                else if (targetType == 2 && user->zone) // Monstruo
                {
                    CMob* targetMob = CZone::FindMob(user->zone, targetId);
                    if (targetMob && targetMob->info)
                        announcement = "[RN] >>> TARGET: " + std::string(targetMob->info->mobName.data()) + " <<<";
                }
            }
            else if (!content.empty())
            {
                // Mensaje libre a la raid
                announcement = "[RN] " + content;
            }

            if (!announcement.empty())
            {
                GameChatUnionOutgoing outgoing{};
                outgoing.senderId = user->id;
                
                size_t len = announcement.length();
                if (len > 127) len = 127;

                outgoing.messageLength = static_cast<uint8_t>(len + 1);
                std::memcpy(outgoing.message.data(), announcement.c_str(), len);
                outgoing.message[len] = '\0';

                // Usamos el Broadcast de CParty (Opcode 0x1112)
                CParty::Send(user->party, &outgoing, outgoing.length());
            }

            return true; // Bloquear ejecución original
        }

        return false;
    }
}

// Dirección de la función original en Shaiya Lotus (Encontrada por escaneo)
unsigned u0x47F5F2 = 0x47F5F2;

// NAKED: Proxy de Tabla con lectura de registro EBX
void __declspec(naked) naked_chat_proxy()
{
    __asm
    {
        pushad
        // En tu servidor Lotus: 
        // EBX = Dirección del Paquete
        // ECX = Dirección del Usuario (this)
        push ebx
        push ecx
        call packet_chat::handle_chat
        add esp, 8

        test al, al
        jne block_original

        popad
        // Saltar al manejador de chat original de Lotus
        jmp u0x47F5F2

    block_original:
        popad
        // Tu servidor usa un 'ret' simple para estas funciones (sin pop 4)
        ret
    }
}

namespace hook
{
    void packet_chat()
    {
        // Hook de Tabla de Dispatcher de Grupo 0x11 (Ajuste para Lotus)
        // VA Tabla: 0x47FC80 -> Indice 1 (Chat Normal): 0x47FC84
        unsigned* chat_ptr = (unsigned*)0x47FC84;
        util::write_memory(chat_ptr, &naked_chat_proxy, 4);
    }
}
