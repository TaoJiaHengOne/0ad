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

 // 预编译头文件
#include "precompiled.h"

// 核心组件和接口
#include "simulation2/system/Component.h"
#include "ICmpObstructionManager.h"

// 其他相关组件接口
#include "ICmpPosition.h"
#include "ICmpRangeManager.h"

// 消息类型、辅助工具和序列化
#include "simulation2/MessageTypes.h"
#include "simulation2/helpers/Geometry.h"
#include "simulation2/helpers/Grid.h"
#include "simulation2/helpers/Rasterize.h"
#include "simulation2/helpers/Render.h"
#include "simulation2/helpers/Spatial.h"
#include "simulation2/serialization/SerializedTypes.h"

// 图形、数学和日志工具
#include "graphics/Overlay.h"
#include "maths/MathUtil.h"
#include "ps/Profile.h"
#include "renderer/Scene.h"
#include "ps/CLogger.h"

// 在外部，标签(tag)是不透明的非零正整数。
// 在内部，它们是被标记（按形状类型）的形状列表索引。
// 通过位操作，一个整数可以同时存储类型（单位/静态）和索引。
// idx 必须非零。
#define TAG_IS_VALID(tag) ((tag).valid())      // 检查标签是否有效
#define TAG_IS_UNIT(tag) (((tag).n & 1) == 0)   // 检查标签是否为单位类型 (最低位为0)
#define TAG_IS_STATIC(tag) (((tag).n & 1) == 1) // 检查标签是否为静态类型 (最低位为1)
#define UNIT_INDEX_TO_TAG(idx) tag_t(((idx) << 1) | 0)   // 将单位索引转换为标签 (左移一位，最低位置0)
#define STATIC_INDEX_TO_TAG(idx) tag_t(((idx) << 1) | 1) // 将静态索引转换为标签 (左移一位，最低位置1)
#define TAG_TO_INDEX(tag) ((tag).n >> 1)       // 从标签中提取索引 (右移一位)

namespace
{
	/**
	 * 每个障碍物细分方格的大小。
	 * TODO: 寻找最优值，而不是盲目猜测。
	 */
	constexpr entity_pos_t OBSTRUCTION_SUBDIVISION_SIZE = entity_pos_t::FromInt(32);

	/**
	 * 移动单位的轴对齐圆形形状的内部表示。
	 */
	struct UnitShape
	{
		entity_id_t entity;      // 实体ID
		entity_pos_t x, z;       // 世界坐标
		entity_pos_t clearance;  // 间隙（半径）
		ICmpObstructionManager::flags_t flags; // 标志位
		entity_id_t group;       // 控制组 (通常是所有者实体，或阵型控制器实体) (单位会忽略同组内的其他单位碰撞)
	};

	/**
	 * 建筑等静态物体的任意旋转方形形状的内部表示。
	 */
	struct StaticShape
	{
		entity_id_t entity;      // 实体ID
		entity_pos_t x, z;       // 世界坐标
		CFixedVector2D u, v;     // 正交单位向量 - 局部坐标系的轴
		entity_pos_t hw, hh;     // 局部坐标系下的半宽/半高
		ICmpObstructionManager::flags_t flags; // 标志位
		entity_id_t group;       // 主控制组
		entity_id_t group2;      // 次控制组
	};
} // anonymous namespace

/**
 * UnitShape 的序列化帮助器模板特化。
 */
template<>
struct SerializeHelper<UnitShape>
{
	template<typename S>
	void operator()(S& serialize, const char* UNUSED(name), Serialize::qualify<S, UnitShape> value) const
	{
		serialize.NumberU32_Unbounded("entity", value.entity);
		serialize.NumberFixed_Unbounded("x", value.x);
		serialize.NumberFixed_Unbounded("z", value.z);
		serialize.NumberFixed_Unbounded("clearance", value.clearance);
		serialize.NumberU8_Unbounded("flags", value.flags);
		serialize.NumberU32_Unbounded("group", value.group);
	}
};

/**
 * StaticShape 的序列化帮助器模板特化。
 */
template<>
struct SerializeHelper<StaticShape>
{
	template<typename S>
	void operator()(S& serialize, const char* UNUSED(name), Serialize::qualify<S, StaticShape> value) const
	{
		serialize.NumberU32_Unbounded("entity", value.entity);
		serialize.NumberFixed_Unbounded("x", value.x);
		serialize.NumberFixed_Unbounded("z", value.z);
		serialize.NumberFixed_Unbounded("u.x", value.u.X);
		serialize.NumberFixed_Unbounded("u.y", value.u.Y);
		serialize.NumberFixed_Unbounded("v.x", value.v.X);
		serialize.NumberFixed_Unbounded("v.y", value.v.Y);
		serialize.NumberFixed_Unbounded("hw", value.hw);
		serialize.NumberFixed_Unbounded("hh", value.hh);
		serialize.NumberU8_Unbounded("flags", value.flags);
		serialize.NumberU32_Unbounded("group", value.group);
		serialize.NumberU32_Unbounded("group2", value.group2);
	}
};

// 障碍物管理器的具体实现，这是一个系统组件
class CCmpObstructionManager final : public ICmpObstructionManager
{
public:
	// 静态类初始化
	static void ClassInit(CComponentManager& componentManager)
	{
		// 订阅渲染提交消息，用于调试叠加层的绘制
		componentManager.SubscribeToMessageType(MT_RenderSubmit);
	}

	// 使用默认的组件分配器宏
	DEFAULT_COMPONENT_ALLOCATOR(ObstructionManager)

		// --- 成员变量 ---
		bool m_DebugOverlayEnabled; // 是否启用调试叠加层
	bool m_DebugOverlayDirty;   // 调试叠加层是否需要重新生成
	std::vector<SOverlayLine> m_DebugOverlayLines; // 存储调试叠加层的线段

	// 空间细分数据结构，用于快速查询附近的障碍物
	SpatialSubdivision m_UnitSubdivision;   // 单位障碍物的空间划分
	SpatialSubdivision m_StaticSubdivision; // 静态障碍物的空间划分

	// 存储所有障碍物形状的具体数据
	// TODO: 使用 std::map 有点低效；有没有更好的方式存储这些？
	std::map<u32, UnitShape> m_UnitShapes;
	std::map<u32, StaticShape> m_StaticShapes;
	u32 m_UnitShapeNext;   // 下一个分配的单位形状ID
	u32 m_StaticShapeNext; // 下一个分配的静态形状ID

	// 缓存的最大单位间隙值
	entity_pos_t m_MaxClearance;

	// 通行性计算是否使用圆形模型
	bool m_PassabilityCircular;

	// 世界地图的边界坐标
	entity_pos_t m_WorldX0;
	entity_pos_t m_WorldZ0;
	entity_pos_t m_WorldX1;
	entity_pos_t m_WorldZ1;

	// 定义组件的XML结构
	static std::string GetSchema()
	{
		return "<a:component type='system'/><empty/>";
	}

	// 初始化
	void Init(const CParamNode&) override
	{
		m_DebugOverlayEnabled = false;
		m_DebugOverlayDirty = true;

		m_UnitShapeNext = 1; // ID从1开始，0为无效
		m_StaticShapeNext = 1;

		m_UpdateInformations.dirty = true;
		m_UpdateInformations.globallyDirty = true;

		m_PassabilityCircular = false;

		m_WorldX0 = m_WorldZ0 = m_WorldX1 = m_WorldZ1 = entity_pos_t::Zero();

		// 用伪值初始化 (这些值会在SetBounds调用时被替换)
		ResetSubdivisions(entity_pos_t::FromInt(1024), entity_pos_t::FromInt(1024));
	}

	// 销毁
	void Deinit() override
	{
	}

	// 序列化和反序列化的通用辅助函数
	template<typename S>
	void SerializeCommon(S& serialize)
	{
		Serializer(serialize, "unit subdiv", m_UnitSubdivision);
		Serializer(serialize, "static subdiv", m_StaticSubdivision);

		serialize.NumberFixed_Unbounded("max clearance", m_MaxClearance);

		Serializer(serialize, "unit shapes", m_UnitShapes);
		Serializer(serialize, "static shapes", m_StaticShapes);
		serialize.NumberU32_Unbounded("unit shape next", m_UnitShapeNext);
		serialize.NumberU32_Unbounded("static shape next", m_StaticShapeNext);

		serialize.Bool("circular", m_PassabilityCircular);

		serialize.NumberFixed_Unbounded("world x0", m_WorldX0);
		serialize.NumberFixed_Unbounded("world z0", m_WorldZ0);
		serialize.NumberFixed_Unbounded("world x1", m_WorldX1);
		serialize.NumberFixed_Unbounded("world z1", m_WorldZ1);
	}

	// 序列化（保存）
	void Serialize(ISerializer& serialize) override
	{
		// TODO: 也许可以通过不存储所有障碍物来优化，
		// 而是在反序列化时从其他实体重新生成它们。
		SerializeCommon(serialize);
	}

	// 反序列化（加载）
	void Deserialize(const CParamNode& paramNode, IDeserializer& deserialize) override
	{
		Init(paramNode);

		SerializeCommon(deserialize);

		// 重新创建脏区网格
		i32 size = ((m_WorldX1 - m_WorldX0) / Pathfinding::NAVCELL_SIZE_INT).ToInt_RoundToInfinity();
		m_UpdateInformations.dirtinessGrid = Grid<u8>(size, size);
	}

	// 消息处理
	void HandleMessage(const CMessage& msg, bool UNUSED(global)) override
	{
		switch (msg.GetType())
		{
		case MT_RenderSubmit: // 响应渲染提交消息
		{
			const CMessageRenderSubmit& msgData = static_cast<const CMessageRenderSubmit&> (msg);
			RenderSubmit(msgData.collector); // 提交调试绘图
			break;
		}
		}
	}

	// 注意：反序列化时，此函数在组件重置后不会被调用。
	// 所以这里发生的任何事情都应该被安全地序列化。
	void SetBounds(entity_pos_t x0, entity_pos_t z0, entity_pos_t x1, entity_pos_t z1) override
	{
		m_WorldX0 = x0;
		m_WorldZ0 = z0;
		m_WorldX1 = x1;
		m_WorldZ1 = z1;
		MakeDirtyAll(); // 将所有内容标记为脏

		// 细分系统的边界：
		ENSURE(x0.IsZero() && z0.IsZero()); // 暂时不实现非零偏移
		ResetSubdivisions(x1, z1);

		i32 size = ((m_WorldX1 - m_WorldX0) / Pathfinding::NAVCELL_SIZE_INT).ToInt_RoundToInfinity();
		m_UpdateInformations.dirtinessGrid = Grid<u8>(size, size);

		CmpPtr<ICmpPathfinder> cmpPathfinder(GetSystemEntity());
		if (cmpPathfinder)
			m_MaxClearance = cmpPathfinder->GetMaximumClearance();
	}

	// 重置空间细分网格
	void ResetSubdivisions(entity_pos_t x1, entity_pos_t z1)
	{
		m_UnitSubdivision.Reset(x1, z1, OBSTRUCTION_SUBDIVISION_SIZE);
		m_StaticSubdivision.Reset(x1, z1, OBSTRUCTION_SUBDIVISION_SIZE);

		// 将所有现有形状重新添加到新的网格中
		for (std::map<u32, UnitShape>::iterator it = m_UnitShapes.begin(); it != m_UnitShapes.end(); ++it)
		{
			CFixedVector2D center(it->second.x, it->second.z);
			CFixedVector2D halfSize(it->second.clearance, it->second.clearance);
			m_UnitSubdivision.Add(it->first, center - halfSize, center + halfSize);
		}

		for (std::map<u32, StaticShape>::iterator it = m_StaticShapes.begin(); it != m_StaticShapes.end(); ++it)
		{
			CFixedVector2D center(it->second.x, it->second.z);
			CFixedVector2D bbHalfSize = Geometry::GetHalfBoundingBox(it->second.u, it->second.v, CFixedVector2D(it->second.hw, it->second.hh));
			m_StaticSubdivision.Add(it->first, center - bbHalfSize, center + bbHalfSize);
		}
	}

	// 添加一个单位形状
	tag_t AddUnitShape(entity_id_t ent, entity_pos_t x, entity_pos_t z, entity_pos_t clearance, flags_t flags, entity_id_t group) override
	{
		UnitShape shape = { ent, x, z, clearance, flags, group };
		u32 id = m_UnitShapeNext++;
		m_UnitShapes[id] = shape;

		m_UnitSubdivision.Add(id, CFixedVector2D(x - clearance, z - clearance), CFixedVector2D(x + clearance, z + clearance));

		MakeDirtyUnit(flags, id, shape); // 将相关区域标记为脏

		return UNIT_INDEX_TO_TAG(id);
	}

	// 添加一个静态形状
	tag_t AddStaticShape(entity_id_t ent, entity_pos_t x, entity_pos_t z, entity_angle_t a, entity_pos_t w, entity_pos_t h, flags_t flags, entity_id_t group, entity_id_t group2 /* = INVALID_ENTITY */) override
	{
		fixed s, c;
		sincos_approx(a, s, c);
		CFixedVector2D u(c, -s);
		CFixedVector2D v(s, c);

		StaticShape shape = { ent, x, z, u, v, w / 2, h / 2, flags, group, group2 };
		u32 id = m_StaticShapeNext++;
		m_StaticShapes[id] = shape;

		CFixedVector2D center(x, z);
		CFixedVector2D bbHalfSize = Geometry::GetHalfBoundingBox(u, v, CFixedVector2D(w / 2, h / 2));
		m_StaticSubdivision.Add(id, center - bbHalfSize, center + bbHalfSize);

		MakeDirtyStatic(flags, id, shape); // 将相关区域标记为脏

		return STATIC_INDEX_TO_TAG(id);
	}

	// 获取单位形状的障碍物方块表示
	ObstructionSquare GetUnitShapeObstruction(entity_pos_t x, entity_pos_t z, entity_pos_t clearance) const override
	{
		CFixedVector2D u(entity_pos_t::FromInt(1), entity_pos_t::Zero());
		CFixedVector2D v(entity_pos_t::Zero(), entity_pos_t::FromInt(1));
		ObstructionSquare o = { x, z, u, v, clearance, clearance };
		return o;
	}

	// 获取静态形状的障碍物方块表示
	ObstructionSquare GetStaticShapeObstruction(entity_pos_t x, entity_pos_t z, entity_angle_t a, entity_pos_t w, entity_pos_t h) const override
	{
		fixed s, c;
		sincos_approx(a, s, c);
		CFixedVector2D u(c, -s);
		CFixedVector2D v(s, c);

		ObstructionSquare o = { x, z, u, v, w / 2, h / 2 };
		return o;
	}

	// 移动一个形状
	void MoveShape(tag_t tag, entity_pos_t x, entity_pos_t z, entity_angle_t a) override
	{
		ENSURE(TAG_IS_VALID(tag));

		if (TAG_IS_UNIT(tag))
		{
			UnitShape& shape = m_UnitShapes[TAG_TO_INDEX(tag)];

			MakeDirtyUnit(shape.flags, TAG_TO_INDEX(tag), shape); // 标记旧区域为脏

			// 在空间细分网格中移动
			m_UnitSubdivision.Move(TAG_TO_INDEX(tag),
				CFixedVector2D(shape.x - shape.clearance, shape.z - shape.clearance),
				CFixedVector2D(shape.x + shape.clearance, shape.z + shape.clearance),
				CFixedVector2D(x - shape.clearance, z - shape.clearance),
				CFixedVector2D(x + shape.clearance, z + shape.clearance));

			// 更新形状数据
			shape.x = x;
			shape.z = z;

			MakeDirtyUnit(shape.flags, TAG_TO_INDEX(tag), shape); // 标记新区域为脏
		}
		else
		{
			fixed s, c;
			sincos_approx(a, s, c);
			CFixedVector2D u(c, -s);
			CFixedVector2D v(s, c);

			StaticShape& shape = m_StaticShapes[TAG_TO_INDEX(tag)];

			MakeDirtyStatic(shape.flags, TAG_TO_INDEX(tag), shape); // 标记旧区域为脏

			CFixedVector2D fromBbHalfSize = Geometry::GetHalfBoundingBox(shape.u, shape.v, CFixedVector2D(shape.hw, shape.hh));
			CFixedVector2D toBbHalfSize = Geometry::GetHalfBoundingBox(u, v, CFixedVector2D(shape.hw, shape.hh));
			m_StaticSubdivision.Move(TAG_TO_INDEX(tag),
				CFixedVector2D(shape.x, shape.z) - fromBbHalfSize,
				CFixedVector2D(shape.x, shape.z) + fromBbHalfSize,
				CFixedVector2D(x, z) - toBbHalfSize,
				CFixedVector2D(x, z) + toBbHalfSize);

			// 更新形状数据
			shape.x = x;
			shape.z = z;
			shape.u = u;
			shape.v = v;

			MakeDirtyStatic(shape.flags, TAG_TO_INDEX(tag), shape); // 标记新区域为脏
		}
	}

	// 设置单位的移动标志
	void SetUnitMovingFlag(tag_t tag, bool moving) override
	{
		ENSURE(TAG_IS_VALID(tag) && TAG_IS_UNIT(tag));

		if (TAG_IS_UNIT(tag))
		{
			UnitShape& shape = m_UnitShapes[TAG_TO_INDEX(tag)];
			if (moving)
				shape.flags |= FLAG_MOVING;
			else
				shape.flags &= (flags_t)~FLAG_MOVING;

			MakeDirtyDebug(); // 仅需更新调试视图
		}
	}

	// 设置单位的控制组
	void SetUnitControlGroup(tag_t tag, entity_id_t group) override
	{
		ENSURE(TAG_IS_VALID(tag) && TAG_IS_UNIT(tag));

		if (TAG_IS_UNIT(tag))
		{
			UnitShape& shape = m_UnitShapes[TAG_TO_INDEX(tag)];
			shape.group = group;
		}
	}

	// 设置静态物体的控制组
	void SetStaticControlGroup(tag_t tag, entity_id_t group, entity_id_t group2) override
	{
		ENSURE(TAG_IS_VALID(tag) && TAG_IS_STATIC(tag));

		if (TAG_IS_STATIC(tag))
		{
			StaticShape& shape = m_StaticShapes[TAG_TO_INDEX(tag)];
			shape.group = group;
			shape.group2 = group2;
		}
	}

	// 移除一个形状
	void RemoveShape(tag_t tag) override
	{
		ENSURE(TAG_IS_VALID(tag));

		if (TAG_IS_UNIT(tag))
		{
			UnitShape& shape = m_UnitShapes[TAG_TO_INDEX(tag)];
			m_UnitSubdivision.Remove(TAG_TO_INDEX(tag),
				CFixedVector2D(shape.x - shape.clearance, shape.z - shape.clearance),
				CFixedVector2D(shape.x + shape.clearance, shape.z + shape.clearance));

			MakeDirtyUnit(shape.flags, TAG_TO_INDEX(tag), shape);

			m_UnitShapes.erase(TAG_TO_INDEX(tag));
		}
		else
		{
			StaticShape& shape = m_StaticShapes[TAG_TO_INDEX(tag)];

			CFixedVector2D center(shape.x, shape.z);
			CFixedVector2D bbHalfSize = Geometry::GetHalfBoundingBox(shape.u, shape.v, CFixedVector2D(shape.hw, shape.hh));
			m_StaticSubdivision.Remove(TAG_TO_INDEX(tag), center - bbHalfSize, center + bbHalfSize);

			MakeDirtyStatic(shape.flags, TAG_TO_INDEX(tag), shape);

			m_StaticShapes.erase(TAG_TO_INDEX(tag));
		}
	}

	// 根据标签获取障碍物方块
	ObstructionSquare GetObstruction(tag_t tag) const override
	{
		ENSURE(TAG_IS_VALID(tag));

		if (TAG_IS_UNIT(tag))
		{
			const UnitShape& shape = m_UnitShapes.at(TAG_TO_INDEX(tag));
			CFixedVector2D u(entity_pos_t::FromInt(1), entity_pos_t::Zero());
			CFixedVector2D v(entity_pos_t::Zero(), entity_pos_t::FromInt(1));
			ObstructionSquare o = { shape.x, shape.z, u, v, shape.clearance, shape.clearance };
			return o;
		}
		else
		{
			const StaticShape& shape = m_StaticShapes.at(TAG_TO_INDEX(tag));
			ObstructionSquare o = { shape.x, shape.z, shape.u, shape.v, shape.hw, shape.hh };
			return o;
		}
	}

	// --- 距离和范围查询函数 ---
	fixed DistanceToPoint(entity_id_t ent, entity_pos_t px, entity_pos_t pz) const override;
	fixed MaxDistanceToPoint(entity_id_t ent, entity_pos_t px, entity_pos_t pz) const override;
	fixed DistanceToTarget(entity_id_t ent, entity_id_t target) const override;
	fixed MaxDistanceToTarget(entity_id_t ent, entity_id_t target) const override;
	fixed DistanceBetweenShapes(const ObstructionSquare& source, const ObstructionSquare& target) const override;
	fixed MaxDistanceBetweenShapes(const ObstructionSquare& source, const ObstructionSquare& target) const override;

	bool IsInPointRange(entity_id_t ent, entity_pos_t px, entity_pos_t pz, entity_pos_t minRange, entity_pos_t maxRange, bool opposite) const override;
	bool IsInTargetRange(entity_id_t ent, entity_id_t target, entity_pos_t minRange, entity_pos_t maxRange, bool opposite) const override;
	bool IsInTargetParabolicRange(entity_id_t ent, entity_id_t target, entity_pos_t minRange, entity_pos_t maxRange, entity_pos_t yOrigin, bool opposite) const override;
	bool IsPointInPointRange(entity_pos_t x, entity_pos_t z, entity_pos_t px, entity_pos_t pz, entity_pos_t minRange, entity_pos_t maxRange) const override;
	bool AreShapesInRange(const ObstructionSquare& source, const ObstructionSquare& target, entity_pos_t minRange, entity_pos_t maxRange, bool opposite) const override;

	// --- 碰撞测试函数 ---
	bool TestLine(const IObstructionTestFilter& filter, entity_pos_t x0, entity_pos_t z0, entity_pos_t x1, entity_pos_t z1, entity_pos_t r, bool relaxClearanceForUnits = false) const override;
	bool TestUnitLine(const IObstructionTestFilter& filter, entity_pos_t x0, entity_pos_t z0, entity_pos_t x1, entity_pos_t z1, entity_pos_t r, bool relaxClearanceForUnits) const override;
	bool TestStaticShape(const IObstructionTestFilter& filter, entity_pos_t x, entity_pos_t z, entity_pos_t a, entity_pos_t w, entity_pos_t h, std::vector<entity_id_t>* out) const override;
	bool TestUnitShape(const IObstructionTestFilter& filter, entity_pos_t x, entity_pos_t z, entity_pos_t r, std::vector<entity_id_t>* out) const override;

	// --- 寻路和栅格化相关函数 ---
	void Rasterize(Grid<NavcellData>& grid, const std::vector<PathfinderPassability>& passClasses, bool fullUpdate) override;
	void GetObstructionsInRange(const IObstructionTestFilter& filter, entity_pos_t x0, entity_pos_t z0, entity_pos_t x1, entity_pos_t z1, std::vector<ObstructionSquare>& squares) const override;
	void GetUnitObstructionsInRange(const IObstructionTestFilter& filter, entity_pos_t x0, entity_pos_t z0, entity_pos_t x1, entity_pos_t z1, std::vector<ObstructionSquare>& squares) const override;
	void GetStaticObstructionsInRange(const IObstructionTestFilter& filter, entity_pos_t x0, entity_pos_t z0, entity_pos_t x1, entity_pos_t z1, std::vector<ObstructionSquare>& squares) const override;
	void GetUnitsOnObstruction(const ObstructionSquare& square, std::vector<entity_id_t>& out, const IObstructionTestFilter& filter, bool strict = false) const override;
	void GetStaticObstructionsOnObstruction(const ObstructionSquare& square, std::vector<entity_id_t>& out, const IObstructionTestFilter& filter) const override;

	// 设置通行性计算是否为圆形
	void SetPassabilityCircular(bool enabled) override
	{
		m_PassabilityCircular = enabled;
		MakeDirtyAll(); // 需要完全重算

		CMessageObstructionMapShapeChanged msg;
		GetSimContext().GetComponentManager().BroadcastMessage(msg);
	}

	// 获取通行性计算是否为圆形
	bool GetPassabilityCircular() const override
	{
		return m_PassabilityCircular;
	}

	// 设置是否启用调试叠加层
	void SetDebugOverlay(bool enabled) override
	{
		m_DebugOverlayEnabled = enabled;
		m_DebugOverlayDirty = true;
		if (!enabled)
			m_DebugOverlayLines.clear();
	}

	// 提交渲染（用于调试）
	void RenderSubmit(SceneCollector& collector);

	// 获取更新信息（用于寻路器）
	void UpdateInformations(GridUpdateInformation& informations) override
	{
		if (!m_UpdateInformations.dirtinessGrid.blank())
			informations.MergeAndClear(m_UpdateInformations);
	}

private:
	// 用于远程寻路器的动态更新
	GridUpdateInformation m_UpdateInformations; // 存储脏区网格和状态
	// 这些向量可能包含已被删除的形状的ID
	std::vector<u32> m_DirtyStaticShapes; // 脏的静态形状列表
	std::vector<u32> m_DirtyUnitShapes;   // 脏的单位形状列表

	/**
	 * 将所有之前栅格化的网格和调试显示标记为脏。
	 * 当世界边界改变时调用此函数。
	 */
	void MakeDirtyAll()
	{
		m_UpdateInformations.dirty = true;
		m_UpdateInformations.globallyDirty = true;
		m_UpdateInformations.dirtinessGrid.reset();

		m_DebugOverlayDirty = true;
	}

	/**
	 * 将调试显示标记为脏。
	 * 当除了单位的'moving'标志外没有任何变化时调用。
	 */
	void MakeDirtyDebug()
	{
		m_DebugOverlayDirty = true;
	}

	// 在脏区网格上标记一个圆形区域
	inline void MarkDirtinessGrid(const entity_pos_t& x, const entity_pos_t& z, const entity_pos_t& r)
	{
		MarkDirtinessGrid(x, z, CFixedVector2D(r, r));
	}

	// 在脏区网格上标记一个矩形区域
	inline void MarkDirtinessGrid(const entity_pos_t& x, const entity_pos_t& z, const CFixedVector2D& hbox)
	{
		if (m_UpdateInformations.dirtinessGrid.m_W == 0)
			return;

		u16 j0, j1, i0, i1;
		Pathfinding::NearestNavcell(x - hbox.X, z - hbox.Y, i0, j0, m_UpdateInformations.dirtinessGrid.m_W, m_UpdateInformations.dirtinessGrid.m_H);
		Pathfinding::NearestNavcell(x + hbox.X, z + hbox.Y, i1, j1, m_UpdateInformations.dirtinessGrid.m_W, m_UpdateInformations.dirtinessGrid.m_H);

		for (int j = j0; j < j1; ++j)
			for (int i = i0; i < i1; ++i)
				m_UpdateInformations.dirtinessGrid.set(i, j, 1);
	}

	/**
	 * 如果栅格化网格依赖于此形状，则将其标记为脏。
	 * 当静态形状改变时调用。
	 */
	void MakeDirtyStatic(flags_t flags, u32 index, const StaticShape& shape)
	{
		m_DebugOverlayDirty = true;

		// 提早退出以加速地图初始化。
		if (m_UpdateInformations.globallyDirty)
			return;

		if (flags & (FLAG_BLOCK_PATHFINDING | FLAG_BLOCK_FOUNDATION))
		{
			m_UpdateInformations.dirty = true;

			if (std::find(m_DirtyStaticShapes.begin(), m_DirtyStaticShapes.end(), index) == m_DirtyStaticShapes.end())
				m_DirtyStaticShapes.push_back(index);

			// 所有与网格更新部分重叠的形状也应该被标记为脏。
			// 我们将使与修改后的形状及其间隙对应的网格区域无效，
			// 我们需要获取其间隙可能与此区域重叠的形状。所以我们需要将搜索区域
			// 扩展两倍的最大间隙。
			CFixedVector2D center(shape.x, shape.z);
			CFixedVector2D hbox = Geometry::GetHalfBoundingBox(shape.u, shape.v, CFixedVector2D(shape.hw, shape.hh));
			CFixedVector2D expand(m_MaxClearance, m_MaxClearance);

			std::vector<u32> staticsNear;
			m_StaticSubdivision.GetInRange(staticsNear, center - hbox - expand * 2, center + hbox + expand * 2);
			for (u32& staticId : staticsNear)
				if (std::find(m_DirtyStaticShapes.begin(), m_DirtyStaticShapes.end(), staticId) == m_DirtyStaticShapes.end())
					m_DirtyStaticShapes.push_back(staticId);

			std::vector<u32> unitsNear;
			m_UnitSubdivision.GetInRange(unitsNear, center - hbox - expand * 2, center + hbox + expand * 2);
			for (u32& unitId : unitsNear)
				if (std::find(m_DirtyUnitShapes.begin(), m_DirtyUnitShapes.end(), unitId) == m_DirtyUnitShapes.end())
					m_DirtyUnitShapes.push_back(unitId);

			MarkDirtinessGrid(shape.x, shape.z, hbox + expand);
		}
	}

	/**
	 * 如果栅格化网格依赖于此形状，则将其标记为脏。
	 * 当单位形状改变时调用。
	 */
	void MakeDirtyUnit(flags_t flags, u32 index, const UnitShape& shape)
	{
		m_DebugOverlayDirty = true;

		// 提早退出以加速地图初始化。
		if (m_UpdateInformations.globallyDirty)
			return;

		if (flags & (FLAG_BLOCK_PATHFINDING | FLAG_BLOCK_FOUNDATION))
		{
			m_UpdateInformations.dirty = true;

			if (std::find(m_DirtyUnitShapes.begin(), m_DirtyUnitShapes.end(), index) == m_DirtyUnitShapes.end())
				m_DirtyUnitShapes.push_back(index);

			// 逻辑同上 MakeDirtyStatic
			CFixedVector2D center(shape.x, shape.z);

			std::vector<u32> staticsNear;
			m_StaticSubdivision.GetNear(staticsNear, center, shape.clearance + m_MaxClearance * 2);
			for (u32& staticId : staticsNear)
				if (std::find(m_DirtyStaticShapes.begin(), m_DirtyStaticShapes.end(), staticId) == m_DirtyStaticShapes.end())
					m_DirtyStaticShapes.push_back(staticId);

			std::vector<u32> unitsNear;
			m_UnitSubdivision.GetNear(unitsNear, center, shape.clearance + m_MaxClearance * 2);
			for (u32& unitId : unitsNear)
				if (std::find(m_DirtyUnitShapes.begin(), m_DirtyUnitShapes.end(), unitId) == m_DirtyUnitShapes.end())
					m_DirtyUnitShapes.push_back(unitId);

			MarkDirtinessGrid(shape.x, shape.z, shape.clearance + m_MaxClearance);
		}
	}

	/**
	 * 返回给定点是否在世界边界内（并至少有r的边距）。
	 */
	inline bool IsInWorld(entity_pos_t x, entity_pos_t z, entity_pos_t r) const
	{
		return (m_WorldX0 + r <= x && x <= m_WorldX1 - r && m_WorldZ0 + r <= z && z <= m_WorldZ1 - r);
	}

	/**
	 * 返回给定点是否在世界边界内。
	 */
	inline bool IsInWorld(const CFixedVector2D& p) const
	{
		return (m_WorldX0 <= p.X && p.X <= m_WorldX1 && m_WorldZ0 <= p.Y && p.Y <= m_WorldZ1);
	}

	// 栅格化辅助函数
	void RasterizeHelper(Grid<NavcellData>& grid, ICmpObstructionManager::flags_t requireMask, bool fullUpdate, pass_class_t appliedMask, entity_pos_t clearance = fixed::Zero()) const;
};

// 注册组件类型
REGISTER_COMPONENT_TYPE(ObstructionManager)

/**
 * DistanceTo 函数族，最终都会计算一个向量长度、DistanceBetweenShapes 或
 * MaxDistanceBetweenShapes。MaxFoo 系列计算的是对边到对边的距离。
 * 当距离未定义时，我们返回-1。
 */
	fixed CCmpObstructionManager::DistanceToPoint(entity_id_t ent, entity_pos_t px, entity_pos_t pz) const
{
	ObstructionSquare obstruction;
	CmpPtr<ICmpObstruction> cmpObstruction(GetSimContext(), ent);
	// 如果实体有障碍物组件，则计算形状到点的距离
	if (cmpObstruction && cmpObstruction->GetObstructionSquare(obstruction))
	{
		ObstructionSquare point;
		point.x = px;
		point.z = pz;
		return DistanceBetweenShapes(obstruction, point);
	}

	// 否则，计算实体中心到点的距离
	CmpPtr<ICmpPosition> cmpPosition(GetSimContext(), ent);
	if (!cmpPosition || !cmpPosition->IsInWorld())
		return fixed::FromInt(-1);

	return (CFixedVector2D(cmpPosition->GetPosition2D().X, cmpPosition->GetPosition2D().Y) - CFixedVector2D(px, pz)).Length();
}

// 计算实体到点的最大距离（对边到点）
fixed CCmpObstructionManager::MaxDistanceToPoint(entity_id_t ent, entity_pos_t px, entity_pos_t pz) const
{
	ObstructionSquare obstruction;
	CmpPtr<ICmpObstruction> cmpObstruction(GetSimContext(), ent);
	if (!cmpObstruction || !cmpObstruction->GetObstructionSquare(obstruction))
	{
		ObstructionSquare point;
		point.x = px;
		point.z = pz;
		return MaxDistanceBetweenShapes(obstruction, point);
	}

	CmpPtr<ICmpPosition> cmpPosition(GetSimContext(), ent);
	if (!cmpPosition || !cmpPosition->IsInWorld())
		return fixed::FromInt(-1);

	return (CFixedVector2D(cmpPosition->GetPosition2D().X, cmpPosition->GetPosition2D().Y) - CFixedVector2D(px, pz)).Length();
}

// 计算实体到目标的最小距离（边到边）
fixed CCmpObstructionManager::DistanceToTarget(entity_id_t ent, entity_id_t target) const
{
	ObstructionSquare obstruction;
	CmpPtr<ICmpObstruction> cmpObstruction(GetSimContext(), ent);
	if (!cmpObstruction || !cmpObstruction->GetObstructionSquare(obstruction))
	{
		CmpPtr<ICmpPosition> cmpPosition(GetSimContext(), ent);
		if (!cmpPosition || !cmpPosition->IsInWorld())
			return fixed::FromInt(-1);
		return DistanceToPoint(target, cmpPosition->GetPosition2D().X, cmpPosition->GetPosition2D().Y);
	}

	ObstructionSquare target_obstruction;
	CmpPtr<ICmpObstruction> cmpObstructionTarget(GetSimContext(), target);
	if (!cmpObstructionTarget || !cmpObstructionTarget->GetObstructionSquare(target_obstruction))
	{
		CmpPtr<ICmpPosition> cmpPositionTarget(GetSimContext(), target);
		if (!cmpPositionTarget || !cmpPositionTarget->IsInWorld())
			return fixed::FromInt(-1);
		return DistanceToPoint(ent, cmpPositionTarget->GetPosition2D().X, cmpPositionTarget->GetPosition2D().Y);
	}

	return DistanceBetweenShapes(obstruction, target_obstruction);
}

// 计算实体到目标的最大距离（对边到对边）
fixed CCmpObstructionManager::MaxDistanceToTarget(entity_id_t ent, entity_id_t target) const
{
	ObstructionSquare obstruction;
	CmpPtr<ICmpObstruction> cmpObstruction(GetSimContext(), ent);
	if (!cmpObstruction || !cmpObstruction->GetObstructionSquare(obstruction))
	{
		CmpPtr<ICmpPosition> cmpPosition(GetSimContext(), ent);
		if (!cmpPosition || !cmpPosition->IsInWorld())
			return fixed::FromInt(-1);
		return MaxDistanceToPoint(target, cmpPosition->GetPosition2D().X, cmpPosition->GetPosition2D().Y);
	}

	ObstructionSquare target_obstruction;
	CmpPtr<ICmpObstruction> cmpObstructionTarget(GetSimContext(), target);
	if (!cmpObstructionTarget || !cmpObstructionTarget->GetObstructionSquare(target_obstruction))
	{
		CmpPtr<ICmpPosition> cmpPositionTarget(GetSimContext(), target);
		if (!cmpPositionTarget || !cmpPositionTarget->IsInWorld())
			return fixed::FromInt(-1);
		return MaxDistanceToPoint(ent, cmpPositionTarget->GetPosition2D().X, cmpPositionTarget->GetPosition2D().Y);
	}

	return MaxDistanceBetweenShapes(obstruction, target_obstruction);
}

// 计算两个形状间的最小距离
fixed CCmpObstructionManager::DistanceBetweenShapes(const ObstructionSquare& source, const ObstructionSquare& target) const
{
	// 球体-球体碰撞。
	if (source.hh == fixed::Zero() && target.hh == fixed::Zero())
		return (CFixedVector2D(target.x, target.z) - CFixedVector2D(source.x, source.z)).Length() - source.hw - target.hw;

	// 方块-方块碰撞。
	if (source.hh != fixed::Zero() && target.hh != fixed::Zero())
		return Geometry::DistanceSquareToSquare(
			CFixedVector2D(target.x, target.z) - CFixedVector2D(source.x, source.z),
			source.u, source.v, CFixedVector2D(source.hw, source.hh),
			target.u, target.v, CFixedVector2D(target.hw, target.hh));

	// 为了覆盖剩下的两种情况，形状 a 是方形，形状 b 是圆形。
	const ObstructionSquare& a = source.hh == fixed::Zero() ? target : source;
	const ObstructionSquare& b = source.hh == fixed::Zero() ? source : target;
	return Geometry::DistanceToSquare(
		CFixedVector2D(b.x, b.z) - CFixedVector2D(a.x, a.z),
		a.u, a.v, CFixedVector2D(a.hw, a.hh), true) - b.hw;
}

// 计算两个形状间的最大距离
fixed CCmpObstructionManager::MaxDistanceBetweenShapes(const ObstructionSquare& source, const ObstructionSquare& target) const
{
	// 球体-球体碰撞。
	if (source.hh == fixed::Zero() && target.hh == fixed::Zero())
		return (CFixedVector2D(target.x, target.z) - CFixedVector2D(source.x, source.z)).Length() + source.hw + target.hw;

	// 方块-方块碰撞。
	if (source.hh != fixed::Zero() && target.hh != fixed::Zero())
		return Geometry::MaxDistanceSquareToSquare(
			CFixedVector2D(target.x, target.z) - CFixedVector2D(source.x, source.z),
			source.u, source.v, CFixedVector2D(source.hw, source.hh),
			target.u, target.v, CFixedVector2D(target.hw, target.hh));

	// 为了覆盖剩下的两种情况，形状 a 是方形，形状 b 是圆形。
	const ObstructionSquare& a = source.hh == fixed::Zero() ? target : source;
	const ObstructionSquare& b = source.hh == fixed::Zero() ? source : target;
	return Geometry::MaxDistanceToSquare(
		CFixedVector2D(b.x, b.z) - CFixedVector2D(a.x, a.z),
		a.u, a.v, CFixedVector2D(a.hw, a.hh), true) + b.hw;
}

/**
 * IsInRange 函数族，依赖于 DistanceTo 函数族。
 *
 * 如果边到边的距离小于 maxRange，则在范围内。
 * 并且当 opposite 为 true 时，对边到对边的距离大于 minRange；
 * 或者当 opposite 为 false 时，边到边的距离大于 minRange。
 *
 * 对 minRange 使用对边意味着一个单位在建筑的攻击范围内，如果它比
 * clearance-buildingsize 更远，这通常是负数（因此返回true）。
 * 注意：从游戏角度看，这意味着单位可以轻松攻击建筑，这是好的，
 * 但也意味着建筑可以轻松攻击单位。建筑通常应该从边缘而不是对边开火，
 * 所以这看起来很奇怪。因此，可以选择将 opposite 设置为 false，使用边到边的距离。
 *
 * 我们不使用平方距离，因为它们很可能溢出。
 * TODO 避免溢出并改用平方距离。
 * 我们使用 0.0001 的容差来避免舍入误差。
 */
bool CCmpObstructionManager::IsInPointRange(entity_id_t ent, entity_pos_t px, entity_pos_t pz, entity_pos_t minRange, entity_pos_t maxRange, bool opposite) const
{
	fixed dist = DistanceToPoint(ent, px, pz);
	return maxRange != NEVER_IN_RANGE && dist != fixed::FromInt(-1) &&
		(dist <= (maxRange + fixed::FromFloat(0.0001f)) || maxRange == ALWAYS_IN_RANGE) &&
		(opposite ? MaxDistanceToPoint(ent, px, pz) : dist) >= minRange - fixed::FromFloat(0.0001f);
}

bool CCmpObstructionManager::IsInTargetRange(entity_id_t ent, entity_id_t target, entity_pos_t minRange, entity_pos_t maxRange, bool opposite) const
{
	fixed dist = DistanceToTarget(ent, target);
	return maxRange != NEVER_IN_RANGE && dist != fixed::FromInt(-1) &&
		(dist <= (maxRange + fixed::FromFloat(0.0001f)) || maxRange == ALWAYS_IN_RANGE) &&
		(opposite ? MaxDistanceToTarget(ent, target) : dist) >= minRange - fixed::FromFloat(0.0001f);
}

// 判断是否在抛物线攻击范围内
bool CCmpObstructionManager::IsInTargetParabolicRange(entity_id_t ent, entity_id_t target, entity_pos_t minRange, entity_pos_t maxRange, entity_pos_t yOrigin, bool opposite) const
{
	CmpPtr<ICmpRangeManager> cmpRangeManager(GetSystemEntity());
	return IsInTargetRange(ent, target, minRange, cmpRangeManager->GetEffectiveParabolicRange(ent, target, maxRange, yOrigin), opposite);
}

// 判断点是否在点的范围内
bool CCmpObstructionManager::IsPointInPointRange(entity_pos_t x, entity_pos_t z, entity_pos_t px, entity_pos_t pz, entity_pos_t minRange, entity_pos_t maxRange) const
{
	entity_pos_t distance = (CFixedVector2D(x, z) - CFixedVector2D(px, pz)).Length();
	return maxRange != NEVER_IN_RANGE && (distance <= (maxRange + fixed::FromFloat(0.0001f)) || maxRange == ALWAYS_IN_RANGE) &&
		distance >= minRange - fixed::FromFloat(0.0001f);
}

// 判断两个形状是否在指定范围内
bool CCmpObstructionManager::AreShapesInRange(const ObstructionSquare& source, const ObstructionSquare& target, entity_pos_t minRange, entity_pos_t maxRange, bool opposite) const
{
	fixed dist = DistanceBetweenShapes(source, target);
	return maxRange != NEVER_IN_RANGE && dist != fixed::FromInt(-1) &&
		(dist <= (maxRange + fixed::FromFloat(0.0001f)) || maxRange == ALWAYS_IN_RANGE) &&
		(opposite ? MaxDistanceBetweenShapes(source, target) : dist) >= minRange - fixed::FromFloat(0.0001f);
}

// 测试一条线段是否与任何障碍物碰撞
bool CCmpObstructionManager::TestLine(const IObstructionTestFilter& filter, entity_pos_t x0, entity_pos_t z0, entity_pos_t x1, entity_pos_t z1, entity_pos_t r, bool relaxClearanceForUnits) const
{
	// 检查两个端点是否都在世界内（这意味着整条线都在）
	if (!IsInWorld(x0, z0, r) || !IsInWorld(x1, z1, r))
		return true;

	CFixedVector2D posMin(std::min(x0, x1) - r, std::min(z0, z1) - r);
	CFixedVector2D posMax(std::max(x0, x1) + r, std::max(z0, z1) + r);

	// 用于单位-单位碰撞的实际半径。如果 relaxClearanceForUnits，会更小以允许更多重叠。
	entity_pos_t unitUnitRadius = r;
	if (relaxClearanceForUnits)
		unitUnitRadius -= entity_pos_t::FromInt(1) / 2;

	std::vector<entity_id_t> unitShapes;
	m_UnitSubdivision.GetInRange(unitShapes, posMin, posMax);
	for (const entity_id_t& shape : unitShapes)
	{
		std::map<u32, UnitShape>::const_iterator it = m_UnitShapes.find(shape);
		ENSURE(it != m_UnitShapes.end());

		if (!filter.TestShape(UNIT_INDEX_TO_TAG(it->first), it->second.flags, it->second.group, INVALID_ENTITY))
			continue;

		CFixedVector2D center(it->second.x, it->second.z);
		CFixedVector2D halfSize(it->second.clearance + unitUnitRadius, it->second.clearance + unitUnitRadius);
		if (Geometry::TestRayAASquare(CFixedVector2D(x0, z0) - center, CFixedVector2D(x1, z1) - center, halfSize))
			return true;
	}

	std::vector<entity_id_t> staticShapes;
	m_StaticSubdivision.GetInRange(staticShapes, posMin, posMax);
	for (const entity_id_t& shape : staticShapes)
	{
		std::map<u32, StaticShape>::const_iterator it = m_StaticShapes.find(shape);
		ENSURE(it != m_StaticShapes.end());

		if (!filter.TestShape(STATIC_INDEX_TO_TAG(it->first), it->second.flags, it->second.group, it->second.group2))
			continue;

		CFixedVector2D center(it->second.x, it->second.z);
		CFixedVector2D halfSize(it->second.hw + r, it->second.hh + r);
		if (Geometry::TestRaySquare(CFixedVector2D(x0, z0) - center, CFixedVector2D(x1, z1) - center, it->second.u, it->second.v, halfSize))
			return true;
	}

	return false;
}

// 测试一条线段是否与任何单位障碍物碰撞
bool CCmpObstructionManager::TestUnitLine(const IObstructionTestFilter& filter, entity_pos_t x0, entity_pos_t z0, entity_pos_t x1, entity_pos_t z1, entity_pos_t r, bool relaxClearanceForUnits) const
{
	// 检查两个端点是否都在世界内
	if (!IsInWorld(x0, z0, r) || !IsInWorld(x1, z1, r))
		return true;

	CFixedVector2D posMin(std::min(x0, x1) - r, std::min(z0, z1) - r);
	CFixedVector2D posMax(std::max(x0, x1) + r, std::max(z0, z1) + r);

	entity_pos_t unitUnitRadius = r;
	if (relaxClearanceForUnits)
		unitUnitRadius -= entity_pos_t::FromInt(1) / 2;

	std::vector<entity_id_t> unitShapes;
	m_UnitSubdivision.GetInRange(unitShapes, posMin, posMax);
	for (const entity_id_t& shape : unitShapes)
	{
		std::map<u32, UnitShape>::const_iterator it = m_UnitShapes.find(shape);
		ENSURE(it != m_UnitShapes.end());

		if (!filter.TestShape(UNIT_INDEX_TO_TAG(it->first), it->second.flags, it->second.group, INVALID_ENTITY))
			continue;

		CFixedVector2D center(it->second.x, it->second.z);
		CFixedVector2D halfSize(it->second.clearance + unitUnitRadius, it->second.clearance + unitUnitRadius);
		if (Geometry::TestRayAASquare(CFixedVector2D(x0, z0) - center, CFixedVector2D(x1, z1) - center, halfSize))
			return true;
	}
	return false;
}

// 测试一个静态形状是否与任何障碍物碰撞
bool CCmpObstructionManager::TestStaticShape(const IObstructionTestFilter& filter,
	entity_pos_t x, entity_pos_t z, entity_pos_t a, entity_pos_t w, entity_pos_t h,
	std::vector<entity_id_t>* out) const
{
	PROFILE("TestStaticShape");

	if (out)
		out->clear();

	fixed s, c;
	sincos_approx(a, s, c);
	CFixedVector2D u(c, -s);
	CFixedVector2D v(s, c);
	CFixedVector2D center(x, z);
	CFixedVector2D halfSize(w / 2, h / 2);
	CFixedVector2D corner1 = u.Multiply(halfSize.X) + v.Multiply(halfSize.Y);
	CFixedVector2D corner2 = u.Multiply(halfSize.X) - v.Multiply(halfSize.Y);

	// 检查所有角点是否都在世界内
	if (!IsInWorld(center + corner1) || !IsInWorld(center + corner2) ||
		!IsInWorld(center - corner1) || !IsInWorld(center - corner2))
	{
		if (out)
			out->push_back(INVALID_ENTITY); // 没有实体ID，所以只推入一个任意标记
		else
			return true;
	}

	// 计算包围盒并获取附近的障碍物
	fixed bbHalfWidth = std::max(corner1.X.Absolute(), corner2.X.Absolute());
	fixed bbHalfHeight = std::max(corner1.Y.Absolute(), corner2.Y.Absolute());
	CFixedVector2D posMin(x - bbHalfWidth, z - bbHalfHeight);
	CFixedVector2D posMax(x + bbHalfWidth, z + bbHalfHeight);

	std::vector<entity_id_t> unitShapes;
	m_UnitSubdivision.GetInRange(unitShapes, posMin, posMax);
	for (entity_id_t& shape : unitShapes)
	{
		std::map<u32, UnitShape>::const_iterator it = m_UnitShapes.find(shape);
		ENSURE(it != m_UnitShapes.end());

		if (!filter.TestShape(UNIT_INDEX_TO_TAG(it->first), it->second.flags, it->second.group, INVALID_ENTITY))
			continue;

		CFixedVector2D center1(it->second.x, it->second.z);

		if (Geometry::PointIsInSquare(center1 - center, u, v, CFixedVector2D(halfSize.X + it->second.clearance, halfSize.Y + it->second.clearance)))
		{
			if (out)
				out->push_back(it->second.entity);
			else
				return true;
		}
	}

	std::vector<entity_id_t> staticShapes;
	m_StaticSubdivision.GetInRange(staticShapes, posMin, posMax);
	for (entity_id_t& shape : staticShapes)
	{
		std::map<u32, StaticShape>::const_iterator it = m_StaticShapes.find(shape);
		ENSURE(it != m_StaticShapes.end());

		if (!filter.TestShape(STATIC_INDEX_TO_TAG(it->first), it->second.flags, it->second.group, it->second.group2))
			continue;

		CFixedVector2D center1(it->second.x, it->second.z);
		CFixedVector2D halfSize1(it->second.hw, it->second.hh);
		if (Geometry::TestSquareSquare(center, u, v, halfSize, center1, it->second.u, it->second.v, halfSize1))
		{
			if (out)
				out->push_back(it->second.entity);
			else
				return true;
		}
	}

	if (out)
		return !out->empty(); // 如果列表不为空，则发生碰撞
	else
		return false; // 如果到这里，说明没有碰撞
}

// 测试一个单位形状是否与任何障碍物碰撞
bool CCmpObstructionManager::TestUnitShape(const IObstructionTestFilter& filter,
	entity_pos_t x, entity_pos_t z, entity_pos_t clearance,
	std::vector<entity_id_t>* out) const
{
	PROFILE("TestUnitShape");

	// 检查形状是否在世界内
	if (!IsInWorld(x, z, clearance))
	{
		if (out)
			out->push_back(INVALID_ENTITY);
		else
			return true;
	}

	CFixedVector2D center(x, z);
	CFixedVector2D posMin(x - clearance, z - clearance);
	CFixedVector2D posMax(x + clearance, z + clearance);

	// 与单位碰撞
	std::vector<entity_id_t> unitShapes;
	m_UnitSubdivision.GetInRange(unitShapes, posMin, posMax);
	for (const entity_id_t& shape : unitShapes)
	{
		std::map<u32, UnitShape>::const_iterator it = m_UnitShapes.find(shape);
		ENSURE(it != m_UnitShapes.end());

		if (!filter.TestShape(UNIT_INDEX_TO_TAG(it->first), it->second.flags, it->second.group, INVALID_ENTITY))
			continue;

		entity_pos_t c1 = it->second.clearance;

		if (!(
			it->second.x + c1 < x - clearance ||
			it->second.x - c1 > x + clearance ||
			it->second.z + c1 < z - clearance ||
			it->second.z - c1 > z + clearance))
		{
			if (out)
				out->push_back(it->second.entity);
			else
				return true;
		}
	}

	// 与静态物体碰撞
	std::vector<entity_id_t> staticShapes;
	m_StaticSubdivision.GetInRange(staticShapes, posMin, posMax);
	for (const entity_id_t& shape : staticShapes)
	{
		std::map<u32, StaticShape>::const_iterator it = m_StaticShapes.find(shape);
		ENSURE(it != m_StaticShapes.end());

		if (!filter.TestShape(STATIC_INDEX_TO_TAG(it->first), it->second.flags, it->second.group, it->second.group2))
			continue;

		CFixedVector2D center1(it->second.x, it->second.z);
		if (Geometry::PointIsInSquare(center1 - center, it->second.u, it->second.v, CFixedVector2D(it->second.hw + clearance, it->second.hh + clearance)))
		{
			if (out)
				out->push_back(it->second.entity);
			else
				return true;
		}
	}

	if (out)
		return !out->empty();
	else
		return false;
}

// 将障碍物栅格化到寻路网格上
void CCmpObstructionManager::Rasterize(Grid<NavcellData>& grid, const std::vector<PathfinderPassability>& passClasses, bool fullUpdate)
{
	PROFILE3("Rasterize Obstructions");

	// 只有当整个单元格严格位于形状内部时，才将其标记为阻塞。
	// (这确保了形状的几何边界始终是可达的。)

	// 通行性类别将根据其 Obstruction 值来栅格化形状。
	// 值不是 "pathfinding" 的类别不应使用 Clearance。

	// 分类不同的通行性类别
	std::map<entity_pos_t, u16> pathfindingMasks;
	u16 foundationMask = 0;
	for (const PathfinderPassability& passability : passClasses)
	{
		switch (passability.m_Obstructions)
		{
		case PathfinderPassability::PATHFINDING:
		{
			std::map<entity_pos_t, u16>::iterator it = pathfindingMasks.find(passability.m_Clearance);
			if (it == pathfindingMasks.end())
				pathfindingMasks[passability.m_Clearance] = passability.m_Mask;
			else
				it->second |= passability.m_Mask;
			break;
		}
		case PathfinderPassability::FOUNDATION:
			foundationMask |= passability.m_Mask;
			break;
		default:
			continue;
		}
	}

	// MakeDirty* 函数只考虑 FLAG_BLOCK_PATHFINDING 和 FLAG_BLOCK_FOUNDATION 标志，
	// 所以它们应该是唯一借助 m_Dirty*Shapes 向量进行栅格化的标志。

	// 对每种寻路径大小进行栅格化
	for (auto& maskPair : pathfindingMasks)
		RasterizeHelper(grid, FLAG_BLOCK_PATHFINDING, fullUpdate, maskPair.second, maskPair.first);

	// 对地基进行栅格化
	RasterizeHelper(grid, FLAG_BLOCK_FOUNDATION, fullUpdate, foundationMask);

	m_DirtyStaticShapes.clear();
	m_DirtyUnitShapes.clear();
}

// 栅格化辅助函数
void CCmpObstructionManager::RasterizeHelper(Grid<NavcellData>& grid, ICmpObstructionManager::flags_t requireMask, bool fullUpdate, pass_class_t appliedMask, entity_pos_t clearance) const
{
	for (auto& pair : m_StaticShapes)
	{
		const StaticShape& shape = pair.second;
		if (!(shape.flags & requireMask))
			continue;

		// 如果不是完全更新，则只处理脏的形状
		if (!fullUpdate && std::find(m_DirtyStaticShapes.begin(), m_DirtyStaticShapes.end(), pair.first) == m_DirtyStaticShapes.end())
			continue;

		// TODO: 对于大的 'expand' 值，使用圆角进行栅格化可能会更好。
		ObstructionSquare square = { shape.x, shape.z, shape.u, shape.v, shape.hw, shape.hh };
		SimRasterize::Spans spans;
		SimRasterize::RasterizeRectWithClearance(spans, square, clearance, Pathfinding::NAVCELL_SIZE);
		for (SimRasterize::Span& span : spans)
		{
			i16 j = Clamp(span.j, (i16)0, (i16)(grid.m_H - 1));
			i16 i0 = std::max(span.i0, (i16)0);
			i16 i1 = std::min(span.i1, (i16)grid.m_W);

			for (i16 i = i0; i < i1; ++i)
				grid.set(i, j, grid.get(i, j) | appliedMask);
		}
	}

	for (auto& pair : m_UnitShapes)
	{
		if (!(pair.second.flags & requireMask))
			continue;

		if (!fullUpdate && std::find(m_DirtyUnitShapes.begin(), m_DirtyUnitShapes.end(), pair.first) == m_DirtyUnitShapes.end())
			continue;

		CFixedVector2D center(pair.second.x, pair.second.z);
		entity_pos_t r = pair.second.clearance + clearance;

		u16 i0, j0, i1, j1;
		Pathfinding::NearestNavcell(center.X - r, center.Y - r, i0, j0, grid.m_W, grid.m_H);
		Pathfinding::NearestNavcell(center.X + r, center.Y + r, i1, j1, grid.m_W, grid.m_H);
		for (u16 j = j0 + 1; j < j1; ++j)
			for (u16 i = i0 + 1; i < i1; ++i)
				grid.set(i, j, grid.get(i, j) | appliedMask);
	}
}

// 获取范围内的所有障碍物
void CCmpObstructionManager::GetObstructionsInRange(const IObstructionTestFilter& filter, entity_pos_t x0, entity_pos_t z0, entity_pos_t x1, entity_pos_t z1, std::vector<ObstructionSquare>& squares) const
{
	GetUnitObstructionsInRange(filter, x0, z0, x1, z1, squares);
	GetStaticObstructionsInRange(filter, x0, z0, x1, z1, squares);
}

// 获取范围内的单位障碍物
void CCmpObstructionManager::GetUnitObstructionsInRange(const IObstructionTestFilter& filter, entity_pos_t x0, entity_pos_t z0, entity_pos_t x1, entity_pos_t z1, std::vector<ObstructionSquare>& squares) const
{
	PROFILE("GetObstructionsInRange");

	ENSURE(x0 <= x1 && z0 <= z1);

	std::vector<entity_id_t> unitShapes;
	m_UnitSubdivision.GetInRange(unitShapes, CFixedVector2D(x0, z0), CFixedVector2D(x1, z1));
	for (entity_id_t& unitShape : unitShapes)
	{
		std::map<u32, UnitShape>::const_iterator it = m_UnitShapes.find(unitShape);
		ENSURE(it != m_UnitShapes.end());

		if (!filter.TestShape(UNIT_INDEX_TO_TAG(it->first), it->second.flags, it->second.group, INVALID_ENTITY))
			continue;

		entity_pos_t c = it->second.clearance;

		// 如果该对象完全在请求范围之外，则跳过
		if (it->second.x + c < x0 || it->second.x - c > x1 || it->second.z + c < z0 || it->second.z - c > z1)
			continue;

		CFixedVector2D u(entity_pos_t::FromInt(1), entity_pos_t::Zero());
		CFixedVector2D v(entity_pos_t::Zero(), entity_pos_t::FromInt(1));
		squares.emplace_back(ObstructionSquare{ it->second.x, it->second.z, u, v, c, c });
	}
}

// 获取范围内的静态障碍物
void CCmpObstructionManager::GetStaticObstructionsInRange(const IObstructionTestFilter& filter, entity_pos_t x0, entity_pos_t z0, entity_pos_t x1, entity_pos_t z1, std::vector<ObstructionSquare>& squares) const
{
	PROFILE("GetObstructionsInRange");

	ENSURE(x0 <= x1 && z0 <= z1);

	std::vector<entity_id_t> staticShapes;
	m_StaticSubdivision.GetInRange(staticShapes, CFixedVector2D(x0, z0), CFixedVector2D(x1, z1));
	for (entity_id_t& staticShape : staticShapes)
	{
		std::map<u32, StaticShape>::const_iterator it = m_StaticShapes.find(staticShape);
		ENSURE(it != m_StaticShapes.end());

		if (!filter.TestShape(STATIC_INDEX_TO_TAG(it->first), it->second.flags, it->second.group, it->second.group2))
			continue;

		entity_pos_t r = it->second.hw + it->second.hh; // 过高估计边缘到中心的距离

		// 如果其过高估计的包围盒完全在请求范围之外，则跳过
		if (it->second.x + r < x0 || it->second.x - r > x1 || it->second.z + r < z0 || it->second.z - r > z1)
			continue;

		// TODO: 也许我们应该使用 Geometry::GetHalfBoundingBox 来更精确？

		squares.emplace_back(ObstructionSquare{ it->second.x, it->second.z, it->second.u, it->second.v, it->second.hw, it->second.hh });
	}
}

// 获取与指定障碍物方块重叠的单位
void CCmpObstructionManager::GetUnitsOnObstruction(const ObstructionSquare& square, std::vector<entity_id_t>& out, const IObstructionTestFilter& filter, bool strict) const
{
	PROFILE("GetUnitsOnObstruction");

	// 为了避免获取到在不可通行单元格上的单位，我们想要找到所有
	// 受到建筑形状的 RasterizeRectWithClearance 影响的单位，其单位间隙
	// 覆盖了该单位所在的寻路单元格。

	std::vector<entity_id_t> unitShapes;
	CFixedVector2D center(square.x, square.z);
	CFixedVector2D expandedBox =
		Geometry::GetHalfBoundingBox(square.u, square.v, CFixedVector2D(square.hw, square.hh)) +
		CFixedVector2D(m_MaxClearance, m_MaxClearance);
	m_UnitSubdivision.GetInRange(unitShapes, center - expandedBox, center + expandedBox);

	std::map<entity_pos_t, SimRasterize::Spans> rasterizedRects;

	for (const u32& unitShape : unitShapes)
	{
		std::map<u32, UnitShape>::const_iterator it = m_UnitShapes.find(unitShape);
		ENSURE(it != m_UnitShapes.end());

		const UnitShape& shape = it->second;

		if (!filter.TestShape(UNIT_INDEX_TO_TAG(unitShape), shape.flags, shape.group, INVALID_ENTITY))
			continue;

		if (rasterizedRects.find(shape.clearance) == rasterizedRects.end())
		{
			// 栅格化是真实形状的近似。
			// 根据您的用途，您可能希望栅格化更严格或更宽松，
			// 即这可能返回一些实际上不在形状上的单位（如果设置了strict）
			// 或者可能不返回一些在形状上的单位（如果未设置strict）。
			// 地基需要非严格，否则有时会检测到建造者单位
			// 在形状上，从而命令他们离开。
			SimRasterize::Spans& newSpans = rasterizedRects[shape.clearance];
			if (strict)
				SimRasterize::RasterizeRectWithClearance(newSpans, square, shape.clearance, Pathfinding::NAVCELL_SIZE);
			else
				SimRasterize::RasterizeRectWithClearance(newSpans, square, shape.clearance - Pathfinding::CLEARANCE_EXTENSION_RADIUS, Pathfinding::NAVCELL_SIZE);
		}

		SimRasterize::Spans& spans = rasterizedRects[shape.clearance];

		// 检查单位中心所在的寻路单元格是否在任何span中
		u16 i = (shape.x / Pathfinding::NAVCELL_SIZE).ToInt_RoundToNegInfinity();
		u16 j = (shape.z / Pathfinding::NAVCELL_SIZE).ToInt_RoundToNegInfinity();

		for (const SimRasterize::Span& span : spans)
		{
			if (j == span.j && span.i0 <= i && i < span.i1)
			{
				out.push_back(shape.entity);
				break;
			}
		}
	}
}

// 获取与指定障碍物方块重叠的静态障碍物
void CCmpObstructionManager::GetStaticObstructionsOnObstruction(const ObstructionSquare& square, std::vector<entity_id_t>& out, const IObstructionTestFilter& filter) const
{
	PROFILE("GetStaticObstructionsOnObstruction");

	std::vector<entity_id_t> staticShapes;
	CFixedVector2D center(square.x, square.z);
	CFixedVector2D expandedBox = Geometry::GetHalfBoundingBox(square.u, square.v, CFixedVector2D(square.hw, square.hh));
	m_StaticSubdivision.GetInRange(staticShapes, center - expandedBox, center + expandedBox);

	for (const u32& staticShape : staticShapes)
	{
		std::map<u32, StaticShape>::const_iterator it = m_StaticShapes.find(staticShape);
		ENSURE(it != m_StaticShapes.end());

		const StaticShape& shape = it->second;

		if (!filter.TestShape(STATIC_INDEX_TO_TAG(staticShape), shape.flags, shape.group, shape.group2))
			continue;

		// 精确的方块-方块碰撞检测
		if (Geometry::TestSquareSquare(
			center,
			square.u,
			square.v,
			CFixedVector2D(square.hw, square.hh),
			CFixedVector2D(shape.x, shape.z),
			shape.u,
			shape.v,
			CFixedVector2D(shape.hw, shape.hh)))
		{
			out.push_back(shape.entity);
		}
	}
}

// 提交调试渲染
void CCmpObstructionManager::RenderSubmit(SceneCollector& collector)
{
	if (!m_DebugOverlayEnabled)
		return;

	CColor defaultColor(0, 0, 1, 1);
	CColor movingColor(1, 0, 1, 1);
	CColor boundsColor(1, 1, 0, 1);

	// 如果形状已更改，则重新生成所有叠加层
	if (m_DebugOverlayDirty)
	{
		m_DebugOverlayLines.clear();

		// 绘制世界边界
		m_DebugOverlayLines.push_back(SOverlayLine());
		m_DebugOverlayLines.back().m_Color = boundsColor;
		SimRender::ConstructSquareOnGround(GetSimContext(),
			(m_WorldX0 + m_WorldX1).ToFloat() / 2.f, (m_WorldZ0 + m_WorldZ1).ToFloat() / 2.f,
			(m_WorldX1 - m_WorldX0).ToFloat(), (m_WorldZ1 - m_WorldZ0).ToFloat(),
			0, m_DebugOverlayLines.back(), true);

		// 绘制所有单位形状
		for (std::map<u32, UnitShape>::iterator it = m_UnitShapes.begin(); it != m_UnitShapes.end(); ++it)
		{
			m_DebugOverlayLines.push_back(SOverlayLine());
			m_DebugOverlayLines.back().m_Color = ((it->second.flags & FLAG_MOVING) ? movingColor : defaultColor);
			SimRender::ConstructSquareOnGround(GetSimContext(), it->second.x.ToFloat(), it->second.z.ToFloat(), it->second.clearance.ToFloat(), it->second.clearance.ToFloat(), 0, m_DebugOverlayLines.back(), true);
		}

		// 绘制所有静态形状
		for (std::map<u32, StaticShape>::iterator it = m_StaticShapes.begin(); it != m_StaticShapes.end(); ++it)
		{
			m_DebugOverlayLines.push_back(SOverlayLine());
			m_DebugOverlayLines.back().m_Color = defaultColor;
			float a = atan2f(it->second.v.X.ToFloat(), it->second.v.Y.ToFloat());
			SimRender::ConstructSquareOnGround(GetSimContext(), it->second.x.ToFloat(), it->second.z.ToFloat(), it->second.hw.ToFloat() * 2, it->second.hh.ToFloat() * 2, a, m_DebugOverlayLines.back(), true);
		}

		m_DebugOverlayDirty = false;
	}

	// 提交所有线段进行渲染
	for (size_t i = 0; i < m_DebugOverlayLines.size(); ++i)
		collector.Submit(&m_DebugOverlayLines[i]);
}
