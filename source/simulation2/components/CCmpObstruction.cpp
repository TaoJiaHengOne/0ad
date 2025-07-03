/* Copyright (C) 2022 Wildfire Games.
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

 // 预编译头文件
#include "precompiled.h"

// 包含组件系统、障碍物接口等核心定义
#include "simulation2/system/Component.h"
#include "ICmpObstruction.h"

// 包含消息类型、相关组件接口和序列化类型
#include "simulation2/MessageTypes.h"
#include "simulation2/components/ICmpObstructionManager.h"
#include "simulation2/components/ICmpTerrain.h"
#include "simulation2/components/ICmpUnitMotion.h"
#include "simulation2/components/ICmpWaterManager.h"
#include "simulation2/serialization/SerializedTypes.h"

// 包含日志记录工具
#include "ps/CLogger.h"

// 序列化帮助器的模板特化，用于处理 ICmpObstructionManager::tag_t 类型
template<>
struct SerializeHelper<ICmpObstructionManager::tag_t>
{
	template<typename S>
	void operator()(S& serialize, const char* UNUSED(name), Serialize::qualify<S, ICmpObstructionManager::tag_t> value)
	{
		// 将tag作为一个无符号32位整数进行序列化
		serialize.NumberU32_Unbounded("tag", value.n);
	}
};

/**
 * 障碍物（Obstruction）组件的实现。
 * 当实体移动或死亡时，此组件会根据 ICmpFootprint 派生出的形状，
 * 保持 ICmpPathfinder（寻路器）的世界模型更新。
 */
class CCmpObstruction final : public ICmpObstruction
{
public:
	// 静态类初始化函数，在组件管理器中注册消息订阅
	static void ClassInit(CComponentManager& componentManager)
	{
		// 订阅“位置改变”消息，以更新障碍物位置
		componentManager.SubscribeToMessageType(MT_PositionChanged);
		// 订阅“销毁”消息，以移除障碍物
		componentManager.SubscribeToMessageType(MT_Destroy);
	}

	// 使用默认的组件分配器宏
	DEFAULT_COMPONENT_ALLOCATOR(Obstruction)

		// 类型别名，方便使用
		typedef ICmpObstructionManager::tag_t tag_t;
	typedef ICmpObstructionManager::flags_t flags_t;

	// --- 模板状态（Template state）: 从实体模板中读取的初始配置 ---

	EObstructionType m_Type; // 障碍物类型 (UNIT, STATIC, CLUSTER)

	entity_pos_t m_Size0; // 尺寸0 (半径或宽度)
	entity_pos_t m_Size1; // 尺寸1 (半径或深度)
	flags_t m_TemplateFlags; // 从模板中读取的标志位
	entity_pos_t m_Clearance; // 单位的间隙大小（用于单位类型的障碍物）

	// 用于复合障碍物（CLUSTER）的子形状定义
	typedef struct {
		entity_pos_t dx, dz;      // 相对于中心的偏移
		entity_angle_t da;        // 相对于中心的角度偏移
		entity_pos_t size0, size1; // 宽度和深度
		flags_t flags;            // 该子形状的标志位
	} Shape;

	std::vector<Shape> m_Shapes; // 存储所有子形状的向量

	// --- 动态状态（Dynamic state）: 在游戏运行时会改变的状态 ---

	/// 障碍物是否处于激活状态（参与碰撞检测），或者只是一个非活动的占位符。
	bool m_Active;
	/// 与此障碍物关联的实体当前是否在移动。仅适用于UNIT类型的障碍物。
	bool m_Moving;
	/// 障碍物的控制组是否应保持一致，并用于设置与之碰撞的实体的控制组。
	bool m_ControlPersist;

	// 权宜之计(WORKAROUND): 在处理销毁消息时，障碍物组件可能收到使其重新激活障碍物的消息，
	// 从而留下悬空的障碍物。为避免这种情况，如果此标志为true，则绝不重新激活障碍物。
	bool m_IsDestroyed = false;

	/**
	 * 主要控制组标识符。指示此实体形状属于哪个控制组。
	 * 通常与障碍物测试过滤器结合使用，使同一组成员在测试时互相忽略。
	 * 默认值为实体ID。绝不能设置为INVALID_ENTITY。
	 */
	entity_id_t m_ControlGroup;

	/**
	 * 可选的次要控制组标识符。与 m_ControlGroup 类似；如果设置为有效值，
	 * 此字段标识实体形状所属的附加次要控制组。
	 * 设置为 INVALID_ENTITY 表示不分配次要组。默认为 INVALID_ENTITY。
	 *
	 * 仅当一个实体属于单个控制组不足以满足需求时才需要。否则可以忽略。
	 */
	entity_id_t m_ControlGroup2;

	/// 此实体障碍物形状的标识符，由障碍物管理器注册。包含结构信息，但在此处应视为不透明数据。
	tag_t m_Tag;
	/// 复合障碍物的子形状标识符列表。
	std::vector<tag_t> m_ClusterTags;

	/// 影响此实体障碍物形状行为的一组标志。
	flags_t m_Flags;

	// 定义了此组件在实体模板XML文件中的结构
	static std::string GetSchema()
	{
		return
			"<a:example/>"
			"<a:help>使该实体能够阻挡其他单位的移动。</a:help>"
			"<choice>"
			// 静态障碍物，如建筑
			"<element name='Static'>"
			"<attribute name='width'>"
			"<data type='decimal'>"
			"<param name='minInclusive'>1.5</param>"
			"</data>"
			"</attribute>"
			"<attribute name='depth'>"
			"<data type='decimal'>"
			"<param name='minInclusive'>1.5</param>"
			"</data>"
			"</attribute>"
			"</element>"
			// 单位障碍物
			"<element name='Unit'>"
			"<empty/>"
			"</element>"
			// 复合障碍物，由多个部分组成
			"<element name='Obstructions'>"
			"<zeroOrMore>"
			"<element>"
			"<anyName/>"
			"<optional>"
			"<attribute name='x'>"
			"<data type='decimal'/>"
			"</attribute>"
			"</optional>"
			"<optional>"
			"<attribute name='z'>"
			"<data type='decimal'/>"
			"</attribute>"
			"</optional>"
			"<attribute name='width'>"
			"<data type='decimal'>"
			"<param name='minInclusive'>1.5</param>"
			"</data>"
			"</attribute>"
			"<attribute name='depth'>"
			"<data type='decimal'>"
			"<param name='minInclusive'>1.5</param>"
			"</data>"
			"</attribute>"
			"</element>"
			"</zeroOrMore>"
			"</element>"
			"</choice>"
			"<element name='Active' a:help='如果为false，此实体将被其他单位在碰撞测试中忽略，但仍可执行自己的碰撞测试'>"
			"<data type='boolean'/>"
			"</element>"
			"<element name='BlockMovement' a:help='单位是否被允许穿过此实体'>"
			"<data type='boolean'/>"
			"</element>"
			"<element name='BlockPathfinding' a:help='远程寻路器是否应避免通过此实体的路径。只应为大型静止障碍物设置'>"
			"<data type='boolean'/>"
			"</element>"
			"<element name='BlockFoundation' a:help='玩家是否不能在此实体之上放置建筑地基。如果为true，BlockConstruction也应为true'>"
			"<data type='boolean'/>"
			"</element>"
			"<element name='BlockConstruction' a:help='玩家是否不能在放置于此实体之上的建筑开始施工'>"
			"<data type='boolean'/>"
			"</element>"
			"<element name='DeleteUponConstruction' a:help='当在此实体之上开始建造建筑时，是否应删除此实体（例如树木）。'>"
			"<data type='boolean'/>"
			"</element>"
			"<element name='DisableBlockMovement' a:help='如果为true，BlockMovement将被覆盖并视为false。（这是处理地基的特例）'>"
			"<data type='boolean'/>"
			"</element>"
			"<element name='DisableBlockPathfinding' a:help='如果为true，BlockPathfinding将被覆盖并视为false。（这是处理地基的特例）'>"
			"<data type='boolean'/>"
			"</element>"
			"<optional>"
			"<element name='ControlPersist' a:help='如果存在，此实体的控制组将被赋予与之碰撞的实体。'>"
			"<empty/>"
			"</element>"
			"</optional>";
	}

	// 从模板数据初始化组件
	void Init(const CParamNode& paramNode) override
	{
		// 最小障碍物尺寸是寻路网格尺寸 * sqrt(2)
		// 这在schema中被强制要求为最小1.5
		fixed minObstruction = (Pathfinding::NAVCELL_SIZE.Square() * 2).Sqrt();
		m_TemplateFlags = 0;
		// 根据模板设置初始标志
		if (paramNode.GetChild("BlockMovement").ToBool())
			m_TemplateFlags |= ICmpObstructionManager::FLAG_BLOCK_MOVEMENT;
		if (paramNode.GetChild("BlockPathfinding").ToBool())
			m_TemplateFlags |= ICmpObstructionManager::FLAG_BLOCK_PATHFINDING;
		if (paramNode.GetChild("BlockFoundation").ToBool())
			m_TemplateFlags |= ICmpObstructionManager::FLAG_BLOCK_FOUNDATION;
		if (paramNode.GetChild("BlockConstruction").ToBool())
			m_TemplateFlags |= ICmpObstructionManager::FLAG_BLOCK_CONSTRUCTION;
		if (paramNode.GetChild("DeleteUponConstruction").ToBool())
			m_TemplateFlags |= ICmpObstructionManager::FLAG_DELETE_UPON_CONSTRUCTION;

		m_Flags = m_TemplateFlags;
		// 处理地基等特例，覆盖某些标志
		if (paramNode.GetChild("DisableBlockMovement").ToBool())
			m_Flags &= (flags_t)(~ICmpObstructionManager::FLAG_BLOCK_MOVEMENT);
		if (paramNode.GetChild("DisableBlockPathfinding").ToBool())
			m_Flags &= (flags_t)(~ICmpObstructionManager::FLAG_BLOCK_PATHFINDING);

		// 根据类型解析不同的障碍物数据
		if (paramNode.GetChild("Unit").IsOk())
		{
			m_Type = UNIT;

			CmpPtr<ICmpUnitMotion> cmpUnitMotion(GetEntityHandle());
			if (cmpUnitMotion)
				m_Clearance = cmpUnitMotion->GetUnitClearance();
		}
		else if (paramNode.GetChild("Static").IsOk())
		{
			m_Type = STATIC;
			m_Size0 = paramNode.GetChild("Static").GetChild("@width").ToFixed();
			m_Size1 = paramNode.GetChild("Static").GetChild("@depth").ToFixed();
			ENSURE(m_Size0 > minObstruction);
			ENSURE(m_Size1 > minObstruction);
		}
		else // CLUSTER 类型
		{
			m_Type = CLUSTER;
			CFixedVector2D max = CFixedVector2D(fixed::FromInt(0), fixed::FromInt(0));
			CFixedVector2D min = CFixedVector2D(fixed::FromInt(0), fixed::FromInt(0));
			const CParamNode::ChildrenMap& clusterMap = paramNode.GetChild("Obstructions").GetChildren();
			for (CParamNode::ChildrenMap::const_iterator it = clusterMap.begin(); it != clusterMap.end(); ++it)
			{
				Shape b;
				b.size0 = it->second.GetChild("@width").ToFixed();
				b.size1 = it->second.GetChild("@depth").ToFixed();
				ENSURE(b.size0 > minObstruction);
				ENSURE(b.size1 > minObstruction);
				b.dx = it->second.GetChild("@x").ToFixed();
				b.dz = it->second.GetChild("@z").ToFixed();
				b.da = entity_angle_t::FromInt(0);
				b.flags = m_Flags;
				m_Shapes.push_back(b);
				// 计算整个复合体的包围盒
				max.X = std::max(max.X, b.dx + b.size0 / 2);
				max.Y = std::max(max.Y, b.dz + b.size1 / 2);
				min.X = std::min(min.X, b.dx - b.size0 / 2);
				min.Y = std::min(min.Y, b.dz - b.size1 / 2);
			}
			m_Size0 = fixed::FromInt(2).Multiply(std::max(max.X, -min.X));
			m_Size1 = fixed::FromInt(2).Multiply(std::max(max.Y, -min.Y));
		}

		// 初始化动态状态
		m_Active = paramNode.GetChild("Active").ToBool();
		m_ControlPersist = paramNode.GetChild("ControlPersist").IsOk();

		m_Tag = tag_t();
		if (m_Type == CLUSTER)
			m_ClusterTags.clear();
		m_Moving = false;
		m_ControlGroup = GetEntityId(); // 默认控制组是自身实体ID
		m_ControlGroup2 = INVALID_ENTITY;
	}

	// 组件销毁时的清理工作
	void Deinit() override
	{
	}

	// 序列化和反序列化共用的辅助函数
	template<typename S>
	void SerializeCommon(S& serialize)
	{
		serialize.Bool("active", m_Active);
		serialize.Bool("moving", m_Moving);
		serialize.NumberU32_Unbounded("control group", m_ControlGroup);
		serialize.NumberU32_Unbounded("control group 2", m_ControlGroup2);
		serialize.NumberU32_Unbounded("tag", m_Tag.n);
		serialize.NumberU8_Unbounded("flags", m_Flags);
		if (m_Type == CLUSTER)
			Serializer(serialize, "cluster tags", m_ClusterTags);
		if (m_Type == UNIT)
			serialize.NumberFixed_Unbounded("clearance", m_Clearance);
	}

	// 序列化（保存游戏状态）
	void Serialize(ISerializer& serialize) override
	{
		SerializeCommon(serialize);
	}

	// 反序列化（加载游戏状态）
	void Deserialize(const CParamNode& paramNode, IDeserializer& deserialize) override
	{
		Init(paramNode);
		SerializeCommon(deserialize);
	}

	// 处理接收到的消息
	void HandleMessage(const CMessage& msg, bool UNUSED(global)) override
	{
		switch (msg.GetType())
		{
		case MT_PositionChanged: // 当实体位置改变时
		{
			// 如果障碍物不活动或已被标记为销毁，则不处理
			if (!m_Active || m_IsDestroyed)
				break;

			const CMessagePositionChanged& data = static_cast<const CMessagePositionChanged&> (msg);

			// 如果实体不在世界中，且本身就没有有效的障碍物标签，则无需任何改变
			if (!data.inWorld && !m_Tag.valid())
				break;

			CmpPtr<ICmpObstructionManager> cmpObstructionManager(GetSystemEntity());
			if (!cmpObstructionManager)
				break; // 错误：障碍物管理器不存在

			if (data.inWorld && m_Tag.valid())
			{
				// 实体在世界中且已有障碍物，移动它
				cmpObstructionManager->MoveShape(m_Tag, data.x, data.z, data.a);

				// 如果是复合障碍物，还要移动所有子形状
				if (m_Type == CLUSTER)
				{
					for (size_t i = 0; i < m_Shapes.size(); ++i)
					{
						Shape& b = m_Shapes[i];
						fixed s, c;
						sincos_approx(data.a, s, c); // 计算旋转后的偏移
						cmpObstructionManager->MoveShape(m_ClusterTags[i], data.x + b.dx.Multiply(c) + b.dz.Multiply(s), data.z + b.dz.Multiply(c) - b.dx.Multiply(s), data.a + b.da);
					}
				}
			}
			else if (data.inWorld && !m_Tag.valid())
			{
				// 实体进入世界但尚无障碍物，创建新的
				if (m_Type == STATIC)
					m_Tag = cmpObstructionManager->AddStaticShape(GetEntityId(),
						data.x, data.z, data.a, m_Size0, m_Size1, m_Flags, m_ControlGroup, m_ControlGroup2);
				else if (m_Type == UNIT)
					m_Tag = cmpObstructionManager->AddUnitShape(GetEntityId(),
						data.x, data.z, m_Clearance, (flags_t)(m_Flags | (m_Moving ? ICmpObstructionManager::FLAG_MOVING : 0)), m_ControlGroup);
				else // CLUSTER
					AddClusterShapes(data.x, data.z, data.a);
			}
			else if (!data.inWorld && m_Tag.valid())
			{
				// 实体离开世界且有障碍物，移除它
				cmpObstructionManager->RemoveShape(m_Tag);
				m_Tag = tag_t();
				if (m_Type == CLUSTER)
					RemoveClusterShapes();
			}
			break;
		}
		case MT_Destroy: // 当实体被销毁时
		{
			// 标记障碍物为已销毁，以防止在此之后被重新激活
			m_IsDestroyed = true;
			m_Active = false;

			if (m_Tag.valid())
			{
				CmpPtr<ICmpObstructionManager> cmpObstructionManager(GetSystemEntity());
				if (!cmpObstructionManager)
					break; // 错误

				// 从管理器中移除形状
				cmpObstructionManager->RemoveShape(m_Tag);
				m_Tag = tag_t();
				if (m_Type == CLUSTER)
					RemoveClusterShapes();
			}
			break;
		}
		}
	}

	// 设置障碍物是否激活
	void SetActive(bool active) override
	{
		if (active && !m_Active && !m_IsDestroyed)
		{
			// 从非激活 -> 激活
			m_Active = true;

			// 构建障碍物形状
			CmpPtr<ICmpObstructionManager> cmpObstructionManager(GetSystemEntity());
			if (!cmpObstructionManager)
				return; // 错误

			CmpPtr<ICmpPosition> cmpPosition(GetEntityHandle());
			if (!cmpPosition)
				return; // 错误

			if (!cmpPosition->IsInWorld())
				return; // 不在世界中，不需要障碍物

			// TODO: 此处代码与消息处理器中有重复
			CFixedVector2D pos = cmpPosition->GetPosition2D();
			if (m_Type == STATIC)
				m_Tag = cmpObstructionManager->AddStaticShape(GetEntityId(),
					pos.X, pos.Y, cmpPosition->GetRotation().Y, m_Size0, m_Size1, m_Flags, m_ControlGroup, m_ControlGroup2);
			else if (m_Type == UNIT)
				m_Tag = cmpObstructionManager->AddUnitShape(GetEntityId(),
					pos.X, pos.Y, m_Clearance, (flags_t)(m_Flags | (m_Moving ? ICmpObstructionManager::FLAG_MOVING : 0)), m_ControlGroup);
			else
				AddClusterShapes(pos.X, pos.Y, cmpPosition->GetRotation().Y);

			// 由UnitMotion使用，以激活/停用推挤行为
			if (m_TemplateFlags & ICmpObstructionManager::FLAG_BLOCK_MOVEMENT)
			{
				CMessageMovementObstructionChanged msg;
				GetSimContext().GetComponentManager().PostMessage(GetEntityId(), msg);
			}
		}
		else if (!active && m_Active)
		{
			// 从激活 -> 非激活
			m_Active = false;

			// 删除障碍物形状
			// TODO: 此处代码与消息处理器中有重复
			if (m_Tag.valid())
			{
				CmpPtr<ICmpObstructionManager> cmpObstructionManager(GetSystemEntity());
				if (!cmpObstructionManager)
					return; // 错误

				cmpObstructionManager->RemoveShape(m_Tag);
				m_Tag = tag_t();
				if (m_Type == CLUSTER)
					RemoveClusterShapes();
			}

			// 由UnitMotion使用，以激活/停用推挤行为
			if (m_TemplateFlags & ICmpObstructionManager::FLAG_BLOCK_MOVEMENT)
			{
				CMessageMovementObstructionChanged msg;
				GetSimContext().GetComponentManager().PostMessage(GetEntityId(), msg);
			}
		}
		// else 状态未改变，什么都不做
	}

	// 设置是否禁用移动和寻路阻挡
	void SetDisableBlockMovementPathfinding(bool movementDisabled, bool pathfindingDisabled, int32_t shape) override
	{
		flags_t* flags = NULL;
		if (shape == -1) // -1表示主形状
			flags = &m_Flags;
		else if (m_Type == CLUSTER && shape < (int32_t)m_Shapes.size()) // 否则是某个子形状
			flags = &m_Shapes[shape].flags;
		else
			return; // 错误

		// 移除阻挡/寻路标志，或者
		// 如果模板中启用了它们，则重新添加
		if (movementDisabled)
			*flags &= (flags_t)(~ICmpObstructionManager::FLAG_BLOCK_MOVEMENT);
		else
			*flags |= (flags_t)(m_TemplateFlags & ICmpObstructionManager::FLAG_BLOCK_MOVEMENT);
		if (pathfindingDisabled)
			*flags &= (flags_t)(~ICmpObstructionManager::FLAG_BLOCK_PATHFINDING);
		else
			*flags |= (flags_t)(m_TemplateFlags & ICmpObstructionManager::FLAG_BLOCK_PATHFINDING);

		// 用新的标志重置形状（效率有点低 - 我们应该有一个
		// ICmpObstructionManager::SetFlags之类的函数）
		if (m_Active)
		{
			SetActive(false);
			SetActive(true);
		}
	}

	// 获取移动阻挡标志
	bool GetBlockMovementFlag(bool templateOnly) const override
	{
		// 必须是激活状态，且相应标志位为1
		return m_Active && ((templateOnly ? m_TemplateFlags : m_Flags) & ICmpObstructionManager::FLAG_BLOCK_MOVEMENT) != 0;
	}

	// 获取障碍物类型
	EObstructionType GetObstructionType() const override
	{
		return m_Type;
	}

	// 获取障碍物标签
	ICmpObstructionManager::tag_t GetObstruction() const override
	{
		return m_Tag;
	}

	// 获取上一回合的障碍物占据范围
	bool GetPreviousObstructionSquare(ICmpObstructionManager::ObstructionSquare& out) const override
	{
		return GetObstructionSquare(out, true);
	}

	// 获取当前回合的障碍物占据范围
	bool GetObstructionSquare(ICmpObstructionManager::ObstructionSquare& out) const override
	{
		return GetObstructionSquare(out, false);
	}

	// 获取障碍物占据范围（核心实现）
	virtual bool GetObstructionSquare(ICmpObstructionManager::ObstructionSquare& out, bool previousPosition) const
	{
		CmpPtr<ICmpPosition> cmpPosition(GetEntityHandle());
		if (!cmpPosition)
			return false; // 错误

		CmpPtr<ICmpObstructionManager> cmpObstructionManager(GetSystemEntity());
		if (!cmpObstructionManager)
			return false; // 错误

		if (!cmpPosition->IsInWorld())
			return false; // 不在世界中，没有占据范围

		// 根据是请求当前位置还是上一回合位置，获取坐标
		CFixedVector2D pos;
		if (previousPosition)
			pos = cmpPosition->GetPreviousPosition2D();
		else
			pos = cmpPosition->GetPosition2D();

		// 根据障碍物类型计算占据范围
		if (m_Type == UNIT)
			out = cmpObstructionManager->GetUnitShapeObstruction(pos.X, pos.Y, m_Clearance);
		else
			out = cmpObstructionManager->GetStaticShapeObstruction(pos.X, pos.Y, cmpPosition->GetRotation().Y, m_Size0, m_Size1);
		return true;
	}

	// 获取障碍物尺寸（半径）
	entity_pos_t GetSize() const override
	{
		if (m_Type == UNIT)
			return m_Clearance;
		else
			return CFixedVector2D(m_Size0 / 2, m_Size1 / 2).Length();
	}

	// 获取静态尺寸（宽度和深度）
	CFixedVector2D GetStaticSize() const override
	{
		return m_Type == STATIC ? CFixedVector2D(m_Size0, m_Size1) : CFixedVector2D();
	}

	// 设置单位的间隙大小
	void SetUnitClearance(const entity_pos_t& clearance) override
	{
		// 此函数不发送 MovementObstructionChanged 消息
		// 因为它只是一个解决初始化顺序问题的权宜之计，并直接在 UnitMotion 中使用。
		if (m_Type == UNIT)
			m_Clearance = clearance;
	}

	// 检查控制组是否是“持久”的
	bool IsControlPersistent() const override
	{
		return m_ControlPersist;
	}

	// 检查是否适合在岸边放置（例如码头）
	bool CheckShorePlacement() const override
	{
		ICmpObstructionManager::ObstructionSquare s;
		if (!GetObstructionSquare(s))
			return false;

		// 计算形状的前后点
		CFixedVector2D front = CFixedVector2D(s.x, s.z) + s.v.Multiply(s.hh);
		CFixedVector2D back = CFixedVector2D(s.x, s.z) - s.v.Multiply(s.hh);

		CmpPtr<ICmpTerrain> cmpTerrain(GetSystemEntity());
		CmpPtr<ICmpWaterManager> cmpWaterManager(GetSystemEntity());
		if (!cmpTerrain || !cmpWaterManager)
			return false;

		// 保证这些常量与寻路器中的一致。
		// 检查前端是否在水中足够深，后端是否在陆地上
		return cmpWaterManager->GetWaterLevel(front.X, front.Y) - cmpTerrain->GetGroundLevel(front.X, front.Y) > fixed::FromInt(1) &&
			cmpWaterManager->GetWaterLevel(back.X, back.Y) - cmpTerrain->GetGroundLevel(back.X, back.Y) < fixed::FromInt(2);
	}

	// 检查地基放置的合法性
	EFoundationCheck CheckFoundation(const std::string& className) const override
	{
		return CheckFoundation(className, false);
	}

	// 检查地基放置的合法性（核心实现）
	EFoundationCheck CheckFoundation(const std::string& className, bool onlyCenterPoint) const override
	{
		CmpPtr<ICmpPosition> cmpPosition(GetEntityHandle());
		if (!cmpPosition)
			return FOUNDATION_CHECK_FAIL_ERROR; // 错误

		if (!cmpPosition->IsInWorld())
			return FOUNDATION_CHECK_FAIL_NO_OBSTRUCTION; // 没有障碍物

		CFixedVector2D pos = cmpPosition->GetPosition2D();

		CmpPtr<ICmpPathfinder> cmpPathfinder(GetSystemEntity());
		if (!cmpPathfinder)
			return FOUNDATION_CHECK_FAIL_ERROR; // 错误

		// 使用 SkipControlGroupsRequireFlagObstructionFilter 的前提条件
		if (m_ControlGroup == INVALID_ENTITY)
		{
			LOGERROR("[CmpObstruction] 无法测试地基障碍物；主控制组必须有效");
			return FOUNDATION_CHECK_FAIL_ERROR;
		}

		// 获取通行性类别
		pass_class_t passClass = cmpPathfinder->GetPassabilityClass(className);

		// 忽略同一控制组内的碰撞，或与其他不阻挡地基的形状的碰撞。
		// 注意，由于每个实体的控制组默认为其实体ID，这通常等同于
		// 只忽略实体自身的形状和其他不阻挡地基的形状。
		SkipControlGroupsRequireFlagObstructionFilter filter(m_ControlGroup, m_ControlGroup2,
			ICmpObstructionManager::FLAG_BLOCK_FOUNDATION);

		// 根据类型调用不同的放置检查函数
		if (m_Type == UNIT)
			return cmpPathfinder->CheckUnitPlacement(filter, pos.X, pos.Y, m_Clearance, passClass, onlyCenterPoint);
		else
			return cmpPathfinder->CheckBuildingPlacement(filter, pos.X, pos.Y, cmpPosition->GetRotation().Y, m_Size0, m_Size1, GetEntityId(), passClass, onlyCenterPoint);
	}

	// 检查是否有重复的地基
	bool CheckDuplicateFoundation() const override
	{
		CmpPtr<ICmpPosition> cmpPosition(GetEntityHandle());
		if (!cmpPosition)
			return false; // 错误

		if (!cmpPosition->IsInWorld())
			return false; // 没有障碍物

		CFixedVector2D pos = cmpPosition->GetPosition2D();

		CmpPtr<ICmpObstructionManager> cmpObstructionManager(GetSystemEntity());
		if (!cmpObstructionManager)
			return false; // 错误

		// 使用 SkipControlGroupsRequireFlagObstructionFilter 的前提条件
		if (m_ControlGroup == INVALID_ENTITY)
		{
			LOGERROR("[CmpObstruction] 无法测试地基障碍物；主控制组必须有效");
			return false;
		}

		// 忽略与实体的碰撞，除非它们阻挡地基并匹配两个控制组。
		SkipTagRequireControlGroupsAndFlagObstructionFilter filter(m_Tag, m_ControlGroup, m_ControlGroup2,
			ICmpObstructionManager::FLAG_BLOCK_FOUNDATION);

		if (m_Type == UNIT)
			return !cmpObstructionManager->TestUnitShape(filter, pos.X, pos.Y, m_Clearance, NULL);
		else
			return !cmpObstructionManager->TestStaticShape(filter, pos.X, pos.Y, cmpPosition->GetRotation().Y, m_Size0, m_Size1, NULL);
	}

	// 根据标志获取与之碰撞的实体列表
	std::vector<entity_id_t> GetEntitiesByFlags(flags_t flags) const override
	{
		std::vector<entity_id_t> ret;

		CmpPtr<ICmpObstructionManager> cmpObstructionManager(GetSystemEntity());
		if (!cmpObstructionManager)
			return ret; // 错误

		// 忽略同一控制组内的碰撞，或与不匹配过滤器的其他形状的碰撞。
		SkipControlGroupsRequireFlagObstructionFilter filter(false, m_ControlGroup, m_ControlGroup2, flags);

		ICmpObstructionManager::ObstructionSquare square;
		if (!GetObstructionSquare(square))
			return ret; // 错误

		// 在障碍物范围内查找单位和静态障碍物
		cmpObstructionManager->GetUnitsOnObstruction(square, ret, filter, false);
		cmpObstructionManager->GetStaticObstructionsOnObstruction(square, ret, filter);

		return ret;
	}

	// 获取阻挡移动的实体列表
	std::vector<entity_id_t> GetEntitiesBlockingMovement() const override
	{
		return GetEntitiesByFlags(ICmpObstructionManager::FLAG_BLOCK_MOVEMENT);
	}

	// 获取阻挡建造的实体列表
	std::vector<entity_id_t> GetEntitiesBlockingConstruction() const override
	{
		return GetEntitiesByFlags(ICmpObstructionManager::FLAG_BLOCK_CONSTRUCTION);
	}

	// 获取建造时会被删除的实体列表
	std::vector<entity_id_t> GetEntitiesDeletedUponConstruction() const override
	{
		return GetEntitiesByFlags(ICmpObstructionManager::FLAG_DELETE_UPON_CONSTRUCTION);
	}

	// 设置单位是否正在移动的标志
	void SetMovingFlag(bool enabled) override
	{
		m_Moving = enabled;

		if (m_Tag.valid() && m_Type == UNIT)
		{
			CmpPtr<ICmpObstructionManager> cmpObstructionManager(GetSystemEntity());
			if (cmpObstructionManager)
				cmpObstructionManager->SetUnitMovingFlag(m_Tag, m_Moving);
		}
	}

	// 设置主控制组
	void SetControlGroup(entity_id_t group) override
	{
		m_ControlGroup = group;
		UpdateControlGroups();
	}

	// 设置次控制组
	void SetControlGroup2(entity_id_t group2) override
	{
		m_ControlGroup2 = group2;
		UpdateControlGroups();
	}

	// 获取主控制组
	entity_id_t GetControlGroup() const override
	{
		return m_ControlGroup;
	}

	// 获取次控制组
	entity_id_t GetControlGroup2() const override
	{
		return m_ControlGroup2;
	}

	// 更新障碍物管理器中的控制组信息
	void UpdateControlGroups()
	{
		if (m_Tag.valid())
		{
			CmpPtr<ICmpObstructionManager> cmpObstructionManager(GetSystemEntity());
			if (cmpObstructionManager)
			{
				if (m_Type == UNIT)
				{
					cmpObstructionManager->SetUnitControlGroup(m_Tag, m_ControlGroup);
				}
				else // STATIC 或 CLUSTER
				{
					cmpObstructionManager->SetStaticControlGroup(m_Tag, m_ControlGroup, m_ControlGroup2);
					if (m_Type == CLUSTER)
					{
						for (size_t i = 0; i < m_ClusterTags.size(); ++i)
						{
							cmpObstructionManager->SetStaticControlGroup(m_ClusterTags[i], m_ControlGroup, m_ControlGroup2);
						}
					}
				}
			}
		}
	}

	// 解决地基碰撞问题
	void ResolveFoundationCollisions() const override
	{
		if (m_Type == UNIT)
			return; // 单位不参与地基碰撞解决

		CmpPtr<ICmpObstructionManager> cmpObstructionManager(GetSystemEntity());
		if (!cmpObstructionManager)
			return;

		CmpPtr<ICmpPosition> cmpPosition(GetEntityHandle());
		if (!cmpPosition)
			return; // 错误

		if (!cmpPosition->IsInWorld())
			return; // 没有障碍物

		CFixedVector2D pos = cmpPosition->GetPosition2D();

		// 过滤器：忽略同组或不阻挡地基的物体
		SkipControlGroupsRequireFlagObstructionFilter filter(m_ControlGroup, m_ControlGroup2,
			ICmpObstructionManager::FLAG_BLOCK_FOUNDATION);

		std::vector<entity_id_t> collisions;
		// 测试是否有碰撞
		if (cmpObstructionManager->TestStaticShape(filter, pos.X, pos.Y, cmpPosition->GetRotation().Y, m_Size0, m_Size1, &collisions))
		{
			std::vector<entity_id_t> persistentEnts, normalEnts;

			// 将实体分类为“持久控制组”和“普通实体”
			if (m_ControlPersist)
				persistentEnts.push_back(m_ControlGroup);
			else
				normalEnts.push_back(GetEntityId());

			for (std::vector<entity_id_t>::iterator it = collisions.begin(); it != collisions.end(); ++it)
			{
				entity_id_t ent = *it;
				if (ent == INVALID_ENTITY)
					continue;

				CmpPtr<ICmpObstruction> cmpObstruction(GetSimContext(), ent);
				if (!cmpObstruction->IsControlPersistent())
					normalEnts.push_back(ent);
				else
					persistentEnts.push_back(cmpObstruction->GetControlGroup());
			}

			// 如果没有可用的持久控制组，则无法解决碰撞
			if (persistentEnts.empty())
				return;

			// 尝试用一个持久控制组替换碰撞实体的控制组
			for (const entity_id_t normalEnt : normalEnts)
			{
				CmpPtr<ICmpObstruction> cmpObstruction(GetSimContext(), normalEnt);
				for (const entity_id_t persistent : normalEnts) // 似乎这里应该是 persistentEnts?
				{
					entity_id_t group = cmpObstruction->GetControlGroup();

					// 只覆盖“默认”的控制组（即控制组ID等于实体ID）
					if (group == normalEnt)
						cmpObstruction->SetControlGroup(persistent);
					// 或者如果次要控制组为空，则设置次要控制组
					else if (cmpObstruction->GetControlGroup2() == INVALID_ENTITY && group != persistent)
						cmpObstruction->SetControlGroup2(persistent);
				}
			}
		}
	}
protected:

	// 辅助函数：添加复合障碍物的所有子形状
	inline void AddClusterShapes(entity_pos_t x, entity_pos_t z, entity_angle_t a)
	{
		CmpPtr<ICmpObstructionManager> cmpObstructionManager(GetSystemEntity());
		if (!cmpObstructionManager)
			return; // 错误

		flags_t flags = m_Flags;
		// 复合障碍物的主形状本身不阻挡移动和寻路，只有子形状才阻挡
		flags &= (flags_t)(~ICmpObstructionManager::FLAG_BLOCK_MOVEMENT);
		flags &= (flags_t)(~ICmpObstructionManager::FLAG_BLOCK_PATHFINDING);

		// 添加主形状（作为包围盒）
		m_Tag = cmpObstructionManager->AddStaticShape(GetEntityId(),
			x, z, a, m_Size0, m_Size1, flags, m_ControlGroup, m_ControlGroup2);

		fixed s, c;
		sincos_approx(a, s, c);

		// 添加所有子形状
		for (size_t i = 0; i < m_Shapes.size(); ++i)
		{
			Shape& b = m_Shapes[i];
			tag_t tag = cmpObstructionManager->AddStaticShape(GetEntityId(),
				x + b.dx.Multiply(c) + b.dz.Multiply(s), z + b.dz.Multiply(c) - b.dx.Multiply(s), a + b.da, b.size0, b.size1, b.flags, m_ControlGroup, m_ControlGroup2);
			m_ClusterTags.push_back(tag);
		}
	}

	// 辅助函数：移除复合障碍物的所有子形状
	inline void RemoveClusterShapes()
	{
		CmpPtr<ICmpObstructionManager> cmpObstructionManager(GetSystemEntity());
		if (!cmpObstructionManager)
			return; // 错误

		for (size_t i = 0; i < m_ClusterTags.size(); ++i)
		{
			if (m_ClusterTags[i].valid())
			{
				cmpObstructionManager->RemoveShape(m_ClusterTags[i]);
			}
		}
		m_ClusterTags.clear();
	}

};

// 注册此组件类型
REGISTER_COMPONENT_TYPE(Obstruction)
