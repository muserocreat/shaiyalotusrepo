#include "CGameData.h"
#include "ItemInfo.h"
#include "MobInfo.h"
#include "ProductInfo.h"
#include "SkillInfo.h"
using namespace shaiya;

unsigned CGameData::GetExp(int grow, int level)
{
    unsigned u0x464FF0 = 0x464FF0;
    unsigned result = 0;

    __asm
    {
        mov ecx,level
        mov eax,grow
        call u0x464FF0
        mov result, eax
    }
    return result;
}

ItemInfo* CGameData::GetItemInfo(int type_/*eax*/, int typeId/*ecx*/)
{
    unsigned u0x4059B0 = 0x4059B0;
    ItemInfo* result = nullptr;

    __asm
    {
        mov ecx,typeId
        mov eax,type_
        call u0x4059B0
        mov result, eax
    }
    return result;
}

MobInfo* CGameData::GetMobInfo(int mobId/*eax*/)
{
    unsigned u0x408C00 = 0x408C00;
    MobInfo* result = nullptr;

    __asm
    {
        mov ecx,g_GameData
        mov eax,mobId
        call u0x408C00
        mov result, eax
    }
    return result;
}

ProductInfo* CGameData::GetProductInfo(const char* productCode/*eax*/)
{
    unsigned u0x47A5F0 = 0x47A5F0;
    ProductInfo* result = nullptr;

    __asm
    {
        mov eax,[productCode]
        call u0x47A5F0
        mov result, eax
    }
    return result;
}

SkillInfo* CGameData::GetSkillInfo(int skillId/*eax*/, int skillLv/*edx*/)
{
    unsigned u0x41BB30 = 0x41BB30;
    SkillInfo* result = nullptr;

    __asm
    {
        mov edx,skillLv
        mov eax,skillId
        call u0x41BB30
        mov result, eax
    }
    return result;
}
