#include "include/shaiya/DynamicNaked.h"
#include "include/shaiya/CUser.h"
#include "include/shaiya/NetworkHelper.h"
#include <shaiya/include/network/game/outgoing/0100.h>
#include <shaiya/include/common/ItemSlot.h>

namespace shaiya {
    /**
     * @brief Valida si el personaje puede equipar un objeto dado su mapa actual.
     * En el mapa 60 solo se permite equipar Armas (Slot 5) y Escudos (Slot 6).
     * Si la validación falla, se envía una notificación al jugador (ID de mensaje 2025).
     */
    bool DynamicNaked::CanEquip(CUser* user, int itemSlot) {
        // Mapa 60 es la zona designada para el evento Naked
        if (user->mapId != 60) return true;

        // Reglas estrictas: Solo se pueden cambiar/equipar armas y escudos
        if (itemSlot == ItemSlot::Weapon || itemSlot == ItemSlot::Shield) {
            return true;
        }

        // --- Notificación al Jugador (Ref: BossParty 2025) ---
        // Se construye el paquete opcode 0x122 (System Message)
        GameSystemMessageOutgoing msg{};
        msg.opcode = 0x122;
        msg.isNotification = false; // Mensaje de chat/pantalla estándar
        msg.messageNumber = 2025;   // ID del mensaje en el String.sdata (Cliente)
        
        // Se envía el mensaje directamente al jugador (user)
        NetworkHelper::Send(user, &msg, sizeof(msg));
        // -----------------------------------------------------

        // Bloqueo de re-equipado para cualquier otra categoría
        return false;
    }
}
