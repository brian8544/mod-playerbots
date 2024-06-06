/*
 * Copyright (C) 2016+ AzerothCore <www.azerothcore.org>, released under GNU GPL v2 license, you may redistribute it and/or modify it under version 2 of the License, or (at your option), any later version.
 */

#include "EnemyPlayerValue.h"
#include "Playerbots.h"
#include "ServerFacade.h"

bool NearestEnemyPlayersValue::AcceptUnit(Unit* unit)
{
    // Check if the bot is currently in a vehicle
    bool inCannon = botAI->IsInVehicle(false, true);

    // Attempt to cast the unit to a Player type
    Player* enemy = dynamic_cast<Player*>(unit);

    // Verify if the cast was successful and perform additional checks
    if (enemy &&                               // Check if the unit is indeed a Player
        (botAI->IsOpposing(enemy) ||           // Check if the enemy is on the opposing faction
            (bot->InArena() && enemy->InArena() && bot->GetTeamId() != enemy->GetTeamId())) &&  // Additional check for arena and team
        enemy->IsPvP() &&                      // Check if the enemy is flagged for PvP
        !sPlayerbotAIConfig->IsPvpProhibited(enemy->GetZoneId(), enemy->GetAreaId()) &&  // Check if PvP is allowed in the current zone/area
        !enemy->HasFlag(UNIT_FIELD_FLAGS, UNIT_FLAG_NON_ATTACKABLE | UNIT_FLAG_NON_ATTACKABLE_2) &&  // Check if the enemy is attackable
        ((inCannon || !enemy->HasFlag(UNIT_FIELD_FLAGS, UNIT_FLAG_NOT_SELECTABLE))) &&   // Check if the enemy is selectable or if the bot is in a vehicle
        // Additional checks (commented out) for stealth and invisibility are omitted for simplicity
        enemy->CanSeeOrDetect(bot) &&           // Check if the enemy can see or detect the bot
        !(enemy->HasAuraType(SPELL_AURA_SPIRIT_OF_REDEMPTION)))  // Check if the enemy does not have the Spirit of Redemption aura
    {
        return true;  // All conditions are met, accept the unit as an enemy
    }

    return false;  // Conditions are not met, do not accept the unit as an enemy
}

Unit* EnemyPlayerValue::Calculate()
{
    bool inCannon = botAI->IsInVehicle(false, true);

    // 1. Check units we are currently in combat with.
    std::vector<Unit*> targets;
    Unit* pVictim = bot->GetVictim();
    HostileReference* pReference = bot->getHostileRefMgr().getFirst();
    while (pReference)
    {
        ThreatMgr* threatMgr = pReference->GetSource();
        if (Unit* pTarget = threatMgr->GetOwner())
        {
            if (pTarget != pVictim && pTarget->IsPlayer() && pTarget->CanSeeOrDetect(bot) && bot->IsWithinDist(pTarget, VISIBILITY_DISTANCE_NORMAL))
            {
                if (bot->GetTeamId() == TEAM_HORDE)
                {
                    if (pTarget->HasAura(23333))
                        return pTarget;
                }
                else
                {
                    if (pTarget->HasAura(23335))
                        return pTarget;
                }

                targets.push_back(pTarget);
            }
        }

        pReference = pReference->next();
    }

    if (!targets.empty())
    {
        std::sort(targets.begin(), targets.end(), [&](Unit const* pUnit1, Unit const* pUnit2)
        {
            return bot->GetDistance(pUnit1) < bot->GetDistance(pUnit2);
        });

        return *targets.begin();
    }

    // 2. Find enemy player in range.

    GuidVector players = AI_VALUE(GuidVector, "nearest enemy players");
    float const maxAggroDistance = GetMaxAttackDistance();
    for (const auto& gTarget : players)
    {
        Unit* pUnit = botAI->GetUnit(gTarget);
        if (!pUnit)
            continue;

        Player* pTarget = dynamic_cast<Player*>(pUnit);
        if (!pTarget)
            continue;

        if (pTarget == pVictim)
            continue;

        if (bot->GetTeamId() == TEAM_HORDE)
        {
            if (pTarget->HasAura(23333))
                return pTarget;
        }
        else
        {
            if (pTarget->HasAura(23335))
                return pTarget;
        }

        // Aggro weak enemies from further away.
        uint32 const aggroDistance = (inCannon || bot->GetHealth() > pTarget->GetHealth()) ? maxAggroDistance : 20.0f;
        if (!bot->IsWithinDist(pTarget, aggroDistance))
            continue;

        if (bot->IsWithinLOSInMap(pTarget) && (inCannon || (fabs(bot->GetPositionZ() - pTarget->GetPositionZ()) < 30.0f)))
            return pTarget;
    }

    // 3. Check party attackers.

    if (Group* pGroup = bot->GetGroup())
    {
        for (GroupReference* itr = pGroup->GetFirstMember(); itr != nullptr; itr = itr->next())
        {
            if (Unit* pMember = itr->GetSource())
            {
                if (pMember == bot)
                    continue;

                if (sServerFacade->GetDistance2d(bot, pMember) > 30.0f)
                    continue;

                if (Unit* pAttacker = pMember->getAttackerForHelper())
                    if (pAttacker->IsPlayer() && bot->IsWithinDist(pAttacker, maxAggroDistance * 2.0f) && bot->IsWithinLOSInMap(pAttacker) && pAttacker != pVictim
                        && pAttacker->CanSeeOrDetect(bot))
                        return pAttacker;
            }
        }
    }

    return nullptr;
}

float EnemyPlayerValue::GetMaxAttackDistance()
{
    if (!bot->GetBattleground())
        return 60.0f;

    Battleground* bg = bot->GetBattleground();
    if (!bg)
        return 40.0f;

    BattlegroundTypeId bgType = bg->GetBgTypeID();
    if (bgType == BATTLEGROUND_RB)
        bgType = bg->GetBgTypeID(true);

    if (bgType == BATTLEGROUND_IC)
    {
        if (botAI->IsInVehicle(false, true))
            return 120.0f;
    }

    return 40.0f;
}
