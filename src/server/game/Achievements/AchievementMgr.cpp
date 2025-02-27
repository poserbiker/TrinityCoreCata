/*
 * This file is part of the TrinityCore Project. See AUTHORS file for Copyright information
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program. If not, see <http://www.gnu.org/licenses/>.
 */

#include "AchievementMgr.h"
#include "AchievementPackets.h"
#include "ArenaTeamMgr.h"
#include "Battleground.h"
#include "CellImpl.h"
#include "ChatTextBuilder.h"
#include "Containers.h"
#include "DatabaseEnv.h"
#include "DBCEnums.h"
#include "DisableMgr.h"
#include "GameEventMgr.h"
#include "GameTime.h"
#include "GridNotifiersImpl.h"
#include "Group.h"
#include "Guild.h"
#include "GuildMgr.h"
#include "InstanceScript.h"
#include "Item.h"
#include "Language.h"
#include "Log.h"
#include "Mail.h"
#include "Map.h"
#include "MapManager.h"
#include "ObjectMgr.h"
#include "Player.h"
#include "ReputationMgr.h"
#include "ScriptMgr.h"
#include "SpellMgr.h"
#include "World.h"
#include "WorldSession.h"
#include "WorldStateMgr.h"

bool AchievementCriteriaData::IsValid(AchievementCriteriaEntry const* criteria)
{
    if (dataType >= MAX_ACHIEVEMENT_CRITERIA_DATA_TYPE)
    {
        TC_LOG_ERROR("sql.sql", "Table `achievement_criteria_data` for criteria (Entry: %u) contains a wrong data type (%u), ignored.", criteria->ID, dataType);
        return false;
    }

    switch (criteria->Type)
    {
        case ACHIEVEMENT_CRITERIA_TYPE_KILL_CREATURE:
        case ACHIEVEMENT_CRITERIA_TYPE_KILL_CREATURE_TYPE:
        case ACHIEVEMENT_CRITERIA_TYPE_WIN_BG:
        case ACHIEVEMENT_CRITERIA_TYPE_DEATH_IN_DUNGEON:
        case ACHIEVEMENT_CRITERIA_TYPE_FALL_WITHOUT_DYING:
        case ACHIEVEMENT_CRITERIA_TYPE_COMPLETE_QUEST:          // only hardcoded list
        case ACHIEVEMENT_CRITERIA_TYPE_CAST_SPELL:
        case ACHIEVEMENT_CRITERIA_TYPE_WIN_RATED_ARENA:
        case ACHIEVEMENT_CRITERIA_TYPE_DO_EMOTE:
        case ACHIEVEMENT_CRITERIA_TYPE_SPECIAL_PVP_KILL:
        case ACHIEVEMENT_CRITERIA_TYPE_WIN_DUEL:
        case ACHIEVEMENT_CRITERIA_TYPE_LOOT_TYPE:
        case ACHIEVEMENT_CRITERIA_TYPE_LAND_TARGETED_SPELL_ON_SPELL_TARGET:
        case ACHIEVEMENT_CRITERIA_TYPE_BE_SPELL_TARGET:
        case ACHIEVEMENT_CRITERIA_TYPE_BE_SPELL_TARGET2:
        case ACHIEVEMENT_CRITERIA_TYPE_EQUIP_EPIC_ITEM:
        case ACHIEVEMENT_CRITERIA_TYPE_ROLL_NEED_ON_LOOT:
        case ACHIEVEMENT_CRITERIA_TYPE_ROLL_GREED_ON_LOOT:
        case ACHIEVEMENT_CRITERIA_TYPE_BG_OBJECTIVE_CAPTURE:
        case ACHIEVEMENT_CRITERIA_TYPE_HONORABLE_KILL:
        case ACHIEVEMENT_CRITERIA_TYPE_COMPLETE_DAILY_QUEST:    // only Children's Week achievements
        case ACHIEVEMENT_CRITERIA_TYPE_USE_ITEM:                // only Children's Week achievements
        case ACHIEVEMENT_CRITERIA_TYPE_GET_KILLING_BLOWS:
        case ACHIEVEMENT_CRITERIA_TYPE_REACH_LEVEL:
        case ACHIEVEMENT_CRITERIA_TYPE_ON_LOGIN:
            break;
        default:
            if (dataType != ACHIEVEMENT_CRITERIA_DATA_TYPE_SCRIPT)
            {
                TC_LOG_ERROR("sql.sql", "Table `achievement_criteria_data` contains data for a non-supported criteria type (Entry: %u Type: %u), ignored.", criteria->ID, criteria->Type);
                return false;
            }
            break;
    }

    switch (dataType)
    {
        case ACHIEVEMENT_CRITERIA_DATA_TYPE_NONE:
        case ACHIEVEMENT_CRITERIA_DATA_TYPE_INSTANCE_SCRIPT:
            return true;
        case ACHIEVEMENT_CRITERIA_DATA_TYPE_T_CREATURE:
            if (!creature.id || !sObjectMgr->GetCreatureTemplate(creature.id))
            {
                TC_LOG_ERROR("sql.sql", "Table `achievement_criteria_data` (Entry: %u Type: %u) for data type ACHIEVEMENT_CRITERIA_DATA_TYPE_CREATURE (%u) contains a non-existing creature id in value1 (%u), ignored.",
                    criteria->ID, criteria->Type, dataType, creature.id);
                return false;
            }
            return true;
        case ACHIEVEMENT_CRITERIA_DATA_TYPE_T_PLAYER_CLASS_RACE:
            if (!classRace.class_id && !classRace.race_id)
            {
                TC_LOG_ERROR("sql.sql", "Table `achievement_criteria_data` (Entry: %u Type: %u) for data type ACHIEVEMENT_CRITERIA_DATA_TYPE_T_PLAYER_CLASS_RACE (%u) must not have 0 in either value field, ignored.",
                    criteria->ID, criteria->Type, dataType);
                return false;
            }
            if (classRace.class_id && ((1 << (classRace.class_id-1)) & CLASSMASK_ALL_PLAYABLE) == 0)
            {
                TC_LOG_ERROR("sql.sql", "Table `achievement_criteria_data` (Entry: %u Type: %u) for data type ACHIEVEMENT_CRITERIA_DATA_TYPE_T_PLAYER_CLASS_RACE (%u) contains a non-existing class in value1 (%u), ignored.",
                    criteria->ID, criteria->Type, dataType, classRace.class_id);
                return false;
            }
            if (classRace.race_id && ((1 << (classRace.race_id-1)) & RACEMASK_ALL_PLAYABLE) == 0)
            {
                TC_LOG_ERROR("sql.sql", "Table `achievement_criteria_data` (Entry: %u Type: %u) for data type ACHIEVEMENT_CRITERIA_DATA_TYPE_T_PLAYER_CLASS_RACE (%u) contains a non-existing race in value2 (%u), ignored.",
                    criteria->ID, criteria->Type, dataType, classRace.race_id);
                return false;
            }
            return true;
        case ACHIEVEMENT_CRITERIA_DATA_TYPE_T_PLAYER_LESS_HEALTH:
            if (health.percent < 1 || health.percent > 100)
            {
                TC_LOG_ERROR("sql.sql", "Table `achievement_criteria_data` (Entry: %u Type: %u) for data type ACHIEVEMENT_CRITERIA_DATA_TYPE_PLAYER_LESS_HEALTH (%u) contains a wrong percent value in value1 (%u), ignored.",
                    criteria->ID, criteria->Type, dataType, health.percent);
                return false;
            }
            return true;
        case ACHIEVEMENT_CRITERIA_DATA_TYPE_S_AURA:
        case ACHIEVEMENT_CRITERIA_DATA_TYPE_T_AURA:
        {
            SpellInfo const* spellEntry = sSpellMgr->GetSpellInfo(aura.spell_id);
            if (!spellEntry)
            {
                TC_LOG_ERROR("sql.sql", "Table `achievement_criteria_data` (Entry: %u Type: %u) for data type %s (%u) contains a wrong spell id in value1 (%u), ignored.",
                    criteria->ID, criteria->Type, (dataType == ACHIEVEMENT_CRITERIA_DATA_TYPE_S_AURA?"ACHIEVEMENT_CRITERIA_DATA_TYPE_S_AURA":"ACHIEVEMENT_CRITERIA_DATA_TYPE_T_AURA"), dataType, aura.spell_id);
                return false;
            }
            if (aura.effect_idx >= 3)
            {
                TC_LOG_ERROR("sql.sql", "Table `achievement_criteria_data` (Entry: %u Type: %u) for data type %s (%u) contains a wrong spell effect index in value2 (%u), ignored.",
                    criteria->ID, criteria->Type, (dataType == ACHIEVEMENT_CRITERIA_DATA_TYPE_S_AURA?"ACHIEVEMENT_CRITERIA_DATA_TYPE_S_AURA":"ACHIEVEMENT_CRITERIA_DATA_TYPE_T_AURA"), dataType, aura.effect_idx);
                return false;
            }
            if (!spellEntry->Effects[aura.effect_idx].ApplyAuraName)
            {
                TC_LOG_ERROR("sql.sql", "Table `achievement_criteria_data` (Entry: %u Type: %u) for data type %s (%u) contains a non-aura spell effect (ID: %u Effect: %u), ignored.",
                    criteria->ID, criteria->Type, (dataType == ACHIEVEMENT_CRITERIA_DATA_TYPE_S_AURA?"ACHIEVEMENT_CRITERIA_DATA_TYPE_S_AURA":"ACHIEVEMENT_CRITERIA_DATA_TYPE_T_AURA"), dataType, aura.spell_id, aura.effect_idx);
                return false;
            }
            return true;
        }
        case ACHIEVEMENT_CRITERIA_DATA_TYPE_VALUE:
            if (value.compType >= COMP_TYPE_MAX)
            {
                TC_LOG_ERROR("sql.sql", "Table `achievement_criteria_data` (Entry: %u Type: %u) for data type ACHIEVEMENT_CRITERIA_DATA_TYPE_VALUE (%u) contains a wrong ComparisionType in value2 (%u), ignored.",
                    criteria->ID, criteria->Type, dataType, value.compType);
                return false;
            }
            return true;
        case ACHIEVEMENT_CRITERIA_DATA_TYPE_T_LEVEL:
            if (level.minlevel > STRONG_MAX_LEVEL)
            {
                TC_LOG_ERROR("sql.sql", "Table `achievement_criteria_data` (Entry: %u Type: %u) for data type ACHIEVEMENT_CRITERIA_DATA_TYPE_T_LEVEL (%u) contains a wrong minlevel in value1 (%u), ignored.",
                    criteria->ID, criteria->Type, dataType, level.minlevel);
                return false;
            }
            return true;
        case ACHIEVEMENT_CRITERIA_DATA_TYPE_T_GENDER:
            if (gender.gender > GENDER_NONE)
            {
                TC_LOG_ERROR("sql.sql", "Table `achievement_criteria_data` (Entry: %u Type: %u) for data type ACHIEVEMENT_CRITERIA_DATA_TYPE_T_GENDER (%u) contains a wrong gender value in value1 (%u), ignored.",
                    criteria->ID, criteria->Type, dataType, gender.gender);
                return false;
            }
            return true;
        case ACHIEVEMENT_CRITERIA_DATA_TYPE_SCRIPT:
            if (!ScriptId)
            {
                TC_LOG_ERROR("sql.sql", "Table `achievement_criteria_data` (Entry: %u Type: %u) for data type ACHIEVEMENT_CRITERIA_DATA_TYPE_SCRIPT (%u) does not have a ScriptName set, ignored.",
                    criteria->ID, criteria->Type, dataType);
                return false;
            }
            return true;
        case ACHIEVEMENT_CRITERIA_DATA_TYPE_MAP_PLAYER_COUNT:
            if (map_players.maxcount <= 0)
            {
                TC_LOG_ERROR("sql.sql", "Table `achievement_criteria_data` (Entry: %u Type: %u) for data type ACHIEVEMENT_CRITERIA_DATA_TYPE_MAP_PLAYER_COUNT (%u) contains a wrong max players count in value1 (%u), ignored.",
                    criteria->ID, criteria->Type, dataType, map_players.maxcount);
                return false;
            }
            return true;
        case ACHIEVEMENT_CRITERIA_DATA_TYPE_T_TEAM:
            if (team.team != ALLIANCE && team.team != HORDE)
            {
                TC_LOG_ERROR("sql.sql", "Table `achievement_criteria_data` (Entry: %u Type: %u) for data type ACHIEVEMENT_CRITERIA_DATA_TYPE_T_TEAM (%u) contains an unknown team value in value1 (%u), ignored.",
                    criteria->ID, criteria->Type, dataType, team.team);
                return false;
            }
            return true;
        case ACHIEVEMENT_CRITERIA_DATA_TYPE_S_DRUNK:
            if (drunk.state >= MAX_DRUNKEN)
            {
                TC_LOG_ERROR("sql.sql", "Table `achievement_criteria_data` (Entry: %u Type: %u) for data type ACHIEVEMENT_CRITERIA_DATA_TYPE_S_DRUNK (%u) contains an unknown drunken state value in value1 (%u), ignored.",
                    criteria->ID, criteria->Type, dataType, drunk.state);
                return false;
            }
            return true;
        case ACHIEVEMENT_CRITERIA_DATA_TYPE_HOLIDAY:
            if (!sHolidaysStore.LookupEntry(holiday.id))
            {
                TC_LOG_ERROR("sql.sql", "Table `achievement_criteria_data` (Entry: %u Type: %u) for data type ACHIEVEMENT_CRITERIA_DATA_TYPE_HOLIDAY (%u) contains an unknown holiday entry in value1 (%u), ignored.",
                    criteria->ID, criteria->Type, dataType, holiday.id);
                return false;
            }
            return true;
        case ACHIEVEMENT_CRITERIA_DATA_TYPE_GAME_EVENT:
        {
            GameEventMgr::GameEventDataMap const& events = sGameEventMgr->GetEventMap();
            if (game_event.id < 1 || game_event.id >= events.size())
            {
                TC_LOG_ERROR("sql.sql", "Table `achievement_criteria_data` (Entry: %u Type: %u) for data type ACHIEVEMENT_CRITERIA_DATA_TYPE_GAME_EVENT (%u) has unknown game_event in value1 (%u), ignored.",
                    criteria->ID, criteria->Type, dataType, game_event.id);
                return false;
            }
            return true;
        }
        case ACHIEVEMENT_CRITERIA_DATA_TYPE_BG_LOSS_TEAM_SCORE:
            return true;                                    // not check correctness node indexes
        case ACHIEVEMENT_CRITERIA_DATA_TYPE_S_EQUIPPED_ITEM:
        case ACHIEVEMENT_CRITERIA_DATA_TYPE_S_ITEM_QUALITY:
            if (equipped_item.item_quality >= MAX_ITEM_QUALITY)
            {
                TC_LOG_ERROR("sql.sql", "Table `achievement_criteria_data` (Entry: %u Type: %u) for data type %s (%u) contains an unknown quality state value in value1 (%u), ignored.",
                    criteria->ID, criteria->Type, (dataType == ACHIEVEMENT_CRITERIA_DATA_TYPE_S_EQUIPPED_ITEM ? "ACHIEVEMENT_CRITERIA_DATA_TYPE_S_EQUIPPED_ITEM" : "ACHIEVEMENT_CRITERIA_DATA_TYPE_S_ITEM_QUALITY"), dataType, equipped_item.item_quality);
                return false;
            }
            return true;
        case ACHIEVEMENT_CRITERIA_DATA_TYPE_S_PLAYER_CLASS_RACE:
            if (!classRace.class_id && !classRace.race_id)
            {
                TC_LOG_ERROR("sql.sql", "Table `achievement_criteria_data` (Entry: %u Type: %u) for data type ACHIEVEMENT_CRITERIA_DATA_TYPE_S_PLAYER_CLASS_RACE (%u) should not have 0 in either value field. Ignored.",
                    criteria->ID, criteria->Type, dataType);
                return false;
            }
            if (classRace.class_id && ((1 << (classRace.class_id-1)) & CLASSMASK_ALL_PLAYABLE) == 0)
            {
                TC_LOG_ERROR("sql.sql", "Table `achievement_criteria_data` (Entry: %u Type: %u) for data type ACHIEVEMENT_CRITERIA_DATA_TYPE_S_PLAYER_CLASS_RACE (%u) contains a non-existing class entry in value1 (%u), ignored.",
                    criteria->ID, criteria->Type, dataType, classRace.class_id);
                return false;
            }
            if (classRace.race_id && ((1 << (classRace.race_id-1)) & RACEMASK_ALL_PLAYABLE) == 0)
            {
                TC_LOG_ERROR("sql.sql", "Table `achievement_criteria_data` (Entry: %u Type: %u) for data type ACHIEVEMENT_CRITERIA_DATA_TYPE_S_PLAYER_CLASS_RACE (%u) contains a non-existing race entry in value2 (%u), ignored.",
                    criteria->ID, criteria->Type, dataType, classRace.race_id);
                return false;
            }
            return true;
        case ACHIEVEMENT_CRITERIA_DATA_TYPE_S_KNOWN_TITLE:
            if (!sCharTitlesStore.LookupEntry(known_title.title_id))
            {
                TC_LOG_ERROR("sql.sql", "Table `achievement_criteria_data` (Entry: %u Type: %u) for data type ACHIEVEMENT_CRITERIA_DATA_TYPE_S_KNOWN_TITLE (%u) contains an unknown title_id in value1 (%u), ignore.",
                    criteria->ID, criteria->Type, dataType, known_title.title_id);
                return false;
            }
            return true;
        default:
            TC_LOG_ERROR("sql.sql", "Table `achievement_criteria_data` (Entry: %u Type: %u) contains data of a non-supported data type (%u), ignored.", criteria->ID, criteria->Type, dataType);
            return false;
    }
}

bool AchievementCriteriaData::Meets(uint32 criteria_id, Player const* source, WorldObject const* target, uint32 miscValue1 /*= 0*/, uint32 miscValue2 /* = 0*/) const
{
    switch (dataType)
    {
        case ACHIEVEMENT_CRITERIA_DATA_TYPE_NONE:
            return true;
        case ACHIEVEMENT_CRITERIA_DATA_TYPE_T_CREATURE:
            if (!target || target->GetTypeId() != TYPEID_UNIT)
                return false;
            return target->GetEntry() == creature.id;
        case ACHIEVEMENT_CRITERIA_DATA_TYPE_T_PLAYER_CLASS_RACE:
            if (!target || target->GetTypeId() != TYPEID_PLAYER)
                return false;
            if (classRace.class_id && classRace.class_id != target->ToPlayer()->getClass())
                return false;
            if (classRace.race_id && classRace.race_id != target->ToPlayer()->getRace())
                return false;
            return true;
        case ACHIEVEMENT_CRITERIA_DATA_TYPE_S_PLAYER_CLASS_RACE:
            if (source->GetTypeId() != TYPEID_PLAYER)
                return false;
            if (classRace.class_id && classRace.class_id != source->ToPlayer()->getClass())
                return false;
            if (classRace.race_id && classRace.race_id != source->ToPlayer()->getRace())
                return false;
            return true;
        case ACHIEVEMENT_CRITERIA_DATA_TYPE_T_PLAYER_LESS_HEALTH:
            if (!target || target->GetTypeId() != TYPEID_PLAYER)
                return false;
            return !target->ToPlayer()->HealthAbovePct(health.percent);
        case ACHIEVEMENT_CRITERIA_DATA_TYPE_S_AURA:
            return source->HasAuraEffect(aura.spell_id, aura.effect_idx);
        case ACHIEVEMENT_CRITERIA_DATA_TYPE_T_AURA:
        {
            if (!target)
                return false;
            Unit const* unitTarget = target->ToUnit();
            if (!unitTarget)
                return false;
            return unitTarget->HasAuraEffect(aura.spell_id, aura.effect_idx);
        }
        case ACHIEVEMENT_CRITERIA_DATA_TYPE_VALUE:
            return CompareValues(ComparisionType(value.compType), miscValue1, value.value);
        case ACHIEVEMENT_CRITERIA_DATA_TYPE_T_LEVEL:
            if (!target || !target->IsUnit())
                return false;
            return target->ToUnit()->getLevel() >= level.minlevel;
        case ACHIEVEMENT_CRITERIA_DATA_TYPE_T_GENDER:
        {
            if (!target)
                return false;
            Unit const* unitTarget = target->ToUnit();
            if (!unitTarget)
                return false;
            return unitTarget->getGender() == gender.gender;
        }
        case ACHIEVEMENT_CRITERIA_DATA_TYPE_SCRIPT:
        {
            Unit const* unitTarget = nullptr;
            if (target)
                unitTarget = target->ToUnit();
            return sScriptMgr->OnCriteriaCheck(ScriptId, const_cast<Player*>(source), const_cast<Unit*>(unitTarget));
        }
        case ACHIEVEMENT_CRITERIA_DATA_TYPE_MAP_PLAYER_COUNT:
            return source->GetMap()->GetPlayersCountExceptGMs() <= map_players.maxcount;
        case ACHIEVEMENT_CRITERIA_DATA_TYPE_T_TEAM:
            if (!target || target->GetTypeId() != TYPEID_PLAYER)
                return false;
            return target->ToPlayer()->GetTeam() == team.team;
        case ACHIEVEMENT_CRITERIA_DATA_TYPE_S_DRUNK:
            return Player::GetDrunkenstateByValue(source->GetDrunkValue()) >= DrunkenState(drunk.state);
        case ACHIEVEMENT_CRITERIA_DATA_TYPE_HOLIDAY:
            return IsHolidayActive(HolidayIds(holiday.id));
        case ACHIEVEMENT_CRITERIA_DATA_TYPE_GAME_EVENT:
            return IsEventActive(game_event.id);
        case ACHIEVEMENT_CRITERIA_DATA_TYPE_BG_LOSS_TEAM_SCORE:
        {
            Battleground* bg = source->GetBattleground();
            if (!bg)
                return false;

            uint32 score = bg->GetTeamScore(source->GetTeamId() == TEAM_ALLIANCE ? TEAM_HORDE : TEAM_ALLIANCE);
            return score >= bg_loss_team_score.min_score && score <= bg_loss_team_score.max_score;
        }
        case ACHIEVEMENT_CRITERIA_DATA_TYPE_INSTANCE_SCRIPT:
        {
            if (!source->IsInWorld())
                return false;
            Map* map = source->GetMap();
            if (!map->IsDungeon())
            {
                TC_LOG_ERROR("achievement", "Achievement system call ACHIEVEMENT_CRITERIA_DATA_TYPE_INSTANCE_SCRIPT (%u) for achievement criteria %u in a non-dungeon/non-raid map %u",
                    dataType, criteria_id, map->GetId());
                return false;
            }
            InstanceScript* instance = map->ToInstanceMap()->GetInstanceScript();
            if (!instance)
            {
                TC_LOG_ERROR("achievement", "Achievement system call ACHIEVEMENT_CRITERIA_DATA_TYPE_INSTANCE_SCRIPT (%u) for achievement criteria %u in map %u, but the map does not have an instance script.",
                    dataType, criteria_id, map->GetId());
                return false;
            }

            Unit const* unitTarget = nullptr;
            if (target)
                unitTarget = target->ToUnit();
            return instance->CheckAchievementCriteriaMeet(criteria_id, source, unitTarget, miscValue1);
        }
        case ACHIEVEMENT_CRITERIA_DATA_TYPE_S_EQUIPPED_ITEM:
        {
            AchievementCriteriaEntry const* entry = ASSERT_NOTNULL(sAchievementMgr->GetAchievementCriteria(criteria_id));

            uint32 itemId = (entry->Type == ACHIEVEMENT_CRITERIA_TYPE_EQUIP_EPIC_ITEM ? miscValue2 : miscValue1);
            ItemTemplate const* itemTemplate = sObjectMgr->GetItemTemplate(itemId);
            if (!itemTemplate)
                return false;

            return itemTemplate->GetBaseItemLevel() >= equipped_item.item_level && itemTemplate->GetQuality() >= equipped_item.item_quality;
        }
        case ACHIEVEMENT_CRITERIA_DATA_TYPE_MAP_ID:
            return source->GetMapId() == map_id.mapId;
        case ACHIEVEMENT_CRITERIA_DATA_TYPE_S_KNOWN_TITLE:
        {
            if (CharTitlesEntry const* titleInfo = sCharTitlesStore.LookupEntry(known_title.title_id))
                return source && source->HasTitle(titleInfo->Mask_ID);

            return false;
        }
        case ACHIEVEMENT_CRITERIA_DATA_TYPE_S_ITEM_QUALITY:
        {
            ItemTemplate const* pProto = sObjectMgr->GetItemTemplate(miscValue1);
            if (!pProto)
                return false;
            return pProto->GetQuality() == item.item_quality;
        }
        default:
            break;
    }
    return false;
}

bool AchievementCriteriaDataSet::Meets(Player const* source, WorldObject const* target, uint32 miscValue1 /*= 0*/, uint32 miscValue2 /*= 0*/) const
{
    for (Storage::const_iterator itr = storage.begin(); itr != storage.end(); ++itr)
        if (!itr->Meets(criteria_id, source, target, miscValue1, miscValue2))
            return false;

    return true;
}

template<class T>
AchievementMgr<T>::AchievementMgr(T* owner): _owner(owner), _achievementPoints(0) { }

template<class T>
AchievementMgr<T>::~AchievementMgr() { }

template<class T>
void AchievementMgr<T>::SendPacket(WorldPacket const* /*data*/) const { }

template<>
void AchievementMgr<Guild>::SendPacket(WorldPacket const* data) const
{
    GetOwner()->BroadcastPacket(data);
}

template<>
void AchievementMgr<Player>::SendPacket(WorldPacket const* data) const
{
    GetOwner()->SendDirectMessage(data);
}

template<class T>
void AchievementMgr<T>::RemoveCriteriaProgress(AchievementCriteriaEntry const* entry)
{
    if (!entry)
        return;

    CriteriaProgressMap::iterator criteriaProgress = m_criteriaProgress.find(entry->ID);
    if (criteriaProgress == m_criteriaProgress.end())
        return;

    WorldPacket data(SMSG_CRITERIA_DELETED, 4);
    data << uint32(entry->ID);
    SendPacket(&data);

    m_criteriaProgress.erase(criteriaProgress);
}

template<>
void AchievementMgr<Guild>::RemoveCriteriaProgress(AchievementCriteriaEntry const* entry)
{
    if (!entry)
        return;

    auto criteriaProgress = m_criteriaProgress.find(entry->ID);
    if (criteriaProgress == m_criteriaProgress.end())
        return;

    ObjectGuid guid = GetOwner()->GetGUID();

    WorldPacket data(SMSG_GUILD_CRITERIA_DELETED, 4 + 8);
    data.WriteBit(guid[6]);
    data.WriteBit(guid[5]);
    data.WriteBit(guid[7]);
    data.WriteBit(guid[0]);
    data.WriteBit(guid[1]);
    data.WriteBit(guid[3]);
    data.WriteBit(guid[2]);
    data.WriteBit(guid[4]);

    data.WriteByteSeq(guid[2]);
    data.WriteByteSeq(guid[3]);
    data.WriteByteSeq(guid[4]);
    data.WriteByteSeq(guid[1]);
    data.WriteByteSeq(guid[7]);
    data << uint32(entry->ID);
    data.WriteByteSeq(guid[5]);
    data.WriteByteSeq(guid[0]);
    data.WriteByteSeq(guid[6]);

    SendPacket(&data);

    criteriaProgress->second.counter = 0;
    criteriaProgress->second.changed = true;
    m_criteriaProgress.erase(criteriaProgress);
}

template<class T>
void AchievementMgr<T>::DeleteFromDB(ObjectGuid /*guid*/)
{
}

template<>
void AchievementMgr<Player>::DeleteFromDB(ObjectGuid guid)
{
    CharacterDatabaseTransaction trans = CharacterDatabase.BeginTransaction();

    CharacterDatabasePreparedStatement* stmt = CharacterDatabase.GetPreparedStatement(CHAR_DEL_CHAR_ACHIEVEMENT);
    stmt->setUInt32(0, guid.GetCounter());
    trans->Append(stmt);

    stmt = CharacterDatabase.GetPreparedStatement(CHAR_DEL_CHAR_ACHIEVEMENT_PROGRESS);
    stmt->setUInt32(0, guid.GetCounter());
    trans->Append(stmt);

    CharacterDatabase.CommitTransaction(trans);
}

template<>
void AchievementMgr<Guild>::DeleteFromDB(ObjectGuid guid)
{
    CharacterDatabaseTransaction trans = CharacterDatabase.BeginTransaction();

    CharacterDatabasePreparedStatement* stmt = CharacterDatabase.GetPreparedStatement(CHAR_DEL_ALL_GUILD_ACHIEVEMENTS);
    stmt->setUInt32(0, guid.GetCounter());
    trans->Append(stmt);

    stmt = CharacterDatabase.GetPreparedStatement(CHAR_DEL_ALL_GUILD_ACHIEVEMENT_CRITERIA);
    stmt->setUInt32(0, guid.GetCounter());
    trans->Append(stmt);

    CharacterDatabase.CommitTransaction(trans);
}

template<class T>
void AchievementMgr<T>::SaveToDB(CharacterDatabaseTransaction& /*trans*/)
{
}

template<>
void AchievementMgr<Player>::SaveToDB(CharacterDatabaseTransaction& trans)
{
    if (!m_completedAchievements.empty())
    {
        for (CompletedAchievementMap::iterator iter = m_completedAchievements.begin(); iter != m_completedAchievements.end(); ++iter)
        {
            if (!iter->second.changed)
                continue;

            CharacterDatabasePreparedStatement* stmt = CharacterDatabase.GetPreparedStatement(CHAR_DEL_CHAR_ACHIEVEMENT_BY_ACHIEVEMENT);
            stmt->setUInt16(0, iter->first);
            stmt->setUInt32(1, GetOwner()->GetGUID().GetCounter());
            trans->Append(stmt);

            stmt = CharacterDatabase.GetPreparedStatement(CHAR_INS_CHAR_ACHIEVEMENT);
            stmt->setUInt32(0, GetOwner()->GetGUID().GetCounter());
            stmt->setUInt16(1, iter->first);
            stmt->setUInt32(2, uint32(iter->second.date));
            trans->Append(stmt);

            iter->second.changed = false;
        }
    }

    if (!m_criteriaProgress.empty())
    {
        for (CriteriaProgressMap::iterator iter = m_criteriaProgress.begin(); iter != m_criteriaProgress.end(); ++iter)
        {
            if (!iter->second.changed)
                continue;

            CharacterDatabasePreparedStatement* stmt = CharacterDatabase.GetPreparedStatement(CHAR_DEL_CHAR_ACHIEVEMENT_PROGRESS_BY_CRITERIA);
            stmt->setUInt32(0, GetOwner()->GetGUID().GetCounter());
            stmt->setUInt16(1, iter->first);
            trans->Append(stmt);

            if (iter->second.counter)
            {
                stmt = CharacterDatabase.GetPreparedStatement(CHAR_INS_CHAR_ACHIEVEMENT_PROGRESS);
                stmt->setUInt32(0, GetOwner()->GetGUID().GetCounter());
                stmt->setUInt16(1, iter->first);
                stmt->setUInt32(2, iter->second.counter);
                stmt->setUInt32(3, uint32(iter->second.date));
                trans->Append(stmt);
            }

            iter->second.changed = false;
        }
    }
}

template<>
void AchievementMgr<Guild>::SaveToDB(CharacterDatabaseTransaction& trans)
{
    CharacterDatabasePreparedStatement* stmt;
    std::ostringstream guidstr;
    for (CompletedAchievementMap::const_iterator itr = m_completedAchievements.begin(); itr != m_completedAchievements.end(); ++itr)
    {
        if (!itr->second.changed)
            continue;

        stmt = CharacterDatabase.GetPreparedStatement(CHAR_DEL_GUILD_ACHIEVEMENT);
        stmt->setUInt32(0, GetOwner()->GetId());
        stmt->setUInt16(1, itr->first);
        trans->Append(stmt);

        stmt = CharacterDatabase.GetPreparedStatement(CHAR_INS_GUILD_ACHIEVEMENT);
        stmt->setUInt32(0, GetOwner()->GetId());
        stmt->setUInt16(1, itr->first);
        stmt->setUInt32(2, itr->second.date);
        for (GuidSet::const_iterator gItr = itr->second.guids.begin(); gItr != itr->second.guids.end(); ++gItr)
            guidstr << gItr->GetCounter() << ',';

        stmt->setString(3, guidstr.str());
        trans->Append(stmt);

        guidstr.str("");
    }

    for (CriteriaProgressMap::const_iterator itr = m_criteriaProgress.begin(); itr != m_criteriaProgress.end(); ++itr)
    {
        if (!itr->second.changed)
            continue;

        stmt = CharacterDatabase.GetPreparedStatement(CHAR_DEL_GUILD_ACHIEVEMENT_CRITERIA);
        stmt->setUInt32(0, GetOwner()->GetId());
        stmt->setUInt16(1, itr->first);
        trans->Append(stmt);

        stmt = CharacterDatabase.GetPreparedStatement(CHAR_INS_GUILD_ACHIEVEMENT_CRITERIA);
        stmt->setUInt32(0, GetOwner()->GetId());
        stmt->setUInt16(1, itr->first);
        stmt->setUInt64(2, itr->second.counter);
        stmt->setUInt32(3, itr->second.date);
        stmt->setUInt32(4, itr->second.CompletedGUID.GetCounter());
        trans->Append(stmt);
    }
}

template<class T>
void AchievementMgr<T>::LoadFromDB(PreparedQueryResult /*achievementResult*/, PreparedQueryResult /*criteriaResult*/)
{
}

template<>
void AchievementMgr<Player>::LoadFromDB(PreparedQueryResult achievementResult, PreparedQueryResult criteriaResult)
{
    if (achievementResult)
    {
        do
        {
            Field* fields = achievementResult->Fetch();
            uint32 achievementid = fields[0].GetUInt16();

            // must not happen: cleanup at server startup in sAchievementMgr->LoadCompletedAchievements()
            AchievementEntry const* achievement = sAchievementMgr->GetAchievement(achievementid);
            if (!achievement)
                continue;

            CompletedAchievementData& ca = m_completedAchievements[achievementid];
            ca.date = time_t(fields[1].GetUInt32());
            ca.changed = false;

            _achievementPoints += achievement->Points;

            // title achievement rewards are retroactive
            if (AchievementReward const* reward = sAchievementMgr->GetAchievementReward(achievement))
                if (uint32 titleId = reward->TitleID[Player::TeamForRace(GetOwner()->getRace()) == ALLIANCE ? 0 : 1])
                    if (CharTitlesEntry const* titleEntry = sCharTitlesStore.LookupEntry(titleId))
                        GetOwner()->SetTitle(titleEntry);

        }
        while (achievementResult->NextRow());
    }

    if (criteriaResult)
    {
        do
        {
            Field* fields  = criteriaResult->Fetch();
            uint32 id      = fields[0].GetUInt16();
            uint64 counter = fields[1].GetUInt64();
            time_t date    = time_t(fields[2].GetUInt32());

            AchievementCriteriaEntry const* criteria = sAchievementMgr->GetAchievementCriteria(id);
            if (!criteria)
            {
                // Removing non-existing criteria data for all characters
                TC_LOG_ERROR("achievement", "Non-existing achievement criteria %u data has been removed from the table `character_achievement_progress`.", id);

                CharacterDatabasePreparedStatement* stmt = CharacterDatabase.GetPreparedStatement(CHAR_DEL_INVALID_ACHIEV_PROGRESS_CRITERIA);
                stmt->setUInt16(0, uint16(id));
                CharacterDatabase.Execute(stmt);

                continue;
            }

            if (criteria->TimerTime && time_t(date + criteria->TimerTime) < GameTime::GetGameTime())
                continue;

            CriteriaProgress& progress = m_criteriaProgress[id];
            progress.counter = counter;
            progress.date    = date;
            progress.changed = false;
        }
        while (criteriaResult->NextRow());
    }
}

template<>
void AchievementMgr<Guild>::LoadFromDB(PreparedQueryResult achievementResult, PreparedQueryResult criteriaResult)
{
    if (achievementResult)
    {
        do
        {
            Field* fields = achievementResult->Fetch();
            uint32 achievementid = fields[0].GetUInt16();

            // must not happen: cleanup at server startup in sAchievementMgr->LoadCompletedAchievements()
            AchievementEntry const* achievement = sAchievementMgr->GetAchievement(achievementid);
            if (!achievement)
                continue;

            CompletedAchievementData& ca = m_completedAchievements[achievementid];
            ca.date = time_t(fields[1].GetUInt32());
            Tokenizer guids(fields[2].GetString(), ' ');
            for (uint32 i = 0; i < guids.size(); ++i)
                ca.guids.insert(ObjectGuid(HighGuid::Player, uint32(atol(guids[i]))));

            ca.changed = false;

            _achievementPoints += achievement->Points;
        }
        while (achievementResult->NextRow());
    }

    if (criteriaResult)
    {
        do
        {
            Field* fields = criteriaResult->Fetch();
            uint32 id      = fields[0].GetUInt16();
            uint32 counter = fields[1].GetUInt32();
            time_t date    = time_t(fields[2].GetUInt32());
            uint32 guid    = fields[3].GetUInt32();

            AchievementCriteriaEntry const* criteria = sAchievementMgr->GetAchievementCriteria(id);
            if (!criteria)
            {
                // we will remove not existed criteria for all guilds
                TC_LOG_ERROR("achievement", "Non-existing achievement criteria %u data removed from table `guild_achievement_progress`.", id);

                CharacterDatabasePreparedStatement* stmt = CharacterDatabase.GetPreparedStatement(CHAR_DEL_INVALID_ACHIEV_PROGRESS_CRITERIA_GUILD);
                stmt->setUInt16(0, uint16(id));
                CharacterDatabase.Execute(stmt);
                continue;
            }

            if (criteria->TimerTime && time_t(date + criteria->TimerTime) < GameTime::GetGameTime())
                continue;

            CriteriaProgress& progress = m_criteriaProgress[id];
            progress.counter = counter;
            progress.date    = date;
            progress.CompletedGUID = ObjectGuid(HighGuid::Player, guid);
            progress.changed = false;
        } while (criteriaResult->NextRow());
    }
}

template<class T>
void AchievementMgr<T>::Reset()
{
}

template<>
void AchievementMgr<Player>::Reset()
{
    for (CompletedAchievementMap::const_iterator iter = m_completedAchievements.begin(); iter != m_completedAchievements.end(); ++iter)
    {
        WorldPacket data(SMSG_ACHIEVEMENT_DELETED, 4);
        data << uint32(iter->first);
        SendPacket(&data);
    }

    for (CriteriaProgressMap::const_iterator iter = m_criteriaProgress.begin(); iter != m_criteriaProgress.end(); ++iter)
    {
        WorldPacket data(SMSG_CRITERIA_DELETED, 4);
        data << uint32(iter->first);
        SendPacket(&data);
    }

    m_completedAchievements.clear();
    _achievementPoints = 0;
    m_criteriaProgress.clear();
    DeleteFromDB(GetOwner()->GetGUID());

    // re-fill data
    CheckAllAchievementCriteria(GetOwner());
}

template<>
void AchievementMgr<Guild>::Reset()
{
    ObjectGuid guid = GetOwner()->GetGUID();
    for (CompletedAchievementMap::const_iterator iter = m_completedAchievements.begin(); iter != m_completedAchievements.end(); ++iter)
    {
        WorldPacket data(SMSG_GUILD_ACHIEVEMENT_DELETED, 4);
        data.WriteBit(guid[4]);
        data.WriteBit(guid[1]);
        data.WriteBit(guid[2]);
        data.WriteBit(guid[3]);
        data.WriteBit(guid[0]);
        data.WriteBit(guid[7]);
        data.WriteBit(guid[5]);
        data.WriteBit(guid[6]);
        data << uint32(iter->first);
        data.WriteByteSeq(guid[5]);
        data.WriteByteSeq(guid[1]);
        data.WriteByteSeq(guid[3]);
        data.WriteByteSeq(guid[6]);
        data.WriteByteSeq(guid[0]);
        data.WriteByteSeq(guid[7]);
        data.AppendPackedTime(iter->second.date);
        data.WriteByteSeq(guid[4]);
        data.WriteByteSeq(guid[2]);
        SendPacket(&data);
    }

    while (!m_criteriaProgress.empty())
        if (AchievementCriteriaEntry const* criteria = sAchievementMgr->GetAchievementCriteria(m_criteriaProgress.begin()->first))
            RemoveCriteriaProgress(criteria);

    _achievementPoints = 0;
    m_completedAchievements.clear();
    DeleteFromDB(GetOwner()->GetGUID());
}

template<class T>
void AchievementMgr<T>::SendAchievementEarned(AchievementEntry const* achievement) const
{
    // Don't send for achievements with ACHIEVEMENT_FLAG_HIDDEN
    if (achievement->Flags & ACHIEVEMENT_FLAG_HIDDEN)
        return;

    TC_LOG_DEBUG("achievement", "AchievementMgr::SendAchievementEarned(%u)", achievement->ID);

    if (Guild* guild = sGuildMgr->GetGuildById(GetOwner()->GetGuildId()))
    {
        Trinity::BroadcastTextBuilder _builder(GetOwner(), CHAT_MSG_GUILD_ACHIEVEMENT, BROADCAST_TEXT_ACHIEVEMENT_EARNED, GetOwner()->getGender(), _owner, achievement->ID);
        Trinity::LocalizedPacketDo<Trinity::BroadcastTextBuilder> _localizer(_builder);
        guild->BroadcastWorker(_localizer, GetOwner());
    }

    if (achievement->Flags & (ACHIEVEMENT_FLAG_REALM_FIRST_KILL | ACHIEVEMENT_FLAG_REALM_FIRST_REACH | ACHIEVEMENT_FLAG_REALM_FIRST_GUILD))
    {
        // broadcast realm first reached
        WorldPacket data(SMSG_SERVER_FIRST_ACHIEVEMENT, GetOwner()->GetName().size() + 1 + 8 + 4 + 4);
        data << GetOwner()->GetName();
        data << uint64(GetOwner()->GetGUID());
        data << uint32(achievement->ID);
        data << uint32(0);                                  // 1=link supplied string as player name, 0=display plain string
        sWorld->SendGlobalMessage(&data);
    }
    // if player is in world he can tell his friends about new achievement
    else if (GetOwner()->IsInWorld())
    {
        Trinity::BroadcastTextBuilder _builder(GetOwner(), CHAT_MSG_ACHIEVEMENT, BROADCAST_TEXT_ACHIEVEMENT_EARNED, GetOwner()->getGender(), _owner, achievement->ID);
        Trinity::LocalizedPacketDo<Trinity::BroadcastTextBuilder> _localizer(_builder);
        Trinity::PlayerDistWorker<Trinity::LocalizedPacketDo<Trinity::BroadcastTextBuilder>> _worker(GetOwner(), sWorld->getFloatConfig(CONFIG_LISTEN_RANGE_SAY), _localizer);
        Cell::VisitWorldObjects(GetOwner(), _worker, sWorld->getFloatConfig(CONFIG_LISTEN_RANGE_SAY));
    }

    WorldPacket data(SMSG_ACHIEVEMENT_EARNED, 8+4+8);
    data << GetOwner()->GetPackGUID();
    data << uint32(achievement->ID);
    data.AppendPackedTime(GameTime::GetGameTime());
    data << uint32(0);  // does not notify player ingame
    GetOwner()->SendMessageToSetInRange(&data, sWorld->getFloatConfig(CONFIG_LISTEN_RANGE_SAY), true);
}

template<>
void AchievementMgr<Guild>::SendAchievementEarned(AchievementEntry const* achievement) const
{
    ObjectGuid guid = GetOwner()->GetGUID();

    WorldPacket data(SMSG_GUILD_ACHIEVEMENT_EARNED, 8+4+8);
    data.WriteBit(guid[3]);
    data.WriteBit(guid[1]);
    data.WriteBit(guid[0]);
    data.WriteBit(guid[7]);
    data.WriteBit(guid[4]);
    data.WriteBit(guid[6]);
    data.WriteBit(guid[2]);
    data.WriteBit(guid[5]);

    data.WriteByteSeq(guid[2]);
    data.AppendPackedTime(GameTime::GetGameTime());
    data.WriteByteSeq(guid[0]);
    data.WriteByteSeq(guid[4]);
    data.WriteByteSeq(guid[1]);
    data.WriteByteSeq(guid[3]);
    data << uint32(achievement->ID);
    data.WriteByteSeq(guid[7]);
    data.WriteByteSeq(guid[5]);
    data.WriteByteSeq(guid[6]);

    SendPacket(&data);
}

template<class T>
void AchievementMgr<T>::SendCriteriaUpdate(AchievementCriteriaEntry const* /*entry*/, CriteriaProgress const* /*progress*/, uint32 /*timeElapsed*/, bool /*timedCompleted*/) const
{
}

template<>
void AchievementMgr<Player>::SendCriteriaUpdate(AchievementCriteriaEntry const* entry, CriteriaProgress const* progress, uint32 timeElapsed, bool timedCompleted) const
{
    WorldPackets::Achievement::CriteriaUpdate criteriaUpdate;
    criteriaUpdate.CriteriaID = entry->ID;
    criteriaUpdate.Quantity = progress->counter;
    criteriaUpdate.PlayerGUID = GetOwner()->GetGUID();
    criteriaUpdate.Flags = 0;
    if (entry->TimerTime)
        criteriaUpdate.Flags = timedCompleted ? 1 : 0; // 1 is for keeping the counter at 0 in client

    criteriaUpdate.CurrentTime = progress->date;
    criteriaUpdate.ElapsedTime = timeElapsed;
    criteriaUpdate.CreationTime = 0;

    SendPacket(criteriaUpdate.Write());
}

template<>
void AchievementMgr<Guild>::SendCriteriaUpdate(AchievementCriteriaEntry const* entry, CriteriaProgress const* progress, uint32 /*timeElapsed*/, bool /*timedCompleted*/) const
{
    //will send response to criteria progress request
    WorldPacket data(SMSG_GUILD_CRITERIA_DATA, 3 + 1 + 1 + 8 + 8 + 4 + 4 + 4 + 4 + 4);

    ObjectGuid counter(progress->counter); // for accessing every byte individually
    ObjectGuid guid = progress->CompletedGUID;

    data.WriteBits(1, 21);
    data.WriteBit(counter[4]);
    data.WriteBit(counter[1]);
    data.WriteBit(guid[2]);
    data.WriteBit(counter[3]);
    data.WriteBit(guid[1]);
    data.WriteBit(counter[5]);
    data.WriteBit(counter[0]);
    data.WriteBit(guid[3]);
    data.WriteBit(counter[2]);
    data.WriteBit(guid[7]);
    data.WriteBit(guid[5]);
    data.WriteBit(guid[0]);
    data.WriteBit(counter[6]);
    data.WriteBit(guid[6]);
    data.WriteBit(counter[7]);
    data.WriteBit(guid[4]);

    data.FlushBits();

    data.WriteByteSeq(guid[5]);
    data << uint32(progress->date);      // unknown date
    data.WriteByteSeq(counter[3]);
    data.WriteByteSeq(counter[7]);
    data << uint32(progress->date);      // unknown date
    data.WriteByteSeq(counter[6]);
    data.WriteByteSeq(guid[4]);
    data.WriteByteSeq(guid[1]);
    data.WriteByteSeq(counter[4]);
    data.WriteByteSeq(guid[3]);
    data.WriteByteSeq(counter[0]);
    data.WriteByteSeq(guid[2]);
    data.WriteByteSeq(counter[1]);
    data.WriteByteSeq(guid[6]);
    data << uint32(progress->date);      // last update time (not packed!)
    data << uint32(entry->ID);
    data.WriteByteSeq(counter[5]);
    data << uint32(0);
    data.WriteByteSeq(guid[7]);
    data.WriteByteSeq(counter[2]);
    data.WriteByteSeq(guid[0]);

    GetOwner()->BroadcastPacketIfTrackingAchievement(&data, entry->ID);
}

template<class T>
void AchievementMgr<T>::SendAllTrackedCriterias(Player* /*receiver*/, std::set<uint32> const& /*trackedCriterias*/) const
{
}

template<>
void AchievementMgr<Guild>::SendAllTrackedCriterias(Player* receiver, std::set<uint32> const& trackedCriterias) const
{
    return;

    ObjectGuid counter;
    ObjectGuid guid;
    uint32 trackedCriteriasCount = 0;

    for (std::set<uint32>::iterator itr = trackedCriterias.begin(); itr != trackedCriterias.end(); ++itr)
    {
        AchievementCriteriaEntry const* entry = sAchievementMgr->GetAchievementCriteria(*itr);

        CriteriaProgressMap::const_iterator progress = m_criteriaProgress.find(entry->ID);
        if (progress == m_criteriaProgress.end())
            continue;

        ++trackedCriteriasCount;
    }

    ByteBuffer criteriaBits(21 + (trackedCriteriasCount * (8 + 8)));
    ByteBuffer criteriaData(trackedCriteriasCount * (8 + 8 + 4 + 4 + 4 + 4 + 4));

    criteriaBits.WriteBits(trackedCriteriasCount, 21);

    for (std::set<uint32>::iterator itr = trackedCriterias.begin(); itr != trackedCriterias.end(); ++itr)
    {
        AchievementCriteriaEntry const* entry = sAchievementMgr->GetAchievementCriteria(*itr);

        CriteriaProgressMap::const_iterator progress = m_criteriaProgress.find(entry->ID);
        if (progress == m_criteriaProgress.end())
            continue;

        counter.Set(progress->second.counter);
        guid = progress->second.CompletedGUID;

        criteriaBits.WriteBit(counter[4]);
        criteriaBits.WriteBit(counter[1]);
        criteriaBits.WriteBit(guid[2]);
        criteriaBits.WriteBit(counter[3]);
        criteriaBits.WriteBit(guid[1]);
        criteriaBits.WriteBit(counter[5]);
        criteriaBits.WriteBit(counter[0]);
        criteriaBits.WriteBit(guid[3]);
        criteriaBits.WriteBit(counter[2]);
        criteriaBits.WriteBit(guid[7]);
        criteriaBits.WriteBit(guid[5]);
        criteriaBits.WriteBit(guid[0]);
        criteriaBits.WriteBit(counter[6]);
        criteriaBits.WriteBit(guid[6]);
        criteriaBits.WriteBit(counter[7]);
        criteriaBits.WriteBit(guid[4]);

        criteriaData.WriteByteSeq(guid[5]);
        criteriaData << uint32(progress->second.date); // unknown date
        criteriaData.WriteByteSeq(counter[3]);
        criteriaData.WriteByteSeq(counter[7]);
        criteriaData << uint32(progress->second.date); // unknown date
        criteriaData.WriteByteSeq(counter[6]);
        criteriaData.WriteByteSeq(guid[4]);
        criteriaData.WriteByteSeq(guid[1]);
        criteriaData.WriteByteSeq(counter[4]);
        criteriaData.WriteByteSeq(guid[3]);
        criteriaData.WriteByteSeq(counter[0]);
        criteriaData.WriteByteSeq(guid[2]);
        criteriaData.WriteByteSeq(counter[1]);
        criteriaData.WriteByteSeq(guid[6]);
        criteriaData << uint32(progress->second.date); // last update time (not packed!)
        criteriaData << uint32(entry->ID);
        criteriaData.WriteByteSeq(counter[5]);
        criteriaData << uint32(0);
        criteriaData.WriteByteSeq(guid[7]);
        criteriaData.WriteByteSeq(counter[2]);
        criteriaData.WriteByteSeq(guid[0]);
    }

    WorldPacket data(SMSG_GUILD_CRITERIA_DATA, criteriaBits.size() + criteriaData.size());
    data.append(criteriaBits);

    if (trackedCriteriasCount)
        data.append(criteriaData);

    receiver->SendDirectMessage(&data);
}

/**
 * called at player login. The player might have fulfilled some achievements when the achievement system wasn't working yet
 */
template<class T>
void AchievementMgr<T>::CheckAllAchievementCriteria(Player* referencePlayer)
{
    // suppress sending packets
    for (uint32 i = 0; i < ACHIEVEMENT_CRITERIA_TYPE_TOTAL; ++i)
        UpdateAchievementCriteria(AchievementCriteriaTypes(i), 0, 0, 0, nullptr, referencePlayer);
}

static const uint32 achievIdByArenaSlot[MAX_ARENA_SLOT] = {1057, 1107, 1108};
static const uint32 achievIdForDungeon[][4] =
{
    // ach_cr_id, is_dungeon, is_raid, is_heroic_dungeon
    { 321,       true,      true,   true  },
    { 916,       false,     true,   false },
    { 917,       false,     true,   false },
    { 918,       true,      false,  false },
    { 2219,      false,     false,  true  },
    { 0,         false,     false,  false }
};

// Helper function to avoid having to specialize template for a 800 line long function
template <typename T> static bool IsGuild() { return false; }
template<> bool IsGuild<Guild>() { return true; }

/**
 * this function will be called whenever the user might have done a criteria relevant action
 */
template<class T>
void AchievementMgr<T>::UpdateAchievementCriteria(AchievementCriteriaTypes type, uint64 miscValue1 /*= 0*/, uint64 miscValue2 /*= 0*/, uint64 miscValue3 /*= 0*/, WorldObject const* ref /*= nullptr*/, Player* referencePlayer /*= nullptr*/, GameObject* go /*= nullptr*/)
{
    if (type >= ACHIEVEMENT_CRITERIA_TYPE_TOTAL)
    {
        TC_LOG_DEBUG("achievement", "UpdateAchievementCriteria: Wrong criteria type %u", type);
        return;
    }

    if (!referencePlayer)
    {
        TC_LOG_DEBUG("achievement", "UpdateAchievementCriteria: Player is nullptr! Cant update criteria");
        return;
    }

    // disable for gamemasters with GM-mode enabled
    if (referencePlayer->IsGameMaster())
    {
        TC_LOG_DEBUG("achievement", "UpdateAchievementCriteria: [Player %s GM mode on] %s, %s (%u), " UI64FMTD ", " UI64FMTD ", " UI64FMTD
            , referencePlayer->GetName().c_str(), GetOwner()->GetGUID().ToString().c_str(), AchievementGlobalMgr::GetCriteriaTypeString(type), type, miscValue1, miscValue2, miscValue3);
        return;
    }

    TC_LOG_DEBUG("achievement", "UpdateAchievementCriteria: %s, %s (%u), " UI64FMTD ", " UI64FMTD ", " UI64FMTD
        , GetOwner()->GetGUID().ToString().c_str(), AchievementGlobalMgr::GetCriteriaTypeString(type), type, miscValue1, miscValue2, miscValue3);

    // Lua_GetGuildLevelEnabled() is checked in achievement UI to display guild tab
    if (IsGuild<T>() && !sWorld->getBoolConfig(CONFIG_GUILD_LEVELING_ENABLED))
        return;

    AchievementCriteriaEntryList const& achievementCriteriaList = sAchievementMgr->GetAchievementCriteriaByType(type, miscValue1, IsGuild<T>());
    for (AchievementCriteriaEntryList::const_iterator i = achievementCriteriaList.begin(); i != achievementCriteriaList.end(); ++i)
    {
        AchievementCriteriaEntry const* achievementCriteria = (*i);
        AchievementEntry const* achievement = sAchievementMgr->GetAchievement(achievementCriteria->AchievementID);
        if (!achievement)
        {
            TC_LOG_ERROR("achievement", "UpdateAchievementCriteria: Achievement %u not found!", achievementCriteria->AchievementID);
            continue;
        }

        if (!CanUpdateCriteria(achievementCriteria, achievement, miscValue1, miscValue2, miscValue3, ref, referencePlayer, go))
            continue;

        switch (type)
        {
            // special cases, db data is checked later
            case ACHIEVEMENT_CRITERIA_TYPE_WIN_RATED_ARENA:
            case ACHIEVEMENT_CRITERIA_TYPE_ROLL_NEED_ON_LOOT:
            case ACHIEVEMENT_CRITERIA_TYPE_ROLL_GREED_ON_LOOT:
                break;
            default:
                if (AchievementCriteriaDataSet const* data = sAchievementMgr->GetCriteriaDataSet(achievementCriteria))
                    if (!data->Meets(referencePlayer, ref, miscValue1, miscValue2))
                        continue;
                break;
        }

        switch (type)
        {
            // std. case: increment at 1
            case ACHIEVEMENT_CRITERIA_TYPE_NUMBER_OF_TALENT_RESETS:
            case ACHIEVEMENT_CRITERIA_TYPE_LOSE_DUEL:
            case ACHIEVEMENT_CRITERIA_TYPE_CREATE_AUCTION:
            case ACHIEVEMENT_CRITERIA_TYPE_WON_AUCTIONS:    /* FIXME: for online player only currently */
            case ACHIEVEMENT_CRITERIA_TYPE_ROLL_NEED:
            case ACHIEVEMENT_CRITERIA_TYPE_ROLL_GREED:
            case ACHIEVEMENT_CRITERIA_TYPE_QUEST_ABANDONED:
            case ACHIEVEMENT_CRITERIA_TYPE_FLIGHT_PATHS_TAKEN:
            case ACHIEVEMENT_CRITERIA_TYPE_ACCEPTED_SUMMONINGS:
            case ACHIEVEMENT_CRITERIA_TYPE_DEATH:
            case ACHIEVEMENT_CRITERIA_TYPE_WIN_BG:
            case ACHIEVEMENT_CRITERIA_TYPE_COMPLETE_DAILY_QUEST:
            case ACHIEVEMENT_CRITERIA_TYPE_COMPLETE_BATTLEGROUND:
            case ACHIEVEMENT_CRITERIA_TYPE_DEATH_AT_MAP:
            case ACHIEVEMENT_CRITERIA_TYPE_DEATH_IN_DUNGEON:
            case ACHIEVEMENT_CRITERIA_TYPE_KILLED_BY_CREATURE:
            case ACHIEVEMENT_CRITERIA_TYPE_KILLED_BY_PLAYER:
            case ACHIEVEMENT_CRITERIA_TYPE_DEATHS_FROM:
            case ACHIEVEMENT_CRITERIA_TYPE_BE_SPELL_TARGET:
            case ACHIEVEMENT_CRITERIA_TYPE_BE_SPELL_TARGET2:
            case ACHIEVEMENT_CRITERIA_TYPE_CAST_SPELL:
            case ACHIEVEMENT_CRITERIA_TYPE_LAND_TARGETED_SPELL_ON_SPELL_TARGET:
            case ACHIEVEMENT_CRITERIA_TYPE_WIN_RATED_ARENA:
            case ACHIEVEMENT_CRITERIA_TYPE_USE_ITEM:
            case ACHIEVEMENT_CRITERIA_TYPE_ROLL_NEED_ON_LOOT:
            case ACHIEVEMENT_CRITERIA_TYPE_ROLL_GREED_ON_LOOT:
            case ACHIEVEMENT_CRITERIA_TYPE_DO_EMOTE:
            case ACHIEVEMENT_CRITERIA_TYPE_USE_GAMEOBJECT:
            case ACHIEVEMENT_CRITERIA_TYPE_FISH_IN_GAMEOBJECT:
            case ACHIEVEMENT_CRITERIA_TYPE_WIN_DUEL:
            case ACHIEVEMENT_CRITERIA_TYPE_HK_CLASS:
            case ACHIEVEMENT_CRITERIA_TYPE_HK_RACE:
            case ACHIEVEMENT_CRITERIA_TYPE_BG_OBJECTIVE_CAPTURE:
            case ACHIEVEMENT_CRITERIA_TYPE_HONORABLE_KILL:
            case ACHIEVEMENT_CRITERIA_TYPE_SPECIAL_PVP_KILL:
            case ACHIEVEMENT_CRITERIA_TYPE_GET_KILLING_BLOWS:
            case ACHIEVEMENT_CRITERIA_TYPE_HONORABLE_KILL_AT_AREA:
            case ACHIEVEMENT_CRITERIA_TYPE_WIN_ARENA: // This also behaves like ACHIEVEMENT_CRITERIA_TYPE_WIN_RATED_ARENA
            case ACHIEVEMENT_CRITERIA_TYPE_ON_LOGIN:
            case ACHIEVEMENT_CRITERIA_TYPE_HONORABLE_KILLS_GUILD:
            case ACHIEVEMENT_CRITERIA_TYPE_KILL_CREATURE_TYPE_GUILD:
            case ACHIEVEMENT_CRITERIA_TYPE_COMPLETE_GUILD_CHALLENGE_TYPE:
            case ACHIEVEMENT_CRITERIA_TYPE_COMPLETE_GUILD_CHALLENGE:
                SetCriteriaProgress(achievementCriteria, 1, referencePlayer, PROGRESS_ACCUMULATE);
                break;
                // std case: increment at miscValue1
            case ACHIEVEMENT_CRITERIA_TYPE_MONEY_FROM_VENDORS:
            case ACHIEVEMENT_CRITERIA_TYPE_GOLD_SPENT_FOR_TALENTS:
            case ACHIEVEMENT_CRITERIA_TYPE_MONEY_FROM_QUEST_REWARD:
            case ACHIEVEMENT_CRITERIA_TYPE_SPENT_GOLD_GUILD_REPAIRS:
            case ACHIEVEMENT_CRITERIA_TYPE_GOLD_SPENT_FOR_TRAVELLING:
            case ACHIEVEMENT_CRITERIA_TYPE_GOLD_SPENT_AT_BARBER:
            case ACHIEVEMENT_CRITERIA_TYPE_GOLD_SPENT_FOR_MAIL:
            case ACHIEVEMENT_CRITERIA_TYPE_LOOT_MONEY:
            case ACHIEVEMENT_CRITERIA_TYPE_GOLD_EARNED_BY_AUCTIONS:/* FIXME: for online player only currently */
            case ACHIEVEMENT_CRITERIA_TYPE_TOTAL_DAMAGE_RECEIVED:
            case ACHIEVEMENT_CRITERIA_TYPE_TOTAL_HEALING_RECEIVED:
            case ACHIEVEMENT_CRITERIA_TYPE_USE_LFD_TO_GROUP_WITH_PLAYERS:
            case ACHIEVEMENT_CRITERIA_TYPE_DAMAGE_DONE:
            case ACHIEVEMENT_CRITERIA_TYPE_HEALING_DONE:
                SetCriteriaProgress(achievementCriteria, miscValue1, referencePlayer, PROGRESS_ACCUMULATE);
                break;
            case ACHIEVEMENT_CRITERIA_TYPE_KILL_CREATURE:
            case ACHIEVEMENT_CRITERIA_TYPE_KILL_CREATURE_TYPE:
            case ACHIEVEMENT_CRITERIA_TYPE_LOOT_TYPE:
            case ACHIEVEMENT_CRITERIA_TYPE_OWN_ITEM:
            case ACHIEVEMENT_CRITERIA_TYPE_LOOT_ITEM:
            case ACHIEVEMENT_CRITERIA_TYPE_CURRENCY:
            case ACHIEVEMENT_CRITERIA_TYPE_CRAFT_ITEMS_GUILD:
            case ACHIEVEMENT_CRITERIA_TYPE_LOOT_EPIC_ITEM:
            case ACHIEVEMENT_CRITERIA_TYPE_RECEIVE_EPIC_ITEM:
            case ACHIEVEMENT_CRITERIA_TYPE_COMPLETE_ARCHAEOLOGY_PROJECTS:
                SetCriteriaProgress(achievementCriteria, miscValue2, referencePlayer, PROGRESS_ACCUMULATE);
                break;
                // std case: high value at miscValue1
            case ACHIEVEMENT_CRITERIA_TYPE_HIGHEST_AUCTION_BID:
            case ACHIEVEMENT_CRITERIA_TYPE_HIGHEST_AUCTION_SOLD: /* FIXME: for online player only currently */
            case ACHIEVEMENT_CRITERIA_TYPE_HIGHEST_HIT_DEALT:
            case ACHIEVEMENT_CRITERIA_TYPE_HIGHEST_HIT_RECEIVED:
            case ACHIEVEMENT_CRITERIA_TYPE_HIGHEST_HEAL_CAST:
            case ACHIEVEMENT_CRITERIA_TYPE_HIGHEST_HEALING_RECEIVED:
                SetCriteriaProgress(achievementCriteria, miscValue1, referencePlayer, PROGRESS_HIGHEST);
                break;
            case ACHIEVEMENT_CRITERIA_TYPE_REACH_LEVEL:
                SetCriteriaProgress(achievementCriteria, referencePlayer->getLevel(), referencePlayer);
                break;
            case ACHIEVEMENT_CRITERIA_TYPE_REACH_SKILL_LEVEL:
                if (uint32 skillvalue = referencePlayer->GetBaseSkillValue(achievementCriteria->Asset.SkillID))
                    SetCriteriaProgress(achievementCriteria, skillvalue, referencePlayer);
                break;
            case ACHIEVEMENT_CRITERIA_TYPE_LEARN_SKILL_LEVEL:
                if (uint32 skillStep = referencePlayer->GetSkillStep(achievementCriteria->Asset.SkillID))
                    SetCriteriaProgress(achievementCriteria, skillStep, referencePlayer);
                break;
            case ACHIEVEMENT_CRITERIA_TYPE_COMPLETE_QUEST_COUNT:
                SetCriteriaProgress(achievementCriteria, referencePlayer->GetRewardedQuestCount(), referencePlayer);
                break;
            case ACHIEVEMENT_CRITERIA_TYPE_COMPLETE_DAILY_QUEST_DAILY:
            {
                time_t nextDailyResetTime = sWorld->GetNextDailyQuestsResetTime();
                CriteriaProgress* progress = GetCriteriaProgress(achievementCriteria);

                if (!miscValue1) // Login case.
                {
                    // reset if player missed one day.
                    if (progress && progress->date < (nextDailyResetTime - 2 * DAY))
                        SetCriteriaProgress(achievementCriteria, 0, referencePlayer, PROGRESS_SET);
                    continue;
                }

                ProgressType progressType;
                if (!progress)
                    // 1st time. Start count.
                    progressType = PROGRESS_SET;
                else if (progress->date < (nextDailyResetTime - 2 * DAY))
                    // last progress is older than 2 days. Player missed 1 day => Restart count.
                    progressType = PROGRESS_SET;
                else if (progress->date < (nextDailyResetTime - DAY))
                    // last progress is between 1 and 2 days. => 1st time of the day.
                    progressType = PROGRESS_ACCUMULATE;
                else
                    // last progress is within the day before the reset => Already counted today.
                    continue;

                SetCriteriaProgress(achievementCriteria, 1, referencePlayer, progressType);
                break;
            }
            case ACHIEVEMENT_CRITERIA_TYPE_COMPLETE_QUESTS_IN_ZONE:
            {
                uint32 counter = 0;

                RewardedQuestSet const& rewQuests = referencePlayer->getRewardedQuests();
                for (RewardedQuestSet::const_iterator itr = rewQuests.begin(); itr != rewQuests.end(); ++itr)
                {
                    Quest const* quest = sObjectMgr->GetQuestTemplate(*itr);
                    if (quest && quest->GetZoneOrSort() >= 0 && uint32(quest->GetZoneOrSort()) == achievementCriteria->Asset.ZoneID)
                        ++counter;
                }
                SetCriteriaProgress(achievementCriteria, counter, referencePlayer);
                break;
            }
            case ACHIEVEMENT_CRITERIA_TYPE_FALL_WITHOUT_DYING:
                // miscValue1 is the ingame fallheight*100 as stored in dbc
                SetCriteriaProgress(achievementCriteria, miscValue1, referencePlayer);
                break;
            case ACHIEVEMENT_CRITERIA_TYPE_BUY_GUILD_BANK_SLOTS:
                SetCriteriaProgress(achievementCriteria, miscValue1, referencePlayer);
                break;
            case ACHIEVEMENT_CRITERIA_TYPE_COMPLETE_QUEST:
                SetCriteriaProgress(achievementCriteria, 1, referencePlayer, PROGRESS_ACCUMULATE);
                break;
            case ACHIEVEMENT_CRITERIA_TYPE_LEARN_SPELL:
            case ACHIEVEMENT_CRITERIA_TYPE_EXPLORE_AREA:
            case ACHIEVEMENT_CRITERIA_TYPE_VISIT_BARBER_SHOP:
            case ACHIEVEMENT_CRITERIA_TYPE_EQUIP_EPIC_ITEM:
            case ACHIEVEMENT_CRITERIA_TYPE_EQUIP_ITEM:
            case ACHIEVEMENT_CRITERIA_TYPE_COMPLETE_ACHIEVEMENT:
            case ACHIEVEMENT_CRITERIA_TYPE_BUY_GUILD_TABARD:
                SetCriteriaProgress(achievementCriteria, 1, referencePlayer);
                break;
            case ACHIEVEMENT_CRITERIA_TYPE_BUY_BANK_SLOT:
                SetCriteriaProgress(achievementCriteria, referencePlayer->GetBankBagSlotCount(), referencePlayer);
                break;
            case ACHIEVEMENT_CRITERIA_TYPE_GAIN_REPUTATION:
            {
                int32 reputation = referencePlayer->GetReputationMgr().GetReputation(achievementCriteria->Asset.FactionID);
                if (reputation > 0)
                    SetCriteriaProgress(achievementCriteria, reputation, referencePlayer);
                break;
            }
            case ACHIEVEMENT_CRITERIA_TYPE_GAIN_EXALTED_REPUTATION:
                SetCriteriaProgress(achievementCriteria, referencePlayer->GetReputationMgr().GetExaltedFactionCount(), referencePlayer);
                break;
            case ACHIEVEMENT_CRITERIA_TYPE_LEARN_SKILLLINE_SPELLS:
            {
                uint32 spellCount = 0;
                for (std::pair<uint32 const, PlayerSpell> const& spellIter : referencePlayer->GetSpellMap())
                {
                    SkillLineAbilityMapBounds bounds = sSpellMgr->GetSkillLineAbilityMapBounds(spellIter.first);
                    for (SkillLineAbilityMap::const_iterator skillIter = bounds.first; skillIter != bounds.second; ++skillIter)
                    {
                        if (skillIter->second->SkillLine == achievementCriteria->Asset.SkillID)
                        {
                            // do not add couter twice if by any chance skill is listed twice in dbc (eg. skill 777 and spell 22717)
                            ++spellCount;
                            break;
                        }
                    }
                }
                SetCriteriaProgress(achievementCriteria, spellCount, referencePlayer);
                break;
            }
            case ACHIEVEMENT_CRITERIA_TYPE_GAIN_REVERED_REPUTATION:
                SetCriteriaProgress(achievementCriteria, referencePlayer->GetReputationMgr().GetReveredFactionCount(), referencePlayer);
                break;
            case ACHIEVEMENT_CRITERIA_TYPE_GAIN_HONORED_REPUTATION:
                SetCriteriaProgress(achievementCriteria, referencePlayer->GetReputationMgr().GetHonoredFactionCount(), referencePlayer);
                break;
            case ACHIEVEMENT_CRITERIA_TYPE_KNOWN_FACTIONS:
                SetCriteriaProgress(achievementCriteria, referencePlayer->GetReputationMgr().GetVisibleFactionCount(), referencePlayer);
                break;
            case ACHIEVEMENT_CRITERIA_TYPE_LEARN_SKILL_LINE:
            {
                uint32 spellCount = 0;
                for (std::pair<uint32 const, PlayerSpell> const& spellIter : referencePlayer->GetSpellMap())
                {
                    SkillLineAbilityMapBounds bounds = sSpellMgr->GetSkillLineAbilityMapBounds(spellIter.first);
                    for (SkillLineAbilityMap::const_iterator skillIter = bounds.first; skillIter != bounds.second; ++skillIter)
                        if (skillIter->second->SkillLine == achievementCriteria->Asset.SkillID)
                        {
                            // do not add couter twice if by any chance skill is listed twice in dbc (eg. skill 777 and spell 22717)
                            ++spellCount;
                            break;
                        }
                }
                SetCriteriaProgress(achievementCriteria, spellCount, referencePlayer);
                break;
            }
            case ACHIEVEMENT_CRITERIA_TYPE_EARN_HONORABLE_KILL:
                SetCriteriaProgress(achievementCriteria, referencePlayer->GetUInt32Value(PLAYER_FIELD_LIFETIME_HONORABLE_KILLS), referencePlayer);
                break;
            case ACHIEVEMENT_CRITERIA_TYPE_HIGHEST_GOLD_VALUE_OWNED:
                SetCriteriaProgress(achievementCriteria, referencePlayer->GetMoney(), referencePlayer, PROGRESS_HIGHEST);
                break;
            case ACHIEVEMENT_CRITERIA_TYPE_EARN_ACHIEVEMENT_POINTS:
                if (!miscValue1)
                    SetCriteriaProgress(achievementCriteria, _achievementPoints, referencePlayer, PROGRESS_SET);
                else
                    SetCriteriaProgress(achievementCriteria, miscValue1, referencePlayer, PROGRESS_ACCUMULATE);
                break;
            case ACHIEVEMENT_CRITERIA_TYPE_EARN_GUILD_ACHIEVEMENT_POINTS:
                SetCriteriaProgress(achievementCriteria, miscValue1, referencePlayer, PROGRESS_ACCUMULATE);
                break;
            case ACHIEVEMENT_CRITERIA_TYPE_COMPLETE_QUESTS_GUILD:
                SetCriteriaProgress(achievementCriteria, 1, referencePlayer, PROGRESS_ACCUMULATE);
                break;
            case ACHIEVEMENT_CRITERIA_TYPE_HIGHEST_TEAM_RATING:
            {
                uint32 reqTeamType = achievementCriteria->Asset.TeamType;

                if (miscValue1)
                {
                    if (miscValue2 != reqTeamType)
                        continue;

                    SetCriteriaProgress(achievementCriteria, miscValue1, referencePlayer, PROGRESS_HIGHEST);
                }
                else // login case
                {
                    for (uint32 arena_slot = 0; arena_slot < MAX_ARENA_SLOT; ++arena_slot)
                    {
                        uint32 teamId = referencePlayer->GetArenaTeamId(arena_slot);
                        if (!teamId)
                            continue;

                        ArenaTeam* team = sArenaTeamMgr->GetArenaTeamById(teamId);
                        if (!team || team->GetType() != reqTeamType)
                            continue;

                        SetCriteriaProgress(achievementCriteria, team->GetStats().Rating, referencePlayer, PROGRESS_HIGHEST);
                        break;
                    }
                }

                break;
            }
            case ACHIEVEMENT_CRITERIA_TYPE_HIGHEST_PERSONAL_RATING:
            {
                uint32 reqTeamType = achievementCriteria->Asset.TeamType;

                if (miscValue1)
                {
                    if (miscValue2 != reqTeamType)
                        continue;

                    SetCriteriaProgress(achievementCriteria, miscValue1, referencePlayer, PROGRESS_HIGHEST);
                }
                else // login case
                {
                    for (uint32 arena_slot = 0; arena_slot < MAX_ARENA_SLOT; ++arena_slot)
                    {
                        uint32 teamId = referencePlayer->GetArenaTeamId(arena_slot);
                        if (!teamId)
                            continue;

                        ArenaTeam* team = sArenaTeamMgr->GetArenaTeamById(teamId);
                        if (!team || team->GetType() != reqTeamType)
                            continue;

                        if (ArenaTeamMember const* member = team->GetMember(referencePlayer->GetGUID()))
                        {
                            SetCriteriaProgress(achievementCriteria, member->PersonalRating, referencePlayer, PROGRESS_HIGHEST);
                            break;
                        }
                    }
                }
                break;
            }
            case ACHIEVEMENT_CRITERIA_TYPE_REACH_GUILD_LEVEL:
                SetCriteriaProgress(achievementCriteria, miscValue1, referencePlayer);
                break;
            case ACHIEVEMENT_CRITERIA_TYPE_CATCH_FROM_POOL:
                SetCriteriaProgress(achievementCriteria, 1, referencePlayer, PROGRESS_ACCUMULATE);
                break;
                // FIXME: not triggered in code as result, need to implement
            case ACHIEVEMENT_CRITERIA_TYPE_COMPLETE_RAID:
            case ACHIEVEMENT_CRITERIA_TYPE_PLAY_ARENA:
            case ACHIEVEMENT_CRITERIA_TYPE_OWN_RANK:
            case ACHIEVEMENT_CRITERIA_TYPE_WIN_RATED_BATTLEGROUND:
            case ACHIEVEMENT_CRITERIA_TYPE_REACH_BG_RATING:
                break;                                   // Not implemented yet :(
        }

        if (IsCompletedCriteria(achievementCriteria, achievement))
            CompletedCriteriaFor(achievement, referencePlayer);

        // check again the completeness for SUMM and REQ COUNT achievements,
        // as they don't depend on the completed criteria but on the sum of the progress of each individual criteria
        if (achievement->Flags & ACHIEVEMENT_FLAG_SUMM)
            if (IsCompletedAchievement(achievement))
                CompletedAchievement(achievement, referencePlayer);

        if (AchievementEntryList const* achRefList = sAchievementMgr->GetAchievementByReferencedId(achievement->ID))
            for (AchievementEntryList::const_iterator itr = achRefList->begin(); itr != achRefList->end(); ++itr)
                if (IsCompletedAchievement(*itr))
                    CompletedAchievement(*itr, referencePlayer);
    }
}

template<class T>
bool AchievementMgr<T>::IsCompletedCriteria(AchievementCriteriaEntry const* achievementCriteria, AchievementEntry const* achievement)
{
    if (!achievement)
        return false;

    // counter can never complete
    if (achievement->Flags & ACHIEVEMENT_FLAG_COUNTER)
        return false;

    if (achievement->Flags & (ACHIEVEMENT_FLAG_REALM_FIRST_REACH | ACHIEVEMENT_FLAG_REALM_FIRST_KILL | ACHIEVEMENT_FLAG_REALM_FIRST_GUILD))
    {
        // someone on this realm has already completed that achievement
        if (sAchievementMgr->IsRealmCompleted(achievement))
            return false;
    }

    CriteriaProgress const* progress = GetCriteriaProgress(achievementCriteria);
    if (!progress)
        return false;

    switch (AchievementCriteriaTypes(achievementCriteria->Type))
    {
        case ACHIEVEMENT_CRITERIA_TYPE_WIN_BG:
        case ACHIEVEMENT_CRITERIA_TYPE_KILL_CREATURE:
        case ACHIEVEMENT_CRITERIA_TYPE_COMPLETE_ARCHAEOLOGY_PROJECTS:
        case ACHIEVEMENT_CRITERIA_TYPE_REACH_LEVEL:
        case ACHIEVEMENT_CRITERIA_TYPE_REACH_GUILD_LEVEL:
        case ACHIEVEMENT_CRITERIA_TYPE_REACH_SKILL_LEVEL:
        case ACHIEVEMENT_CRITERIA_TYPE_COMPLETE_ACHIEVEMENT:
        case ACHIEVEMENT_CRITERIA_TYPE_COMPLETE_GUILD_CHALLENGE:
        case ACHIEVEMENT_CRITERIA_TYPE_COMPLETE_QUEST_COUNT:
        case ACHIEVEMENT_CRITERIA_TYPE_COMPLETE_DAILY_QUEST_DAILY:
        case ACHIEVEMENT_CRITERIA_TYPE_COMPLETE_QUESTS_IN_ZONE:
        case ACHIEVEMENT_CRITERIA_TYPE_DAMAGE_DONE:
        case ACHIEVEMENT_CRITERIA_TYPE_HEALING_DONE:
        case ACHIEVEMENT_CRITERIA_TYPE_COMPLETE_DAILY_QUEST:
        case ACHIEVEMENT_CRITERIA_TYPE_FALL_WITHOUT_DYING:
        case ACHIEVEMENT_CRITERIA_TYPE_COMPLETE_QUEST:
        case ACHIEVEMENT_CRITERIA_TYPE_BE_SPELL_TARGET:
        case ACHIEVEMENT_CRITERIA_TYPE_BE_SPELL_TARGET2:
        case ACHIEVEMENT_CRITERIA_TYPE_CAST_SPELL:
        case ACHIEVEMENT_CRITERIA_TYPE_LAND_TARGETED_SPELL_ON_SPELL_TARGET:
        case ACHIEVEMENT_CRITERIA_TYPE_BG_OBJECTIVE_CAPTURE:
        case ACHIEVEMENT_CRITERIA_TYPE_HONORABLE_KILL_AT_AREA:
        case ACHIEVEMENT_CRITERIA_TYPE_HONORABLE_KILL:
        case ACHIEVEMENT_CRITERIA_TYPE_EARN_HONORABLE_KILL:
        case ACHIEVEMENT_CRITERIA_TYPE_HONORABLE_KILLS_GUILD:
        case ACHIEVEMENT_CRITERIA_TYPE_KILL_CREATURE_TYPE_GUILD:
        case ACHIEVEMENT_CRITERIA_TYPE_OWN_ITEM:
        case ACHIEVEMENT_CRITERIA_TYPE_WIN_RATED_ARENA:
        case ACHIEVEMENT_CRITERIA_TYPE_HIGHEST_PERSONAL_RATING:
        case ACHIEVEMENT_CRITERIA_TYPE_USE_ITEM:
        case ACHIEVEMENT_CRITERIA_TYPE_LOOT_ITEM:
        case ACHIEVEMENT_CRITERIA_TYPE_BUY_GUILD_BANK_SLOTS:
        case ACHIEVEMENT_CRITERIA_TYPE_BUY_BANK_SLOT:
        case ACHIEVEMENT_CRITERIA_TYPE_GAIN_REPUTATION:
        case ACHIEVEMENT_CRITERIA_TYPE_COMPLETE_GUILD_CHALLENGE_TYPE:
        case ACHIEVEMENT_CRITERIA_TYPE_GAIN_EXALTED_REPUTATION:
        case ACHIEVEMENT_CRITERIA_TYPE_VISIT_BARBER_SHOP:
        case ACHIEVEMENT_CRITERIA_TYPE_EQUIP_EPIC_ITEM:
        case ACHIEVEMENT_CRITERIA_TYPE_ROLL_NEED_ON_LOOT:
        case ACHIEVEMENT_CRITERIA_TYPE_ROLL_GREED_ON_LOOT:
        case ACHIEVEMENT_CRITERIA_TYPE_HK_CLASS:
        case ACHIEVEMENT_CRITERIA_TYPE_HK_RACE:
        case ACHIEVEMENT_CRITERIA_TYPE_DO_EMOTE:
        case ACHIEVEMENT_CRITERIA_TYPE_EQUIP_ITEM:
        case ACHIEVEMENT_CRITERIA_TYPE_SPENT_GOLD_GUILD_REPAIRS:
        case ACHIEVEMENT_CRITERIA_TYPE_MONEY_FROM_QUEST_REWARD:
        case ACHIEVEMENT_CRITERIA_TYPE_LOOT_MONEY:
        case ACHIEVEMENT_CRITERIA_TYPE_USE_GAMEOBJECT:
        case ACHIEVEMENT_CRITERIA_TYPE_SPECIAL_PVP_KILL:
        case ACHIEVEMENT_CRITERIA_TYPE_FISH_IN_GAMEOBJECT:
        case ACHIEVEMENT_CRITERIA_TYPE_LEARN_SKILLLINE_SPELLS:
        case ACHIEVEMENT_CRITERIA_TYPE_WIN_DUEL:
        case ACHIEVEMENT_CRITERIA_TYPE_LOOT_TYPE:
        case ACHIEVEMENT_CRITERIA_TYPE_LEARN_SKILL_LINE:
        case ACHIEVEMENT_CRITERIA_TYPE_EARN_GUILD_ACHIEVEMENT_POINTS:
        case ACHIEVEMENT_CRITERIA_TYPE_COMPLETE_QUESTS_GUILD:
        case ACHIEVEMENT_CRITERIA_TYPE_USE_LFD_TO_GROUP_WITH_PLAYERS:
        case ACHIEVEMENT_CRITERIA_TYPE_GET_KILLING_BLOWS:
        case ACHIEVEMENT_CRITERIA_TYPE_CURRENCY:
        case ACHIEVEMENT_CRITERIA_TYPE_CRAFT_ITEMS_GUILD:
        case ACHIEVEMENT_CRITERIA_TYPE_CATCH_FROM_POOL:
            return progress->counter >= achievementCriteria->Quantity;
        case ACHIEVEMENT_CRITERIA_TYPE_EXPLORE_AREA:
        case ACHIEVEMENT_CRITERIA_TYPE_BUY_GUILD_TABARD:
        case ACHIEVEMENT_CRITERIA_TYPE_LEARN_SPELL:
            return progress->counter >= 1;
        case ACHIEVEMENT_CRITERIA_TYPE_LEARN_SKILL_LEVEL:
            return progress->counter >= achievementCriteria->Quantity;
        case ACHIEVEMENT_CRITERIA_TYPE_EARN_ACHIEVEMENT_POINTS:
            return progress->counter >= 9000;
        case ACHIEVEMENT_CRITERIA_TYPE_WIN_ARENA:
        case ACHIEVEMENT_CRITERIA_TYPE_LOOT_EPIC_ITEM:
            return achievementCriteria->Quantity && progress->counter >= achievementCriteria->Quantity;
        case ACHIEVEMENT_CRITERIA_TYPE_ON_LOGIN:
            return true;
            // handle all statistic-only criteria here
        case ACHIEVEMENT_CRITERIA_TYPE_COMPLETE_BATTLEGROUND:
        case ACHIEVEMENT_CRITERIA_TYPE_DEATH_AT_MAP:
        case ACHIEVEMENT_CRITERIA_TYPE_DEATH:
        case ACHIEVEMENT_CRITERIA_TYPE_DEATH_IN_DUNGEON:
        case ACHIEVEMENT_CRITERIA_TYPE_KILLED_BY_CREATURE:
        case ACHIEVEMENT_CRITERIA_TYPE_KILLED_BY_PLAYER:
        case ACHIEVEMENT_CRITERIA_TYPE_DEATHS_FROM:
        case ACHIEVEMENT_CRITERIA_TYPE_HIGHEST_TEAM_RATING:
        case ACHIEVEMENT_CRITERIA_TYPE_MONEY_FROM_VENDORS:
        case ACHIEVEMENT_CRITERIA_TYPE_GOLD_SPENT_FOR_TALENTS:
        case ACHIEVEMENT_CRITERIA_TYPE_NUMBER_OF_TALENT_RESETS:
        case ACHIEVEMENT_CRITERIA_TYPE_GOLD_SPENT_AT_BARBER:
        case ACHIEVEMENT_CRITERIA_TYPE_GOLD_SPENT_FOR_MAIL:
        case ACHIEVEMENT_CRITERIA_TYPE_LOSE_DUEL:
        case ACHIEVEMENT_CRITERIA_TYPE_KILL_CREATURE_TYPE:
        case ACHIEVEMENT_CRITERIA_TYPE_GOLD_EARNED_BY_AUCTIONS:
        case ACHIEVEMENT_CRITERIA_TYPE_CREATE_AUCTION:
        case ACHIEVEMENT_CRITERIA_TYPE_HIGHEST_AUCTION_BID:
        case ACHIEVEMENT_CRITERIA_TYPE_HIGHEST_AUCTION_SOLD:
        case ACHIEVEMENT_CRITERIA_TYPE_HIGHEST_GOLD_VALUE_OWNED:
        case ACHIEVEMENT_CRITERIA_TYPE_WON_AUCTIONS:
        case ACHIEVEMENT_CRITERIA_TYPE_GAIN_REVERED_REPUTATION:
        case ACHIEVEMENT_CRITERIA_TYPE_GAIN_HONORED_REPUTATION:
        case ACHIEVEMENT_CRITERIA_TYPE_KNOWN_FACTIONS:
        case ACHIEVEMENT_CRITERIA_TYPE_RECEIVE_EPIC_ITEM:
        case ACHIEVEMENT_CRITERIA_TYPE_ROLL_NEED:
        case ACHIEVEMENT_CRITERIA_TYPE_ROLL_GREED:
        case ACHIEVEMENT_CRITERIA_TYPE_QUEST_ABANDONED:
        case ACHIEVEMENT_CRITERIA_TYPE_FLIGHT_PATHS_TAKEN:
        case ACHIEVEMENT_CRITERIA_TYPE_ACCEPTED_SUMMONINGS:
        default:
            break;
    }
    return false;
}

template<class T>
void AchievementMgr<T>::CompletedCriteriaFor(AchievementEntry const* achievement, Player* referencePlayer)
{
    // counter can never complete
    if (achievement->Flags & ACHIEVEMENT_FLAG_COUNTER)
        return;

    // already completed and stored
    if (HasAchieved(achievement->ID))
        return;

    if (IsCompletedAchievement(achievement))
        CompletedAchievement(achievement, referencePlayer);
}

template<class T>
bool AchievementMgr<T>::IsCompletedAchievement(AchievementEntry const* entry)
{
    // counter can never complete
    if (entry->Flags & ACHIEVEMENT_FLAG_COUNTER)
        return false;

    // for achievement with referenced achievement criterias get from referenced and counter from self
    uint32 achievementForTestId = entry->SharesCriteria ? entry->SharesCriteria : entry->ID;
    uint32 achievementForTestCount = entry->MinimumCriteria;

    AchievementCriteriaEntryList const* cList = sAchievementMgr->GetAchievementCriteriaByAchievement(achievementForTestId);
    if (!cList)
        return false;
    uint64 count = 0;

    // For SUMM achievements, we have to count the progress of each criteria of the achievement.
    // Oddly, the target count is NOT contained in the achievement, but in each individual criteria
    if (entry->Flags & ACHIEVEMENT_FLAG_SUMM)
    {
        for (AchievementCriteriaEntryList::const_iterator itr = cList->begin(); itr != cList->end(); ++itr)
        {
            AchievementCriteriaEntry const* criteria = *itr;

            CriteriaProgress const* progress = GetCriteriaProgress(criteria);
            if (!progress)
                continue;

            count += progress->counter;

            // for counters, field4 contains the main count requirement
            if (count >= criteria->Quantity)
                return true;
        }
        return false;
    }

    // Default case - need complete all or
    bool completed_all = true;
    for (AchievementCriteriaEntryList::const_iterator itr = cList->begin(); itr != cList->end(); ++itr)
    {
        AchievementCriteriaEntry const* criteria = *itr;

        bool completed = IsCompletedCriteria(criteria, entry);

        // found an uncompleted criteria, but DONT return false yet - there might be a completed criteria with ACHIEVEMENT_CRITERIA_COMPLETE_FLAG_ALL
        if (completed)
            ++count;
        else
            completed_all = false;

        // completed as have req. count of completed criterias
        if (achievementForTestCount > 0 && achievementForTestCount <= count)
           return true;
    }

    // all criterias completed requirement
    if (completed_all && achievementForTestCount == 0)
        return true;

    return false;
}

template<class T>
CriteriaProgress* AchievementMgr<T>::GetCriteriaProgress(AchievementCriteriaEntry const* entry)
{
    CriteriaProgressMap::iterator iter = m_criteriaProgress.find(entry->ID);

    if (iter == m_criteriaProgress.end())
        return nullptr;

    return &(iter->second);
}

template<class T>
void AchievementMgr<T>::SetCriteriaProgress(AchievementCriteriaEntry const* entry, uint64 changeValue, Player* referencePlayer, ProgressType ptype)
{
    TC_LOG_DEBUG("achievement", "SetCriteriaProgress(%u, " UI64FMTD ") for (%s)",
                   entry->ID, changeValue, GetOwner()->GetGUID().ToString().c_str());

    CriteriaProgress* progress = GetCriteriaProgress(entry);
    if (!progress)
    {
        // not create record for 0 counter but allow it for timed achievements
        // we will need to send 0 progress to client to start the timer
        if (changeValue == 0 && !entry->TimerTime)
            return;

        progress = &m_criteriaProgress[entry->ID];
        progress->counter = changeValue;
    }
    else
    {
        uint64 newValue = 0;
        switch (ptype)
        {
            case PROGRESS_SET:
                newValue = changeValue;
                break;
            case PROGRESS_ACCUMULATE:
            {
                // avoid overflow
                uint64 max_value = std::numeric_limits<uint64>::max();
                newValue = max_value - progress->counter > changeValue ? progress->counter + changeValue : max_value;
                break;
            }
            case PROGRESS_HIGHEST:
                newValue = progress->counter < changeValue ? changeValue : progress->counter;
                break;
        }

        // not update (not mark as changed) if counter will have same value
        if (progress->counter == newValue && !entry->TimerTime)
            return;

        progress->counter = newValue;
    }

    progress->changed = true;
    progress->date = GameTime::GetGameTime(); // set the date to the latest update.
    progress->CompletedGUID = referencePlayer ? referencePlayer->GetGUID() : ObjectGuid::Empty;

    Seconds timeElapsed = Seconds::zero();

    AchievementEntry const* achievement = sAchievementMgr->GetAchievement(entry->AchievementID);
    bool criteriaComplete = IsCompletedCriteria(entry, achievement);
    if (entry->TimerTime)
    {
        auto startedItr = _startedCriteria.find(entry->ID);
        if (startedItr != _startedCriteria.end())
        {
            // Client expects this in packet
            timeElapsed = duration_cast<Seconds>(Seconds(entry->TimerTime) - startedItr->second);

            // Remove the timer, we wont need it anymore
            if (criteriaComplete)
                 _startedCriteria.erase(startedItr);
		}
    }

    SendCriteriaUpdate(entry, progress, timeElapsed.count(), criteriaComplete);
}

template<class T>
void AchievementMgr<T>::UpdateTimedAchievementCriteria(Milliseconds timeDiff)
{
    for (auto itr = _startedCriteria.begin(); itr != _startedCriteria.end();)
    {
        // Time is up, remove timer and reset progress
        if (itr->second <= timeDiff)
        {
            AchievementCriteriaEntry const* entry = sAchievementMgr->GetAchievementCriteria(itr->first);
            RemoveCriteriaProgress(entry);
            itr = _startedCriteria.erase(itr);
        }
        else
        {
            itr->second -= timeDiff;
            ++itr;
        }
    }
}

template<class T>
void AchievementMgr<T>::StartAchievementCriteria(AchievementCriteriaStartEvent /*startEvent*/, uint32 /*entry*/, Milliseconds /*timeLost = Milliseconds::zero() */)
{
}

template<>
void AchievementMgr<Player>::StartAchievementCriteria(AchievementCriteriaStartEvent startEvent, uint32 asset, Milliseconds timeLost /*= Milliseconds::zero()*/)
{
    AchievementCriteriaEntryList const* criteriaList = sAchievementMgr->GetAchievementCriteriaByStartEvent(startEvent, asset);
    if (!criteriaList)
        return;

    for (AchievementCriteriaEntry const* criteria : *criteriaList)
    {
        Milliseconds timeLimit = Milliseconds::max(); // this value is for criteria that have a start event requirement but no time limit
        if (criteria->TimerTime)
            timeLimit = Seconds(criteria->TimerTime);

        timeLimit -= timeLost;

        if (timeLimit <= Milliseconds::zero())
            continue;

        AchievementEntry const* achievement = sAchievementMgr->GetAchievement(criteria->AchievementID);
        bool canStart = !IsCompletedCriteria(criteria, achievement);

        if (!canStart)
            continue;

        auto [itr, isNew] = _startedCriteria.try_emplace(criteria->ID, timeLimit);
        if (!isNew)
        {
            if (!(criteria->Flags & ACHIEVEMENT_CRITERIA_FLAG_HIDDEN))
                continue;

            itr->second = timeLimit;
        }

        SetCriteriaProgress(criteria, 0, GetOwner(), PROGRESS_SET);
    }
}

template<class T>
void AchievementMgr<T>::FailAchievementCriteria(AchievementCriteriaFailEvent failEvent, uint32 asset)
{
    AchievementCriteriaEntryList const* criteriaList = sAchievementMgr->GetAchievementCriteriaByFailEvent(failEvent, asset);
    if (!criteriaList)
        return;

    for (AchievementCriteriaEntry const* criteria : *criteriaList)
    {
        _startedCriteria.erase(criteria->ID);
        RemoveCriteriaProgress(criteria);
    }
}

template<>
void AchievementMgr<Player>::CompletedAchievement(AchievementEntry const* achievement, Player* referencePlayer)
{
    // disable for gamemasters with GM-mode enabled
    if (GetOwner()->IsGameMaster())
        return;

    if (achievement->Flags & ACHIEVEMENT_FLAG_COUNTER || HasAchieved(achievement->ID))
        return;

    if (achievement->Flags & ACHIEVEMENT_FLAG_SHOW_IN_GUILD_NEWS)
        if (Guild* guild = referencePlayer->GetGuild())
            guild->AddGuildNews(GUILD_NEWS_PLAYER_ACHIEVEMENT, referencePlayer->GetGUID(), achievement->Flags & ACHIEVEMENT_FLAG_SHOW_IN_GUILD_HEADER, achievement->ID);

    if (!GetOwner()->GetSession()->PlayerLoading())
        SendAchievementEarned(achievement);

    TC_LOG_INFO("achievement", "AchievementMgr::CompletedAchievement(%u). Player: %s (%u)",
        achievement->ID, GetOwner()->GetName().c_str(), GetOwner()->GetGUID().GetCounter());

    CompletedAchievementData& ca = m_completedAchievements[achievement->ID];
    ca.date = GameTime::GetGameTime();
    ca.changed = true;

    if (achievement->Flags & (ACHIEVEMENT_FLAG_REALM_FIRST_REACH | ACHIEVEMENT_FLAG_REALM_FIRST_KILL))
        sAchievementMgr->SetRealmCompleted(achievement);

    _achievementPoints += achievement->Points;

    UpdateAchievementCriteria(ACHIEVEMENT_CRITERIA_TYPE_COMPLETE_ACHIEVEMENT, achievement->ID, 0, 0, nullptr, referencePlayer);
    UpdateAchievementCriteria(ACHIEVEMENT_CRITERIA_TYPE_EARN_ACHIEVEMENT_POINTS, achievement->Points, 0, 0, nullptr, referencePlayer);

    // reward items and titles if any
    AchievementReward const* reward = sAchievementMgr->GetAchievementReward(achievement);

    // no rewards
    if (!reward)
        return;

    // titles
    //! Currently there's only one achievement that deals with gender-specific titles.
    //! Since no common attributes were found, (not even in titleRewardFlags field)
    //! we explicitly check by ID. Maybe in the future we could move the achievement_reward
    //! condition fields to the condition system.
    if (uint32 titleId = reward->TitleID[achievement->ID == 1793 ? GetOwner()->GetByteValue(PLAYER_BYTES_3, PLAYER_BYTES_3_OFFSET_GENDER) : (GetOwner()->GetTeam() == ALLIANCE ? 0 : 1)])
        if (CharTitlesEntry const* titleEntry = sCharTitlesStore.LookupEntry(titleId))
            GetOwner()->SetTitle(titleEntry);

    // mail
    if (reward->Sender)
    {
        MailDraft draft(reward->MailTemplateID);

        if (!reward->MailTemplateID)
        {
            // subject and text
            std::string subject = reward->Subject;
            std::string text = reward->Text;

            LocaleConstant localeConstant = GetOwner()->GetSession()->GetSessionDbLocaleIndex();
            if (localeConstant != LOCALE_enUS)
            {
                if (AchievementRewardLocale const* loc = sAchievementMgr->GetAchievementRewardLocale(achievement))
                {
                    ObjectMgr::GetLocaleString(loc->Subject, localeConstant, subject);
                    ObjectMgr::GetLocaleString(loc->Text, localeConstant, text);
                }
            }

            draft = MailDraft(subject, text);
        }

        CharacterDatabaseTransaction trans = CharacterDatabase.BeginTransaction();

        Item* item = reward->ItemID ? Item::CreateItem(reward->ItemID, 1, GetOwner()) : nullptr;
        if (item)
        {
            // save new item before send
            item->SaveToDB(trans);                               // save for prevent lost at next mail load, if send fail then item will deleted

            // item
            draft.AddItem(item);
        }

        draft.SendMailTo(trans, GetOwner(), MailSender(MAIL_CREATURE, reward->Sender));
        CharacterDatabase.CommitTransaction(trans);
    }
}

template<>
void AchievementMgr<Guild>::CompletedAchievement(AchievementEntry const* achievement, Player* referencePlayer)
{
    TC_LOG_DEBUG("achievement", "AchievementMgr<Guild>::CompletedAchievement(%u)", achievement->ID);

    if (achievement->Flags & ACHIEVEMENT_FLAG_COUNTER || HasAchieved(achievement->ID))
        return;

    if (achievement->Flags & ACHIEVEMENT_FLAG_SHOW_IN_GUILD_NEWS)
        if (Guild* guild = referencePlayer->GetGuild())
            guild->AddGuildNews(GUILD_NEWS_GUILD_ACHIEVEMENT, ObjectGuid::Empty, achievement->Flags & ACHIEVEMENT_FLAG_SHOW_IN_GUILD_HEADER, achievement->ID);

    SendAchievementEarned(achievement);
    CompletedAchievementData& ca = m_completedAchievements[achievement->ID];
    ca.date = GameTime::GetGameTime();
    ca.changed = true;

    if (achievement->Flags & ACHIEVEMENT_FLAG_SHOW_GUILD_MEMBERS)
    {
        if (referencePlayer->GetGuildId() == GetOwner()->GetId())
            ca.guids.insert(referencePlayer->GetGUID());

        if (Group const* group = referencePlayer->GetGroup())
            for (GroupReference const* ref = group->GetFirstMember(); ref != nullptr; ref = ref->next())
                if (Player const* groupMember = ref->GetSource())
                    if (groupMember->GetGuildId() == GetOwner()->GetId())
                        ca.guids.insert(groupMember->GetGUID());
    }

    sAchievementMgr->SetRealmCompleted(achievement);

    _achievementPoints += achievement->Points;

    UpdateAchievementCriteria(ACHIEVEMENT_CRITERIA_TYPE_COMPLETE_ACHIEVEMENT, achievement->ID, 0, 0, nullptr, referencePlayer);
    UpdateAchievementCriteria(ACHIEVEMENT_CRITERIA_TYPE_EARN_GUILD_ACHIEVEMENT_POINTS, achievement->Points, 0, 0, nullptr, referencePlayer);
}

struct VisibleAchievementPred
{
    bool operator()(CompletedAchievementMap::value_type const& val)
    {
        AchievementEntry const* achievement = sAchievementMgr->GetAchievement(val.first);
        return achievement && !(achievement->Flags & ACHIEVEMENT_FLAG_HIDDEN);
    }
};

template<class T>
void AchievementMgr<T>::SendAllAchievementData(Player* /*receiver*/) const
{
    VisibleAchievementPred isVisible;
    size_t numCriteria = m_criteriaProgress.size();
    size_t numAchievements = std::count_if(m_completedAchievements.begin(), m_completedAchievements.end(), isVisible);
    ByteBuffer criteriaData(numCriteria * (4 + 4 + 4 + 4 + 8 + 8));
    ObjectGuid guid = GetOwner()->GetGUID();
    ObjectGuid counter;

    WorldPacket data(SMSG_ALL_ACHIEVEMENT_DATA, 4 + numAchievements * (4 + 4) + 4 + numCriteria * (4 + 4 + 4 + 4 + 8 + 8));
    data.WriteBits(numCriteria, 21);
    for (CriteriaProgressMap::const_iterator itr = m_criteriaProgress.begin(); itr != m_criteriaProgress.end(); ++itr)
    {
        counter.Set(itr->second.counter);

        data.WriteBit(guid[4]);
        data.WriteBit(counter[3]);
        data.WriteBit(guid[5]);
        data.WriteBit(counter[0]);
        data.WriteBit(counter[6]);
        data.WriteBit(guid[3]);
        data.WriteBit(guid[0]);
        data.WriteBit(counter[4]);
        data.WriteBit(guid[2]);
        data.WriteBit(counter[7]);
        data.WriteBit(guid[7]);
        data.WriteBits(0u, 2);
        data.WriteBit(guid[6]);
        data.WriteBit(counter[2]);
        data.WriteBit(counter[1]);
        data.WriteBit(counter[5]);
        data.WriteBit(guid[1]);

        criteriaData.WriteByteSeq(guid[3]);
        criteriaData.WriteByteSeq(counter[5]);
        criteriaData.WriteByteSeq(counter[6]);
        criteriaData.WriteByteSeq(guid[4]);
        criteriaData.WriteByteSeq(guid[6]);
        criteriaData.WriteByteSeq(counter[2]);
        criteriaData << uint32(0); // timer 2
        criteriaData.WriteByteSeq(guid[2]);
        criteriaData << uint32(itr->first); // criteria id
        criteriaData.WriteByteSeq(guid[5]);
        criteriaData.WriteByteSeq(counter[0]);
        criteriaData.WriteByteSeq(counter[3]);
        criteriaData.WriteByteSeq(counter[1]);
        criteriaData.WriteByteSeq(counter[4]);
        criteriaData.WriteByteSeq(guid[0]);
        criteriaData.WriteByteSeq(guid[7]);
        criteriaData.WriteByteSeq(counter[7]);
        criteriaData << uint32(0); // timer 1
        criteriaData.AppendPackedTime(itr->second.date);  // criteria date
        criteriaData.WriteByteSeq(guid[1]);
    }

    data.WriteBits(numAchievements, 23);
    data.FlushBits();
    data.append(criteriaData);

    for (CompletedAchievementMap::const_iterator itr = m_completedAchievements.begin(); itr != m_completedAchievements.end(); ++itr)
    {
        if (!isVisible(*itr))
            continue;

        data << uint32(itr->first);
        data.AppendPackedTime(itr->second.date);
    }

    SendPacket(&data);
}

template<>
void AchievementMgr<Guild>::SendAllAchievementData(Player* receiver) const
{
    VisibleAchievementPred isVisible;

    auto count = std::count_if(m_completedAchievements.begin(), m_completedAchievements.end(), isVisible);

    WorldPacket data(SMSG_GUILD_ACHIEVEMENT_DATA, count * (4 + 4) + 3);
    data.WriteBits(count, 23);
    data.FlushBits();

    for (CompletedAchievementMap::const_iterator itr = m_completedAchievements.begin(); itr != m_completedAchievements.end(); ++itr)
    {
        if (!isVisible(*itr))
            continue;

        data.AppendPackedTime(itr->second.date);
        data << uint32(itr->first);
    }

    receiver->SendDirectMessage(&data);
}

template<>
void AchievementMgr<Player>::SendAchievementInfo(Player* receiver, uint32 /*achievementId = 0 */) const
{
    ObjectGuid guid = GetOwner()->GetGUID();
    ObjectGuid counter;

    VisibleAchievementPred isVisible;
    size_t numCriteria = m_criteriaProgress.size();
    size_t numAchievements = std::count_if(m_completedAchievements.begin(), m_completedAchievements.end(), isVisible);
    ByteBuffer criteriaData(numCriteria * 16);

    WorldPacket data(SMSG_RESPOND_INSPECT_ACHIEVEMENTS, 1 + 8 + 3 + 3 + numAchievements * (4 + 4) + numCriteria * (0));
    data.WriteBit(guid[7]);
    data.WriteBit(guid[4]);
    data.WriteBit(guid[1]);
    data.WriteBits(numAchievements, 23);
    data.WriteBit(guid[0]);
    data.WriteBit(guid[3]);
    data.WriteBits(numCriteria, 21);
    data.WriteBit(guid[2]);
    for (CriteriaProgressMap::const_iterator itr = m_criteriaProgress.begin(); itr != m_criteriaProgress.end(); ++itr)
    {
        counter.Set(itr->second.counter);

        data.WriteBit(counter[5]);
        data.WriteBit(counter[3]);
        data.WriteBit(guid[1]);
        data.WriteBit(guid[4]);
        data.WriteBit(guid[2]);
        data.WriteBit(counter[6]);
        data.WriteBit(guid[0]);
        data.WriteBit(counter[4]);
        data.WriteBit(counter[1]);
        data.WriteBit(counter[2]);
        data.WriteBit(guid[3]);
        data.WriteBit(guid[7]);
        data.WriteBits(0, 2);           // criteria progress flags
        data.WriteBit(counter[0]);
        data.WriteBit(guid[5]);
        data.WriteBit(guid[6]);
        data.WriteBit(counter[7]);

        criteriaData.WriteByteSeq(guid[3]);
        criteriaData.WriteByteSeq(counter[4]);
        criteriaData << uint32(0);      // timer 1
        criteriaData.WriteByteSeq(guid[1]);
        criteriaData.AppendPackedTime(itr->second.date);
        criteriaData.WriteByteSeq(counter[3]);
        criteriaData.WriteByteSeq(counter[7]);
        criteriaData.WriteByteSeq(guid[5]);
        criteriaData.WriteByteSeq(counter[0]);
        criteriaData.WriteByteSeq(guid[4]);
        criteriaData.WriteByteSeq(guid[2]);
        criteriaData.WriteByteSeq(guid[6]);
        criteriaData.WriteByteSeq(guid[7]);
        criteriaData.WriteByteSeq(counter[6]);
        criteriaData << uint32(itr->first);
        criteriaData << uint32(0);      // timer 2
        criteriaData.WriteByteSeq(counter[1]);
        criteriaData.WriteByteSeq(counter[5]);
        criteriaData.WriteByteSeq(guid[0]);
        criteriaData.WriteByteSeq(counter[2]);
    }

    data.WriteBit(guid[6]);
    data.WriteBit(guid[5]);
    data.FlushBits();
    data.append(criteriaData);
    data.WriteByteSeq(guid[1]);
    data.WriteByteSeq(guid[6]);
    data.WriteByteSeq(guid[3]);
    data.WriteByteSeq(guid[0]);
    data.WriteByteSeq(guid[2]);

    for (CompletedAchievementMap::const_iterator itr = m_completedAchievements.begin(); itr != m_completedAchievements.end(); ++itr)
    {
        if (!isVisible(*itr))
            continue;

        data << uint32(itr->first);
        data.AppendPackedTime(itr->second.date);
    }

    data.WriteByteSeq(guid[7]);
    data.WriteByteSeq(guid[4]);
    data.WriteByteSeq(guid[5]);

    receiver->SendDirectMessage(&data);
}

template<>
void AchievementMgr<Guild>::SendAchievementInfo(Player* receiver, uint32 achievementId /*= 0*/) const
{
    //will send response to criteria progress request
    AchievementCriteriaEntryList const* criteria = sAchievementMgr->GetAchievementCriteriaByAchievement(achievementId);
    if (!criteria)
    {
        // send empty packet
        WorldPacket data(SMSG_GUILD_CRITERIA_DATA, 3);
        data.WriteBits(0, 21);
        data.FlushBits();
        receiver->GetSession()->SendPacket(&data);
        return;
    }

    ObjectGuid counter;
    ObjectGuid guid;
    uint32 numCriteria = 0;
    ByteBuffer criteriaData(criteria->size() * (8 + 8 + 4 + 4 + 4));
    ByteBuffer criteriaBits(criteria->size() * (8 + 8) / 8);
    for (AchievementCriteriaEntryList::const_iterator itr = criteria->begin(); itr != criteria->end(); ++itr)
    {
        uint32 criteriaId = (*itr)->ID;
        CriteriaProgressMap::const_iterator progress = m_criteriaProgress.find(criteriaId);
        if (progress == m_criteriaProgress.end())
            continue;

        ++numCriteria;
    }

    if (!numCriteria)
    {
        WorldPacket data(SMSG_GUILD_CRITERIA_DATA, 3);
        data.WriteBits(0, 21);
        receiver->GetSession()->SendPacket(&data);
        return;
    }

    WorldPacket data(SMSG_GUILD_CRITERIA_DATA, 3 + numCriteria * (1 + 1 + 1 + 1 + 4 + 4 + 4 + 4 + 4));
    data.WriteBits(numCriteria, 21);

    for (AchievementCriteriaEntryList::const_iterator itr = criteria->begin(); itr != criteria->end(); ++itr)
    {
        uint32 criteriaId = (*itr)->ID;
        CriteriaProgressMap::const_iterator progress = m_criteriaProgress.find(criteriaId);
        if (progress == m_criteriaProgress.end())
            continue;

        counter.Set(progress->second.counter);
        guid = progress->second.CompletedGUID;

        data.WriteBit(counter[4]);
        data.WriteBit(counter[1]);
        data.WriteBit(guid[2]);
        data.WriteBit(counter[3]);
        data.WriteBit(guid[1]);
        data.WriteBit(counter[5]);
        data.WriteBit(counter[0]);
        data.WriteBit(guid[3]);
        data.WriteBit(counter[2]);
        data.WriteBit(guid[7]);
        data.WriteBit(guid[5]);
        data.WriteBit(guid[0]);
        data.WriteBit(counter[6]);
        data.WriteBit(guid[6]);
        data.WriteBit(counter[7]);
        data.WriteBit(guid[4]);
    }

    for (AchievementCriteriaEntryList::const_iterator itr = criteria->begin(); itr != criteria->end(); ++itr)
    {
        uint32 criteriaId = (*itr)->ID;
        CriteriaProgressMap::const_iterator progress = m_criteriaProgress.find(criteriaId);
        if (progress == m_criteriaProgress.end())
            continue;

        counter.Set(progress->second.counter);
        guid = progress->second.CompletedGUID;

        data.WriteByteSeq(guid[5]);
        data << uint32(progress->second.date);      // unknown date
        data.WriteByteSeq(counter[3]);
        data.WriteByteSeq(counter[7]);
        data << uint32(progress->second.date);      // unknown date
        data.WriteByteSeq(counter[6]);
        data.WriteByteSeq(guid[4]);
        data.WriteByteSeq(guid[1]);
        data.WriteByteSeq(counter[4]);
        data.WriteByteSeq(guid[3]);
        data.WriteByteSeq(counter[0]);
        data.WriteByteSeq(guid[2]);
        data.WriteByteSeq(counter[1]);
        data.WriteByteSeq(guid[6]);
        data << uint32(progress->second.date);      // last update time (not packed!)
        data << uint32(criteriaId);
        data.WriteByteSeq(counter[5]);
        data << uint32(0);
        data.WriteByteSeq(guid[7]);
        data.WriteByteSeq(counter[2]);
        data.WriteByteSeq(guid[0]);
    }

    receiver->SendDirectMessage(&data);
}

template<class T>
bool AchievementMgr<T>::HasAchieved(uint32 achievementId) const
{
    return m_completedAchievements.find(achievementId) != m_completedAchievements.end();
}

template<class T>
CompletedAchievementData* AchievementMgr<T>::GetCompletedDataForAchievement(uint32 achievementId)
{
    CompletedAchievementMap::iterator itr = m_completedAchievements.find(achievementId);
    if(itr != m_completedAchievements.end())
    {
        return &(itr->second);
    }
    else
    {
        return nullptr;
    }
}

template<class T>
bool AchievementMgr<T>::CanUpdateCriteria(AchievementCriteriaEntry const* criteria, AchievementEntry const* achievement, uint64 miscValue1, uint64 miscValue2, uint64 miscValue3, WorldObject const* ref, Player* referencePlayer, GameObject* go)
{
    if (DisableMgr::IsDisabledFor(DISABLE_TYPE_ACHIEVEMENT_CRITERIA, criteria->ID, nullptr))
    {
        TC_LOG_TRACE("achievement", "CanUpdateCriteria: (Id: %u Type %s) Disabled",
            criteria->ID, AchievementGlobalMgr::GetCriteriaTypeString(criteria->Type));
        return false;
    }

    if ((achievement->Faction == ACHIEVEMENT_FACTION_HORDE    && referencePlayer->GetTeam() != HORDE) ||
        (achievement->Faction == ACHIEVEMENT_FACTION_ALLIANCE && referencePlayer->GetTeam() != ALLIANCE))
    {
        TC_LOG_TRACE("achievement", "CanUpdateCriteria: (Id: %u Type %s Achievement %u) Wrong faction",
            criteria->ID, AchievementGlobalMgr::GetCriteriaTypeString(criteria->Type), achievement->ID);
        return false;
    }

    if (IsCompletedCriteria(criteria, achievement))
    {
        TC_LOG_TRACE("achievement", "CanUpdateCriteria: %s (Id: %u Type %s) Is Completed",
            criteria->Description, criteria->ID, AchievementGlobalMgr::GetCriteriaTypeString(criteria->Type));
        return false;
    }

    if (!RequirementsSatisfied(criteria, miscValue1, miscValue2, miscValue3, ref, referencePlayer, go))
    {
        TC_LOG_TRACE("achievement", "CanUpdateCriteria: %s (Id: %u Type %s) Requirements not satisfied",
            criteria->Description, criteria->ID, AchievementGlobalMgr::GetCriteriaTypeString(criteria->Type));
        return false;
    }

    if (!AdditionalRequirementsSatisfied(criteria, miscValue1, miscValue2, ref, referencePlayer))
    {
        TC_LOG_TRACE("achievement", "CanUpdateCriteria: %s (Id: %u Type %s) Additional requirements not satisfied",
            criteria->Description, criteria->ID, AchievementGlobalMgr::GetCriteriaTypeString(criteria->Type));
        return false;
    }

    if (!ConditionsSatisfied(criteria, referencePlayer))
    {
        TC_LOG_TRACE("achievement", "CanUpdateCriteria: (Id: %u Type %s) Conditions not satisfied",
            criteria->ID, AchievementGlobalMgr::GetCriteriaTypeString(criteria->Type));
        return false;
    }

    if (criteria->RequiredWorldStateID != 0)
        if (sWorldStateMgr->GetValue(criteria->RequiredWorldStateID, referencePlayer->GetMap()) != criteria->RequiredWorldStateValue)
            return false;

    return true;
}

template<class T>
bool AchievementMgr<T>::ConditionsSatisfied(AchievementCriteriaEntry const* criteria, Player* /*referencePlayer*/) const
{
    if (criteria->StartEvent && !_startedCriteria.contains(criteria->ID))
        return false;

    return true;
}

template<class T>
bool AchievementMgr<T>::RequirementsSatisfied(AchievementCriteriaEntry const* achievementCriteria, uint64 miscValue1, uint64 miscValue2, uint64 miscValue3, WorldObject const* ref, Player* referencePlayer, GameObject* go) const
{
    switch (AchievementCriteriaTypes(achievementCriteria->Type))
    {
        case ACHIEVEMENT_CRITERIA_TYPE_ACCEPTED_SUMMONINGS:
        case ACHIEVEMENT_CRITERIA_TYPE_COMPLETE_DAILY_QUEST:
        case ACHIEVEMENT_CRITERIA_TYPE_CREATE_AUCTION:
        case ACHIEVEMENT_CRITERIA_TYPE_FALL_WITHOUT_DYING:
        case ACHIEVEMENT_CRITERIA_TYPE_FLIGHT_PATHS_TAKEN:
        case ACHIEVEMENT_CRITERIA_TYPE_GET_KILLING_BLOWS:
        case ACHIEVEMENT_CRITERIA_TYPE_GOLD_EARNED_BY_AUCTIONS:
        case ACHIEVEMENT_CRITERIA_TYPE_GOLD_SPENT_AT_BARBER:
        case ACHIEVEMENT_CRITERIA_TYPE_GOLD_SPENT_FOR_MAIL:
        case ACHIEVEMENT_CRITERIA_TYPE_GOLD_SPENT_FOR_TALENTS:
        case ACHIEVEMENT_CRITERIA_TYPE_GOLD_SPENT_FOR_TRAVELLING:
        case ACHIEVEMENT_CRITERIA_TYPE_HIGHEST_AUCTION_BID:
        case ACHIEVEMENT_CRITERIA_TYPE_HIGHEST_AUCTION_SOLD:
        case ACHIEVEMENT_CRITERIA_TYPE_HIGHEST_HEALING_RECEIVED:
        case ACHIEVEMENT_CRITERIA_TYPE_HIGHEST_HEAL_CAST:
        case ACHIEVEMENT_CRITERIA_TYPE_HIGHEST_HIT_DEALT:
        case ACHIEVEMENT_CRITERIA_TYPE_HIGHEST_HIT_RECEIVED:
        case ACHIEVEMENT_CRITERIA_TYPE_HONORABLE_KILL:
        case ACHIEVEMENT_CRITERIA_TYPE_LOOT_MONEY:
        case ACHIEVEMENT_CRITERIA_TYPE_LOSE_DUEL:
        case ACHIEVEMENT_CRITERIA_TYPE_MONEY_FROM_QUEST_REWARD:
        case ACHIEVEMENT_CRITERIA_TYPE_MONEY_FROM_VENDORS:
        case ACHIEVEMENT_CRITERIA_TYPE_NUMBER_OF_TALENT_RESETS:
        case ACHIEVEMENT_CRITERIA_TYPE_QUEST_ABANDONED:
        case ACHIEVEMENT_CRITERIA_TYPE_REACH_GUILD_LEVEL:
        case ACHIEVEMENT_CRITERIA_TYPE_ROLL_GREED:
        case ACHIEVEMENT_CRITERIA_TYPE_ROLL_NEED:
        case ACHIEVEMENT_CRITERIA_TYPE_SPECIAL_PVP_KILL:
        case ACHIEVEMENT_CRITERIA_TYPE_TOTAL_DAMAGE_RECEIVED:
        case ACHIEVEMENT_CRITERIA_TYPE_TOTAL_HEALING_RECEIVED:
        case ACHIEVEMENT_CRITERIA_TYPE_USE_LFD_TO_GROUP_WITH_PLAYERS:
        case ACHIEVEMENT_CRITERIA_TYPE_VISIT_BARBER_SHOP:
        case ACHIEVEMENT_CRITERIA_TYPE_WIN_DUEL:
        case ACHIEVEMENT_CRITERIA_TYPE_WIN_RATED_ARENA:
        case ACHIEVEMENT_CRITERIA_TYPE_WON_AUCTIONS:
        case ACHIEVEMENT_CRITERIA_TYPE_SPENT_GOLD_GUILD_REPAIRS:
        case ACHIEVEMENT_CRITERIA_TYPE_BUY_GUILD_BANK_SLOTS:
        case ACHIEVEMENT_CRITERIA_TYPE_EARN_GUILD_ACHIEVEMENT_POINTS:
            if (!miscValue1)
                return false;
            break;
        case ACHIEVEMENT_CRITERIA_TYPE_BUY_BANK_SLOT:
        case ACHIEVEMENT_CRITERIA_TYPE_COMPLETE_DAILY_QUEST_DAILY:
        case ACHIEVEMENT_CRITERIA_TYPE_COMPLETE_QUEST_COUNT:
        case ACHIEVEMENT_CRITERIA_TYPE_EARN_ACHIEVEMENT_POINTS:
        case ACHIEVEMENT_CRITERIA_TYPE_GAIN_EXALTED_REPUTATION:
        case ACHIEVEMENT_CRITERIA_TYPE_GAIN_HONORED_REPUTATION:
        case ACHIEVEMENT_CRITERIA_TYPE_GAIN_REVERED_REPUTATION:
        case ACHIEVEMENT_CRITERIA_TYPE_HIGHEST_GOLD_VALUE_OWNED:
        case ACHIEVEMENT_CRITERIA_TYPE_HIGHEST_PERSONAL_RATING:
        case ACHIEVEMENT_CRITERIA_TYPE_HIGHEST_TEAM_RATING:
        case ACHIEVEMENT_CRITERIA_TYPE_KNOWN_FACTIONS:
        case ACHIEVEMENT_CRITERIA_TYPE_REACH_LEVEL:
        case ACHIEVEMENT_CRITERIA_TYPE_BUY_GUILD_TABARD:
        case ACHIEVEMENT_CRITERIA_TYPE_COMPLETE_QUESTS_GUILD:
        case ACHIEVEMENT_CRITERIA_TYPE_HONORABLE_KILLS_GUILD:
        case ACHIEVEMENT_CRITERIA_TYPE_KILL_CREATURE_TYPE_GUILD:
        case ACHIEVEMENT_CRITERIA_TYPE_ON_LOGIN:
        case ACHIEVEMENT_CRITERIA_TYPE_COMPLETE_GUILD_CHALLENGE:
            break;
        case ACHIEVEMENT_CRITERIA_TYPE_COMPLETE_ACHIEVEMENT:
            if ((miscValue1 && achievementCriteria->Asset.AchievementID != miscValue1) || (!miscValue1 && m_completedAchievements.find(achievementCriteria->Asset.AchievementID) == m_completedAchievements.end()))
                return false;
            break;
        case ACHIEVEMENT_CRITERIA_TYPE_CRAFT_ITEMS_GUILD:
            if (!miscValue1 || !miscValue2)
                return false;
            break;
        case ACHIEVEMENT_CRITERIA_TYPE_WIN_BG:
            if (!miscValue1 || achievementCriteria->Asset.MapID != referencePlayer->GetMapId())
                return false;
            break;
        case ACHIEVEMENT_CRITERIA_TYPE_KILL_CREATURE:
            if (!miscValue1 || achievementCriteria->Asset.CreatureID != miscValue1)
                return false;
            break;
        case ACHIEVEMENT_CRITERIA_TYPE_REACH_SKILL_LEVEL:
            // update at loading or specific skill update
            if (miscValue1 && miscValue1 != achievementCriteria->Asset.SkillID)
                return false;
            break;
        case ACHIEVEMENT_CRITERIA_TYPE_LEARN_SKILL_LEVEL:
            // update at loading or specific skill update
            if (miscValue1 && miscValue1 != achievementCriteria->Asset.SkillID)
                return false;
            break;
        case ACHIEVEMENT_CRITERIA_TYPE_COMPLETE_QUESTS_IN_ZONE:
            if (miscValue1 && miscValue1 != achievementCriteria->Asset.ZoneID)
                return false;
            break;
        case ACHIEVEMENT_CRITERIA_TYPE_COMPLETE_BATTLEGROUND:
            if (!miscValue1 || referencePlayer->GetMapId() != achievementCriteria->Asset.MapID)
                return false;
            break;
        case ACHIEVEMENT_CRITERIA_TYPE_DEATH_AT_MAP:
            if (!miscValue1 || referencePlayer->GetMapId() != achievementCriteria->Asset.MapID)
                return false;
            break;
        case ACHIEVEMENT_CRITERIA_TYPE_DEATH:
        {
            if (!miscValue1)
                return false;
            // skip wrong arena achievements, if not achievIdByArenaSlot then normal total death counter
            bool notfit = false;
            for (int j = 0; j < MAX_ARENA_SLOT; ++j)
            {
                if (achievIdByArenaSlot[j] == achievementCriteria->AchievementID)
                {
                    Battleground* bg = referencePlayer->GetBattleground();
                    if (!bg || !bg->isArena() || ArenaTeam::GetSlotByType(bg->GetArenaType()) != j)
                        notfit = true;
                    break;
                }
            }
            if (notfit)
                return false;
            break;
        }
        case ACHIEVEMENT_CRITERIA_TYPE_DEATH_IN_DUNGEON:
        {
            if (!miscValue1)
                return false;

            Map const* map = referencePlayer->IsInWorld() ? referencePlayer->GetMap() : sMapMgr->FindMap(referencePlayer->GetMapId(), referencePlayer->GetInstanceId());
            if (!map || !map->IsDungeon())
                return false;

            // search case
            bool found = false;
            for (int j = 0; achievIdForDungeon[j][0]; ++j)
            {
                if (achievIdForDungeon[j][0] == achievementCriteria->AchievementID)
                {
                    if (map->IsRaid())
                    {
                        // if raid accepted (ignore difficulty)
                        if (!achievIdForDungeon[j][2])
                            break;                      // for
                    }
                    else if (referencePlayer->GetDungeonDifficulty() == DUNGEON_DIFFICULTY_NORMAL)
                    {
                        // dungeon in normal mode accepted
                        if (!achievIdForDungeon[j][1])
                            break;                      // for
                    }
                    else
                    {
                        // dungeon in heroic mode accepted
                        if (!achievIdForDungeon[j][3])
                            break;                      // for
                    }

                    found = true;
                    break;                              // for
                }
            }
            if (!found)
                return false;

            //FIXME: work only for instances where max == min for players
            if (((InstanceMap*)map)->GetMaxPlayers() != achievementCriteria->Asset.GroupSize)
                return false;
            break;
        }
        case ACHIEVEMENT_CRITERIA_TYPE_KILLED_BY_CREATURE:
            if (!miscValue1 || miscValue1 != achievementCriteria->Asset.CreatureID)
                return false;
            break;
        case ACHIEVEMENT_CRITERIA_TYPE_KILLED_BY_PLAYER:
            if (!miscValue1 || !ref || ref->GetTypeId() != TYPEID_PLAYER)
                return false;
            break;
        case ACHIEVEMENT_CRITERIA_TYPE_DEATHS_FROM:
            if (!miscValue1 || miscValue2 != achievementCriteria->Asset.DamageType)
                return false;
            break;
        case ACHIEVEMENT_CRITERIA_TYPE_COMPLETE_QUEST:
        {
            // if miscValues != 0, it contains the questID.
            if (miscValue1)
            {
                if (miscValue1 != achievementCriteria->Asset.QuestID)
                    return false;
            }
            else
            {
                // login case.
                if (!referencePlayer->GetQuestRewardStatus(achievementCriteria->Asset.QuestID))
                    return false;
            }

            if (AchievementCriteriaDataSet const* data = sAchievementMgr->GetCriteriaDataSet(achievementCriteria))
                if (!data->Meets(referencePlayer, ref))
                    return false;
            break;
        }
        case ACHIEVEMENT_CRITERIA_TYPE_BE_SPELL_TARGET:
        case ACHIEVEMENT_CRITERIA_TYPE_BE_SPELL_TARGET2:
        case ACHIEVEMENT_CRITERIA_TYPE_LAND_TARGETED_SPELL_ON_SPELL_TARGET:
            if (!miscValue1 || miscValue1 != achievementCriteria->Asset.SpellID)
                return false;
            break;
        case ACHIEVEMENT_CRITERIA_TYPE_CAST_SPELL:
            if (!miscValue1 || miscValue1 != achievementCriteria->Asset.SpellID)
                return false;
            break;
        case ACHIEVEMENT_CRITERIA_TYPE_LEARN_SPELL:
            if (miscValue1 && miscValue1 != achievementCriteria->Asset.SpellID)
                return false;

            if (!referencePlayer->HasSpell(achievementCriteria->Asset.SpellID))
                return false;
            break;
        case ACHIEVEMENT_CRITERIA_TYPE_LOOT_TYPE:
            // miscValue1 = itemId - miscValue2 = count of item loot
            // miscValue3 = loot_type (note: 0 = LOOT_CORPSE and then it ignored)
            if (!miscValue1 || !miscValue2 || !miscValue3 || miscValue3 != achievementCriteria->Asset.LootType)
                return false;
            break;
        case ACHIEVEMENT_CRITERIA_TYPE_OWN_ITEM:
            if (miscValue1 && achievementCriteria->Asset.ItemID != miscValue1)
                return false;
            break;
        case ACHIEVEMENT_CRITERIA_TYPE_USE_ITEM:
            if (!miscValue1 || achievementCriteria->Asset.ItemID != miscValue1)
                return false;
            break;
        case ACHIEVEMENT_CRITERIA_TYPE_LOOT_ITEM:
            if (!miscValue1 || miscValue1 != achievementCriteria->Asset.ItemID)
                return false;
            break;
        case ACHIEVEMENT_CRITERIA_TYPE_EXPLORE_AREA:
        {
            WorldMapOverlayEntry const* worldOverlayEntry = sWorldMapOverlayStore.LookupEntry(achievementCriteria->Asset.WorldMapOverlayID);
            if (!worldOverlayEntry)
                break;

            bool matchFound = false;
            for (int j = 0; j < MAX_WORLD_MAP_OVERLAY_AREA_IDX; ++j)
            {
                AreaTableEntry const* area = sAreaTableStore.LookupEntry(worldOverlayEntry->AreaID[j]);
                if (!area)
                    break;

                uint32 playerIndexOffset = uint32(area->AreaBit) / 32;
                if (playerIndexOffset >= PLAYER_EXPLORED_ZONES_SIZE)
                    continue;

                uint32 mask = 1 << (uint32(area->AreaBit) % 32);
                if (referencePlayer->GetUInt32Value(PLAYER_EXPLORED_ZONES_1 + playerIndexOffset) & mask)
                {
                    matchFound = true;
                    break;
                }
            }

            if (!matchFound)
                return false;
            break;
        }
        case ACHIEVEMENT_CRITERIA_TYPE_GAIN_REPUTATION:
            if (miscValue1 && miscValue1 != achievementCriteria->Asset.FactionID)
                return false;
            break;
        case ACHIEVEMENT_CRITERIA_TYPE_COMPLETE_GUILD_CHALLENGE_TYPE:
            if (!miscValue1 || miscValue1 != achievementCriteria->Asset.GuildChallengeType)
                return false;
            break;
        case ACHIEVEMENT_CRITERIA_TYPE_EQUIP_EPIC_ITEM:
            // miscValue1 = itemSlot miscValue2 = itemid
            if (!miscValue2 || miscValue1 != achievementCriteria->Asset.ItemSlot)
                return false;
            break;
        case ACHIEVEMENT_CRITERIA_TYPE_ROLL_NEED_ON_LOOT:
        case ACHIEVEMENT_CRITERIA_TYPE_ROLL_GREED_ON_LOOT:
        {
            // miscValue1 = itemid miscValue2 = diced value
            if (!miscValue1 || miscValue2 != achievementCriteria->Asset.RollValue)
                return false;

            ItemTemplate const* proto = sObjectMgr->GetItemTemplate(uint32(miscValue1));
            if (!proto)
                return false;
            break;
        }
        case ACHIEVEMENT_CRITERIA_TYPE_DO_EMOTE:
            if (!miscValue1 || miscValue1 != achievementCriteria->Asset.EmoteID)
                return false;
            break;
        case ACHIEVEMENT_CRITERIA_TYPE_DAMAGE_DONE:
        case ACHIEVEMENT_CRITERIA_TYPE_HEALING_DONE:
            if (!miscValue1)
                return false;

            if (AchievementCriteriaFailEvent(achievementCriteria->FailEvent) == AchievementCriteriaFailEvent::LeaveBattleground)
            {
                if (!referencePlayer->InBattleground())
                    return false;

                // map specific case (BG in fact) expected player targeted damage/heal
                if (!ref || ref->GetTypeId() != TYPEID_PLAYER)
                    return false;
            }
            break;
        case ACHIEVEMENT_CRITERIA_TYPE_EQUIP_ITEM:
            // miscValue1 = item_id
            if (!miscValue1 || miscValue1 != achievementCriteria->Asset.ItemID)
                return false;
            break;
        case ACHIEVEMENT_CRITERIA_TYPE_USE_GAMEOBJECT:
            if (!miscValue1 || miscValue1 != achievementCriteria->Asset.GameObjectID)
                return false;
            break;
        case ACHIEVEMENT_CRITERIA_TYPE_FISH_IN_GAMEOBJECT:
            if (!miscValue1 || miscValue1 != achievementCriteria->Asset.GameObjectID)
                return false;
            break;
        case ACHIEVEMENT_CRITERIA_TYPE_LEARN_SKILLLINE_SPELLS:
            if (miscValue1 && miscValue1 != achievementCriteria->Asset.SkillID)
                return false;
            break;
        case ACHIEVEMENT_CRITERIA_TYPE_LOOT_EPIC_ITEM:
        case ACHIEVEMENT_CRITERIA_TYPE_RECEIVE_EPIC_ITEM:
        {
            if (!miscValue1)
                return false;
            break;
        }
        case ACHIEVEMENT_CRITERIA_TYPE_LEARN_SKILL_LINE:
            if (miscValue1 && miscValue1 != achievementCriteria->Asset.SkillID)
                return false;
            break;
        case ACHIEVEMENT_CRITERIA_TYPE_HK_CLASS:
            if (!miscValue1 || miscValue1 != achievementCriteria->Asset.ClassID)
                return false;
            break;
        case ACHIEVEMENT_CRITERIA_TYPE_HK_RACE:
            if (!miscValue1 || miscValue1 != achievementCriteria->Asset.RaceID)
                return false;
            break;
        case ACHIEVEMENT_CRITERIA_TYPE_BG_OBJECTIVE_CAPTURE:
            if (!miscValue1 || miscValue1 != achievementCriteria->Asset.ObjectiveId)
                return false;
            break;
        case ACHIEVEMENT_CRITERIA_TYPE_HONORABLE_KILL_AT_AREA:
            if (!miscValue1 || miscValue1 != achievementCriteria->Asset.AreaID)
                return false;
            break;
        case ACHIEVEMENT_CRITERIA_TYPE_CURRENCY:
            if (!miscValue1 || !miscValue2 || int64(miscValue2) < 0
                || miscValue1 != achievementCriteria->Asset.CurrencyID)
                return false;
            break;
        case ACHIEVEMENT_CRITERIA_TYPE_WIN_ARENA:
            if (miscValue1 != achievementCriteria->Asset.MapID)
                return false;
            break;
        case ACHIEVEMENT_CRITERIA_TYPE_CATCH_FROM_POOL:
        {
            if (!go || go->GetGoType() != GAMEOBJECT_TYPE_FISHINGHOLE)
                return false;
            ItemTemplate const* proto = sObjectMgr->GetItemTemplate(uint32(miscValue1));
            if (!proto || proto->GetClass() == ITEM_CLASS_QUEST)
                return false;
            break;
        }
        default:
            break;
    }
    return true;
}

template<class T>
bool AchievementMgr<T>::AdditionalRequirementsSatisfied(AchievementCriteriaEntry const* criteria, uint64 miscValue1, uint64 miscValue2, WorldObject const* ref, Player* referencePlayer) const
{
    for (uint8 i = 0; i < MAX_ADDITIONAL_CRITERIA_CONDITIONS; ++i)
    {
        uint32 reqType = criteria->AdditionalConditionType[i];
        uint32 reqValue = criteria->AdditionalConditionValue[i];

        switch (AchievementCriteriaAdditionalCondition(reqType))
        {
            case ACHIEVEMENT_CRITERIA_ADDITIONAL_CONDITION_SOURCE_DRUNK_VALUE: // 1
            {
                if (referencePlayer->GetDrunkValue() < reqValue)
                    return false;
                break;
            }
            case ACHIEVEMENT_CRITERIA_ADDITIONAL_CONDITION_SOURCE_PLAYER_CONDITION: // 2
            {
                PlayerConditionEntry const* playerCondition = sPlayerConditionStore.LookupEntry(reqValue);
                if (!playerCondition || !ConditionMgr::IsPlayerMeetingCondition(referencePlayer, playerCondition))
                    return false;
                break;
            }
            case ACHIEVEMENT_CRITERIA_ADDITIONAL_CONDITION_ITEM_LEVEL: // 3
            {
                uint64 itemId = (criteria->Type == ACHIEVEMENT_CRITERIA_TYPE_EQUIP_EPIC_ITEM ? miscValue2 : miscValue1);
                ItemTemplate const* const item = sObjectMgr->GetItemTemplate(uint32(itemId));
                if (!item || item->GetBaseItemLevel() < reqValue)
                    return false;
                break;
            }
            case ACHIEVEMENT_CRITERIA_ADDITIONAL_CONDITION_TARGET_CREATURE_ENTRY: // 4
                if (!ref || ref->GetEntry() != reqValue)
                    return false;
                break;
            case ACHIEVEMENT_CRITERIA_ADDITIONAL_CONDITION_TARGET_MUST_BE_PLAYER: // 5
                if (!ref || ref->GetTypeId() != TYPEID_PLAYER)
                    return false;
                break;
            case ACHIEVEMENT_CRITERIA_ADDITIONAL_CONDITION_TARGET_MUST_BE_DEAD: // 6
                if (!ref || !ref->IsUnit() || ref->ToUnit()->IsAlive())
                    return false;
                break;
            case ACHIEVEMENT_CRITERIA_ADDITIONAL_CONDITION_TARGET_MUST_BE_ENEMY: // 7
                if (!ref || !referencePlayer->IsHostileTo(ref))
                    return false;
                break;
            case ACHIEVEMENT_CRITERIA_ADDITIONAL_CONDITION_SOURCE_HAS_AURA: // 8
                if (!referencePlayer->HasAura(reqValue))
                    return false;
                break;
            case ACHIEVEMENT_CRITERIA_ADDITIONAL_CONDITION_SOURCE_HAS_AURA_TYPE: // 9
                if (!referencePlayer->HasAuraType(AuraType(reqValue)))
                    return false;
                break;
            case ACHIEVEMENT_CRITERIA_ADDITIONAL_CONDITION_TARGET_HAS_AURA: // 10
                if (!ref || !ref->IsUnit() || !ref->ToUnit()->HasAura(reqValue))
                    return false;
                break;
            case ACHIEVEMENT_CRITERIA_ADDITIONAL_CONDITION_TARGET_HAS_AURA_TYPE: // 11
                if (!ref || !ref->IsUnit() || !ref->ToUnit()->HasAuraType(AuraType(reqValue)))
                    return false;
                break;
            case ACHIEVEMENT_CRITERIA_ADDITIONAL_CONDITION_SOURCE_AURA_STATE: // 12
                if (!referencePlayer->HasAuraState(AuraStateType(reqValue)))
                    return false;
                break;
            case ACHIEVEMENT_CRITERIA_ADDITIONAL_CONDITION_TARGET_AURA_STATE: // 13
                if (!ref || !ref->IsUnit() || !ref->ToUnit()->HasAuraState(AuraStateType(reqValue)))
                    return false;
                break;
            case ACHIEVEMENT_CRITERIA_ADDITIONAL_CONDITION_ITEM_QUALITY_MIN: // 14
            {
                uint64 itemId = (criteria->Type == ACHIEVEMENT_CRITERIA_TYPE_EQUIP_EPIC_ITEM ? miscValue2 : miscValue1);
                ItemTemplate const* const item = sObjectMgr->GetItemTemplate(uint32(itemId));
                if (!item || item->GetQuality() < reqValue)
                    return false;
                break;
            }
            case ACHIEVEMENT_CRITERIA_ADDITIONAL_CONDITION_ITEM_QUALITY_EQUALS: // 15
            {
                uint64 itemId = (criteria->Type == ACHIEVEMENT_CRITERIA_TYPE_EQUIP_EPIC_ITEM ? miscValue2 : miscValue1);
                ItemTemplate const* const item = sObjectMgr->GetItemTemplate(uint32(itemId));
                if (!item || item->GetQuality() != reqValue)
                    return false;
                break;
            }
            case ACHIEVEMENT_CRITERIA_ADDITIONAL_CONDITION_SOURCE_IS_ALIVE: // 16
            {
                if (!referencePlayer->IsAlive())
                    return false;
                break;
            }
            case ACHIEVEMENT_CRITERIA_ADDITIONAL_CONDITION_SOURCE_AREA_OR_ZONE: // 17
            {
                uint32 zoneId, areaId;
                referencePlayer->GetZoneAndAreaId(zoneId, areaId);
                if (zoneId != reqValue && areaId != reqValue)
                    return false;
                break;
            }
            case ACHIEVEMENT_CRITERIA_ADDITIONAL_CONDITION_TARGET_AREA_OR_ZONE: // 18
            {
                if (!ref)
                    return false;
                uint32 zoneId, areaId;
                ref->GetZoneAndAreaId(zoneId, areaId);
                if (zoneId != reqValue && areaId != reqValue)
                    return false;
                break;
            }
            case ACHIEVEMENT_CRITERIA_ADDITIONAL_CONDITION_MAP_DIFFICULTY: // 20
                if (uint32(referencePlayer->GetMap()->GetDifficulty()) != reqValue)
                    return false;
                break;
            case ACHIEVEMENT_CRITERIA_ADDITIONAL_CONDITION_SOURCE_RACE: // 25
                if (referencePlayer->getRace() != reqValue)
                    return false;
                break;
            case ACHIEVEMENT_CRITERIA_ADDITIONAL_CONDITION_SOURCE_CLASS: // 26
                if (referencePlayer->getClass() != reqValue)
                    return false;
                break;
            case ACHIEVEMENT_CRITERIA_ADDITIONAL_CONDITION_TARGET_RACE: // 27
                if (!ref || !ref->IsUnit() || ref->ToUnit()->getRace() != reqValue)
                    return false;
                break;
            case ACHIEVEMENT_CRITERIA_ADDITIONAL_CONDITION_TARGET_CLASS: // 28
                if (!ref || !ref->IsUnit() || ref->ToUnit()->getClass() != reqValue)
                    return false;
                break;
            case ACHIEVEMENT_CRITERIA_ADDITIONAL_CONDITION_MAX_GROUP_MEMBERS: // 29
                if (referencePlayer->GetGroup() && referencePlayer->GetGroup()->GetMembersCount() >= reqValue)
                    return false;
                break;
            case ACHIEVEMENT_CRITERIA_ADDITIONAL_CONDITION_TARGET_CREATURE_TYPE: // 30
            {
                if (!ref)
                    return false;
                if (!ref->IsUnit() || ref->ToUnit()->GetCreatureType() != reqValue)
                    return false;
                break;
            }
            case ACHIEVEMENT_CRITERIA_ADDITIONAL_CONDITION_SOURCE_MAP: // 32
                if (referencePlayer->GetMapId() != reqValue)
                    return false;
                break;
            case ACHIEVEMENT_CRITERIA_ADDITIONAL_CONDITION_ITEM_CLASS: // 33
            {
                uint64 itemId = (criteria->Type == ACHIEVEMENT_CRITERIA_TYPE_EQUIP_EPIC_ITEM ? miscValue2 : miscValue1);
                ItemTemplate const* const item = sObjectMgr->GetItemTemplate(uint32(itemId));
                if (!item || item->GetClass() != reqValue)
                    return false;
                break;
            }
            case ACHIEVEMENT_CRITERIA_ADDITIONAL_CONDITION_ITEM_SUBCLASS: // 34
            {
                uint64 itemId = (criteria->Type == ACHIEVEMENT_CRITERIA_TYPE_EQUIP_EPIC_ITEM ? miscValue2 : miscValue1);
                ItemTemplate const* const item = sObjectMgr->GetItemTemplate(uint32(itemId));
                if (!item || item->GetSubClass() != reqValue)
                    return false;
                break;
            }
            case ACHIEVEMENT_CRITERIA_ADDITIONAL_CONDITION_COMPLETE_QUEST_NOT_IN_GROUP: // 35
            {
                if (referencePlayer->GetGroup())
                    return false;
                break;
            }
            case ACHIEVEMENT_CRITERIA_ADDITIONAL_CONDITION_TITLE_BIT_INDEX: // 38
                // miscValue1 is title's bit index
                if (miscValue1 != reqValue)
                    return false;
                break;
            case ACHIEVEMENT_CRITERIA_ADDITIONAL_CONDITION_SOURCE_LEVEL: // 39
                if (referencePlayer->getLevel() != reqValue)
                    return false;
                break;
            case ACHIEVEMENT_CRITERIA_ADDITIONAL_CONDITION_TARGET_LEVEL: // 40
                if (!ref || !ref->IsUnit() || ref->ToUnit()->getLevel() != reqValue)
                    return false;
                break;
            case ACHIEVEMENT_CRITERIA_ADDITIONAL_CONDITION_TARGET_ZONE: // 41
                if (!ref || ref->GetZoneId() != reqValue)
                    return false;
                break;
            case ACHIEVEMENT_CRITERIA_ADDITIONAL_CONDITION_TARGET_HEALTH_PERCENT_BELOW: // 46
                if (!ref || !ref->IsUnit() || ref->ToUnit()->GetHealthPct() >= reqValue)
                    return false;
                break;
            case ACHIEVEMENT_CRITERIA_ADDITIONAL_CONDITION_TARGET_PLAYER_CONDITION: // 55
            {
                if (!ref || !ref->IsPlayer())
                    return false;

                PlayerConditionEntry const* playerCondition = sPlayerConditionStore.LookupEntry(reqValue);
                if (!playerCondition || !ConditionMgr::IsPlayerMeetingCondition(ref->ToPlayer(), playerCondition))
                    return false;
                break;
            }
            case ACHIEVEMENT_CRITERIA_ADDITIONAL_CONDITION_MIN_ACHIEVEMENT_POINTS: // 56
            {
                if (referencePlayer->GetAchievementPoints() < reqValue)
                    return false;
                break;
            }
            case ACHIEVEMENT_CRITERIA_ADDITIONAL_CONDITION_REQUIRES_GUILD_GROUP: // 61
            {
                Guild* guild = referencePlayer->GetGuild();
                Group* group = referencePlayer->GetGroup();
                if (!guild || !group || !group->IsGuildGroupFor(referencePlayer))
                    return false;
                break;
            }
            case ACHIEVEMENT_CRITERIA_ADDITIONAL_CONDITION_GUILD_REPUTATION: // 62
                if (referencePlayer->GetReputation(FACTION_GUILD) < reqValue)
                    return false;
                break;
            case ACHIEVEMENT_CRITERIA_ADDITIONAL_CONDITION_PROJECT_RARITY: // 65
            {
                if (!miscValue1)
                    return false;

                ResearchProjectEntry const* rp = sResearchProjectStore.LookupEntry(miscValue1);
                if (!rp) return false;

                if (rp->Rarity != reqValue)
                    return false;

                break;
            }
            case ACHIEVEMENT_CRITERIA_ADDITIONAL_CONDITION_PROJECT_RACE: // 66
            {
                if (!miscValue1)
                    return false;

                ResearchProjectEntry const* rp = sResearchProjectStore.LookupEntry(miscValue1);
                if (!rp) return false;

                if (rp->ResearchBranchID != reqValue)
                    return false;

                break;
            }
            default:
                break;
        }
    }
    return true;
}

char const* AchievementGlobalMgr::GetCriteriaTypeString(uint32 type)
{
    return GetCriteriaTypeString(AchievementCriteriaTypes(type));
}

char const* AchievementGlobalMgr::GetCriteriaTypeString(AchievementCriteriaTypes type)
{
    switch (type)
    {
        case ACHIEVEMENT_CRITERIA_TYPE_KILL_CREATURE:
            return "KILL_CREATURE";
        case ACHIEVEMENT_CRITERIA_TYPE_WIN_BG:
            return "TYPE_WIN_BG";
        case ACHIEVEMENT_CRITERIA_TYPE_COMPLETE_ARCHAEOLOGY_PROJECTS:
            return "COMPLETE_RESEARCH";
        case ACHIEVEMENT_CRITERIA_TYPE_REACH_LEVEL:
            return "REACH_LEVEL";
        case ACHIEVEMENT_CRITERIA_TYPE_REACH_SKILL_LEVEL:
            return "REACH_SKILL_LEVEL";
        case ACHIEVEMENT_CRITERIA_TYPE_COMPLETE_ACHIEVEMENT:
            return "COMPLETE_ACHIEVEMENT";
        case ACHIEVEMENT_CRITERIA_TYPE_COMPLETE_QUEST_COUNT:
            return "COMPLETE_QUEST_COUNT";
        case ACHIEVEMENT_CRITERIA_TYPE_COMPLETE_DAILY_QUEST_DAILY:
            return "COMPLETE_DAILY_QUEST_DAILY";
        case ACHIEVEMENT_CRITERIA_TYPE_COMPLETE_QUESTS_IN_ZONE:
            return "COMPLETE_QUESTS_IN_ZONE";
        case ACHIEVEMENT_CRITERIA_TYPE_CURRENCY:
            return "CURRENCY";
        case ACHIEVEMENT_CRITERIA_TYPE_DAMAGE_DONE:
            return "DAMAGE_DONE";
        case ACHIEVEMENT_CRITERIA_TYPE_COMPLETE_DAILY_QUEST:
            return "COMPLETE_DAILY_QUEST";
        case ACHIEVEMENT_CRITERIA_TYPE_COMPLETE_BATTLEGROUND:
            return "COMPLETE_BATTLEGROUND";
        case ACHIEVEMENT_CRITERIA_TYPE_DEATH_AT_MAP:
            return "DEATH_AT_MAP";
        case ACHIEVEMENT_CRITERIA_TYPE_DEATH:
            return "DEATH";
        case ACHIEVEMENT_CRITERIA_TYPE_DEATH_IN_DUNGEON:
            return "DEATH_IN_DUNGEON";
        case ACHIEVEMENT_CRITERIA_TYPE_COMPLETE_RAID:
            return "COMPLETE_RAID";
        case ACHIEVEMENT_CRITERIA_TYPE_KILLED_BY_CREATURE:
            return "KILLED_BY_CREATURE";
        case ACHIEVEMENT_CRITERIA_TYPE_KILLED_BY_PLAYER:
            return "KILLED_BY_PLAYER";
        case ACHIEVEMENT_CRITERIA_TYPE_FALL_WITHOUT_DYING:
            return "FALL_WITHOUT_DYING";
        case ACHIEVEMENT_CRITERIA_TYPE_DEATHS_FROM:
            return "DEATHS_FROM";
        case ACHIEVEMENT_CRITERIA_TYPE_COMPLETE_QUEST:
            return "COMPLETE_QUEST";
        case ACHIEVEMENT_CRITERIA_TYPE_BE_SPELL_TARGET:
            return "BE_SPELL_TARGET";
        case ACHIEVEMENT_CRITERIA_TYPE_CAST_SPELL:
            return "CAST_SPELL";
        case ACHIEVEMENT_CRITERIA_TYPE_BG_OBJECTIVE_CAPTURE:
            return "BG_OBJECTIVE_CAPTURE";
        case ACHIEVEMENT_CRITERIA_TYPE_HONORABLE_KILL_AT_AREA:
            return "HONORABLE_KILL_AT_AREA";
        case ACHIEVEMENT_CRITERIA_TYPE_WIN_ARENA:
            return "WIN_ARENA";
        case ACHIEVEMENT_CRITERIA_TYPE_PLAY_ARENA:
            return "PLAY_ARENA";
        case ACHIEVEMENT_CRITERIA_TYPE_LEARN_SPELL:
            return "LEARN_SPELL";
        case ACHIEVEMENT_CRITERIA_TYPE_HONORABLE_KILL:
            return "HONORABLE_KILL";
        case ACHIEVEMENT_CRITERIA_TYPE_OWN_ITEM:
            return "OWN_ITEM";
        case ACHIEVEMENT_CRITERIA_TYPE_WIN_RATED_ARENA:
            return "WIN_RATED_ARENA";
        case ACHIEVEMENT_CRITERIA_TYPE_HIGHEST_TEAM_RATING:
            return "HIGHEST_TEAM_RATING";
        case ACHIEVEMENT_CRITERIA_TYPE_HIGHEST_PERSONAL_RATING:
            return "HIGHEST_PERSONAL_RATING";
        case ACHIEVEMENT_CRITERIA_TYPE_LEARN_SKILL_LEVEL:
            return "LEARN_SKILL_LEVEL";
        case ACHIEVEMENT_CRITERIA_TYPE_USE_ITEM:
            return "USE_ITEM";
        case ACHIEVEMENT_CRITERIA_TYPE_LOOT_ITEM:
            return "LOOT_ITEM";
        case ACHIEVEMENT_CRITERIA_TYPE_EXPLORE_AREA:
            return "EXPLORE_AREA";
        case ACHIEVEMENT_CRITERIA_TYPE_OWN_RANK:
            return "OWN_RANK";
        case ACHIEVEMENT_CRITERIA_TYPE_BUY_BANK_SLOT:
            return "BUY_BANK_SLOT";
        case ACHIEVEMENT_CRITERIA_TYPE_GAIN_REPUTATION:
            return "GAIN_REPUTATION";
        case ACHIEVEMENT_CRITERIA_TYPE_GAIN_EXALTED_REPUTATION:
            return "GAIN_EXALTED_REPUTATION";
        case ACHIEVEMENT_CRITERIA_TYPE_VISIT_BARBER_SHOP:
            return "VISIT_BARBER_SHOP";
        case ACHIEVEMENT_CRITERIA_TYPE_EQUIP_EPIC_ITEM:
            return "EQUIP_EPIC_ITEM";
        case ACHIEVEMENT_CRITERIA_TYPE_ROLL_NEED_ON_LOOT:
            return "ROLL_NEED_ON_LOOT";
        case ACHIEVEMENT_CRITERIA_TYPE_ROLL_GREED_ON_LOOT:
            return "GREED_ON_LOOT";
        case ACHIEVEMENT_CRITERIA_TYPE_HK_CLASS:
            return "HK_CLASS";
        case ACHIEVEMENT_CRITERIA_TYPE_HK_RACE:
            return "HK_RACE";
        case ACHIEVEMENT_CRITERIA_TYPE_DO_EMOTE:
            return "DO_EMOTE";
        case ACHIEVEMENT_CRITERIA_TYPE_HEALING_DONE:
            return "HEALING_DONE";
        case ACHIEVEMENT_CRITERIA_TYPE_GET_KILLING_BLOWS:
            return "GET_KILLING_BLOWS";
        case ACHIEVEMENT_CRITERIA_TYPE_EQUIP_ITEM:
            return "EQUIP_ITEM";
        case ACHIEVEMENT_CRITERIA_TYPE_MONEY_FROM_VENDORS:
            return "MONEY_FROM_VENDORS";
        case ACHIEVEMENT_CRITERIA_TYPE_GOLD_SPENT_FOR_TALENTS:
            return "GOLD_SPENT_FOR_TALENTS";
        case ACHIEVEMENT_CRITERIA_TYPE_NUMBER_OF_TALENT_RESETS:
            return "NUMBER_OF_TALENT_RESETS";
        case ACHIEVEMENT_CRITERIA_TYPE_MONEY_FROM_QUEST_REWARD:
            return "MONEY_FROM_QUEST_REWARD";
        case ACHIEVEMENT_CRITERIA_TYPE_GOLD_SPENT_FOR_TRAVELLING:
            return "GOLD_SPENT_FOR_TRAVELLING";
        case ACHIEVEMENT_CRITERIA_TYPE_GOLD_SPENT_AT_BARBER:
            return "GOLD_SPENT_AT_BARBER";
        case ACHIEVEMENT_CRITERIA_TYPE_GOLD_SPENT_FOR_MAIL:
            return "GOLD_SPENT_FOR_MAIL";
        case ACHIEVEMENT_CRITERIA_TYPE_LOOT_MONEY:
            return "LOOT_MONEY";
        case ACHIEVEMENT_CRITERIA_TYPE_USE_GAMEOBJECT:
            return "USE_GAMEOBJECT";
        case ACHIEVEMENT_CRITERIA_TYPE_BE_SPELL_TARGET2:
            return "BE_SPELL_TARGET2";
        case ACHIEVEMENT_CRITERIA_TYPE_SPECIAL_PVP_KILL:
            return "SPECIAL_PVP_KILL";
        case ACHIEVEMENT_CRITERIA_TYPE_FISH_IN_GAMEOBJECT:
            return "FISH_IN_GAMEOBJECT";
        case ACHIEVEMENT_CRITERIA_TYPE_ON_LOGIN:
            return "ON_LOGIN";
        case ACHIEVEMENT_CRITERIA_TYPE_LEARN_SKILLLINE_SPELLS:
            return "LEARN_SKILLLINE_SPELLS";
        case ACHIEVEMENT_CRITERIA_TYPE_WIN_DUEL:
            return "WIN_DUEL";
        case ACHIEVEMENT_CRITERIA_TYPE_LOSE_DUEL:
            return "LOSE_DUEL";
        case ACHIEVEMENT_CRITERIA_TYPE_KILL_CREATURE_TYPE:
            return "KILL_CREATURE_TYPE";
        case ACHIEVEMENT_CRITERIA_TYPE_GOLD_EARNED_BY_AUCTIONS:
            return "GOLD_EARNED_BY_AUCTIONS";
        case ACHIEVEMENT_CRITERIA_TYPE_CREATE_AUCTION:
            return "CREATE_AUCTION";
        case ACHIEVEMENT_CRITERIA_TYPE_HIGHEST_AUCTION_BID:
            return "HIGHEST_AUCTION_BID";
        case ACHIEVEMENT_CRITERIA_TYPE_WON_AUCTIONS:
            return "WON_AUCTIONS";
        case ACHIEVEMENT_CRITERIA_TYPE_HIGHEST_AUCTION_SOLD:
            return "HIGHEST_AUCTION_SOLD";
        case ACHIEVEMENT_CRITERIA_TYPE_HIGHEST_GOLD_VALUE_OWNED:
            return "HIGHEST_GOLD_VALUE_OWNED";
        case ACHIEVEMENT_CRITERIA_TYPE_GAIN_REVERED_REPUTATION:
            return "GAIN_REVERED_REPUTATION";
        case ACHIEVEMENT_CRITERIA_TYPE_GAIN_HONORED_REPUTATION:
            return "GAIN_HONORED_REPUTATION";
        case ACHIEVEMENT_CRITERIA_TYPE_KNOWN_FACTIONS:
            return "KNOWN_FACTIONS";
        case ACHIEVEMENT_CRITERIA_TYPE_LOOT_EPIC_ITEM:
            return "LOOT_EPIC_ITEM";
        case ACHIEVEMENT_CRITERIA_TYPE_RECEIVE_EPIC_ITEM:
            return "RECEIVE_EPIC_ITEM";
        case ACHIEVEMENT_CRITERIA_TYPE_ROLL_NEED:
            return "ROLL_NEED";
        case ACHIEVEMENT_CRITERIA_TYPE_ROLL_GREED:
            return "ROLL_GREED";
        case ACHIEVEMENT_CRITERIA_TYPE_HIGHEST_HIT_DEALT:
            return "HIT_DEALT";
        case ACHIEVEMENT_CRITERIA_TYPE_HIGHEST_HIT_RECEIVED:
            return "HIT_RECEIVED";
        case ACHIEVEMENT_CRITERIA_TYPE_TOTAL_DAMAGE_RECEIVED:
            return "TOTAL_DAMAGE_RECEIVED";
        case ACHIEVEMENT_CRITERIA_TYPE_HIGHEST_HEAL_CAST:
            return "HIGHEST_HEAL_CAST";
        case ACHIEVEMENT_CRITERIA_TYPE_TOTAL_HEALING_RECEIVED:
            return "TOTAL_HEALING_RECEIVED";
        case ACHIEVEMENT_CRITERIA_TYPE_HIGHEST_HEALING_RECEIVED:
            return "HIGHEST_HEALING_RECEIVED";
        case ACHIEVEMENT_CRITERIA_TYPE_QUEST_ABANDONED:
            return "QUEST_ABANDONED";
        case ACHIEVEMENT_CRITERIA_TYPE_FLIGHT_PATHS_TAKEN:
            return "FLIGHT_PATHS_TAKEN";
        case ACHIEVEMENT_CRITERIA_TYPE_LOOT_TYPE:
            return "LOOT_TYPE";
        case ACHIEVEMENT_CRITERIA_TYPE_LAND_TARGETED_SPELL_ON_SPELL_TARGET:
            return "LAND_TARGETED_SPELL_ON_SPELL_TARGET";
        case ACHIEVEMENT_CRITERIA_TYPE_LEARN_SKILL_LINE:
            return "LEARN_SKILL_LINE";
        case ACHIEVEMENT_CRITERIA_TYPE_EARN_HONORABLE_KILL:
            return "EARN_HONORABLE_KILL";
        case ACHIEVEMENT_CRITERIA_TYPE_ACCEPTED_SUMMONINGS:
            return "ACCEPTED_SUMMONINGS";
        case ACHIEVEMENT_CRITERIA_TYPE_EARN_ACHIEVEMENT_POINTS:
            return "EARN_ACHIEVEMENT_POINTS";
        case ACHIEVEMENT_CRITERIA_TYPE_USE_LFD_TO_GROUP_WITH_PLAYERS:
            return "USE_LFD_TO_GROUP_WITH_PLAYERS";
        case ACHIEVEMENT_CRITERIA_TYPE_SPENT_GOLD_GUILD_REPAIRS:
            return "SPENT_GOLD_GUILD_REPAIRS";
        case ACHIEVEMENT_CRITERIA_TYPE_REACH_GUILD_LEVEL:
            return "REACH_GUILD_LEVEL";
        case ACHIEVEMENT_CRITERIA_TYPE_CRAFT_ITEMS_GUILD:
            return "CRAFT_ITEMS_GUILD";
        case ACHIEVEMENT_CRITERIA_TYPE_CATCH_FROM_POOL:
            return "CATCH_FROM_POOL";
        case ACHIEVEMENT_CRITERIA_TYPE_BUY_GUILD_BANK_SLOTS:
            return "BUY_GUILD_BANK_SLOTS";
        case ACHIEVEMENT_CRITERIA_TYPE_EARN_GUILD_ACHIEVEMENT_POINTS:
            return "EARN_GUILD_ACHIEVEMENT_POINTS";
        case ACHIEVEMENT_CRITERIA_TYPE_WIN_RATED_BATTLEGROUND:
            return "WIN_RATED_BATTLEGROUND";
        case ACHIEVEMENT_CRITERIA_TYPE_REACH_BG_RATING:
            return "REACH_BG_RATING";
        case ACHIEVEMENT_CRITERIA_TYPE_BUY_GUILD_TABARD:
            return "BUY_GUILD_TABARD";
        case ACHIEVEMENT_CRITERIA_TYPE_COMPLETE_QUESTS_GUILD:
            return "COMPLETE_QUESTS_GUILD";
        case ACHIEVEMENT_CRITERIA_TYPE_HONORABLE_KILLS_GUILD:
            return "HONORABLE_KILLS_GUILD";
        case ACHIEVEMENT_CRITERIA_TYPE_KILL_CREATURE_TYPE_GUILD:
            return "KILL_CREATURE_TYPE_GUILD";
        case ACHIEVEMENT_CRITERIA_TYPE_COMPLETE_GUILD_CHALLENGE_TYPE:
            return "GUILD_CHALLENGE_TYPE";
        case ACHIEVEMENT_CRITERIA_TYPE_COMPLETE_GUILD_CHALLENGE:
            return "GUILD_CHALLENGE";
    }
    return "MISSING_TYPE";
}

template class AchievementMgr<Guild>;
template class AchievementMgr<Player>;

AchievementGlobalMgr* AchievementGlobalMgr::instance()
{
    static AchievementGlobalMgr instance;
    return &instance;
}

inline bool IsAchievementCriteriaTypeStoredByMiscValue(AchievementCriteriaTypes type)
{
    switch (type)
    {
        case ACHIEVEMENT_CRITERIA_TYPE_KILL_CREATURE:
        case ACHIEVEMENT_CRITERIA_TYPE_WIN_BG:
        case ACHIEVEMENT_CRITERIA_TYPE_REACH_SKILL_LEVEL:
        case ACHIEVEMENT_CRITERIA_TYPE_COMPLETE_ACHIEVEMENT:
        case ACHIEVEMENT_CRITERIA_TYPE_COMPLETE_QUESTS_IN_ZONE:
        case ACHIEVEMENT_CRITERIA_TYPE_COMPLETE_BATTLEGROUND:
        case ACHIEVEMENT_CRITERIA_TYPE_KILLED_BY_CREATURE:
        case ACHIEVEMENT_CRITERIA_TYPE_COMPLETE_QUEST:
        case ACHIEVEMENT_CRITERIA_TYPE_BE_SPELL_TARGET:
        case ACHIEVEMENT_CRITERIA_TYPE_CAST_SPELL:
        case ACHIEVEMENT_CRITERIA_TYPE_BG_OBJECTIVE_CAPTURE:
        case ACHIEVEMENT_CRITERIA_TYPE_HONORABLE_KILL_AT_AREA:
        case ACHIEVEMENT_CRITERIA_TYPE_LEARN_SPELL:
        case ACHIEVEMENT_CRITERIA_TYPE_OWN_ITEM:
        case ACHIEVEMENT_CRITERIA_TYPE_LEARN_SKILL_LEVEL:
        case ACHIEVEMENT_CRITERIA_TYPE_USE_ITEM:
        case ACHIEVEMENT_CRITERIA_TYPE_LOOT_ITEM:
        case ACHIEVEMENT_CRITERIA_TYPE_EXPLORE_AREA:
        case ACHIEVEMENT_CRITERIA_TYPE_GAIN_REPUTATION:
        case ACHIEVEMENT_CRITERIA_TYPE_EQUIP_EPIC_ITEM:
        case ACHIEVEMENT_CRITERIA_TYPE_HK_CLASS:
        case ACHIEVEMENT_CRITERIA_TYPE_HK_RACE:
        case ACHIEVEMENT_CRITERIA_TYPE_DO_EMOTE:
        case ACHIEVEMENT_CRITERIA_TYPE_EQUIP_ITEM:
        case ACHIEVEMENT_CRITERIA_TYPE_USE_GAMEOBJECT:
        case ACHIEVEMENT_CRITERIA_TYPE_BE_SPELL_TARGET2:
        case ACHIEVEMENT_CRITERIA_TYPE_FISH_IN_GAMEOBJECT:
        case ACHIEVEMENT_CRITERIA_TYPE_LEARN_SKILLLINE_SPELLS:
        case ACHIEVEMENT_CRITERIA_TYPE_LOOT_TYPE:
        case ACHIEVEMENT_CRITERIA_TYPE_LAND_TARGETED_SPELL_ON_SPELL_TARGET:
        case ACHIEVEMENT_CRITERIA_TYPE_LEARN_SKILL_LINE:
            return true;
        default:
            break;
    }
    return false;
}

AchievementCriteriaEntryList const& AchievementGlobalMgr::GetAchievementCriteriaByType(AchievementCriteriaTypes type, uint32 miscValue, bool guild) const
{
    if (guild)
    {
        if (miscValue && IsAchievementCriteriaTypeStoredByMiscValue(type))
        {
            auto itr = m_GuildAchievementCriteriasByMiscValue[type].find(miscValue);
            if (itr != m_GuildAchievementCriteriasByMiscValue[type].end())
                return itr->second;
        }

        return m_GuildAchievementCriteriasByType[type];
    }
    else
    {
        if (miscValue && IsAchievementCriteriaTypeStoredByMiscValue(type))
        {
            auto itr = m_AchievementCriteriasByMiscValue[type].find(miscValue);
            if (itr != m_AchievementCriteriasByMiscValue[type].end())
                return itr->second;
        }

        return m_AchievementCriteriasByType[type];
    }
}

AchievementCriteriaEntryList const* AchievementGlobalMgr::GetAchievementCriteriaByStartEvent(AchievementCriteriaStartEvent startEvent, int32 asset) const
{
    return Trinity::Containers::MapGetValuePtr(_criteriasByStartEvent[size_t(startEvent)], asset);
}

AchievementCriteriaEntryList const* AchievementGlobalMgr::GetAchievementCriteriaByFailEvent(AchievementCriteriaFailEvent startEvent, int32 asset) const
{
    return Trinity::Containers::MapGetValuePtr(_criteriasByFailEvent[size_t(startEvent)], asset);
}

bool AchievementGlobalMgr::IsRealmCompleted(AchievementEntry const* achievement) const
{
    auto itr = _allCompletedAchievements.find(achievement->ID);
    if (itr == _allCompletedAchievements.end())
        return false;

    if (itr->second == std::chrono::system_clock::time_point::min())
        return false;

    if (itr->second == std::chrono::system_clock::time_point::max())
        return true;

    // Allow completing the realm first kill for entire minute after first person did it
    // it may allow more than one group to achieve it (highly unlikely)
    // but apparently this is how blizz handles it as well
    if (achievement->Flags & ACHIEVEMENT_FLAG_REALM_FIRST_KILL)
        return (GameTime::GetGameTimeSystemPoint() - itr->second) > Minutes(1);

    return true;
}

void AchievementGlobalMgr::SetRealmCompleted(AchievementEntry const* achievement)
{
    if (IsRealmCompleted(achievement))
        return;

    _allCompletedAchievements[achievement->ID] = GameTime::GetGameTimeSystemPoint();
}

inline AchievementCriteriaStartEvent ToStartEvent(uint32 timedStartEvent)
{
    switch (static_cast<AchievementCriteriaTimerStartEvent>(timedStartEvent))
    {
        case AchievementCriteriaTimerStartEvent::SendEvent:     return AchievementCriteriaStartEvent::SendEvent;
        case AchievementCriteriaTimerStartEvent::AcceptQuest:   return AchievementCriteriaStartEvent::AcceptQuest;
        case AchievementCriteriaTimerStartEvent::CastSpell:     return AchievementCriteriaStartEvent::CastSpell;
        case AchievementCriteriaTimerStartEvent::GainAura:      return AchievementCriteriaStartEvent::GainAura;
        case AchievementCriteriaTimerStartEvent::KillNPC:       return AchievementCriteriaStartEvent::KillNPC;
        case AchievementCriteriaTimerStartEvent::UseItem:       return AchievementCriteriaStartEvent::UseItem;
        default:
            return AchievementCriteriaStartEvent::None;
    }
}

//==========================================================
void AchievementGlobalMgr::LoadAchievementCriteriaList()
{
    uint32 oldMSTime = getMSTime();

    if (sAchievementCriteriaStore.GetNumRows() == 0)
    {
        TC_LOG_ERROR("server.loading", ">> Loaded 0 achievement criteria.");
        return;
    }

    uint32 criterias = 0;
    uint32 guildCriterias = 0;
    for (AchievementCriteriaEntry const* criteria : sAchievementCriteriaStore)
    {
        ASSERT(criteria->Type < ACHIEVEMENT_CRITERIA_TYPE_TOTAL, "ACHIEVEMENT_CRITERIA_TYPE_TOTAL must be greater than or equal to %u but is currently equal to %u",
            criteria->Type + 1, ACHIEVEMENT_CRITERIA_TYPE_TOTAL);

        AchievementEntry const* achievement = sAchievementMgr->GetAchievement(criteria->AchievementID);

        m_AchievementCriteriaListByAchievement[criteria->AchievementID].push_back(criteria);

        bool guild = false;
        if (achievement && achievement->Flags & ACHIEVEMENT_FLAG_GUILD)
        {
            ++guildCriterias, m_GuildAchievementCriteriasByType[criteria->Type].push_back(criteria);
            guild = true;
        }
        else
            ++criterias, m_AchievementCriteriasByType[criteria->Type].push_back(criteria);

        if (IsAchievementCriteriaTypeStoredByMiscValue(AchievementCriteriaTypes(criteria->Type)))
        {
            if (criteria->Type != ACHIEVEMENT_CRITERIA_TYPE_EXPLORE_AREA)
                guild ? m_GuildAchievementCriteriasByMiscValue[criteria->Type][criteria->Asset.ID].push_back(criteria) : m_AchievementCriteriasByMiscValue[criteria->Type][criteria->Asset.ID].push_back(criteria);
            else
            {
                WorldMapOverlayEntry const* worldOverlayEntry = sWorldMapOverlayStore.LookupEntry(criteria->Asset.WorldMapOverlayID);
                if (!worldOverlayEntry)
                    break;

                for (uint8 j = 0; j < MAX_WORLD_MAP_OVERLAY_AREA_IDX; ++j)
                {
                    if (worldOverlayEntry->AreaID[j])
                    {
                        bool valid = true;
                        for (uint8 i = 0; i < j; ++i)
                            if (worldOverlayEntry->AreaID[j] == worldOverlayEntry->AreaID[i])
                                valid = false;
                        if (valid)
                            guild ? m_GuildAchievementCriteriasByMiscValue[criteria->Type][worldOverlayEntry->AreaID[j]].push_back(criteria) : m_AchievementCriteriasByMiscValue[criteria->Type][worldOverlayEntry->AreaID[j]].push_back(criteria);
                    }
                }
            }
        }

        if (criteria->StartEvent)
            _criteriasByStartEvent[criteria->StartEvent][criteria->StartAsset].push_back(criteria);

        // Timed criterias use a own event definition which is name-identical with StartEvent so we translate it and add the criterias to the StartEvent container
        if (criteria->TimerStartEvent)
        {
            AchievementCriteriaStartEvent startEvent = ToStartEvent(criteria->TimerStartEvent);
            if (startEvent != AchievementCriteriaStartEvent::None)
                _criteriasByStartEvent[AsUnderlyingType(startEvent)][criteria->TimerAsset].push_back(criteria);
        }

        if (criteria->FailEvent)
            _criteriasByFailEvent[criteria->FailEvent][criteria->FailAsset].push_back(criteria);
    }

    TC_LOG_INFO("server.loading", ">> Loaded %u achievement criteria and %u guild achievement crieteria in %u ms.", criterias, guildCriterias, GetMSTimeDiffToNow(oldMSTime));
}

void AchievementGlobalMgr::LoadAchievementReferenceList()
{
    uint32 oldMSTime = getMSTime();

    if (sAchievementStore.GetNumRows() == 0)
    {
        TC_LOG_INFO("server.loading", ">> Loaded 0 achievement references.");
        return;
    }

    uint32 count = 0;

    for (uint32 entryId = 0; entryId < sAchievementStore.GetNumRows(); ++entryId)
    {
        AchievementEntry const* achievement = sAchievementMgr->GetAchievement(entryId);
        if (!achievement || !achievement->SharesCriteria)
            continue;

        m_AchievementListByReferencedId[achievement->SharesCriteria].push_back(achievement);
        ++count;
    }

    // Once Bitten, Twice Shy (10 player) - Icecrown Citadel
    if (AchievementEntry const* achievement = sAchievementMgr->GetAchievement(4539))
        const_cast<AchievementEntry*>(achievement)->MapID = 631;    // Correct map requirement (currently has Ulduar)

    // Straw That Broke the Camel's Back - Halls of Origination
    if (AchievementEntry const* achievement = sAchievementMgr->GetAchievement(5294))
        const_cast<AchievementEntry*>(achievement)->MapID = 644;    // Correct map requirement

    TC_LOG_INFO("server.loading", ">> Loaded %u achievement references in %u ms.", count, GetMSTimeDiffToNow(oldMSTime));
}

void AchievementGlobalMgr::LoadAchievementCriteriaData()
{
    uint32 oldMSTime = getMSTime();

    m_criteriaDataMap.clear();                              // need for reload case

    QueryResult result = WorldDatabase.Query("SELECT criteria_id, type, value1, value2, ScriptName FROM achievement_criteria_data");

    if (!result)
    {
        TC_LOG_INFO("server.loading", ">> Loaded 0 additional achievement criteria data. DB table `achievement_criteria_data` is empty.");
        return;
    }

    uint32 count = 0;

    do
    {
        Field* fields = result->Fetch();
        uint32 criteria_id = fields[0].GetUInt32();

        AchievementCriteriaEntry const* criteria = sAchievementMgr->GetAchievementCriteria(criteria_id);

        if (!criteria)
        {
            TC_LOG_ERROR("sql.sql", "Table `achievement_criteria_data` contains data for non-existing criteria (Entry: %u). Ignored.", criteria_id);
            continue;
        }

        uint32 dataType = fields[1].GetUInt8();
        std::string scriptName = fields[4].GetString();
        uint32 scriptId = 0;
        if (scriptName.length()) // not empty
        {
            if (dataType != ACHIEVEMENT_CRITERIA_DATA_TYPE_SCRIPT)
                TC_LOG_ERROR("sql.sql", "Table `achievement_criteria_data` contains a ScriptName for non-scripted data type (Entry: %u, type %u), useless data.", criteria_id, dataType);
            else
                scriptId = sObjectMgr->GetScriptId(scriptName);
        }

        AchievementCriteriaData data(dataType, fields[2].GetUInt32(), fields[3].GetUInt32(), scriptId);

        if (!data.IsValid(criteria))
            continue;

        // this will allocate empty data set storage
        AchievementCriteriaDataSet& dataSet = m_criteriaDataMap[criteria_id];
        dataSet.SetCriteriaId(criteria_id);

        // add real data only for not NONE data types
        if (data.dataType != ACHIEVEMENT_CRITERIA_DATA_TYPE_NONE)
            dataSet.Add(data);

        // counting data by and data types
        ++count;
    }
    while (result->NextRow());

    TC_LOG_INFO("server.loading", ">> Loaded %u additional achievement criteria data in %u ms", count, GetMSTimeDiffToNow(oldMSTime));
}

void AchievementGlobalMgr::LoadCompletedAchievements()
{
    uint32 oldMSTime = getMSTime();

    // Populate _allCompletedAchievements with all realm first achievement ids to make multithreaded access safer
    // while it will not prevent races, it will prevent crashes that happen because std::unordered_map key was added
    // instead the only potential race will happen on value associated with the key
    for (uint32 i = 0; i < sAchievementStore.GetNumRows(); ++i)
        if (AchievementEntry const* achievement = sAchievementStore.LookupEntry(i))
            if (achievement->Flags & (ACHIEVEMENT_FLAG_REALM_FIRST_REACH | ACHIEVEMENT_FLAG_REALM_FIRST_KILL))
                _allCompletedAchievements[achievement->ID] = std::chrono::system_clock::time_point::min();

    QueryResult result = CharacterDatabase.Query("SELECT achievement FROM character_achievement GROUP BY achievement");

    if (!result)
    {
        TC_LOG_INFO("server.loading", ">> Loaded 0 realm first completed achievements. DB table `character_achievement` is empty.");
        return;
    }

    do
    {
        Field* fields = result->Fetch();

        uint16 achievementId = fields[0].GetUInt16();
        AchievementEntry const* achievement = sAchievementMgr->GetAchievement(achievementId);
        if (!achievement)
        {
            // Remove non-existing achievements from all characters
            TC_LOG_ERROR("achievement", "Non-existing achievement %u data has been removed from the table `character_achievement`.", achievementId);

            CharacterDatabasePreparedStatement* stmt = CharacterDatabase.GetPreparedStatement(CHAR_DEL_INVALID_ACHIEVMENT);
            stmt->setUInt16(0, uint16(achievementId));
            CharacterDatabase.Execute(stmt);

            continue;
        }
        else if (achievement->Flags & (ACHIEVEMENT_FLAG_REALM_FIRST_REACH | ACHIEVEMENT_FLAG_REALM_FIRST_KILL | ACHIEVEMENT_FLAG_REALM_FIRST_GUILD))
            _allCompletedAchievements[achievementId] = std::chrono::system_clock::time_point::max();
    } while (result->NextRow());

    TC_LOG_INFO("server.loading", ">> Loaded %lu realm first completed achievements in %u ms.", (unsigned long)_allCompletedAchievements.size(), GetMSTimeDiffToNow(oldMSTime));
}

void AchievementGlobalMgr::LoadRewards()
{
    uint32 oldMSTime = getMSTime();

    m_achievementRewards.clear();                           // need for reload case

    //                                               0   1       2       3       4       5        6     7
    QueryResult result = WorldDatabase.Query("SELECT ID, TitleA, TitleH, ItemID, Sender, Subject, Text, MailTemplateID FROM achievement_reward");


    if (!result)
    {
        TC_LOG_ERROR("server.loading", ">> Loaded 0 achievement rewards. DB table `achievement_reward` is empty.");
        return;
    }

    do
    {
        Field* fields = result->Fetch();
        uint32 id     = fields[0].GetUInt32();
        AchievementEntry const* achievement = GetAchievement(id);
        if (!achievement)
        {
            TC_LOG_ERROR("sql.sql", "Table `achievement_reward` contains a wrong achievement ID (%u), ignored.", id);
            continue;
        }

        AchievementReward reward;
        reward.TitleID[0]     = fields[1].GetUInt32();
        reward.TitleID[1]     = fields[2].GetUInt32();
        reward.ItemID         = fields[3].GetUInt32();
        reward.Sender         = fields[4].GetUInt32();
        reward.Subject        = fields[5].GetString();
        reward.Text           = fields[6].GetString();
        reward.MailTemplateID = fields[7].GetUInt32();

        // must be title or mail at least
        if (!reward.TitleID[0] && !reward.TitleID[1] && !reward.Sender)
        {
            TC_LOG_ERROR("sql.sql", "Table `achievement_reward` (ID: %u) does not contain title or item reward data. Ignored.", id);
            continue;
        }

        if (achievement->Faction == ACHIEVEMENT_FACTION_ANY && (!reward.TitleID[0] ^ !reward.TitleID[1]))
            TC_LOG_ERROR("sql.sql", "Table `achievement_reward` (ID: %u) contains the title (A: %u H: %u) for only one team.", id, reward.TitleID[0], reward.TitleID[1]);

        if (reward.TitleID[0])
        {
            CharTitlesEntry const* titleEntry = sCharTitlesStore.LookupEntry(reward.TitleID[0]);
            if (!titleEntry)
            {
                TC_LOG_ERROR("sql.sql", "Table `achievement_reward` (Entry: %u) contains an invalid title id (%u) in `title_A`, set to 0", id, reward.TitleID[0]);
                reward.TitleID[0] = 0;
            }
        }

        if (reward.TitleID[1])
        {
            CharTitlesEntry const* titleEntry = sCharTitlesStore.LookupEntry(reward.TitleID[1]);
            if (!titleEntry)
            {
                TC_LOG_ERROR("sql.sql", "Table `achievement_reward` (Entry: %u) contains an invalid title id (%u) in `title_H`, set to 0", id, reward.TitleID[1]);
                reward.TitleID[1] = 0;
            }
        }

        //check mail data before item for report including wrong item case
        if (reward.Sender)
        {
            if (!sObjectMgr->GetCreatureTemplate(reward.Sender))
            {
                TC_LOG_ERROR("sql.sql", "Table `achievement_reward` (ID: %u) contains an invalid creature ID %u as sender, mail reward skipped.", id, reward.Sender);
                reward.Sender = 0;
            }
        }
        else
        {
            if (reward.ItemID)
                TC_LOG_ERROR("sql.sql", "Table `achievement_reward` (ID: %u) does not have sender data, but contains an item reward. Item will not be rewarded.", id);

            if (!reward.Subject.empty())
                TC_LOG_ERROR("sql.sql", "Table `achievement_reward` (ID: %u) does not have sender data, but contains a mail subject.", id);

            if (!reward.Text.empty())
                TC_LOG_ERROR("sql.sql", "Table `achievement_reward` (ID: %u) does not have sender data, but contains mail text.", id);

            if (reward.MailTemplateID)
                TC_LOG_ERROR("sql.sql", "Table `achievement_reward` (ID: %u) does not have sender data, but has a MailTemplate.", id);
        }

        if (reward.MailTemplateID)
        {
            if (!sMailTemplateStore.LookupEntry(reward.MailTemplateID))
            {
                TC_LOG_ERROR("sql.sql", "Table `achievement_reward` (ID: %u) is using an invalid MailTemplate (%u).", id, reward.MailTemplateID);
                reward.MailTemplateID = 0;
            }
            else if (!reward.Subject.empty() || !reward.Text.empty())
                TC_LOG_ERROR("sql.sql", "Table `achievement_reward` (ID: %u) is using MailTemplate (%u) and mail subject/text.", id, reward.MailTemplateID);
        }

        if (reward.ItemID)
        {
            if (!sObjectMgr->GetItemTemplate(reward.ItemID))
            {
                TC_LOG_ERROR("sql.sql", "Table `achievement_reward` (ID: %u) contains an invalid item id %u, reward mail will not contain the rewarded item.", id, reward.ItemID);
                reward.ItemID = 0;
            }
        }

        m_achievementRewards[id] = reward;
    } while (result->NextRow());

    TC_LOG_INFO("server.loading", ">> Loaded %u achievement rewards in %u ms.", uint32(m_achievementRewards.size()), GetMSTimeDiffToNow(oldMSTime));
}

void AchievementGlobalMgr::LoadRewardLocales()
{
    uint32 oldMSTime = getMSTime();

    m_achievementRewardLocales.clear();                       // need for reload case

    //                                               0   1       2        3
    QueryResult result = WorldDatabase.Query("SELECT ID, Locale, Subject, Text FROM achievement_reward_locale");

    if (!result)
    {
        TC_LOG_INFO("server.loading", ">> Loaded 0 achievement reward locale strings.  DB table `achievement_reward_locale` is empty.");
        return;
    }

    do
    {
        Field* fields = result->Fetch();

        uint32 id               = fields[0].GetUInt32();
        std::string localeName  = fields[1].GetString();

        LocaleConstant locale = GetLocaleByName(localeName);
        if (locale == LOCALE_enUS)
            continue;

        if (m_achievementRewards.find(id) == m_achievementRewards.end())
        {
            TC_LOG_ERROR("sql.sql", "Table `achievement_reward_locale` (ID: %u) contains locale strings for a non-existing achievement reward.", id);
            continue;
        }

        AchievementRewardLocale& data = m_achievementRewardLocales[id];
        ObjectMgr::AddLocaleString(fields[2].GetString(), locale, data.Subject);
        ObjectMgr::AddLocaleString(fields[3].GetString(), locale, data.Text);
    } while (result->NextRow());

    TC_LOG_INFO("server.loading", ">> Loaded %u achievement reward locale strings in %u ms.", uint32(m_achievementRewardLocales.size()), GetMSTimeDiffToNow(oldMSTime));
}

AchievementEntry const* AchievementGlobalMgr::GetAchievement(uint32 achievementId) const
{
    return sAchievementStore.LookupEntry(achievementId);
}

AchievementCriteriaEntry const* AchievementGlobalMgr::GetAchievementCriteria(uint32 criteriaId) const
{
    return sAchievementCriteriaStore.LookupEntry(criteriaId);
}
