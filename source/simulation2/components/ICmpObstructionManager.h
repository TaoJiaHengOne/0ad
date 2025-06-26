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

#ifndef INCLUDED_ICMPOBSTRUCTIONMANAGER
#define INCLUDED_ICMPOBSTRUCTIONMANAGER

#include "simulation2/system/Interface.h"

#include "maths/FixedVector2D.h"
#include "simulation2/helpers/Position.h"

#include <vector>

 // 前向声明，避免不必要的头文件包含
class IObstructionTestFilter;
template<typename T>
class Grid;
struct GridUpdateInformation;
using NavcellData = u16;
class PathfinderPassability;


/**
 * 障碍物管理器：提供对世界中物体的高效空间查询。
 *
 * 该类处理两种类型的形状：
 * “静态”形状，通常代表建筑物，是具有给定宽度、高度和角度的矩形；
 * 和“单位”形状，代表可以在世界中移动的单位，它们有半径但没有旋转。
 * （由于短程寻路器使用的算法，单位有时表现为轴对齐的正方形，有时
 * 近似为圆形。）
 *
 * 其他类（特别是ICmpObstruction）向此接口注册形状并保持其更新。
 *
 * @c Test 函数提供精确的碰撞测试。
 * 就碰撞而言，形状的边缘算作形状的“内部”。
 * 这些函数接受一个 IObstructionTestFilter 参数，它可以限制
 * 被计为碰撞的形状集合。
 *
 * 单位可以被标记为移动或静止，这仅决定了
 * 某些过滤器是包含还是排除它们。
 *
 * @c Rasterize 函数将当前的形状集合近似到一个二维网格上，
 * 以便用于基于地块的寻路。
 */
class ICmpObstructionManager : public IComponent
{
public:
	/**
	 * 所有类型形状的标准表示，供几何处理代码使用。
	 */
	struct ObstructionSquare
	{
		entity_pos_t x, z;   // 中心位置
		CFixedVector2D u, v; // 代表方向的正交单位向量，“水平”和“垂直”
		entity_pos_t hw, hh; // 正方形的半宽和半高
	};

	/**
	 * 形状的外部标识符。
	 * （这是一个结构体而不是原始的u32，以保证类型安全。）
	 */
	struct tag_t
	{
		// 默认构造函数
		tag_t() : n(0) {}
		// 显式构造函数
		explicit tag_t(u32 n) : n(n) {}
		// 检查标签是否有效
		bool valid() const { return n != 0; }

		u32 n; // 标签的实际数值
	};

	/**
	 * 影响形状障碍物行为的布尔标志位。
	 */
	enum EFlags
	{
		FLAG_BLOCK_MOVEMENT = (1 << 0), // 阻止单位移动通过此形状
		FLAG_BLOCK_FOUNDATION = (1 << 1), // 阻止在此形状上放置地基
		FLAG_BLOCK_CONSTRUCTION = (1 << 2), // 阻止在此形状上建造建筑
		FLAG_BLOCK_PATHFINDING = (1 << 3), // 阻止地块寻路器选择通过此形状的路径
		FLAG_MOVING = (1 << 4), // 为 unitMotion 保留 - 参见其用法。
		FLAG_DELETE_UPON_CONSTRUCTION = (1 << 5)  // 当放置在此实体之上的建筑开始建造时，删除此实体
	};

	/**
	 * EFlag 值的位掩码。
	 */
	typedef u8 flags_t;

	/**
	 * 设置世界的边界。
	 * 任何在边界之外的点都被视为有障碍物。
	 * @param x0,z0,x1,z1 世界角落的坐标
	 */
	virtual void SetBounds(entity_pos_t x0, entity_pos_t z0, entity_pos_t x1, entity_pos_t z1) = 0;

	/**
	 * 注册一个静态形状。
	 *
	 * @param ent 与此形状关联的实体ID（如果没有则为INVALID_ENTITY）
	 * @param x,z 中心的世界空间坐标
	 * @param a 旋转角度（从+Z方向顺时针）
	 * @param w 宽度（沿X轴的大小）
	 * @param h 高度（沿Z轴的大小）
	 * @param flags 一组EFlags值
	 * @param group 形状的主要控制组。必须是一个有效的控制组ID。
	 * @param group2 可选；形状的次要控制组。默认为INVALID_ENTITY。
	 * @return 一个用于操作该形状的有效标签
	 * @see StaticShape
	 */
	virtual tag_t AddStaticShape(entity_id_t ent, entity_pos_t x, entity_pos_t z, entity_angle_t a,
		entity_pos_t w, entity_pos_t h, flags_t flags, entity_id_t group, entity_id_t group2 = INVALID_ENTITY) = 0;

	/**
	 * 注册一个单位形状。
	 *
	 * @param ent 与此形状关联的实体ID（如果没有则为INVALID_ENTITY）
	 * @param x,z 中心的世界空间坐标
	 * @param clearance 单位的寻路间隙（作为半径使用）
	 * @param flags 一组EFlags值
	 * @param group 控制组（通常是所属实体，或阵型控制器实体
	 * - 单位会忽略与同组内其他单位的碰撞）
	 * @return 一个用于操作该形状的有效标签
	 * @see UnitShape
	 */
	virtual tag_t AddUnitShape(entity_id_t ent, entity_pos_t x, entity_pos_t z, entity_pos_t clearance,
		flags_t flags, entity_id_t group) = 0;

	/**
	 * 调整一个已存在形状的位置和角度。
	 * @param tag 形状的标签（必须有效）
	 * @param x 中心的X坐标，世界空间
	 * @param z 中心的Z坐标，世界空间
	 * @param a 旋转角度（从+Z方向顺时针）；对单位形状无效
	 */
	virtual void MoveShape(tag_t tag, entity_pos_t x, entity_pos_t z, entity_angle_t a) = 0;

	/**
	 * 设置一个单位形状是移动还是静止。
	 * @param tag 形状的标签（必须有效且为单位形状）
	 * @param moving 单位当前是否在世界中移动，或为静止
	 */
	virtual void SetUnitMovingFlag(tag_t tag, bool moving) = 0;

	/**
	 * 设置一个单位形状的控制组。
	 * @param tag 形状的标签（必须有效且为单位形状）
	 * @param group 控制组的实体ID
	 */
	virtual void SetUnitControlGroup(tag_t tag, entity_id_t group) = 0;

	/**
	 * 设置一个静态形状的控制组。
	 * @param tag 要设置控制组的形状标签。必须是有效且为静态形状的标签。
	 * @param group 控制组的实体ID。
	 */
	virtual void SetStaticControlGroup(tag_t tag, entity_id_t group, entity_id_t group2) = 0;

	/**
	 * 移除一个已存在的形状。此后该标签将失效，不得再使用。
	 * @param tag 形状的标签（必须有效）
	 */
	virtual void RemoveShape(tag_t tag) = 0;

	/**
	 * 返回从障碍物到点(px, pz)的距离，如果实体不在世界中，则返回-1。
	 */
	virtual fixed DistanceToPoint(entity_id_t ent, entity_pos_t px, entity_pos_t pz) const = 0;

	/**
	 * 计算实体与点之间的最大直线距离。
	 */
	virtual fixed MaxDistanceToPoint(entity_id_t ent, entity_pos_t px, entity_pos_t pz) const = 0;

	/**
	 * 计算实体与目标之间的最短距离。
	 */
	virtual fixed DistanceToTarget(entity_id_t ent, entity_id_t target) const = 0;

	/**
	 * 计算实体与目标之间的最大直线距离。
	 */
	virtual fixed MaxDistanceToTarget(entity_id_t ent, entity_id_t target) const = 0;

	/**
	 * 计算源形状与目标形状之间的最短直线距离。
	 */
	virtual fixed DistanceBetweenShapes(const ObstructionSquare& source, const ObstructionSquare& target) const = 0;

	/**
	 * 计算源形状与目标形状之间的最大直线距离。
	 */
	virtual fixed MaxDistanceBetweenShapes(const ObstructionSquare& source, const ObstructionSquare& target) const = 0;

	/**
	 * 检查给定实体是否在给定参数的其他点的范围内。
	 * @param maxRange - 可以是一个非负小数，ALWAYS_IN_RANGE 或 NEVER_IN_RANGE。
	 */
	virtual bool IsInPointRange(entity_id_t ent, entity_pos_t px, entity_pos_t pz, entity_pos_t minRange, entity_pos_t maxRange, bool opposite) const = 0;

	/**
	 * 检查给定实体是否在给定参数的目标范围内。
	 * @param maxRange - 可以是一个非负小数，ALWAYS_IN_RANGE 或 NEVER_IN_RANGE。
	 */
	virtual bool IsInTargetRange(entity_id_t ent, entity_id_t target, entity_pos_t minRange, entity_pos_t maxRange, bool opposite) const = 0;

	/**
	 * 检查给定实体是否在给定参数的目标的抛物线范围内。
	 * @param maxRange - 可以是一个非负小数，ALWAYS_IN_RANGE 或 NEVER_IN_RANGE。
	 */
	virtual bool IsInTargetParabolicRange(entity_id_t ent, entity_id_t target, entity_pos_t minRange, entity_pos_t maxRange, entity_pos_t yOrigin, bool opposite) const = 0;

	/**
	 * 检查给定点是否在给定参数的其他点的范围内。
	 * @param maxRange - 可以是一个非负小数，ALWAYS_IN_RANGE 或 NEVER_IN_RANGE。
	 */
	virtual bool IsPointInPointRange(entity_pos_t x, entity_pos_t z, entity_pos_t px, entity_pos_t pz, entity_pos_t minRange, entity_pos_t maxRange) const = 0;

	/**
	 * 检查给定形状是否在给定参数的目标形状范围内。
	 * @param maxRange - 可以是一个非负小数，ALWAYS_IN_RANGE 或 NEVER_IN_RANGE。
	 */
	virtual bool AreShapesInRange(const ObstructionSquare& source, const ObstructionSquare& target, entity_pos_t minRange, entity_pos_t maxRange, bool opposite) const = 0;

	/**
	 * 对一条平头粗线与当前的形状集合进行碰撞测试。
	 * 线帽在端点之外延伸 @p r 的距离。
	 * 只计算从形状外部进入内部的交集。
	 * @param filter 用于限制被计数的形状的过滤器
	 * @param x0 线的第一个点的X坐标
	 * @param z0 线的第一个点的Z坐标
	 * @param x1 线的第二个点的X坐标
	 * @param z1 线的第二个点的Z坐标
	 * @param r 线的半径（半宽）
	 * @param relaxClearanceForUnits 单位间的碰撞是否应该更宽松。
	 * @return 如果有碰撞则返回true
	 */
	virtual bool TestLine(const IObstructionTestFilter& filter, entity_pos_t x0, entity_pos_t z0, entity_pos_t x1, entity_pos_t z1, entity_pos_t r, bool relaxClearanceForUnits) const = 0;

	/**
	 * 对一条单位的移动路线进行碰撞测试
	 */
	virtual bool TestUnitLine(const IObstructionTestFilter& filter, entity_pos_t x0, entity_pos_t z0, entity_pos_t x1, entity_pos_t z1, entity_pos_t r, bool relaxClearanceForUnits) const = 0;

	/**
	 * 对一个静态正方形形状与当前的形状集合进行碰撞测试。
	 * @param filter 用于限制被测试的形状的过滤器
	 * @param x 中心的X坐标
	 * @param z 中心的Z坐标
	 * @param a 旋转角度（从+Z方向顺时针）
	 * @param w 宽度（沿X轴的大小）
	 * @param h 高度（沿Z轴的大小）
	 * @param out 如果非NULL，所有碰撞形状的实体将被添加到此列表中
	 * @return 如果有碰撞则返回true
	 */
	virtual bool TestStaticShape(const IObstructionTestFilter& filter,
		entity_pos_t x, entity_pos_t z, entity_pos_t a, entity_pos_t w, entity_pos_t h,
		std::vector<entity_id_t>* out) const = 0;

	/**
	 * 对一个单位形状与当前注册的形状集合进行碰撞测试，并可选择性地将碰撞
	 * 形状的实体列表写入输出列表。
	 *
	 * @param filter 用于限制被测试的形状的过滤器
	 * @param x 形状中心的X坐标
	 * @param z 形状中心的Z坐标
	 * @param clearance 形状单位的间隙
	 * @param out 如果非NULL，所有碰撞形状的实体将被添加到此列表中
	 *
	 * @return 如果有碰撞则返回true
	 */
	virtual bool TestUnitShape(const IObstructionTestFilter& filter,
		entity_pos_t x, entity_pos_t z, entity_pos_t clearance,
		std::vector<entity_id_t>* out) const = 0;

	/**
	 * 将当前的形状集合光栅化到一个导航单元格网格上，适用于 @p passClasses 中包含的所有通行性类别。
	 * 如果 @p fullUpdate 为 false，该函数将只处理“脏”的形状。
	 * 形状通过将它们的掩码与 @p grid 进行“或”运算来按 @p passClasses 的间隙进行扩展。
	 */
	virtual void Rasterize(Grid<NavcellData>& grid, const std::vector<PathfinderPassability>& passClasses, bool fullUpdate) = 0;

	/**
	 * 获取“脏”信息并在之后重置它。然后由CCmpPathfinder的角色
	 * 在需要时将信息传递给其他组件（如AI等）。
	 * 如果不需要更新，返回值为false。
	 */
	virtual void UpdateInformations(GridUpdateInformation& informations) = 0;

	/**
	 * 找到所有在给定范围内部（或部分在内部）的障碍物。
	 * @param filter 用于限制被计数的形状的过滤器
	 * @param x0 范围左边缘的X坐标
	 * @param z0 范围底边缘的Z坐标
	 * @param x1 范围右边缘的X坐标
	 * @param z1 范围顶边缘的Z坐标
	 * @param squares 障碍物的输出列表
	 */
	virtual void GetObstructionsInRange(const IObstructionTestFilter& filter, entity_pos_t x0, entity_pos_t z0, entity_pos_t x1, entity_pos_t z1, std::vector<ObstructionSquare>& squares) const = 0;
	/**
	 * 在给定范围内获取静态障碍物
	 */
	virtual void GetStaticObstructionsInRange(const IObstructionTestFilter& filter, entity_pos_t x0, entity_pos_t z0, entity_pos_t x1, entity_pos_t z1, std::vector<ObstructionSquare>& squares) const = 0;
	/**
	 * 在给定范围内获取单位障碍物
	 */
	virtual void GetUnitObstructionsInRange(const IObstructionTestFilter& filter, entity_pos_t x0, entity_pos_t z0, entity_pos_t x1, entity_pos_t z1, std::vector<ObstructionSquare>& squares) const = 0;
	/**
	 * 获取与给定障碍物重叠的其他静态障碍物
	 */
	virtual void GetStaticObstructionsOnObstruction(const ObstructionSquare& square, std::vector<entity_id_t>& out, const IObstructionTestFilter& filter) const = 0;

	/**
	 * 返回与给定障碍物正方形相交的所有单位形状的实体ID，
	 * 并使用给定的过滤器进行筛选。
	 * @param square 我们想要比较的障碍物正方形。
	 * @param out 障碍物的输出列表
	 * @param filter 用于筛选障碍单位的过滤器
	 * @param strict 检查时是严格还是更宽松（即光栅化或多或少）。默认为false。
	 */
	virtual void GetUnitsOnObstruction(const ObstructionSquare& square, std::vector<entity_id_t>& out, const IObstructionTestFilter& filter, bool strict = false) const = 0;

	/**
	 * 获取代表给定形状的障碍物正方形。
	 * @param tag 形状的标签（必须有效）
	 */
	virtual ObstructionSquare GetObstruction(tag_t tag) const = 0;

	/**
	 * 获取代表单位形状的障碍物正方形
	 */
	virtual ObstructionSquare GetUnitShapeObstruction(entity_pos_t x, entity_pos_t z, entity_pos_t clearance) const = 0;

	/**
	 * 获取代表静态形状的障碍物正方形
	 */
	virtual ObstructionSquare GetStaticShapeObstruction(entity_pos_t x, entity_pos_t z, entity_angle_t a, entity_pos_t w, entity_pos_t h) const = 0;

	/**
	 * 设置通行性是否被限制在一个圆形地图内。
	 */
	virtual void SetPassabilityCircular(bool enabled) = 0;

	/**
	 * 获取通行性是否被限制在一个圆形地图内。
	 */
	virtual bool GetPassabilityCircular() const = 0;

	/**
	 * 切换调试信息的渲染。
	 */
	virtual void SetDebugOverlay(bool enabled) = 0;

	// 声明组件的接口类型
	DECLARE_INTERFACE_TYPE(ObstructionManager)
};

/**
 * ICmpObstructionManager @c Test 函数的接口，用于过滤掉不需要的形状。
 */
class IObstructionTestFilter
{
public:
	// 类型别名，方便使用
	typedef ICmpObstructionManager::tag_t tag_t;
	typedef ICmpObstructionManager::flags_t flags_t;

	// 虚析构函数
	virtual ~IObstructionTestFilter() {}

	/**
	 * 如果具有指定参数的形状应该被测试碰撞，则返回true。
	 * 对于所有会碰撞的形状，以及一些不会碰撞的形状，都会调用此函数。
	 *
	 * @param tag 被测试形状的标签
	 * @param flags 形状的一组EFlags
	 * @param group 形状的控制组（通常是形状的单位，或单位的阵型控制器，或0）
	 * @param group2 形状的可选次要控制组，如果未指定则为INVALID_ENTITY。目前
	 * 仅存在于静态形状。
	 */
	virtual bool TestShape(tag_t tag, flags_t flags, entity_id_t group, entity_id_t group2) const = 0;
};

/**
 * 障碍物测试过滤器，将测试所有形状。
 */
class NullObstructionFilter : public IObstructionTestFilter
{
public:
	// 重写 TestShape 方法，总是返回 true
	virtual bool TestShape(tag_t, flags_t, entity_id_t UNUSED(group), entity_id_t UNUSED(group2)) const
	{
		return true;
	}
};

/**
 * 障碍物测试过滤器，将仅测试静止（即非移动）的形状。
 */
class StationaryOnlyObstructionFilter : public IObstructionTestFilter
{
public:
	// 重写 TestShape 方法，只对非移动形状返回 true
	virtual bool TestShape(tag_t, flags_t flags, entity_id_t UNUSED(group), entity_id_t UNUSED(group2)) const
	{
		return !(flags & ICmpObstructionManager::FLAG_MOVING);
	}
};

/**
 * 障碍物测试过滤器，将拒绝给定控制组中的形状，
 * 并拒绝不阻挡单位移动的形状，以及可选择性地拒绝移动中的形状。
 */
class ControlGroupMovementObstructionFilter : public IObstructionTestFilter
{
	bool m_AvoidMoving; // 是否避开移动中的单位
	entity_id_t m_Group; // 要排除的控制组

public:
	// 构造函数
	ControlGroupMovementObstructionFilter(bool avoidMoving, entity_id_t group) :
		m_AvoidMoving(avoidMoving), m_Group(group)
	{}

	// 重写 TestShape 方法
	virtual bool TestShape(tag_t, flags_t flags, entity_id_t group, entity_id_t group2) const
	{
		// 如果形状属于要排除的控制组，则不测试
		if (group == m_Group || (group2 != INVALID_ENTITY && group2 == m_Group))
			return false;

		// 如果形状不阻挡移动，则不测试
		if (!(flags & ICmpObstructionManager::FLAG_BLOCK_MOVEMENT))
			return false;

		// 如果形状正在移动且我们不避开移动单位，则不测试
		if ((flags & ICmpObstructionManager::FLAG_MOVING) && !m_AvoidMoving)
			return false;

		return true;
	}
};

/**
 * 障碍物测试过滤器，将仅测试满足以下条件的形状：
 * - 不属于任一指定的控制组
 * - 并且，根据 'exclude' 参数的值：
 * - 设置了至少一个指定的标志位。
 * - 或者没有设置任何指定的标志位。
 *
 * 第一个（主要）要拒绝形状的控制组必须指定且有效。第二个
 * 要拒绝实体的控制组可以设置为INVALID_ENTITY以不使用它。
 *
 * 此过滤器很有用，例如，允许同一控制组内的地基
 * 可以任意紧密地放置和建造（例如，需要紧密连接的墙体部件）。
 */
class SkipControlGroupsRequireFlagObstructionFilter : public IObstructionTestFilter
{
	bool m_Exclude; // 标志位，决定是包含还是排除掩码匹配的形状
	entity_id_t m_Group;  // 要跳过的主要控制组
	entity_id_t m_Group2; // 要跳过的次要控制组
	flags_t m_Mask; // 标志位掩码

public:
	// 构造函数
	SkipControlGroupsRequireFlagObstructionFilter(bool exclude, entity_id_t group1, entity_id_t group2, flags_t mask) :
		m_Exclude(exclude), m_Group(group1), m_Group2(group2), m_Mask(mask)
	{
		Init();
	}

	// 构造函数重载
	SkipControlGroupsRequireFlagObstructionFilter(entity_id_t group1, entity_id_t group2, flags_t mask) :
		m_Exclude(false), m_Group(group1), m_Group2(group2), m_Mask(mask)
	{
		Init();
	}

	// 重写 TestShape 方法
	virtual bool TestShape(tag_t, flags_t flags, entity_id_t group, entity_id_t group2) const
	{
		// 不测试共享我们任一控制组的形状。
		if (group == m_Group || group == m_Group2 || (group2 != INVALID_ENTITY &&
			(group2 == m_Group || group2 == m_Group2)))
			return false;

		// 如果 m_Exclude 为 true，则不测试任何具有
		// m_Mask 中指定的障碍物标志的形状。
		if (m_Exclude)
			return (flags & m_Mask) == 0;

		// 否则，仅包括至少匹配 m_Mask 中一个标志的形状。
		return (flags & m_Mask) != 0;
	}
private:
	// 初始化函数
	void Init()
	{
		// 要筛选掉的主要控制组必须有效
		ENSURE(m_Group != INVALID_ENTITY);

		// 为简单起见，如果 m_Group2 是 INVALID_ENTITY（即未使用），则将其设置为等于 m_Group
		// 这样我们在 TestShape() 中需要考虑的特殊情况就更少了。
		if (m_Group2 == INVALID_ENTITY)
			m_Group2 = m_Group;
	}
};

/**
 * 障碍物测试过滤器，将仅测试满足以下条件的形状：
 * - 属于两个指定的控制组
 * - 并且设置了至少一个指定的标志位。
 *
 * 第一个（主要）要包含形状的控制组必须指定且有效。
 *
 * 此过滤器对于防止具有相同控制组的实体
 * 碰撞很有用（例如，在现有墙体上建造新的墙段）
 *
 * @todo 此过滤器需要测试用例。
 */
class SkipTagRequireControlGroupsAndFlagObstructionFilter : public IObstructionTestFilter
{
	tag_t m_Tag; // 要跳过的标签
	entity_id_t m_Group; // 必须匹配的第一个控制组
	entity_id_t m_Group2; // 必须匹配的第二个控制组
	flags_t m_Mask; // 必须匹配的标志位掩码

public:
	// 构造函数
	SkipTagRequireControlGroupsAndFlagObstructionFilter(tag_t tag, entity_id_t group1, entity_id_t group2, flags_t mask) :
		m_Tag(tag), m_Group(group1), m_Group2(group2), m_Mask(mask)
	{
		ENSURE(m_Group != INVALID_ENTITY);
	}

	// 重写 TestShape 方法
	virtual bool TestShape(tag_t tag, flags_t flags, entity_id_t group, entity_id_t group2) const
	{
		// 要被包含在测试中，形状必须不具有指定的标签，并且必须
		// 匹配 m_Mask 中的至少一个标志，以及两个控制组。
		return (tag.n != m_Tag.n && (flags & m_Mask) != 0 && ((group == m_Group
			&& group2 == m_Group2) || (group2 == m_Group && group == m_Group2)));
	}
};

/**
 * 障碍物测试过滤器，将仅测试不具有指定标签集的形状。
 */
class SkipTagObstructionFilter : public IObstructionTestFilter
{
	tag_t m_Tag; // 要跳过的标签
public:
	// 构造函数
	SkipTagObstructionFilter(tag_t tag) : m_Tag(tag)
	{
	}

	// 重写 TestShape 方法
	virtual bool TestShape(tag_t tag, flags_t, entity_id_t UNUSED(group), entity_id_t UNUSED(group2)) const
	{
		return tag.n != m_Tag.n;
	}
};

/**
 * 类似于 ControlGroupMovementObstructionFilter，但还忽略一个特定的标签。参见 D3482 了解其存在原因。
 */
class SkipTagAndControlGroupObstructionFilter : public IObstructionTestFilter
{
	entity_id_t m_Group; // 要跳过的控制组
	tag_t m_Tag; // 要跳过的标签
	bool m_AvoidMoving; // 是否避开移动单位

public:
	// 构造函数
	SkipTagAndControlGroupObstructionFilter(tag_t tag, bool avoidMoving, entity_id_t group) :
		m_Tag(tag), m_Group(group), m_AvoidMoving(avoidMoving)
	{}

	// 重写 TestShape 方法
	virtual bool TestShape(tag_t tag, flags_t flags, entity_id_t group, entity_id_t group2) const
	{
		// 如果标签匹配，则不测试
		if (tag.n == m_Tag.n)
			return false;

		// 如果控制组匹配，则不测试
		if (group == m_Group || (group2 != INVALID_ENTITY && group2 == m_Group))
			return false;

		// 如果形状在移动且我们不避开移动单位，则不测试
		if ((flags & ICmpObstructionManager::FLAG_MOVING) && !m_AvoidMoving)
			return false;

		// 如果形状不阻挡移动，则不测试
		if (!(flags & ICmpObstructionManager::FLAG_BLOCK_MOVEMENT))
			return false;

		return true;
	}
};


/**
 * 障碍物测试过滤器，将仅测试满足以下条件的形状：
 * - 不具有指定的标签
 * - 并且设置了至少一个指定的标志位。
 */
class SkipTagRequireFlagsObstructionFilter : public IObstructionTestFilter
{
	tag_t m_Tag; // 要跳过的标签
	flags_t m_Mask; // 必须匹配的标志位掩码
public:
	// 构造函数
	SkipTagRequireFlagsObstructionFilter(tag_t tag, flags_t mask) : m_Tag(tag), m_Mask(mask)
	{
	}

	// 重写 TestShape 方法
	virtual bool TestShape(tag_t tag, flags_t flags, entity_id_t UNUSED(group), entity_id_t UNUSED(group2)) const
	{
		return (tag.n != m_Tag.n && (flags & m_Mask) != 0);
	}
};

#endif // INCLUDED_ICMPOBSTRUCTIONMANAGER
