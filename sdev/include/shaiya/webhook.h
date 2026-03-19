#pragma once

namespace shaiya
{
    struct CUser;

    namespace webhook
    {
        void send_kill_feed(CUser* killer, CUser* victim);
        void handle_kill(CUser* killer, unsigned victimId, int exp, bool isQuest);
        void init();
    }
}
