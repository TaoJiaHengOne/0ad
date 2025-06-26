/* Copyright (C) 2021 Wildfire Games.
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

#ifndef INCLUDED_LONGPATHFINDER
#define INCLUDED_LONGPATHFINDER

#include "Pathfinding.h"

#include "graphics/Overlay.h"
#include "renderer/Scene.h"
#include "renderer/TerrainOverlay.h"
#include "simulation2/helpers/Grid.h"
#include "simulation2/helpers/PriorityQueue.h"

#include <map>

 /**
  * 表示一个地块的二维坐标。
  * i/j 分量被打包到一个 u32 中，因为我们通常用这些
  * 对象进行相等性比较，而 VC2010 优化器似乎不会自动
  * 在单个操作中比较两个 u16。
  * TODO: 也许 VC2012 会？
  */
struct TileID
{
	// 默认构造函数
	TileID() { }

	// 构造函数，将 i 和 j 坐标打包
	TileID(u16 i, u16 j) : data((i << 16) | j) { }

	// 等于运算符重载
	bool operator==(const TileID& b) const
	{
		return data == b.data;
	}

	/// 返回 (i,j) 的字典序
	bool operator<(const TileID& b) const
	{
		return data < b.data;
	}

	// 获取 i 坐标
	u16 i() const { return data >> 16; }
	// 获取 j 坐标
	u16 j() const { return data & 0xFFFF; }

private:
	u32 data; // 存储打包后的坐标数据
};

/**
 * 用于 A* 计算的地块数据。
 * (我们为每个地形地块存储一个此类型的数组，所以它应该为大小进行优化)
 */
struct PathfindTile
{
public:
	// 状态枚举
	enum {
		STATUS_UNEXPLORED = 0, // 未探索
		STATUS_OPEN = 1,       // 在开放列表中
		STATUS_CLOSED = 2      // 在关闭列表中
	};

	// 检查状态的辅助函数
	bool IsUnexplored() const { return GetStatus() == STATUS_UNEXPLORED; }
	bool IsOpen() const { return GetStatus() == STATUS_OPEN; }
	bool IsClosed() const { return GetStatus() == STATUS_CLOSED; }
	void SetStatusOpen() { SetStatus(STATUS_OPEN); }
	void SetStatusClosed() { SetStatus(STATUS_CLOSED); }

	// 根据此地块的 i,j 坐标，获取最佳路径上前驱地块的 pi,pj 坐标
	inline int GetPredI(int i) const { return i + GetPredDI(); }
	inline int GetPredJ(int j) const { return j + GetPredDJ(); }

	// 获取到此地块的成本
	inline PathCost GetCost() const { return g; }
	// 设置到此地块的成本
	inline void SetCost(PathCost cost) { g = cost; }

private:
	PathCost g; // 到达此地块的成本
	u32 data;   // 2位状态；15位前驱I坐标偏移；15位前驱J坐标偏移；为存储效率而打包

public:
	// 获取状态
	inline u8 GetStatus() const
	{
		return data & 3;
	}

	// 设置状态
	inline void SetStatus(u8 s)
	{
		ASSERT(s < 4);
		data &= ~3;
		data |= (s & 3);
	}

	// 获取前驱的I坐标偏移
	int GetPredDI() const
	{
		return (i32)data >> 17;
	}

	// 获取前驱的J坐标偏移
	int GetPredDJ() const
	{
		return ((i32)data << 15) >> 17;
	}

	// 根据此地块的 i,j 坐标，设置前驱的 pi,pj 坐标
	inline void SetPred(int pi, int pj, int i, int j)
	{
		int di = pi - i;
		int dj = pj - j;
		ASSERT(-16384 <= di && di < 16384);
		ASSERT(-16384 <= dj && dj < 16384);
		data &= 3;
		data |= (((u32)di & 0x7FFF) << 17) | (((u32)dj & 0x7FFF) << 2);
	}
};

/**
 * 圆形区域结构体，用于定义排除区域
 */
struct CircularRegion
{
	CircularRegion(entity_pos_t x, entity_pos_t z, entity_pos_t r) : x(x), z(z), r(r) {}
	entity_pos_t x, z, r; // 圆心坐标和半径
};

// 类型别名，方便使用
typedef PriorityQueueHeap<TileID, PathCost, PathCost> PriorityQueue;
typedef SparseGrid<PathfindTile> PathfindTileGrid;

// 前向声明跳点缓存类
class JumpPointCache;

/**
 * 寻路器状态结构体，保存了寻路过程中的所有动态数据
 */
struct PathfinderState
{
	u32 steps; // 算法迭代次数

	PathGoal goal; // 路径目标

	u16 iGoal, jGoal; // 目标地块索引

	pass_class_t passClass; // 通行性类别

	PriorityQueue open; // 开放列表（优先队列）
	// (没有显式的关闭列表；它被编码在 PathfindTile 中)

	PathfindTileGrid* tiles;    // 指向地块数据网格的指针
	Grid<NavcellData>* terrain; // 指向地形通行性网格的指针

	PathCost hBest;      // 已发现的离目标最近的地块的启发式成本
	u16 iBest, jBest;    // 最近的地块索引

	const JumpPointCache* jpc; // 指向跳点缓存的指针
};

// 前向声明
class LongOverlay;
class HierarchicalPathfinder;

/**
 * 远程寻路器类
 */
class LongPathfinder
{
public:
	// 构造函数
	LongPathfinder();
	// 析构函数
	~LongPathfinder();

	/**
	 * 设置调试覆盖层的可见性
	 * @param enabled 是否启用
	 */
	void SetDebugOverlay(bool enabled);

	/**
	 * 设置调试路径，用于在渲染中显示
	 * @param hierPath 分层寻路器引用
	 * @param x0, z0 起点坐标
	 * @param goal 目标
	 * @param passClass 通行性类别
	 */
	void SetDebugPath(const HierarchicalPathfinder& hierPath, entity_pos_t x0, entity_pos_t z0, const PathGoal& goal, pass_class_t passClass)
	{
		if (!m_Debug.Overlay)
			return;

		SAFE_DELETE(m_Debug.Grid);
		delete m_Debug.Path;
		m_Debug.Path = new WaypointPath();
		ComputePath(hierPath, x0, z0, goal, passClass, *m_Debug.Path);
		m_Debug.PassClass = passClass;
	}

	/**
	 * 使用新的通行性网格重新加载寻路器
	 * @param passabilityGrid 新的通行性网格指针
	 */
	void Reload(Grid<NavcellData>* passabilityGrid)
	{
		m_Grid = passabilityGrid;
		ASSERT(passabilityGrid->m_H == passabilityGrid->m_W);
		m_GridSize = passabilityGrid->m_W;

		m_JumpPointCache.clear();
	}

	/**
	 * 使用新的通行性网格更新寻路器
	 * @param passabilityGrid 新的通行性网格指针
	 */
	void Update(Grid<NavcellData>* passabilityGrid)
	{
		m_Grid = passabilityGrid;
		ASSERT(passabilityGrid->m_H == passabilityGrid->m_W);
		ASSERT(m_GridSize == passabilityGrid->m_H);

		m_JumpPointCache.clear();
	}

	/**
	 * 计算从给定点到目标的基于地块的路径，并返回路径点集合。
	 * 路径点对应于路径上水平/垂直相邻地块的中心。
	 */
	void ComputePath(const HierarchicalPathfinder& hierPath, entity_pos_t x0, entity_pos_t z0, const PathGoal& origGoal,
		pass_class_t passClass, WaypointPath& path) const;

	/**
	 * 计算从给定点到目标的基于地块的路径，排除
	 * 在 excludedRegions 中指定的区域（这些区域被视为不可通行），并返回路径点集合。
	 * 路径点对应于路径上水平/垂直相邻地块的中心。
	 */
	void ComputePath(const HierarchicalPathfinder& hierPath, entity_pos_t x0, entity_pos_t z0, const PathGoal& origGoal,
		pass_class_t passClass, std::vector<CircularRegion> excludedRegions, WaypointPath& path);

	/**
	 * 获取调试数据
	 * @param steps 步数
	 * @param time 时间
	 * @param grid 调试网格
	 */
	void GetDebugData(u32& steps, double& time, Grid<u8>& grid) const
	{
		GetDebugDataJPS(steps, time, grid);
	}

	Grid<NavcellData>* m_Grid; // 通行性网格指针
	u16 m_GridSize; // 网格大小

	// 调试 - 上次寻路操作的输出。
	struct Debug
	{
		// 原子变量 - 用于切换调试。
		std::atomic<LongOverlay*> Overlay = nullptr;
		// 可变成员 - 由 ComputeJPSPath 设置（因此可能来自不同线程）。
		// 如有必要，通过互斥锁同步。
		mutable PathfindTileGrid* Grid = nullptr;
		mutable u32 Steps;
		mutable double Time;
		mutable PathGoal Goal;

		WaypointPath* Path = nullptr;
		pass_class_t PassClass;
	} m_Debug;

private:
	/**
	 * 计算启发式成本
	 */
	PathCost CalculateHeuristic(int i, int j, int iGoal, int jGoal) const;
	/**
	 * 处理一个邻居节点
	 */
	void ProcessNeighbour(int pi, int pj, int i, int j, PathCost pg, PathfinderState& state) const;

	/**
	 * JPS 算法辅助函数
	 * @param detectGoal 如果 m_UseJPSCache 为 true，则不使用此参数
	 */
	void AddJumpedHoriz(int i, int j, int di, PathCost g, PathfinderState& state, bool detectGoal) const;
	int HasJumpedHoriz(int i, int j, int di, PathfinderState& state, bool detectGoal) const;
	void AddJumpedVert(int i, int j, int dj, PathCost g, PathfinderState& state, bool detectGoal) const;
	int HasJumpedVert(int i, int j, int dj, PathfinderState& state, bool detectGoal) const;
	void AddJumpedDiag(int i, int j, int di, int dj, PathCost g, PathfinderState& state) const;

	/**
	 * 详见 LongPathfinder.cpp 中的实现细节
	 * TODO: 清理文档
	 */
	void ComputeJPSPath(const HierarchicalPathfinder& hierPath, entity_pos_t x0, entity_pos_t z0, const PathGoal& origGoal, pass_class_t passClass, WaypointPath& path) const;
	/**
	 * 获取JPS算法的调试数据
	 */
	void GetDebugDataJPS(u32& steps, double& time, Grid<u8>& grid) const;

	// ComputePath 的辅助函数

	/**
	 * 给定一个具有任意路径点集合的路径，更新
	 * 路径点以使其更优。
	 * 如果 @param maxDist 非零，路径点之间的间距最多为 @param maxDist。
	 * 在这种情况下，(x0, z0) 与第一个路径点之间的距离也将小于 maxDist。
	 */
	void ImprovePathWaypoints(WaypointPath& path, pass_class_t passClass, entity_pos_t maxDist, entity_pos_t x0, entity_pos_t z0) const;

	/**
	 * 生成一个通行性地图，存储在导航单元格的第16位，基于 passClass，
	 * 但带有一组不可通行的圆形区域。
	 */
	void GenerateSpecialMap(pass_class_t passClass, std::vector<CircularRegion> excludedRegions);

	bool m_UseJPSCache; // 是否使用跳点缓存
	// 这里可以使用 mutable，因为缓存不会改变远程寻路器的外部 const 性质。
	// 这是线程安全的，因为它是顺序无关的（对于给定的一组参数，函数的输出没有变化）。
	// 显然，这意味着缓存实际上应该是一个缓存，而不应返回与
	// 未缓存时不同的结果。
	mutable std::map<pass_class_t, std::shared_ptr<JumpPointCache>> m_JumpPointCache;
};

#endif // INCLUDED_LONGPATHFINDER
