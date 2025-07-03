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

 /*
  * 版权 (C) 2025 Wildfire Games。
  * 本文件是 0 A.D. 的一部分。
  *
  * 0 A.D. 是自由软件：您可以根据自由软件基金会发布的 GNU 通用公共许可证
  *（版本2或您选择的任何更高版本）的条款重新分发和/或修改它。
  *
  * 0 A.D. 的分发是希望它能有用，但没有任何保证；甚至没有对
  * 适销性或特定用途适用性的默示保证。有关更多详细信息，请参阅 GNU 通用公共许可证。
  *
  * 您应该已经随 0 A.D. 收到一份 GNU 通用公共许可证的副本。如果没有，请参阅 <http://www.gnu.org/licenses/>。
  */


#ifndef INCLUDED_CCMPUNITMOTION
#define INCLUDED_CCMPUNITMOTION

#include "simulation2/system/Component.h"
#include "ICmpUnitMotion.h"

#include "simulation2/components/CCmpUnitMotionManager.h"
#include "simulation2/components/ICmpObstruction.h"
#include "simulation2/components/ICmpObstructionManager.h"
#include "simulation2/components/ICmpOwnership.h"
#include "simulation2/components/ICmpPosition.h"
#include "simulation2/components/ICmpPathfinder.h"
#include "simulation2/components/ICmpRangeManager.h"
#include "simulation2/components/ICmpValueModificationManager.h"
#include "simulation2/components/ICmpVisual.h"
#include "simulation2/helpers/Geometry.h"
#include "simulation2/helpers/Render.h"
#include "simulation2/MessageTypes.h"
#include "simulation2/serialization/SerializedPathfinder.h"
#include "simulation2/serialization/SerializedTypes.h"

#include "graphics/Overlay.h"
#include "maths/FixedVector2D.h"
#include "ps/CLogger.h"
#include "ps/Profile.h"
#include "renderer/Scene.h"

#include <algorithm>

  // NB: this implementation of ICmpUnitMotion is very tightly coupled with UnitMotionManager.
  // As such, both are compiled in the same TU.
  // 注意：ICmpUnitMotion 的这个实现与 UnitMotionManager 紧密耦合。
  // 因此，两者在同一个翻译单元（TU）中编译。

  // For debugging; units will start going straight to the target
  // instead of calling the pathfinder
  // 用于调试；单位将开始直接走向目标，而不是调用寻路器
#define DISABLE_PATHFINDER 0

namespace
{
	/**
	 * Min/Max range to restrict short path queries to. (Larger ranges are (much) slower,
	 * smaller ranges might miss some legitimate routes around large obstacles.)
	 * NB: keep the max-range in sync with the vertex pathfinder "move the search space" heuristic.
	 */
	 /**
	   * 用于限制短路径查询的最小/最大范围。（范围越大，速度（越）慢，
	   * 范围越小，可能会错过围绕大型障碍物的某些合法路线。）
	   * 注意：保持最大范围与顶点寻路器的“移动搜索空间”启发式算法同步。
	   */
	constexpr entity_pos_t SHORT_PATH_MIN_SEARCH_RANGE = entity_pos_t::FromInt(12 * Pathfinding::NAVCELL_SIZE_INT);
	constexpr entity_pos_t SHORT_PATH_MAX_SEARCH_RANGE = entity_pos_t::FromInt(56 * Pathfinding::NAVCELL_SIZE_INT);
	constexpr entity_pos_t SHORT_PATH_SEARCH_RANGE_INCREMENT = entity_pos_t::FromInt(4 * Pathfinding::NAVCELL_SIZE_INT);
	constexpr u8 SHORT_PATH_SEARCH_RANGE_INCREASE_DELAY = 1;

	/**
	 * When using the short-pathfinder to rejoin a long-path waypoint, aim for a circle of this radius around the waypoint.
	 */
	 /**
	   * 当使用短程寻路器重新加入长程路径的航点时，瞄准航点周围这个半径的圆形区域。
	   */
	constexpr entity_pos_t SHORT_PATH_LONG_WAYPOINT_RANGE = entity_pos_t::FromInt(4 * Pathfinding::NAVCELL_SIZE_INT);

	/**
	 * Minimum distance to goal for a long path request
	 */
	 /**
	   * 请求长程路径时与目标的最小距离。
	   */
	constexpr entity_pos_t LONG_PATH_MIN_DIST = entity_pos_t::FromInt(16 * Pathfinding::NAVCELL_SIZE_INT);

	/**
	 * If we are this close to our target entity/point, then think about heading
	 * for it in a straight line instead of pathfinding.
	 */
	 /**
	   * 如果我们离目标实体/点这么近，就考虑直接朝它直线前进，而不是进行寻路。
	   */
	constexpr entity_pos_t DIRECT_PATH_RANGE = entity_pos_t::FromInt(24 * Pathfinding::NAVCELL_SIZE_INT);

	/**
	 * To avoid recomputing paths too often, have some leeway for target range checks
	 * based on our distance to the target. Increase that incertainty by one navcell
	 * for every this many tiles of distance.
	 */
	 /**
	   * 为避免过于频繁地重新计算路径，根据我们与目标的距离，为目标范围检查设置一些余地。
	   * 每隔这么多格的距离，将不确定性增加一个导航单元（navcell）。
	   */
	constexpr entity_pos_t TARGET_UNCERTAINTY_MULTIPLIER = entity_pos_t::FromInt(8 * Pathfinding::NAVCELL_SIZE_INT);

	/**
	 * When following a known imperfect path (i.e. a path that won't take us in range of our goal
	 * we still recompute a new path every N turn to adapt to moving targets (for example, ships that must pickup
	 * units may easily end up in this state, they still need to adjust to moving units).
	 * This is rather arbitrary and mostly for simplicity & optimisation (a better recomputing algorithm
	 * would not need this).
	 */
	 /**
	   * 当遵循已知的不完美路径时（即，一条不会将我们带到目标范围内的路径），
	   * 我们仍然每 N 个回合重新计算一条新路径，以适应移动的目标（例如，必须接载
	   * 单位的船只很容易处于这种状态，它们仍然需要适应移动的单位）。
	   * 这个设定相当随意，主要是为了简单和优化（一个更好的重计算算法
	   * 将不需要这个）。
	   */
	constexpr u8 KNOWN_IMPERFECT_PATH_RESET_COUNTDOWN = 12;

	/**
	 * When we fail to move this many turns in a row, inform other components that the move will fail.
	 * Experimentally, this number needs to be somewhat high or moving groups of units will lead to stuck units.
	 * However, too high means units will look idle for a long time when they are failing to move.
	 * TODO: if UnitMotion could send differentiated "unreachable" and "currently stuck" failing messages,
	 * this could probably be lowered.
	 */
	 /**
	   * 当我们连续这么多回合移动失败时，通知其他组件移动将失败。
	   * 根据经验，这个数字需要设置得比较高，否则移动中的单位群组会导致单位卡住。
	   * 然而，太高则意味着单位在移动失败时会看起来空闲很长时间。
	   * 待办事项：如果 UnitMotion 可以发送区分“无法到达”和“当前卡住”的失败消息，
	   * 这个值可能会降低。
	   */
	constexpr u8 MAX_FAILED_MOVEMENTS = 35;

	/**
	 * When computing paths but failing to move, we want to occasionally alternate pathfinder systems
	 * to avoid getting stuck (the short pathfinder can unstuck the long-range one and vice-versa, depending).
	 */
	 /**
	   * 在计算路径但移动失败时，我们希望偶尔交替使用寻路系统
	   * 以避免卡住（短程寻路器可以解救长程寻路器，反之亦然）。
	   */
	constexpr u8 ALTERNATE_PATH_TYPE_DELAY = 3;
	constexpr u8 ALTERNATE_PATH_TYPE_EVERY = 6;

	/**
	 * Units can occasionally get stuck near corners. The cause is a mismatch between CheckMovement and the short pathfinder.
	 * The problem is the short pathfinder finds an impassable path when units are right on an obstruction edge.
	 * Fixing this math mismatch is perhaps possible, but fixing it in UM is rather easy: just try backing up a bit
	 * and that will probably un-stuck the unit. This is the 'failed movement' turn on which to try that.
	 */
	 /**
	   * 单位有时会在角落卡住。原因是 CheckMovement 和短程寻路器之间的不匹配。
	   * 问题是当单位正好在障碍物边缘时，短程寻路器会找到一条不可通行的路径。
	   * 修正这个数学上的不匹配也许是可能的，但在单位移动（UM）中修复它相当容易：只需尝试后退一点
	   * 这很可能会解开单位的卡住状态。这是在“移动失败”的回合尝试此操作的时机。
	   */
	constexpr u8 BACKUP_HACK_DELAY = 10;

	/**
	 * After this many failed computations, start sending "VERY_OBSTRUCTED" messages instead.
	 * Should probably be larger than ALTERNATE_PATH_TYPE_DELAY.
	 */
	 /**
	   * 经过这么多次失败的计算后，开始发送 "VERY_OBSTRUCTED" (严重受阻) 消息。
	   * 这个值可能应该大于 ALTERNATE_PATH_TYPE_DELAY (交替寻路类型延迟)。
	   */
	constexpr u8 VERY_OBSTRUCTED_THRESHOLD = 10;

	// 长路径覆盖层颜色
	const CColor OVERLAY_COLOR_LONG_PATH(1, 1, 1, 1);
	// 短路径覆盖层颜色
	const CColor OVERLAY_COLOR_SHORT_PATH(1, 0, 0, 1);
} // anonymous namespace

// 单位移动组件的最终实现
class CCmpUnitMotion final : public ICmpUnitMotion
{
	// 友元类，允许 CCmpUnitMotionManager 访问此类的私有和保护成员
	friend class CCmpUnitMotionManager;
public:
	// 类初始化，在组件管理器中注册并订阅所需的消息类型
	static void ClassInit(CComponentManager& componentManager)
	{
		componentManager.SubscribeToMessageType(MT_Create);
		componentManager.SubscribeToMessageType(MT_Destroy);
		componentManager.SubscribeToMessageType(MT_PathResult);
		componentManager.SubscribeToMessageType(MT_OwnershipChanged);
		componentManager.SubscribeToMessageType(MT_ValueModification);
		componentManager.SubscribeToMessageType(MT_MovementObstructionChanged);
		componentManager.SubscribeToMessageType(MT_Deserialized);
	}

	// 默认的组件内存分配器
	DEFAULT_COMPONENT_ALLOCATOR(UnitMotion)

		// 是否启用调试覆盖层
		bool m_DebugOverlayEnabled;
	// 用于调试的长路径覆盖层线条
	std::vector<SOverlayLine> m_DebugOverlayLongPathLines;
	// 用于调试的短路径覆盖层线条
	std::vector<SOverlayLine> m_DebugOverlayShortPathLines;

	// Template state:
	// 模板状态：

	// 是否是阵型控制器
	bool m_IsFormationController;

	// 模板定义的步行速度、奔跑乘数、加速度和重量
	fixed m_TemplateWalkSpeed, m_TemplateRunMultiplier, m_TemplateAcceleration, m_TemplateWeight;
	// 通行性类别ID
	pass_class_t m_PassClass;
	// 通行性类别名称
	std::string m_PassClassName;

	// Dynamic state:
	// 动态状态：

	// 单位的碰撞体积（清除区）
	entity_pos_t m_Clearance;

	// cached for efficiency
	// 为效率而缓存的步行速度和奔跑乘数
	fixed m_WalkSpeed, m_RunMultiplier;

	// 移动后是否朝向目标点
	bool m_FacePointAfterMove;

	// Whether the unit participates in pushing.
	// 单位是否参与推挤
	bool m_Pushing = false;

	// Whether the unit blocks movement (& is blocked by movement blockers)
	// Cached from ICmpObstruction.
	// 单位是否阻挡移动（并被移动阻挡物所阻挡）
	// 从 ICmpObstruction 缓存
	bool m_BlockMovement = false;

	// Internal counter used when recovering from obstructed movement.
	// Most notably, increases the search range of the vertex pathfinder.
	// See HandleObstructedMove() for more details.
	// 用于从受阻移动中恢复的内部计数器。
	// 最值得注意的是，它会增加顶点寻路器的搜索范围。
	// 详见 HandleObstructedMove()。
	u8 m_FailedMovements = 0;

	// If > 0, PathingUpdateNeeded returns false always.
	// This exists because the goal may be unreachable to the short/long pathfinder.
	// In such cases, we would compute inacceptable paths and PathingUpdateNeeded would trigger every turn,
	// which would be quite bad for performance.
	// To avoid that, when we know the new path is imperfect, treat it as OK and follow it anyways.
	// When reaching the end, we'll go through HandleObstructedMove and reset regardless.
	// To still recompute now and then (the target may be moving), this is a countdown decremented on each frame.
	// 如果 > 0，PathingUpdateNeeded 将始终返回 false。
	// 这是因为目标可能对短/长程寻路器来说是不可达的。
	// 在这种情况下，我们会计算出不可接受的路径，并且 PathingUpdateNeeded 会在每个回合触发，
	// 这对性能非常不利。
	// 为避免这种情况，当我们知道新路径不完美时，就把它当作可行的路径并继续跟随。
	// 到达终点时，我们会通过 HandleObstructedMove 并无论如何都会重置。
	// 为了仍然能不时地重新计算（因为目标可能在移动），这是一个在每帧递减的倒计时器。
	u8 m_FollowKnownImperfectPathCountdown = 0;

	// 寻路凭证，用于跟踪异步寻路请求
	struct Ticket {
		u32 m_Ticket = 0; // asynchronous request ID we're waiting for, or 0 if none
		// 正在等待的异步请求ID，如果没有则为0
		enum Type {
			SHORT_PATH, // 短路径
			LONG_PATH   // 长路径
		} m_Type = SHORT_PATH; // Pick some default value to avoid UB.
		// 选择一个默认值以避免未定义行为

// 清除凭证
		void clear() { m_Ticket = 0; }
	} m_ExpectedPathTicket;

	// 移动请求结构体
	struct MoveRequest {
		enum Type {
			NONE,   // 无
			POINT,  // 到点
			ENTITY, // 到实体
			OFFSET  // 到（相对于实体的）偏移位置
		} m_Type = NONE;
		entity_id_t m_Entity = INVALID_ENTITY; // 目标实体ID
		CFixedVector2D m_Position;             // 目标位置或偏移量
		entity_pos_t m_MinRange, m_MaxRange;   // 最小和最大范围

		// For readability
		// 为提高可读性
		CFixedVector2D GetOffset() const { return m_Position; };

		MoveRequest() = default;
		MoveRequest(CFixedVector2D pos, entity_pos_t minRange, entity_pos_t maxRange) : m_Type(POINT), m_Position(pos), m_MinRange(minRange), m_MaxRange(maxRange) {};
		MoveRequest(entity_id_t target, entity_pos_t minRange, entity_pos_t maxRange) : m_Type(ENTITY), m_Entity(target), m_MinRange(minRange), m_MaxRange(maxRange) {};
		MoveRequest(entity_id_t target, CFixedVector2D offset) : m_Type(OFFSET), m_Entity(target), m_Position(offset) {};
	} m_MoveRequest;

	// If this is not INVALID_ENTITY, the unit is a formation member.
	// 如果这个值不是 INVALID_ENTITY，那么该单位是阵型成员。
	entity_id_t m_FormationController = INVALID_ENTITY;

	// If the entity moves, it will do so at m_WalkSpeed * m_SpeedMultiplier.
	// 如果实体移动，它的速度将是 m_WalkSpeed * m_SpeedMultiplier。
	fixed m_SpeedMultiplier;
	// This caches the resulting speed from m_WalkSpeed * m_SpeedMultiplier for convenience.
	// 为方便起见，这里缓存了 m_WalkSpeed * m_SpeedMultiplier 的最终速度。
	fixed m_Speed;

	// Mean speed over the last turn.
	// 上一回合的平均速度。
	fixed m_LastTurnSpeed;

	// The speed achieved at the end of the current turn.
	// 当前回合结束时达到的速度。
	fixed m_CurrentSpeed;

	// 瞬间转向角度
	fixed m_InstantTurnAngle;

	// 加速度
	fixed m_Acceleration;

	// Currently active paths (storing waypoints in reverse order).
	// The last item in each path is the point we're currently heading towards.
	// 当前活动的路径（以相反顺序存储航点）。
	// 每个路径中的最后一项是我们当前正前往的点。
	WaypointPath m_LongPath;  // 长路径
	WaypointPath m_ShortPath; // 短路径

	// 获取此组件的XML结构定义
	static std::string GetSchema()
	{
		return
			"<a:help>Provides the unit with the ability to move around the world by itself.</a:help>" // 为单位提供在世界中自行移动的能力
			"<a:example>"
			"<WalkSpeed>7.0</WalkSpeed>"
			"<PassabilityClass>default</PassabilityClass>"
			"</a:example>"
			"<element name='FormationController'>" // 阵型控制器
			"<data type='boolean'/>"
			"</element>"
			"<element name='WalkSpeed' a:help='Basic movement speed (in meters per second).'>" // 基本移动速度（米/秒）
			"<ref name='positiveDecimal'/>"
			"</element>"
			"<optional>"
			"<element name='RunMultiplier' a:help='How much faster the unit goes when running (as a multiple of walk speed).'>" // 单位奔跑时速度增加的倍数（相对于步行速度）
			"<ref name='positiveDecimal'/>"
			"</element>"
			"</optional>"
			"<element name='InstantTurnAngle' a:help='Angle we can turn instantly. Any value greater than pi will disable turning times. Avoid zero since it stops the entity every turn.'>" // 我们可以立即转向的角度。任何大于pi的值都将禁用转向时间。避免使用零，因为它会在每个回合停止实体。
			"<ref name='positiveDecimal'/>"
			"</element>"
			"<element name='Acceleration' a:help='Acceleration (in meters per second^2).'>" // 加速度（米/秒^2）
			"<ref name='positiveDecimal'/>"
			"</element>"
			"<element name='PassabilityClass' a:help='Identifies the terrain passability class (values are defined in special/pathfinder.xml).'>" // 标识地形通行性类别（值在 special/pathfinder.xml 中定义）
			"<text/>"
			"</element>"
			"<element name='Weight' a:help='Makes this unit both push harder and harder to push. 10 is considered the base value.'>" // 使该单位更难被推动，也更容易推动其他单位。10被认为是基础值。
			"<ref name='positiveDecimal'/>"
			"</element>"
			"<optional>"
			"<element name='DisablePushing'>" // 禁用推挤
			"<data type='boolean'/>"
			"</element>"
			"</optional>";
	}

	// 从模板参数节点初始化组件
	void Init(const CParamNode& paramNode) override
	{
		m_IsFormationController = paramNode.GetChild("FormationController").ToBool();

		m_FacePointAfterMove = true;

		m_WalkSpeed = m_TemplateWalkSpeed = m_Speed = paramNode.GetChild("WalkSpeed").ToFixed();
		m_SpeedMultiplier = fixed::FromInt(1);
		m_LastTurnSpeed = m_CurrentSpeed = fixed::Zero();

		m_RunMultiplier = m_TemplateRunMultiplier = fixed::FromInt(1);
		if (paramNode.GetChild("RunMultiplier").IsOk())
			m_RunMultiplier = m_TemplateRunMultiplier = paramNode.GetChild("RunMultiplier").ToFixed();

		m_InstantTurnAngle = paramNode.GetChild("InstantTurnAngle").ToFixed();

		m_Acceleration = m_TemplateAcceleration = paramNode.GetChild("Acceleration").ToFixed();

		m_TemplateWeight = paramNode.GetChild("Weight").ToFixed();

		m_PassClassName = paramNode.GetChild("PassabilityClass").ToString();
		SetPassabilityData(m_PassClassName);

		CmpPtr<ICmpObstruction> cmpObstruction(GetEntityHandle());
		if (cmpObstruction)
			m_BlockMovement = cmpObstruction->GetBlockMovementFlag(true);

		SetParticipateInPushing(!paramNode.GetChild("DisablePushing").IsOk() || !paramNode.GetChild("DisablePushing").ToBool());

		m_DebugOverlayEnabled = false;
	}

	// 组件析构（或反初始化）
	void Deinit() override
	{
	}

	// 通用的序列化/反序列化函数
	template<typename S>
	void SerializeCommon(S& serialize)
	{
		// m_Clearance and m_PassClass are constructed from this.
		// m_Clearance 和 m_PassClass 是由此构造的。
		serialize.StringASCII("pass class", m_PassClassName, 0, 64);

		serialize.NumberU32_Unbounded("ticket", m_ExpectedPathTicket.m_Ticket);
		Serializer(serialize, "ticket type", m_ExpectedPathTicket.m_Type, Ticket::Type::LONG_PATH);

		serialize.NumberU8_Unbounded("failed movements", m_FailedMovements);
		serialize.NumberU8_Unbounded("followknownimperfectpath", m_FollowKnownImperfectPathCountdown);

		Serializer(serialize, "target type", m_MoveRequest.m_Type, MoveRequest::Type::OFFSET);
		serialize.NumberU32_Unbounded("target entity", m_MoveRequest.m_Entity);
		serialize.NumberFixed_Unbounded("target pos x", m_MoveRequest.m_Position.X);
		serialize.NumberFixed_Unbounded("target pos y", m_MoveRequest.m_Position.Y);
		serialize.NumberFixed_Unbounded("target min range", m_MoveRequest.m_MinRange);
		serialize.NumberFixed_Unbounded("target max range", m_MoveRequest.m_MaxRange);

		serialize.NumberU32_Unbounded("formation controller", m_FormationController);

		serialize.NumberFixed_Unbounded("speed multiplier", m_SpeedMultiplier);

		serialize.NumberFixed_Unbounded("last turn speed", m_LastTurnSpeed);
		serialize.NumberFixed_Unbounded("current speed", m_CurrentSpeed);

		serialize.NumberFixed_Unbounded("instant turn angle", m_InstantTurnAngle);

		serialize.NumberFixed_Unbounded("acceleration", m_Acceleration);

		serialize.Bool("facePointAfterMove", m_FacePointAfterMove);
		serialize.Bool("pushing", m_Pushing);

		Serializer(serialize, "long path", m_LongPath.m_Waypoints);
		Serializer(serialize, "short path", m_ShortPath.m_Waypoints);
	}

	// 序列化，用于保存游戏状态
	void Serialize(ISerializer& serialize) override
	{
		SerializeCommon(serialize);
	}

	// 反序列化，用于加载游戏状态
	void Deserialize(const CParamNode& paramNode, IDeserializer& deserialize) override
	{
		Init(paramNode);

		SerializeCommon(deserialize);

		SetPassabilityData(m_PassClassName);

		CmpPtr<ICmpObstruction> cmpObstruction(GetEntityHandle());
		if (cmpObstruction)
			m_BlockMovement = cmpObstruction->GetBlockMovementFlag(false);
	}

	// 处理组件接收到的消息
	void HandleMessage(const CMessage& msg, bool UNUSED(global)) override
	{
		switch (msg.GetType())
		{
		case MT_RenderSubmit: // 渲染提交消息
		{
			PROFILE("UnitMotion::RenderSubmit");
			const CMessageRenderSubmit& msgData = static_cast<const CMessageRenderSubmit&> (msg);
			RenderSubmit(msgData.collector);
			break;
		}
		case MT_PathResult: // 路径计算结果消息
		{
			const CMessagePathResult& msgData = static_cast<const CMessagePathResult&> (msg);
			PathResult(msgData.ticket, msgData.path);
			break;
		}
		case MT_Create: // 实体创建消息
		{
			if (!ENTITY_IS_LOCAL(GetEntityId()))
				CmpPtr<ICmpUnitMotionManager>(GetSystemEntity())->Register(this, GetEntityId(), m_IsFormationController);
			break;
		}
		case MT_Destroy: // 实体销毁消息
		{
			if (!ENTITY_IS_LOCAL(GetEntityId()))
				CmpPtr<ICmpUnitMotionManager>(GetSystemEntity())->Unregister(GetEntityId());
			break;
		}
		case MT_MovementObstructionChanged: // 移动障碍物改变消息
		{
			CmpPtr<ICmpObstruction> cmpObstruction(GetEntityHandle());
			if (cmpObstruction)
				m_BlockMovement = cmpObstruction->GetBlockMovementFlag(false);
			break;
		}
		case MT_ValueModification: // 数值修改消息
		{
			const CMessageValueModification& msgData = static_cast<const CMessageValueModification&> (msg);
			if (msgData.component != L"UnitMotion")
				break;
			[[fallthrough]]; // 如果是 UnitMotion 的修改，则继续执行下面的逻辑
		}
		case MT_OwnershipChanged: // 所有权变更消息
		{
			OnValueModification();
			break;
		}
		case MT_Deserialized: // 反序列化完成消息
		{
			OnValueModification();
			break;
		}
		}
	}

	// 根据需要动态更新消息订阅
	void UpdateMessageSubscriptions()
	{
		bool needRender = m_DebugOverlayEnabled;
		GetSimContext().GetComponentManager().DynamicSubscriptionNonsync(MT_RenderSubmit, this, needRender);
	}

	// 检查是否已请求移动
	bool IsMoveRequested() const override
	{
		return m_MoveRequest.m_Type != MoveRequest::NONE;
	}

	// 获取速度乘数
	fixed GetSpeedMultiplier() const override
	{
		return m_SpeedMultiplier;
	}

	// 设置速度乘数
	void SetSpeedMultiplier(fixed multiplier) override
	{
		m_SpeedMultiplier = std::min(multiplier, m_RunMultiplier);
		m_Speed = m_SpeedMultiplier.Multiply(GetWalkSpeed());
	}

	// 获取最终计算出的速度
	fixed GetSpeed() const override
	{
		return m_Speed;
	}

	// 获取基础步行速度
	fixed GetWalkSpeed() const override
	{
		return m_WalkSpeed;
	}

	// 获取奔跑速度乘数
	fixed GetRunMultiplier() const override
	{
		return m_RunMultiplier;
	}

	// 估算单位在未来给定时间 `dt` 后的位置
	CFixedVector2D EstimateFuturePosition(const fixed dt) const override
	{
		CmpPtr<ICmpPosition> cmpPosition(GetEntityHandle());
		if (!cmpPosition || !cmpPosition->IsInWorld())
			return CFixedVector2D();

		// TODO: formation members should perhaps try to use the controller's position.
		// 待办事项：阵型成员或许应该尝试使用控制器的位置。

		CFixedVector2D pos = cmpPosition->GetPosition2D();
		entity_angle_t angle = cmpPosition->GetRotation().Y;
		fixed speed = m_CurrentSpeed;
		// Copy the path so we don't change it.
		// 复制路径，以免修改它。
		WaypointPath shortPath = m_ShortPath;
		WaypointPath longPath = m_LongPath;

		PerformMove(dt, cmpPosition->GetTurnRate(), shortPath, longPath, pos, speed, angle, 0);
		return pos;
	}

	// 获取加速度
	fixed GetAcceleration() const override
	{
		return m_Acceleration;
	}

	// 设置加速度
	void SetAcceleration(fixed acceleration) override
	{
		m_Acceleration = acceleration;
	}

	// 获取单位的重量（用于推挤）
	virtual entity_pos_t GetWeight() const
	{
		return m_TemplateWeight;
	}

	// 获取通行性类别ID
	pass_class_t GetPassabilityClass() const override
	{
		return m_PassClass;
	}

	// 获取通行性类别名称
	std::string GetPassabilityClassName() const override
	{
		return m_PassClassName;
	}

	// 设置通行性类别名称
	void SetPassabilityClassName(const std::string& passClassName) override
	{
		if (!m_IsFormationController)
		{
			LOGWARNING("Only formation controllers can change their passability class"); // 只有阵型控制器才能改变它们的通行性类别
			return;
		}
		SetPassabilityData(passClassName);
	}

	// 获取当前速度
	fixed GetCurrentSpeed() const override
	{
		return m_CurrentSpeed;
	}

	// 设置移动后是否朝向目标点
	void SetFacePointAfterMove(bool facePointAfterMove) override
	{
		m_FacePointAfterMove = facePointAfterMove;
	}

	// 获取移动后是否朝向目标点的设置
	bool GetFacePointAfterMove() const override
	{
		return m_FacePointAfterMove;
	}
		
	// 设置是否启用调试覆盖层
	void SetDebugOverlay(bool enabled) override
	{
		m_DebugOverlayEnabled = enabled;
		UpdateMessageSubscriptions();
	}

	// 移动到指定点和范围
	bool MoveToPointRange(entity_pos_t x, entity_pos_t z, entity_pos_t minRange, entity_pos_t maxRange) override
	{
		return MoveTo(MoveRequest(CFixedVector2D(x, z), minRange, maxRange));
	}

	// 移动到目标实体和范围
	bool MoveToTargetRange(entity_id_t target, entity_pos_t minRange, entity_pos_t maxRange) override
	{
		return MoveTo(MoveRequest(target, minRange, maxRange));
	}

	// 移动到阵型中的指定偏移位置
	void MoveToFormationOffset(entity_id_t controller, entity_pos_t x, entity_pos_t z) override
	{
		// Pass the controller to the move request anyways.
		// 无论如何都将控制器传递给移动请求。
		MoveTo(MoveRequest(controller, CFixedVector2D(x, z)));
	}

	// 设置单位为阵型成员
	void SetMemberOfFormation(entity_id_t controller) override
	{
		m_FormationController = controller;
	}

	// 检查目标范围是否可达
	bool IsTargetRangeReachable(entity_id_t target, entity_pos_t minRange, entity_pos_t maxRange) override;

	// 使单位朝向指定点
	void FaceTowardsPoint(entity_pos_t x, entity_pos_t z) override;

	/**
	 * Clears the current MoveRequest - the unit will stop and no longer try and move.
	 * This should never be called from UnitMotion, since MoveToX orders are given
	 * by other components - these components should also decide when to stop.
	 */
	 /**
	   * 清除当前的移动请求 - 单位将停止并不再尝试移动。
	   * 这个函数绝不应该从 UnitMotion 内部调用，因为 MoveToX 命令是由其他组件发出的 -
	   * 这些组件也应该决定何时停止。
	   */
	void StopMoving() override
	{
		if (m_FacePointAfterMove)
		{
			CmpPtr<ICmpPosition> cmpPosition(GetEntityHandle());
			if (cmpPosition && cmpPosition->IsInWorld())
			{
				CFixedVector2D targetPos;
				if (ComputeTargetPosition(targetPos))
					FaceTowardsPointFromPos(cmpPosition->GetPosition2D(), targetPos.X, targetPos.Y);
			}
		}

		m_MoveRequest = MoveRequest();
		m_ExpectedPathTicket.clear();
		m_LongPath.m_Waypoints.clear();
		m_ShortPath.m_Waypoints.clear();
	}

	// 获取单位的清除区（碰撞体积）
	entity_pos_t GetUnitClearance() const override
	{
		return m_Clearance;
	}

private:
	// 检查是否为阵型成员
	bool IsFormationMember() const
	{
		return m_FormationController != INVALID_ENTITY;
	}

	// 检查是否作为阵型的一部分在移动
	bool IsMovingAsFormation() const
	{
		return IsFormationMember() && m_MoveRequest.m_Type == MoveRequest::OFFSET;
	}

	// 检查阵型控制器是否在移动
	bool IsFormationControllerMoving() const
	{
		CmpPtr<ICmpUnitMotion> cmpControllerMotion(GetSimContext(), m_FormationController);
		return cmpControllerMotion && cmpControllerMotion->IsMoveRequested();
	}

	// 获取单位所属的组（如果是阵型成员，则为控制器ID，否则为自身ID）
	entity_id_t GetGroup() const
	{
		return IsFormationMember() ? m_FormationController : GetEntityId();
	}

	// 设置是否参与推挤
	void SetParticipateInPushing(bool pushing)
	{
		CmpPtr<ICmpUnitMotionManager> cmpUnitMotionManager(GetSystemEntity());
		m_Pushing = pushing && cmpUnitMotionManager->IsPushingActivated();
	}

	// 根据通行性类别名称设置通行性数据
	void SetPassabilityData(const std::string& passClassName)
	{
		m_PassClassName = passClassName;
		CmpPtr<ICmpPathfinder> cmpPathfinder(GetSystemEntity());
		if (cmpPathfinder)
		{
			m_PassClass = cmpPathfinder->GetPassabilityClass(passClassName);
			m_Clearance = cmpPathfinder->GetClearance(m_PassClass);

			CmpPtr<ICmpObstruction> cmpObstruction(GetEntityHandle());
			if (cmpObstruction)
				cmpObstruction->SetUnitClearance(m_Clearance);
		}
	}

	/**
	 * Warns other components that our current movement will likely fail (e.g. we won't be able to reach our target)
	 * This should only be called before the actual movement in a given turn, or units might both move and try to do things
	 * on the same turn, leading to gliding units.
	 */
	 /**
	   * 警告其他组件，我们当前的移动很可能会失败（例如，我们将无法到达目标）。
	   * 这应该只在给定回合的实际移动之前调用，否则单位可能会在同一回合内既移动又尝试做其他事情，
	   * 导致单位滑行。
	   */
	void MoveFailed()
	{
		// Don't notify if we are a formation member in a moving formation - we can occasionally be stuck for a long time
		// if our current offset is unreachable, but we don't want to end up stuck.
		// (If the formation controller has stopped moving however, we can safely message).
		// 如果我们是在移动阵型中的阵型成员，则不通知 - 如果我们当前的偏移位置无法到达，我们有时会卡住很长时间，
		// 但我们不希望最终卡住。
		// （然而，如果阵型控制器已停止移动，我们可以安全地发送消息）。
		if (IsFormationMember() && IsFormationControllerMoving())
			return;

		CMessageMotionUpdate msg(CMessageMotionUpdate::LIKELY_FAILURE);
		GetSimContext().GetComponentManager().PostMessage(GetEntityId(), msg);
	}

	/**
	 * Warns other components that our current movement is likely over (i.e. we probably reached our destination)
	 * This should only be called before the actual movement in a given turn, or units might both move and try to do things
	 * on the same turn, leading to gliding units.
	 */
	 /**
	   * 警告其他组件，我们当前的移动可能已经结束（即，我们可能已到达目的地）。
	   * 这应该只在给定回合的实际移动之前调用，否则单位可能会在同一回合内既移动又尝试做其他事情，
	   * 导致单位滑行。
	   */
	void MoveSucceeded()
	{
		// Don't notify if we are a formation member in a moving formation - we can occasionally be stuck for a long time
		// if our current offset is unreachable, but we don't want to end up stuck.
		// (If the formation controller has stopped moving however, we can safely message).
		// 如果我们是在移动阵型中的阵型成员，则不通知 - 如果我们当前的偏移位置无法到达，我们有时会卡住很长时间，
		// 但我们不希望最终卡住。
		// （然而，如果阵型控制器已停止移动，我们可以安全地发送消息）。
		if (IsFormationMember() && IsFormationControllerMoving())
			return;

		CMessageMotionUpdate msg(CMessageMotionUpdate::LIKELY_SUCCESS);
		GetSimContext().GetComponentManager().PostMessage(GetEntityId(), msg);
	}

	/**
	 * Warns other components that our current movement was obstructed (i.e. we failed to move this turn).
	 * This should only be called before the actual movement in a given turn, or units might both move and try to do things
	 * on the same turn, leading to gliding units.
	 */
	 /**
	   * 警告其他组件，我们当前的移动受阻（即，我们本回合移动失败）。
	   * 这应该只在给定回合的实际移动之前调用，否则单位可能会在同一回合内既移动又尝试做其他事情，
	   * 导致单位滑行。
	   */
	void MoveObstructed()
	{
		// Don't notify if we are a formation member in a moving formation - we can occasionally be stuck for a long time
		// if our current offset is unreachable, but we don't want to end up stuck.
		// (If the formation controller has stopped moving however, we can safely message).
		// 如果我们是在移动阵型中的阵型成员，则不通知 - 如果我们当前的偏移位置无法到达，我们有时会卡住很长时间，
		// 但我们不希望最终卡住。
		// （然而，如果阵型控制器已停止移动，我们可以安全地发送消息）。
		if (IsFormationMember() && IsFormationControllerMoving())
			return;

		CMessageMotionUpdate msg(m_FailedMovements >= VERY_OBSTRUCTED_THRESHOLD ?
			CMessageMotionUpdate::VERY_OBSTRUCTED : CMessageMotionUpdate::OBSTRUCTED);
		GetSimContext().GetComponentManager().PostMessage(GetEntityId(), msg);
	}

	/**
	 * Increment the number of failed movements and notify other components if required.
	 * @returns true if the failure was notified, false otherwise.
	 */
	 /**
	   * 增加移动失败次数，并在需要时通知其他组件。
	   * @return 如果失败被通知，则返回 true，否则返回 false。
	   */
	bool IncrementFailedMovementsAndMaybeNotify()
	{
		m_FailedMovements++;
		if (m_FailedMovements >= MAX_FAILED_MOVEMENTS)
		{
			MoveFailed();
			m_FailedMovements = 0;
			return true;
		}
		return false;
	}

	/**
	 * If path would take us farther away from the goal than pos currently is, return false, else return true.
	 */
	 /**
	   * 如果路径会让我们比当前位置离目标更远，则返回 false，否则返回 true。
	   */
	bool RejectFartherPaths(const PathGoal& goal, const WaypointPath& path, const CFixedVector2D& pos) const;

	// 检查是否应该交替使用不同的寻路器
	bool ShouldAlternatePathfinder() const
	{
		return (m_FailedMovements == ALTERNATE_PATH_TYPE_DELAY) || ((MAX_FAILED_MOVEMENTS - ALTERNATE_PATH_TYPE_DELAY) % ALTERNATE_PATH_TYPE_EVERY == 0);
	}

	// 检查目标是否在短路径寻路范围内
	bool InShortPathRange(const PathGoal& goal, const CFixedVector2D& pos) const
	{
		return goal.DistanceToPoint(pos) < LONG_PATH_MIN_DIST;
	}

	// 计算短路径的搜索范围
	entity_pos_t ShortPathSearchRange() const
	{
		u8 multiple = m_FailedMovements < SHORT_PATH_SEARCH_RANGE_INCREASE_DELAY ? 0 : m_FailedMovements - SHORT_PATH_SEARCH_RANGE_INCREASE_DELAY;
		fixed searchRange = SHORT_PATH_MIN_SEARCH_RANGE + SHORT_PATH_SEARCH_RANGE_INCREMENT * multiple;
		if (searchRange > SHORT_PATH_MAX_SEARCH_RANGE)
			searchRange = SHORT_PATH_MAX_SEARCH_RANGE;
		return searchRange;
	}

	/**
	 * Handle the result of an asynchronous path query.
	 */
	 /**
	   * 处理异步路径查询的结果。
	   */
	void PathResult(u32 ticket, const WaypointPath& path);

	// 当数值被修改时调用（如科技升级）
	void OnValueModification()
	{
		CmpPtr<ICmpValueModificationManager> cmpValueModificationManager(GetSystemEntity());
		if (!cmpValueModificationManager)
			return;

		m_WalkSpeed = cmpValueModificationManager->ApplyModifications(L"UnitMotion/WalkSpeed", m_TemplateWalkSpeed, GetEntityId());
		m_RunMultiplier = cmpValueModificationManager->ApplyModifications(L"UnitMotion/RunMultiplier", m_TemplateRunMultiplier, GetEntityId());

		// For MT_Deserialize compute m_Speed from the serialized m_SpeedMultiplier.
		// For MT_ValueModification and MT_OwnershipChanged, adjust m_SpeedMultiplier if needed
		// (in case then new m_RunMultiplier value is lower than the old).
		// 对于 MT_Deserialize，从序列化的 m_SpeedMultiplier 计算 m_Speed。
		// 对于 MT_ValueModification 和 MT_OwnershipChanged，如果需要，调整 m_SpeedMultiplier
		// （以防新的 m_RunMultiplier 值低于旧值）。
		SetSpeedMultiplier(m_SpeedMultiplier);
	}

	/**
	 * Check if we are at destination early in the turn, this both lets units react faster
	 * and ensure that distance comparisons are done while units are not being moved
	 * (otherwise they won't be commutative).
	 */
	 /**
	   * 在回合开始时检查我们是否到达目的地，这既能让单位更快地反应，
	   * 也确保距离比较是在单位没有移动时进行的
	   * （否则它们将不满足交换律）。
	   */
	void OnTurnStart();

	// 移动前的准备工作
	void PreMove(CCmpUnitMotionManager::MotionState& state);
	// 执行移动
	void Move(CCmpUnitMotionManager::MotionState& state, fixed dt);
	// 移动后的收尾工作
	void PostMove(CCmpUnitMotionManager::MotionState& state, fixed dt);

	// 检查是否可能已到达目的地
	bool PossiblyAtDestination() const override;

	/**
	 * Process the move the unit will do this turn.
	 * This does not send actually change the position.
	 * @returns true if the move was obstructed.
	 */
	 /**
	   * 处理单位本回合要执行的移动。
	   * 这并不会实际改变位置。
	   * @return 如果移动受阻，则返回 true。
	   */
	bool PerformMove(fixed dt, const fixed& turnRate, WaypointPath& shortPath, WaypointPath& longPath, CFixedVector2D& pos, fixed& speed, entity_angle_t& angle, uint8_t pushingPressure) const;

	/**
	 * Update other components on our speed.
	 * (For performance, this should try to avoid sending messages).
	 */
	 /**
	   * 更新其他组件关于我们速度的信息。
	   * （为性能考虑，应尽量避免发送消息）。
	   */
	void UpdateMovementState(entity_pos_t speed, entity_pos_t meanSpeed);

	/**
	 * React if our move was obstructed.
	 * @param moved - true if the unit still managed to move.
	 * @returns true if the obstruction required handling, false otherwise.
	 */
	 /**
	   * 如果我们的移动受阻，则作出反应。
	   * @param moved - 如果单位仍然设法移动了，则为 true。
	   * @return 如果障碍需要处理，则返回 true，否则返回 false。
	   */
	bool HandleObstructedMove(bool moved);

	/**
	 * Returns true if the target position is valid. False otherwise.
	 * (this may indicate that the target is e.g. out of the world/dead).
	 * NB: for code-writing convenience, if we have no target, this returns true.
	 */
	 /**
	   * 如果目标位置有效，则返回 true。否则返回 false。
	   * （这可能表示目标例如在世界之外/已死亡）。
	   * 注意：为方便编码，如果我们没有目标，此函数返回 true。
	   */
	bool TargetHasValidPosition(const MoveRequest& moveRequest) const;
	bool TargetHasValidPosition() const
	{
		return TargetHasValidPosition(m_MoveRequest);
	}

	/**
	 * Computes the current location of our target entity (plus offset).
	 * Returns false if no target entity or no valid position.
	 */
	 /**
	   * 计算我们目标实体（加上偏移量）的当前位置。
	   * 如果没有目标实体或没有有效位置，则返回 false。
	   */
	bool ComputeTargetPosition(CFixedVector2D& out, const MoveRequest& moveRequest) const;
	bool ComputeTargetPosition(CFixedVector2D& out) const
	{
		return ComputeTargetPosition(out, m_MoveRequest);
	}

	/**
	 * Attempts to replace the current path with a straight line to the target,
	 * if it's close enough and the route is not obstructed.
	 */
	 /**
	   * 尝试用一条直线路径替换当前路径以到达目标，
	   * 如果距离足够近且路线没有受阻。
	   */
	bool TryGoingStraightToTarget(const CFixedVector2D& from, bool updatePaths);

	/**
	 * Returns whether our we need to recompute a path to reach our target.
	 */
	 /**
	   * 返回我们是否需要重新计算路径以到达目标。
	   */
	bool PathingUpdateNeeded(const CFixedVector2D& from) const;

	/**
	 * Rotate to face towards the target point, given the current pos
	 */
	 /**
	   * 根据当前位置，旋转以朝向目标点。
	   */
	void FaceTowardsPointFromPos(const CFixedVector2D& pos, entity_pos_t x, entity_pos_t z);

	/**
	 * Units in 'pushing' mode are marked as 'moving' in the obstruction manager.
	 * Units in 'pushing' mode should skip them in checkMovement (to enable pushing).
	 * However, units for which pushing is deactivated should collide against everyone.
	 * Units that don't block movement never participate in pushing, but they also
	 * shouldn't collide with pushing units.
	 */
	 /**
	   * 处于“推挤”模式的单位在障碍物管理器中被标记为“移动中”。
	   * 处于“推挤”模式的单位在 checkMovement 中应跳过它们（以启用推挤）。
	   * 然而，推挤被禁用的单位应该与所有单位发生碰撞。
	   * 不阻挡移动的单位从不参与推挤，但它们也
	   * 不应与推挤中的单位碰撞。
	   */
	bool ShouldCollideWithMovingUnits() const
	{
		return !m_Pushing && m_BlockMovement;
	}

	/**
	 * Returns an appropriate obstruction filter for use with path requests.
	 */
	 /**
	   * 返回一个适合用于路径请求的障碍物过滤器。
	   */
	ControlGroupMovementObstructionFilter GetObstructionFilter() const
	{
		return ControlGroupMovementObstructionFilter(ShouldCollideWithMovingUnits(), GetGroup());
	}
	/**
	 * Filter a specific tag on top of the existing control groups.
	 */
	 /**
	   * 在现有控制组的基础上，过滤一个特定的标签。
	   */
	SkipTagAndControlGroupObstructionFilter GetObstructionFilter(const ICmpObstructionManager::tag_t& tag) const
	{
		return SkipTagAndControlGroupObstructionFilter(tag, ShouldCollideWithMovingUnits(), GetGroup());
	}

	/**
	 * Decide whether to approximate the given range from a square target as a circle,
	 * rather than as a square.
	 */
	 /**
	   * 决定是否将给定范围内的方形目标近似为圆形，
	   * 而不是方形。
	   */
	bool ShouldTreatTargetAsCircle(entity_pos_t range, entity_pos_t circleRadius) const;

	/**
	 * Create a PathGoal from a move request.
	 * @returns true if the goal was successfully created.
	 */
	 /**
	   * 从移动请求创建一个 PathGoal。
	   * @return 如果目标成功创建，则返回 true。
	   */
	bool ComputeGoal(PathGoal& out, const MoveRequest& moveRequest) const;

	/**
	 * Compute a path to the given goal from the given position.
	 * Might go in a straight line immediately, or might start an asynchronous path request.
	 */
	 /**
	   * 从给定位置计算到给定目标的路径。
	   * 可能会立即走直线，或者可能会启动一个异步路径请求。
	   */
	void ComputePathToGoal(const CFixedVector2D& from, const PathGoal& goal);

	/**
	 * Start an asynchronous long path query.
	 */
	 /**
	   * 启动一个异步的长程路径查询。
	   */
	void RequestLongPath(const CFixedVector2D& from, const PathGoal& goal);

	/**
	 * Start an asynchronous short path query.
	 * @param extendRange - if true, extend the search range to at least the distance to the goal.
	 */
	 /**
	   * 启动一个异步的短程路径查询。
	   * @param extendRange - 如果为 true，则将搜索范围扩展到至少与目标的距离。
	   */
	void RequestShortPath(const CFixedVector2D& from, const PathGoal& goal, bool extendRange);

	/**
	 * General handler for MoveTo interface functions.
	 */
	 /**
	   * MoveTo 接口函数的通用处理器。
	   */
	bool MoveTo(MoveRequest request);

	/**
	 * Convert a path into a renderable list of lines
	 */
	 /**
	   * 将路径转换为可渲染的线条列表。
	   */
	void RenderPath(const WaypointPath& path, std::vector<SOverlayLine>& lines, CColor color);

	// 提交渲染数据
	void RenderSubmit(SceneCollector& collector);
};

// 注册 UnitMotion 组件类型
REGISTER_COMPONENT_TYPE(UnitMotion)

// 如果新路径会使单位离目标更远，则拒绝该路径
bool CCmpUnitMotion::RejectFartherPaths(const PathGoal& goal, const WaypointPath& path, const CFixedVector2D& pos) const
{
	if (path.m_Waypoints.empty())
		return false;

	// Reject the new path if it does not lead us closer to the target's position.
	// 如果新路径不会让我们更接近目标位置，则拒绝它。
	if (goal.DistanceToPoint(pos) <= goal.DistanceToPoint(CFixedVector2D(path.m_Waypoints.front().x, path.m_Waypoints.front().z)))
		return true;

	return false;
}

// 处理异步寻路查询的结果
void CCmpUnitMotion::PathResult(u32 ticket, const WaypointPath& path)
{
	// Ignore obsolete path requests
	// 忽略过时的路径请求
	if (ticket != m_ExpectedPathTicket.m_Ticket || m_MoveRequest.m_Type == MoveRequest::NONE)
		return;

	Ticket::Type ticketType = m_ExpectedPathTicket.m_Type;
	m_ExpectedPathTicket.clear();

	// If we not longer have a position, we won't be able to do much.
	// Fail in the next Move() call.
	// 如果我们不再有位置，就做不了什么了。
	// 在下一次 Move() 调用中失败。
	CmpPtr<ICmpPosition> cmpPosition(GetEntityHandle());
	if (!cmpPosition || !cmpPosition->IsInWorld())
		return;
	CFixedVector2D pos = cmpPosition->GetPosition2D();

	// Assume all long paths were towards the goal, and assume short paths were if there are no long waypoints.
	// 假设所有长路径都是朝向目标的，并假设如果没有长路径航点，短路径也是朝向目标的。
	bool pathedTowardsGoal = ticketType == Ticket::LONG_PATH || m_LongPath.m_Waypoints.empty();

	// Check if we need to run the short-path hack (warning: tricky control flow).
	// 检查是否需要运行短路径修正（警告：复杂的控制流）。
	bool shortPathHack = false;
	if (path.m_Waypoints.empty())
	{
		// No waypoints means pathing failed. If this was a long-path, try the short-path hack.
		// 没有航点意味着寻路失败。如果这是一个长路径，尝试短路径修正。
		if (!pathedTowardsGoal)
			return;
		shortPathHack = ticketType == Ticket::LONG_PATH;
	}
	else if (PathGoal goal; pathedTowardsGoal && ComputeGoal(goal, m_MoveRequest) && RejectFartherPaths(goal, path, pos))
	{
		// Reject paths that would take the unit further away from the goal.
		// This assumes that we prefer being closer 'as the crow flies' to unreachable goals.
		// This is a hack of sorts around units 'dancing' between two positions (see e.g. #3144),
		// but never actually failing to move, ergo never actually informing unitAI that it succeeds/fails.
		// (for short paths, only do so if aiming directly for the goal
		// as sub-goals may be farther than we are).
		// 拒绝会让单位离目标更远的路径。
		// 这假设我们更倾向于“直线距离”上更接近无法到达的目标。
		// 这是一种权宜之计，用于解决单位在两个位置之间“跳舞”的问题（参见 #3144），
		// 但从未真正移动失败，因此也从未通知 unitAI 成功或失败。
		// （对于短路径，只有在直接瞄准目标时才这样做，
		// 因为子目标可能比我们现在的位置更远）。

		// If this was a long-path and we no longer have waypoints, try the short-path hack.
		// 如果这是一个长路径并且我们不再有航点，尝试短路径修正。
		if (!m_LongPath.m_Waypoints.empty())
			return;
		shortPathHack = ticketType == Ticket::LONG_PATH;
	}

	// Short-path hack: if the long-range pathfinder doesn't find an acceptable path, push a fake waypoint at the goal.
	// This means HandleObstructedMove will use the short-pathfinder to try and reach it,
	// and that may find a path as the vertex pathfinder is more precise.
	// 短路径修正：如果长程寻路器找不到可接受的路径，就在目标位置推入一个假的航点。
	// 这意味着 HandleObstructedMove 将使用短程寻路器尝试到达它，
	// 这可能会找到一条路径，因为顶点寻路器更精确。
	if (shortPathHack)
	{
		// If we're resorting to the short-path hack, the situation is dire. Most likely, the goal is unreachable.
		// We want to find a path or fail fast. Bump failed movements so the short pathfinder will run at max-range
		// right away. This is safe from a performance PoV because it can only happen if the target is unreachable to
		// the long-range pathfinder, which is rare, and since the entity will fail to move if the goal is actually unreachable,
		// the failed movements will be increased to MAX anyways, so just shortcut.
		// 如果我们求助于短路径修正，情况就很糟糕了。目标很可能无法到达。
		// 我们想要找到一条路径或快速失败。增加失败移动次数，这样短程寻路器就会立即以最大范围运行。
		// 从性能角度看这是安全的，因为它只在目标对长程寻路器不可达时发生，这种情况很少见，
		// 而且如果目标确实无法到达，实体将移动失败，失败移动次数无论如何都会增加到最大值，所以这里只是一个捷径。
		m_FailedMovements = MAX_FAILED_MOVEMENTS - 2;

		CFixedVector2D targetPos;
		if (ComputeTargetPosition(targetPos))
			m_LongPath.m_Waypoints.emplace_back(Waypoint{ targetPos.X, targetPos.Y });
		return;
	}

	if (ticketType == Ticket::LONG_PATH)
	{
		m_LongPath = path;
		// Long paths don't properly follow diagonals because of JPS/the grid. Since units now take time turning,
		// they can actually slow down substantially if they have to do a one navcell diagonal movement,
		// which is somewhat common at the beginning of a new path.
		// For that reason, if the first waypoint is really close, check if we can't go directly to the second.
		// 由于 JPS/网格的原因，长路径不能很好地跟随对角线。由于单位现在转向需要时间，
		// 如果它们必须进行一个导航单元的对角线移动，它们实际上会大幅减速，
		// 这在新路径的开始部分相当常见。
		// 因此，如果第一个航点非常近，检查我们是否可以直接去第二个。
		if (m_LongPath.m_Waypoints.size() >= 2)
		{
			const Waypoint& firstWpt = m_LongPath.m_Waypoints.back();
			if (CFixedVector2D(firstWpt.x - pos.X, firstWpt.z - pos.Y).CompareLength(Pathfinding::NAVCELL_SIZE * 4) <= 0)
			{
				CmpPtr<ICmpPathfinder> cmpPathfinder(GetSystemEntity());
				ENSURE(cmpPathfinder);
				const Waypoint& secondWpt = m_LongPath.m_Waypoints[m_LongPath.m_Waypoints.size() - 2];
				if (cmpPathfinder->CheckMovement(GetObstructionFilter(), pos.X, pos.Y, secondWpt.x, secondWpt.z, m_Clearance, m_PassClass))
					m_LongPath.m_Waypoints.pop_back();
			}

		}
	}
	else
		m_ShortPath = path;

	m_FollowKnownImperfectPathCountdown = 0;

	if (!pathedTowardsGoal)
		return;

	// Performance hack: If we were pathing towards the goal and this new path won't put us in range,
	// it's highly likely that we are going somewhere unreachable.
	// However, Move() will try to recompute the path every turn, which can be quite slow.
	// To avoid this, act as if our current path leads us to the correct destination.
	// NB: for short-paths, the problem might be that the search space is too small
	// but we'll still follow this path until the en and try again then.
	// Because we reject farther paths, it works out.
	// 性能修正：如果我们正在朝目标寻路，而这条新路径无法将我们带入范围，
	// 那么我们很可能正前往一个无法到达的地方。
	// 然而，Move() 会尝试在每个回合重新计算路径，这可能非常慢。
	// 为避免这种情况，假装我们当前的路径能将我们带到正确的目的地。
	// 注意：对于短路径，问题可能在于搜索空间太小，
	// 但我们仍会沿着这条路走到终点，然后再试一次。
	// 因为我们拒绝了更远的路径，所以这样做是可行的。
	if (PathingUpdateNeeded(pos))
	{
		// Inform other components early, as they might have better behaviour than waiting for the path to carry out.
		// Send OBSTRUCTED at first - moveFailed is likely to trigger path recomputation and we might end up
		// recomputing too often for nothing.
		// 尽早通知其他组件，因为它们可能有比等待路径执行更好的行为。
		// 首先发送 OBSTRUCTED - moveFailed 可能会触发路径重新计算，我们可能会因此
		// 过于频繁地进行无用的重计算。
		if (!IncrementFailedMovementsAndMaybeNotify())
			MoveObstructed();
		// We'll automatically recompute a path when this reaches 0, as a way to improve behaviour.
		// (See D665 - this is needed because the target may be moving, and we should adjust to that).
		// 当这个倒计时到 0 时，我们将自动重新计算路径，作为改善行为的一种方式。
		// (参见 D665 - 这是必要的，因为目标可能在移动，我们应该适应它)。
		m_FollowKnownImperfectPathCountdown = KNOWN_IMPERFECT_PATH_RESET_COUNTDOWN;
	}
}

// 在回合开始时调用
void CCmpUnitMotion::OnTurnStart()
{
	if (PossiblyAtDestination())
		MoveSucceeded();
	else if (!TargetHasValidPosition())
	{
		// Scrap waypoints - we don't know where to go.
		// If the move request remains unchanged and the target again has a valid position later on,
		// moving will be resumed.
		// Units may want to move to move to the target's last known position,
		// but that should be decided by UnitAI (handling MoveFailed), not UnitMotion.
		// 放弃航点 - 我们不知道该去哪里。
		// 如果移动请求保持不变并且目标稍后再次具有有效位置，
		// 移动将恢复。
		// 单位可能想要移动到目标的最后一个已知位置，
		// 但这应该由 UnitAI（处理 MoveFailed）决定，而不是 UnitMotion。
		m_LongPath.m_Waypoints.clear();
		m_ShortPath.m_Waypoints.clear();

		MoveFailed();
	}
}

// 移动前准备
void CCmpUnitMotion::PreMove(CCmpUnitMotionManager::MotionState& state)
{
	state.ignore = !m_Pushing || !m_BlockMovement;

	state.wasObstructed = false;
	state.wentStraight = false;

	// If we were idle and will still be, no need for an update.
	// 如果我们之前是空闲的，并且将继续空闲，则无需更新。
	state.needUpdate = state.cmpPosition->IsInWorld() &&
		(m_CurrentSpeed != fixed::Zero() || m_LastTurnSpeed != fixed::Zero() || m_MoveRequest.m_Type != MoveRequest::NONE);

	if (!m_BlockMovement)
		return;

	state.controlGroup = IsFormationMember() ? m_FormationController : INVALID_ENTITY;

	// Update moving flag, this is an internal construct used for pushing,
	// so it does not really reflect whether the unit is actually moving or not.
	// 更新移动标志，这是一个用于推挤的内部构造，
	// 因此它并不真正反映单位是否实际在移动。
	state.isMoving = m_Pushing && m_MoveRequest.m_Type != MoveRequest::NONE;
	CmpPtr<ICmpObstruction> cmpObstruction(GetEntityHandle());
	if (cmpObstruction)
		cmpObstruction->SetMovingFlag(state.isMoving);
}

// 执行移动逻辑
void CCmpUnitMotion::Move(CCmpUnitMotionManager::MotionState& state, fixed dt)
{
	PROFILE("Move");

	// If we're chasing a potentially-moving unit and are currently close
	// enough to its current position, and we can head in a straight line
	// to it, then throw away our current path and go straight to it.
	// 如果我们正在追逐一个可能移动的单位，并且当前离它的位置足够近，
	// 而且我们可以直线朝它前进，那么就丢弃我们当前的路径，直接朝它走。
	state.wentStraight = TryGoingStraightToTarget(state.initialPos, true);

	state.wasObstructed = PerformMove(dt, state.cmpPosition->GetTurnRate(), m_ShortPath, m_LongPath, state.pos, state.speed, state.angle, state.pushingPressure);
}

// 移动后处理
void CCmpUnitMotion::PostMove(CCmpUnitMotionManager::MotionState& state, fixed dt)
{
	// Update our speed over this turn so that the visual actor shows the correct animation.
	// 更新我们在这回合的速度，以便视觉角色显示正确的动画。
	if (state.pos == state.initialPos)
	{
		if (state.angle != state.initialAngle)
			state.cmpPosition->TurnTo(state.angle);
		UpdateMovementState(fixed::Zero(), fixed::Zero());
	}
	else
	{
		// Update the Position component after our movement (if we actually moved anywhere)
		// 在我们移动后更新位置组件（如果我们确实移动了）。
		CFixedVector2D offset = state.pos - state.initialPos;
		state.cmpPosition->MoveAndTurnTo(state.pos.X, state.pos.Y, state.angle);

		// Calculate the mean speed over this past turn.
		// 计算过去这回合的平均速度。
		UpdateMovementState(state.speed, offset.Length() / dt);
	}

	if (state.wasObstructed && HandleObstructedMove(state.pos != state.initialPos))
		return;
	else if (!state.wasObstructed && state.pos != state.initialPos)
		m_FailedMovements = 0;

	const bool needPathUpdate{ PathingUpdateNeeded(state.pos) };

	// If we're following a long-path, check if we might run into units in advance, to smoothe motion.
	// 如果我们正在跟随一条长路径，提前检查是否会遇到单位，以平滑运动。
	if (!needPathUpdate && !state.wasObstructed && m_LongPath.m_Waypoints.size() >= 1 && m_ShortPath.m_Waypoints.empty())
	{
		ICmpObstructionManager::tag_t specificIgnore;
		if (m_MoveRequest.m_Type == MoveRequest::ENTITY)
		{
			CmpPtr<ICmpObstruction> cmpTargetObstruction(GetSimContext(), m_MoveRequest.m_Entity);
			if (cmpTargetObstruction)
				specificIgnore = cmpTargetObstruction->GetObstruction();
		}
		CmpPtr<ICmpObstructionManager> cmpObstructionManager(GetSystemEntity());
		if (cmpObstructionManager && cmpObstructionManager->TestUnitLine(GetObstructionFilter(specificIgnore),
			state.pos.X, state.pos.Y, m_LongPath.m_Waypoints.back().x, m_LongPath.m_Waypoints.back().z, m_Clearance, true))
		{
			// We will run into something: request a new short path.
			// If we have several waypoints left, aim for the one after next directly.
			// (This is mostly because the obstruction might be at the waypoint, and the end is kind of treated specially).
			// Else just path to the goal.
			// 我们会撞上东西：请求一条新的短路径。
			// 如果我们还剩下几个航点，直接瞄准下一个航点之后的那个。
			// （这主要是因为障碍物可能就在航点上，而终点有点特殊处理）。
			// 否则就直接寻路到目标。
			if (m_LongPath.m_Waypoints.size() > 1) {
				fixed radius = Pathfinding::NAVCELL_SIZE * 2;
				m_LongPath.m_Waypoints.pop_back();
				PathGoal subgoal = { PathGoal::CIRCLE, m_LongPath.m_Waypoints.back().x, m_LongPath.m_Waypoints.back().z, radius };
				RequestShortPath(state.pos, subgoal, false);
			}
			else {
				// If we only have one waypoint left, request a short path to the waypoint itself.
				// 如果我们只剩下一个航点，请求一条到该航点本身的短路径。
				PathGoal goal;
				if (ComputeGoal(goal, m_MoveRequest))
					RequestShortPath(state.pos, goal, false);
			}
		}
	}

	// If we moved straight, and didn't quite finish the path, reset - we'll update it next turn if still OK.
	// 如果我们直线移动了，但没有完全走完路径，就重置 - 如果下一回合仍然可以，我们会更新它。
	if (state.wentStraight && !state.wasObstructed)
		m_ShortPath.m_Waypoints.clear();

	// We may need to recompute our path sometimes (e.g. if our target moves).
	// Since we request paths asynchronously anyways, this does not need to be done before moving.
	// 我们有时可能需要重新计算路径（例如，如果我们的目标移动了）。
	// 因为我们无论如何都是异步请求路径，所以这不需要在移动之前完成。
	if (!state.wentStraight && needPathUpdate)
	{
		PathGoal goal;
		if (ComputeGoal(goal, m_MoveRequest))
			ComputePathToGoal(state.pos, goal);
	}
	else if (m_FollowKnownImperfectPathCountdown > 0)
		--m_FollowKnownImperfectPathCountdown;
}

// 检查是否可能已在目的地
bool CCmpUnitMotion::PossiblyAtDestination() const
{
	if (m_MoveRequest.m_Type == MoveRequest::NONE)
		return false;

	CmpPtr<ICmpObstructionManager> cmpObstructionManager(GetSystemEntity());
	ENSURE(cmpObstructionManager);

	if (m_MoveRequest.m_Type == MoveRequest::POINT)
		return cmpObstructionManager->IsInPointRange(GetEntityId(), m_MoveRequest.m_Position.X, m_MoveRequest.m_Position.Y, m_MoveRequest.m_MinRange, m_MoveRequest.m_MaxRange, false);
	if (m_MoveRequest.m_Type == MoveRequest::ENTITY)
		return cmpObstructionManager->IsInTargetRange(GetEntityId(), m_MoveRequest.m_Entity, m_MoveRequest.m_MinRange, m_MoveRequest.m_MaxRange, false);
	if (m_MoveRequest.m_Type == MoveRequest::OFFSET)
	{
		CmpPtr<ICmpUnitMotion> cmpControllerMotion(GetSimContext(), m_MoveRequest.m_Entity);
		if (cmpControllerMotion && cmpControllerMotion->IsMoveRequested())
			return false;

		// In formation, return a match only if we are exactly at the target position.
		// Otherwise, units can go in an infinite "walzting" loop when the Idle formation timer
		// reforms them.
		// 在阵型中，只有当我们精确地在目标位置时才返回匹配。
		// 否则，当空闲阵型计时器重新组织它们时，单位可能会陷入无限的“华尔兹”循环。
		CFixedVector2D targetPos;
		ComputeTargetPosition(targetPos);
		CmpPtr<ICmpPosition> cmpPosition(GetEntityHandle());
		return (targetPos - cmpPosition->GetPosition2D()).CompareLength(fixed::Zero()) <= 0;
	}
	return false;
}

// 执行移动的核心逻辑
bool CCmpUnitMotion::PerformMove(fixed dt, const fixed& turnRate, WaypointPath& shortPath, WaypointPath& longPath, CFixedVector2D& pos, fixed& speed, entity_angle_t& angle, uint8_t pushingPressure) const
{
	// If there are no waypoint, behave as though we were obstructed and let HandleObstructedMove handle it.
	// 如果没有航点，表现得就像我们被阻塞了一样，让 HandleObstructedMove 来处理。
	if (shortPath.m_Waypoints.empty() && longPath.m_Waypoints.empty())
		return true;

	// Wrap the angle to (-Pi, Pi].
	// 将角度规整到 (-Pi, Pi] 范围内。
	while (angle > entity_angle_t::Pi())
		angle -= entity_angle_t::Pi() * 2;
	while (angle < -entity_angle_t::Pi())
		angle += entity_angle_t::Pi() * 2;

	CmpPtr<ICmpPathfinder> cmpPathfinder(GetSystemEntity());
	ENSURE(cmpPathfinder);

	fixed basicSpeed = m_Speed;
	// If in formation, run to keep up; otherwise just walk.
	// 如果在阵型中，跑起来跟上；否则就走。
	if (IsMovingAsFormation())
		basicSpeed = m_Speed.Multiply(m_RunMultiplier);

	// If pushing pressure is applied, slow the unit down.
	// 如果施加了推挤压力，减慢单位速度。
	if (pushingPressure)
	{
		// Values below this pressure don't slow the unit down (avoids slowing groups down).
		// 低于此压力的值不会减慢单位速度（避免减慢群体速度）。
		constexpr int pressureMinThreshold = 10;

		// Lower speed up to a floor to prevent units from getting stopped.
		// This helped pushing particularly for fast units, since they'll end up slowing down.
		// 将速度降低到一个下限，以防止单位停下来。
		// 这尤其有助于快速单位的推挤，因为它们最终会减速。
		constexpr int maxPressure = CCmpUnitMotionManager::MAX_PRESSURE - pressureMinThreshold - 80;
		constexpr entity_pos_t floorSpeed = entity_pos_t::FromFraction(3, 2);
		static_assert(maxPressure > 0);

		uint8_t slowdown = maxPressure - std::min(maxPressure, std::max(0, pushingPressure - pressureMinThreshold));
		basicSpeed = basicSpeed.Multiply(fixed::FromInt(slowdown) / maxPressure);
		// NB: lowering this too much will make the units behave a lot like viscous fluid
		// when the density becomes extreme. While perhaps realistic (and kind of neat),
		// it's not very helpful for gameplay. Empirically, a value of 1.5 avoids most of the effect
		// while still slowing down movement significantly, and seems like a good balance.
		// Min with the template speed to allow units that are explicitly absurdly slow.
		// 注意：把这个值降得太低会使单位在密度变得极高时表现得非常像粘性流体。
		// 虽然也许很真实（而且有点酷），但对游戏性没什么帮助。根据经验，1.5 的值可以避免大部分这种效果，
		// 同时仍然显著减慢移动速度，看起来是一个很好的平衡点。
		// 与模板速度取最小值，以允许那些明确设定为极慢的单位。
		basicSpeed = std::max(std::min(m_TemplateWalkSpeed, floorSpeed), basicSpeed);
	}

	// TODO: would be nice to support terrain-dependent speed again.
	// 待办事项：如果能再次支持依赖地形的速度就好了。
	fixed maxSpeed = basicSpeed;

	fixed timeLeft = dt;
	fixed zero = fixed::Zero();

	ICmpObstructionManager::tag_t specificIgnore;
	if (m_MoveRequest.m_Type == MoveRequest::ENTITY)
	{
		CmpPtr<ICmpObstruction> cmpTargetObstruction(GetSimContext(), m_MoveRequest.m_Entity);
		if (cmpTargetObstruction)
			specificIgnore = cmpTargetObstruction->GetObstruction();
	}

	while (timeLeft > zero)
	{
		// If we ran out of path, we have to stop.
		// 如果我们的路径走完了，就必须停下来。
		if (shortPath.m_Waypoints.empty() && longPath.m_Waypoints.empty())
			break;

		CFixedVector2D target;
		if (shortPath.m_Waypoints.empty())
			target = CFixedVector2D(longPath.m_Waypoints.back().x, longPath.m_Waypoints.back().z);
		else
			target = CFixedVector2D(shortPath.m_Waypoints.back().x, shortPath.m_Waypoints.back().z);

		CFixedVector2D offset = target - pos;

		if (turnRate > zero && !offset.IsZero())
		{
			fixed angleDiff = angle - atan2_approx(offset.X, offset.Y);
			fixed absoluteAngleDiff = angleDiff.Absolute();
			if (absoluteAngleDiff > entity_angle_t::Pi())
				absoluteAngleDiff = entity_angle_t::Pi() * 2 - absoluteAngleDiff;

			// We only rotate to the instantTurnAngle angle. The rest we rotate during movement.
			// 我们只旋转到 instantTurnAngle 角度。剩下的我们在移动中旋转。
			if (absoluteAngleDiff > m_InstantTurnAngle)
			{
				// Stop moving when rotating this far.
				// 转动这么远时停止移动。
				speed = zero;

				fixed maxRotation = turnRate.Multiply(timeLeft);

				// Figure out whether rotating will increase or decrease the angle, and how far we need to rotate in that direction.
				// 确定旋转会增加还是减少角度，以及我们需要朝那个方向旋转多远。
				int direction = (entity_angle_t::Zero() < angleDiff && angleDiff <= entity_angle_t::Pi()) || angleDiff < -entity_angle_t::Pi() ? -1 : 1;

				// Can't rotate far enough, just rotate in the correct direction.
				// 不能旋转足够远，就只朝正确的方向旋转。
				if (absoluteAngleDiff - m_InstantTurnAngle > maxRotation)
				{
					angle += maxRotation * direction;
					if (angle * direction > entity_angle_t::Pi())
						angle -= entity_angle_t::Pi() * 2 * direction;
					break;
				}
				// Rotate towards the next waypoint and continue moving.
				// 朝下一个航点旋转并继续移动。
				angle = atan2_approx(offset.X, offset.Y);
				timeLeft = std::min(maxRotation, maxRotation - absoluteAngleDiff + m_InstantTurnAngle) / turnRate;
			}
			else
			{
				// Modify the speed depending on the angle difference.
				// 根据角度差修改速度。
				fixed sin, cos;
				sincos_approx(angleDiff, sin, cos);
				speed = speed.Multiply(cos);
				angle = atan2_approx(offset.X, offset.Y);
			}
		}

		// Work out how far we can travel in timeLeft.
		// 计算出在 timeLeft 时间内我们能走多远。
		fixed accelTime = std::min(timeLeft, (maxSpeed - speed) / m_Acceleration);
		fixed accelDist = speed.Multiply(accelTime) + accelTime.Square().Multiply(m_Acceleration) / 2;
		fixed maxdist = accelDist + maxSpeed.Multiply(timeLeft - accelTime);

		// If the target is close, we can move there directly.
		// 如果目标很近，我们可以直接移动到那里。
		fixed offsetLength = offset.Length();
		if (offsetLength <= maxdist)
		{
			if (cmpPathfinder->CheckMovement(GetObstructionFilter(specificIgnore), pos.X, pos.Y, target.X, target.Y, m_Clearance, m_PassClass))
			{
				pos = target;

				// Spend the rest of the time heading towards the next waypoint.
				// Either we still need to accelerate after, or we have reached maxSpeed.
				// The former is much less likely than the latter: usually we can reach
				// maxSpeed within one waypoint. So the Sqrt is not too bad.
				// 把剩下的时间用来朝向下一个航点。
				// 要么之后我们还需要加速，要么我们已经达到了最大速度。
				// 前者比后者可能性小得多：通常我们可以在一个航点内达到最大速度。所以开方运算不算太糟。
				if (offsetLength <= accelDist)
				{
					fixed requiredTime = (-speed + (speed.Square() + offsetLength.Multiply(m_Acceleration).Multiply(fixed::FromInt(2))).Sqrt()) / m_Acceleration;
					timeLeft -= requiredTime;
					speed += m_Acceleration.Multiply(requiredTime);
				}
				else
				{
					timeLeft -= accelTime + (offsetLength - accelDist) / maxSpeed;
					speed = maxSpeed;
				}

				if (shortPath.m_Waypoints.empty())
					longPath.m_Waypoints.pop_back();
				else
					shortPath.m_Waypoints.pop_back();

				continue;
			}
			else
			{
				// Error - path was obstructed.
				// 错误 - 路径受阻。
				return true;
			}
		}
		else
		{
			// Not close enough, so just move in the right direction.
			// 不够近，所以只朝正确的方向移动。
			offset.Normalize(maxdist);
			target = pos + offset;

			speed = std::min(maxSpeed, speed + m_Acceleration.Multiply(timeLeft));

			if (cmpPathfinder->CheckMovement(GetObstructionFilter(specificIgnore), pos.X, pos.Y, target.X, target.Y, m_Clearance, m_PassClass))
				pos = target;
			else
				return true;

			break;
		}
	}
	return false;
}

// 更新移动状态（速度等），并通知视觉组件
void CCmpUnitMotion::UpdateMovementState(entity_pos_t speed, entity_pos_t meanSpeed)
{
	CmpPtr<ICmpVisual> cmpVisual(GetEntityHandle());
	if (cmpVisual)
	{
		if (meanSpeed == fixed::Zero())
			cmpVisual->SelectMovementAnimation("idle", fixed::FromInt(1));
		else
			cmpVisual->SelectMovementAnimation(meanSpeed > (m_WalkSpeed / 2).Multiply(m_RunMultiplier + fixed::FromInt(1)) ? "run" : "walk", meanSpeed);
	}

	m_LastTurnSpeed = meanSpeed;
	m_CurrentSpeed = speed;
}

// 处理移动受阻的情况
bool CCmpUnitMotion::HandleObstructedMove(bool moved)
{
	CmpPtr<ICmpPosition> cmpPosition(GetEntityHandle());
	if (!cmpPosition || !cmpPosition->IsInWorld())
		return false;

	// We failed to move, inform other components as they might handle it.
	// (don't send messages on the first failure, as that would be too noisy).
	// Also don't increment above the initial MoveObstructed message if we actually manage to move a little.
	// 我们移动失败了，通知其他组件，它们可能会处理。
	// （不要在第一次失败时发送消息，因为那样会太吵）。
	// 如果我们实际上设法移动了一点，也不要增加超过初始 MoveObstructed 消息的计数。
	if (!moved || m_FailedMovements < 2)
	{
		if (!IncrementFailedMovementsAndMaybeNotify() && m_FailedMovements >= 2)
			MoveObstructed();
	}

	PathGoal goal;
	if (!ComputeGoal(goal, m_MoveRequest))
		return false;

	// At this point we have a position in the world since ComputeGoal checked for that.
	// 此时，我们在世界中有一个位置，因为 ComputeGoal 已经检查过了。
	CFixedVector2D pos = cmpPosition->GetPosition2D();

	// Assume that we are merely obstructed and the long path is salvageable, so try going around the obstruction.
	// This could be a separate function, but it doesn't really make sense to call it outside of here, and I can't find a name.
	// I use an IIFE to have nice 'return' semantics still.
	// 假设我们只是被阻挡了，长路径是可挽救的，所以尝试绕过障碍物。
	// 这可以是一个独立的函数，但在别处调用它没什么意义，而且我找不到名字。
	// 我使用立即调用函数表达式（IIFE）来仍然有良好的 'return' 语义。
	if ([&]() -> bool {
		// If the goal is close enough, we should ignore any remaining long waypoint and just
		// short path there directly, as that improves behaviour in general - see D2095).
		// 如果目标足够近，我们应该忽略任何剩余的长路径航点，直接
		// 短路径到那里，因为这通常能改善行为 - 参见 D2095）。
		if (InShortPathRange(goal, pos))
			return false;

			// On rare occasions, when following a short path, we can end up in a position where
			// the short pathfinder thinks we are inside an obstruction (and can leave)
			// but the CheckMovement logic doesn't. I believe the cause is a small numerical difference
			// in their calculation, but haven't been able to pinpoint it precisely.
			// In those cases, the solution is to back away to prevent the short-pathfinder from being confused.
			// TODO: this should only be done if we're obstructed by a static entity.
			// 在极少数情况下，当沿着短路径行进时，我们可能会到达一个位置，
			// 短路径寻路器认为我们在障碍物内部（并且可以离开），
			// 但 CheckMovement 逻辑不这么认为。我相信原因是它们计算中的微小数值差异，
			// 但一直没能精确定位。
			// 在这些情况下，解决方案是后退，以防止短路径寻路器混淆。
			// 待办事项：这只应该在我们被静态实体阻挡时才做。
			if (!m_ShortPath.m_Waypoints.empty() && m_FailedMovements == BACKUP_HACK_DELAY)
			{
				Waypoint next = m_ShortPath.m_Waypoints.back();
				CFixedVector2D backUp(pos.X - next.x, pos.Y - next.z);
				backUp.Normalize();
				next.x = pos.X + backUp.X;
				next.z = pos.Y + backUp.Y;
				m_ShortPath.m_Waypoints.push_back(next);
				return true;
			}
		// Delete the next waypoint if it's reasonably close,
		// because it might be blocked by units and thus unreachable.
		// NB: this number is tricky. Make it too high, and units start going down dead ends, which looks odd (#5795)
		// Make it too low, and they might get stuck behind other obstructed entities.
		// It also has performance implications because it calls the short-pathfinder.
		// 如果下一个航点相当近，就删除它，
		// 因为它可能被单位阻挡而无法到达。
		// 注意：这个数字很棘手。设得太高，单位会开始走进死胡同，看起来很奇怪（#5795）
		// 设得太低，它们可能会被其他受阻实体卡住。
		// 它还会影响性能，因为它会调用短路径寻路器。
		fixed skipbeyond = std::max(ShortPathSearchRange() / 3, Pathfinding::NAVCELL_SIZE * 8);
		if (m_LongPath.m_Waypoints.size() > 1 &&
			(pos - CFixedVector2D(m_LongPath.m_Waypoints.back().x, m_LongPath.m_Waypoints.back().z)).CompareLength(skipbeyond) < 0)
		{
			m_LongPath.m_Waypoints.pop_back();
		}
		else if (ShouldAlternatePathfinder())
		{
			// Recompute the whole thing occasionally, in case we got stuck in a dead end from removing long waypoints.
			// 偶尔重新计算整个路径，以防我们因移除长路径航点而陷入死胡同。
			RequestLongPath(pos, goal);
			return true;
		}

		if (m_LongPath.m_Waypoints.empty())
			return false;

		// Compute a short path in the general vicinity of the next waypoint, to help pathfinding in crowds.
		// The goal here is to manage to move in the general direction of our target, not to be super accurate.
		// 在下一个航点的大致范围内计算一条短路径，以帮助在人群中寻路。
		// 这里的目标是设法朝我们目标的总方向移动，而不是要超级精确。
		fixed radius = Clamp(skipbeyond / 3, Pathfinding::NAVCELL_SIZE * 4, Pathfinding::NAVCELL_SIZE * 12);
		PathGoal subgoal = { PathGoal::CIRCLE, m_LongPath.m_Waypoints.back().x, m_LongPath.m_Waypoints.back().z, radius };
		RequestShortPath(pos, subgoal, false);
		return true;
		}()) return true;

	// If we couldn't use a workaround, try recomputing the entire path.
	// 如果我们无法使用权宜之计，尝试重新计算整个路径。
	ComputePathToGoal(pos, goal);

	return true;
}

// 检查目标是否有有效位置
bool CCmpUnitMotion::TargetHasValidPosition(const MoveRequest& moveRequest) const
{
	if (moveRequest.m_Type != MoveRequest::ENTITY)
		return true;

	CmpPtr<ICmpPosition> cmpPosition(GetSimContext(), moveRequest.m_Entity);
	return cmpPosition && cmpPosition->IsInWorld();
}

// 计算目标的位置
bool CCmpUnitMotion::ComputeTargetPosition(CFixedVector2D& out, const MoveRequest& moveRequest) const
{
	if (moveRequest.m_Type == MoveRequest::POINT)
	{
		out = moveRequest.m_Position;
		return true;
	}

	CmpPtr<ICmpPosition> cmpTargetPosition(GetSimContext(), moveRequest.m_Entity);
	if (!cmpTargetPosition || !cmpTargetPosition->IsInWorld())
		return false;

	if (moveRequest.m_Type == MoveRequest::OFFSET)
	{
		// There is an offset, so compute it relative to orientation
		// 有一个偏移量，所以根据方向计算它
		entity_angle_t angle = cmpTargetPosition->GetRotation().Y;
		CFixedVector2D offset = moveRequest.GetOffset().Rotate(angle);
		out = cmpTargetPosition->GetPosition2D() + offset;
	}
	else
	{
		out = cmpTargetPosition->GetPosition2D();
		// Position is only updated after all units have moved & pushed.
		// Therefore, we may need to interpolate the target position, depending on when this call takes place during the turn:
		//  - On "Turn Start", we'll check positions directly without interpolation.
		//  - During movement, we'll call this for direct-pathing & we need to interpolate
		//    (this way, we move where the unit will end up at the end of _this_ turn, making it match on next turn start).
		//  - After movement, we'll call this to request paths & we need to interpolate
		//    (this way, we'll move where the unit ends up in the end of _next_ turn, making it a match in 2 turns).
		// TODO: This does not really aim many turns in advance, with orthogonal trajectories it probably should.
		// 位置只有在所有单位移动和推挤之后才更新。
		// 因此，我们可能需要根据此调用在回合中的发生时间来插值目标位置：
		//  - 在“回合开始”时，我们将直接检查位置，不进行插值。
		//  - 在移动期间，我们将为直接寻路调用此函数，并且我们需要插值
		//    （这样，我们移动到单位在本回合结束时将到达的位置，使其在下一回合开始时匹配）。
		//  - 移动后，我们将调用此函数来请求路径，并且需要插值
		//    （这样，我们将移动到单位在下一回合结束时将到达的位置，使其在 2 回合后匹配）。
		// 待办事项：这并没有真正提前很多回合进行瞄准，对于正交轨迹，它可能应该这样做。
		CmpPtr<ICmpUnitMotion> cmpUnitMotion(GetSimContext(), moveRequest.m_Entity);
		CmpPtr<ICmpUnitMotionManager> cmpUnitMotionManager(GetSystemEntity());
		bool needInterpolation = cmpUnitMotion && cmpUnitMotion->IsMoveRequested() && cmpUnitMotionManager->ComputingMotion();
		if (needInterpolation)
		{
			// Add predicted movement.
			// 添加预测的移动。
			CFixedVector2D tempPos = out + (out - cmpTargetPosition->GetPreviousPosition2D());
			out = tempPos;
		}
	}
	return true;
}

// 尝试直接朝目标直线移动
bool CCmpUnitMotion::TryGoingStraightToTarget(const CFixedVector2D& from, bool updatePaths)
{
	// Assume if we have short paths we want to follow them.
	// Exception: offset movement (formations) generally have very short deltas
	// and to look good we need them to walk-straight most of the time.
	// 假设如果我们有短路径，我们想跟随它们。
	// 例外：偏移移动（阵型）通常有非常短的增量，
	// 为了好看，我们需要它们大部分时间都走直线。
	if (!IsFormationMember() && !m_ShortPath.m_Waypoints.empty())
		return false;

	CFixedVector2D targetPos;
	if (!ComputeTargetPosition(targetPos))
		return false;

	CmpPtr<ICmpPathfinder> cmpPathfinder(GetSystemEntity());
	if (!cmpPathfinder)
		return false;

	// Move the goal to match the target entity's new position
	// 移动目标以匹配目标实体的新位置
	PathGoal goal;
	if (!ComputeGoal(goal, m_MoveRequest))
		return false;
	goal.x = targetPos.X;
	goal.z = targetPos.Y;
	// (we ignore changes to the target's rotation, since only buildings are
	// square and buildings don't move)
	// （我们忽略目标旋转的变化，因为只有建筑物是方形的，
	// 而建筑物不会移动）

	// Find the point on the goal shape that we should head towards
	// 找到我们应该朝向的目标形状上的点
	CFixedVector2D goalPos = goal.NearestPointOnGoal(from);

	// Fail if the target is too far away
	// 如果目标太远，则失败
	if ((goalPos - from).CompareLength(DIRECT_PATH_RANGE) > 0)
		return false;

	// Check if there's any collisions on that route.
	// For entity goals, skip only the specific obstruction tag or with e.g. walls we might ignore too many entities.
	// 检查该路线上是否有任何碰撞。
	// 对于实体目标，仅跳过特定的障碍物标签，否则例如墙壁可能会忽略太多实体。
	ICmpObstructionManager::tag_t specificIgnore;
	if (m_MoveRequest.m_Type == MoveRequest::ENTITY)
	{
		CmpPtr<ICmpObstruction> cmpTargetObstruction(GetSimContext(), m_MoveRequest.m_Entity);
		if (cmpTargetObstruction)
			specificIgnore = cmpTargetObstruction->GetObstruction();
	}

	// Check movement against units - we want to use the short pathfinder to walk around those if needed.
	// 检查与单位的移动 - 我们希望在需要时使用短路径寻路器绕过它们。
	if (specificIgnore.valid())
	{
		if (!cmpPathfinder->CheckMovement(GetObstructionFilter(specificIgnore), from.X, from.Y, goalPos.X, goalPos.Y, m_Clearance, m_PassClass))
			return false;
	}
	else if (!cmpPathfinder->CheckMovement(GetObstructionFilter(), from.X, from.Y, goalPos.X, goalPos.Y, m_Clearance, m_PassClass))
		return false;

	if (!updatePaths)
		return true;

	// That route is okay, so update our path
	// 那条路线没问题，所以更新我们的路径
	m_LongPath.m_Waypoints.clear();
	m_ShortPath.m_Waypoints.clear();
	m_ShortPath.m_Waypoints.emplace_back(Waypoint{ goalPos.X, goalPos.Y });
	return true;
}

// 检查是否需要更新寻路
bool CCmpUnitMotion::PathingUpdateNeeded(const CFixedVector2D& from) const
{
	if (m_MoveRequest.m_Type == MoveRequest::NONE)
		return false;

	CFixedVector2D targetPos;
	if (!ComputeTargetPosition(targetPos))
		return false;

	if (m_FollowKnownImperfectPathCountdown > 0 && (!m_LongPath.m_Waypoints.empty() || !m_ShortPath.m_Waypoints.empty()))
		return false;

	if (PossiblyAtDestination())
		return false;

	// Get the obstruction shape and translate it where we estimate the target to be.
	// 获取障碍物形状并将其平移到我们估计的目标位置。
	ICmpObstructionManager::ObstructionSquare estimatedTargetShape;
	if (m_MoveRequest.m_Type == MoveRequest::ENTITY)
	{
		CmpPtr<ICmpObstruction> cmpTargetObstruction(GetSimContext(), m_MoveRequest.m_Entity);
		if (cmpTargetObstruction)
			cmpTargetObstruction->GetObstructionSquare(estimatedTargetShape);
	}

	estimatedTargetShape.x = targetPos.X;
	estimatedTargetShape.z = targetPos.Y;

	CmpPtr<ICmpObstruction> cmpObstruction(GetEntityHandle());
	ICmpObstructionManager::ObstructionSquare shape;
	if (cmpObstruction)
		cmpObstruction->GetObstructionSquare(shape);

	// Translate our own obstruction shape to our last waypoint or our current position, lacking that.
	// 将我们自己的障碍物形状平移到我们的最后一个航点或我们当前的位置（如果缺少航点）。
	if (m_LongPath.m_Waypoints.empty() && m_ShortPath.m_Waypoints.empty())
	{
		shape.x = from.X;
		shape.z = from.Y;
	}
	else
	{
		const Waypoint& lastWaypoint = m_LongPath.m_Waypoints.empty() ? m_ShortPath.m_Waypoints.front() : m_LongPath.m_Waypoints.front();
		shape.x = lastWaypoint.x;
		shape.z = lastWaypoint.z;
	}

	CmpPtr<ICmpObstructionManager> cmpObstructionManager(GetSystemEntity());
	ENSURE(cmpObstructionManager);

	// Increase the ranges with distance, to avoid recomputing every turn against units that are moving and far-away for example.
	// 随距离增加范围，以避免例如对正在移动且距离很远的单位每回合重新计算。
	entity_pos_t distance = (from - CFixedVector2D(estimatedTargetShape.x, estimatedTargetShape.z)).Length();

	// TODO: it could be worth computing this based on time to collision instead of linear distance.
	// 待办事项：基于碰撞时间而不是线性距离来计算这个可能更有价值。
	entity_pos_t minRange = std::max(m_MoveRequest.m_MinRange - distance / TARGET_UNCERTAINTY_MULTIPLIER, entity_pos_t::Zero());
	entity_pos_t maxRange = m_MoveRequest.m_MaxRange < entity_pos_t::Zero() ? m_MoveRequest.m_MaxRange :
		m_MoveRequest.m_MaxRange + distance / TARGET_UNCERTAINTY_MULTIPLIER;

	if (cmpObstructionManager->AreShapesInRange(shape, estimatedTargetShape, minRange, maxRange, false))
		return false;

	return true;
}

// 使单位朝向指定点
void CCmpUnitMotion::FaceTowardsPoint(entity_pos_t x, entity_pos_t z)
{
	CmpPtr<ICmpPosition> cmpPosition(GetEntityHandle());
	if (!cmpPosition || !cmpPosition->IsInWorld())
		return;

	CFixedVector2D pos = cmpPosition->GetPosition2D();
	FaceTowardsPointFromPos(pos, x, z);
}

// 从指定位置朝向目标点
void CCmpUnitMotion::FaceTowardsPointFromPos(const CFixedVector2D& pos, entity_pos_t x, entity_pos_t z)
{
	CFixedVector2D target(x, z);
	CFixedVector2D offset = target - pos;
	if (!offset.IsZero())
	{
		entity_angle_t angle = atan2_approx(offset.X, offset.Y);

		CmpPtr<ICmpPosition> cmpPosition(GetEntityHandle());
		if (!cmpPosition)
			return;
		cmpPosition->TurnTo(angle);
	}
}

// The pathfinder cannot go to "rounded rectangles" goals, which are what happens with square targets and a non-null range.
// Depending on what the best approximation is, we either pretend the target is a circle or a square.
// One needs to be careful that the approximated geometry will be in the range.
// 寻路器无法到达“圆角矩形”目标，这是方形目标和非空范围时发生的情况。
// 根据最佳近似是什么，我们要么假装目标是圆形，要么是方形。
// 需要注意的是，近似的几何形状将在范围内。
bool CCmpUnitMotion::ShouldTreatTargetAsCircle(entity_pos_t range, entity_pos_t circleRadius) const
{
	// Given a square, plus a target range we should reach, the shape at that distance
	// is a round-cornered square which we can approximate as either a circle or as a square.
	// Previously, we used the shape that minimized the worst-case error.
	// However that is unsage in some situations. So let's be less clever and
	// just check if our range is at least three times bigger than the circleradius
	// 给定一个正方形，加上我们应该达到的目标范围，该距离处的形状
	// 是一个圆角正方形，我们可以将其近似为圆形或正方形。
	// 以前，我们使用最小化最坏情况误差的形状。
	// 然而，在某些情况下这是不安全的。所以让我们不要那么聪明，
	// 只检查我们的范围是否至少比圆形半径大三倍。
	return (range > circleRadius * 3);
}

// 从移动请求计算寻路目标
bool CCmpUnitMotion::ComputeGoal(PathGoal& out, const MoveRequest& moveRequest) const
{
	if (moveRequest.m_Type == MoveRequest::NONE)
		return false;

	CmpPtr<ICmpPosition> cmpPosition(GetEntityHandle());
	if (!cmpPosition || !cmpPosition->IsInWorld())
		return false;

	CFixedVector2D pos = cmpPosition->GetPosition2D();

	CFixedVector2D targetPosition;
	if (!ComputeTargetPosition(targetPosition, moveRequest))
		return false;

	ICmpObstructionManager::ObstructionSquare targetObstruction;
	if (moveRequest.m_Type == MoveRequest::ENTITY)
	{
		CmpPtr<ICmpObstruction> cmpTargetObstruction(GetSimContext(), moveRequest.m_Entity);
		if (cmpTargetObstruction)
			cmpTargetObstruction->GetObstructionSquare(targetObstruction);
	}
	targetObstruction.x = targetPosition.X;
	targetObstruction.z = targetPosition.Y;

	ICmpObstructionManager::ObstructionSquare obstruction;
	CmpPtr<ICmpObstruction> cmpObstruction(GetEntityHandle());
	if (cmpObstruction)
		cmpObstruction->GetObstructionSquare(obstruction);
	else
	{
		obstruction.x = pos.X;
		obstruction.z = pos.Y;
	}

	CmpPtr<ICmpObstructionManager> cmpObstructionManager(GetSystemEntity());
	ENSURE(cmpObstructionManager);

	out.x = targetObstruction.x;
	out.z = targetObstruction.z;
	out.hw = targetObstruction.hw;
	out.hh = targetObstruction.hh;
	out.u = targetObstruction.u;
	out.v = targetObstruction.v;

	if (moveRequest.m_MinRange > fixed::Zero() || moveRequest.m_MaxRange > fixed::Zero() ||
		targetObstruction.hw > fixed::Zero())
		out.type = PathGoal::SQUARE;
	else
	{
		out.type = PathGoal::POINT;
		return true;
	}

	entity_pos_t distance = cmpObstructionManager->DistanceBetweenShapes(obstruction, targetObstruction);

	entity_pos_t circleRadius = CFixedVector2D(targetObstruction.hw, targetObstruction.hh).Length();

	// TODO: because we cannot move to rounded rectangles, we have to make conservative approximations.
	// This means we might end up in a situation where cons(max-range) < min range < max range < cons(min-range)
	// When going outside of the min-range or inside the max-range, the unit will still go through the correct range
	// but if it moves fast enough, this might not be picked up by PossiblyAtDestination().
	// Fixing this involves moving to rounded rectangles, or checking more often in PerformMove().
	// In the meantime, one should avoid that 'Speed over a turn' > MaxRange - MinRange, in case where
	// min-range is not 0 and max-range is not infinity.
	// 待办事项：因为我们不能移动到圆角矩形，我们必须做保守的近似。
	// 这意味着我们可能会陷入这样一种情况：cons(max-range) < min-range < max-range < cons(min-range)
	// 当移出最小范围或移入最大范围时，单位仍会通过正确的范围，
	// 但如果它移动得足够快，PossiblyAtDestination() 可能不会检测到。
	// 解决这个问题需要移动到圆角矩形，或在 PerformMove() 中更频繁地检查。
	// 在此期间，应避免“一回合内的速度” > MaxRange - MinRange，在
	// 最小范围不为 0 且最大范围不是无穷大的情况下。
	if (distance < moveRequest.m_MinRange)
	{
		// Distance checks are nearest edge to nearest edge, so we need to account for our clearance
		// and we must make sure diagonals also fit so multiply by slightly more than sqrt(2)
		// 距离检查是最近边缘到最近边缘，所以我们需要考虑我们的清除区
		// 并且我们必须确保对角线也适合，所以乘以略大于 sqrt(2) 的值
		entity_pos_t goalDistance = moveRequest.m_MinRange + m_Clearance * 3 / 2;

		if (ShouldTreatTargetAsCircle(moveRequest.m_MinRange, circleRadius))
		{
			// We are safely away from the obstruction itself if we are away from the circumscribing circle
			// 如果我们远离外接圆，我们就安全地远离了障碍物本身
			out.type = PathGoal::INVERTED_CIRCLE;
			out.hw = circleRadius + goalDistance;
		}
		else
		{
			out.type = PathGoal::INVERTED_SQUARE;
			out.hw = targetObstruction.hw + goalDistance;
			out.hh = targetObstruction.hh + goalDistance;
		}
	}
	else if (moveRequest.m_MaxRange >= fixed::Zero() && distance > moveRequest.m_MaxRange)
	{
		if (ShouldTreatTargetAsCircle(moveRequest.m_MaxRange, circleRadius))
		{
			entity_pos_t goalDistance = moveRequest.m_MaxRange;
			// We must go in-range of the inscribed circle, not the circumscribing circle.
			// 我们必须进入内切圆的范围，而不是外接圆。
			circleRadius = std::min(targetObstruction.hw, targetObstruction.hh);

			out.type = PathGoal::CIRCLE;
			out.hw = circleRadius + goalDistance;
		}
		else
		{
			// The target is large relative to our range, so treat it as a square and
			// get close enough that the diagonals come within range
			// 目标相对于我们的范围很大，所以把它当作一个正方形，
			// 靠近到足以让对角线进入范围
			entity_pos_t goalDistance = moveRequest.m_MaxRange * 2 / 3; // multiply by slightly less than 1/sqrt(2)
			// 乘以略小于 1/sqrt(2) 的值

			out.type = PathGoal::SQUARE;
			entity_pos_t delta = std::max(goalDistance, m_Clearance + entity_pos_t::FromInt(4) / 16); // ensure it's far enough to not intersect the building itself
			// 确保它足够远，不会与建筑物本身相交
			out.hw = targetObstruction.hw + delta;
			out.hh = targetObstruction.hh + delta;
		}
	}
	// Do nothing in particular in case we are already in range.
	// 如果我们已经在范围内，就什么都不做。
	return true;
}

// 计算到目标的路径
void CCmpUnitMotion::ComputePathToGoal(const CFixedVector2D& from, const PathGoal& goal)
{
#if DISABLE_PATHFINDER
	{
		CmpPtr<ICmpPathfinder> cmpPathfinder(GetSimContext(), SYSTEM_ENTITY);
		CFixedVector2D goalPos = m_FinalGoal.NearestPointOnGoal(from);
		m_LongPath.m_Waypoints.clear();
		m_ShortPath.m_Waypoints.clear();
		m_ShortPath.m_Waypoints.emplace_back(Waypoint{ goalPos.X, goalPos.Y });
		return;
	}
#endif

	// If the target is close enough, hope that we'll be able to go straight next turn.
	// 如果目标足够近，希望我们下一回合能走直线。
	if (!ShouldAlternatePathfinder() && TryGoingStraightToTarget(from, false))
	{
		// NB: since we may fail to move straight next turn, we should edge our bets.
		// Since the 'go straight' logic currently fires only if there's no short path,
		// we'll compute a long path regardless to make sure _that_ stays up to date.
		// (it's also extremely likely to be very fast to compute, so no big deal).
		// 注意：因为我们下一回合可能无法直线移动，我们应该两边下注。
		// 因为“走直线”逻辑目前只在没有短路径时触发，
		// 我们无论如何都会计算一条长路径，以确保它保持最新。
		// （它计算起来也极有可能非常快，所以没什么大不了的）。
		m_ShortPath.m_Waypoints.clear();
		RequestLongPath(from, goal);
		return;
	}

	// Otherwise we need to compute a path.
	// 否则我们需要计算一条路径。

	// If it's close then just do a short path, not a long path
	// TODO: If it's close on the opposite side of a river then we really
	// need a long path, so we shouldn't simply check linear distance
	// the check is arbitrary but should be a reasonably small distance.
	// We want to occasionally compute a long path if we're computing short-paths, because the short path domain
	// is bounded and thus it can't around very large static obstacles.
	// Likewise, we want to compile a short-path occasionally when the target is far because we might be stuck
	// on a navcell surrounded by impassable navcells, but the short-pathfinder could move us out of there.
	// 如果很近，就只做短路径，不做长路径
	// 待办事项：如果在河的另一边很近，我们真的需要一条长路径，所以我们不应该只检查线性距离
	// 这个检查是任意的，但应该是一个相当小的距离。
	// 我们希望在计算短路径时偶尔计算一条长路径，因为短路径域是有界的，因此它不能绕过非常大的静态障碍物。
	// 同样，我们希望在目标很远时偶尔编译一条短路径，因为我们可能会被困在一个被不可通行导航单元包围的导航单元上，
	// 但短路径寻路器可以把我们移出去。
	bool shortPath = InShortPathRange(goal, from);
	if (ShouldAlternatePathfinder())
		shortPath = !shortPath;
	if (shortPath)
	{
		m_LongPath.m_Waypoints.clear();
		// Extend the range so that our first path is probably valid.
		// 扩展范围，以便我们的第一条路径可能是有效的。
		RequestShortPath(from, goal, true);
	}
	else
	{
		m_ShortPath.m_Waypoints.clear();
		RequestLongPath(from, goal);
	}
}

// 请求长程路径
void CCmpUnitMotion::RequestLongPath(const CFixedVector2D& from, const PathGoal& goal)
{
	CmpPtr<ICmpPathfinder> cmpPathfinder(GetSystemEntity());
	if (!cmpPathfinder)
		return;

	// this is by how much our waypoints will be apart at most.
	// this value here seems sensible enough.
	// 这是我们的航点之间最多相隔的距离。
	// 这个值在这里看起来足够合理。
	PathGoal improvedGoal = goal;
	improvedGoal.maxdist = SHORT_PATH_MIN_SEARCH_RANGE - entity_pos_t::FromInt(1);

	cmpPathfinder->SetDebugPath(from.X, from.Y, improvedGoal, m_PassClass);

	m_ExpectedPathTicket.m_Type = Ticket::LONG_PATH;
	m_ExpectedPathTicket.m_Ticket = cmpPathfinder->ComputePathAsync(from.X, from.Y, improvedGoal, m_PassClass, GetEntityId());
}

// 请求短程路径
void CCmpUnitMotion::RequestShortPath(const CFixedVector2D& from, const PathGoal& goal, bool extendRange)
{
	CmpPtr<ICmpPathfinder> cmpPathfinder(GetSystemEntity());
	if (!cmpPathfinder)
		return;

	entity_pos_t searchRange = ShortPathSearchRange();
	if (extendRange)
	{
		CFixedVector2D dist(from.X - goal.x, from.Y - goal.z);
		if (dist.CompareLength(searchRange - entity_pos_t::FromInt(1)) >= 0)
		{
			searchRange = dist.Length() + fixed::FromInt(1);
			if (searchRange > SHORT_PATH_MAX_SEARCH_RANGE)
				searchRange = SHORT_PATH_MAX_SEARCH_RANGE;
		}
	}

	m_ExpectedPathTicket.m_Type = Ticket::SHORT_PATH;
	m_ExpectedPathTicket.m_Ticket = cmpPathfinder->ComputeShortPathAsync(from.X, from.Y, m_Clearance, searchRange, goal, m_PassClass, ShouldCollideWithMovingUnits(), GetGroup(), GetEntityId());
}

// 移动到指定请求
bool CCmpUnitMotion::MoveTo(MoveRequest request)
{
	PROFILE("MoveTo");

	if (request.m_MinRange == request.m_MaxRange && !request.m_MinRange.IsZero())
		LOGWARNING("MaxRange must be larger than MinRange; See CCmpUnitMotion.cpp for more information"); // MaxRange 必须大于 MinRange；详见 CCmpUnitMotion.cpp

	CmpPtr<ICmpPosition> cmpPosition(GetEntityHandle());
	if (!cmpPosition || !cmpPosition->IsInWorld())
		return false;

	PathGoal goal;
	if (!ComputeGoal(goal, request))
		return false;

	m_MoveRequest = request;
	m_FailedMovements = 0;
	m_FollowKnownImperfectPathCountdown = 0;

	ComputePathToGoal(cmpPosition->GetPosition2D(), goal);
	return true;
}

// 检查目标范围是否可达
bool CCmpUnitMotion::IsTargetRangeReachable(entity_id_t target, entity_pos_t minRange, entity_pos_t maxRange)
{
	CmpPtr<ICmpPosition> cmpPosition(GetEntityHandle());
	if (!cmpPosition || !cmpPosition->IsInWorld())
		return false;

	MoveRequest request(target, minRange, maxRange);
	PathGoal goal;
	if (!ComputeGoal(goal, request))
		return false;

	CmpPtr<ICmpPathfinder> cmpPathfinder(GetSimContext(), SYSTEM_ENTITY);
	CFixedVector2D pos = cmpPosition->GetPosition2D();
	return cmpPathfinder->IsGoalReachable(pos.X, pos.Y, goal, m_PassClass);
}

// 渲染路径以供调试
void CCmpUnitMotion::RenderPath(const WaypointPath& path, std::vector<SOverlayLine>& lines, CColor color)
{
	bool floating = false;
	CmpPtr<ICmpPosition> cmpPosition(GetEntityHandle());
	if (cmpPosition)
		floating = cmpPosition->CanFloat();

	lines.clear();
	std::vector<float> waypointCoords;
	for (size_t i = 0; i < path.m_Waypoints.size(); ++i)
	{
		float x = path.m_Waypoints[i].x.ToFloat();
		float z = path.m_Waypoints[i].z.ToFloat();
		waypointCoords.push_back(x);
		waypointCoords.push_back(z);
		lines.push_back(SOverlayLine());
		lines.back().m_Color = color;
		SimRender::ConstructSquareOnGround(GetSimContext(), x, z, 1.0f, 1.0f, 0.0f, lines.back(), floating);
	}
	float x = cmpPosition->GetPosition2D().X.ToFloat();
	float z = cmpPosition->GetPosition2D().Y.ToFloat();
	waypointCoords.push_back(x);
	waypointCoords.push_back(z);
	lines.push_back(SOverlayLine());
	lines.back().m_Color = color;
	SimRender::ConstructLineOnGround(GetSimContext(), waypointCoords, lines.back(), floating);

}

// 提交渲染数据到场景收集器
void CCmpUnitMotion::RenderSubmit(SceneCollector& collector)
{
	if (!m_DebugOverlayEnabled)
		return;

	RenderPath(m_LongPath, m_DebugOverlayLongPathLines, OVERLAY_COLOR_LONG_PATH);
	RenderPath(m_ShortPath, m_DebugOverlayShortPathLines, OVERLAY_COLOR_SHORT_PATH);

	for (size_t i = 0; i < m_DebugOverlayLongPathLines.size(); ++i)
		collector.Submit(&m_DebugOverlayLongPathLines[i]);

	for (size_t i = 0; i < m_DebugOverlayShortPathLines.size(); ++i)
		collector.Submit(&m_DebugOverlayShortPathLines[i]);
}

#endif // INCLUDED_CCMPUNITMOTION
