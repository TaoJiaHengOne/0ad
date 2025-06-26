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

#ifndef INCLUDED_CCMPPATHFINDER_COMMON
#define INCLUDED_CCMPPATHFINDER_COMMON

 /**
  * @file
  * 声明 CCmpPathfinder 类。其实现主要在 CCmpPathfinder.cpp 中完成，
  * 但短程（基于顶点）的寻路是在 CCmpPathfinder_Vertex.cpp 中完成的。
  * 本文件提供了这两个文件所需的通用代码。
  *
  * 远程寻路是由一个 LongPathfinder 对象完成的。
  */

#include "simulation2/system/Component.h"

#include "ICmpPathfinder.h"

#include "graphics/Overlay.h"
#include "graphics/Terrain.h"
#include "maths/MathUtil.h"
#include "ps/CLogger.h"
#include "ps/TaskManager.h"
#include "renderer/TerrainOverlay.h"
#include "simulation2/components/ICmpObstructionManager.h"
#include "simulation2/helpers/Grid.h"

#include <vector>

  // 前向声明，避免不必要的头文件包含
class HierarchicalPathfinder;
class LongPathfinder;
class VertexPathfinder;

class SceneCollector;
class AtlasOverlay;

// 预处理器宏，用于在非调试版本中禁用寻路调试功能
#ifdef NDEBUG
#define PATHFIND_DEBUG 0
#else
#define PATHFIND_DEBUG 1
#endif

/**
 * ICmpPathfinder 接口的实现类
 */
class CCmpPathfinder final : public ICmpPathfinder
{
public:
	// 静态类初始化函数，用于在组件管理器中注册消息订阅
	static void ClassInit(CComponentManager& componentManager)
	{
		componentManager.SubscribeToMessageType(MT_Deserialized); // 订阅反序列化完成消息
		componentManager.SubscribeToMessageType(MT_RenderSubmit); // 用于调试覆盖层
		componentManager.SubscribeToMessageType(MT_TerrainChanged); // 订阅地形变更消息
		componentManager.SubscribeToMessageType(MT_WaterChanged);   // 订阅水体变更消息
		componentManager.SubscribeToMessageType(MT_ObstructionMapShapeChanged); // 订阅障碍物地图形状变更消息
	}

	// 析构函数
	~CCmpPathfinder();

	// 使用默认的组件分配器
	DEFAULT_COMPONENT_ALLOCATOR(Pathfinder)

		// 模板状态 (从模板文件中加载的静态数据):

	std::map<std::string, pass_class_t> m_PassClassMasks; // 通行性类别名称到掩码的映射
	std::vector<PathfinderPassability> m_PassClasses;      // 通行性类别数据列表
	u16 m_MaxSameTurnMoves; // 在 StartProcessingMoves 中当 useMax 为 true 时，一回合内最多计算的路径数量。

	// 动态状态 (游戏运行时的数据):

	// 延迟构建的动态状态 (不被序列化):

	u16 m_GridSize; // 地图每边的导航单元格（Navcell）数量。
	Grid<NavcellData>* m_Grid; // 包含地形/通行性信息的格子图
	Grid<NavcellData>* m_TerrainOnlyGrid; // 与 m_Grid 相同，但只包含地形信息，以避免一些重复计算

	// 将智能更新信息保存在内存中，以避免格子图导致的内存碎片。
	// 这只应在 UpdateGrid() 中使用，不保证数据在其他任何地方都被正确初始化。
	GridUpdateInformation m_DirtinessInformation; // 当前的“脏”区域信息
	// 来自智能更新的数据为AI管理器存储
	GridUpdateInformation m_AIPathfinderDirtinessInformation; // 供AI使用的“脏”区域信息
	bool m_TerrainDirty; // 地形是否变“脏”的标志

	std::vector<VertexPathfinder> m_VertexPathfinders; // 短程（顶点）寻路器实例列表
	std::unique_ptr<HierarchicalPathfinder> m_PathfinderHier; // 指向分层寻路器的智能指针
	std::unique_ptr<LongPathfinder> m_LongPathfinder; // 指向远程寻路器的智能指针

	// 每个活动的异步路径计算任务对应一个。
	std::vector<Future<void>> m_Futures;

	// 路径请求的模板类
	template<typename T>
	class PathRequests {
	public:
		std::vector<T> m_Requests; // 存储路径请求的列表
		std::vector<PathResult> m_Results; // 存储路径计算结果的列表
		// 这是下一个要计算的路径在数组中的索引。
		std::atomic<size_t> m_NextPathToCompute = 0;
		// 在所有已调度的路径被计算完成之前，此值为 false。
		std::atomic<bool> m_ComputeDone = true;

		// 清除已计算的请求
		void ClearComputed()
		{
			if (m_Results.size() == m_Requests.size())
				m_Requests.clear();
			else
				m_Requests.erase(m_Requests.end() - m_Results.size(), m_Requests.end());
			m_Results.clear();
		}

		/**
		 * @param max - 如果非零，表示要处理多少条路径。
		 */
		void PrepareForComputation(u16 max)
		{
			size_t n = m_Requests.size();
			if (max && n > max)
				n = max;
			m_NextPathToCompute = 0;
			m_Results.resize(n);
			m_ComputeDone = n == 0;
		}

		// 模板函数，用于计算路径
		template<typename U>
		void Compute(const CCmpPathfinder& cmpPathfinder, const U& pathfinder);
	};
	PathRequests<LongPathRequest> m_LongPathRequests; // 远程路径请求
	PathRequests<ShortPathRequest> m_ShortPathRequests; // 短程路径请求

	u32 m_NextAsyncTicket; // 用于异步路径请求的唯一ID。

	AtlasOverlay* m_AtlasOverlay; // 指向在 Atlas 编辑器中使用的覆盖层

	// 获取组件的 Schema 定义
	static std::string GetSchema()
	{
		return "<a:component type='system'/><empty/>";
	}

	// 组件初始化
	void Init(const CParamNode& paramNode) override;

	// 组件销毁
	void Deinit() override;

	// 通用的序列化/反序列化模板函数
	template<typename S>
	void SerializeCommon(S& serialize);

	// 序列化
	void Serialize(ISerializer& serialize) override;

	// 反序列化
	void Deserialize(const CParamNode& paramNode, IDeserializer& deserialize) override;

	// 消息处理函数
	void HandleMessage(const CMessage& msg, bool global) override;

	// 获取通行性类别
	pass_class_t GetPassabilityClass(const std::string& name) const override;

	// 获取所有通行性类别
	void GetPassabilityClasses(std::map<std::string, pass_class_t>& passClasses) const override;
	void GetPassabilityClasses(
		std::map<std::string, pass_class_t>& nonPathfindingPassClasses,
		std::map<std::string, pass_class_t>& pathfindingPassClasses) const override;

	// 根据掩码获取通行性数据
	const PathfinderPassability* GetPassabilityFromMask(pass_class_t passClass) const;

	// 获取单位间隙
	entity_pos_t GetClearance(pass_class_t passClass) const override
	{
		const PathfinderPassability* passability = GetPassabilityFromMask(passClass);
		if (!passability)
			return fixed::Zero();

		return passability->m_Clearance;
	}

	// 获取最大单位间隙
	entity_pos_t GetMaximumClearance() const override
	{
		entity_pos_t max = fixed::Zero();

		for (const PathfinderPassability& passability : m_PassClasses)
			if (passability.m_Clearance > max)
				max = passability.m_Clearance;

		return max + Pathfinding::CLEARANCE_EXTENSION_RADIUS;
	}

	// 获取通行性格子图
	const Grid<NavcellData>& GetPassabilityGrid() override;

	// 获取AI寻路器的“脏”信息
	const GridUpdateInformation& GetAIPathfinderDirtinessInformation() const override
	{
		return m_AIPathfinderDirtinessInformation;
	}

	// 清空AI寻路器的“脏”信息
	void FlushAIPathfinderDirtinessInformation() override
	{
		m_AIPathfinderDirtinessInformation.Clean();
	}

	// 计算海岸线格子图
	Grid<u16> ComputeShoreGrid(bool expandOnWater = false) override;

	// 立即计算长程路径
	void ComputePathImmediate(entity_pos_t x0, entity_pos_t z0, const PathGoal& goal, pass_class_t passClass, WaypointPath& ret) const override;

	// 异步计算长程路径
	u32 ComputePathAsync(entity_pos_t x0, entity_pos_t z0, const PathGoal& goal, pass_class_t passClass, entity_id_t notify) override;

	// 立即计算短程路径
	WaypointPath ComputeShortPathImmediate(const ShortPathRequest& request) const override;

	// 异步计算短程路径
	u32 ComputeShortPathAsync(entity_pos_t x0, entity_pos_t z0, entity_pos_t clearance, entity_pos_t range, const PathGoal& goal, pass_class_t passClass, bool avoidMovingUnits, entity_id_t controller, entity_id_t notify) override;

	// 检查目标是否可达
	bool IsGoalReachable(entity_pos_t x0, entity_pos_t z0, const PathGoal& goal, pass_class_t passClass) override;

	// 设置调试路径
	void SetDebugPath(entity_pos_t x0, entity_pos_t z0, const PathGoal& goal, pass_class_t passClass) override;

	// 设置调试覆盖层
	void SetDebugOverlay(bool enabled) override;

	// 设置分层寻路器的调试覆盖层
	void SetHierDebugOverlay(bool enabled) override;

	// 获取调试数据
	void GetDebugData(u32& steps, double& time, Grid<u8>& grid) const override;

	// 在Atlas中设置覆盖层
	void SetAtlasOverlay(bool enable, pass_class_t passClass = 0) override;

	// 围绕一点分布单位
	std::vector<CFixedVector2D> DistributeAround(std::vector<entity_id_t> units, entity_pos_t x, entity_pos_t z) const override;

	// 检查移动路线
	bool CheckMovement(const IObstructionTestFilter& filter, entity_pos_t x0, entity_pos_t z0, entity_pos_t x1, entity_pos_t z1, entity_pos_t r, pass_class_t passClass) const override;

	// 检查单位放置位置
	ICmpObstruction::EFoundationCheck CheckUnitPlacement(const IObstructionTestFilter& filter, entity_pos_t x, entity_pos_t z, entity_pos_t r, pass_class_t passClass, bool onlyCenterPoint) const override;

	// 检查建筑放置位置
	ICmpObstruction::EFoundationCheck CheckBuildingPlacement(const IObstructionTestFilter& filter, entity_pos_t x, entity_pos_t z, entity_pos_t a, entity_pos_t w, entity_pos_t h, entity_id_t id, pass_class_t passClass) const override;

	// 检查建筑放置位置 (重载版本)
	ICmpObstruction::EFoundationCheck CheckBuildingPlacement(const IObstructionTestFilter& filter, entity_pos_t x, entity_pos_t z, entity_pos_t a, entity_pos_t w, entity_pos_t h, entity_id_t id, pass_class_t passClass, bool onlyCenterPoint) const override;

	// 发送已请求的路径
	void SendRequestedPaths() override;

	// 开始处理移动请求
	void StartProcessingMoves(bool useMax) override;

	// 获取要处理的移动请求列表
	template <typename T>
	std::vector<T> GetMovesToProcess(std::vector<T>& requests, bool useMax = false, size_t maxMoves = 0);

	// 将请求推送到工作线程
	template <typename T>
	void PushRequestsToWorkers(std::vector<T>& from);

	/**
	 * 如有必要，根据当前的障碍物列表重新生成格子图。
	 */
	void UpdateGrid() override;

	/**
	 * 在不更新“脏”信息的情况下更新仅包含地形的格子图。
	 * 用于在 Atlas 中进行快速的通行性更新。
	 */
	void MinimalTerrainUpdate(int itile0, int jtile0, int itile1, int jtile1);

	/**
	 * 重新生成仅包含地形的格子图。
	 * Atlas 不需要扩展通行性单元格。
	 */
	void TerrainUpdateHelper(bool expandPassability = true, int itile0 = -1, int jtile0 = -1, int itile1 = -1, int jtile1 = -1);

	// 提交渲染任务
	void RenderSubmit(SceneCollector& collector);
};

/**
 * Atlas 编辑器中使用的地形纹理覆盖层类。
 */
class AtlasOverlay : public TerrainTextureOverlay
{
public:
	const CCmpPathfinder* m_Pathfinder; // 指向寻路器组件的指针
	pass_class_t m_PassClass; // 要显示的通行性类别

	// 构造函数
	AtlasOverlay(const CCmpPathfinder* pathfinder, pass_class_t passClass) :
		TerrainTextureOverlay(Pathfinding::NAVCELLS_PER_TERRAIN_TILE), m_Pathfinder(pathfinder), m_PassClass(passClass)
	{
	}

	// 构建 RGBA 纹理数据
	void BuildTextureRGBA(u8* data, size_t w, size_t h) override
	{
		// 根据仅包含地形的格子图，渲染导航单元格的通行性
		u8* p = data;
		for (size_t j = 0; j < h; ++j)
		{
			for (size_t i = 0; i < w; ++i)
			{
				SColor4ub color(0, 0, 0, 0); // 默认透明
				// 如果此单元格对于指定的通行性类别是不可通行的
				if (!IS_PASSABLE(m_Pathfinder->m_TerrainOnlyGrid->get((int)i, (int)j), m_PassClass))
					color = SColor4ub(255, 0, 0, 127); // 设为半透明红色

				// 写入颜色数据
				*p++ = color.R;
				*p++ = color.G;
				*p++ = color.B;
				*p++ = color.A;
			}
		}
	}
};

#endif // INCLUDED_CCMPPATHFINDER_COMMON
