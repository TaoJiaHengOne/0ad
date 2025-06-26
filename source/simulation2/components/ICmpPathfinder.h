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

#ifndef INCLUDED_ICMPPATHFINDER
#define INCLUDED_ICMPPATHFINDER

#include "simulation2/system/Interface.h"

#include "simulation2/components/ICmpObstruction.h"
#include "simulation2/helpers/Pathfinding.h"

#include <map>

 // 前向声明，避免不必要的头文件包含
class IObstructionTestFilter;
class PathGoal;
template<typename T> class Grid;

// 由异步工作线程返回，用于在主线程中发送消息。
struct WaypointPath;

/**
 * 路径计算结果的结构体
 */
struct PathResult
{
	// 默认构造函数
	PathResult() = default;
	// 构造函数，初始化所有成员
	PathResult(u32 t, entity_id_t n, WaypointPath p) : ticket(t), notify(n), path(p) {};

	u32 ticket;          // 路径请求的唯一标识票据
	entity_id_t notify;  // 接收路径结果通知的实体ID
	WaypointPath path;   // 计算出的路径点
};

/**
 * 寻路器算法。
 *
 * 有两种不同的模式：一种是基于地块（tile）的寻路器，适用于长距离寻路，
 * 会考虑地形成本但忽略单位；另一种是基于顶点（vertex）的“短程”寻路器，
 * 提供精确的路径并能避开其他单位。
 *
 * 两者都使用相同的 PathGoal（路径目标）概念：可以是一个点、一个圆形或一个正方形。
 * （如果起始点在目标形状内部，则路径将向外移动以到达形状的轮廓。）
 *
 * 输出的是一个路点（waypoint）列表。
 */
class ICmpPathfinder : public IComponent
{
public:

	/**
	 * 获取所有已知的通行性类别列表。
	 */
	virtual void GetPassabilityClasses(std::map<std::string, pass_class_t>& passClasses) const = 0;

	/**
	* 获取通行性类别列表，将寻路类别和其他类别分开。
	*/
	virtual void GetPassabilityClasses(
		std::map<std::string, pass_class_t>& nonPathfindingPassClasses,
		std::map<std::string, pass_class_t>& pathfindingPassClasses) const = 0;

	/**
	 * 根据给定的通行性类别名称获取其标签（tag）。
	 * 如果名称无法识别，则记录一个错误并返回一个可接受的值。
	 */
	virtual pass_class_t GetPassabilityClass(const std::string& name) const = 0;

	/**
	 * 获取指定通行性类别的单位间隙（半径）。
	 */
	virtual entity_pos_t GetClearance(pass_class_t passClass) const = 0;

	/**
	 * 获取所有通行性类别中最大的单位间隙。
	 */
	virtual entity_pos_t GetMaximumClearance() const = 0;

	/**
	 * 获取通行性格子图。
	 */
	virtual const Grid<NavcellData>& GetPassabilityGrid() = 0;

	/**
	 * 获取自AI上次访问并刷新以来累积的“脏”信息。
	 */
	virtual const GridUpdateInformation& GetAIPathfinderDirtinessInformation() const = 0;
	/**
	 * 清空AI寻路器的“脏”信息。
	 */
	virtual void FlushAIPathfinderDirtinessInformation() = 0;

	/**
	 * 获取一个表示到地形地块海岸线距离的格子图。
	 */
	virtual Grid<u16> ComputeShoreGrid(bool expandOnWater = false) = 0;

	/**
	 * ComputePath 的异步版本。
	 * 异步请求一个长程路径计算。
	 * 结果将作为 CMessagePathResult 消息发送给 'notify' 实体。
	 * 返回一个唯一的非零数字，它将与结果中的 'ticket' 匹配，
	 * 以便调用者可以识别他们发出的每个单独请求。
	 */
	virtual u32 ComputePathAsync(entity_pos_t x0, entity_pos_t z0, const PathGoal& goal, pass_class_t passClass, entity_id_t notify) = 0;

	/*
	 * 立即请求一个长程路径计算（同步）。
	 */
	virtual void ComputePathImmediate(entity_pos_t x0, entity_pos_t z0, const PathGoal& goal, pass_class_t passClass, WaypointPath& ret) const = 0;

	/**
	 * 异步请求一个短程路径计算。
	 * 结果将作为 CMessagePathResult 消息发送给 'notify' 实体。
	 * 返回一个唯一的非零数字，它将与结果中的 'ticket' 匹配，
	 * 以便调用者可以识别他们发出的每个单独请求。
	 */
	virtual u32 ComputeShortPathAsync(entity_pos_t x0, entity_pos_t z0, entity_pos_t clearance, entity_pos_t range, const PathGoal& goal, pass_class_t passClass, bool avoidMovingUnits, entity_id_t controller, entity_id_t notify) = 0;

	/*
	 * 立即请求一个短程路径计算（同步）。
	 */
	virtual WaypointPath ComputeShortPathImmediate(const ShortPathRequest& request) const = 0;

	/**
	 * 如果调试覆盖层已启用，则渲染将由 ComputePath 计算的路径。
	 */
	virtual void SetDebugPath(entity_pos_t x0, entity_pos_t z0, const PathGoal& goal, pass_class_t passClass) = 0;

	/**
	 * 将单位围绕一个点进行分布，返回一个位置列表。
	 */
	virtual std::vector<CFixedVector2D> DistributeAround(std::vector<entity_id_t> units, entity_pos_t x, entity_pos_t z) const = 0;

	/**
	 * @return 如果对于给定的 passClass，目标可以从 (x0, z0) 到达，则返回 true，否则返回 false。
	 * 警告：这是同步的，开销较大，不应过于频繁地调用。
	 */
	virtual bool IsGoalReachable(entity_pos_t x0, entity_pos_t z0, const PathGoal& goal, pass_class_t passClass) = 0;

	/**
	 * 检查给定的移动路线是否有效，并且不会碰到任何障碍物
	 * 或不可通行的地形。
	 * 如果移动可行，则返回 true。
	 */
	virtual bool CheckMovement(const IObstructionTestFilter& filter, entity_pos_t x0, entity_pos_t z0, entity_pos_t x1, entity_pos_t z1, entity_pos_t r, pass_class_t passClass) const = 0;

	/**
	 * 检查一个单位放置在此处是否有效，并且不会碰到任何障碍物
	 * 或不可通行的地形。
	 * 当 onlyCenterPoint = true 时，仅检查单位的中心地块。
	 * @return 如果放置可行，则返回 ICmpObstruction::FOUNDATION_CHECK_SUCCESS，否则
	 * 返回一个描述失败类型的值。
	 */
	virtual ICmpObstruction::EFoundationCheck CheckUnitPlacement(const IObstructionTestFilter& filter, entity_pos_t x, entity_pos_t z, entity_pos_t r, pass_class_t passClass, bool onlyCenterPoint = false) const = 0;

	/**
	 * 检查一个建筑物放置在此处是否有效，并且不会碰到任何障碍物
	 * 或不可通行的地形。
	 * @return 如果放置可行，则返回 ICmpObstruction::FOUNDATION_CHECK_SUCCESS，否则
	 * 返回一个描述失败类型的值。
	 */
	virtual ICmpObstruction::EFoundationCheck CheckBuildingPlacement(const IObstructionTestFilter& filter, entity_pos_t x, entity_pos_t z, entity_pos_t a, entity_pos_t w, entity_pos_t h, entity_id_t id, pass_class_t passClass) const = 0;

	/**
	 * 检查一个建筑物放置在此处是否有效，并且不会碰到任何障碍物
	 * 或不可通行的地形。
	 * 当 onlyCenterPoint = true 时，仅检查建筑物的中心地块。
	 * @return 如果放置可行，则返回 ICmpObstruction::FOUNDATION_CHECK_SUCCESS，否则
	 * 返回一个描述失败类型的值。
	 */
	virtual ICmpObstruction::EFoundationCheck CheckBuildingPlacement(const IObstructionTestFilter& filter, entity_pos_t x, entity_pos_t z, entity_pos_t a, entity_pos_t w, entity_pos_t h, entity_id_t id, pass_class_t passClass, bool onlyCenterPoint) const = 0;


	/**
	 * 切换调试信息的存储和渲染。
	 */
	virtual void SetDebugOverlay(bool enabled) = 0;

	/**
	 * 切换分层寻路器调试信息的存储和渲染。
	 */
	virtual void SetHierDebugOverlay(bool enabled) = 0;

	/**
	 * 完成异步路径请求的计算，并发送 CMessagePathResult 消息。
	 */
	virtual void SendRequestedPaths() = 0;

	/**
	 * 告知异步寻路器线程它们可以开始计算路径。
	 */
	virtual void StartProcessingMoves(bool useMax) = 0;

	/**
	 * 如有必要，根据当前的障碍物列表重新生成格子图。
	 */
	virtual void UpdateGrid() = 0;

	/**
	 * 返回有关上一次 ComputePath 的一些统计信息。
	 */
	virtual void GetDebugData(u32& steps, double& time, Grid<u8>& grid) const = 0;

	/**
	 * 在 Atlas 编辑器中设置寻路器通行性覆盖层。
	 */
	virtual void SetAtlasOverlay(bool enable, pass_class_t passClass = 0) = 0;

	// 声明组件的接口类型
	DECLARE_INTERFACE_TYPE(Pathfinder)
};

#endif // INCLUDED_ICMPPATHFINDER
