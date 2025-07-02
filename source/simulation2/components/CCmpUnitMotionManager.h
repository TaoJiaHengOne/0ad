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
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with 0 A.D.  If not, see <http://www.gnu.org/licenses/>.
 */

 // 防止头文件被重复包含的宏保护
#ifndef INCLUDED_CCMPUNITMOTIONMANAGER
#define INCLUDED_CCMPUNITMOTIONMANAGER

#include "simulation2/system/Component.h"
#include "ICmpUnitMotionManager.h"

#include "simulation2/MessageTypes.h"
#include "simulation2/components/ICmpTerrain.h"
#include "simulation2/helpers/Grid.h"
#include "simulation2/system/EntityMap.h"

class CCmpUnitMotion;

/**
 * @class CCmpUnitMotionManager
 * @brief 一个系统组件，用于管理所有单位的移动、碰撞和相互推挤。
 * 它统一处理大量单位的移动逻辑，以优化性能并实现群体行进为。
 */
class CCmpUnitMotionManager final : public ICmpUnitMotionManager
{
public:
	// 静态初始化函数，在组件管理器中注册该类
	static void ClassInit(CComponentManager& componentManager);

	// 为该组件提供默认的内存分配器
	DEFAULT_COMPONENT_ALLOCATOR(UnitMotionManager)

		/**
		 * 推挤压力的最大值。
		 */
		static constexpr int MAX_PRESSURE = 255;

	/**
	 * @struct MotionState
	 * @brief 每个单位的持久化状态。
	 */
	struct MotionState
	{
		MotionState(ICmpPosition* cmpPos, CCmpUnitMotion* cmpMotion);

		// 组件引用 - 在移动期间这些引用必须保持有效。
		// 注意：这通常是一种非常危险的做法，
		// 但由于与 CCmpUnitMotion 的紧密耦合，这种做法是可行的。
		// 注意：这假设组件在内存中的地址*不会*改变，
		// 这在当前是一个合理的假设，但未来可能会改变。
		ICmpPosition* cmpPosition;
		CCmpUnitMotion* cmpUnitMotion;

		// 单位开始移动前的位置
		CFixedVector2D initialPos;
		// 移动过程中的瞬时位置。
		CFixedVector2D pos;

		// 从附近单位累积的“推力”。
		CFixedVector2D push;

		// 单位的当前速度
		fixed speed;

		// 初始朝向角度
		fixed initialAngle;
		// 当前朝向角度
		fixed angle;

		// 用于阵型 - 具有相同控制组的单位不会在远距离互相推挤。
		// (这是必需的，因为阵型可能很紧凑，大型单位可能会因此永远无法稳定下来。)
		entity_id_t controlGroup = INVALID_ENTITY;

		// 这是一个临时的计数器，用于存储一个实体承受的“推挤压力”有多大。
		// 越大的压力会使单位减速，并使其更难被推动，
		// 这能有效地使碰撞的单位群陷入困境。
		uint8_t pushingPressure = 0;

		// 元标记 -> 此实体既不会推挤也不会被推挤。
		// (用于那些禁用了障碍物的实体)。
		bool ignore = false;

		// 如果为 true，则该实体在移动过程中需要被处理。
		bool needUpdate = false;

		// 标记单位本回合是否直线移动
		bool wentStraight = false;
		// 标记单位本回合是否曾被阻挡
		bool wasObstructed = false;

		// 出于效率考虑，克隆自障碍物管理器的标志
		bool isMoving = false;
	};

	// “模板”状态，不会被序列化（不能在游戏中途更改）。

	// 单位间相互推挤的最大距离是单位间隙之和乘以该因子，
	// 该因子本身已预先乘以了圆形-方形校正因子。
	entity_pos_t m_PushingRadiusMultiplier;
	// 分别对移动单位和静止单位的最大推挤距离的加法修正值。
	entity_pos_t m_MovingPushExtension;
	entity_pos_t m_StaticPushExtension;
	// 推挤“扩散”的乘数。
	// 这应被理解为在最大距离的百分之多少内，推挤将是“全力”的。
	entity_pos_t m_MovingPushingSpread;
	entity_pos_t m_StaticPushingSpread;

	// 低于此值的推力将被忽略 - 这可以防止单位因非常小的增量而永远移动。
	entity_pos_t m_MinimalPushing;

	// 推挤压力强度的乘数。
	entity_pos_t m_PushingPressureStrength;
	// 每回合推挤压力的衰减值。
	entity_pos_t m_PushingPressureDecay;

	// 这些向量在反序列化时重建。

	// 存储所有受管理的普通单位的运动状态
	EntityMap<MotionState> m_Units;
	// 存储所有阵型控制器的运动状态
	EntityMap<MotionState> m_FormationControllers;

	// 以下是回合内的局部状态，不被序列化。

	// 用于空间划分的网格，加速查找移动单位的邻居
	Grid<std::vector<EntityMap<MotionState>::iterator>> m_MovingUnits;
	// 标记当前是否正在计算单位移动
	bool m_ComputingMotion;

	// 获取此组件的XML结构定义
	static std::string GetSchema()
	{
		return "<a:component type='system'/><empty/>";
	}

	// 初始化组件
	void Init(const CParamNode&) override;

	// 卸载组件时的清理工作
	void Deinit() override
	{
	}

	// 序列化组件状态
	void Serialize(ISerializer& serialize) override;
	// 反序列化组件状态
	void Deserialize(const CParamNode& paramNode, IDeserializer& deserialize) override;

	// 处理消息
	void HandleMessage(const CMessage& msg, bool global) override;

	// 注册一个单位移动组件到管理器
	void Register(CCmpUnitMotion* component, entity_id_t ent, bool formationController) override;
	// 从管理器中注销一个单位
	void Unregister(entity_id_t ent) override;

	// 返回当前是否正在计算移动
	bool ComputingMotion() const override
	{
		return m_ComputingMotion;
	}

	// 返回推挤效果是否已激活
	bool IsPushingActivated() const override
	{
		return m_PushingRadiusMultiplier != entity_pos_t::Zero();
	}

private:
	// 在反序列化后调用的处理函数
	void OnDeserialized();
	// 重置空间划分网格
	void ResetSubdivisions();
	// 在每回合开始时调用
	void OnTurnStart();

	// 移动所有普通单位
	void MoveUnits(fixed dt);
	// 移动所有阵型
	void MoveFormations(fixed dt);
	// 移动给定实体集合的通用函数
	void Move(EntityMap<MotionState>& ents, fixed dt);

	// 计算并应用两个单位 a 和 b 之间的推力
	void Push(EntityMap<MotionState>::value_type& a, EntityMap<MotionState>::value_type& b, fixed dt);
};

// 将该组件类型注册到引擎的组件系统中
REGISTER_COMPONENT_TYPE(UnitMotionManager)

#endif // INCLUDED_CCMPUNITMOTIONMANAGER
