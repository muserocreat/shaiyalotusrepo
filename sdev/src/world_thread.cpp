#include <chrono>
#include <iostream>
#include <util/util.h>
#include <shaiya/include/network/game/outgoing/1F00.h>
#include "include/main.h"
#include "include/shaiya/CUser.h"
#include "include/shaiya/CWorld.h"
#include "include/shaiya/ItemInfo.h"
#include "include/shaiya/NetworkHelper.h"
#include "include/shaiya/RewardItem.h"
#include "include/shaiya/DynamicDrop.h"
#include "include/shaiya/DynamicMall.h"
#include "include/shaiya/DynamicPoint.h"
#include "include/shaiya/BossPartyValidator.h"
#include "include/shaiya/SDatabasePool.h"
#include "include/shaiya/SDatabase.h"
using namespace shaiya;

namespace world_thread
{
    inline std::chrono::system_clock::time_point next_update_reward_item_event;
    void update_reward_item_event()
    {
        using namespace std::chrono_literals;

        auto now = std::chrono::system_clock::now();
        if (now < next_update_reward_item_event)
            return;

        next_update_reward_item_event = now + 3000ms;

        for (auto&& [billingId, progress] : g_rewardItemProgress)
        {
            if (progress.completed)
                continue;

            auto now = std::chrono::system_clock::now();
            if (now < progress.timeout)
                continue;

            progress.completed = true;

            auto user = CWorld::FindUserBill(billingId);
            if (!user)
                continue;

            GameRewardItemGetOutgoing outgoing{};
            NetworkHelper::Send(user, &outgoing, sizeof(GameRewardItemGetOutgoing));
        }

        // Ejecutar trigger nativo ODBC de drops
        DynamicDropManager::CheckReloadTrigger();
        
        // Ejecutar trigger nativo ODBC de Tienda
        DynamicMallManager::CheckReloadTrigger();

        // Ejecutar trigger nativo ODBC de Puntos
        DynamicPointManager::CheckReloadTrigger();

        // Ejecutar trigger nativo ODBC de Boss Party Rules
        BossPartyValidator::CheckReloadTrigger();
    }
}

// CWorldThread::UpdateKillCount
unsigned u0x4042A0 = 0x4042A0;
unsigned u0x404076 = 0x404076;
void __declspec(naked) naked_0x404071()
{
    __asm
    {
        // original
        call u0x4042A0

        pushad

        call world_thread::update_reward_item_event

        popad
     
        jmp u0x404076
    }
}

void hook::world_thread()
{
    // CWorldThread::Update
    util::detour((void*)0x404071, naked_0x404071, 5);
}
