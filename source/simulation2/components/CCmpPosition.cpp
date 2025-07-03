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
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with 0 A.D.  If not, see <http://www.gnu.org/licenses/>.
 */

 // 预编译头文件，用于加速编译过程
#include "precompiled.h"

// 包含组件系统、位置接口等核心定义
#include "simulation2/system/Component.h"
#include "ICmpPosition.h"

// 包含消息类型、序列化类型定义
#include "simulation2/MessageTypes.h"
#include "simulation2/serialization/SerializedTypes.h"

// 包含地形、视觉、水体管理器等相关组件接口
#include "ICmpTerrain.h"
#include "ICmpVisual.h"
#include "ICmpWaterManager.h"

// 包含图形、数学库和日志记录等工具
#include "graphics/Terrain.h"
#include "lib/rand.h"
#include "maths/MathUtil.h"
#include "maths/Matrix3D.h"
#include "maths/Vector3D.h"
#include "maths/Vector2D.h"
#include "ps/CLogger.h"
#include "ps/Profile.h"

/**
 * ICmpPosition 接口的基础实现。
 * 这个组件负责处理实体在游戏世界中的位置、方向和移动相关的所有数据。
 */
class CCmpPosition final : public ICmpPosition
{
public:
	// 静态类初始化函数，在组件管理器中注册消息订阅
	static void ClassInit(CComponentManager& componentManager)
	{
		// 订阅 "回合开始" 消息，用于更新上一回合的位置
		componentManager.SubscribeToMessageType(MT_TurnStart);
		// 订阅 "地形改变" 消息，用于更新高度等
		componentManager.SubscribeToMessageType(MT_TerrainChanged);
		// 订阅 "水体改变" 消息，用于更新浮动单位的高度
		componentManager.SubscribeToMessageType(MT_WaterChanged);
		// 订阅 "反序列化完成" 消息，用于在加载游戏后进行初始化
		componentManager.SubscribeToMessageType(MT_Deserialized);

		// TODO: 如果这个组件成为性能瓶颈，
		// 应当通过创建一个新的 PositionStatic 组件来优化。
		// 该静态组件不订阅消息，也不存储 LastX/LastZ，
		// 应用于所有不会移动的实体。
	}

	// 使用默认的组件分配器宏
	DEFAULT_COMPONENT_ALLOCATOR(Position)

		// --- 模板状态（Template state）: 从实体模板中读取的初始配置 ---

		// 定义实体如何根据地形坡度自动调整姿态
		enum class AnchorType
	{
		UNDEFINED = -1, // 仅用于 m_ActorAnchorType，因为它是可选的。表示未定义。
		UPRIGHT = 0,    // 保持直立（如人类单位）
		PITCH = 1,      // 沿俯仰轴旋转以适应地形（如动物）
		PITCH_ROLL = 2, // 同时沿俯仰和翻滚轴旋转（如四轮车）
		ROLL = 3,       // 沿翻滚轴旋转
	};

	AnchorType m_AnchorType; // 实体根据地形调整姿态的方式

	bool m_Floating;          // 该实体是否能在水上漂浮
	entity_pos_t m_FloatDepth; // 实体漂浮在水中的深度

	// Y轴旋转速度，单位：弧度/秒。用于 InterpolatedRotY（平滑的图形旋转）跟随 RotY（逻辑旋转）和单位运动。
	fixed m_RotYSpeed;

	// --- 动态状态（Dynamic state）: 在游戏运行时会改变的状态 ---

	bool m_InWorld; // 实体当前是否存在于游戏世界中
	// m_LastX/Z 存储最近一个回合开始时的位置
	// m_PrevX/Z 存储上上个回合的位置
	entity_pos_t m_X, m_Z, m_LastX, m_LastZ, m_PrevX, m_PrevZ; // 如果 !InWorld，这些值为未定义的垃圾数据

	entity_pos_t m_Y, m_LastYDifference; // Y坐标，可以是相对高度或绝对高度
	bool m_RelativeToGround; // Y坐标(m_Y)是相对于地形/水面，还是绝对高度

	fixed m_ConstructionProgress; // 建造进度（0.0 到 1.0），影响实体在建造时的视觉高度

	// 当实体是炮塔时，只使用 m_RotY，表示相对于父实体的旋转
	entity_angle_t m_RotX, m_RotY, m_RotZ; // X, Y, Z 轴的逻辑旋转角度

	entity_id_t m_TurretParent; // 炮塔的父实体ID
	CFixedVector3D m_TurretPosition; // 炮塔相对于父实体的位置偏移
	std::set<entity_id_t> m_Turrets; // 该实体拥有的炮塔列表

	// --- 非序列化数据（Not serialized）: 不需要存盘，每次加载后重新计算 ---
	AnchorType m_ActorAnchorType; // 由Actor（模型文件）覆盖的姿态锚定类型
	// 用于图形渲染的平滑插值旋转角度
	float m_InterpolatedRotX, m_InterpolatedRotY, m_InterpolatedRotZ;
	// 上一帧的插值旋转角度，用于计算新的插值
	float m_LastInterpolatedRotX, m_LastInterpolatedRotZ;
	bool m_ActorFloating; // 由Actor（模型文件）覆盖的是否漂浮属性

	bool m_EnabledMessageInterpolate; // 是否已启用 MT_Interpolate 消息订阅，用于优化

	// 定义了此组件在实体模板XML文件中的结构
	static std::string GetSchema()
	{
		return
			"<a:help>允许此实体存在于世界中的一个位置（和方向），并定义定位的一些细节。</a:help>"
			"<a:example>"
			"<Anchor>upright</Anchor>"
			"<Altitude>0.0</Altitude>"
			"<Floating>false</Floating>"
			"<FloatDepth>0.0</FloatDepth>"
			"<TurnRate>6.0</TurnRate>"
			"</a:example>"
			"<element name='Anchor' a:help='自动旋转以适应地形坡度'>"
			"<choice>"
			"<value a:help='始终保持直立（例如：人类）'>upright</value>"
			"<value a:help='前后旋转以适应地形（例如：动物）'>pitch</value>"
			"<value a:help='左右翻滚以适应地形'>roll</value>"
			"<value a:help='全方位旋转以适应地形（例如：四轮车）'>pitch-roll</value>"
			"</choice>"
			"</element>"
			"<element name='Altitude' a:help='距离地面的高度（米）'>"
			"<data type='decimal'/>"
			"</element>"
			"<element name='Floating' a:help='实体是否漂浮在水上'>"
			"<data type='boolean'/>"
			"</element>"
			"<element name='FloatDepth' a:help='实体在水上漂浮的深度（需要Floating为true）'>"
			"<ref name='nonNegativeDecimal'/>"
			"</element>"
			"<element name='TurnRate' a:help='Y轴最大旋转速度，单位为弧度/秒。用于所有图形旋转和一些由unitMotion驱动的实际单位旋转。'>"
			"<ref name='positiveDecimal'/>"
			"</element>";
	}

	// 从模板数据初始化组件
	void Init(const CParamNode& paramNode) override
	{
		m_AnchorType = ParseAnchorString(paramNode.GetChild("Anchor").ToString());
		m_ActorAnchorType = AnchorType::UNDEFINED;

		m_InWorld = false;

		m_LastYDifference = entity_pos_t::Zero();
		m_Y = paramNode.GetChild("Altitude").ToFixed();
		m_RelativeToGround = true;
		m_Floating = paramNode.GetChild("Floating").ToBool();
		m_FloatDepth = paramNode.GetChild("FloatDepth").ToFixed();

		m_RotYSpeed = paramNode.GetChild("TurnRate").ToFixed();

		m_RotX = m_RotY = m_RotZ = entity_angle_t::FromInt(0);
		m_InterpolatedRotX = m_InterpolatedRotY = m_InterpolatedRotZ = 0.f;
		m_LastInterpolatedRotX = m_LastInterpolatedRotZ = 0.f;

		m_TurretParent = INVALID_ENTITY;
		m_TurretPosition = CFixedVector3D();

		m_ActorFloating = false;

		m_EnabledMessageInterpolate = false;
	}

	// 组件销毁时的清理工作
	void Deinit() override
	{
	}

	// 序列化（保存游戏状态）
	void Serialize(ISerializer& serialize) override
	{
		serialize.Bool("in world", m_InWorld);
		if (m_InWorld)
		{
			serialize.NumberFixed_Unbounded("x", m_X);
			serialize.NumberFixed_Unbounded("y", m_Y);
			serialize.NumberFixed_Unbounded("z", m_Z);
			serialize.NumberFixed_Unbounded("last x", m_LastX);
			serialize.NumberFixed_Unbounded("last y diff", m_LastYDifference);
			serialize.NumberFixed_Unbounded("last z", m_LastZ);
		}

		serialize.NumberFixed_Unbounded("rot x", m_RotX);
		serialize.NumberFixed_Unbounded("rot y", m_RotY);
		serialize.NumberFixed_Unbounded("rot z", m_RotZ);
		serialize.NumberFixed_Unbounded("rot y speed", m_RotYSpeed);
		serialize.NumberFixed_Unbounded("altitude", m_Y);
		serialize.Bool("relative", m_RelativeToGround);
		serialize.Bool("floating", m_Floating);
		serialize.NumberFixed_Unbounded("float depth", m_FloatDepth);
		serialize.NumberFixed_Unbounded("constructionprogress", m_ConstructionProgress);

		// 在Debug模式下，额外序列化一些文本信息，方便调试
		if (serialize.IsDebug())
		{
			const char* anchor = "???";
			switch (m_AnchorType)
			{
			case AnchorType::PITCH:
				anchor = "pitch";
				break;

			case AnchorType::PITCH_ROLL:
				anchor = "pitch-roll";
				break;

			case AnchorType::ROLL:
				anchor = "roll";
				break;

			case AnchorType::UPRIGHT: // upright是默认值
			default:
				anchor = "upright";
				break;
			}
			serialize.StringASCII("anchor", anchor, 0, 16);
		}
		serialize.NumberU32_Unbounded("turret parent", m_TurretParent);
		if (m_TurretParent != INVALID_ENTITY)
		{
			serialize.NumberFixed_Unbounded("x", m_TurretPosition.X);
			serialize.NumberFixed_Unbounded("y", m_TurretPosition.Y);
			serialize.NumberFixed_Unbounded("z", m_TurretPosition.Z);
		}
		Serializer(serialize, "turrets", m_Turrets);
	}

	// 反序列化（加载游戏状态）
	void Deserialize(const CParamNode& paramNode, IDeserializer& deserialize) override
	{
		// 首先像新建一样初始化
		Init(paramNode);

		deserialize.Bool("in world", m_InWorld);
		if (m_InWorld)
		{
			deserialize.NumberFixed_Unbounded("x", m_X);
			deserialize.NumberFixed_Unbounded("y", m_Y);
			deserialize.NumberFixed_Unbounded("z", m_Z);
			deserialize.NumberFixed_Unbounded("last x", m_LastX);
			deserialize.NumberFixed_Unbounded("last y diff", m_LastYDifference);
			deserialize.NumberFixed_Unbounded("last z", m_LastZ);
		}

		deserialize.NumberFixed_Unbounded("rot x", m_RotX);
		deserialize.NumberFixed_Unbounded("rot y", m_RotY);
		deserialize.NumberFixed_Unbounded("rot z", m_RotZ);
		deserialize.NumberFixed_Unbounded("rot y speed", m_RotYSpeed);
		deserialize.NumberFixed_Unbounded("altitude", m_Y);
		deserialize.Bool("relative", m_RelativeToGround);
		deserialize.Bool("floating", m_Floating);
		deserialize.NumberFixed_Unbounded("float depth", m_FloatDepth);
		deserialize.NumberFixed_Unbounded("constructionprogress", m_ConstructionProgress);
		// TODO: 是否应该对所有这些值进行范围检查？

		// 将插值的Y轴旋转设置为逻辑值，避免加载后突然旋转
		m_InterpolatedRotY = m_RotY.ToFloat();

		deserialize.NumberU32_Unbounded("turret parent", m_TurretParent);
		if (m_TurretParent != INVALID_ENTITY)
		{
			deserialize.NumberFixed_Unbounded("x", m_TurretPosition.X);
			deserialize.NumberFixed_Unbounded("y", m_TurretPosition.Y);
			deserialize.NumberFixed_Unbounded("z", m_TurretPosition.Z);
		}
		Serializer(deserialize, "turrets", m_Turrets);

		// 如果实体在世界中，更新其XZ轴的旋转以适应地形
		if (m_InWorld)
			UpdateXZRotation();

		// 更新消息订阅状态
		UpdateMessageSubscriptions();
	}

	// 在所有组件都反序列化完成后调用
	void Deserialized()
	{
		// 广播插值位置变化，以确保其他系统（如图形）更新
		AdvertiseInterpolatedPositionChanges();
	}

	// 更新炮塔的位置，使其跟随父实体
	void UpdateTurretPosition() override
	{
		if (m_TurretParent == INVALID_ENTITY)
			return;
		CmpPtr<ICmpPosition> cmpPosition(GetSimContext(), m_TurretParent);
		if (!cmpPosition)
		{
			LOGERROR("炮塔的父实体没有Position组件");
			return;
		}
		if (!cmpPosition->IsInWorld())
			// 如果父实体不在世界中，炮塔也移出世界
			MoveOutOfWorld();
		else
		{
			// 计算炮塔在世界中的绝对位置
			// 1. 获取炮塔相对于父实体的偏移
			CFixedVector2D rotatedPosition = CFixedVector2D(m_TurretPosition.X, m_TurretPosition.Z);
			// 2. 根据父实体的旋转来旋转这个偏移
			rotatedPosition = rotatedPosition.Rotate(cmpPosition->GetRotation().Y);
			// 3. 加上父实体的世界坐标
			CFixedVector2D rootPosition = cmpPosition->GetPosition2D();
			entity_pos_t x = rootPosition.X + rotatedPosition.X;
			entity_pos_t z = rootPosition.Y + rotatedPosition.Y;
			if (!m_InWorld || m_X != x || m_Z != z)
				MoveTo(x, z);
			// 计算Y轴高度
			entity_pos_t y = cmpPosition->GetHeightOffset() + m_TurretPosition.Y;
			if (!m_InWorld || GetHeightOffset() != y)
				SetHeightOffset(y);
			m_InWorld = true;
		}
	}

	// 获取此实体拥有的炮塔列表
	std::set<entity_id_t>* GetTurrets() override
	{
		return &m_Turrets;
	}

	// 设置炮塔的父实体和相对偏移
	void SetTurretParent(entity_id_t id, const CFixedVector3D& offset) override
	{
		// 保存当前的Y轴绝对旋转
		entity_angle_t angle = GetRotation().Y;

		// 如果之前有父实体，从旧父实体的炮塔列表中移除自己
		if (m_TurretParent != INVALID_ENTITY)
		{
			CmpPtr<ICmpPosition> cmpPosition(GetSimContext(), m_TurretParent);
			if (cmpPosition)
				cmpPosition->GetTurrets()->erase(GetEntityId());
		}

		// 设置新的父实体和偏移
		m_TurretParent = id;
		m_TurretPosition = offset;

		// 将自己添加到新父实体的炮塔列表中
		if (m_TurretParent != INVALID_ENTITY)
		{
			CmpPtr<ICmpPosition> cmpPosition(GetSimContext(), m_TurretParent);
			if (cmpPosition)
				cmpPosition->GetTurrets()->insert(GetEntityId());
		}

		// 恢复Y轴旋转，因为现在是相对旋转
		SetYRotation(angle);
		// 更新自己的位置
		UpdateTurretPosition();
	}

	// 获取炮塔的父实体ID
	entity_id_t GetTurretParent() const override
	{
		return m_TurretParent;
	}

	// 检查实体是否在游戏世界中
	bool IsInWorld() const override
	{
		return m_InWorld;
	}

	// 将实体移出游戏世界
	void MoveOutOfWorld() override
	{
		m_InWorld = false;

		// 广播位置变化消息
		AdvertisePositionChanges();
		AdvertiseInterpolatedPositionChanges();
	}

	// 移动到指定的XZ坐标
	void MoveTo(entity_pos_t x, entity_pos_t z) override
	{
		m_X = x;
		m_Z = z;

		// 如果之前不在世界中，则进行初始化
		if (!m_InWorld)
		{
			m_InWorld = true;
			m_LastX = m_PrevX = m_X;
			m_LastZ = m_PrevZ = m_Z;
			m_LastYDifference = entity_pos_t::Zero();
		}

		// 广播位置变化消息
		AdvertisePositionChanges();
		AdvertiseInterpolatedPositionChanges();
	}

	// 移动到指定坐标并转向指定方向
	void MoveAndTurnTo(entity_pos_t x, entity_pos_t z, entity_angle_t ry) override
	{
		m_X = x;
		m_Z = z;

		if (!m_InWorld)
		{
			m_InWorld = true;
			m_LastX = m_PrevX = m_X;
			m_LastZ = m_PrevZ = m_Z;
			m_LastYDifference = entity_pos_t::Zero();
		}

		// TurnTo 会广播位置变化消息
		TurnTo(ry);

		AdvertiseInterpolatedPositionChanges();
	}

	// "跳跃"到指定位置，立即更新所有位置历史记录，用于传送等
	void JumpTo(entity_pos_t x, entity_pos_t z) override
	{
		m_LastX = m_PrevX = m_X = x;
		m_LastZ = m_PrevZ = m_Z = z;
		m_InWorld = true;

		UpdateXZRotation();

		m_LastInterpolatedRotX = m_InterpolatedRotX;
		m_LastInterpolatedRotZ = m_InterpolatedRotZ;

		AdvertisePositionChanges();
		AdvertiseInterpolatedPositionChanges();
	}

	// 设置相对于地面的高度偏移
	void SetHeightOffset(entity_pos_t dy) override
	{
		// 计算新旧偏移量的差值，并更新Y坐标
		m_LastYDifference = dy - GetHeightOffset();
		m_Y += m_LastYDifference;
		AdvertiseInterpolatedPositionChanges();
	}

	// 获取相对于地面的高度偏移
	entity_pos_t GetHeightOffset() const override
	{
		if (m_RelativeToGround)
			return m_Y;

		// 如果不是相对地面，高度偏移 = m_Y - 地面高度
		// 如果漂浮，高度偏移 = m_Y - 水面高度 + 漂浮深度
		entity_pos_t baseY;
		CmpPtr<ICmpTerrain> cmpTerrain(GetSystemEntity());
		if (cmpTerrain)
			baseY = cmpTerrain->GetGroundLevel(m_X, m_Z);

		if (m_Floating)
		{
			CmpPtr<ICmpWaterManager> cmpWaterManager(GetSystemEntity());
			if (cmpWaterManager)
				baseY = std::max(baseY, cmpWaterManager->GetWaterLevel(m_X, m_Z) - m_FloatDepth);
		}
		return m_Y - baseY;
	}

	// 设置绝对世界高度
	void SetHeightFixed(entity_pos_t y) override
	{
		// 计算新旧绝对高度的差值，并更新Y坐标
		m_LastYDifference = y - GetHeightFixed();
		m_Y += m_LastYDifference;
		AdvertiseInterpolatedPositionChanges();
	}

	// 获取绝对世界高度
	entity_pos_t GetHeightFixed() const override
	{
		return GetHeightAtFixed(m_X, m_Z);
	}

	// 获取在指定XZ坐标处的绝对世界高度
	entity_pos_t GetHeightAtFixed(entity_pos_t x, entity_pos_t z) const override
	{
		if (!m_RelativeToGround)
			return m_Y;

		// 如果是相对地面，绝对高度 = 地面高度 + m_Y
		// 如果漂浮，绝对高度 = max(地面高度, 水面 - 漂浮深度) + m_Y
		entity_pos_t baseY;
		CmpPtr<ICmpTerrain> cmpTerrain(GetSystemEntity());
		if (cmpTerrain)
			baseY = cmpTerrain->GetGroundLevel(x, z);

		if (m_Floating)
		{
			CmpPtr<ICmpWaterManager> cmpWaterManager(GetSystemEntity());
			if (cmpWaterManager)
				baseY = std::max(baseY, cmpWaterManager->GetWaterLevel(x, z) - m_FloatDepth);
		}
		return m_Y + baseY;
	}

	// 检查高度是否是相对地面
	bool IsHeightRelative() const override
	{
		return m_RelativeToGround;
	}

	// 设置高度是相对地面还是绝对高度
	void SetHeightRelative(bool relative) override
	{
		// 转换m_Y以保持实体在世界中的实际高度不变
		m_Y = relative ? GetHeightOffset() : GetHeightFixed();
		m_RelativeToGround = relative;
		m_LastYDifference = entity_pos_t::Zero();
		AdvertiseInterpolatedPositionChanges();
	}

	// 是否能漂浮
	bool CanFloat() const override
	{
		return m_Floating;
	}

	// 设置是否漂浮
	void SetFloating(bool flag) override
	{
		m_Floating = flag;
		AdvertiseInterpolatedPositionChanges();
	}

	// 设置是否由Actor（模型）定义为漂浮
	void SetActorFloating(bool flag) override
	{
		m_ActorFloating = flag;
		AdvertiseInterpolatedPositionChanges();
	}

	// 设置由Actor（模型）定义的姿态锚定方式
	void SetActorAnchor(const CStr& anchor) override
	{
		m_ActorAnchorType = ParseAnchorString(anchor);
	}

	// 设置建造进度（影响视觉高度）
	void SetConstructionProgress(fixed progress) override
	{
		m_ConstructionProgress = progress;
		AdvertiseInterpolatedPositionChanges();
	}

	// 获取3D世界坐标
	CFixedVector3D GetPosition() const override
	{
		if (!m_InWorld)
		{
			LOGERROR("当实体不在世界中时调用了 CCmpPosition::GetPosition");
			return CFixedVector3D();
		}

		return CFixedVector3D(m_X, GetHeightFixed(), m_Z);
	}

	// 获取2D世界坐标 (XZ平面)
	CFixedVector2D GetPosition2D() const override
	{
		if (!m_InWorld)
		{
			LOGERROR("当实体不在世界中时调用了 CCmpPosition::GetPosition2D");
			return CFixedVector2D();
		}

		return CFixedVector2D(m_X, m_Z);
	}

	// 获取上一回合的3D世界坐标
	CFixedVector3D GetPreviousPosition() const override
	{
		if (!m_InWorld)
		{
			LOGERROR("当实体不在世界中时调用了 CCmpPosition::GetPreviousPosition");
			return CFixedVector3D();
		}

		return CFixedVector3D(m_PrevX, GetHeightAtFixed(m_PrevX, m_PrevZ), m_PrevZ);
	}

	// 获取上一回合的2D世界坐标
	CFixedVector2D GetPreviousPosition2D() const override
	{
		if (!m_InWorld)
		{
			LOGERROR("当实体不在世界中时调用了 CCmpPosition::GetPreviousPosition2D");
			return CFixedVector2D();
		}

		return CFixedVector2D(m_PrevX, m_PrevZ);
	}

	// 获取转向速率
	fixed GetTurnRate() const override
	{
		return m_RotYSpeed;
	}

	// 转向至指定的Y轴角度 (逻辑转向)
	void TurnTo(entity_angle_t y) override
	{
		// 如果是炮塔，传入的y是绝对角度，需转换为相对父实体的角度
		if (m_TurretParent != INVALID_ENTITY)
		{
			CmpPtr<ICmpPosition> cmpPosition(GetSimContext(), m_TurretParent);
			if (cmpPosition)
				y -= cmpPosition->GetRotation().Y;
		}

		m_RotY = y;

		AdvertisePositionChanges();
		UpdateMessageSubscriptions();
	}

	// 立即设置Y轴旋转角度 (逻辑和图形)
	void SetYRotation(entity_angle_t y) override
	{
		// 如果是炮塔，转换为相对角度
		if (m_TurretParent != INVALID_ENTITY)
		{
			CmpPtr<ICmpPosition> cmpPosition(GetSimContext(), m_TurretParent);
			if (cmpPosition)
				y -= cmpPosition->GetRotation().Y;
		}
		m_RotY = y;
		m_InterpolatedRotY = m_RotY.ToFloat(); // 图形旋转也立即更新

		if (m_InWorld)
		{
			UpdateXZRotation();

			m_LastInterpolatedRotX = m_InterpolatedRotX;
			m_LastInterpolatedRotZ = m_InterpolatedRotZ;
		}

		AdvertisePositionChanges();
		UpdateMessageSubscriptions();
	}

	// 设置XZ轴的旋转角度 (通常用于特殊情况，如翻滚的单位)
	void SetXZRotation(entity_angle_t x, entity_angle_t z) override
	{
		m_RotX = x;
		m_RotZ = z;

		if (m_InWorld)
		{
			UpdateXZRotation();

			m_LastInterpolatedRotX = m_InterpolatedRotX;
			m_LastInterpolatedRotZ = m_InterpolatedRotZ;
		}
	}

	// 获取实体的旋转 (X, Y, Z)
	CFixedVector3D GetRotation() const override
	{
		entity_angle_t y = m_RotY;
		// 如果是炮塔，Y轴旋转需要加上父实体的旋转角度，得到绝对角度
		if (m_TurretParent != INVALID_ENTITY)
		{
			CmpPtr<ICmpPosition> cmpPosition(GetSimContext(), m_TurretParent);
			if (cmpPosition)
				y += cmpPosition->GetRotation().Y;
		}
		return CFixedVector3D(m_RotX, y, m_RotZ);
	}

	// 获取上一回合内移动的距离
	fixed GetDistanceTravelled() const override
	{
		if (!m_InWorld)
		{
			LOGERROR("当实体不在世界中时调用了 CCmpPosition::GetDistanceTravelled");
			return fixed::Zero();
		}

		return CFixedVector2D(m_X - m_LastX, m_Z - m_LastZ).Length();
	}

	// 根据建造进度计算视觉上的高度偏移（使建筑从地下“长”出来）
	float GetConstructionProgressOffset(const CVector3D& pos) const
	{
		// 如果建造完成，无偏移
		if (m_ConstructionProgress.IsZero())
			return 0.0f;

		CmpPtr<ICmpVisual> cmpVisual(GetEntityHandle());
		if (!cmpVisual)
			return 0.0f;

		// 使用选择框计算模型尺寸，因为模型本身可能有偏移
		// TODO: 这个方法会烦人地显示贴花，最好能隐藏它们
		CBoundingBoxOriented bounds = cmpVisual->GetSelectionBox();
		if (bounds.IsEmpty())
			return 0.0f;

		// 模型总高度
		float dy = 2.0f * bounds.m_HalfSizes.Y;

		// 如果是漂浮单位，希望它从地形下方开始建造，
		// 所以计算当前位置和地形之间的高度差。
		CmpPtr<ICmpTerrain> cmpTerrain(GetSystemEntity());
		if (cmpTerrain && (m_Floating || m_ActorFloating))
		{
			float ground = cmpTerrain->GetExactGroundLevel(pos.X, pos.Z);
			dy += std::max(0.f, pos.Y - ground);
		}

		// 根据进度计算向下的偏移量
		return (m_ConstructionProgress.ToFloat() - 1.0f) * dy;
	}

	// 获取用于图形渲染的插值后2D位置和Y轴旋转
	void GetInterpolatedPosition2D(float frameOffset, float& x, float& z, float& rotY) const override
	{
		if (!m_InWorld)
		{
			LOGERROR("当实体不在世界中时调用了 CCmpPosition::GetInterpolatedPosition2D");
			return;
		}

		// 在上一回合位置和当前位置之间进行线性插值
		x = Interpolate(m_LastX.ToFloat(), m_X.ToFloat(), frameOffset);
		z = Interpolate(m_LastZ.ToFloat(), m_Z.ToFloat(), frameOffset);

		// 返回平滑插值的Y轴旋转
		rotY = m_InterpolatedRotY;
	}

	// 获取用于图形渲染的完整插值变换矩阵
	CMatrix3D GetInterpolatedTransform(float frameOffset) const override
	{
		// 如果是炮塔，其变换矩阵依赖于父实体
		if (m_TurretParent != INVALID_ENTITY)
		{
			CmpPtr<ICmpPosition> cmpPosition(GetSimContext(), m_TurretParent);
			if (!cmpPosition)
			{
				LOGERROR("炮塔的父实体没有Position组件");
				CMatrix3D m;
				m.SetIdentity();
				return m;
			}
			if (!cmpPosition->IsInWorld())
			{
				LOGERROR("当炮塔实体不在世界中时调用了 CCmpPosition::GetInterpolatedTransform");
				CMatrix3D m;
				m.SetIdentity();
				return m;
			}
			else
			{
				// 先获取父实体的变换矩阵
				CMatrix3D parentTransformMatrix = cmpPosition->GetInterpolatedTransform(frameOffset);
				// 计算自己的相对变换矩阵
				CMatrix3D ownTransformation = CMatrix3D();
				ownTransformation.SetYRotation(m_InterpolatedRotY);
				ownTransformation.Translate(-m_TurretPosition.X.ToFloat(), m_TurretPosition.Y.ToFloat(), -m_TurretPosition.Z.ToFloat());
				// 最终矩阵 = 父实体矩阵 * 自身相对矩阵
				return parentTransformMatrix * ownTransformation;
			}
		}

		if (!m_InWorld)
		{
			LOGERROR("当实体不在世界中时调用了 CCmpPosition::GetInterpolatedTransform");
			CMatrix3D m;
			m.SetIdentity();
			return m;
		}

		// 获取插值后的2D位置和旋转
		float x{ 0.0f }, z{ 0.0f }, rotY{ 0.0f };
		GetInterpolatedPosition2D(frameOffset, x, z, rotY);

		// 计算基础高度 (地形或水面)
		float baseY = 0;
		if (m_RelativeToGround)
		{
			CmpPtr<ICmpTerrain> cmpTerrain(GetSystemEntity());
			if (cmpTerrain)
				baseY = cmpTerrain->GetExactGroundLevel(x, z);

			if (m_Floating || m_ActorFloating)
			{
				CmpPtr<ICmpWaterManager> cmpWaterManager(GetSystemEntity());
				if (cmpWaterManager)
					baseY = std::max(baseY, cmpWaterManager->GetExactWaterLevel(x, z) - m_FloatDepth.ToFloat());
			}
		}

		// 计算插值后的Y坐标
		float y = baseY + m_Y.ToFloat() + Interpolate(-1 * m_LastYDifference.ToFloat(), 0.f, frameOffset);

		CMatrix3D m;

		// 线性插值对于XZ轴旋转已经足够好（因为角度变化很小）
		m.SetXRotation(Interpolate(m_LastInterpolatedRotX, m_InterpolatedRotX, frameOffset));
		m.RotateZ(Interpolate(m_LastInterpolatedRotZ, m_InterpolatedRotZ, frameOffset));

		CVector3D pos(x, y, z);

		// 加上建造进度的偏移
		pos.Y += GetConstructionProgressOffset(pos);

		// 设置Y轴旋转和平移
		m.RotateY(rotY + (float)M_PI); // 加PI是因为模型的朝向定义
		m.Translate(pos);

		return m;
	}

	// 获取插值区间的起始和结束3D位置
	void GetInterpolatedPositions(CVector3D& pos0, CVector3D& pos1) const
	{
		float baseY0 = 0;
		float baseY1 = 0;
		float x0 = m_LastX.ToFloat();
		float z0 = m_LastZ.ToFloat();
		float x1 = m_X.ToFloat();
		float z1 = m_Z.ToFloat();
		if (m_RelativeToGround)
		{
			CmpPtr<ICmpTerrain> cmpTerrain(GetSimContext(), SYSTEM_ENTITY);
			if (cmpTerrain)
			{
				baseY0 = cmpTerrain->GetExactGroundLevel(x0, z0);
				baseY1 = cmpTerrain->GetExactGroundLevel(x1, z1);
			}

			if (m_Floating || m_ActorFloating)
			{
				CmpPtr<ICmpWaterManager> cmpWaterManager(GetSimContext(), SYSTEM_ENTITY);
				if (cmpWaterManager)
				{
					baseY0 = std::max(baseY0, cmpWaterManager->GetExactWaterLevel(x0, z0) - m_FloatDepth.ToFloat());
					baseY1 = std::max(baseY1, cmpWaterManager->GetExactWaterLevel(x1, z1) - m_FloatDepth.ToFloat());
				}
			}
		}

		// 计算起始和结束的Y坐标
		float y0 = baseY0 + m_Y.ToFloat() + m_LastYDifference.ToFloat();
		float y1 = baseY1 + m_Y.ToFloat();

		pos0 = CVector3D(x0, y0, z0);
		pos1 = CVector3D(x1, y1, z1);

		// 加上建造进度偏移
		pos0.Y += GetConstructionProgressOffset(pos0);
		pos1.Y += GetConstructionProgressOffset(pos1);
	}

	// 处理接收到的消息
	void HandleMessage(const CMessage& msg, bool UNUSED(global)) override
	{
		switch (msg.GetType())
		{
		case MT_Interpolate: // 处理图形插值消息，每帧调用
		{
			PROFILE("Position::Interpolate");

			const CMessageInterpolate& msgData = static_cast<const CMessageInterpolate&> (msg);

			float rotY = m_RotY.ToFloat(); // 逻辑目标旋转

			// 如果图形旋转与逻辑旋转不一致，则进行平滑插值
			if (rotY != m_InterpolatedRotY)
			{
				float rotYSpeed = m_RotYSpeed.ToFloat();
				float delta = rotY - m_InterpolatedRotY;
				// 将角度差值 wrap 到 -PI 到 +PI 之间，以走最短路径旋转
				delta = fmodf(delta + (float)M_PI, 2 * (float)M_PI); // 范围 -2PI..2PI
				if (delta < 0) delta += 2 * (float)M_PI; // 范围 0..2PI
				delta -= (float)M_PI; // 范围 -M_PI..M_PI
				// 根据旋转速度和时间间隔，限制本次旋转的最大角度
				float deltaClamped = Clamp(delta, -rotYSpeed * msgData.deltaSimTime, +rotYSpeed * msgData.deltaSimTime);
				// 计算新的图形旋转角度
				m_InterpolatedRotY = rotY + deltaClamped - delta;

				// 更新视觉上的XZ轴旋转（以适应地形）
				if (m_InWorld)
				{
					m_LastInterpolatedRotX = m_InterpolatedRotX;
					m_LastInterpolatedRotZ = m_InterpolatedRotZ;

					UpdateXZRotation();
				}

				// 检查是否还需要继续插值，并更新消息订阅
				UpdateMessageSubscriptions();
			}

			break;
		}
		case MT_TurnStart: // 每个游戏逻辑回合开始时调用
		{
			// 保存上一帧的插值旋转
			m_LastInterpolatedRotX = m_InterpolatedRotX;
			m_LastInterpolatedRotZ = m_InterpolatedRotZ;

			// 如果位置发生变化，更新XZ旋转
			if (m_InWorld && (m_LastX != m_X || m_LastZ != m_Z))
				UpdateXZRotation();

			// 更新位置历史记录
			m_PrevX = m_LastX;
			m_PrevZ = m_LastZ;

			m_LastX = m_X;
			m_LastZ = m_Z;
			m_LastYDifference = entity_pos_t::Zero(); // Y轴偏移在新回合开始时重置

			break;
		}
		case MT_TerrainChanged: // 地形变化时调用
		case MT_WaterChanged:   // 水面变化时调用
		{
			// 广播插值位置变化，以更新实体高度
			AdvertiseInterpolatedPositionChanges();
			break;
		}
		case MT_Deserialized: // 加载游戏完成时调用
		{
			Deserialized();
			break;
		}
		}
	}

private:

	// 将字符串解析为 AnchorType 枚举
	AnchorType ParseAnchorString(const CStr& anchor)
	{
		if (anchor == "pitch")
			return AnchorType::PITCH;
		if (anchor == "roll")
			return AnchorType::ROLL;
		if (anchor == "pitch-roll")
			return AnchorType::PITCH_ROLL;
		return AnchorType::UPRIGHT; // 默认为直立
	}

	/*
	 * 每当 m_RotY 或 m_InterpolatedRotY 改变时必须调用此函数，
	 * 以决定我们是否需要订阅 MT_Interpolate 消息来使单位平滑旋转。
	 * 这是一种性能优化，避免不必要的每帧更新。
	 */
	void UpdateMessageSubscriptions()
	{
		bool needInterpolate = false;

		float rotY = m_RotY.ToFloat();
		if (rotY != m_InterpolatedRotY)
			needInterpolate = true;

		// 如果需要的订阅状态与当前状态不同，则更新订阅
		if (needInterpolate != m_EnabledMessageInterpolate)
		{
			GetSimContext().GetComponentManager().DynamicSubscriptionNonsync(MT_Interpolate, this, needInterpolate);
			m_EnabledMessageInterpolate = needInterpolate;
		}
	}

	/**
	 * 当任何影响 GetPosition2D() 或 GetRotation().Y 返回值的因素改变时，
	 * 必须调用此函数来广播逻辑位置变化消息。
	 * 这些因素包括：
	 * - m_InWorld
	 * - m_X, m_Z
	 * - m_RotY
	 */
	void AdvertisePositionChanges() const
	{
		// 通知所有子炮塔更新它们的位置
		for (std::set<entity_id_t>::const_iterator it = m_Turrets.begin(); it != m_Turrets.end(); ++it)
		{
			CmpPtr<ICmpPosition> cmpPosition(GetSimContext(), *it);
			if (cmpPosition)
				cmpPosition->UpdateTurretPosition();
		}
		// 向系统广播本实体的位置变化消息
		if (m_InWorld)
		{
			CMessagePositionChanged msg(GetEntityId(), true, m_X, m_Z, m_RotY);
			GetSimContext().GetComponentManager().PostMessage(GetEntityId(), msg);
		}
		else
		{
			CMessagePositionChanged msg(GetEntityId(), false, entity_pos_t::Zero(), entity_pos_t::Zero(), entity_angle_t::Zero());
			GetSimContext().GetComponentManager().PostMessage(GetEntityId(), msg);
		}
	}

	/**
	 * 当任何影响 GetInterpolatedPositions() 返回值的因素改变时，
	 * 必须调用此函数来广播插值位置变化消息。
	 * 这些因素包括：
	 * - m_InWorld
	 * - m_X, m_Z
	 * - m_LastX, m_LastZ
	 * - m_Y, m_LastYDifference, m_RelativeToGround
	 * - 如果 m_RelativeToGround，那么单位下方的地面
	 * - 如果 m_RelativeToGround && m_Float，那么水面高度
	 */
	void AdvertiseInterpolatedPositionChanges() const
	{
		if (m_InWorld)
		{
			CVector3D pos0, pos1;
			GetInterpolatedPositions(pos0, pos1);

			CMessageInterpolatedPositionChanged msg(GetEntityId(), true, pos0, pos1);
			GetSimContext().GetComponentManager().PostMessage(GetEntityId(), msg);
		}
		else
		{
			CMessageInterpolatedPositionChanged msg(GetEntityId(), false, CVector3D(), CVector3D());
			GetSimContext().GetComponentManager().PostMessage(GetEntityId(), msg);
		}
	}

	// 更新实体的XZ轴旋转，以使其视觉上适应地形坡度
	void UpdateXZRotation()
	{
		if (!m_InWorld)
		{
			LOGERROR("当实体不在世界中时调用了 CCmpPosition::UpdateXZRotation");
			return;
		}

		// 优先使用Actor定义的锚定方式
		AnchorType anchor = m_ActorAnchorType == AnchorType::UNDEFINED ? m_AnchorType : m_ActorAnchorType;
		// 如果是直立，或者已有手动设置的XZ旋转，则直接使用这些值
		if (anchor == AnchorType::UPRIGHT || !m_RotZ.IsZero() || !m_RotX.IsZero())
		{
			m_InterpolatedRotX = m_RotX.ToFloat();
			m_InterpolatedRotZ = m_RotZ.ToFloat();
			return;
		}

		CmpPtr<ICmpTerrain> cmpTerrain(GetSystemEntity());
		if (!cmpTerrain || !cmpTerrain->IsLoaded())
		{
			LOGERROR("地形未加载");
			return;
		}

		// TODO: 对于大型单位或建筑，是否应该对法线进行平均（平均所有在其下的地块）？
		// 获取脚下地形的精确法线向量
		CVector3D normal = cmpTerrain->CalcExactNormal(m_X.ToFloat(), m_Z.ToFloat());

		// 旋转法线，使其X正方向与单位朝向一致
		CVector2D projected = CVector2D(normal.X, normal.Z);
		projected.Rotate(m_InterpolatedRotY);

		normal.X = projected.X;
		normal.Z = projected.Y;

		// 根据法线计算俯仰(X)和翻滚(Z)角度
		if (anchor == AnchorType::PITCH || anchor == AnchorType::PITCH_ROLL)
			m_InterpolatedRotX = -atan2(normal.Z, normal.Y);

		if (anchor == AnchorType::ROLL || anchor == AnchorType::PITCH_ROLL)
			m_InterpolatedRotZ = atan2(normal.X, normal.Y);
	}
};

// 注册此组件类型，使其在游戏中可用
REGISTER_COMPONENT_TYPE(Position)
