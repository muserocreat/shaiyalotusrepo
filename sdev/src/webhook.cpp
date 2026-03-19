#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <winhttp.h>
#include <string>
#include <thread>
#include <sstream>
#include <util/util.h>
#include "include/main.h"
#include "include/shaiya/CUser.h"
#include "include/shaiya/CWorld.h"
#include "include/shaiya/webhook.h"

#pragma comment(lib, "winhttp.lib")

using namespace shaiya;

namespace shaiya
{
    namespace webhook
    {
        void send_kill_feed(CUser* killer, CUser* victim)
        {
            if (!killer || !victim)
                return;

            std::string killerName = killer->charName.data();
            std::string victimName = victim->charName.data();
            int killerLevel = killer->level;
            int killerKills = killer->kills;
            int killerFamily = (int)killer->family;

            // Incrementamos +1 para que el aviso de Discord muestre la kill que acaba de suceder,
            // ya que el servidor suele disparar este evento un milisegundo antes de sumar la kill al contador permanente.
            int updatedTotalKills = killerKills + 1;

            std::thread([killerName, victimName, killerLevel, updatedTotalKills, killerFamily]() {
                HINTERNET hSession = WinHttpOpen(L"LotusServer/1.0", WINHTTP_ACCESS_TYPE_DEFAULT_PROXY, WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
                if (!hSession) return;

                HINTERNET hConnect = WinHttpConnect(hSession, L"discord.com", INTERNET_DEFAULT_HTTPS_PORT, 0);
                if (!hConnect) {
                    WinHttpCloseHandle(hSession);
                    return;
                }

                std::wstring path = L"/api/webhooks/1480641422704115854/K4p-X-OnMZ9ZGeUppGLr-xX6uM7FxGOhAw4xv9FLSjHH6sl9cwWkVvBmCHacQ8aI0ein";

                HINTERNET hRequest = WinHttpOpenRequest(hConnect, L"POST", path.c_str(), NULL, WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES, WINHTTP_FLAG_SECURE);
                if (!hRequest) {
                    WinHttpCloseHandle(hConnect);
                    WinHttpCloseHandle(hSession);
                    return;
                }

                int color = (killerFamily < 2) ? 3447003 : 15158332; // Azul AoL o Rojo UoF
                std::string faction = (killerFamily < 2) ? "Alliance of Light" : "Union of Fury";

                std::ostringstream oss;
                oss << "{"
                    << "\"username\": \"Lotus Kill Feed\","
                    << "\"embeds\": [{"
                    << "\"title\": \"⚔️ ¡Combate Finalizado!\","
                    << "\"description\": \"**" << killerName << "** ha derrotado a **" << victimName << "**.\","
                    << "\"color\": " << color << ","
                    << "\"fields\": ["
                    << "{\"name\": \"Nivel\", \"value\": \"" << killerLevel << "\", \"inline\": true},"
                    << "{\"name\": \"Kills Totales\", \"value\": \"" << updatedTotalKills << "\", \"inline\": true},"
                    << "{\"name\": \"Facción\", \"value\": \"" << faction << "\", \"inline\": false}"
                    << "],"
                    << "\"footer\": {\"text\": \"Shaiya Lotus - Server Side Kill Feed\"}"
                    << "}]"
                    << "}";

                std::string jsonStr = oss.str();
                WinHttpSendRequest(hRequest, L"Content-Type: application/json\r\n", -1L, (LPVOID)jsonStr.c_str(), (DWORD)jsonStr.size(), (DWORD)jsonStr.size(), 0);
                WinHttpReceiveResponse(hRequest, NULL);

                WinHttpCloseHandle(hRequest);
                WinHttpCloseHandle(hConnect);
                WinHttpCloseHandle(hSession);
            }).detach();
        }

        void handle_kill(CUser* killer, unsigned victimId, int exp, bool isQuest)
        {
            if (isQuest || exp <= 0 || !killer)
                return;

            // Intentamos buscar por el ID que viene en el argumento
            CUser* victim = CWorld::FindUser(victimId);
            
            // Si el ID es inválido o apunta al propio asesino (error de stack o party exp circular),
            // intentamos recuperarlo del target del asesino.
            if (!victim || victim->id == killer->id)
            {
                victim = CWorld::FindUser(killer->target.objectId);
            }

            // Finalmente, si no es el mismo y existe, enviamos a Discord.
            if (victim && victim->id != killer->id)
            {
                send_kill_feed(killer, victim);
            }
        }

        void init()
        {
            // Sin hooks adicionales por ahora. Se utiliza el de AddExpFromUser en user_skill.cpp
        }
    }
}

void hook::webhook()
{
    webhook::init();
}
