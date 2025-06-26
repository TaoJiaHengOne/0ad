/* Copyright (C) 2025 Wildfire Games.
 * 本文件是 0 A.D. 的一部分。
 *
 * 0 A.D. 是自由软件：您可以根据自由软件基金会发布的 GNU 通用公共许可证
 * (GNU General Public License) 的条款（许可证的第 2 版或您选择的任何更新版本）
 * 对其进行再分发和/或修改。
 *
 * 0 A.D. 的分发是希望它能有用，但没有任何担保；甚至没有对“适销性”或
 * “特定用途适用性”的默示担保。详见 GNU 通用公共许可证。
 *
 * 您应该已经随 0 A.D. 收到了一份 GNU 通用公共许可证的副本。
 * 如果没有，请查阅 <http://www.gnu.org/licenses/>。
 */

 /**
  * @file
  * CCmpTerritoryManager 的通用代码和设置代码。
  */

#include "precompiled.h"

#include "simulation2/system/Component.h"
#include "ICmpTerritoryManager.h"

#include "graphics/Overlay.h"
#include "graphics/Terrain.h"
#include "graphics/TextureManager.h"
#include "graphics/TerritoryBoundary.h"
#include "maths/MathUtil.h"
#include "ps/Profile.h"
#include "ps/XML/Xeromyces.h"
#include "renderer/Renderer.h"
#include "renderer/Scene.h"
#include "renderer/TerrainOverlay.h"
#include "simulation2/MessageTypes.h"
#include "simulation2/components/ICmpOwnership.h"
#include "simulation2/components/ICmpPathfinder.h"
#include "simulation2/components/ICmpPlayer.h"
#include "simulation2/components/ICmpPlayerManager.h"
#include "simulation2/components/ICmpPosition.h"
#include "simulation2/components/ICmpTerritoryDecayManager.h"
#include "simulation2/components/ICmpTerritoryInfluence.h"
#include "simulation2/helpers/Grid.h"
#include "simulation2/helpers/Render.h"

#include <queue>

  // 前向声明领土管理器实现类
class CCmpTerritoryManager;

/**
 * 领土调试覆盖层类，用于在地图上渲染领土归属
 */
class TerritoryOverlay final : public TerrainTextureOverlay
{
	NONCOPYABLE(TerritoryOverlay); // 使该类不可拷贝
public:
	CCmpTerritoryManager& m_TerritoryManager; // 领土管理器引用

	// 构造函数
	TerritoryOverlay(CCmpTerritoryManager& manager);
	// 构建 RGBA 纹理数据
	void BuildTextureRGBA(u8* data, size_t w, size_t h) override;
};

/**
 * 领土管理器组件的实现类
 */
class CCmpTerritoryManager : public ICmpTerritoryManager
{
public:
	// 静态类初始化函数，用于订阅消息
	static void ClassInit(CComponentManager& componentManager)
	{
		componentManager.SubscribeGloballyToMessageType(MT_OwnershipChanged); // 订阅全局归属权变更消息
		componentManager.SubscribeGloballyToMessageType(MT_PlayerColorChanged); // 订阅全局玩家颜色变更消息
		componentManager.SubscribeGloballyToMessageType(MT_PositionChanged); // 订阅全局位置变更消息
		componentManager.SubscribeGloballyToMessageType(MT_ValueModification); // 订阅全局数值修改消息
		componentManager.SubscribeToMessageType(MT_ObstructionMapShapeChanged); // 订阅障碍物地图形状变更消息
		componentManager.SubscribeToMessageType(MT_TerrainChanged); // 订阅地形变更消息
		componentManager.SubscribeToMessageType(MT_WaterChanged);   // 订阅水体变更消息
		componentManager.SubscribeToMessageType(MT_Update); // 订阅更新消息
		componentManager.SubscribeToMessageType(MT_Interpolate); // 订阅插值消息
		componentManager.SubscribeToMessageType(MT_RenderSubmit); // 订阅渲染提交消息
	}

	// 使用默认的组件分配器
	DEFAULT_COMPONENT_ALLOCATOR(TerritoryManager)

		// 获取组件的 Schema 定义
		static std::string GetSchema()
	{
		return "<a:component type='system'/><empty/>";
	}

	u8 m_ImpassableCost;      // 不可通行地块的成本
	float m_BorderThickness;  // 边界线厚度
	float m_BorderSeparation; // 边界线间距

	// 玩家 ID 存储在 0-4 位 (TERRITORY_PLAYER_MASK)
	// 已连接标志在第 5 位 (TERRITORY_CONNECTED_MASK)
	// 闪烁标志在第 6 位 (TERRITORY_BLINKING_MASK)
	// 已处理标志在第 7 位 (TERRITORY_PROCESSED_MASK)
	Grid<u8>* m_Territories; // 存储领土信息的格子图

	std::vector<u16> m_TerritoryCellCounts; // 每个玩家拥有的领土地块数量
	u16 m_TerritoryTotalPassableCellCount; // 可通行的领土地块总数

	// 保存每个地块的成本（用于阻止在不可通行的地块上建立领土）
	Grid<u8>* m_CostGrid;

	// 当领土变化时设为 true；将在 Update 阶段发送一个 TerritoriesChanged 消息
	bool m_TriggerEvent;

	// 边界线结构体
	struct SBoundaryLine
	{
		bool blinking; // 是否闪烁
		player_id_t owner; // 所有者
		CColor color; // 颜色
		SOverlayTexturedLine overlay; // 渲染用的覆盖层
	};

	std::vector<SBoundaryLine> m_BoundaryLines; // 边界线列表
	bool m_BoundaryLinesDirty; // 边界线是否需要重新计算的标志

	double m_AnimTime; // 从渲染开始经过的时间，单位秒

	TerritoryOverlay* m_DebugOverlay; // 调试覆盖层指针

	bool m_EnableLineDebugOverlays; ///< 是否为边界线启用节点调试覆盖层？
	std::vector<SOverlayLine> m_DebugBoundaryLineNodes; // 调试用的边界线节点

	// 组件初始化
	void Init(const CParamNode&) override
	{
		m_Territories = NULL;
		m_CostGrid = NULL;
		m_DebugOverlay = NULL;
		//		m_DebugOverlay = new TerritoryOverlay(*this); // 调试覆盖层可以按需启用
		m_BoundaryLinesDirty = true;
		m_TriggerEvent = true;
		m_EnableLineDebugOverlays = false;
		m_DirtyID = 1;
		m_DirtyBlinkingID = 1;
		m_ColorChanged = false;

		m_AnimTime = 0.0;

		m_TerritoryTotalPassableCellCount = 0;

		// 注册 Relax NG 验证器
		g_Xeromyces.AddValidator(g_VFS, "territorymanager", "simulation/data/territorymanager.rng");

		// 加载XML配置文件
		CParamNode externalParamNode;
		CParamNode::LoadXML(externalParamNode, L"simulation/data/territorymanager.xml", "territorymanager");

		int impassableCost = externalParamNode.GetChild("TerritoryManager").GetChild("ImpassableCost").ToInt();
		ENSURE(0 <= impassableCost && impassableCost <= 255);
		m_ImpassableCost = (u8)impassableCost;

		const std::string& visibilityStatus = externalParamNode.GetChild("TerritoryManager").GetChild("VisibilityStatus").ToString();
		m_Enabled = visibilityStatus != "off";
		m_Visible = m_Enabled && visibilityStatus == "visible";

		m_BorderThickness = externalParamNode.GetChild("TerritoryManager").GetChild("BorderThickness").ToFixed().ToFloat();
		m_BorderSeparation = externalParamNode.GetChild("TerritoryManager").GetChild("BorderSeparation").ToFixed().ToFloat();
	}

	// 组件销毁
	void Deinit() override
	{
		SAFE_DELETE(m_Territories);
		SAFE_DELETE(m_CostGrid);
		SAFE_DELETE(m_DebugOverlay);
	}

	// 序列化
	void Serialize(ISerializer& serialize) override
	{
		// 领土状态可以根据需要重新计算，所以我们不需要序列化任何相关数据。
		serialize.Bool("trigger event", m_TriggerEvent);
	}

	// 反序列化
	void Deserialize(const CParamNode& paramNode, IDeserializer& deserialize) override
	{
		Init(paramNode);
		deserialize.Bool("trigger event", m_TriggerEvent);
	}

	// 消息处理
	void HandleMessage(const CMessage& msg, bool UNUSED(global)) override
	{
		switch (msg.GetType())
		{
		case MT_OwnershipChanged: // 归属权变更
		{
			const CMessageOwnershipChanged& msgData = static_cast<const CMessageOwnershipChanged&> (msg);
			MakeDirtyIfRelevantEntity(msgData.entity);
			break;
		}
		case MT_PlayerColorChanged: // 玩家颜色变更
		{
			MakeDirty();
			break;
		}
		case MT_PositionChanged: // 位置变更
		{
			const CMessagePositionChanged& msgData = static_cast<const CMessagePositionChanged&> (msg);
			MakeDirtyIfRelevantEntity(msgData.entity);
			break;
		}
		case MT_ValueModification: // 数值修改
		{
			const CMessageValueModification& msgData = static_cast<const CMessageValueModification&> (msg);
			if (msgData.component == L"TerritoryInfluence")
				MakeDirty();
			break;
		}
		case MT_ObstructionMapShapeChanged: // 障碍物地图形状变更
		case MT_TerrainChanged: // 地形变更
		case MT_WaterChanged: // 水体变更
		{
			// 同时重新计算成本网格以支持atlas的更改
			SAFE_DELETE(m_CostGrid);
			MakeDirty();
			break;
		}
		case MT_Update: // 更新
		{
			if (m_TriggerEvent)
			{
				m_TriggerEvent = false;
				GetSimContext().GetComponentManager().BroadcastMessage(CMessageTerritoriesChanged());
			}
			break;
		}
		case MT_Interpolate: // 插值
		{
			const CMessageInterpolate& msgData = static_cast<const CMessageInterpolate&> (msg);
			Interpolate(msgData.deltaSimTime, msgData.offset);
			break;
		}
		case MT_RenderSubmit: // 渲染提交
		{
			const CMessageRenderSubmit& msgData = static_cast<const CMessageRenderSubmit&> (msg);
			RenderSubmit(msgData.collector, msgData.frustum, msgData.culling);
			break;
		}
		}
	}

	// 检查实体是否为聚落或领土影响源；
	// 忽略任何其他实体
	void MakeDirtyIfRelevantEntity(entity_id_t ent)
	{
		CmpPtr<ICmpTerritoryInfluence> cmpTerritoryInfluence(GetSimContext(), ent);
		if (cmpTerritoryInfluence)
			MakeDirty();
	}

	// 获取领土格子图
	const Grid<u8>& GetTerritoryGrid() override
	{
		CalculateTerritories();
		ENSURE(m_Territories);
		return *m_Territories;
	}

	// 获取指定位置的领土所有者
	player_id_t GetOwner(entity_pos_t x, entity_pos_t z) override;
	// 获取邻近地块信息
	std::vector<u32> GetNeighbours(entity_pos_t x, entity_pos_t z, bool filterConnected) override;
	// 检查领土是否已连接
	bool IsConnected(entity_pos_t x, entity_pos_t z) override;

	// 设置领土闪烁状态
	void SetTerritoryBlinking(entity_pos_t x, entity_pos_t z, bool enable) override;
	// 检查领土是否在闪烁
	bool IsTerritoryBlinking(entity_pos_t x, entity_pos_t z) override;

	// 为支持领土渲染数据的延迟更新，
	// 我们在这里维护一个 DirtyID，并在领土变化时递增它；
	// 如果调用者的 DirtyID 较低，则它需要更新。
	// 我们也对闪烁更新使用 DirtyBlinkingID 做同样的事情。

	size_t m_DirtyID; // 领土“脏”状态ID
	size_t m_DirtyBlinkingID; // 领土闪烁“脏”状态ID

	bool m_ColorChanged; // 颜色是否变更的标志

	// 将领土标记为“脏”，需要重新计算
	void MakeDirty()
	{
		SAFE_DELETE(m_Territories);
		++m_DirtyID;
		m_BoundaryLinesDirty = true;
		m_TriggerEvent = true;
	}

	// 检查纹理是否需要更新
	bool NeedUpdateTexture(size_t* dirtyID) override
	{
		if (*dirtyID == m_DirtyID && !m_ColorChanged)
			return false;

		*dirtyID = m_DirtyID;
		m_ColorChanged = false;
		return true;
	}

	// 检查AI是否需要更新
	bool NeedUpdateAI(size_t* dirtyID, size_t* dirtyBlinkingID) const override
	{
		if (*dirtyID == m_DirtyID && *dirtyBlinkingID == m_DirtyBlinkingID)
			return false;

		*dirtyID = m_DirtyID;
		*dirtyBlinkingID = m_DirtyBlinkingID;
		return true;
	}

	// 计算成本格子图
	void CalculateCostGrid();

	// 计算领土
	void CalculateTerritories();

	// 获取领土百分比
	u8 GetTerritoryPercentage(player_id_t player) override;

	// 计算边界线
	std::vector<STerritoryBoundary> ComputeBoundaries();

	// 更新边界线
	void UpdateBoundaryLines();

	// 插值计算
	void Interpolate(float frameTime, float frameOffset);

	// 提交渲染任务
	void RenderSubmit(SceneCollector& collector, const CFrustum& frustum, bool culling);

	// 设置可见性
	void SetVisibility(bool visible) override
	{
		if (!m_Enabled)
			return;

		m_Visible = visible;
	}

	// 检查是否可见
	bool IsVisible() const override
	{
		return m_Enabled && m_Visible;
	}

	// 更新颜色
	void UpdateColors() override;

private:
	bool m_Visible; // 是否可见
	bool m_Enabled; // 是否启用
};

// 再次注册组件类型（可能是为了确保链接）
REGISTER_COMPONENT_TYPE(TerritoryManager)

// 地块数据类型，用于更方便地访问坐标
struct Tile
{
	Tile(u16 i, u16 j) : x(i), z(j) { }
	u16 x, z;
};

/**
 * 基于队列的八方向泛洪填充算法。
 *
 * @param origin 泛洪填充的起始点。在第一次迭代中，它
 *	作为第二个参数传递给 @see decider，并且只有当调用返回
 *	@c true 时，泛洪填充才会继续。
 * @param gridSize 边界外的地块永远不会被扩展。
 *	@see decider 不会用这些地块来调用。
 * @param decider 它被调用时，第一个参数是已经添加的地块，第二个参数
 *	是其邻居。调用应返回是否将波前扩展到该邻居（如果总是
 *	返回 @c true，将发生无限循环）。在第一次迭代中，
 *	@see decider 被调用时，第一个参数为 nullptr，第二个参数为
 *	@see origin。
 */
template<typename Decider>
void Floodfill(const Tile& origin, const Tile& gridSize, Decider decider)
{
	static_assert(std::is_invocable_r_v<bool, Decider, const Tile*, const Tile&>);

	constexpr std::array<std::array<int, 2>, 8> neighbours{ {{1, 0}, {-1, 0}, {0, 1}, {0, -1}, {1, 1},
		{-1, -1}, {1, -1}, {-1, 1}} };
	std::queue<Tile> openTiles;

	const auto emplaceIfRequested = [decider = std::move(decider), &openTiles](
		const Tile* currentTile, const Tile& neighbourTile)
		{
			if (decider(currentTile, neighbourTile))
				openTiles.emplace(neighbourTile);
		};

	emplaceIfRequested(nullptr, origin);
	while (!openTiles.empty())
	{
		const Tile currentTile{ openTiles.front() };
		openTiles.pop();
		for (const std::array<int, 2>&neighbour : neighbours)
		{
			const Tile neighbourTile{ static_cast<u16>(currentTile.x + std::get<0>(neighbour)),
				static_cast<u16>(currentTile.z + std::get<1>(neighbour)) };

			// 检查边界，下溢将导致值再次变大。
			if (neighbourTile.x < gridSize.x && neighbourTile.z < gridSize.z)
				emplaceIfRequested(&currentTile, neighbourTile);
		}
	}
}

/**
 * 计算离给定点最近的网格上的地块索引
 */
static void NearestTerritoryTile(entity_pos_t x, entity_pos_t z, u16& i, u16& j, u16 w, u16 h)
{
	entity_pos_t scale = Pathfinding::NAVCELL_SIZE * ICmpTerritoryManager::NAVCELLS_PER_TERRITORY_TILE;
	i = Clamp((x / scale).ToInt_RoundToNegInfinity(), 0, w - 1);
	j = Clamp((z / scale).ToInt_RoundToNegInfinity(), 0, h - 1);
}

// 计算成本格子图
void CCmpTerritoryManager::CalculateCostGrid()
{
	if (m_CostGrid)
		return;

	CmpPtr<ICmpPathfinder> cmpPathfinder(GetSystemEntity());
	if (!cmpPathfinder)
		return;

	pass_class_t passClassTerritory = cmpPathfinder->GetPassabilityClass("default-terrain-only");
	pass_class_t passClassUnrestricted = cmpPathfinder->GetPassabilityClass("unrestricted");

	const Grid<NavcellData>& passGrid = cmpPathfinder->GetPassabilityGrid();

	int tilesW = passGrid.m_W / NAVCELLS_PER_TERRITORY_TILE;
	int tilesH = passGrid.m_H / NAVCELLS_PER_TERRITORY_TILE;

	m_CostGrid = new Grid<u8>(tilesW, tilesH);
	m_TerritoryTotalPassableCellCount = 0;

	for (int i = 0; i < tilesW; ++i)
	{
		for (int j = 0; j < tilesH; ++j)
		{
			NavcellData c = 0;
			for (u16 di = 0; di < NAVCELLS_PER_TERRITORY_TILE; ++di)
				for (u16 dj = 0; dj < NAVCELLS_PER_TERRITORY_TILE; ++dj)
					c |= passGrid.get(
						i * NAVCELLS_PER_TERRITORY_TILE + di,
						j * NAVCELLS_PER_TERRITORY_TILE + dj);
			if (!IS_PASSABLE(c, passClassTerritory))
				m_CostGrid->set(i, j, m_ImpassableCost);
			else if (!IS_PASSABLE(c, passClassUnrestricted))
				m_CostGrid->set(i, j, 255); // 在世界之外；使用最大成本
			else
			{
				m_CostGrid->set(i, j, 1);
				++m_TerritoryTotalPassableCellCount;
			}
		}
	}
}

// 计算领土归属
void CCmpTerritoryManager::CalculateTerritories()
{
	if (m_Territories)
		return;

	PROFILE("CalculateTerritories");

	// 如果寻路器尚未加载（例如，在地图初始化期间调用此函数），
	// 则中止计算（并假设调用者可以处理 m_Territories == NULL 的情况）
	CalculateCostGrid();
	if (!m_CostGrid)
		return;

	const u16 tilesW = m_CostGrid->m_W;
	const u16 tilesH = m_CostGrid->m_H;

	m_Territories = new Grid<u8>(tilesW, tilesH);

	// 重置所有玩家的领土计数
	CmpPtr<ICmpPlayerManager> cmpPlayerManager(GetSystemEntity());
	if (cmpPlayerManager && (size_t)cmpPlayerManager->GetNumPlayers() != m_TerritoryCellCounts.size())
		m_TerritoryCellCounts.resize(cmpPlayerManager->GetNumPlayers());
	for (u16& count : m_TerritoryCellCounts)
		count = 0;

	// 找到所有领土影响实体
	CComponentManager::InterfaceList influences = GetSimContext().GetComponentManager().GetEntitiesWithInterface(IID_TerritoryInfluence);

	// 将影响实体分成每个玩家的列表，忽略任何具有无效属性的实体
	std::map<player_id_t, std::vector<entity_id_t> > influenceEntities;
	for (const CComponentManager::InterfacePair& pair : influences)
	{
		entity_id_t ent = pair.first;

		CmpPtr<ICmpOwnership> cmpOwnership(GetSimContext(), ent);
		if (!cmpOwnership)
			continue;

		// 忽略盖亚和未分配或我们无法表示的玩家
		player_id_t owner = cmpOwnership->GetOwner();
		if (owner <= 0 || owner > TERRITORY_PLAYER_MASK)
			continue;

		influenceEntities[owner].push_back(ent);
	}

	// 存储整体最佳权重以供比较
	Grid<u32> bestWeightGrid(tilesW, tilesH);
	// 存储根影响源以将领土标记为已连接
	std::vector<entity_id_t> rootInfluenceEntities;

	for (const std::pair<const player_id_t, std::vector<entity_id_t>>& pair : influenceEntities)
	{
		// entityGrid 存储单个实体的权重，并为每个实体重置
		Grid<u32> entityGrid(tilesW, tilesH);
		// playerGrid 存储此玩家所有实体的组合权重
		Grid<u32> playerGrid(tilesW, tilesH);

		u8 owner = static_cast<u8>(pair.first);
		const std::vector<entity_id_t>& ents = pair.second;
		// 对于 2^16 个实体，我们不会溢出，因为权重也限制在 2^16
		ENSURE(ents.size() < 1 << 16);
		// 计算当前实体的影响图，然后将其添加到玩家网格中
		for (entity_id_t ent : ents)
		{
			CmpPtr<ICmpPosition> cmpPosition(GetSimContext(), ent);
			if (!cmpPosition || !cmpPosition->IsInWorld())
				continue;

			CmpPtr<ICmpTerritoryInfluence> cmpTerritoryInfluence(GetSimContext(), ent);
			const u32 originWeight = cmpTerritoryInfluence->GetWeight();
			u32 radius = cmpTerritoryInfluence->GetRadius();
			if (originWeight == 0 || radius == 0)
				continue;
			const u32 relativeFalloff = originWeight *
				(Pathfinding::NAVCELL_SIZE * NAVCELLS_PER_TERRITORY_TILE)
				.ToInt_RoundToNegInfinity() / radius;

			CFixedVector2D pos = cmpPosition->GetPosition2D();
			u16 i, j;
			NearestTerritoryTile(pos.X, pos.Y, i, j, tilesW, tilesH);

			if (cmpTerritoryInfluence->IsRoot())
				rootInfluenceEntities.push_back(ent);

			// 向外扩展影响
			Floodfill({ i, j }, { tilesW, tilesH }, [&](const Tile* current, const Tile& neighbour)
				{
					const bool diagonalProgression{ current && neighbour.x != current->x &&
						neighbour.z != current->z };

					const u32 falloffPerTile{ relativeFalloff *
						m_CostGrid->get(neighbour.x, neighbour.z) };
					// 对角线邻居 -> 乘以约 sqrt(2)
					const u32 falloff{ diagonalProgression ? (falloffPerTile * 362) / 256 :
						falloffPerTile };

					// 如果新成本不优于该地块的先前值，则不扩展
					// （经过安排以避免在 entityGrid.get(x, z) < falloff 时下溢）
					if (current &&
						entityGrid.get(current->x, current->z) <=
						entityGrid.get(neighbour.x, neighbour.z) + falloff)
					{
						return false;
					}

					// 此地块的权重 = 前驱的权重 - 从前驱的衰减
					const u32 weight{ current ? entityGrid.get(current->x, current->z) - falloff :
						originWeight };
					const u32 totalWeight{ weight + (current ?
						playerGrid.get(neighbour.x, neighbour.z) -
						entityGrid.get(neighbour.x, neighbour.z) : 0) };

					playerGrid.set(neighbour.x, neighbour.z, totalWeight);
					entityGrid.set(neighbour.x, neighbour.z, weight);
					// 如果此权重优于迄今为止的最佳权重，则设置所有者
					if (totalWeight > bestWeightGrid.get(neighbour.x, neighbour.z))
					{
						bestWeightGrid.set(neighbour.x, neighbour.z, totalWeight);
						m_Territories->set(neighbour.x, neighbour.z, owner);
					}
					return true;
				});
			entityGrid.reset();
		}
	}

	// 检测连接到其玩家所属的“根”影响源（通常是市政中心）的领土，
	// 并用已连接标志标记它们
	for (entity_id_t ent : rootInfluenceEntities)
	{
		// （这些组件必须有效，否则实体不会被添加到此列表中）
		CmpPtr<ICmpOwnership> cmpOwnership(GetSimContext(), ent);
		CmpPtr<ICmpPosition> cmpPosition(GetSimContext(), ent);

		CFixedVector2D pos = cmpPosition->GetPosition2D();
		u16 i, j;
		NearestTerritoryTile(pos.X, pos.Y, i, j, tilesW, tilesH);

		u8 owner = (u8)cmpOwnership->GetOwner();

		Floodfill({ i, j }, { tilesW, tilesH }, [&](const Tile*, const Tile& neighbour)
			{
				// 不扩展非所有者地块，或已经具有已连接掩码的地块
				if (m_Territories->get(neighbour.x, neighbour.z) != owner)
					return false;
				m_Territories->set(neighbour.x, neighbour.z, owner | TERRITORY_CONNECTED_MASK);
				if (m_CostGrid->get(neighbour.x, neighbour.z) < m_ImpassableCost)
					++m_TerritoryCellCounts[owner];
				return true;
			});
	}

	// 然后重新计算闪烁的地块
	CmpPtr<ICmpTerritoryDecayManager> cmpTerritoryDecayManager(GetSystemEntity());
	if (cmpTerritoryDecayManager)
	{
		size_t dirtyBlinkingID = m_DirtyBlinkingID;
		cmpTerritoryDecayManager->SetBlinkingEntities();
		m_DirtyBlinkingID = dirtyBlinkingID;
	}
}

// 计算边界线
std::vector<STerritoryBoundary> CCmpTerritoryManager::ComputeBoundaries()
{
	PROFILE("ComputeBoundaries");

	CalculateTerritories();
	ENSURE(m_Territories);

	return CTerritoryBoundaryCalculator::ComputeBoundaries(m_Territories);
}

// 获取领土百分比
u8 CCmpTerritoryManager::GetTerritoryPercentage(player_id_t player)
{
	if (player <= 0 || (m_Territories && static_cast<size_t>(player) >= m_TerritoryCellCounts.size()))
		return 0;

	CalculateTerritories();

	// 领土可能已被重新计算，检查玩家是否仍然存在。
	if (m_TerritoryTotalPassableCellCount == 0 || static_cast<size_t>(player) >= m_TerritoryCellCounts.size())
		return 0;

	u8 percentage = (m_TerritoryCellCounts[player] * 100) / m_TerritoryTotalPassableCellCount;
	ENSURE(percentage <= 100);
	return percentage;
}

// 更新边界线
void CCmpTerritoryManager::UpdateBoundaryLines()
{
	PROFILE("update boundary lines");

	m_BoundaryLines.clear();
	m_DebugBoundaryLineNodes.clear();

	if (!CRenderer::IsInitialised())
		return;

	std::vector<STerritoryBoundary> boundaries = ComputeBoundaries();

	CTextureProperties texturePropsBase("art/textures/misc/territory_border.png");
	texturePropsBase.SetAddressMode(
		Renderer::Backend::Sampler::AddressMode::CLAMP_TO_BORDER,
		Renderer::Backend::Sampler::AddressMode::CLAMP_TO_EDGE);
	texturePropsBase.SetAnisotropicFilter(true);
	CTexturePtr textureBase = g_Renderer.GetTextureManager().CreateTexture(texturePropsBase);

	CTextureProperties texturePropsMask("art/textures/misc/territory_border_mask.png");
	texturePropsMask.SetAddressMode(
		Renderer::Backend::Sampler::AddressMode::CLAMP_TO_BORDER,
		Renderer::Backend::Sampler::AddressMode::CLAMP_TO_EDGE);
	texturePropsMask.SetAnisotropicFilter(true);
	CTexturePtr textureMask = g_Renderer.GetTextureManager().CreateTexture(texturePropsMask);

	CmpPtr<ICmpPlayerManager> cmpPlayerManager(GetSystemEntity());
	if (!cmpPlayerManager)
		return;

	for (size_t i = 0; i < boundaries.size(); ++i)
	{
		if (boundaries[i].points.empty())
			continue;

		CColor color(1, 0, 1, 1);
		CmpPtr<ICmpPlayer> cmpPlayer(GetSimContext(), cmpPlayerManager->GetPlayerByID(boundaries[i].owner));
		if (cmpPlayer)
			color = cmpPlayer->GetDisplayedColor();

		m_BoundaryLines.push_back(SBoundaryLine());
		m_BoundaryLines.back().blinking = boundaries[i].blinking;
		m_BoundaryLines.back().owner = boundaries[i].owner;
		m_BoundaryLines.back().color = color;
		m_BoundaryLines.back().overlay.m_SimContext = &GetSimContext();
		m_BoundaryLines.back().overlay.m_TextureBase = textureBase;
		m_BoundaryLines.back().overlay.m_TextureMask = textureMask;
		m_BoundaryLines.back().overlay.m_Color = color;
		m_BoundaryLines.back().overlay.m_Thickness = m_BorderThickness;
		m_BoundaryLines.back().overlay.m_Closed = true;

		SimRender::SmoothPointsAverage(boundaries[i].points, m_BoundaryLines.back().overlay.m_Closed);
		SimRender::InterpolatePointsRNS(boundaries[i].points, m_BoundaryLines.back().overlay.m_Closed, m_BorderSeparation);

		std::vector<CVector2D>& points = m_BoundaryLines.back().overlay.m_Coords;
		for (size_t j = 0; j < boundaries[i].points.size(); ++j)
		{
			points.push_back(boundaries[i].points[j]);

			if (m_EnableLineDebugOverlays)
			{
				const size_t numHighlightNodes = 7; // 高亮显示两端的最后X个节点，以查看它们在哪里相遇（如果闭合）
				SOverlayLine overlayNode;
				if (j > boundaries[i].points.size() - 1 - numHighlightNodes)
					overlayNode.m_Color = CColor(1.f, 0.f, 0.f, 1.f);
				else if (j < numHighlightNodes)
					overlayNode.m_Color = CColor(0.f, 1.f, 0.f, 1.f);
				else
					overlayNode.m_Color = CColor(1.0f, 1.0f, 1.0f, 1.0f);

				overlayNode.m_Thickness = 0.1f;
				SimRender::ConstructCircleOnGround(GetSimContext(), boundaries[i].points[j].X, boundaries[i].points[j].Y, 0.1f, overlayNode, true);
				m_DebugBoundaryLineNodes.push_back(overlayNode);
			}
		}

	}
}

// 插值计算，用于动画
void CCmpTerritoryManager::Interpolate(float frameTime, float UNUSED(frameOffset))
{
	m_AnimTime += frameTime;

	if (m_BoundaryLinesDirty)
	{
		UpdateBoundaryLines();
		m_BoundaryLinesDirty = false;
	}

	for (size_t i = 0; i < m_BoundaryLines.size(); ++i)
	{
		if (m_BoundaryLines[i].blinking)
		{
			CColor c = m_BoundaryLines[i].color;
			c.a *= 0.2f + 0.8f * fabsf((float)cos(m_AnimTime * M_PI)); // TODO: 应该让美术师调整这个
			m_BoundaryLines[i].overlay.m_Color = c;
		}
	}
}

// 提交渲染任务
void CCmpTerritoryManager::RenderSubmit(SceneCollector& collector, const CFrustum& frustum, bool culling)
{
	if (!IsVisible())
		return;

	for (size_t i = 0; i < m_BoundaryLines.size(); ++i)
	{
		if (culling && !m_BoundaryLines[i].overlay.IsVisibleInFrustum(frustum))
			continue;
		collector.Submit(&m_BoundaryLines[i].overlay);
	}

	for (size_t i = 0; i < m_DebugBoundaryLineNodes.size(); ++i)
		collector.Submit(&m_DebugBoundaryLineNodes[i]);

}

// 获取指定位置的所有者
player_id_t CCmpTerritoryManager::GetOwner(entity_pos_t x, entity_pos_t z)
{
	u16 i, j;
	if (!m_Territories)
	{
		CalculateTerritories();
		if (!m_Territories)
			return 0;
	}

	NearestTerritoryTile(x, z, i, j, m_Territories->m_W, m_Territories->m_H);
	return m_Territories->get(i, j) & TERRITORY_PLAYER_MASK;
}

// 获取邻近地块信息
std::vector<u32> CCmpTerritoryManager::GetNeighbours(entity_pos_t x, entity_pos_t z, bool filterConnected)
{
	CmpPtr<ICmpPlayerManager> cmpPlayerManager(GetSystemEntity());
	if (!cmpPlayerManager)
		return std::vector<u32>();

	std::vector<u32> ret(cmpPlayerManager->GetNumPlayers(), 0);
	CalculateTerritories();
	if (!m_Territories)
		return ret;

	u16 i, j;
	NearestTerritoryTile(x, z, i, j, m_Territories->m_W, m_Territories->m_H);

	// 计算邻居
	player_id_t thisOwner = m_Territories->get(i, j) & TERRITORY_PLAYER_MASK;

	u16 tilesW = m_Territories->m_W;
	u16 tilesH = m_Territories->m_H;

	// 使用一个泛洪填充算法，填充到边界并记住所有者
	Grid<bool> markerGrid(tilesW, tilesH);

	Floodfill({ i, j }, { tilesW, tilesH }, [&](const Tile*, const Tile& neighbour)
		{
			if (markerGrid.get(neighbour.x, neighbour.z))
				return false;
			// 无论如何都将地块标记为已访问
			markerGrid.set(neighbour.x, neighbour.z, true);
			int owner = m_Territories->get(neighbour.x, neighbour.z) & TERRITORY_PLAYER_MASK;
			if (owner != thisOwner)
			{
				if (owner == 0 || !filterConnected || (m_Territories->get(neighbour.x, neighbour.z) & TERRITORY_CONNECTED_MASK) != 0)
					ret[owner]++; // 如果需要，将玩家添加到邻居列表
				return false; // 不再扩展非所有者的地块
			}
			return true;
		});

	return ret;
}

// 检查领土是否已连接
bool CCmpTerritoryManager::IsConnected(entity_pos_t x, entity_pos_t z)
{
	u16 i, j;
	CalculateTerritories();
	if (!m_Territories)
		return false;

	NearestTerritoryTile(x, z, i, j, m_Territories->m_W, m_Territories->m_H);
	return (m_Territories->get(i, j) & TERRITORY_CONNECTED_MASK) != 0;
}

// 设置领土闪烁状态
void CCmpTerritoryManager::SetTerritoryBlinking(entity_pos_t x, entity_pos_t z, bool enable)
{
	CalculateTerritories();
	if (!m_Territories)
		return;

	u16 i, j;
	NearestTerritoryTile(x, z, i, j, m_Territories->m_W, m_Territories->m_H);

	u16 tilesW = m_Territories->m_W;
	u16 tilesH = m_Territories->m_H;

	player_id_t thisOwner = m_Territories->get(i, j) & TERRITORY_PLAYER_MASK;

	Floodfill({ i, j }, { tilesW, tilesH }, [&](const Tile*, const Tile& neighbour)
		{
			const u8 bitmask{ m_Territories->get(neighbour.x, neighbour.z) };
			if ((bitmask & TERRITORY_PLAYER_MASK) != thisOwner)
				return false;
			const bool blinking{ (bitmask & TERRITORY_BLINKING_MASK) != 0 };
			if (enable != blinking)
			{
				m_Territories->set(neighbour.x, neighbour.z, enable ?
					bitmask | TERRITORY_BLINKING_MASK : bitmask & ~TERRITORY_BLINKING_MASK);
				return true;
			}
			return false;
		});
	++m_DirtyBlinkingID;
	m_BoundaryLinesDirty = true;
}

// 检查领土是否在闪烁
bool CCmpTerritoryManager::IsTerritoryBlinking(entity_pos_t x, entity_pos_t z)
{
	CalculateTerritories();
	if (!m_Territories)
		return false;

	u16 i, j;
	NearestTerritoryTile(x, z, i, j, m_Territories->m_W, m_Territories->m_H);
	return (m_Territories->get(i, j) & TERRITORY_BLINKING_MASK) != 0;
}

// 更新颜色
void CCmpTerritoryManager::UpdateColors()
{
	m_ColorChanged = true;

	CmpPtr<ICmpPlayerManager> cmpPlayerManager(GetSystemEntity());
	if (!cmpPlayerManager)
		return;

	for (SBoundaryLine& boundaryLine : m_BoundaryLines)
	{
		CmpPtr<ICmpPlayer> cmpPlayer(GetSimContext(), cmpPlayerManager->GetPlayerByID(boundaryLine.owner));
		if (!cmpPlayer)
			continue;

		boundaryLine.color = cmpPlayer->GetDisplayedColor();
		boundaryLine.overlay.m_Color = boundaryLine.color;
	}
}

// TerritoryOverlay 构造函数
TerritoryOverlay::TerritoryOverlay(CCmpTerritoryManager& manager) :
	TerrainTextureOverlay((float)Pathfinding::NAVCELLS_PER_TERRAIN_TILE / ICmpTerritoryManager::NAVCELLS_PER_TERRITORY_TILE),
	m_TerritoryManager(manager)
{ }

// 构建调试覆盖层纹理
void TerritoryOverlay::BuildTextureRGBA(u8* data, size_t w, size_t h)
{
	for (size_t j = 0; j < h; ++j)
	{
		for (size_t i = 0; i < w; ++i)
		{
			SColor4ub color;
			u8 id = (m_TerritoryManager.m_Territories->get((int)i, (int)j) & ICmpTerritoryManager::TERRITORY_PLAYER_MASK);
			color = GetColor(id, 64);
			*data++ = color.R;
			*data++ = color.G;
			*data++ = color.B;
			*data++ = color.A;
		}
	}
}
