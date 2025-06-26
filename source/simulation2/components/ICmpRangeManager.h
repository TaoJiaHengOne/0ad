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

#ifndef INCLUDED_ICMPRANGEMANAGER
#define INCLUDED_ICMPRANGEMANAGER

#include "maths/FixedVector3D.h"
#include "maths/FixedVector2D.h"

#include "simulation2/system/Interface.h"
#include "simulation2/helpers/Position.h"
#include "simulation2/helpers/Player.h"

#include <vector>

class FastSpatialSubdivision;

/**
 * 分配给一个我们永远在其范围内的距离值（由实体在世界之外或在抛物线范围中“过高”导致）。
 * TODO: 也为最小范围（minRanges）添加此项。
 */
const entity_pos_t ALWAYS_IN_RANGE = entity_pos_t::FromInt(-1);

/**
 * 分配给一个我们永远不在其范围内的距离值（由实体在世界之外或在抛物线范围中“过高”导致）。
 * TODO: 也将此项添加到范围查询中。
 */
const entity_pos_t NEVER_IN_RANGE = entity_pos_t::FromInt(-2);

/**
 * 由于 GetVisibility (获取视野) 查询是由范围管理器运行的，
 * 其他使用这些功能的代码无论如何都必须包含 ICmpRangeManager.h，
 * 所以在这里定义这个枚举（理想情况下，它应该在自己的头文件中，
 * 但增加头文件本身会增加编译时间）。
 */
enum class LosVisibility : u8
{
	HIDDEN = 0,  // 隐藏
	FOGGED = 1,  // 迷雾
	VISIBLE = 2  // 可见
};

/**
 * 同样的原则也适用于 CLosQuerier，但为了避免在 CLosQuerier 变化时，
 * 重新编译依赖于 RangeManager 但不依赖于 CLosQuerier 的翻译单元（尤其是头文件），
 * 我们在另一个文件中定义它。使用视野（LOS）的代码届时将显式包含 LOS 的头文件，
 * 这无论如何都是合理的。
 */
class CLosQuerier;

/**
 * 提供了对游戏世界高效的基于范围的查询，
 * 以及基于视野（LOS）的效果（如战争迷雾）。
 *
 * (这些在概念上有所不同，但它们共享大量实现，
 * 所以为了效率，它们被合并到这个类中。)
 *
 * 可能的用例：
 * - 战斗单位需要检测可攻击的敌人进入视野，以便它们可以选择
 * 自动攻击。
 * - 光环效果让一个单位能对特定范围内的所有单位（或同玩家单位、敌方单位）
 * 产生某种影响。
 * - 可捕获的动物需要检测附近是否有玩家单位，并且没有其他
 * 玩家的单位在范围内。
 * - 关卡触发器可能想要检测单位何时进入给定区域。
 * - 从已耗尽资源点采集的单位需要找到一个在旧资源点附近
 * 且可到达的同类型新资源点。
 * - 带有溅射伤害的抛射武器需要找到目标点一定距离内的所有单位。
 * - ...
 *
 * 在大多数情况下，用户是基于事件的，并希望在有单位
 * 进入或离开范围时收到通知，并且查询可以一次性设置且很少更改。
 * 这些查询必须快速。实体被近似为点或圆
 * (查询可以设置为忽略大小，因为视野（LOS）目前会忽略它，而不匹配会产生问题)。
 *
 * 当前设计：
 *
 * 这个类只处理范围查询最常见的部分：
 * 距离、目标接口和玩家归属。
 * 然后调用者可以应用任何它需要的更复杂的过滤。
 *
 * 有两种类型的查询：
 * 被动查询（Passive queries）由 ExecuteQuery 执行，并立即返回匹配的实体。
 * 主动查询（Active queries）由 CreateActiveQuery 设置，然后如果自上次 RangeUpdate 以来
 * 有任何单位进入或离开范围，CMessageRangeUpdate 消息将每回合发送给该实体一次。
 * 查询可以被禁用，这种情况下将不会发送消息。
 */
class ICmpRangeManager : public IComponent
{
public:
	/**
	 * 主动查询的外部标识符。
	 */
	typedef u32 tag_t;

	/**
	 * 访问由范围管理器维护的空间分割结构。
	 * @return 指向空间分割结构的指针。
	 */
	virtual FastSpatialSubdivision* GetSubdivision() = 0;

	/**
	 * 设置世界的边界。
	 * 实体不应在边界之外（否则效率会受影响）。
	 * @param x0,z0,x1,z1 世界角落的坐标
	 */
	virtual void SetBounds(entity_pos_t x0, entity_pos_t z0, entity_pos_t x1, entity_pos_t z1) = 0;

	/**
	 * 执行一个被动查询。
	 * @param source 将围绕其计算范围的实体。
	 * @param minRange 非负的最小距离（米，包含此值）。
	 * @param maxRange 非负的最大距离（米，包含此值）；或-1.0表示忽略距离。
	 * @param owners 匹配实体可能拥有的玩家ID列表；-1匹配没有所有者的实体。
	 * @param requiredInterface 如果非零，则为匹配实体必须实现的接口ID。
	 * @param accountForSize 如果为true，则补偿源/目标实体的大小。
	 * @return 匹配查询的实体列表，按与源实体距离递增的顺序排列。
	 */
	virtual std::vector<entity_id_t> ExecuteQuery(entity_id_t source, entity_pos_t minRange, entity_pos_t maxRange,
		const std::vector<int>& owners, int requiredInterface, bool accountForSize) = 0;

	/**
	 * 执行一个被动查询。
	 * @param pos 将围绕其计算范围的位置。
	 * @param minRange 非负的最小距离（米，包含此值）。
	 * @param maxRange 非负的最大距离（米，包含此值）；或-1.0表示忽略距离。
	 * @param owners 匹配实体可能拥有的玩家ID列表；-1匹配没有所有者的实体。
	 * @param requiredInterface 如果非零，则为匹配实体必须实现的接口ID。
	 * @param accountForSize 如果为true，则补偿源/目标实体的大小。
	 * @return 匹配查询的实体列表，按与源实体距离递增的顺序排列。
	 */
	virtual std::vector<entity_id_t> ExecuteQueryAroundPos(const CFixedVector2D& pos, entity_pos_t minRange, entity_pos_t maxRange,
		const std::vector<int>& owners, int requiredInterface, bool accountForSize) = 0;

	/**
	 * 创建一个主动查询。该查询默认是禁用的。
	 * @param source 将围绕其计算范围的实体。
	 * @param minRange 非负的最小距离（米，包含此值）。
	 * @param maxRange 非负的最大距离（米，包含此值）；或-1.0表示忽略距离。
	 * @param owners 匹配实体可能拥有的玩家ID列表；-1匹配没有所有者的实体。
	 * @param requiredInterface 如果非零，则为匹配实体必须实现的接口ID。
	 * @param flags 如果范围内的实体设置了其中一个标志位，它就会出现。
	 * @param accountForSize 如果为true，则补偿源/目标实体的大小。
	 * @return 查询的唯一非零标识符。
	 */
	virtual tag_t CreateActiveQuery(entity_id_t source, entity_pos_t minRange, entity_pos_t maxRange,
		const std::vector<int>& owners, int requiredInterface, u8 flags, bool accountForSize) = 0;

	/**
	 * 创建一个围绕单位的抛物线形式的主动查询。
	 * 该查询默认是禁用的。
	 * @param source 将围绕其计算范围的实体。
	 * @param minRange 非负的最小水平距离（米，包含此值）。MinRange不进行抛物线检查。
	 * @param maxRange 对于在相同海拔的单位，为非负的最大距离（米，包含此值）；
	 * 或-1.0表示忽略距离。
	 * 对于在不同高度位置的单位，使用一个在单位上方高度为maxRange/2的物理正确的抛物面来查询它们。
	 * @param yOrigin 额外加成，使源单位可以放置得更高，射得更远。
	 * @param owners 匹配实体可能拥有的玩家ID列表；-1匹配没有所有者的实体。
	 * @param requiredInterface 如果非零，则为匹配实体必须实现的接口ID。
	 * @param flags 如果范围内的实体设置了其中一个标志位，它就会出现。
	 * 注意：此函数没有accountForSize参数（假定为true），因为我们目前JS函数最多只能有7个参数。
	 * @return 查询的唯一非零标识符。
	 */
	virtual tag_t CreateActiveParabolicQuery(entity_id_t source, entity_pos_t minRange, entity_pos_t maxRange, entity_pos_t yOrigin,
		const std::vector<int>& owners, int requiredInterface, u8 flags) = 0;


	/**
	 * 在抛物线范围查询中获取有效射程。
	 * @param source 查询原点的实体ID。
	 * @param target 目标实体ID。
	 * @param range 与地形高度进行比较的距离。
	 * @param yOrigin 源单位默认比目标高出的高度。
	 * @return 一个定点数，表示经过高度差抛物线校正后的有效射程。当目标相对于源过高而无法进入射程时，返回-1。
	 */
	virtual entity_pos_t GetEffectiveParabolicRange(entity_id_t source, entity_id_t target, entity_pos_t range, entity_pos_t yOrigin) const = 0;

	/**
	 * 获取实体周围距离为range的8个点的平均高程。
	 * @param id 要查看周围的实体ID。
	 * @param range 与地形高度进行比较的距离。
	 * @return 一个定点数，表示平均差异。当实体平均高于其周围地形时，该值为正。
	 */
	virtual entity_pos_t GetElevationAdaptedRange(const CFixedVector3D& pos, const CFixedVector3D& rot, entity_pos_t range, entity_pos_t yOrigin, entity_pos_t angle) const = 0;

	/**
	 * 销毁一个查询并清理资源。当实体不再需要其
	 * 查询时（例如，当实体被销毁时），必须调用此函数。
	 * @param tag 查询的标识符。
	 */
	virtual void DestroyActiveQuery(tag_t tag) = 0;

	/**
	 * 重新启用一个查询的处理。
	 * @param tag 查询的标识符。
	 */
	virtual void EnableActiveQuery(tag_t tag) = 0;

	/**
	 * 禁用一个查询的处理（将不会发送RangeUpdate消息）。
	 * @param tag 查询的标识符。
	 */
	virtual void DisableActiveQuery(tag_t tag) = 0;

	/**
	 * 检查一个查询的处理是否已启用。
	 * @param tag 一个查询的标识符。
	 */
	virtual bool IsActiveQueryEnabled(tag_t tag) const = 0;

	/**
	 * 立即执行一个查询，如果被禁用则重新启用它。
	 * 下一个RangeUpdate消息将说明自此调用以来谁进入/离开了范围，
	 * 所以你不会错过任何通知。
	 * @param tag 查询的标识符。
	 * @return 匹配查询的实体列表，按与源实体距离递增的顺序排列。
	 */
	virtual std::vector<entity_id_t> ResetActiveQuery(tag_t tag) = 0;

	/**
	 * 返回特定玩家的所有实体列表。
	 * (这个接口之所以在这里，是因为它与很多实现共享。
	 * 也许它应该被扩展得更像没有范围参数的ExecuteQuery。)
	 */
	virtual std::vector<entity_id_t> GetEntitiesByPlayer(player_id_t player) const = 0;

	/**
	 * 返回除盖亚（gaia）单位外的所有玩家的实体列表。
	 */
	virtual std::vector<entity_id_t> GetNonGaiaEntities() const = 0;

	/**
	 * 返回由玩家或盖亚（gaia）拥有的所有实体列表。
	 */
	virtual std::vector<entity_id_t> GetGaiaAndNonGaiaEntities() const = 0;

	/**
	 * 切换调试信息的渲染。
	 */
	virtual void SetDebugOverlay(bool enabled) = 0;

	/**
	 * 返回指定标识符的掩码。
	 */
	virtual u8 GetEntityFlagMask(const std::string& identifier) const = 0;

	/**
	 * 将由标识符指定的标志位设置为实体提供的值。
	 * @param ent 将要被修改其标志位的实体。
	 * @param identifier 将要被修改的标志位。
	 * @param value 将要设置给标志位的值。
	 */
	virtual void SetEntityFlag(entity_id_t ent, const std::string& identifier, bool value) = 0;


	//////////////////////////////////////////////////////////////////
	////            此行下方为视野（LOS）接口                       ////
	//////////////////////////////////////////////////////////////////

	/**
	 * 返回一个CLosQuerier，用于检查顶点位置对于给定玩家
	 * （或其共享视野的其他玩家）是否可见。
	 */
	virtual CLosQuerier GetLosQuerier(player_id_t player) const = 0;

	/**
	 * 为实体ent切换脚本化视野组件的激活状态。
	 */
	virtual void ActivateScriptedVisibility(entity_id_t ent, bool status) = 0;

	/**
	 * 返回给定实体相对于给定玩家的视野状态。
	 * 如果实体不存在或不在世界中，则返回 LosVisibility::HIDDEN。
	 * 此函数遵守 GetLosRevealAll 标志。
	 */
	virtual LosVisibility GetLosVisibility(CEntityHandle ent, player_id_t player) const = 0;
	virtual LosVisibility GetLosVisibility(entity_id_t ent, player_id_t player) const = 0;

	/**
	 * 返回给定位置相对于给定玩家的视野状态。
	 * 此函数遵守 GetLosRevealAll 标志。
	 */
	virtual LosVisibility GetLosVisibilityPosition(entity_pos_t x, entity_pos_t z, player_id_t player) const = 0;

	/**
	 * 请求在下一回合更新实体ent的视野缓存。
	 * 通常用于迷雾效果。
	 */
	virtual void RequestVisibilityUpdate(entity_id_t ent) = 0;


	/**
	 * 为脚本调用包装的GetLosVisibility。
	 * 返回 "hidden", "fogged" 或 "visible"。
	 */
	std::string GetLosVisibility_wrapper(entity_id_t ent, player_id_t player) const;

	/**
	 * 为脚本调用包装的GetLosVisibilityPosition。
	 * 返回 "hidden", "fogged" 或 "visible"。
	 */
	std::string GetLosVisibilityPosition_wrapper(entity_pos_t x, entity_pos_t z, player_id_t player) const;

	/**
	 * 为玩家 p 探索地图（但保留其在战争迷雾中）。
	 */
	virtual void ExploreMap(player_id_t p) = 0;

	/**
	 * 探索每个玩家领土内的地块。
	 * 这只在游戏开始时执行一次。
	 */
	virtual void ExploreTerritories() = 0;

	/**
	 * 为指定玩家 p 显示海岸线。
	 * 这与实体的作用方式类似：如果多次使用 enabled=true 调用 RevealShore，
	 * 那么需要同样次数地使用 enabled=false 调用它，才能使海岸线
	 * 回到战争迷雾中。
	 */
	virtual void RevealShore(player_id_t p, bool enable) = 0;

	/**
	 * 设置是否应为给定玩家显示整个地图。
	 * 如果 player 是 -1，则地图将对所有玩家可见。
	 */
	virtual void SetLosRevealAll(player_id_t player, bool enabled) = 0;

	/**
	 * 返回是否已为给定玩家显示整个地图。
	 */
	virtual bool GetLosRevealAll(player_id_t player) const = 0;

	/**
	 * 设置视野是否被限制在一个圆形地图内。
	 */
	virtual void SetLosCircular(bool enabled) = 0;

	/**
	 * 返回视野是否被限制在一个圆形地图内。
	 */
	virtual bool GetLosCircular() const = 0;

	/**
	 * 将玩家的共享视野数据设置为给定的玩家列表。
	 */
	virtual void SetSharedLos(player_id_t player, const std::vector<player_id_t>& players) = 0;

	/**
	 * 返回玩家的共享视野掩码。
	 */
	virtual u32 GetSharedLosMask(player_id_t player) const = 0;

	/**
	 * 获取指定玩家的地图探索百分比统计。
	 */
	virtual u8 GetPercentMapExplored(player_id_t player) const = 0;

	/**
	 * 获取指定玩家集合的地图探索百分比联合统计。
	 * 注意：此函数从头开始计算统计数据，不应过于频繁地调用。
	 */
	virtual u8 GetUnionPercentMapExplored(const std::vector<player_id_t>& players) const = 0;

	/**
	 * @return 视野（LOS）网格的每边顶点数。
	 */
	virtual size_t GetVerticesPerSide() const = 0;

	/**
	 * 执行一些内部一致性检查，用于测试/调试。
	 */
	virtual void Verify() = 0;

	DECLARE_INTERFACE_TYPE(RangeManager)
};

#endif // INCLUDED_ICMPRANGEMANAGER
