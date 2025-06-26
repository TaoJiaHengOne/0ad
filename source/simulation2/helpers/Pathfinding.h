/* Copyright (C) 2022 Wildfire Games.
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

#ifndef INCLUDED_PATHFINDING
#define INCLUDED_PATHFINDING

#include "graphics/Terrain.h"
#include "maths/MathUtil.h"

#include "simulation2/system/Entity.h"
#include "PathGoal.h"

 // 前向声明，避免不必要的头文件包含
class CParamNode;

// 定义通行性类别的类型别名
typedef u16 pass_class_t;
// 前向声明 Grid 模板类
template<typename T>
class Grid;

/**
 * 远程路径请求的结构体
 */
struct LongPathRequest
{
	u32 ticket;               // 路径请求的唯一标识票据
	entity_pos_t x0;          // 起点 X 坐标
	entity_pos_t z0;          // 起点 Z 坐标
	PathGoal goal;            // 路径目标
	pass_class_t passClass;   // 通行性类别
	entity_id_t notify;       // 接收路径结果通知的实体ID
};

/**
 * 短程路径请求的结构体
 */
struct ShortPathRequest
{
	u32 ticket;               // 路径请求的唯一标识票据
	entity_pos_t x0;          // 起点 X 坐标
	entity_pos_t z0;          // 起点 Z 坐标
	entity_pos_t clearance;   // 单位的间隙（半径）
	entity_pos_t range;       // 寻路范围
	PathGoal goal;            // 路径目标
	pass_class_t passClass;   // 通行性类别
	bool avoidMovingUnits;    // 是否避开移动中的单位
	entity_id_t group;        // 控制组ID
	entity_id_t notify;       // 接收路径结果通知的实体ID
};

/**
 * 路径点结构体
 */
struct Waypoint
{
	entity_pos_t x, z; // 路径点的 X 和 Z 坐标
};

/**
 * 返回的路径。
 * 路径点是以*相反*的顺序存储的（最早的路径点在列表的末尾）
 */
struct WaypointPath
{
	std::vector<Waypoint> m_Waypoints; // 存储路径点的向量
};

/**
 * 表示在统一成本网格上由水平/垂直和对角线移动组成的路径成本。
 * 在溢出前，最大路径长度约为 45K 步。
 */
struct PathCost
{
	// 默认构造函数
	PathCost() : data(0) { }

	/// 从水平/垂直和对角线移动的步数构造
	PathCost(u16 hv, u16 d)
		: data(hv * 65536 + d * 92682) // 2^16 * sqrt(2) ≈ 92681.9
	{
	}

	/// 为给定步数的水平/垂直移动构造
	static PathCost horizvert(u16 n)
	{
		return PathCost(n, 0);
	}

	/// 为给定步数的对角线移动构造
	static PathCost diag(u16 n)
	{
		return PathCost(0, n);
	}

	// 加法运算符重载
	PathCost operator+(const PathCost& a) const
	{
		PathCost c;
		c.data = data + a.data;
		return c;
	}

	// 加法赋值运算符重载
	PathCost& operator+=(const PathCost& a)
	{
		data += a.data;
		return *this;
	}

	// 比较运算符重载
	bool operator<=(const PathCost& b) const { return data <= b.data; }
	bool operator< (const PathCost& b) const { return data < b.data; }
	bool operator>=(const PathCost& b) const { return data >= b.data; }
	bool operator>(const PathCost& b) const { return data > b.data; }

	// 转换为整数
	u32 ToInt()
	{
		return data;
	}

private:
	u32 data; // 存储成本的内部数据
};

// 定义通行性类别可用的位数
inline constexpr int PASS_CLASS_BITS = 16;
// 定义导航单元格的数据类型，每个通行性类别占1位
typedef u16 NavcellData;
// 宏：检查指定类别掩码在此单元格上是否可通行
#define IS_PASSABLE(item, classmask) (((item) & (classmask)) == 0)
// 宏：根据索引创建通行性类别掩码
#define PASS_CLASS_MASK_FROM_INDEX(id) ((pass_class_t)(1u << id))
// 宏：定义一个特殊的通行性类别掩码，用于临时的就地计算
#define SPECIAL_PASS_CLASS PASS_CLASS_MASK_FROM_INDEX((PASS_CLASS_BITS-1)) // 第16位

/**
 * 寻路相关的常量和辅助函数命名空间
 */
namespace Pathfinding
{
	/**
	 * 远程寻路器主要在一个导航网格（一个统一成本的二维通行性网格，
	 * 具有水平/垂直（非对角线）连接性）上操作。
	 * 这是基于地形地块的通行性，加上障碍物形状的光栅化结果，
	 * 所有这些都按单位半径向外扩展。
	 * 由于单位远小于地形地块，导航网格的分辨率应高于地块。
	 * 因此，我们将世界划分为 NxN 个“导航单元格”（nav cell）（其中N为整数，
	 * 最好是2的幂）。
	 */
	inline constexpr fixed NAVCELL_SIZE = fixed::FromInt(1); // 导航单元格的大小
	inline constexpr int NAVCELL_SIZE_INT = 1; // 导航单元格大小的整数形式
	inline constexpr int NAVCELL_SIZE_LOG2 = 0; // 导航单元格大小的以2为底的对数

	/**
	 * 地形网格更粗糙，从一种转换到另一种通常很方便。
	 */
	inline constexpr int NAVCELLS_PER_TERRAIN_TILE = TERRAIN_TILE_SIZE / NAVCELL_SIZE_INT; // 每个地形地块包含的导航单元格数量
	static_assert(TERRAIN_TILE_SIZE% NAVCELL_SIZE_INT == 0, "Terrain tile size is not a multiple of navcell size");

	/**
	 * 为了确保远程寻路器比短程寻路器更严格，
	 * 我们需要稍微过度光栅化。所以我们将间隙半径扩展1。
	 */
	inline constexpr entity_pos_t CLEARANCE_EXTENSION_RADIUS = fixed::FromInt(1);

	/**
	 * 计算离给定点最近的网格上的导航单元格索引
	 * w, h 是网格维度，即每边的导航单元格数量
	 */
	inline void NearestNavcell(entity_pos_t x, entity_pos_t z, u16& i, u16& j, u16 w, u16 h)
	{
		// 使用 NAVCELL_SIZE_INT 以节省定点数除法的开销
		i = static_cast<u16>(Clamp((x / NAVCELL_SIZE_INT).ToInt_RoundToNegInfinity(), 0, w - 1));
		j = static_cast<u16>(Clamp((z / NAVCELL_SIZE_INT).ToInt_RoundToNegInfinity(), 0, h - 1));
	}

	/**
	 * 返回给定地形地块中心的位置
	 */
	inline void TerrainTileCenter(u16 i, u16 j, entity_pos_t& x, entity_pos_t& z)
	{
		static_assert(TERRAIN_TILE_SIZE % 2 == 0);
		x = entity_pos_t::FromInt(i * (int)TERRAIN_TILE_SIZE + (int)TERRAIN_TILE_SIZE / 2);
		z = entity_pos_t::FromInt(j * (int)TERRAIN_TILE_SIZE + (int)TERRAIN_TILE_SIZE / 2);
	}

	/**
	 * 返回给定导航单元格中心的位置
	 */
	inline void NavcellCenter(u16 i, u16 j, entity_pos_t& x, entity_pos_t& z)
	{
		x = entity_pos_t::FromInt(i * 2 + 1).Multiply(NAVCELL_SIZE / 2);
		z = entity_pos_t::FromInt(j * 2 + 1).Multiply(NAVCELL_SIZE / 2);
	}

	/*
	 * 检查线段 (x0,z0)-(x1,z1) 是否与任何不可通行的导航单元格相交。
	 */
	bool CheckLineMovement(entity_pos_t x0, entity_pos_t z0, entity_pos_t x1, entity_pos_t z1,
		pass_class_t passClass, const Grid<NavcellData>& grid);
}

/*
 * 为了高效寻路，我们希望努力最小化每个地块的搜索成本，
 * 所以我们为不同类型的单位预计算地块的通行性标志。
 * 我们也希望最小化内存使用（很容易有10万个地块，所以我们不想
 * 为每个地块存储很多字节）。
 *
 * 为高效处理通行性，我们有少量的通行性类别
 * （例如“步兵”、“船只”）。每个单位属于单个通行性类别，并
 * 用它来进行所有寻路。
 * 通行性由水深、地形坡度、森林密集度、建筑密集度决定。
 * 每个地块每种类别至少需要一个比特位来表示通行性。
 *
 * 并非所有通行性类别都用于实际寻路。寻路器调用
 * CCmpObstructionManager 的 Rasterize() 方法将形状添加到通行性网格上。
 * 光栅化哪些形状取决于每个通行性类别的 m_Obstructions 值。
 *
 * 不用于单位寻路的通行性类别不应使用 Clearance 属性，
 * 并将获得零间隙值。
 */
class PathfinderPassability
{
public:
	// 构造函数，从 XML 节点初始化
	PathfinderPassability(pass_class_t mask, const CParamNode& node);

	// 检查在给定条件下是否可通行
	bool IsPassable(fixed waterdepth, fixed steepness, fixed shoredist) const
	{
		return ((m_MinDepth <= waterdepth && waterdepth <= m_MaxDepth) && (steepness < m_MaxSlope) && (m_MinShore <= shoredist && shoredist <= m_MaxShore));
	}

	pass_class_t m_Mask; // 通行性类别的位掩码

	fixed m_Clearance; // 与静态障碍物的最小距离

	// 障碍物处理方式枚举
	enum ObstructionHandling
	{
		NONE,        // 不处理障碍物
		PATHFINDING, // 用于寻路的障碍物
		FOUNDATION   // 用于地基的障碍物
	};
	ObstructionHandling m_Obstructions; // 障碍物处理方式

private:
	fixed m_MinDepth; // 最小水深
	fixed m_MaxDepth; // 最大水深
	fixed m_MaxSlope; // 最大坡度
	fixed m_MinShore; // 最小离岸距离
	fixed m_MaxShore; // 最大离岸距离
};

#endif // INCLUDED_PATHFINDING
