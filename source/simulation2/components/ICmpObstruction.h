/* Copyright (C) 2021 Wildfire Games.
 * This file is part of 0 A.D.
 *
 * 0 A.D. is free-software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 2 of the License, or
 * (at your option) any later version.
 *
 * 0 A.D. is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with 0 A.D.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef INCLUDED_ICMPOBSTRUCTION
#define INCLUDED_ICMPOBSTRUCTION

 // 包含组件系统接口和障碍物管理器接口的定义
#include "simulation2/system/Interface.h"
#include "simulation2/components/ICmpObstructionManager.h"

/**
 * 将实体标记为会阻挡其他单位移动的障碍物，
 * 并处理碰撞查询。
 * 这是一个接口类，定义了障碍物组件必须实现的功能。
 */
class ICmpObstruction : public IComponent
{
public:

	// 定义了地基放置检查的可能结果
	enum EFoundationCheck {
		FOUNDATION_CHECK_SUCCESS,             // 成功，可以放置
		FOUNDATION_CHECK_FAIL_ERROR,          // 失败，发生内部错误
		FOUNDATION_CHECK_FAIL_NO_OBSTRUCTION, // 失败，实体没有障碍物组件
		FOUNDATION_CHECK_FAIL_OBSTRUCTS_FOUNDATION, // 失败，被其他地基阻挡
		FOUNDATION_CHECK_FAIL_TERRAIN_CLASS   // 失败，地形通行性类别不符
	};

	// 定义了障碍物的基本类型
	enum EObstructionType {
		STATIC,   // 静态障碍物，如建筑
		UNIT,     // 单位障碍物
		CLUSTER   // 复合障碍物，由多个子形状构成
	};

	// 获取此障碍物在障碍物管理器中的唯一标识符(tag)。
	virtual ICmpObstructionManager::tag_t GetObstruction() const = 0;

	/**
	 * 获取与此障碍物形状对应的方块。
	 * @return 成功时返回true并更新@p out；
	 * 失败时返回false (例如，对象不在世界中)。
	 */
	virtual bool GetObstructionSquare(ICmpObstructionManager::ObstructionSquare& out) const = 0;

	/**
	 * 与上面的方法相同，但返回上一回合的障碍物形状。
	 */
	virtual bool GetPreviousObstructionSquare(ICmpObstructionManager::ObstructionSquare& out) const = 0;

	/**
	 * @return 障碍物的尺寸（对于单位是间隙大小，对于建筑是外接圆半径）。
	 */
	virtual entity_pos_t GetSize() const = 0;

	/**
	 * @return 静态障碍物的尺寸(宽, 深)，对于单位形状则返回(0,0)。
	 */
	virtual CFixedVector2D GetStaticSize() const = 0;

	// 获取障碍物的类型 (STATIC, UNIT, 或 CLUSTER)。
	virtual EObstructionType GetObstructionType() const = 0;

	// 为单位类型的障碍物设置其间隙大小。
	virtual void SetUnitClearance(const entity_pos_t& clearance) = 0;

	// 检查此障碍物的控制组是否是“持久的”，即在碰撞时会传递给其他实体。
	virtual bool IsControlPersistent() const = 0;

	/**
	 * 测试障碍物方块的前部是否在水中，后部是否在岸上（用于码头等建筑）。
	 */
	virtual bool CheckShorePlacement() const = 0;

	/**
	 * 测试此实体是否与任何设置为阻挡地基创建的障碍物发生碰撞。
	 * @param ignoredEntities 测试期间要忽略的实体列表。
	 * @return 如果检查通过，返回 FOUNDATION_CHECK_SUCCESS，否则返回描述失败类型的 EFoundationCheck 值。
	 */
	virtual EFoundationCheck CheckFoundation(const std::string& className) const = 0;
	virtual EFoundationCheck CheckFoundation(const std::string& className, bool onlyCenterPoint) const = 0;

	/**
	 * 为脚本调用封装的 CheckFoundation 函数，返回友好的字符串而不是 EFoundationCheck 枚举。
	 * @return 如果检查通过，返回 "success"，否则返回描述失败类型的字符串。
	 */
	virtual std::string CheckFoundation_wrapper(const std::string& className, bool onlyCenterPoint) const;

	/**
	 * 测试此实体是否与任何共享其控制组并阻挡地基创建的障碍物发生碰撞（用于检测重复地基）。
	 * @return 如果地基有效（未被阻挡），则返回 true。
	 */
	virtual bool CheckDuplicateFoundation() const = 0;

	/**
	 * 返回一个实体列表，这些实体拥有与给定标志匹配且与当前障碍物相交的障碍物。
	 * @return 阻挡实体的向量。
	 */
	virtual std::vector<entity_id_t> GetEntitiesByFlags(ICmpObstructionManager::flags_t flags) const = 0;

	/**
	 * 返回正在阻挡移动的实体列表。
	 * @return 阻挡实体的向量。
	 */
	virtual std::vector<entity_id_t> GetEntitiesBlockingMovement() const = 0;

	/**
	 * 返回正在阻挡地基建造的实体列表。
	 * @return 阻挡实体的向量。
	 */
	virtual std::vector<entity_id_t> GetEntitiesBlockingConstruction() const = 0;

	/**
	 * 返回在此障碍物上开始建造时应被删除的实体列表，
	 * 例如羊的尸体。
	 */
	virtual std::vector<entity_id_t> GetEntitiesDeletedUponConstruction() const = 0;

	/**
	 * 检测阻挡地基的实体之间的碰撞，并
	 * 在适当情况下尝试通过设置控制组来修复它们。
	 */
	virtual void ResolveFoundationCollisions() const = 0;

	// 设置障碍物是否激活（是否参与碰撞检测）。
	virtual void SetActive(bool active) = 0;

	// 为单位障碍物设置移动标志（表示单位当前是否在移动）。
	virtual void SetMovingFlag(bool enabled) = 0;

	// 动态地禁用或启用移动阻挡和寻路阻挡标志。
	virtual void SetDisableBlockMovementPathfinding(bool movementDisabled, bool pathfindingDisabled, int32_t shape) = 0;

	/**
	 * @param templateOnly - 是返回原始模板值还是当前值。
	 * @return 获取移动阻挡标志的状态。
	 */
	virtual bool GetBlockMovementFlag(bool templateOnly) const = 0;

	/**
	 * 更改实体所属的控制组。
	 * 控制组用于让单位忽略来自同一组的其他单位的碰撞。
	 * 默认值是实体自身的ID。
	 */
	virtual void SetControlGroup(entity_id_t group) = 0;

	/// 参见 SetControlGroup。获取主控制组。
	virtual entity_id_t GetControlGroup() const = 0;

	// 设置次要控制组。
	virtual void SetControlGroup2(entity_id_t group2) = 0;
	// 获取次要控制组。
	virtual entity_id_t GetControlGroup2() const = 0;

	// 声明接口类型，用于0 A.D.的组件系统。
	DECLARE_INTERFACE_TYPE(Obstruction)
};

#endif // INCLUDED_ICMPOBSTRUCTION
