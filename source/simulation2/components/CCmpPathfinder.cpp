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
  * CCmpPathfinder 的通用代码和设置代码。
  */

#include "precompiled.h"

#include "CCmpPathfinder_Common.h"

#include "simulation2/MessageTypes.h"
#include "simulation2/components/ICmpObstruction.h"
#include "simulation2/components/ICmpObstructionManager.h"
#include "simulation2/components/ICmpPosition.h"
#include "simulation2/components/ICmpTerrain.h"
#include "simulation2/components/ICmpWaterManager.h"
#include "simulation2/helpers/HierarchicalPathfinder.h"
#include "simulation2/helpers/LongPathfinder.h"
#include "simulation2/helpers/MapEdgeTiles.h"
#include "simulation2/helpers/Rasterize.h"
#include "simulation2/helpers/VertexPathfinder.h"
#include "simulation2/serialization/SerializedPathfinder.h"
#include "simulation2/serialization/SerializedTypes.h"

#include "ps/CLogger.h"
#include "ps/CStr.h"
#include "ps/Profile.h"
#include "ps/XML/Xeromyces.h"
#include "renderer/Scene.h"

#include <type_traits>

  // 向组件系统注册 Pathfinder 组件类型
REGISTER_COMPONENT_TYPE(Pathfinder)

// 组件初始化函数
void CCmpPathfinder::Init(const CParamNode&)
{
	// 初始化成员变量
	m_GridSize = 0;
	m_Grid = NULL;
	m_TerrainOnlyGrid = NULL;

	// 清空AI寻路器的“脏”信息
	FlushAIPathfinderDirtinessInformation();

	// 初始化异步请求的唯一票据ID
	m_NextAsyncTicket = 1;

	// 初始化Atlas覆盖层指针
	m_AtlasOverlay = NULL;

	// 获取任务管理器的工作线程数量
	size_t workerThreads = g_TaskManager.GetNumberOfWorkers();
	// 为每个线程（包括主线程）存储一个顶点寻路器。
	while (m_VertexPathfinders.size() < workerThreads + 1)
		m_VertexPathfinders.emplace_back(m_GridSize, m_TerrainOnlyGrid);
	// 创建远程寻路器实例
	m_LongPathfinder = std::make_unique<LongPathfinder>();
	// 创建分层寻路器实例
	m_PathfinderHier = std::make_unique<HierarchicalPathfinder>();

	// 为每个工作线程设置一个 future 对象。
	m_Futures.resize(workerThreads);

	// 注册 Relax NG 验证器
	g_Xeromyces.AddValidator(g_VFS, "pathfinder", "simulation/data/pathfinder.rng");

	// 因为这是作为系统组件使用的（不是从实体模板加载），
	// 我们不能使用真正的 paramNode（在反序列化时它不会被正确处理），
	// 所以从一个特殊的XML文件加载数据。
	CParamNode externalParamNode;
	CParamNode::LoadXML(externalParamNode, L"simulation/data/pathfinder.xml", "pathfinder");

	// 路径计算发生在：
	//  - MT_Update 之前
	//  - MT_MotionUnitFormation 之前
	//  - 在回合结束和回合开始之间异步进行。
	// 后者必须计算所有未完成的请求，但前两者有上限
	// 以避免在那里花费太多时间（因为后者是多线程的，因此开销“更小”）。
	// 这里加载该最大数量（注意，它目前是每次计算调用的上限，而不是每回合）。
	const CParamNode pathingSettings = externalParamNode.GetChild("Pathfinder");
	m_MaxSameTurnMoves = (u16)pathingSettings.GetChild("MaxSameTurnMoves").ToInt();

	// 加载通行性类别定义
	const CParamNode::ChildrenMap& passClasses = externalParamNode.GetChild("Pathfinder").GetChild("PassabilityClasses").GetChildren();
	for (CParamNode::ChildrenMap::const_iterator it = passClasses.begin(); it != passClasses.end(); ++it)
	{
		std::string name = it->first;
		ENSURE((int)m_PassClasses.size() <= PASS_CLASS_BITS);
		pass_class_t mask = PASS_CLASS_MASK_FROM_INDEX(m_PassClasses.size());
		m_PassClasses.push_back(PathfinderPassability(mask, it->second));
		m_PassClassMasks[name] = mask;
	}
}

// 析构函数
CCmpPathfinder::~CCmpPathfinder() {};

// 组件销毁函数
void CCmpPathfinder::Deinit()
{
	SetDebugOverlay(false); // 清理内存

	m_Futures.clear();

	SAFE_DELETE(m_AtlasOverlay);

	SAFE_DELETE(m_Grid);
	SAFE_DELETE(m_TerrainOnlyGrid);
}

// 远程路径请求的序列化辅助模板
template<>
struct SerializeHelper<LongPathRequest>
{
	template<typename S>
	void operator()(S& serialize, const char* UNUSED(name), Serialize::qualify<S, LongPathRequest> value)
	{
		serialize.NumberU32_Unbounded("ticket", value.ticket);
		serialize.NumberFixed_Unbounded("x0", value.x0);
		serialize.NumberFixed_Unbounded("z0", value.z0);
		Serializer(serialize, "goal", value.goal);
		serialize.NumberU16_Unbounded("pass class", value.passClass);
		serialize.NumberU32_Unbounded("notify", value.notify);
	}
};

// 短程路径请求的序列化辅助模板
template<>
struct SerializeHelper<ShortPathRequest>
{
	template<typename S>
	void operator()(S& serialize, const char* UNUSED(name), Serialize::qualify<S, ShortPathRequest> value)
	{
		serialize.NumberU32_Unbounded("ticket", value.ticket);
		serialize.NumberFixed_Unbounded("x0", value.x0);
		serialize.NumberFixed_Unbounded("z0", value.z0);
		serialize.NumberFixed_Unbounded("clearance", value.clearance);
		serialize.NumberFixed_Unbounded("range", value.range);
		Serializer(serialize, "goal", value.goal);
		serialize.NumberU16_Unbounded("pass class", value.passClass);
		serialize.Bool("avoid moving units", value.avoidMovingUnits);
		serialize.NumberU32_Unbounded("group", value.group);
		serialize.NumberU32_Unbounded("notify", value.notify);
	}
};

// 通用的序列化/反序列化函数
template<typename S>
void CCmpPathfinder::SerializeCommon(S& serialize)
{
	Serializer(serialize, "long requests", m_LongPathRequests.m_Requests);
	Serializer(serialize, "short requests", m_ShortPathRequests.m_Requests);
	serialize.NumberU32_Unbounded("next ticket", m_NextAsyncTicket);
	serialize.NumberU16_Unbounded("grid size", m_GridSize);
}

// 序列化函数
void CCmpPathfinder::Serialize(ISerializer& serialize)
{
	SerializeCommon(serialize);
}

// 反序列化函数
void CCmpPathfinder::Deserialize(const CParamNode& paramNode, IDeserializer& deserialize)
{
	Init(paramNode);

	SerializeCommon(deserialize);
}

// 消息处理函数
void CCmpPathfinder::HandleMessage(const CMessage& msg, bool UNUSED(global))
{
	switch (msg.GetType())
	{
	case MT_RenderSubmit: // 渲染提交消息
	{
		const CMessageRenderSubmit& msgData = static_cast<const CMessageRenderSubmit&> (msg);
		RenderSubmit(msgData.collector);
		break;
	}
	case MT_TerrainChanged: // 地形变更消息
	{
		const CMessageTerrainChanged& msgData = static_cast<const CMessageTerrainChanged&>(msg);
		m_TerrainDirty = true;
		MinimalTerrainUpdate(msgData.i0, msgData.j0, msgData.i1, msgData.j1);
		break;
	}
	case MT_WaterChanged: // 水体变更消息
	case MT_ObstructionMapShapeChanged: // 障碍物地图形状变更消息
		m_TerrainDirty = true;
		UpdateGrid();
		break;
	case MT_Deserialized: // 反序列化完成消息
		UpdateGrid();
		// 如果我们序列化时有待处理的请求，我们需要处理它们。
		if (!m_ShortPathRequests.m_Requests.empty() || !m_LongPathRequests.m_Requests.empty())
		{
			ENSURE(CmpPtr<ICmpObstructionManager>(GetSystemEntity()));
			StartProcessingMoves(false);
		}
		break;
	}
}

// 提交渲染任务
void CCmpPathfinder::RenderSubmit(SceneCollector& collector)
{
	g_VertexPathfinderDebugOverlay.RenderSubmit(collector);
	m_PathfinderHier->RenderSubmit(collector);
}

// 设置调试路径以供渲染
void CCmpPathfinder::SetDebugPath(entity_pos_t x0, entity_pos_t z0, const PathGoal& goal, pass_class_t passClass)
{
	m_LongPathfinder->SetDebugPath(*m_PathfinderHier, x0, z0, goal, passClass);
}

// 设置调试覆盖层
void CCmpPathfinder::SetDebugOverlay(bool enabled)
{
	g_VertexPathfinderDebugOverlay.SetDebugOverlay(enabled);
	m_LongPathfinder->SetDebugOverlay(enabled);
}

// 设置分层寻路器的调试覆盖层
void CCmpPathfinder::SetHierDebugOverlay(bool enabled)
{
	m_PathfinderHier->SetDebugOverlay(enabled, &GetSimContext());
}

// 获取调试数据
void CCmpPathfinder::GetDebugData(u32& steps, double& time, Grid<u8>& grid) const
{
	m_LongPathfinder->GetDebugData(steps, time, grid);
}

// 在 Atlas 编辑器中设置覆盖层
void CCmpPathfinder::SetAtlasOverlay(bool enable, pass_class_t passClass)
{
	if (enable)
	{
		if (!m_AtlasOverlay)
			m_AtlasOverlay = new AtlasOverlay(this, passClass);
		m_AtlasOverlay->m_PassClass = passClass;
	}
	else
		SAFE_DELETE(m_AtlasOverlay);
}

// 根据名称获取通行性类别
pass_class_t CCmpPathfinder::GetPassabilityClass(const std::string& name) const
{
	std::map<std::string, pass_class_t>::const_iterator it = m_PassClassMasks.find(name);
	if (it == m_PassClassMasks.end())
	{
		LOGERROR("Invalid passability class name '%s'", name.c_str());
		return 0;
	}

	return it->second;
}

// 获取所有通行性类别
void CCmpPathfinder::GetPassabilityClasses(std::map<std::string, pass_class_t>& passClasses) const
{
	passClasses = m_PassClassMasks;
}

// 获取通行性类别，区分寻路和非寻路类别
void CCmpPathfinder::GetPassabilityClasses(std::map<std::string, pass_class_t>& nonPathfindingPassClasses, std::map<std::string, pass_class_t>& pathfindingPassClasses) const
{
	for (const std::pair<const std::string, pass_class_t>& pair : m_PassClassMasks)
	{
		if ((GetPassabilityFromMask(pair.second)->m_Obstructions == PathfinderPassability::PATHFINDING))
			pathfindingPassClasses[pair.first] = pair.second;
		else
			nonPathfindingPassClasses[pair.first] = pair.second;
	}
}

// 根据掩码获取通行性数据
const PathfinderPassability* CCmpPathfinder::GetPassabilityFromMask(pass_class_t passClass) const
{
	for (const PathfinderPassability& passability : m_PassClasses)
	{
		if (passability.m_Mask == passClass)
			return &passability;
	}

	return NULL;
}

// 获取通行性格子图
const Grid<NavcellData>& CCmpPathfinder::GetPassabilityGrid()
{
	if (!m_Grid)
		UpdateGrid();

	return *m_Grid;
}

/**
 * 给定一个可通行/不可通行的导航单元格（navcell）网格（基于某个通行性掩码），
 * 计算一个新的网格，其中一个导航单元格（根据该掩码）如果
 * 距离原始网格中一个不可通行的导航单元格 <= clearance 个导航单元格，则该单元格也不可通行。
 * 结果会通过“或”运算合并到原始网格上。
 *
 * 这用于在基于地形的导航单元格通行性上增加间隙。
 *
 * TODO PATHFINDER: 通过将间隙测量为欧几里得距离，可能会得到更圆滑的边角；
 * 目前它实际上是使用 dist=max(dx,dy)。
 * 这只对大的间隙才真正是个问题。
 */
static void ExpandImpassableCells(Grid<NavcellData>& grid, u16 clearance, pass_class_t mask)
{
	PROFILE3("ExpandImpassableCells");

	u16 w = grid.m_W;
	u16 h = grid.m_H;

	// 首先将不可通行的单元格水平扩展到一个临时的1位网格中
	Grid<u8> tempGrid(w, h);
	for (u16 j = 0; j < h; ++j)
	{
		// 如果对于任何 i-clearance <= i' <= i+clearance，(i',j)被阻塞，则新单元格(i,j)也被阻塞

		// 计算 i=0 周围被阻塞的单元格数量
		u16 numBlocked = 0;
		for (u16 i = 0; i <= clearance && i < w; ++i)
			if (!IS_PASSABLE(grid.get(i, j), mask))
				++numBlocked;

		for (u16 i = 0; i < w; ++i)
		{
			// 如果被至少一个附近的单元格阻塞，则存储一个标志
			if (numBlocked)
				tempGrid.set(i, j, 1);

			// 向前滑动 numBlocked 窗口：
			// 移除旧的 i-clearance 值，添加新的 (i+1)+clearance 值
			// （避免溢出网格）
			if (i >= clearance && !IS_PASSABLE(grid.get(i - clearance, j), mask))
				--numBlocked;
			if (i + 1 + clearance < w && !IS_PASSABLE(grid.get(i + 1 + clearance, j), mask))
				++numBlocked;
		}
	}

	// 垂直扩展
	for (u16 i = 0; i < w; ++i)
	{
		// 如果对于任何 j-clearance <= j' <= j+clearance，(i,j')被阻塞，则新单元格(i,j)也被阻塞
		// 计算 j=0 周围被阻塞的单元格数量
		u16 numBlocked = 0;
		for (u16 j = 0; j <= clearance && j < h; ++j)
			if (tempGrid.get(i, j))
				++numBlocked;

		for (u16 j = 0; j < h; ++j)
		{
			// 如果被至少一个附近的单元格阻塞，则添加掩码
			if (numBlocked)
				grid.set(i, j, grid.get(i, j) | mask);

			// 向前滑动 numBlocked 窗口：
			// 移除旧的 j-clearance 值，添加新的 (j+1)+clearance 值
			// （避免溢出网格）
			if (j >= clearance && tempGrid.get(i, j - clearance))
				--numBlocked;
			if (j + 1 + clearance < h && tempGrid.get(i, j + 1 + clearance))
				++numBlocked;
		}
	}
}

// 计算海岸线格子图
Grid<u16> CCmpPathfinder::ComputeShoreGrid(bool expandOnWater)
{
	PROFILE3("ComputeShoreGrid");

	CmpPtr<ICmpWaterManager> cmpWaterManager(GetSystemEntity());

	// TODO: 这些位应该来自 ICmpTerrain
	CTerrain& terrain = GetSimContext().GetTerrain();

	// 避免中间计算中的整数溢出
	const u16 shoreMax = 32767;

	u16 shoreGridSize = terrain.GetTilesPerSide();

	// 第一遍 - 找到水下地块
	Grid<u8> waterGrid(shoreGridSize, shoreGridSize);
	for (u16 j = 0; j < shoreGridSize; ++j)
	{
		for (u16 i = 0; i < shoreGridSize; ++i)
		{
			fixed x, z;
			Pathfinding::TerrainTileCenter(i, j, x, z);

			bool underWater = cmpWaterManager && (cmpWaterManager->GetWaterLevel(x, z) > terrain.GetExactGroundLevelFixed(x, z));
			waterGrid.set(i, j, underWater ? 1 : 0);
		}
	}

	// 第二遍 - 找到海岸地块
	Grid<u16> shoreGrid(shoreGridSize, shoreGridSize);
	for (u16 j = 0; j < shoreGridSize; ++j)
	{
		for (u16 i = 0; i < shoreGridSize; ++i)
		{
			// 找到一个陆地地块
			if (!waterGrid.get(i, j))
			{
				// 如果它与水相邻，它就是一个海岸地块
				if ((i > 0 && waterGrid.get(i - 1, j)) || (i > 0 && j < shoreGridSize - 1 && waterGrid.get(i - 1, j + 1)) || (i > 0 && j > 0 && waterGrid.get(i - 1, j - 1))
					|| (i < shoreGridSize - 1 && waterGrid.get(i + 1, j)) || (i < shoreGridSize - 1 && j < shoreGridSize - 1 && waterGrid.get(i + 1, j + 1)) || (i < shoreGridSize - 1 && j > 0 && waterGrid.get(i + 1, j - 1))
					|| (j > 0 && waterGrid.get(i, j - 1)) || (j < shoreGridSize - 1 && waterGrid.get(i, j + 1))
					)
					shoreGrid.set(i, j, 0);
				else
					shoreGrid.set(i, j, shoreMax);
			}
			// 如果我们想在水上扩展，我们希望水地块不是海岸地块
			else if (expandOnWater)
				shoreGrid.set(i, j, shoreMax);
		}
	}

	// 在陆地上扩展影响以找到海岸距离
	for (u16 y = 0; y < shoreGridSize; ++y)
	{
		u16 min = shoreMax;
		for (u16 x = 0; x < shoreGridSize; ++x)
		{
			if (!waterGrid.get(x, y) || expandOnWater)
			{
				u16 g = shoreGrid.get(x, y);
				if (g > min)
					shoreGrid.set(x, y, min);
				else if (g < min)
					min = g;

				++min;
			}
		}
		for (u16 x = shoreGridSize; x > 0; --x)
		{
			if (!waterGrid.get(x - 1, y) || expandOnWater)
			{
				u16 g = shoreGrid.get(x - 1, y);
				if (g > min)
					shoreGrid.set(x - 1, y, min);
				else if (g < min)
					min = g;

				++min;
			}
		}
	}
	for (u16 x = 0; x < shoreGridSize; ++x)
	{
		u16 min = shoreMax;
		for (u16 y = 0; y < shoreGridSize; ++y)
		{
			if (!waterGrid.get(x, y) || expandOnWater)
			{
				u16 g = shoreGrid.get(x, y);
				if (g > min)
					shoreGrid.set(x, y, min);
				else if (g < min)
					min = g;

				++min;
			}
		}
		for (u16 y = shoreGridSize; y > 0; --y)
		{
			if (!waterGrid.get(x, y - 1) || expandOnWater)
			{
				u16 g = shoreGrid.get(x, y - 1);
				if (g > min)
					shoreGrid.set(x, y - 1, min);
				else if (g < min)
					min = g;

				++min;
			}
		}
	}

	return shoreGrid;
}

// 更新格子图
void CCmpPathfinder::UpdateGrid()
{
	PROFILE3("UpdateGrid");

	CmpPtr<ICmpTerrain> cmpTerrain(GetSimContext(), SYSTEM_ENTITY);
	if (!cmpTerrain)
		return; // 错误

	u16 gridSize = cmpTerrain->GetMapSize() / Pathfinding::NAVCELL_SIZE_INT;
	if (gridSize == 0)
		return;

	// 如果地形被调整大小，则删除旧的网格数据
	if (m_Grid && m_GridSize != gridSize)
	{
		SAFE_DELETE(m_Grid);
		SAFE_DELETE(m_TerrainOnlyGrid);
	}

	// 首次需要时初始化地形数据
	if (!m_Grid)
	{
		m_GridSize = gridSize;
		m_Grid = new Grid<NavcellData>(m_GridSize, m_GridSize);
		SAFE_DELETE(m_TerrainOnlyGrid);
		m_TerrainOnlyGrid = new Grid<NavcellData>(m_GridSize, m_GridSize);

		m_DirtinessInformation = { true, true, Grid<u8>(m_GridSize, m_GridSize) };
		m_AIPathfinderDirtinessInformation = m_DirtinessInformation;

		m_TerrainDirty = true;
	}

	// 网格应该被正确初始化和清理。检查后者开销很大，所以只在调试时进行。
#ifdef NDEBUG
	ENSURE(m_DirtinessInformation.dirtinessGrid.compare_sizes(m_Grid));
#else
	ENSURE(m_DirtinessInformation.dirtinessGrid == Grid<u8>(m_GridSize, m_GridSize));
#endif

	CmpPtr<ICmpObstructionManager> cmpObstructionManager(GetSimContext(), SYSTEM_ENTITY);
	cmpObstructionManager->UpdateInformations(m_DirtinessInformation);

	if (!m_DirtinessInformation.dirty && !m_TerrainDirty)
		return;

	// 如果地形已改变，重新计算 m_Grid
	// 否则，使用 m_TerrainOnlyGrid 的数据并添加障碍物
	if (m_TerrainDirty)
	{
		TerrainUpdateHelper();

		*m_Grid = *m_TerrainOnlyGrid;

		m_TerrainDirty = false;
		m_DirtinessInformation.globallyDirty = true;
	}
	else if (m_DirtinessInformation.globallyDirty)
	{
		ENSURE(m_Grid->compare_sizes(m_TerrainOnlyGrid));
		memcpy(m_Grid->m_Data, m_TerrainOnlyGrid->m_Data, (m_Grid->m_W) * (m_Grid->m_H) * sizeof(NavcellData));
	}
	else
	{
		ENSURE(m_Grid->compare_sizes(m_TerrainOnlyGrid));

		for (u16 j = 0; j < m_DirtinessInformation.dirtinessGrid.m_H; ++j)
			for (u16 i = 0; i < m_DirtinessInformation.dirtinessGrid.m_W; ++i)
				if (m_DirtinessInformation.dirtinessGrid.get(i, j) == 1)
					m_Grid->set(i, j, m_TerrainOnlyGrid->get(i, j));
	}

	// 将障碍物光栅化到网格上
	cmpObstructionManager->Rasterize(*m_Grid, m_PassClasses, m_DirtinessInformation.globallyDirty);

	// 更新远程和分层寻路器。
	if (m_DirtinessInformation.globallyDirty)
	{
		std::map<std::string, pass_class_t> nonPathfindingPassClasses, pathfindingPassClasses;
		GetPassabilityClasses(nonPathfindingPassClasses, pathfindingPassClasses);
		m_LongPathfinder->Reload(m_Grid);
		m_PathfinderHier->Recompute(m_Grid, nonPathfindingPassClasses, pathfindingPassClasses);
	}
	else
	{
		m_LongPathfinder->Update(m_Grid);
		m_PathfinderHier->Update(m_Grid, m_DirtinessInformation.dirtinessGrid);
	}

	// 记住AI寻路器也需要执行的必要更新
	m_AIPathfinderDirtinessInformation.MergeAndClear(m_DirtinessInformation);
}

// 最小化地形更新
void CCmpPathfinder::MinimalTerrainUpdate(int itile0, int jtile0, int itile1, int jtile1)
{
	TerrainUpdateHelper(false, itile0, jtile0, itile1, jtile1);
}

// 地形更新辅助函数
void CCmpPathfinder::TerrainUpdateHelper(bool expandPassability, int itile0, int jtile0, int itile1, int jtile1)
{
	PROFILE3("TerrainUpdateHelper");

	CmpPtr<ICmpObstructionManager> cmpObstructionManager(GetSimContext(), SYSTEM_ENTITY);
	CmpPtr<ICmpWaterManager> cmpWaterManager(GetSimContext(), SYSTEM_ENTITY);
	CmpPtr<ICmpTerrain> cmpTerrain(GetSimContext(), SYSTEM_ENTITY);
	CTerrain& terrain = GetSimContext().GetTerrain();

	if (!cmpTerrain || !cmpObstructionManager)
		return;

	u16 gridSize = cmpTerrain->GetMapSize() / Pathfinding::NAVCELL_SIZE_INT;
	if (gridSize == 0)
		return;

	const bool needsNewTerrainGrid = !m_TerrainOnlyGrid || m_GridSize != gridSize;
	if (needsNewTerrainGrid)
	{
		m_GridSize = gridSize;

		SAFE_DELETE(m_TerrainOnlyGrid);
		m_TerrainOnlyGrid = new Grid<NavcellData>(m_GridSize, m_GridSize);

		// 如果此更新来自地图大小调整，我们必须也重新初始化其他网格
		if (!m_TerrainOnlyGrid->compare_sizes(m_Grid))
		{
			SAFE_DELETE(m_Grid);
			m_Grid = new Grid<NavcellData>(m_GridSize, m_GridSize);

			m_DirtinessInformation = { true, true, Grid<u8>(m_GridSize, m_GridSize) };
			m_AIPathfinderDirtinessInformation = m_DirtinessInformation;
		}
	}

	Grid<u16> shoreGrid = ComputeShoreGrid();

	const bool partialTerrainGridUpdate =
		!expandPassability && !needsNewTerrainGrid &&
		itile0 != -1 && jtile0 != -1 && itile1 != -1 && jtile1 != -1;
	int istart = 0, iend = m_GridSize;
	int jstart = 0, jend = m_GridSize;
	if (partialTerrainGridUpdate)
	{
		// 我们需要将边界扩展1个地块，因为坡度和地面
		// 高度是由多个相邻地块计算的。
		// TODO: 添加 CTerrain 常量而不是 1。
		istart = Clamp<int>((itile0 - 1) * Pathfinding::NAVCELLS_PER_TERRAIN_TILE, 0, m_GridSize);
		iend = Clamp<int>((itile1 + 1) * Pathfinding::NAVCELLS_PER_TERRAIN_TILE, 0, m_GridSize);
		jstart = Clamp<int>((jtile0 - 1) * Pathfinding::NAVCELLS_PER_TERRAIN_TILE, 0, m_GridSize);
		jend = Clamp<int>((jtile1 + 1) * Pathfinding::NAVCELLS_PER_TERRAIN_TILE, 0, m_GridSize);
	}

	// 计算初始的地形相关通行性
	for (int j = jstart; j < jend; ++j)
	{
		for (int i = istart; i < iend; ++i)
		{
			// 此导航单元格的世界空间坐标
			fixed x, z;
			Pathfinding::NavcellCenter(i, j, x, z);

			// 此导航单元格的地形地块坐标
			int itile = i / Pathfinding::NAVCELLS_PER_TERRAIN_TILE;
			int jtile = j / Pathfinding::NAVCELLS_PER_TERRAIN_TILE;

			// 收集所有可能需要用来确定通行性的数据：

			fixed height = terrain.GetExactGroundLevelFixed(x, z);

			fixed water;
			if (cmpWaterManager)
				water = cmpWaterManager->GetWaterLevel(x, z);

			fixed depth = water - height;

			// 精确的坡度会给出有点奇怪的输出，所以只使用粗略的基于地块的坡度
			fixed slope = terrain.GetSlopeFixed(itile, jtile);

			// 从 shoreGrid（使用地形地块）获取世界空间坐标
			fixed shoredist = fixed::FromInt(shoreGrid.get(itile, jtile)).MultiplyClamp(TERRAIN_TILE_SIZE);

			// 为此单元格计算每个类别的通行性
			NavcellData t = 0;
			for (const PathfinderPassability& passability : m_PassClasses)
				if (!passability.IsPassable(depth, slope, shoredist))
					t |= passability.m_Mask;

			m_TerrainOnlyGrid->set(i, j, t);
		}
	}

	// 计算世界外的通行性
	const int edgeSize = MAP_EDGE_TILES * Pathfinding::NAVCELLS_PER_TERRAIN_TILE;

	NavcellData edgeMask = 0;
	for (const PathfinderPassability& passability : m_PassClasses)
		edgeMask |= passability.m_Mask;

	int w = m_TerrainOnlyGrid->m_W;
	int h = m_TerrainOnlyGrid->m_H;

	if (cmpObstructionManager->GetPassabilityCircular())
	{
		for (int j = jstart; j < jend; ++j)
		{
			for (int i = istart; i < iend; ++i)
			{
				// 基于 CCmpRangeManager::LosIsOffWorld
				// 但因为是基于地块的而做了调整。
				// （我们将所有值加倍，以便处理半地块坐标。）
				// 这需要比LOS圆圈稍微紧凑一些，
				// 否则单位可能会在边缘的战争迷雾中迷路。

				int dist2 = (i * 2 + 1 - w) * (i * 2 + 1 - w)
					+ (j * 2 + 1 - h) * (j * 2 + 1 - h);

				if (dist2 >= (w - 2 * edgeSize) * (h - 2 * edgeSize))
					m_TerrainOnlyGrid->set(i, j, m_TerrainOnlyGrid->get(i, j) | edgeMask);
			}
		}
	}
	else
	{
		for (u16 j = 0; j < h; ++j)
			for (u16 i = 0; i < edgeSize; ++i)
				m_TerrainOnlyGrid->set(i, j, m_TerrainOnlyGrid->get(i, j) | edgeMask);
		for (u16 j = 0; j < h; ++j)
			for (u16 i = w - edgeSize + 1; i < w; ++i)
				m_TerrainOnlyGrid->set(i, j, m_TerrainOnlyGrid->get(i, j) | edgeMask);
		for (u16 j = 0; j < edgeSize; ++j)
			for (u16 i = edgeSize; i < w - edgeSize + 1; ++i)
				m_TerrainOnlyGrid->set(i, j, m_TerrainOnlyGrid->get(i, j) | edgeMask);
		for (u16 j = h - edgeSize + 1; j < h; ++j)
			for (u16 i = edgeSize; i < w - edgeSize + 1; ++i)
				m_TerrainOnlyGrid->set(i, j, m_TerrainOnlyGrid->get(i, j) | edgeMask);
	}

	if (!expandPassability)
		return;

	// 对于任何具有非零间隙的类别，扩展不可通行网格，
	// 以便我们可以阻止单位过于靠近不可通行的导航单元格。
	// 注意：不可能一次为所有具有相同间隙的通行性类别执行此扩展，
	// 因为对于所有这些通行性类别，不可通行的单元格不一定相同。
	for (PathfinderPassability& passability : m_PassClasses)
	{
		if (passability.m_Clearance == fixed::Zero())
			continue;

		int clearance = (passability.m_Clearance / Pathfinding::NAVCELL_SIZE).ToInt_RoundToInfinity();
		ExpandImpassableCells(*m_TerrainOnlyGrid, clearance, passability.m_Mask);
	}
}

//////////////////////////////////////////////////////////

// 异步计算长程路径
u32 CCmpPathfinder::ComputePathAsync(entity_pos_t x0, entity_pos_t z0, const PathGoal& goal, pass_class_t passClass, entity_id_t notify)
{
	LongPathRequest req = { m_NextAsyncTicket++, x0, z0, goal, passClass, notify };
	m_LongPathRequests.m_Requests.push_back(req);
	return req.ticket;
}

// 异步计算短程路径
u32 CCmpPathfinder::ComputeShortPathAsync(entity_pos_t x0, entity_pos_t z0, entity_pos_t clearance, entity_pos_t range,
	const PathGoal& goal, pass_class_t passClass, bool avoidMovingUnits,
	entity_id_t group, entity_id_t notify)
{
	ShortPathRequest req = { m_NextAsyncTicket++, x0, z0, clearance, range, goal, passClass, avoidMovingUnits, group, notify };
	m_ShortPathRequests.m_Requests.push_back(req);
	return req.ticket;
}

// 立即计算长程路径
void CCmpPathfinder::ComputePathImmediate(entity_pos_t x0, entity_pos_t z0, const PathGoal& goal, pass_class_t passClass, WaypointPath& ret) const
{
	m_LongPathfinder->ComputePath(*m_PathfinderHier, x0, z0, goal, passClass, ret);
}

// 立即计算短程路径
WaypointPath CCmpPathfinder::ComputeShortPathImmediate(const ShortPathRequest& request) const
{
	return m_VertexPathfinders.front().ComputeShortPath(request, CmpPtr<ICmpObstructionManager>(GetSystemEntity()));
}

// 模板函数，用于在工作线程中计算路径请求
template<typename T>
template<typename U>
void CCmpPathfinder::PathRequests<T>::Compute(const CCmpPathfinder& cmpPathfinder, const U& pathfinder)
{
	// 静态断言，确保模板参数类型匹配
	static_assert((std::is_same_v<T, LongPathRequest> && std::is_same_v<U, LongPathfinder>) ||
		(std::is_same_v<T, ShortPathRequest> && std::is_same_v<U, VertexPathfinder>));
	size_t maxN = m_Results.size();
	size_t startIndex = m_Requests.size() - m_Results.size();
	do
	{
		// 原子地获取下一个要处理的任务索引
		size_t workIndex = m_NextPathToCompute++;
		if (workIndex >= maxN)
			break; // 所有任务已分配
		const T& req = m_Requests[startIndex + workIndex];
		PathResult& result = m_Results[workIndex];
		result.ticket = req.ticket;
		result.notify = req.notify;
		// 根据请求类型调用不同的计算函数
		if constexpr (std::is_same_v<T, LongPathRequest>)
			pathfinder.ComputePath(*cmpPathfinder.m_PathfinderHier, req.x0, req.z0, req.goal, req.passClass, result.path);
		else
			result.path = pathfinder.ComputeShortPath(req, CmpPtr<ICmpObstructionManager>(cmpPathfinder.GetSystemEntity()));
		// 如果是最后一个任务，则标记计算完成
		if (workIndex == maxN - 1)
			m_ComputeDone = true;
	} while (true);
}

// 发送已计算完成的路径请求结果
void CCmpPathfinder::SendRequestedPaths()
{
	PROFILE2("SendRequestedPaths");

	if (!m_LongPathRequests.m_ComputeDone || !m_ShortPathRequests.m_ComputeDone)
	{
		// 也在主线程上开始计算以更快地完成。
		m_ShortPathRequests.Compute(*this, m_VertexPathfinders.front());
		m_LongPathRequests.Compute(*this, *m_LongPathfinder);
	}
	// 我们完成了，从 future 中获取异常。
	for (Future<void>& future : m_Futures)
		if (future.Valid())
			future.Get();

	{
		PROFILE2("PostMessages");
		// 发送短程路径结果消息
		for (PathResult& path : m_ShortPathRequests.m_Results)
		{
			CMessagePathResult msg(path.ticket, path.path);
			GetSimContext().GetComponentManager().PostMessage(path.notify, msg);
		}

		// 发送远程路径结果消息
		for (PathResult& path : m_LongPathRequests.m_Results)
		{
			CMessagePathResult msg(path.ticket, path.path);
			GetSimContext().GetComponentManager().PostMessage(path.notify, msg);
		}
	}
	// 清理已完成的请求
	m_ShortPathRequests.ClearComputed();
	m_LongPathRequests.ClearComputed();
}

// 开始处理移动（寻路）请求
void CCmpPathfinder::StartProcessingMoves(bool useMax)
{
	m_ShortPathRequests.PrepareForComputation(useMax ? m_MaxSameTurnMoves : 0);
	m_LongPathRequests.PrepareForComputation(useMax ? m_MaxSameTurnMoves : 0);

	// 唤醒线程有一些开销，所以除非我们需要，否则不要这样做。
	const size_t m = std::min(m_Futures.size(), m_ShortPathRequests.m_Requests.size() + m_LongPathRequests.m_Requests.size());
	for (size_t i = 0; i < m; ++i)
	{
		ENSURE(!m_Futures[i].Valid());
		// 传递 i+1 个顶点寻路器以保留第一个给主线程，
		// 每个线程获取自己的实例以避免缓存数据中的冲突。
		m_Futures[i] = g_TaskManager.PushTask(
			[&pathfinder = *this, &vertexPfr = m_VertexPathfinders[i + 1]]()
			{
				PROFILE2("Async pathfinding");
				pathfinder.m_ShortPathRequests.Compute(pathfinder, vertexPfr);
				pathfinder.m_LongPathRequests.Compute(pathfinder, *pathfinder.m_LongPathfinder);
			});
	}
}


//////////////////////////////////////////////////////////

// 检查目标是否可达
bool CCmpPathfinder::IsGoalReachable(entity_pos_t x0, entity_pos_t z0, const PathGoal& goal, pass_class_t passClass)
{
	PROFILE2("IsGoalReachable");

	u16 i, j;
	Pathfinding::NearestNavcell(x0, z0, i, j, m_GridSize, m_GridSize);
	if (!IS_PASSABLE(m_Grid->get(i, j), passClass))
		m_PathfinderHier->FindNearestPassableNavcell(i, j, passClass);

	return m_PathfinderHier->IsGoalReachable(i, j, goal, passClass);
}

// 围绕一个点分布单位
std::vector<CFixedVector2D> CCmpPathfinder::DistributeAround(std::vector<entity_id_t> units, entity_pos_t x, entity_pos_t z) const
{
	PROFILE2("DistributeAround");

	std::vector<CFixedVector2D> positions;
	if (units.empty())
		return positions;

	positions.reserve(units.size());

	// 初始化螺旋参数
	fixed angle = fixed::Zero();
	fixed radius = fixed::FromInt(1);
	const fixed increment = fixed::FromInt(7) / 4;

	// 计算角度步长，以便在此半径下我们不会让单位过于拥挤
	fixed angleStep = fixed::FromInt(9).MulDiv(fixed::FromInt(1), fixed::Pi().Multiply(radius));

	for (size_t i = 0; i < units.size(); ++i)
	{
		CFixedVector2D offset(radius, fixed::Zero());
		offset = offset.Rotate(angle);

		positions.emplace_back(x + offset.X, z + offset.Y);

		angle += angleStep;
		// 如果我们完成了一个圆周，增加半径并重新计算角度步长
		if (angle >= fixed::Pi().Multiply(fixed::FromInt(2)))
		{
			angle = fixed::Zero();
			radius += increment;
			angleStep = fixed::FromInt(9).MulDiv(fixed::FromInt(1), fixed::Pi().Multiply(radius));
		}
	}

	// 获取当前单位位置以计算行进距离
	std::vector<CFixedVector2D> unitPositions;
	unitPositions.reserve(units.size());

	CmpPtr<ICmpPosition> cmpPosition(GetSystemEntity());
	for (entity_id_t unit : units)
	{
		CmpPtr<ICmpPosition> unitPos(GetSimContext(), unit);
		if (!unitPos || !unitPos->IsInWorld())
			return positions;

		CFixedVector2D pos(unitPos->GetPosition2D());
		unitPositions.push_back(pos);
	}

	// 通过交换位置来优化位置，如果它能减少总行进距离。
	bool improved;
	do
	{
		improved = false;
		for (size_t i = 0; i < positions.size(); ++i)
		{
			// 辅助函数，以整数计算两点之间的平方距离
			auto distSq = [](const CFixedVector2D& p1, const CFixedVector2D& p2) -> u32 {
				i32 dx = (p1.X - p2.X).ToInt_RoundToInfinity();
				i32 dy = (p1.Y - p2.Y).ToInt_RoundToInfinity();
				return dx * dx + dy * dy;
				};

			for (size_t j = i + 1; j < positions.size(); ++j)
			{
				u32 currentDistSq = distSq(positions[i], unitPositions[i]) + distSq(positions[j], unitPositions[j]);
				u32 swappedDistSq = distSq(positions[j], unitPositions[i]) + distSq(positions[i], unitPositions[j]);

				// 如果能减少总平方距离，则交换
				if (swappedDistSq < currentDistSq)
				{
					std::swap(positions[i], positions[j]);
					improved = true;
				}
			}
		}
	} while (improved);

	return positions;
}

// 检查移动路线是否有效
bool CCmpPathfinder::CheckMovement(const IObstructionTestFilter& filter,
	entity_pos_t x0, entity_pos_t z0, entity_pos_t x1, entity_pos_t z1, entity_pos_t r,
	pass_class_t passClass) const
{
	// 首先测试障碍物。过滤器可能会丢弃阻挡寻路的障碍物。
	// 使用更宽松版本的 TestLine 以允许单位-单位碰撞略微重叠。
	// 这使得单位的移动总体上更平滑、更自然。
	CmpPtr<ICmpObstructionManager> cmpObstructionManager(GetSystemEntity());
	if (!cmpObstructionManager || cmpObstructionManager->TestLine(filter, x0, z0, x1, z1, r, true))
		return false;

	// 然后测试地形网格。这应该是不必要的
	// 但如果我们允许地形改变，它就会变得必要。
	return Pathfinding::CheckLineMovement(x0, z0, x1, z1, passClass, *m_TerrainOnlyGrid);
}

// 检查单位放置位置
ICmpObstruction::EFoundationCheck CCmpPathfinder::CheckUnitPlacement(const IObstructionTestFilter& filter,
	entity_pos_t x, entity_pos_t z, entity_pos_t r, pass_class_t passClass, bool UNUSED(onlyCenterPoint)) const
{
	// 检查单位障碍物
	CmpPtr<ICmpObstructionManager> cmpObstructionManager(GetSystemEntity());
	if (!cmpObstructionManager)
		return ICmpObstruction::FOUNDATION_CHECK_FAIL_ERROR;

	if (cmpObstructionManager->TestUnitShape(filter, x, z, r, NULL))
		return ICmpObstruction::FOUNDATION_CHECK_FAIL_OBSTRUCTS_FOUNDATION;

	// 测试地形和静态障碍物：

	u16 i, j;
	Pathfinding::NearestNavcell(x, z, i, j, m_GridSize, m_GridSize);
	if (!IS_PASSABLE(m_Grid->get(i, j), passClass))
		return ICmpObstruction::FOUNDATION_CHECK_FAIL_TERRAIN_CLASS;

	// （静态障碍物将在障碍物形状测试和导航单元格通行性测试中
	// 被冗余地测试，这有点低效，但不应影响行为）

	return ICmpObstruction::FOUNDATION_CHECK_SUCCESS;
}

// 检查建筑放置位置
ICmpObstruction::EFoundationCheck CCmpPathfinder::CheckBuildingPlacement(const IObstructionTestFilter& filter,
	entity_pos_t x, entity_pos_t z, entity_pos_t a, entity_pos_t w,
	entity_pos_t h, entity_id_t id, pass_class_t passClass) const
{
	return CCmpPathfinder::CheckBuildingPlacement(filter, x, z, a, w, h, id, passClass, false);
}

// 检查建筑放置位置 (重载版本)
ICmpObstruction::EFoundationCheck CCmpPathfinder::CheckBuildingPlacement(const IObstructionTestFilter& filter,
	entity_pos_t x, entity_pos_t z, entity_pos_t a, entity_pos_t w,
	entity_pos_t h, entity_id_t id, pass_class_t passClass, bool UNUSED(onlyCenterPoint)) const
{
	// 检查单位障碍物
	CmpPtr<ICmpObstructionManager> cmpObstructionManager(GetSystemEntity());
	if (!cmpObstructionManager)
		return ICmpObstruction::FOUNDATION_CHECK_FAIL_ERROR;

	if (cmpObstructionManager->TestStaticShape(filter, x, z, a, w, h, NULL))
		return ICmpObstruction::FOUNDATION_CHECK_FAIL_OBSTRUCTS_FOUNDATION;

	// 测试地形：

	ICmpObstructionManager::ObstructionSquare square;
	CmpPtr<ICmpObstruction> cmpObstruction(GetSimContext(), id);
	if (!cmpObstruction || !cmpObstruction->GetObstructionSquare(square))
		return ICmpObstruction::FOUNDATION_CHECK_FAIL_NO_OBSTRUCTION;

	entity_pos_t expand;
	const PathfinderPassability* passability = GetPassabilityFromMask(passClass);
	if (passability)
		expand = passability->m_Clearance;

	SimRasterize::Spans spans;
	SimRasterize::RasterizeRectWithClearance(spans, square, expand, Pathfinding::NAVCELL_SIZE);
	for (const SimRasterize::Span& span : spans)
	{
		i16 i0 = span.i0;
		i16 i1 = span.i1;
		i16 j = span.j;

		// 如果任何跨度超出了网格，则失败
		if (i0 < 0 || i1 > m_TerrainOnlyGrid->m_W || j < 0 || j > m_TerrainOnlyGrid->m_H)
			return ICmpObstruction::FOUNDATION_CHECK_FAIL_TERRAIN_CLASS;

		// 如果任何跨度包含不可通行的地块，则失败
		for (i16 i = i0; i < i1; ++i)
			if (!IS_PASSABLE(m_TerrainOnlyGrid->get(i, j), passClass))
				return ICmpObstruction::FOUNDATION_CHECK_FAIL_TERRAIN_CLASS;
	}

	return ICmpObstruction::FOUNDATION_CHECK_SUCCESS;
}
