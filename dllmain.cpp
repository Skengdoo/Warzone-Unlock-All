#include "stdafx.h"
#include "types.h"

#define _PRINT_DEBUG

BOOL g_running = TRUE;

std::once_flag g_flag;

using DWGetLogonStatus_t = int (*)(int);

using MoveResponseToInventory_t = bool(__fastcall*)(LPVOID, int);

extern void Log_(const char* fmt, ...);
#define LOG(fmt, ...) Log_(xorstr_(fmt), ##__VA_ARGS__)

#define LOG_ADDR(var_name)                                      \
        LOG(#var_name ": 0x%llX (0x%llX)", var_name, var_name > base ? var_name - base : 0);    

#define INRANGE(x,a,b)  (x >= a && x <= b) 
#define getBits( x )    (INRANGE((x&(~0x20)),'A','F') ? ((x&(~0x20)) - 'A' + 0xa) : (INRANGE(x,'0','9') ? x - '0' : 0))
#define getByte( x )    (getBits(x[0]) << 4 | getBits(x[1]))

void Log_(const char* fmt, ...) {
    char        text[4096];
    va_list     ap;
    va_start(ap, fmt);
    vsprintf_s(text, fmt, ap);
    va_end(ap);

    std::ofstream logfile(xorstr_("log.txt"), std::ios::app);
    if (logfile.is_open() && text)  logfile << text << std::endl;
    logfile.close();
}

__int64 find_pattern(__int64 range_start, __int64 range_end, const char* pattern) {
    const char* pat = pattern;
    __int64 firstMatch = NULL;
    __int64 pCur = range_start;
    __int64 region_end;
    MEMORY_BASIC_INFORMATION mbi{};
    while (sizeof(mbi) == VirtualQuery((LPCVOID)pCur, &mbi, sizeof(mbi))) {
        if (pCur >= range_end - strlen(pattern))
            break;
        if (!(mbi.Protect & (PAGE_EXECUTE_READ | PAGE_EXECUTE_READWRITE | PAGE_READWRITE))) {
            pCur += mbi.RegionSize;
            continue;
        }
        region_end = pCur + mbi.RegionSize;
        while (pCur < region_end)
        {
            if (!*pat)
                return firstMatch;
            if (*(PBYTE)pat == '\?' || *(BYTE*)pCur == getByte(pat)) {
                if (!firstMatch)
                    firstMatch = pCur;
                if (!pat[1] || !pat[2])
                    return firstMatch;

                if (*(PWORD)pat == '\?\?' || *(PBYTE)pat != '\?')
                    pat += 3;
                else
                    pat += 2;
            }
            else {
                if (firstMatch)
                    pCur = firstMatch;
                pat = pattern;
                firstMatch = 0;
            }
            pCur++;
        }
    }
    return NULL;
}

namespace game {

    __int64 base;
    __int64 lootBase;
    __int64 fpGetLogonStatus;
    __int64 fpMoveResponseToInventory;
    __int64 fpFindStringtable;
    __int64 fpStringtableGetColumnValueForRow;

    bool init() {

        base = (__int64)GetModuleHandle(NULL);
        return true;
    }

    bool find_sigs() {

        MODULEINFO moduleInfo;
        if (!GetModuleInformation((HANDLE)-1, GetModuleHandle(NULL), &moduleInfo, sizeof(MODULEINFO)) || !moduleInfo.lpBaseOfDll) {
            LOG("Couldnt GetModuleInformation");
            return NULL;
        }
        LOG("Base: 0x%llx", moduleInfo.lpBaseOfDll);
        LOG("Size: 0x%llx", moduleInfo.SizeOfImage);

        __int64 searchStart = (__int64)moduleInfo.lpBaseOfDll;
        __int64 searchEnd = (__int64)moduleInfo.lpBaseOfDll + moduleInfo.SizeOfImage;

        bool result = true;

        auto resolve_jmp = [](__int64 addr) -> __int64 {
            return *(int*)(addr + 1) + addr + 5;
        };

        auto resolve_lea = [](__int64 addr) -> __int64 {
            return *(int*)(addr + 3) + addr + 7;
        };

        LOG_ADDR(fpGetLogonStatus =
            find_pattern(searchStart, searchEnd, xorstr_("40 53 48 83 EC 20 48 63 C1 BA")));

        LOG_ADDR(fpFindStringtable = resolve_jmp(
            find_pattern(searchStart, searchEnd, xorstr_("E8 ? ? ? ? 48 8D 15 ? ? ? ? 8D 4B 36"))));

        LOG_ADDR(fpStringtableGetColumnValueForRow = resolve_jmp(
            find_pattern(searchStart, searchEnd, xorstr_("E8 ? ? ? ? 33 D2 48 8B C8 44 8D 42 16"))));

        LOG_ADDR(fpMoveResponseToInventory =
            find_pattern(searchStart, searchEnd, xorstr_("40 53 55 56 57 41 55 41 56 48 83 EC 28 4C")));


        LOG_ADDR(lootBase = resolve_lea(fpMoveResponseToInventory + 17));

        return result;
    }

    static void FindStringTable(const char* name, StringTable** table) {

        reinterpret_cast<void(__cdecl*)(const char*, StringTable**)>(fpFindStringtable)(name, table);
    }

    static char* StringTable_GetColumnValueForRow(void* stringTable, int row, int column) {

        return reinterpret_cast<char* (__cdecl*)(void*, int, int)>(fpStringtableGetColumnValueForRow)(stringTable, row, column);
    }
}

MoveResponseToInventory_t fpMoveResponseOrig = NULL;



//weapon list
LPCSTR cwWeapons[]{
    "weapon", // DLC Weapons and blueprints
    "feature", // Unlock loadout option for acc lvl 1
    "attachment", // attachment for CW Weapons
    // Camos CW
    "iw8_sn_t9standard_camos",
    "iw8_sn_t9quickscope_camos",
    "iw8_sn_t9precisionsemi_camos",
    "iw8_sn_t9powersemi_camos",
    "iw8_sn_t9damagesemi_camos",
    "iw8_sn_t9crossbow_camos",
    "iw8_sn_t9cannon_camos",
    "iw8_sn_t9accurate_camos",
    "iw8_sm_t9standard_camos",
    "iw8_sm_t9spray_camos",
    "iw8_sm_t9powerburst_camos",
    "iw8_sm_t9nailgun_camos",
    "iw8_sm_t9heavy_camos",
    "iw8_sm_t9handling_camos",
    "iw8_sm_t9fastfire_camos",
    "iw8_sm_t9cqb_camos",
    "iw8_sm_t9capacity_camos",
    "iw8_sm_t9burst_camos",
    "iw8_sm_t9accurate_camos",
    "iw8_sh_t9semiauto_camos",
    "iw8_sh_t9pump_camos",
    "iw8_sh_t9fullauto_camos",
    "iw8_pi_t9semiauto_camos",
    "iw8_pi_t9revolver_camos",
    "iw8_pi_t9fullauto_camos",
    "iw8_pi_t9burst_camos",
    "iw8_lm_t9slowfire_camos",
    //New weapons
    "iw8_sm_t9semiauto_camos",
    "iw8_ar_t9british_camos",
    //------
    "iw8_lm_t9light_camos",
    "iw8_lm_t9fastfire_camos",
    "iw8_lm_t9accurate_camos",
    "iw8_me_t9wakizashi_camos",
    "iw8_me_t9sledgehammer_camos",
    "iw8_me_t9machete_camos",
    "iw8_me_t9loadout_camos",
    "iw8_me_t9etool_camos",
    "iw8_me_t9bat_camos",
    "iw8_me_t9ballisticknife_camos",
    "iw8_la_t9standard_camos",
    "iw8_la_t9launcher_camos",
    "iw8_la_t9freefire_camos",
    "iw8_ar_t9standard_camos",
    "iw8_ar_t9slowhandling_camos",
    "iw8_ar_t9slowfire_camos",
    "iw8_ar_t9mobility_camos",
    "iw8_ar_t9longburst_camos",
    "iw8_ar_t9fasthandling_camos",
    "iw8_ar_t9fastfire_camos",
    "iw8_ar_t9fastburst_camos",
    "iw8_ar_t9damage_camos",
    "iw8_ar_t9accurate_camos"
};


/*
* 
        "iw8_pi_golf21_attachment",
        "iw8_pi_papa320_attachment",
        "iw8_pi_decho_attachment",
        "iw8_pi_mike1911_attachment",
        "iw8_pi_cpapa_attachment",
        "iw8_sm_mpapa5_attachment",
        "iw8_sm_beta_attachment",
        "iw8_sm_augolf_attachment",
        "iw8_sm_papa90_attachment",
        "iw8_sm_mpapa7_attachment",
        "iw8_sm_uzulu_attachment",
        "iw8_me_riotshield_attachment",
        "iw8_knife_attachment",
        "iw8_ar_mike4_attachment",
        "iw8_ar_akilo47_attachment",
        "iw8_ar_asierra12_attachment",
        "iw8_ar_falpha_attachment",
        "iw8_ar_mcharlie_attachment",
        "iw8_ar_kilo433_attachment",
        "iw8_ar_falima_attachment",
        "iw8_ar_scharlie_attachment",
        "iw8_la_rpapa7_attachment",
        "iw8_la_gromeo_attachment",
        "iw8_la_juliet_attachment",
        "iw8_la_kgolf_attachment",
        "iw8_sn_mike14_attachment",
        "iw8_sn_kilo98_attachment",
        "iw8_sn_sbeta_attachment",
        "iw8_sn_alpha50_attachment",
        "iw8_sn_delta_attachment",
        "iw8_sn_hdromeo_attachment",
        "iw8_sn_crossbow_attachment",
        "iw8_sh_dpapa12_attachment",
        "iw8_sh_oscar12_attachment",
        "iw8_sh_charlie725_attachment",
        "iw8_sh_romeo870_attachment",
        "iw8_lm_kilo121_attachment",
        "iw8_lm_pkilo_attachment",
        "iw8_lm_lima86_attachment",
        "iw8_lm_mgolf34_attachment",
        "iw8_ar_tango21_attachment",
        "iw8_lm_mgolf36_attachment",
        "iw8_sm_smgolf45_attachment",
        "iw8_sh_mike26_attachment",
        "iw8_ar_sierra552_attachment",
        "iw8_sn_sksierra_attachment",
        "iw8_pi_mike9_attachment",
        "iw8_lm_mkilo3_attachment",
        "iw8_ar_galima_attachment",
        "iw8_sm_victor_attachment",
        "iw8_me_akimboblades_attachment",
        "iw8_me_akimboblunt_attachment",
        "iw8_ar_anovember94_attachment",
        "iw8_sm_charlie9_attachment",
        "iw8_sn_xmike109_attachment",
        "iw8_lm_sierrax_attachment",
        "iw8_sn_romeo700_attachment",
        "iw8_ar_valpha_attachment",
        "iw8_sh_aalpha12_attachment",
        "iw8_pi_mike_attachment",
        "iw8_sm_secho_attachment",
        "iw8_lm_slima_attachment",
        "iw8_pi_golf21_camos",
        "iw8_pi_papa320_camos",
        "iw8_pi_decho_camos",
        "iw8_pi_mike1911_camos",
        "iw8_pi_cpapa_camos",
        "iw8_sm_mpapa5_camos",
        "iw8_sm_beta_camos",
        "iw8_sm_augolf_camos",
        "iw8_sm_papa90_camos",
        "iw8_sm_mpapa7_camos",
        "iw8_sm_uzulu_camos",
        "iw8_me_riotshield_camos",
        "iw8_knife_camos",
        "iw8_ar_mike4_camos",
        "iw8_ar_akilo47_camos",
        "iw8_ar_asierra12_camos",
        "iw8_ar_falpha_camos",
        "iw8_ar_mcharlie_camos",
        "iw8_ar_kilo433_camos",
        "iw8_ar_falima_camos",
        "iw8_ar_scharlie_camos",
        "iw8_la_rpapa7_camos",
        "iw8_la_gromeo_camos",
        "iw8_la_juliet_camos",
        "iw8_la_kgolf_camos",
        "iw8_sn_mike14_camos",
        "iw8_sn_kilo98_camos",
        "iw8_sn_sbeta_camos",
        "iw8_sn_alpha50_camos",
        "iw8_sn_delta_camos",
        "iw8_sn_hdromeo_camos",
        "iw8_sn_crossbow_camos",
        "iw8_sh_dpapa12_camos",
        "iw8_sh_oscar12_camos",
        "iw8_sh_charlie725_camos",
        "iw8_sh_romeo870_camos",
        "iw8_lm_kilo121_camos",
        "iw8_lm_pkilo_camos",
        "iw8_lm_lima86_camos",
        "iw8_lm_mgolf34_camos",
        "iw8_ar_tango21_camos",
        "iw8_lm_mgolf36_camos",
        "iw8_sm_smgolf45_camos",
        "iw8_sh_mike26_camos",
        "iw8_ar_sierra552_camos",
        "iw8_sn_sksierra_camos",
        "iw8_pi_mike9_camos",
        "iw8_lm_mkilo3_camos",
        "iw8_ar_galima_camos",
        "iw8_sm_victor_camos",
        "iw8_me_akimboblades_camos",
        "iw8_me_akimboblunt_camos",
        "iw8_ar_anovember94_camos",
        "iw8_sm_charlie9_camos",
        "iw8_sn_xmike109_camos",
        "iw8_lm_sierrax_camos",
        "iw8_sn_romeo700_camos",
        "iw8_ar_valpha_camos",
        "iw8_sh_aalpha12_camos",
        "iw8_ar_t9standard_camos",
        "iw8_pi_mike_camos",
        "iw8_sm_secho_camos",
        "iw8_lm_slima_camos",
        "iw8_ar_t9accurate_camos",
        "gs_save_slot",
        "iw8_sn_t9powersemi_camos"

*/





bool __fastcall MoveResponseToInventory_Hooked(LPVOID a1, int a2) {

    fpMoveResponseOrig(a1, a2);

    auto pLootBase = game::lootBase;// signature 48 8D 0D ? ? ? ? 48 8D 44 24 ? C7 44 (LEA rcx, pLootBase)

    auto pInventory = (LootItem*)((uintptr_t)pLootBase + 64);

    auto pNumItems = (uint32_t*)((uintptr_t)pLootBase + 240064);

    int curCount = *pNumItems;

    auto updateOrAddItem = [&](int itemId, int quantity) {

        bool bFound = false;

        for (int i = 0; i < 30000; i++) {
            if (pInventory[i].m_itemId == itemId && pInventory[i].m_itemQuantity < 1) {
                pInventory[i].m_itemQuantity++;
                bFound = true;
                break;
            }
        }

        if (!bFound) {
            pInventory[curCount].m_itemId = itemId;
            pInventory[curCount].m_itemQuantity = 1;

            curCount++;
            (*pNumItems)++;

            *(BYTE*)((uintptr_t)pLootBase + 240072) = 0;
        }
    };

    char buf[1024];

    for (int i = 0; i < ARRAYSIZE(cwWeapons); i++)
    {
        sprintf_s(buf, "loot/%s_ids.csv", cwWeapons[i]);

        StringTable* string_table = nullptr;

        game::FindStringTable(buf, &string_table);

        for (int s = 0; s < string_table->rowCount; s++) {

            updateOrAddItem(atoi(game::StringTable_GetColumnValueForRow(string_table, s, 0)), 1);
        }

    }
    MH_RemoveHook((LPVOID)game::fpMoveResponseToInventory);

    return false;
}


void on_attach() {

    game::init();

    if (!game::find_sigs())
        return;
}

void on_detach() {

    g_running = FALSE;
}

DWORD WINAPI thread_proc(LPVOID) {

    std::call_once(g_flag, on_attach);

    while (((DWGetLogonStatus_t)game::fpGetLogonStatus)(0) != 2)
    {
        std::this_thread::sleep_for(
            std::chrono::milliseconds(0));
    }

    if (MH_Initialize() != MH_OK)
        return ERROR_API_UNAVAILABLE;

    if (MH_CreateHook((LPVOID)game::fpMoveResponseToInventory, MoveResponseToInventory_Hooked,
        reinterpret_cast<LPVOID*>(&fpMoveResponseOrig)) == MH_OK) {

        MH_EnableHook((LPVOID)game::fpMoveResponseToInventory);
    }

    return ERROR_SUCCESS;
}

BOOL APIENTRY DllMain(HMODULE hModule,
    DWORD  ul_reason_for_call,
    LPVOID lpReserved) {

    switch (ul_reason_for_call) {
    case DLL_PROCESS_ATTACH: {

        I_beginthreadex(0, 0, (_beginthreadex_proc_type)thread_proc, 0, 0, 0);
    }
                           break;
    case DLL_PROCESS_DETACH:
        on_detach();
        break;
    }
    return TRUE;
}