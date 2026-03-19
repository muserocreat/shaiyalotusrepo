#include <util/util.h>
#include "include/main.h"
#include "include/shaiya/CUser.h"
#include "include/shaiya/DynamicDrop.h"
#include <string>

using namespace shaiya;

namespace packet_chat
{
    void handle_chat(CUser* user, const char* chat)
    {
        // Verificar si el usuario es un Administrador (AuthStatus)
        if (user->authStatus <= AuthStatus::AdminE && chat[0] == '/')
        {
            std::string cmd(chat);
            if (cmd == "/reload drops")
            {
                // Ejecutar la recarga dinámica
                DynamicDropManager::Load();
                
                // Enviar mensaje de confirmación al Administrador
                // (Aquí se usaría la función de envío de paquetes de sistema del core)
                // user->SendSystemMessage("Drops Dinámicos recargados desde SQL.");
            }
        }
    }
}

// Offset para el hook de chat en ps_game EP6
unsigned u0x47A1F5 = 0x47A1F5; // Punto de retorno (después del detour)

void __declspec(naked) naked_0x47A1F0()
{
    __asm
    {
        // Original code at 0x47A1F0
        push ebp
        mov ebp,esp
        and esp,-0x8

        // Verificar que ecx (CUser*) no sea NULL antes de llamar a handle_chat
        test ecx,ecx
        je skip_handler

        pushad
        // El mensaje de chat es el segundo argumento de ChatHandler(this, packet)
        mov eax, [ebp+0xC] // Packet
        add eax, 2         // Saltar Opcode (0x0201)
        
        push eax // chat message
        push ecx // CUser* (this)
        call packet_chat::handle_chat
        add esp, 8
        
        popad

        skip_handler:
        jmp u0x47A1F5
    }
}

namespace hook
{
    void packet_chat()
    {
        // Hookeamos al puro inicio de la funcion para no romper instrucciones a la mitad
        // util::detour((void*)0x47A1F0, naked_0x47A1F0, 5);
    }
}
