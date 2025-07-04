/* Copyright (C) 2025 Wildfire Games.
 * This file is part of 0 A.D.
 *
 * 0 A.D. is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 2 of the License, or
 * (at your option) any later version.
 *
 * 0 A.D. is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with 0 A.D.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "precompiled.h"

#include "simulation2/system/Component.h"
#include "ICmpRangeManager.h"

#include "ICmpTerrain.h"
#include "simulation2/system/EntityMap.h"
#include "simulation2/MessageTypes.h"
#include "simulation2/components/ICmpFogging.h"
#include "simulation2/components/ICmpMirage.h"
#include "simulation2/components/ICmpOwnership.h"
#include "simulation2/components/ICmpPosition.h"
#include "simulation2/components/ICmpObstructionManager.h"
#include "simulation2/components/ICmpTerritoryManager.h"
#include "simulation2/components/ICmpVisibility.h"
#include "simulation2/components/ICmpVision.h"
#include "simulation2/components/ICmpWaterManager.h"
#include "simulation2/helpers/Los.h"
#include "simulation2/helpers/MapEdgeTiles.h"
#include "simulation2/helpers/Render.h"
#include "simulation2/helpers/Spatial.h"
#include "simulation2/serialization/SerializedTypes.h"

#include "graphics/Overlay.h"
#include "lib/timer.h"
#include "ps/CLogger.h"
#include "ps/Profile.h"
#include "renderer/Scene.h"

#define DEBUG_RANGE_MANAGER_BOUNDS 0

namespace
{
/**
 * How many LOS vertices to have per region.
 * LOS regions are used to keep track of units.
 */
constexpr int LOS_REGION_RATIO = 8;

/**
 * Tolerance for parabolic range calculations.
 * TODO C++20: change this to constexpr by fixing CFixed with std::is_constant_evaluated
 */
const fixed PARABOLIC_RANGE_TOLERANCE = fixed::FromInt(1)/2;

/**
 * Convert an owner ID (-1 = unowned, 0 = gaia, 1..30 = players)
 * into a 32-bit mask for quick set-membership tests.
 */
u32 CalcOwnerMask(player_id_t owner)
{
	if (owner >= -1 && owner < 31)
		return 1 << (1+owner);
	else
		return 0; // owner was invalid
}

/**
 * Returns LOS mask for given player.
 */
u32 CalcPlayerLosMask(player_id_t player)
{
	if (player > 0 && player <= 16)
		return (u32)LosState::MASK << (2*(player-1));
	return 0;
}

/**
 * Returns shared LOS mask for given list of players.
 */
u32 CalcSharedLosMask(std::vector<player_id_t> players)
{
	u32 playerMask = 0;
	for (size_t i = 0; i < players.size(); i++)
		playerMask |= CalcPlayerLosMask(players[i]);

	return playerMask;
}

/**
 * Add/remove a player to/from mask, which is a 1-bit mask representing a list of players.
 * Returns true if the mask is modified.
 */
bool SetPlayerSharedDirtyVisibilityBit(u16& mask, player_id_t player, bool enable)
{
	if (player <= 0 || player > 16)
		return false;

	u16 oldMask = mask;

	if (enable)
		mask |= (0x1 << (player - 1));
	else
		mask &= ~(0x1 << (player - 1));

	return oldMask != mask;
}

/**
 * Computes the 2-bit visibility for one player, given the total 32-bit visibilities
 */
LosVisibility GetPlayerVisibility(u32 visibilities, player_id_t player)
{
	if (player > 0 && player <= 16)
		return static_cast<LosVisibility>( (visibilities >> (2 *(player-1))) & 0x3 );
	return LosVisibility::HIDDEN;
}

/**
 * Test whether the visibility is dirty for a given LoS region and a given player
 */
bool IsVisibilityDirty(u16 dirty, player_id_t player)
{
	if (player > 0 && player <= 16)
		return (dirty >> (player - 1)) & 0x1;
	return false;
}

/**
 * Test whether a player share this vision
 */
bool HasVisionSharing(u16 visionSharing, player_id_t player)
{
	return (visionSharing & (1 << (player - 1))) != 0;
}

/**
 * Computes the shared vision mask for the player
 */
u16 CalcVisionSharingMask(player_id_t player)
{
	return 1 << (player-1);
}

/**
 * Representation of a range query.
 */
struct Query
{
	std::vector<entity_id_t> lastMatch;
	CEntityHandle source; // TODO: this could crash if an entity is destroyed while a Query is still referencing it
	entity_pos_t minRange;
	entity_pos_t maxRange;
	entity_pos_t yOrigin; // Used for parabolas only.
	u32 ownersMask;
	i32 interface;
	u8 flagsMask;
	bool enabled;
	bool parabolic;
	bool accountForSize; // If true, the query accounts for unit sizes, otherwise it treats all entities as points.
};

/**
 * Checks whether v is in a parabolic range of (0,0,0)
 * The highest point of the paraboloid is (0,range/2,0)
 * and the circle of distance 'range' around (0,0,0) on height y=0 is part of the paraboloid
 * This equates to computing f(x, z) = y = -(xx + zz)/(2*range) + range/2 > 0,
 * or alternatively sqrt(xx+zz) <= sqrt(range^2 - 2range*y).
 *
 * Avoids sqrting and overflowing.
 */
static bool InParabolicRange(CFixedVector3D v, fixed range)
{
	u64 xx = SQUARE_U64_FIXED(v.X); // xx <= 2^62
	u64 zz = SQUARE_U64_FIXED(v.Z);
	i64 d2 = (xx + zz) >> 1; // d2 <= 2^62 (no overflow)

	i32 y = v.Y.GetInternalValue();
	i32 c = range.GetInternalValue();
	i32 c_2 = c >> 1;

	i64 c2 = MUL_I64_I32_I32(c_2 - y, c);

	return d2 <= c2;
}

struct EntityParabolicRangeOutline
{
	entity_id_t source;
	CFixedVector3D position;
	entity_pos_t range;
	std::vector<entity_pos_t> outline;
};

static std::map<entity_id_t, EntityParabolicRangeOutline> ParabolicRangesOutlines;

/**
 * Representation of an entity, with the data needed for queries.
 */
enum FlagMasks
{
	// flags used for queries
	None = 0x00,
	Normal = 0x01,
	Injured = 0x02,
	AllQuery = Normal | Injured,

	// 0x04 reserved for future use

	// general flags
	InWorld = 0x08,
	RetainInFog = 0x10,
	RevealShore = 0x20,
	ScriptedVisibility = 0x40,
	SharedVision = 0x80
};

struct EntityData
{
	EntityData() :
		visibilities(0), size(0), visionSharing(0),
		owner(-1), flags(FlagMasks::Normal)
		{ }
	entity_pos_t x, z;
	entity_pos_t visionRange;
	u32 visibilities; // 2-bit visibility, per player
	u32 size;
	u16 visionSharing; // 1-bit per player
	i8 owner;
	u8 flags; // See the FlagMasks enum

	template<int mask>
	inline bool HasFlag() const { return (flags & mask) != 0; }

	template<int mask>
	inline void SetFlag(bool val) { flags = val ? (flags | mask) : (flags & ~mask); }

	inline void SetFlag(u8 mask, bool val) { flags = val ? (flags | mask) : (flags & ~mask); }
};

static_assert(sizeof(EntityData) == 24);

/**
 * Functor for sorting entities by distance from a source point.
 * It must only be passed entities that are in 'entities'
 * and are currently in the world.
 */
class EntityDistanceOrdering
{
public:
	EntityDistanceOrdering(const EntityMap<EntityData>& entities, const CFixedVector2D& source) :
		m_EntityData(entities), m_Source(source)
	{
	}

	EntityDistanceOrdering(const EntityDistanceOrdering& entity) = default;

	bool operator()(entity_id_t a, entity_id_t b) const
	{
		const EntityData& da = m_EntityData.find(a)->second;
		const EntityData& db = m_EntityData.find(b)->second;
		CFixedVector2D vecA = CFixedVector2D(da.x, da.z) - m_Source;
		CFixedVector2D vecB = CFixedVector2D(db.x, db.z) - m_Source;
		return (vecA.CompareLength(vecB) < 0);
	}

	const EntityMap<EntityData>& m_EntityData;
	CFixedVector2D m_Source;

private:
	EntityDistanceOrdering& operator=(const EntityDistanceOrdering&);
};
} // anonymous namespace

/**
 * Serialization helper template for Query
 */
template<>
struct SerializeHelper<Query>
{
	template<typename S>
	void Common(S& serialize, const char* UNUSED(name), Serialize::qualify<S, Query> value)
	{
		serialize.NumberFixed_Unbounded("min range", value.minRange);
		serialize.NumberFixed_Unbounded("max range", value.maxRange);
		serialize.NumberFixed_Unbounded("yOrigin", value.yOrigin);
		serialize.NumberU32_Unbounded("owners mask", value.ownersMask);
		serialize.NumberI32_Unbounded("interface", value.interface);
		Serializer(serialize, "last match", value.lastMatch);
		serialize.NumberU8_Unbounded("flagsMask", value.flagsMask);
		serialize.Bool("enabled", value.enabled);
		serialize.Bool("parabolic",value.parabolic);
		serialize.Bool("account for size",value.accountForSize);
	}

	void operator()(ISerializer& serialize, const char* name, Query& value, const CSimContext&)
	{
		Common(serialize, name, value);

		uint32_t id = value.source.GetId();
		serialize.NumberU32_Unbounded("source", id);
	}

	void operator()(IDeserializer& deserialize, const char* name, Query& value, const CSimContext& context)
	{
		Common(deserialize, name, value);

		uint32_t id;
		deserialize.NumberU32_Unbounded("source", id);
		value.source = context.GetComponentManager().LookupEntityHandle(id, true);
		// the referenced entity might not have been deserialized yet,
		// so tell LookupEntityHandle to allocate the handle if necessary
	}
};

/**
 * Serialization helper template for EntityData
 */
template<>
struct SerializeHelper<EntityData>
{
	template<typename S>
	void operator()(S& serialize, const char* UNUSED(name), Serialize::qualify<S, EntityData> value)
	{
		serialize.NumberFixed_Unbounded("x", value.x);
		serialize.NumberFixed_Unbounded("z", value.z);
		serialize.NumberFixed_Unbounded("vision", value.visionRange);
		serialize.NumberU32_Unbounded("visibilities", value.visibilities);
		serialize.NumberU32_Unbounded("size", value.size);
		serialize.NumberU16_Unbounded("vision sharing", value.visionSharing);
		serialize.NumberI8_Unbounded("owner", value.owner);
		serialize.NumberU8_Unbounded("flags", value.flags);
	}
};

/**
 * Range manager implementation.
 * Maintains a list of all entities (and their positions and owners), which is used for
 * queries.
 *
 * LOS implementation is based on the model described in GPG2.
 * (TODO: would be nice to make it cleverer, so e.g. mountains and walls
 * can block vision)
 */
class CCmpRangeManager final : public ICmpRangeManager
{
public:
	static void ClassInit(CComponentManager& componentManager)
	{
		componentManager.SubscribeGloballyToMessageType(MT_Create);
		componentManager.SubscribeGloballyToMessageType(MT_PositionChanged);
		componentManager.SubscribeGloballyToMessageType(MT_OwnershipChanged);
		componentManager.SubscribeGloballyToMessageType(MT_Destroy);
		componentManager.SubscribeGloballyToMessageType(MT_VisionRangeChanged);
		componentManager.SubscribeGloballyToMessageType(MT_VisionSharingChanged);

		componentManager.SubscribeToMessageType(MT_Deserialized);
		componentManager.SubscribeToMessageType(MT_Update);
		componentManager.SubscribeToMessageType(MT_RenderSubmit); // for debug overlays
	}

	DEFAULT_COMPONENT_ALLOCATOR(RangeManager)

	bool m_DebugOverlayEnabled;
	bool m_DebugOverlayDirty;
	std::vector<SOverlayLine> m_DebugOverlayLines;

	// Deserialization flag. A lot of different functions are called by Deserialize()
	// and we don't want to pass isDeserializing bool arguments to all of them...
	bool m_Deserializing;

	// World bounds (entities are expected to be within this range)
	entity_pos_t m_WorldX0;
	entity_pos_t m_WorldZ0;
	entity_pos_t m_WorldX1;
	entity_pos_t m_WorldZ1;

	// Range query state:
	tag_t m_QueryNext; // next allocated id
	std::map<tag_t, Query> m_Queries;
	EntityMap<EntityData> m_EntityData;

	FastSpatialSubdivision m_Subdivision; // spatial index of m_EntityData
	std::vector<entity_id_t> m_SubdivisionResults;

	// LOS state:
	static const player_id_t MAX_LOS_PLAYER_ID = 16;

	using LosRegion = std::pair<u16, u16>;

	std::array<bool, MAX_LOS_PLAYER_ID+2> m_LosRevealAll;
	bool m_LosCircular;
	i32 m_LosVerticesPerSide;

	// Cache for visibility tracking
	i32 m_LosRegionsPerSide;
	bool m_GlobalVisibilityUpdate;
	std::array<bool, MAX_LOS_PLAYER_ID> m_GlobalPlayerVisibilityUpdate;
	Grid<u16> m_DirtyVisibility;
	Grid<std::set<entity_id_t>> m_LosRegions;
	// List of entities that must be updated, regardless of the status of their tile
	std::vector<entity_id_t> m_ModifiedEntities;

	// Counts of units seeing vertex, per vertex, per player (starting with player 0).
	// Use u16 to avoid overflows when we have very large (but not infeasibly large) numbers
	// of units in a very small area.
	// (Note we use vertexes, not tiles, to better match the renderer.)
	// Lazily constructed when it's needed, to save memory in smaller games.
	std::array<Grid<u16>, MAX_LOS_PLAYER_ID> m_LosPlayerCounts;

	// 2-bit LosState per player, starting with player 1 (not 0!) up to player MAX_LOS_PLAYER_ID (inclusive)
	Grid<u32> m_LosState;

	// Special static visibility data for the "reveal whole map" mode
	// (TODO: this is usually a waste of memory)
	Grid<u32> m_LosStateRevealed;

	// Shared LOS masks, one per player.
	std::array<u32, MAX_LOS_PLAYER_ID+2> m_SharedLosMasks;
	// Shared dirty visibility masks, one per player.
	std::array<u16, MAX_LOS_PLAYER_ID+2> m_SharedDirtyVisibilityMasks;

	// Cache explored vertices per player (not serialized)
	u32 m_TotalInworldVertices;
	std::vector<u32> m_ExploredVertices;

	static std::string GetSchema()
	{
		return "<a:component type='system'/><empty/>";
	}

	void Init(const CParamNode&) override
	{
		m_QueryNext = 1;

		m_DebugOverlayEnabled = false;
		m_DebugOverlayDirty = true;

		m_Deserializing = false;
		m_WorldX0 = m_WorldZ0 = m_WorldX1 = m_WorldZ1 = entity_pos_t::Zero();

		// Initialise with bogus values (these will get replaced when
		// SetBounds is called)
		ResetSubdivisions(entity_pos_t::FromInt(1024), entity_pos_t::FromInt(1024));

		m_SubdivisionResults.reserve(4096);

		// The whole map should be visible to Gaia by default, else e.g. animals
		// will get confused when trying to run from enemies
		m_LosRevealAll[0] = true;

		m_GlobalVisibilityUpdate = true;

		m_LosCircular = false;
		m_LosVerticesPerSide = 0;
	}

	void Deinit() override
	{
	}

	template<typename S>
	void SerializeCommon(S& serialize)
	{
		serialize.NumberFixed_Unbounded("world x0", m_WorldX0);
		serialize.NumberFixed_Unbounded("world z0", m_WorldZ0);
		serialize.NumberFixed_Unbounded("world x1", m_WorldX1);
		serialize.NumberFixed_Unbounded("world z1", m_WorldZ1);

		serialize.NumberU32_Unbounded("query next", m_QueryNext);
		Serializer(serialize, "queries", m_Queries, GetSimContext());
		Serializer(serialize, "entity data", m_EntityData);

		Serializer(serialize, "los reveal all", m_LosRevealAll);
		serialize.Bool("los circular", m_LosCircular);
		serialize.NumberI32_Unbounded("los verts per side", m_LosVerticesPerSide);

		serialize.Bool("global visibility update", m_GlobalVisibilityUpdate);
		Serializer(serialize, "global player visibility update", m_GlobalPlayerVisibilityUpdate);
		Serializer(serialize, "dirty visibility", m_DirtyVisibility);
		Serializer(serialize, "modified entities", m_ModifiedEntities);

		// We don't serialize m_Subdivision, m_LosPlayerCounts or m_LosRegions
		// since they can be recomputed from the entity data when deserializing;
		// m_LosState must be serialized since it depends on the history of exploration

		Serializer(serialize, "los state", m_LosState);
		Serializer(serialize, "shared los masks", m_SharedLosMasks);
		Serializer(serialize, "shared dirty visibility masks", m_SharedDirtyVisibilityMasks);
	}

	void Serialize(ISerializer& serialize) override
	{
		SerializeCommon(serialize);
	}

	void Deserialize(const CParamNode& paramNode, IDeserializer& deserialize) override
	{
		Init(paramNode);

		SerializeCommon(deserialize);
	}

	void HandleMessage(const CMessage& msg, bool UNUSED(global)) override
	{
		switch (msg.GetType())
		{
		case MT_Deserialized:
		{
			// Reinitialize subdivisions and LOS data after all
			// other components have been deserialized.
			m_Deserializing = true;
			ResetDerivedData();
			m_Deserializing = false;
			break;
		}
		case MT_Create:
		{
			const CMessageCreate& msgData = static_cast<const CMessageCreate&> (msg);
			entity_id_t ent = msgData.entity;

			// Ignore local entities - we shouldn't let them influence anything
			if (ENTITY_IS_LOCAL(ent))
				break;

			// Ignore non-positional entities
			CmpPtr<ICmpPosition> cmpPosition(GetSimContext(), ent);
			if (!cmpPosition)
				break;

			// The newly-created entity will have owner -1 and position out-of-world
			// (any initialisation of those values will happen later), so we can just
			// use the default-constructed EntityData here
			EntityData entdata;

			// Store the LOS data, if any
			CmpPtr<ICmpVision> cmpVision(GetSimContext(), ent);
			if (cmpVision)
			{
				entdata.visionRange = cmpVision->GetRange();
				entdata.SetFlag<FlagMasks::RevealShore>(cmpVision->GetRevealShore());
			}
			CmpPtr<ICmpVisibility> cmpVisibility(GetSimContext(), ent);
			if (cmpVisibility)
				entdata.SetFlag<FlagMasks::RetainInFog>(cmpVisibility->GetRetainInFog());

			// Store the size
			CmpPtr<ICmpObstruction> cmpObstruction(GetSimContext(), ent);
			if (cmpObstruction)
				entdata.size = cmpObstruction->GetSize().ToInt_RoundToInfinity();

			// Remember this entity
			m_EntityData.insert(ent, entdata);
			break;
		}
		case MT_PositionChanged:
		{
			const CMessagePositionChanged& msgData = static_cast<const CMessagePositionChanged&> (msg);
			entity_id_t ent = msgData.entity;

			EntityMap<EntityData>::iterator it = m_EntityData.find(ent);

			// Ignore if we're not already tracking this entity
			if (it == m_EntityData.end())
				break;

			if (msgData.inWorld)
			{
				if (it->second.HasFlag<FlagMasks::InWorld>())
				{
					CFixedVector2D from(it->second.x, it->second.z);
					CFixedVector2D to(msgData.x, msgData.z);
					m_Subdivision.Move(ent, from, to, it->second.size);
					if (it->second.HasFlag<FlagMasks::SharedVision>())
						SharingLosMove(it->second.visionSharing, it->second.visionRange, from, to);
					else
						LosMove(it->second.owner, it->second.visionRange, from, to);
					LosRegion oldLosRegion = PosToLosRegionsHelper(it->second.x, it->second.z);
					LosRegion newLosRegion = PosToLosRegionsHelper(msgData.x, msgData.z);
					if (oldLosRegion != newLosRegion)
					{
						RemoveFromRegion(oldLosRegion, ent);
						AddToRegion(newLosRegion, ent);
					}
				}
				else
				{
					CFixedVector2D to(msgData.x, msgData.z);
					m_Subdivision.Add(ent, to, it->second.size);
					if (it->second.HasFlag<FlagMasks::SharedVision>())
						SharingLosAdd(it->second.visionSharing, it->second.visionRange, to);
					else
						LosAdd(it->second.owner, it->second.visionRange, to);
					AddToRegion(PosToLosRegionsHelper(msgData.x, msgData.z), ent);
				}

				it->second.SetFlag<FlagMasks::InWorld>(true);
				it->second.x = msgData.x;
				it->second.z = msgData.z;
			}
			else
			{
				if (it->second.HasFlag<FlagMasks::InWorld>())
				{
					CFixedVector2D from(it->second.x, it->second.z);
					m_Subdivision.Remove(ent, from, it->second.size);
					if (it->second.HasFlag<FlagMasks::SharedVision>())
						SharingLosRemove(it->second.visionSharing, it->second.visionRange, from);
					else
						LosRemove(it->second.owner, it->second.visionRange, from);
					RemoveFromRegion(PosToLosRegionsHelper(it->second.x, it->second.z), ent);
				}

				it->second.SetFlag<FlagMasks::InWorld>(false);
				it->second.x = entity_pos_t::Zero();
				it->second.z = entity_pos_t::Zero();
			}

			RequestVisibilityUpdate(ent);

			break;
		}
		case MT_OwnershipChanged:
		{
			const CMessageOwnershipChanged& msgData = static_cast<const CMessageOwnershipChanged&> (msg);
			entity_id_t ent = msgData.entity;

			EntityMap<EntityData>::iterator it = m_EntityData.find(ent);

			// Ignore if we're not already tracking this entity
			if (it == m_EntityData.end())
				break;

			if (it->second.HasFlag<FlagMasks::InWorld>())
			{
				// Entity vision is taken into account in VisionSharingChanged
				// when sharing component activated
				if (!it->second.HasFlag<FlagMasks::SharedVision>())
				{
					CFixedVector2D pos(it->second.x, it->second.z);
					LosRemove(it->second.owner, it->second.visionRange, pos);
					LosAdd(msgData.to, it->second.visionRange, pos);
				}

				if (it->second.HasFlag<FlagMasks::RevealShore>())
				{
					RevealShore(it->second.owner, false);
					RevealShore(msgData.to, true);
				}
			}

			ENSURE(-128 <= msgData.to && msgData.to <= 127);
			it->second.owner = (i8)msgData.to;

			break;
		}
		case MT_Destroy:
		{
			const CMessageDestroy& msgData = static_cast<const CMessageDestroy&> (msg);
			entity_id_t ent = msgData.entity;

			EntityMap<EntityData>::iterator it = m_EntityData.find(ent);

			// Ignore if we're not already tracking this entity
			if (it == m_EntityData.end())
				break;

			if (it->second.HasFlag<FlagMasks::InWorld>())
			{
				m_Subdivision.Remove(ent, CFixedVector2D(it->second.x, it->second.z), it->second.size);
				RemoveFromRegion(PosToLosRegionsHelper(it->second.x, it->second.z), ent);
			}

			// This will be called after Ownership's OnDestroy, so ownership will be set
			// to -1 already and we don't have to do a LosRemove here
			ENSURE(it->second.owner == -1);

			m_EntityData.erase(it);

			break;
		}
		case MT_VisionRangeChanged:
		{
			const CMessageVisionRangeChanged& msgData = static_cast<const CMessageVisionRangeChanged&> (msg);
			entity_id_t ent = msgData.entity;

			EntityMap<EntityData>::iterator it = m_EntityData.find(ent);

			// Ignore if we're not already tracking this entity
			if (it == m_EntityData.end())
				break;

			CmpPtr<ICmpVision> cmpVision(GetSimContext(), ent);
			if (!cmpVision)
				break;

			entity_pos_t oldRange = it->second.visionRange;
			entity_pos_t newRange = msgData.newRange;

			// If the range changed and the entity's in-world, we need to manually adjust it
			//	but if it's not in-world, we only need to set the new vision range

			it->second.visionRange = newRange;

			if (it->second.HasFlag<FlagMasks::InWorld>())
			{
				CFixedVector2D pos(it->second.x, it->second.z);
				if (it->second.HasFlag<FlagMasks::SharedVision>())
				{
					SharingLosRemove(it->second.visionSharing, oldRange, pos);
					SharingLosAdd(it->second.visionSharing, newRange, pos);
				}
				else
				{
					LosRemove(it->second.owner, oldRange, pos);
					LosAdd(it->second.owner, newRange, pos);
				}
			}

			break;
		}
		case MT_VisionSharingChanged:
		{
			const CMessageVisionSharingChanged& msgData = static_cast<const CMessageVisionSharingChanged&> (msg);
			entity_id_t ent = msgData.entity;

			EntityMap<EntityData>::iterator it = m_EntityData.find(ent);

			// Ignore if we're not already tracking this entity
			if (it == m_EntityData.end())
				break;

			ENSURE(msgData.player > 0 && msgData.player < MAX_LOS_PLAYER_ID+1);
			u16 visionChanged = CalcVisionSharingMask(msgData.player);

			if (!it->second.HasFlag<FlagMasks::SharedVision>())
			{
				// Activation of the Vision Sharing
				ENSURE(it->second.owner == (i8)msgData.player);
				it->second.visionSharing = visionChanged;
				it->second.SetFlag<FlagMasks::SharedVision>(true);
				break;
			}

			if (it->second.HasFlag<FlagMasks::InWorld>())
			{
				entity_pos_t range = it->second.visionRange;
				CFixedVector2D pos(it->second.x, it->second.z);
				if (msgData.add)
					LosAdd(msgData.player, range, pos);
				else
					LosRemove(msgData.player, range, pos);
			}

			if (msgData.add)
				it->second.visionSharing |= visionChanged;
			else
				it->second.visionSharing &= ~visionChanged;
			break;
		}
		case MT_Update:
		{
			m_DebugOverlayDirty = true;
			ExecuteActiveQueries();
			UpdateVisibilityData();
			break;
		}
		case MT_RenderSubmit:
		{
			const CMessageRenderSubmit& msgData = static_cast<const CMessageRenderSubmit&> (msg);
			RenderSubmit(msgData.collector);
			break;
		}
		}
	}

	void SetBounds(entity_pos_t x0, entity_pos_t z0, entity_pos_t x1, entity_pos_t z1) override
	{
		// Don't support rectangular looking maps.
		ENSURE(x1-x0 == z1-z0);
		m_WorldX0 = x0;
		m_WorldZ0 = z0;
		m_WorldX1 = x1;
		m_WorldZ1 = z1;
		m_LosVerticesPerSide = ((x1 - x0) / LOS_TILE_SIZE).ToInt_RoundToZero() + 1;

		ResetDerivedData();
	}

	void Verify() override
	{
		// Ignore if map not initialised yet
		if (m_WorldX1.IsZero())
			return;

		// Check that calling ResetDerivedData (i.e. recomputing all the state from scratch)
		// does not affect the incrementally-computed state

		std::array<Grid<u16>, MAX_LOS_PLAYER_ID> oldPlayerCounts = m_LosPlayerCounts;
		Grid<u32> oldStateRevealed = m_LosStateRevealed;
		FastSpatialSubdivision oldSubdivision = m_Subdivision;
		Grid<std::set<entity_id_t> > oldLosRegions = m_LosRegions;

		m_Deserializing = true;
		ResetDerivedData();
		m_Deserializing = false;

		if (oldPlayerCounts != m_LosPlayerCounts)
		{
			for (size_t id = 0; id < m_LosPlayerCounts.size(); ++id)
			{
				debug_printf("player %zu\n", id);
				for (size_t i = 0; i < oldPlayerCounts[id].width(); ++i)
				{
					for (size_t j = 0; j < oldPlayerCounts[id].height(); ++j)
						debug_printf("%i ", oldPlayerCounts[id].get(i,j));
					debug_printf("\n");
				}
			}
			for (size_t id = 0; id < m_LosPlayerCounts.size(); ++id)
			{
				debug_printf("player %zu\n", id);
				for (size_t i = 0; i < m_LosPlayerCounts[id].width(); ++i)
				{
					for (size_t j = 0; j < m_LosPlayerCounts[id].height(); ++j)
						debug_printf("%i ", m_LosPlayerCounts[id].get(i,j));
					debug_printf("\n");
				}
			}
			debug_warn(L"inconsistent player counts");
		}
		if (oldStateRevealed != m_LosStateRevealed)
			debug_warn(L"inconsistent revealed");
		if (oldSubdivision != m_Subdivision)
			debug_warn(L"inconsistent subdivs");
		if (oldLosRegions != m_LosRegions)
			debug_warn(L"inconsistent los regions");
	}

	FastSpatialSubdivision* GetSubdivision() override
	{
		return &m_Subdivision;
	}

	// Reinitialise subdivisions and LOS data, based on entity data
	void ResetDerivedData()
	{
		ENSURE(m_WorldX0.IsZero() && m_WorldZ0.IsZero()); // don't bother implementing non-zero offsets yet
		ResetSubdivisions(m_WorldX1, m_WorldZ1);

		m_LosRegionsPerSide = m_LosVerticesPerSide / LOS_REGION_RATIO;

		for (size_t player_id = 0; player_id < m_LosPlayerCounts.size(); ++player_id)
			m_LosPlayerCounts[player_id].clear();

		m_ExploredVertices.clear();
		m_ExploredVertices.resize(MAX_LOS_PLAYER_ID+1, 0);

		if (m_Deserializing)
		{
			// recalc current exploration stats.
			for (i32 j = 0; j < m_LosVerticesPerSide; j++)
				for (i32 i = 0; i < m_LosVerticesPerSide; i++)
					if (!LosIsOffWorld(i, j))
						for (u8 k = 1; k < MAX_LOS_PLAYER_ID+1; ++k)
							m_ExploredVertices.at(k) += ((m_LosState.get(i, j) & ((u32)LosState::EXPLORED << (2*(k-1)))) > 0);
		} else
			m_LosState.resize(m_LosVerticesPerSide, m_LosVerticesPerSide);

		m_LosStateRevealed.resize(m_LosVerticesPerSide, m_LosVerticesPerSide);

		if (!m_Deserializing)
		{
			m_DirtyVisibility.resize(m_LosRegionsPerSide, m_LosRegionsPerSide);
		}
		ENSURE(m_DirtyVisibility.width() == m_LosRegionsPerSide);
		ENSURE(m_DirtyVisibility.height() == m_LosRegionsPerSide);

		m_LosRegions.resize(m_LosRegionsPerSide, m_LosRegionsPerSide);

		for (EntityMap<EntityData>::const_iterator it = m_EntityData.begin(); it != m_EntityData.end(); ++it)
			if (it->second.HasFlag<FlagMasks::InWorld>())
			{
				if (it->second.HasFlag<FlagMasks::SharedVision>())
					SharingLosAdd(it->second.visionSharing, it->second.visionRange, CFixedVector2D(it->second.x, it->second.z));
				else
					LosAdd(it->second.owner, it->second.visionRange, CFixedVector2D(it->second.x, it->second.z));
				AddToRegion(PosToLosRegionsHelper(it->second.x, it->second.z), it->first);

				if (it->second.HasFlag<FlagMasks::RevealShore>())
					RevealShore(it->second.owner, true);
			}

		m_TotalInworldVertices = 0;
		for (i32 j = 0; j < m_LosVerticesPerSide; ++j)
			for (i32 i = 0; i < m_LosVerticesPerSide; ++i)
			{
				if (LosIsOffWorld(i,j))
					m_LosStateRevealed.get(i, j) = 0;
				else
				{
					m_LosStateRevealed.get(i, j) = 0xFFFFFFFFu;
					m_TotalInworldVertices++;
				}
			}
	}

	void ResetSubdivisions(entity_pos_t x1, entity_pos_t z1)
	{
		m_Subdivision.Reset(x1, z1);

		for (EntityMap<EntityData>::const_iterator it = m_EntityData.begin(); it != m_EntityData.end(); ++it)
			if (it->second.HasFlag<FlagMasks::InWorld>())
				m_Subdivision.Add(it->first, CFixedVector2D(it->second.x, it->second.z), it->second.size);
	}

	tag_t CreateActiveQuery(entity_id_t source,
		entity_pos_t minRange, entity_pos_t maxRange,
		const std::vector<int>& owners, int requiredInterface, u8 flags, bool accountForSize) override
	{
		tag_t id = m_QueryNext++;
		m_Queries[id] = ConstructQuery(source, minRange, maxRange, owners, requiredInterface, flags, accountForSize);

		return id;
	}

	tag_t CreateActiveParabolicQuery(entity_id_t source,
		entity_pos_t minRange, entity_pos_t maxRange, entity_pos_t yOrigin,
		const std::vector<int>& owners, int requiredInterface, u8 flags) override
	{
		tag_t id = m_QueryNext++;
		m_Queries[id] = ConstructParabolicQuery(source, minRange, maxRange, yOrigin, owners, requiredInterface, flags, true);

		return id;
	}

	void DestroyActiveQuery(tag_t tag) override
	{
		if (m_Queries.find(tag) == m_Queries.end())
		{
			LOGERROR("CCmpRangeManager: DestroyActiveQuery called with invalid tag %u", tag);
			return;
		}

		m_Queries.erase(tag);
	}

	void EnableActiveQuery(tag_t tag) override
	{
		std::map<tag_t, Query>::iterator it = m_Queries.find(tag);
		if (it == m_Queries.end())
		{
			LOGERROR("CCmpRangeManager: EnableActiveQuery called with invalid tag %u", tag);
			return;
		}

		Query& q = it->second;
		q.enabled = true;
	}

	void DisableActiveQuery(tag_t tag) override
	{
		std::map<tag_t, Query>::iterator it = m_Queries.find(tag);
		if (it == m_Queries.end())
		{
			LOGERROR("CCmpRangeManager: DisableActiveQuery called with invalid tag %u", tag);
			return;
		}

		Query& q = it->second;
		q.enabled = false;
	}

	bool IsActiveQueryEnabled(tag_t tag) const override
	{
		std::map<tag_t, Query>::const_iterator it = m_Queries.find(tag);
		if (it == m_Queries.end())
		{
			LOGERROR("CCmpRangeManager: IsActiveQueryEnabled called with invalid tag %u", tag);
			return false;
		}

		const Query& q = it->second;
		return q.enabled;
	}

	std::vector<entity_id_t> ExecuteQueryAroundPos(const CFixedVector2D& pos,
		entity_pos_t minRange, entity_pos_t maxRange,
		const std::vector<int>& owners, int requiredInterface, bool accountForSize) override
	{
		Query q = ConstructQuery(INVALID_ENTITY, minRange, maxRange, owners, requiredInterface, GetEntityFlagMask("normal"), accountForSize);
		std::vector<entity_id_t> r;
		PerformQuery(q, r, pos);

		// Return the list sorted by distance from the entity
		std::stable_sort(r.begin(), r.end(), EntityDistanceOrdering(m_EntityData, pos));

		return r;
	}

	std::vector<entity_id_t> ExecuteQuery(entity_id_t source,
		entity_pos_t minRange, entity_pos_t maxRange,
		const std::vector<int>& owners, int requiredInterface, bool accountForSize) override
	{
		PROFILE("ExecuteQuery");

		Query q = ConstructQuery(source, minRange, maxRange, owners, requiredInterface, GetEntityFlagMask("normal"), accountForSize);

		std::vector<entity_id_t> r;

		CmpPtr<ICmpPosition> cmpSourcePosition(q.source);
		if (!cmpSourcePosition || !cmpSourcePosition->IsInWorld())
		{
			// If the source doesn't have a position, then the result is just the empty list
			return r;
		}

		CFixedVector2D pos = cmpSourcePosition->GetPosition2D();
		PerformQuery(q, r, pos);

		// Return the list sorted by distance from the entity
		std::stable_sort(r.begin(), r.end(), EntityDistanceOrdering(m_EntityData, pos));

		return r;
	}

	std::vector<entity_id_t> ResetActiveQuery(tag_t tag) override
	{
		PROFILE("ResetActiveQuery");

		std::vector<entity_id_t> r;

		std::map<tag_t, Query>::iterator it = m_Queries.find(tag);
		if (it == m_Queries.end())
		{
			LOGERROR("CCmpRangeManager: ResetActiveQuery called with invalid tag %u", tag);
			return r;
		}

		Query& q = it->second;
		q.enabled = true;

		CmpPtr<ICmpPosition> cmpSourcePosition(q.source);
		if (!cmpSourcePosition || !cmpSourcePosition->IsInWorld())
		{
			// If the source doesn't have a position, then the result is just the empty list
			q.lastMatch = r;
			return r;
		}

		CFixedVector2D pos = cmpSourcePosition->GetPosition2D();
		PerformQuery(q, r, pos);

		q.lastMatch = r;

		// Return the list sorted by distance from the entity
		std::stable_sort(r.begin(), r.end(), EntityDistanceOrdering(m_EntityData, pos));

		return r;
	}

	std::vector<entity_id_t> GetEntitiesByPlayer(player_id_t player) const override
	{
		return GetEntitiesByMask(CalcOwnerMask(player));
	}

	std::vector<entity_id_t> GetNonGaiaEntities() const override
	{
		return GetEntitiesByMask(~3u); // bit 0 for owner=-1 and bit 1 for gaia
	}

	std::vector<entity_id_t> GetGaiaAndNonGaiaEntities() const override
	{
		return GetEntitiesByMask(~1u); // bit 0 for owner=-1
	}

	std::vector<entity_id_t> GetEntitiesByMask(u32 ownerMask) const
	{
		std::vector<entity_id_t> entities;

		for (EntityMap<EntityData>::const_iterator it = m_EntityData.begin(); it != m_EntityData.end(); ++it)
		{
			// Check owner and add to list if it matches
			if (CalcOwnerMask(it->second.owner) & ownerMask)
				entities.push_back(it->first);
		}

		return entities;
	}

	void SetDebugOverlay(bool enabled) override
	{
		m_DebugOverlayEnabled = enabled;
		m_DebugOverlayDirty = true;
		if (!enabled)
			m_DebugOverlayLines.clear();
	}

	/**
	 * Update all currently-enabled active queries.
	 */
	void ExecuteActiveQueries()
	{
		PROFILE3("ExecuteActiveQueries");

		// Store a queue of all messages before sending any, so we can assume
		// no entities will move until we've finished checking all the ranges
		std::vector<std::pair<entity_id_t, CMessageRangeUpdate> > messages;
		std::vector<entity_id_t> results;
		std::vector<entity_id_t> added;
		std::vector<entity_id_t> removed;

		for (std::map<tag_t, Query>::iterator it = m_Queries.begin(); it != m_Queries.end(); ++it)
		{
			Query& query = it->second;

			if (!query.enabled)
				continue;

			results.clear();
			CmpPtr<ICmpPosition> cmpSourcePosition(query.source);
			if (cmpSourcePosition && cmpSourcePosition->IsInWorld())
			{
				results.reserve(query.lastMatch.size());
				PerformQuery(query, results, cmpSourcePosition->GetPosition2D());
			}

			// Compute the changes vs the last match
			added.clear();
			removed.clear();
			// Return the 'added' list sorted by distance from the entity
			// (Don't bother sorting 'removed' because they might not even have positions or exist any more)
			std::set_difference(results.begin(), results.end(), query.lastMatch.begin(), query.lastMatch.end(),
				std::back_inserter(added));
			std::set_difference(query.lastMatch.begin(), query.lastMatch.end(), results.begin(), results.end(),
				std::back_inserter(removed));
			if (added.empty() && removed.empty())
				continue;

			if (cmpSourcePosition && cmpSourcePosition->IsInWorld())
				std::stable_sort(added.begin(), added.end(), EntityDistanceOrdering(m_EntityData, cmpSourcePosition->GetPosition2D()));

			messages.resize(messages.size() + 1);
			std::pair<entity_id_t, CMessageRangeUpdate>& back = messages.back();
			back.first = query.source.GetId();
			back.second.tag = it->first;
			back.second.added.swap(added);
			back.second.removed.swap(removed);
			query.lastMatch.swap(results);
		}

		CComponentManager& cmpMgr = GetSimContext().GetComponentManager();
		for (size_t i = 0; i < messages.size(); ++i)
			cmpMgr.PostMessage(messages[i].first, messages[i].second);
	}

	/**
	 * Returns whether the given entity matches the given query (ignoring maxRange)
	 */
	bool TestEntityQuery(const Query& q, entity_id_t id, const EntityData& entity) const
	{
		// Quick filter to ignore entities with the wrong owner
		if (!(CalcOwnerMask(entity.owner) & q.ownersMask))
			return false;

		// Ignore entities not present in the world
		if (!entity.HasFlag<FlagMasks::InWorld>())
			return false;

		// Ignore entities that don't match the current flags
		if (!((entity.flags & FlagMasks::AllQuery) & q.flagsMask))
			return false;

		// Ignore self
		if (id == q.source.GetId())
			return false;

		// Ignore if it's missing the required interface
		if (q.interface && !GetSimContext().GetComponentManager().QueryInterface(id, q.interface))
			return false;

		return true;
	}

	/**
	 * Returns a list of distinct entity IDs that match the given query, sorted by ID.
	 */
	void PerformQuery(const Query& q, std::vector<entity_id_t>& r, CFixedVector2D pos)
	{

		// Special case: range is ALWAYS_IN_RANGE means check all entities ignoring distance.
		if (q.maxRange == ALWAYS_IN_RANGE)
		{
			for (EntityMap<EntityData>::const_iterator it = m_EntityData.begin(); it != m_EntityData.end(); ++it)
			{
				if (!TestEntityQuery(q, it->first, it->second))
					continue;

				r.push_back(it->first);
			}
		}
		// Not the entire world, so check a parabolic range, or a regular range.
		else if (q.parabolic)
		{
			// The yOrigin is part of the 3D position, as the source is really that much heigher.
			CmpPtr<ICmpPosition> cmpSourcePosition(q.source);
			CFixedVector3D pos3d = cmpSourcePosition->GetPosition()+
			    CFixedVector3D(entity_pos_t::Zero(), q.yOrigin, entity_pos_t::Zero()) ;
			// Get a quick list of entities that are potentially in range, with a cutoff of 2*maxRange.
			m_SubdivisionResults.clear();
			m_Subdivision.GetNear(m_SubdivisionResults, pos, q.maxRange * 2);

			for (size_t i = 0; i < m_SubdivisionResults.size(); ++i)
			{
				EntityMap<EntityData>::const_iterator it = m_EntityData.find(m_SubdivisionResults[i]);
				ENSURE(it != m_EntityData.end());

				if (!TestEntityQuery(q, it->first, it->second))
					continue;

				CmpPtr<ICmpPosition> cmpSecondPosition(GetSimContext(), m_SubdivisionResults[i]);
				if (!cmpSecondPosition || !cmpSecondPosition->IsInWorld())
					continue;
				CFixedVector3D secondPosition = cmpSecondPosition->GetPosition();

				// Doing an exact check for parabolas with obstruction sizes is not really possible.
				// However, we can prove that InParabolicRange(d, range + size) > InParabolicRange(d, range)
				// in the sense that it always returns true when the latter would, which is enough.
				// To do so, compute the derivative with respect to distance, and notice that
				// they have an intersection after which the former grows slower, and then use that to prove the above.
				// Note that this is only true because we do not account for vertical size here,
				// if we did, we would also need to artificially 'raise' the source over the target.
				entity_pos_t range = q.maxRange + (q.accountForSize ? fixed::FromInt(it->second.size) : fixed::Zero());
				if (!InParabolicRange(CFixedVector3D(it->second.x, secondPosition.Y, it->second.z) - pos3d, range))
					continue;

				if (!q.minRange.IsZero())
					if ((CFixedVector2D(it->second.x, it->second.z) - pos).CompareLength(q.minRange) < 0)
						continue;

				r.push_back(it->first);
			}
			std::sort(r.begin(), r.end());
		}
		// check a regular range (i.e. not the entire world, and not parabolic)
		else
		{
			// Get a quick list of entities that are potentially in range
			m_SubdivisionResults.clear();
			m_Subdivision.GetNear(m_SubdivisionResults, pos, q.maxRange);

			for (size_t i = 0; i < m_SubdivisionResults.size(); ++i)
			{
				EntityMap<EntityData>::const_iterator it = m_EntityData.find(m_SubdivisionResults[i]);
				ENSURE(it != m_EntityData.end());

				if (!TestEntityQuery(q, it->first, it->second))
					continue;

				// Restrict based on approximate circle-circle distance.
				entity_pos_t range = q.maxRange + (q.accountForSize ? fixed::FromInt(it->second.size) : fixed::Zero());
				if ((CFixedVector2D(it->second.x, it->second.z) - pos).CompareLength(range) > 0)
					continue;

				if (!q.minRange.IsZero())
					if ((CFixedVector2D(it->second.x, it->second.z) - pos).CompareLength(q.minRange) < 0)
						continue;

				r.push_back(it->first);
			}
			std::sort(r.begin(), r.end());
		}
	}

	entity_pos_t GetEffectiveParabolicRange(entity_id_t source, entity_id_t target, entity_pos_t range, entity_pos_t yOrigin) const override
	{
		// For non-positive ranges, just return the range.
		if (range < entity_pos_t::Zero())
			return range;

		CmpPtr<ICmpPosition> cmpSourcePosition(GetSimContext(), source);
		if (!cmpSourcePosition || !cmpSourcePosition->IsInWorld())
			return NEVER_IN_RANGE;

		CmpPtr<ICmpPosition> cmpTargetPosition(GetSimContext(), target);
		if (!cmpTargetPosition || !cmpTargetPosition->IsInWorld())
			return NEVER_IN_RANGE;

		entity_pos_t heightDifference = cmpSourcePosition->GetHeightOffset() - cmpTargetPosition->GetHeightOffset() + yOrigin;
		if (heightDifference < -range / 2)
			return NEVER_IN_RANGE;

		entity_pos_t effectiveRange;
		effectiveRange.SetInternalValue(static_cast<i32>(isqrt64(SQUARE_U64_FIXED(range) + static_cast<i64>(heightDifference.GetInternalValue()) * static_cast<i64>(range.GetInternalValue()) * 2)));
		return effectiveRange;
	}

	entity_pos_t GetElevationAdaptedRange(const CFixedVector3D& pos1, const CFixedVector3D& rot, entity_pos_t range, entity_pos_t yOrigin, entity_pos_t angle) const override
	{
		entity_pos_t r = entity_pos_t::Zero();
		CFixedVector3D pos(pos1);

		pos.Y += yOrigin;
		entity_pos_t orientation = rot.Y;

		entity_pos_t maxAngle = orientation + angle/2;
		entity_pos_t minAngle = orientation - angle/2;

		int numberOfSteps = 16;

		if (angle == entity_pos_t::Zero())
			numberOfSteps = 1;

		std::vector<entity_pos_t> coords = getParabolicRangeForm(pos, range, range*2, minAngle, maxAngle, numberOfSteps);

		entity_pos_t part = entity_pos_t::FromInt(numberOfSteps);

		for (int i = 0; i < numberOfSteps; ++i)
			r = r + CFixedVector2D(coords[2*i],coords[2*i+1]).Length() / part;

		return r;

	}

	virtual std::vector<entity_pos_t> getParabolicRangeForm(CFixedVector3D pos, entity_pos_t maxRange, entity_pos_t cutoff, entity_pos_t minAngle, entity_pos_t maxAngle, int numberOfSteps) const
	{
		std::vector<entity_pos_t> r;

		CmpPtr<ICmpTerrain> cmpTerrain(GetSystemEntity());
		if (!cmpTerrain)
			return r;

		// angle = 0 goes in the positive Z direction
		u64 precisionSquared = SQUARE_U64_FIXED(PARABOLIC_RANGE_TOLERANCE);

		CmpPtr<ICmpWaterManager> cmpWaterManager(GetSystemEntity());
		entity_pos_t waterLevel = cmpWaterManager ? cmpWaterManager->GetWaterLevel(pos.X, pos.Z) : entity_pos_t::Zero();
		entity_pos_t thisHeight = pos.Y > waterLevel ? pos.Y : waterLevel;

		for (int i = 0; i < numberOfSteps; ++i)
		{
			entity_pos_t angle = minAngle + (maxAngle - minAngle) / numberOfSteps * i;
			entity_pos_t sin;
			entity_pos_t cos;
			entity_pos_t minDistance = entity_pos_t::Zero();
			entity_pos_t maxDistance = cutoff;
			sincos_approx(angle, sin, cos);

			CFixedVector2D minVector = CFixedVector2D(entity_pos_t::Zero(), entity_pos_t::Zero());
			CFixedVector2D maxVector = CFixedVector2D(sin, cos).Multiply(cutoff);
			entity_pos_t targetHeight = cmpTerrain->GetGroundLevel(pos.X+maxVector.X, pos.Z+maxVector.Y);
			// use water level to display range on water
			targetHeight = targetHeight > waterLevel ? targetHeight : waterLevel;

			if (InParabolicRange(CFixedVector3D(maxVector.X, targetHeight-thisHeight, maxVector.Y), maxRange))
			{
				r.push_back(maxVector.X);
				r.push_back(maxVector.Y);
				continue;
			}

			// Loop until vectors come close enough
			while ((maxVector - minVector).CompareLengthSquared(precisionSquared) > 0)
			{
				// difference still bigger than precision, bisect to get smaller difference
				entity_pos_t newDistance = (minDistance+maxDistance)/entity_pos_t::FromInt(2);

				CFixedVector2D newVector = CFixedVector2D(sin, cos).Multiply(newDistance);

				// get the height of the ground
				targetHeight = cmpTerrain->GetGroundLevel(pos.X+newVector.X, pos.Z+newVector.Y);
				targetHeight = targetHeight > waterLevel ? targetHeight : waterLevel;

				if (InParabolicRange(CFixedVector3D(newVector.X, targetHeight-thisHeight, newVector.Y), maxRange))
				{
					// new vector is in parabolic range, so this is a new minVector
					minVector = newVector;
					minDistance = newDistance;
				}
				else
				{
					// new vector is out parabolic range, so this is a new maxVector
					maxVector = newVector;
					maxDistance = newDistance;
				}

			}
			r.push_back(maxVector.X);
			r.push_back(maxVector.Y);

		}
		r.push_back(r[0]);
		r.push_back(r[1]);

		return r;
	}

	Query ConstructQuery(entity_id_t source,
		entity_pos_t minRange, entity_pos_t maxRange,
		const std::vector<int>& owners, int requiredInterface, u8 flagsMask, bool accountForSize) const
	{
		// Min range must be non-negative.
		if (minRange < entity_pos_t::Zero())
			LOGWARNING("CCmpRangeManager: Invalid min range %f in query for entity %u", minRange.ToDouble(), source);

		// Max range must be non-negative, or else ALWAYS_IN_RANGE.
		// TODO add NEVER_IN_RANGE.
		if (maxRange < entity_pos_t::Zero() && maxRange != ALWAYS_IN_RANGE)
			LOGWARNING("CCmpRangeManager: Invalid max range %f in query for entity %u", maxRange.ToDouble(), source);

		Query q;
		q.enabled = false;
		q.parabolic = false;
		q.source = GetSimContext().GetComponentManager().LookupEntityHandle(source);
		q.minRange = minRange;
		q.maxRange = maxRange;
		q.yOrigin = entity_pos_t::Zero();
		q.accountForSize = accountForSize;

		if (q.accountForSize && q.source.GetId() != INVALID_ENTITY && q.maxRange != ALWAYS_IN_RANGE)
		{
			u32 size = 0;
			if (ENTITY_IS_LOCAL(q.source.GetId()))
			{
				CmpPtr<ICmpObstruction> cmpObstruction(GetSimContext(), q.source.GetId());
				if (cmpObstruction)
					size = cmpObstruction->GetSize().ToInt_RoundToInfinity();
			}
			else
			{
				EntityMap<EntityData>::const_iterator it = m_EntityData.find(q.source.GetId());
				if (it != m_EntityData.end())
					size = it->second.size;
			}
			// Adjust the range query based on the querier's obstruction radius.
			// The smallest side of the obstruction isn't known here, so we can't safely adjust the min-range, only the max.
			// 'size' is the diagonal size rounded up so this will cover all possible rotations of the querier.
			q.maxRange += fixed::FromInt(size);
		}

		q.ownersMask = 0;
		for (size_t i = 0; i < owners.size(); ++i)
			q.ownersMask |= CalcOwnerMask(owners[i]);

		if (q.ownersMask == 0)
			LOGWARNING("CCmpRangeManager: No owners in query for entity %u", source);

		q.interface = requiredInterface;
		q.flagsMask = flagsMask;

		return q;
	}

	Query ConstructParabolicQuery(entity_id_t source,
		entity_pos_t minRange, entity_pos_t maxRange, entity_pos_t yOrigin,
		const std::vector<int>& owners, int requiredInterface, u8 flagsMask, bool accountForSize) const
	{
		Query q = ConstructQuery(source, minRange, maxRange, owners, requiredInterface, flagsMask, accountForSize);
		q.parabolic = true;
		q.yOrigin = yOrigin;
		return q;
	}

	void RenderSubmit(SceneCollector& collector)
	{
		if (!m_DebugOverlayEnabled)
			return;
		static CColor disabledRingColor(1, 0, 0, 1);	// red
		static CColor enabledRingColor(0, 1, 0, 1);	// green
		static CColor subdivColor(0, 0, 1, 1);			// blue
		static CColor rayColor(1, 1, 0, 0.2f);

		if (m_DebugOverlayDirty)
		{
			m_DebugOverlayLines.clear();

			for (std::map<tag_t, Query>::iterator it = m_Queries.begin(); it != m_Queries.end(); ++it)
			{
				Query& q = it->second;

				CmpPtr<ICmpPosition> cmpSourcePosition(q.source);
				if (!cmpSourcePosition || !cmpSourcePosition->IsInWorld())
					continue;
				CFixedVector2D pos = cmpSourcePosition->GetPosition2D();

				// Draw the max range circle
				if (!q.parabolic)
				{
					m_DebugOverlayLines.push_back(SOverlayLine());
					m_DebugOverlayLines.back().m_Color = (q.enabled ? enabledRingColor : disabledRingColor);
					SimRender::ConstructCircleOnGround(GetSimContext(), pos.X.ToFloat(), pos.Y.ToFloat(), q.maxRange.ToFloat(), m_DebugOverlayLines.back(), true);
				}
				else
				{
					// yOrigin is part of the 3D position. As if the unit is really that much higher.
					CFixedVector3D pos3D = cmpSourcePosition->GetPosition();
					pos3D.Y += q.yOrigin;

					std::vector<entity_pos_t> coords;

					// Get the outline from cache if possible
					if (ParabolicRangesOutlines.find(q.source.GetId()) != ParabolicRangesOutlines.end())
					{
						EntityParabolicRangeOutline e = ParabolicRangesOutlines[q.source.GetId()];
						if (e.position == pos3D && e.range == q.maxRange)
						{
							// outline is cached correctly, use it
							coords = e.outline;
						}
						else
						{
							// outline was cached, but important parameters changed
							// (position, elevation, range)
							// update it
							coords = getParabolicRangeForm(pos3D,q.maxRange,q.maxRange*2, entity_pos_t::Zero(), entity_pos_t::FromFloat(2.0f*3.14f),70);
							e.outline = coords;
							e.range = q.maxRange;
							e.position = pos3D;
							ParabolicRangesOutlines[q.source.GetId()] = e;
						}
					}
					else
					{
						// outline wasn't cached (first time you enable the range overlay
						// or you created a new entiy)
						// cache a new outline
						coords = getParabolicRangeForm(pos3D,q.maxRange,q.maxRange*2, entity_pos_t::Zero(), entity_pos_t::FromFloat(2.0f*3.14f),70);
						EntityParabolicRangeOutline e;
						e.source = q.source.GetId();
						e.range = q.maxRange;
						e.position = pos3D;
						e.outline = coords;
						ParabolicRangesOutlines[q.source.GetId()] = e;
					}

					CColor thiscolor = q.enabled ? enabledRingColor : disabledRingColor;

					// draw the outline (piece by piece)
					for (size_t i = 3; i < coords.size(); i += 2)
					{
						std::vector<float> c;
						c.push_back((coords[i - 3] + pos3D.X).ToFloat());
						c.push_back((coords[i - 2] + pos3D.Z).ToFloat());
						c.push_back((coords[i - 1] + pos3D.X).ToFloat());
						c.push_back((coords[i] + pos3D.Z).ToFloat());
						m_DebugOverlayLines.push_back(SOverlayLine());
						m_DebugOverlayLines.back().m_Color = thiscolor;
						SimRender::ConstructLineOnGround(GetSimContext(), c, m_DebugOverlayLines.back(), true);
					}
				}

				// Draw the min range circle
				if (!q.minRange.IsZero())
					SimRender::ConstructCircleOnGround(GetSimContext(), pos.X.ToFloat(), pos.Y.ToFloat(), q.minRange.ToFloat(), m_DebugOverlayLines.back(), true);

				// Draw a ray from the source to each matched entity
				for (size_t i = 0; i < q.lastMatch.size(); ++i)
				{
					CmpPtr<ICmpPosition> cmpTargetPosition(GetSimContext(), q.lastMatch[i]);
					if (!cmpTargetPosition || !cmpTargetPosition->IsInWorld())
						continue;
					CFixedVector2D targetPos = cmpTargetPosition->GetPosition2D();

					std::vector<float> coords;
					coords.push_back(pos.X.ToFloat());
					coords.push_back(pos.Y.ToFloat());
					coords.push_back(targetPos.X.ToFloat());
					coords.push_back(targetPos.Y.ToFloat());

					m_DebugOverlayLines.push_back(SOverlayLine());
					m_DebugOverlayLines.back().m_Color = rayColor;
					SimRender::ConstructLineOnGround(GetSimContext(), coords, m_DebugOverlayLines.back(), true);
				}
			}

			// render subdivision grid
			float divSize = m_Subdivision.GetDivisionSize();
			int size = m_Subdivision.GetWidth();
			for (int x = 0; x < size; ++x)
			{
				for (int y = 0; y < size; ++y)
				{
					m_DebugOverlayLines.push_back(SOverlayLine());
					m_DebugOverlayLines.back().m_Color = subdivColor;

					float xpos = x*divSize + divSize/2;
					float zpos = y*divSize + divSize/2;
					SimRender::ConstructSquareOnGround(GetSimContext(), xpos, zpos, divSize, divSize, 0.0f,
						m_DebugOverlayLines.back(), false, 1.0f);
				}
			}

			m_DebugOverlayDirty = false;
		}

		for (size_t i = 0; i < m_DebugOverlayLines.size(); ++i)
			collector.Submit(&m_DebugOverlayLines[i]);
	}

	u8 GetEntityFlagMask(const std::string& identifier) const override
	{
		if (identifier == "normal")
			return FlagMasks::Normal;
		if (identifier == "injured")
			return FlagMasks::Injured;

		LOGWARNING("CCmpRangeManager: Invalid flag identifier %s", identifier.c_str());
		return FlagMasks::None;
	}

	void SetEntityFlag(entity_id_t ent, const std::string& identifier, bool value) override
	{
		EntityMap<EntityData>::iterator it = m_EntityData.find(ent);

		// We don't have this entity
		if (it == m_EntityData.end())
			return;

		u8 flag = GetEntityFlagMask(identifier);

		if (flag == FlagMasks::None)
			LOGWARNING("CCmpRangeManager: Invalid flag identifier %s for entity %u", identifier.c_str(), ent);
		else
			it->second.SetFlag(flag, value);
	}

	// ****************************************************************

	// LOS implementation:

	CLosQuerier GetLosQuerier(player_id_t player) const override
	{
		if (GetLosRevealAll(player))
			return CLosQuerier(0xFFFFFFFFu, m_LosStateRevealed, m_LosVerticesPerSide);
		else
			return CLosQuerier(GetSharedLosMask(player), m_LosState, m_LosVerticesPerSide);
	}

	void ActivateScriptedVisibility(entity_id_t ent, bool status) override
	{
		EntityMap<EntityData>::iterator it = m_EntityData.find(ent);
		if (it != m_EntityData.end())
			it->second.SetFlag<FlagMasks::ScriptedVisibility>(status);
	}

	LosVisibility ComputeLosVisibility(CEntityHandle ent, player_id_t player) const
	{
		// Entities not with positions in the world are never visible
		if (ent.GetId() == INVALID_ENTITY)
			return LosVisibility::HIDDEN;
		CmpPtr<ICmpPosition> cmpPosition(ent);
		if (!cmpPosition || !cmpPosition->IsInWorld())
			return LosVisibility::HIDDEN;

		// Mirage entities, whatever the situation, are visible for one specific player
		CmpPtr<ICmpMirage> cmpMirage(ent);
		if (cmpMirage && cmpMirage->GetPlayer() != player)
			return LosVisibility::HIDDEN;

		CFixedVector2D pos = cmpPosition->GetPosition2D();
		int i = (pos.X / LOS_TILE_SIZE).ToInt_RoundToNearest();
		int j = (pos.Y / LOS_TILE_SIZE).ToInt_RoundToNearest();

		// Reveal flag makes all positioned entities visible and all mirages useless
		if (GetLosRevealAll(player))
		{
			if (LosIsOffWorld(i, j) || cmpMirage)
				return LosVisibility::HIDDEN;
			return LosVisibility::VISIBLE;
		}

		// Get visible regions
		CLosQuerier los(GetSharedLosMask(player), m_LosState, m_LosVerticesPerSide);

		CmpPtr<ICmpVisibility> cmpVisibility(ent);

		// Possibly ask the scripted Visibility component
		EntityMap<EntityData>::const_iterator it = m_EntityData.find(ent.GetId());
		if (it != m_EntityData.end())
		{
			if (it->second.HasFlag<FlagMasks::ScriptedVisibility>() && cmpVisibility)
				return cmpVisibility->GetVisibility(player, los.IsVisible(i, j), los.IsExplored(i, j));
		}
		else
		{
			if (cmpVisibility && cmpVisibility->IsActivated())
				return cmpVisibility->GetVisibility(player, los.IsVisible(i, j), los.IsExplored(i, j));
		}

		// Else, default behavior

		if (los.IsVisible(i, j))
		{
			if (cmpMirage)
				return LosVisibility::HIDDEN;

			return LosVisibility::VISIBLE;
		}

		if (!los.IsExplored(i, j))
			return LosVisibility::HIDDEN;

		// Invisible if the 'retain in fog' flag is not set, and in a non-visible explored region
		// Try using the 'retainInFog' flag in m_EntityData to save a script call
		if (it != m_EntityData.end())
		{
			if (!it->second.HasFlag<FlagMasks::RetainInFog>())
				return LosVisibility::HIDDEN;
		}
		else
		{
			if (!(cmpVisibility && cmpVisibility->GetRetainInFog()))
				return LosVisibility::HIDDEN;
		}

		if (cmpMirage)
			return LosVisibility::FOGGED;

		CmpPtr<ICmpOwnership> cmpOwnership(ent);
		if (!cmpOwnership)
			return LosVisibility::FOGGED;

		if (cmpOwnership->GetOwner() == player)
		{
			CmpPtr<ICmpFogging> cmpFogging(ent);
			if (!(cmpFogging && cmpFogging->IsMiraged(player)))
				return LosVisibility::FOGGED;

			return LosVisibility::HIDDEN;
		}

		// Fogged entities are hidden in two cases:
		// - They were not scouted
		// - A mirage replaces them
		CmpPtr<ICmpFogging> cmpFogging(ent);
		if (cmpFogging && cmpFogging->IsActivated() &&
			(!cmpFogging->WasSeen(player) || cmpFogging->IsMiraged(player)))
			return LosVisibility::HIDDEN;

		return LosVisibility::FOGGED;
	}

	LosVisibility ComputeLosVisibility(entity_id_t ent, player_id_t player) const
	{
		CEntityHandle handle = GetSimContext().GetComponentManager().LookupEntityHandle(ent);
		return ComputeLosVisibility(handle, player);
	}

	LosVisibility GetLosVisibility(CEntityHandle ent, player_id_t player) const override
	{
		entity_id_t entId = ent.GetId();

		// Entities not with positions in the world are never visible
		if (entId == INVALID_ENTITY)
			return LosVisibility::HIDDEN;

		CmpPtr<ICmpPosition> cmpPosition(ent);
		if (!cmpPosition || !cmpPosition->IsInWorld())
			return LosVisibility::HIDDEN;

		// Gaia and observers do not have a visibility cache
		if (player <= 0)
			return ComputeLosVisibility(ent, player);

		CFixedVector2D pos = cmpPosition->GetPosition2D();

		if (IsVisibilityDirty(m_DirtyVisibility[PosToLosRegionsHelper(pos.X, pos.Y)], player))
			return ComputeLosVisibility(ent, player);

		if (std::find(m_ModifiedEntities.begin(), m_ModifiedEntities.end(), entId) != m_ModifiedEntities.end())
			return ComputeLosVisibility(ent, player);

		EntityMap<EntityData>::const_iterator it = m_EntityData.find(entId);
		if (it == m_EntityData.end())
			return ComputeLosVisibility(ent, player);

		return static_cast<LosVisibility>(GetPlayerVisibility(it->second.visibilities, player));
	}

	LosVisibility GetLosVisibility(entity_id_t ent, player_id_t player) const override
	{
		CEntityHandle handle = GetSimContext().GetComponentManager().LookupEntityHandle(ent);
		return GetLosVisibility(handle, player);
	}

	LosVisibility GetLosVisibilityPosition(entity_pos_t x, entity_pos_t z, player_id_t player) const override
	{
		int i = (x / LOS_TILE_SIZE).ToInt_RoundToNearest();
		int j = (z / LOS_TILE_SIZE).ToInt_RoundToNearest();

		// Reveal flag makes all positioned entities visible and all mirages useless
		if (GetLosRevealAll(player))
		{
			if (LosIsOffWorld(i, j))
				return LosVisibility::HIDDEN;
			else
				return LosVisibility::VISIBLE;
		}

		// Get visible regions
		CLosQuerier los(GetSharedLosMask(player), m_LosState, m_LosVerticesPerSide);

		if (los.IsVisible(i,j))
			return LosVisibility::VISIBLE;
		if (los.IsExplored(i,j))
			return LosVisibility::FOGGED;
		return LosVisibility::HIDDEN;
	}

	size_t GetVerticesPerSide() const override
	{
		return m_LosVerticesPerSide;
	}

	LosRegion LosVertexToLosRegionsHelper(u16 x, u16 z) const
	{
		return LosRegion {
			Clamp(x/LOS_REGION_RATIO, 0, m_LosRegionsPerSide - 1),
			Clamp(z/LOS_REGION_RATIO, 0, m_LosRegionsPerSide - 1)
		};
	}

	LosRegion PosToLosRegionsHelper(entity_pos_t x, entity_pos_t z) const
	{
		u16 i = Clamp(
			(x/(LOS_TILE_SIZE*LOS_REGION_RATIO)).ToInt_RoundToZero(),
			0,
			m_LosRegionsPerSide - 1);
		u16 j = Clamp(
			(z/(LOS_TILE_SIZE*LOS_REGION_RATIO)).ToInt_RoundToZero(),
			0,
			m_LosRegionsPerSide - 1);
		return std::make_pair(i, j);
	}

	void AddToRegion(LosRegion region, entity_id_t ent)
	{
		m_LosRegions[region].insert(ent);
	}

	void RemoveFromRegion(LosRegion region, entity_id_t ent)
	{
		std::set<entity_id_t>::const_iterator regionIt = m_LosRegions[region].find(ent);
		if (regionIt != m_LosRegions[region].end())
			m_LosRegions[region].erase(regionIt);
	}

	void UpdateVisibilityData()
	{
		PROFILE("UpdateVisibilityData");

		for (u16 i = 0; i < m_LosRegionsPerSide; ++i)
			for (u16 j = 0; j < m_LosRegionsPerSide; ++j)
			{
				LosRegion pos{i, j};
				for (player_id_t player = 1; player < MAX_LOS_PLAYER_ID + 1; ++player)
					if (IsVisibilityDirty(m_DirtyVisibility[pos], player) || m_GlobalPlayerVisibilityUpdate[player-1] == 1 || m_GlobalVisibilityUpdate)
						for (const entity_id_t& ent : m_LosRegions[pos])
							UpdateVisibility(ent, player);

				m_DirtyVisibility[pos] = 0;
			}

		std::fill(m_GlobalPlayerVisibilityUpdate.begin(), m_GlobalPlayerVisibilityUpdate.end(), false);
		m_GlobalVisibilityUpdate = false;

		// Calling UpdateVisibility can modify m_ModifiedEntities, so be careful:
		// infinite loops could be triggered by feedback between entities and their mirages.
		std::map<entity_id_t, u8> attempts;
		while (!m_ModifiedEntities.empty())
		{
			entity_id_t ent = m_ModifiedEntities.back();
			m_ModifiedEntities.pop_back();

			++attempts[ent];
			ENSURE(attempts[ent] < 100 && "Infinite loop in UpdateVisibilityData");

			UpdateVisibility(ent);
		}
	}

	void RequestVisibilityUpdate(entity_id_t ent) override
	{
		if (std::find(m_ModifiedEntities.begin(), m_ModifiedEntities.end(), ent) == m_ModifiedEntities.end())
			m_ModifiedEntities.push_back(ent);
	}

	void UpdateVisibility(entity_id_t ent, player_id_t player)
	{
		EntityMap<EntityData>::iterator itEnts = m_EntityData.find(ent);
		if (itEnts == m_EntityData.end())
			return;

		LosVisibility oldVis = GetPlayerVisibility(itEnts->second.visibilities, player);
		LosVisibility newVis = ComputeLosVisibility(itEnts->first, player);

		if (oldVis == newVis)
			return;

		itEnts->second.visibilities = (itEnts->second.visibilities & ~(0x3 << 2 * (player - 1))) | ((u8)newVis << 2 * (player - 1));

		CMessageVisibilityChanged msg(player, ent, static_cast<int>(oldVis), static_cast<int>(newVis));
		GetSimContext().GetComponentManager().PostMessage(ent, msg);
	}

	void UpdateVisibility(entity_id_t ent)
	{
		for (player_id_t player = 1; player < MAX_LOS_PLAYER_ID + 1; ++player)
			UpdateVisibility(ent, player);
	}

	void SetLosRevealAll(player_id_t player, bool enabled) override
	{
		if (player == -1)
			m_LosRevealAll[MAX_LOS_PLAYER_ID+1] = enabled;
		else
		{
			ENSURE(player >= 0 && player <= MAX_LOS_PLAYER_ID);
			m_LosRevealAll[player] = enabled;
		}

		// On next update, update the visibility of every entity in the world
		m_GlobalVisibilityUpdate = true;
	}

	bool GetLosRevealAll(player_id_t player) const override
	{
		// Special player value can force reveal-all for every player
		if (m_LosRevealAll[MAX_LOS_PLAYER_ID+1] || player == -1)
			return true;
		ENSURE(player >= 0 && player <= MAX_LOS_PLAYER_ID+1);
		// Otherwise check the player-specific flag
		if (m_LosRevealAll[player])
			return true;

		return false;
	}

	void SetLosCircular(bool enabled) override
	{
		m_LosCircular = enabled;

		ResetDerivedData();
	}

	bool GetLosCircular() const override
	{
		return m_LosCircular;
	}

	void SetSharedLos(player_id_t player, const std::vector<player_id_t>& players) override
	{
		m_SharedLosMasks[player] = CalcSharedLosMask(players);

		// Units belonging to any of 'players' can now trigger visibility updates for 'player'.
		// If shared LOS partners have been removed, we disable visibility updates from them
		// in order to improve performance. That also allows us to properly determine whether
		// 'player' needs a global visibility update for this turn.
		bool modified = false;

		for (player_id_t p = 1; p < MAX_LOS_PLAYER_ID+1; ++p)
		{
			bool inList = std::find(players.begin(), players.end(), p) != players.end();

			if (SetPlayerSharedDirtyVisibilityBit(m_SharedDirtyVisibilityMasks[p], player, inList))
				modified = true;
		}

		if (modified && (size_t)player <= m_GlobalPlayerVisibilityUpdate.size())
			m_GlobalPlayerVisibilityUpdate[player-1] = 1;
	}

	u32 GetSharedLosMask(player_id_t player) const override
	{
		return m_SharedLosMasks[player];
	}

	void ExploreMap(player_id_t p) override
	{
		for (i32 j = 0; j < m_LosVerticesPerSide; ++j)
			for (i32 i = 0; i < m_LosVerticesPerSide; ++i)
			{
				if (LosIsOffWorld(i,j))
					continue;
				u32 &explored = m_ExploredVertices.at(p);
				explored += !(m_LosState.get(i, j) & ((u32)LosState::EXPLORED << (2*(p-1))));
				m_LosState.get(i, j) |= ((u32)LosState::EXPLORED << (2*(p-1)));
			}

		SeeExploredEntities(p);
	}

	void ExploreTerritories() override
	{
		PROFILE3("ExploreTerritories");

		CmpPtr<ICmpTerritoryManager> cmpTerritoryManager(GetSystemEntity());
		const Grid<u8>& grid = cmpTerritoryManager->GetTerritoryGrid();

		// Territory data is stored per territory-tile (typically a multiple of terrain-tiles).
		// LOS data is stored per los vertex (in reality tiles too, but it's the center that matters).
		// This scales from LOS coordinates to Territory coordinates.
		auto scale = [](i32 coord, i32 max) -> i32 {
			return std::min(max, (coord * LOS_TILE_SIZE + LOS_TILE_SIZE / 2) / (ICmpTerritoryManager::NAVCELLS_PER_TERRITORY_TILE * Pathfinding::NAVCELL_SIZE_INT));
		};

		// For each territory-tile, if it is owned by a valid player then update the LOS
		// for every vertex inside/around that tile, to mark them as explored.
		for (i32 j = 0; j < m_LosVerticesPerSide; ++j)
			for (i32 i = 0; i < m_LosVerticesPerSide; ++i)
			{
				// TODO: This fetches data redundantly if the los grid is smaller than the territory grid
				// (but it's unlikely to matter much).
				u8 p = grid.get(scale(i, grid.width() - 1), scale(j, grid.height() - 1)) & ICmpTerritoryManager::TERRITORY_PLAYER_MASK;
				if (p > 0 && p <= MAX_LOS_PLAYER_ID)
				{
					u32& explored = m_ExploredVertices.at(p);

					if (LosIsOffWorld(i, j))
						continue;

					u32& losState = m_LosState.get(i, j);
					if (!(losState & ((u32)LosState::EXPLORED << (2*(p-1)))))
					{
						++explored;
						losState |= ((u32)LosState::EXPLORED << (2*(p-1)));
					}
				}
			}

		for (player_id_t p = 1; p < MAX_LOS_PLAYER_ID+1; ++p)
			SeeExploredEntities(p);
	}

	/**
	 * Force any entity in explored territory to appear for player p.
	 * This is useful for miraging entities inside the territory borders at the beginning of a game,
	 * or if the "Explore Map" option has been set.
	 */
	void SeeExploredEntities(player_id_t p) const
	{
		// Warning: Code related to fogging (like ForceMiraging) shouldn't be
		// invoked while iterating through m_EntityData.
		// Otherwise, by deleting mirage entities and so on, that code will
		// change the indexes in the map, leading to segfaults.
		// So we just remember what entities to mirage and do that later.
		std::vector<entity_id_t> miragableEntities;

		for (EntityMap<EntityData>::const_iterator it = m_EntityData.begin(); it != m_EntityData.end(); ++it)
		{
			CmpPtr<ICmpPosition> cmpPosition(GetSimContext(), it->first);
			if (!cmpPosition || !cmpPosition->IsInWorld())
				continue;

			CFixedVector2D pos = cmpPosition->GetPosition2D();
			int i = (pos.X / LOS_TILE_SIZE).ToInt_RoundToNearest();
			int j = (pos.Y / LOS_TILE_SIZE).ToInt_RoundToNearest();

			CLosQuerier los(GetSharedLosMask(p), m_LosState, m_LosVerticesPerSide);
			if (!los.IsExplored(i,j) || los.IsVisible(i,j))
				continue;

			CmpPtr<ICmpFogging> cmpFogging(GetSimContext(), it->first);
			if (cmpFogging)
				miragableEntities.push_back(it->first);
		}

		for (std::vector<entity_id_t>::iterator it = miragableEntities.begin(); it != miragableEntities.end(); ++it)
		{
			CmpPtr<ICmpFogging> cmpFogging(GetSimContext(), *it);
			ENSURE(cmpFogging && "Impossible to retrieve Fogging component, previously achieved");
			cmpFogging->ForceMiraging(p);
		}
	}

	void RevealShore(player_id_t p, bool enable) override
	{
		if (p <= 0 || p > MAX_LOS_PLAYER_ID)
			return;

		// Maximum distance to the shore
		const u16 maxdist = 10;

		CmpPtr<ICmpPathfinder> cmpPathfinder(GetSystemEntity());
		const Grid<u16>& shoreGrid = cmpPathfinder->ComputeShoreGrid(true);
		ENSURE(shoreGrid.m_W == m_LosVerticesPerSide-1 && shoreGrid.m_H == m_LosVerticesPerSide-1);

		Grid<u16>& counts = m_LosPlayerCounts.at(p);
		ENSURE(!counts.blank());

		for (u16 j = 0; j < shoreGrid.m_H; ++j)
			for (u16 i = 0; i < shoreGrid.m_W; ++i)
			{
				u16 shoredist = shoreGrid.get(i, j);
				if (shoredist > maxdist)
					continue;

				// Maybe we could be more clever and don't add dummy strips of one tile
				if (enable)
					LosAddStripHelper(p, i, i, j, counts);
				else
					LosRemoveStripHelper(p, i, i, j, counts);
			}
	}

	/**
	 * Returns whether the given vertex is outside the normal bounds of the world
	 * (i.e. outside the range of a circular map)
	 */
	inline bool LosIsOffWorld(ssize_t i, ssize_t j) const
	{
		if (m_LosCircular)
		{
			// With a circular map, vertex is off-world if hypot(i - size/2, j - size/2) >= size/2:

			ssize_t dist2 = (i - m_LosVerticesPerSide/2)*(i - m_LosVerticesPerSide/2)
					+ (j - m_LosVerticesPerSide/2)*(j - m_LosVerticesPerSide/2);

			ssize_t r = m_LosVerticesPerSide / 2 - MAP_EDGE_TILES + 1;
				// subtract a bit from the radius to ensure nice
				// SoD blurring around the edges of the map

			return (dist2 >= r*r);
		}
		else
		{
			// With a square map, the outermost edge of the map should be off-world,
			// so the SoD texture blends out nicely
			return i < MAP_EDGE_TILES || j < MAP_EDGE_TILES ||
				i >= m_LosVerticesPerSide - MAP_EDGE_TILES ||
				j >= m_LosVerticesPerSide - MAP_EDGE_TILES;
		}
	}

	/**
	 * Update the LOS state of tiles within a given horizontal strip (i0,j) to (i1,j) (inclusive).
	 */
	inline void LosAddStripHelper(u8 owner, i32 i0, i32 i1, i32 j, Grid<u16>& counts)
	{
		if (i1 < i0)
			return;

		u32 &explored = m_ExploredVertices.at(owner);
		for (i32 i = i0; i <= i1; ++i)
		{
			// Increasing from zero to non-zero - move from unexplored/explored to visible+explored
			if (counts.get(i, j) == 0)
			{
				if (!LosIsOffWorld(i, j))
				{
					explored += !(m_LosState.get(i, j) & ((u32)LosState::EXPLORED << (2*(owner-1))));
					m_LosState.get(i, j) |= (((int)LosState::VISIBLE | (u32)LosState::EXPLORED) << (2*(owner-1)));
				}

				MarkVisibilityDirtyAroundTile(owner, i, j);
			}

			ENSURE(counts.get(i, j) < std::numeric_limits<u16>::max());
			counts.get(i, j) = (u16)(counts.get(i, j) + 1); // ignore overflow; the player should never have 64K units
		}
	}

	/**
	 * Update the LOS state of tiles within a given horizontal strip (i0,j) to (i1,j) (inclusive).
	 */
	inline void LosRemoveStripHelper(u8 owner, i32 i0, i32 i1, i32 j, Grid<u16>& counts)
	{
		if (i1 < i0)
			return;

		for (i32 i = i0; i <= i1; ++i)
		{
			ASSERT(counts.get(i, j) > 0);
			counts.get(i, j) = (u16)(counts.get(i, j) - 1);

			// Decreasing from non-zero to zero - move from visible+explored to explored
			if (counts.get(i, j) == 0)
			{
				// (If LosIsOffWorld then this is a no-op, so don't bother doing the check)
				m_LosState.get(i, j) &= ~((int)LosState::VISIBLE << (2*(owner-1)));

				MarkVisibilityDirtyAroundTile(owner, i, j);
			}
		}
	}

	inline void MarkVisibilityDirtyAroundTile(u8 owner, i32 i, i32 j)
	{
		// If we're still in the deserializing process, we must not modify m_DirtyVisibility
		if (m_Deserializing)
			return;

		// Mark the LoS regions around the updated vertex
		// 1: left-up, 2: right-up, 3: left-down, 4: right-down
		LosRegion n1 = LosVertexToLosRegionsHelper(i-1, j-1);
		LosRegion n2 = LosVertexToLosRegionsHelper(i-1, j);
		LosRegion n3 = LosVertexToLosRegionsHelper(i, j-1);
		LosRegion n4 = LosVertexToLosRegionsHelper(i, j);

		u16 sharedDirtyVisibilityMask = m_SharedDirtyVisibilityMasks[owner];

		if (j > 0 && i > 0)
			m_DirtyVisibility[n1] |= sharedDirtyVisibilityMask;
		if (n2 != n1 && j > 0 && i < m_LosVerticesPerSide)
			m_DirtyVisibility[n2] |= sharedDirtyVisibilityMask;
		if (n3 != n1 && j < m_LosVerticesPerSide && i > 0)
			m_DirtyVisibility[n3] |= sharedDirtyVisibilityMask;
		if (n4 != n1 && j < m_LosVerticesPerSide && i < m_LosVerticesPerSide)
			m_DirtyVisibility[n4] |= sharedDirtyVisibilityMask;
	}

	/**
	 * Update the LOS state of tiles within a given circular range,
	 * either adding or removing visibility depending on the template parameter.
	 * Assumes owner is in the valid range.
	 */
	template<bool adding>
	void LosUpdateHelper(u8 owner, entity_pos_t visionRange, CFixedVector2D pos)
	{
		if (m_LosVerticesPerSide == 0) // do nothing if not initialised yet
			return;

		PROFILE("LosUpdateHelper");

		Grid<u16>& counts = m_LosPlayerCounts.at(owner);

		// Lazy initialisation of counts:
		if (counts.blank())
			counts.resize(m_LosVerticesPerSide, m_LosVerticesPerSide);

		// Compute the circular region as a series of strips.
		// Rather than quantise pos to vertexes, we do more precise sub-tile computations
		// to get smoother behaviour as a unit moves rather than jumping a whole tile
		// at once.
		// To avoid the cost of sqrt when computing the outline of the circle,
		// we loop from the bottom to the top and estimate the width of the current
		// strip based on the previous strip, then adjust each end of the strip
		// inwards or outwards until it's the widest that still falls within the circle.

		// Compute top/bottom coordinates, and clamp to exclude the 1-tile border around the map
		// (so that we never render the sharp edge of the map)
		i32 j0 = ((pos.Y - visionRange)/LOS_TILE_SIZE).ToInt_RoundToInfinity();
		i32 j1 = ((pos.Y + visionRange)/LOS_TILE_SIZE).ToInt_RoundToNegInfinity();
		i32 j0clamp = std::max(j0, 1);
		i32 j1clamp = std::min(j1, m_LosVerticesPerSide-2);

		// Translate world coordinates into fractional tile-space coordinates
		entity_pos_t x = pos.X / LOS_TILE_SIZE;
		entity_pos_t y = pos.Y / LOS_TILE_SIZE;
		entity_pos_t r = visionRange / LOS_TILE_SIZE;
		entity_pos_t r2 = r.Square();

		// Compute the integers on either side of x
		i32 xfloor = (x - entity_pos_t::Epsilon()).ToInt_RoundToNegInfinity();
		i32 xceil = (x + entity_pos_t::Epsilon()).ToInt_RoundToInfinity();

		// Initialise the strip (i0, i1) to a rough guess
		i32 i0 = xfloor;
		i32 i1 = xceil;

		for (i32 j = j0clamp; j <= j1clamp; ++j)
		{
			// Adjust i0 and i1 to be the outermost values that don't exceed
			// the circle's radius (i.e. require dy^2 + dx^2 <= r^2).
			// When moving the points inwards, clamp them to xceil+1 or xfloor-1
			// so they don't accidentally shoot off in the wrong direction forever.

			entity_pos_t dy = entity_pos_t::FromInt(j) - y;
			entity_pos_t dy2 = dy.Square();
			while (dy2 + (entity_pos_t::FromInt(i0-1) - x).Square() <= r2)
				--i0;
			while (i0 < xceil && dy2 + (entity_pos_t::FromInt(i0) - x).Square() > r2)
				++i0;
			while (dy2 + (entity_pos_t::FromInt(i1+1) - x).Square() <= r2)
				++i1;
			while (i1 > xfloor && dy2 + (entity_pos_t::FromInt(i1) - x).Square() > r2)
				--i1;

#if DEBUG_RANGE_MANAGER_BOUNDS
			if (i0 <= i1)
			{
				ENSURE(dy2 + (entity_pos_t::FromInt(i0) - x).Square() <= r2);
				ENSURE(dy2 + (entity_pos_t::FromInt(i1) - x).Square() <= r2);
			}
			ENSURE(dy2 + (entity_pos_t::FromInt(i0 - 1) - x).Square() > r2);
			ENSURE(dy2 + (entity_pos_t::FromInt(i1 + 1) - x).Square() > r2);
#endif

			// Clamp the strip to exclude the 1-tile border,
			// then add or remove the strip as requested
			i32 i0clamp = std::max(i0, 1);
			i32 i1clamp = std::min(i1, m_LosVerticesPerSide-2);
			if (adding)
				LosAddStripHelper(owner, i0clamp, i1clamp, j, counts);
			else
				LosRemoveStripHelper(owner, i0clamp, i1clamp, j, counts);
		}
	}

	/**
	 * Update the LOS state of tiles within a given circular range,
	 * by removing visibility around the 'from' position
	 * and then adding visibility around the 'to' position.
	 */
	void LosUpdateHelperIncremental(u8 owner, entity_pos_t visionRange, CFixedVector2D from, CFixedVector2D to)
	{
		if (m_LosVerticesPerSide == 0) // do nothing if not initialised yet
			return;

		PROFILE("LosUpdateHelperIncremental");

		Grid<u16>& counts = m_LosPlayerCounts.at(owner);

		// Lazy initialisation of counts:
		if (counts.blank())
			counts.resize(m_LosVerticesPerSide, m_LosVerticesPerSide);

		// See comments in LosUpdateHelper.
		// This does exactly the same, except computing the strips for
		// both circles simultaneously.
		// (The idea is that the circles will be heavily overlapping,
		// so we can compute the difference between the removed/added strips
		// and only have to touch tiles that have a net change.)

		i32 j0_from = ((from.Y - visionRange)/LOS_TILE_SIZE).ToInt_RoundToInfinity();
		i32 j1_from = ((from.Y + visionRange)/LOS_TILE_SIZE).ToInt_RoundToNegInfinity();
		i32 j0_to = ((to.Y - visionRange)/LOS_TILE_SIZE).ToInt_RoundToInfinity();
		i32 j1_to = ((to.Y + visionRange)/LOS_TILE_SIZE).ToInt_RoundToNegInfinity();
		i32 j0clamp = std::max(std::min(j0_from, j0_to), 1);
		i32 j1clamp = std::min(std::max(j1_from, j1_to), m_LosVerticesPerSide-2);

		entity_pos_t x_from = from.X / LOS_TILE_SIZE;
		entity_pos_t y_from = from.Y / LOS_TILE_SIZE;
		entity_pos_t x_to = to.X / LOS_TILE_SIZE;
		entity_pos_t y_to = to.Y / LOS_TILE_SIZE;
		entity_pos_t r = visionRange / LOS_TILE_SIZE;
		entity_pos_t r2 = r.Square();

		i32 xfloor_from = (x_from - entity_pos_t::Epsilon()).ToInt_RoundToNegInfinity();
		i32 xceil_from = (x_from + entity_pos_t::Epsilon()).ToInt_RoundToInfinity();
		i32 xfloor_to = (x_to - entity_pos_t::Epsilon()).ToInt_RoundToNegInfinity();
		i32 xceil_to = (x_to + entity_pos_t::Epsilon()).ToInt_RoundToInfinity();

		i32 i0_from = xfloor_from;
		i32 i1_from = xceil_from;
		i32 i0_to = xfloor_to;
		i32 i1_to = xceil_to;

		for (i32 j = j0clamp; j <= j1clamp; ++j)
		{
			entity_pos_t dy_from = entity_pos_t::FromInt(j) - y_from;
			entity_pos_t dy2_from = dy_from.Square();
			while (dy2_from + (entity_pos_t::FromInt(i0_from-1) - x_from).Square() <= r2)
				--i0_from;
			while (i0_from < xceil_from && dy2_from + (entity_pos_t::FromInt(i0_from) - x_from).Square() > r2)
				++i0_from;
			while (dy2_from + (entity_pos_t::FromInt(i1_from+1) - x_from).Square() <= r2)
				++i1_from;
			while (i1_from > xfloor_from && dy2_from + (entity_pos_t::FromInt(i1_from) - x_from).Square() > r2)
				--i1_from;

			entity_pos_t dy_to = entity_pos_t::FromInt(j) - y_to;
			entity_pos_t dy2_to = dy_to.Square();
			while (dy2_to + (entity_pos_t::FromInt(i0_to-1) - x_to).Square() <= r2)
				--i0_to;
			while (i0_to < xceil_to && dy2_to + (entity_pos_t::FromInt(i0_to) - x_to).Square() > r2)
				++i0_to;
			while (dy2_to + (entity_pos_t::FromInt(i1_to+1) - x_to).Square() <= r2)
				++i1_to;
			while (i1_to > xfloor_to && dy2_to + (entity_pos_t::FromInt(i1_to) - x_to).Square() > r2)
				--i1_to;

#if DEBUG_RANGE_MANAGER_BOUNDS
			if (i0_from <= i1_from)
			{
				ENSURE(dy2_from + (entity_pos_t::FromInt(i0_from) - x_from).Square() <= r2);
				ENSURE(dy2_from + (entity_pos_t::FromInt(i1_from) - x_from).Square() <= r2);
			}
			ENSURE(dy2_from + (entity_pos_t::FromInt(i0_from - 1) - x_from).Square() > r2);
			ENSURE(dy2_from + (entity_pos_t::FromInt(i1_from + 1) - x_from).Square() > r2);
			if (i0_to <= i1_to)
			{
				ENSURE(dy2_to + (entity_pos_t::FromInt(i0_to) - x_to).Square() <= r2);
				ENSURE(dy2_to + (entity_pos_t::FromInt(i1_to) - x_to).Square() <= r2);
			}
			ENSURE(dy2_to + (entity_pos_t::FromInt(i0_to - 1) - x_to).Square() > r2);
			ENSURE(dy2_to + (entity_pos_t::FromInt(i1_to + 1) - x_to).Square() > r2);
#endif

			// Check whether this strip moved at all
			if (!(i0_to == i0_from && i1_to == i1_from))
			{
				i32 i0clamp_from = std::max(i0_from, 1);
				i32 i1clamp_from = std::min(i1_from, m_LosVerticesPerSide-2);
				i32 i0clamp_to = std::max(i0_to, 1);
				i32 i1clamp_to = std::min(i1_to, m_LosVerticesPerSide-2);

				// Check whether one strip is negative width,
				// and we can just add/remove the entire other strip
				if (i1clamp_from < i0clamp_from)
				{
					LosAddStripHelper(owner, i0clamp_to, i1clamp_to, j, counts);
				}
				else if (i1clamp_to < i0clamp_to)
				{
					LosRemoveStripHelper(owner, i0clamp_from, i1clamp_from, j, counts);
				}
				else
				{
					// There are four possible regions of overlap between the two strips
					// (remove before add, remove after add, add before remove, add after remove).
					// Process each of the regions as its own strip.
					// (If this produces negative-width strips then they'll just get ignored
					// which is fine.)
					// (If the strips don't actually overlap (which is very rare with normal unit
					// movement speeds), the region between them will be both added and removed,
					// so we have to do the add first to avoid overflowing to -1 and triggering
					// assertion failures.)
					LosAddStripHelper(owner, i0clamp_to, i0clamp_from-1, j, counts);
					LosAddStripHelper(owner, i1clamp_from+1, i1clamp_to, j, counts);
					LosRemoveStripHelper(owner, i0clamp_from, i0clamp_to-1, j, counts);
					LosRemoveStripHelper(owner, i1clamp_to+1, i1clamp_from, j, counts);
				}
			}
		}
	}

	void LosAdd(player_id_t owner, entity_pos_t visionRange, CFixedVector2D pos)
	{
		if (visionRange.IsZero() || owner <= 0 || owner > MAX_LOS_PLAYER_ID)
			return;

		LosUpdateHelper<true>((u8)owner, visionRange, pos);
	}

	void SharingLosAdd(u16 visionSharing, entity_pos_t visionRange, CFixedVector2D pos)
	{
		if (visionRange.IsZero())
			return;

		for (player_id_t i = 1; i < MAX_LOS_PLAYER_ID+1; ++i)
			if (HasVisionSharing(visionSharing, i))
				LosAdd(i, visionRange, pos);
	}

	void LosRemove(player_id_t owner, entity_pos_t visionRange, CFixedVector2D pos)
	{
		if (visionRange.IsZero() || owner <= 0 || owner > MAX_LOS_PLAYER_ID)
			return;

		LosUpdateHelper<false>((u8)owner, visionRange, pos);
	}

	void SharingLosRemove(u16 visionSharing, entity_pos_t visionRange, CFixedVector2D pos)
	{
		if (visionRange.IsZero())
			return;

		for (player_id_t i = 1; i < MAX_LOS_PLAYER_ID+1; ++i)
			if (HasVisionSharing(visionSharing, i))
				LosRemove(i, visionRange, pos);
	}

	void LosMove(player_id_t owner, entity_pos_t visionRange, CFixedVector2D from, CFixedVector2D to)
	{
		if (visionRange.IsZero() || owner <= 0 || owner > MAX_LOS_PLAYER_ID)
			return;

		if ((from - to).CompareLength(visionRange) > 0)
		{
			// If it's a very large move, then simply remove and add to the new position
			LosUpdateHelper<false>((u8)owner, visionRange, from);
			LosUpdateHelper<true>((u8)owner, visionRange, to);
		}
		else
			// Otherwise use the version optimised for mostly-overlapping circles
			LosUpdateHelperIncremental((u8)owner, visionRange, from, to);
	}

	void SharingLosMove(u16 visionSharing, entity_pos_t visionRange, CFixedVector2D from, CFixedVector2D to)
	{
		if (visionRange.IsZero())
			return;

		for (player_id_t i = 1; i < MAX_LOS_PLAYER_ID+1; ++i)
			if (HasVisionSharing(visionSharing, i))
				LosMove(i, visionRange, from, to);
	}

	u8 GetPercentMapExplored(player_id_t player) const override
	{
		return m_ExploredVertices.at((u8)player) * 100 / m_TotalInworldVertices;
	}

	u8 GetUnionPercentMapExplored(const std::vector<player_id_t>& players) const override
	{
		u32 exploredVertices = 0;
		std::vector<player_id_t>::const_iterator playerIt;

		for (i32 j = 0; j < m_LosVerticesPerSide; j++)
			for (i32 i = 0; i < m_LosVerticesPerSide; i++)
			{
				if (LosIsOffWorld(i, j))
					continue;

				for (playerIt = players.begin(); playerIt != players.end(); ++playerIt)
					if (m_LosState.get(i, j) & ((u32)LosState::EXPLORED << (2*((*playerIt)-1))))
					{
						exploredVertices += 1;
						break;
					}
			}

		return exploredVertices * 100 / m_TotalInworldVertices;
	}
};

REGISTER_COMPONENT_TYPE(RangeManager)
