#include <windows.h>
#include <string>
#include <thread>
#include <chrono>
#include <util/util.h>
#include "include/main.h"
#include "include/shaiya/Static.h"
using namespace shaiya;

namespace window_title
{
    const char* window_title_format = "Shaiya Lotus";
    const char* window_class_name = "Shaiya";
    const int update_interval_seconds = 3;

    // Obtener el nombre del personaje desde memoria
    char* get_character_name()
    {
        return reinterpret_cast<char*>(0x09144CE);
    }

    // Obtener kills totales del personaje
    uint32_t get_total_kills()
    {
        if (!g_pPlayerData)
            return 0;
        return g_pPlayerData->kills;
    }

    // Actualizar el título de la ventana
    void updateWindowTitle()
    {
        while (true)
        {
            char* charName = get_character_name();
            
            if (g_var && g_var->hwnd)
            {
                if (strlen(charName) > 1)
                {
                    uint32_t kills = get_total_kills();
                    char titleBuffer[256];
                    sprintf_s(titleBuffer, sizeof(titleBuffer), 
                             "%s - Playing as %s | %d Kills | www.shaiyalotus.com", 
                             window_title_format, charName, kills);
                    SetWindowTextA(g_var->hwnd, titleBuffer);
                }
                else
                {
                    std::string title = std::string(window_title_format) + " - No character selected | www.shaiyalotus.com";
                    SetWindowTextA(g_var->hwnd, title.c_str());
                }
            }
            
            std::this_thread::sleep_for(std::chrono::seconds(update_interval_seconds));
        }
    }

    // Iniciar el thread de actualización del título
    void start_title_updater()
    {
        std::thread titleThread(updateWindowTitle);
        titleThread.detach();
    }
}

// Función de inicialización llamada desde main
void hook::window_title()
{
    window_title::start_title_updater();
}
