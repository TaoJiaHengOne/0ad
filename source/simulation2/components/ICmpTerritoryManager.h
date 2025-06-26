/* Copyright (C) 2024 Wildfire Games.
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

#ifndef INCLUDED_ICMPTERRITORYMANAGER
#define INCLUDED_ICMPTERRITORYMANAGER

#include "simulation2/system/Interface.h"

#include "simulation2/helpers/Player.h"
#include "simulation2/components/ICmpPosition.h"

#include <vector>

 // 前向声明 Grid 模板类
template<typename T>
class Grid;

/**
 * 领土管理器组件接口
 */
class ICmpTerritoryManager : public IComponent
{
public:
	/**
	 * 返回领土纹理是否需要更新。
	 */
	virtual bool NeedUpdateTexture(size_t* dirtyID) = 0;

	/**
	 * 返回 AI 领土图是否需要更新。
	 */
	virtual bool NeedUpdateAI(size_t* dirtyID, size_t* dirtyBlinkingID) const = 0;

	/**
	 * 每个领土地块包含的寻路导航单元格（navcell）数量。
	 * 通行性数据是按导航单元格存储的，但我们可能不需要那么高的
	 * 分辨率，较低的分辨率可以使边界线看起来更漂亮，
	 * 并且占用更少的内存，所以我们对通行性数据进行降采样。
	 */
	static const int NAVCELLS_PER_TERRITORY_TILE = 8;

	// 定义用于领土数据的位掩码
	static const int TERRITORY_PLAYER_MASK = 0x1F;      // 玩家ID掩码 (支持最多31个玩家)
	static const int TERRITORY_CONNECTED_MASK = 0x20;   // “已连接”标志位掩码
	static const int TERRITORY_BLINKING_MASK = 0x40;    // “闪烁”标志位掩码
	static const int TERRITORY_PROCESSED_MASK = 0x80;   //< 内部使用；标记一个地块为已处理。

	/**
	 * 对于每个地块，TERRITORY_PLAYER_MASK 位是玩家ID；
	 * 如果地块连接到一个根对象（市政中心等），则 TERRITORY_CONNECTED_MASK 位被设置。
	 */
	virtual const Grid<u8>& GetTerritoryGrid() = 0;

	/**
	 * 获取给定位置的领土所有者。
	 * @return 所有者的玩家ID；如果是中立领土，则返回0
	 */
	virtual player_id_t GetOwner(entity_pos_t x, entity_pos_t z) = 0;

	/**
	 * 获取所选位置的每个玩家的相邻地块数量
	 * @return 一个包含每个玩家相邻地块数量的列表
	 */
	virtual std::vector<u32> GetNeighbours(entity_pos_t x, entity_pos_t z, bool filterConnected) = 0;

	/**
	 * 获取给定位置的领土是否连接到该领土玩家所拥有的一个根对象
	 * （市政中心等）。
	 */
	virtual bool IsConnected(entity_pos_t x, entity_pos_t z) = 0;

	/**
	 * 将一块领土设置为闪烁状态。必须在每次领土计算时更新。
	 */
	virtual void SetTerritoryBlinking(entity_pos_t x, entity_pos_t z, bool enable) = 0;

	/**
	 * 检查一块领土是否正在闪烁。
	 */
	virtual bool IsTerritoryBlinking(entity_pos_t x, entity_pos_t z) = 0;

	/**
	 * 返回由给定玩家控制的世界百分比，该百分比由
	 * 该玩家拥有的领土地块数量定义。
	 */
	virtual u8 GetTerritoryPercentage(player_id_t player) = 0;

	/**
	 * 启用或禁用领土边界的渲染。
	 */
	virtual void SetVisibility(bool visible) = 0;

	/**
	 * 更新边界和领土颜色。
	 */
	virtual void UpdateColors() = 0;

	/**
	 * 领土边界线是否可见。
	 */
	virtual bool IsVisible() const = 0;

	// 声明组件的接口类型
	DECLARE_INTERFACE_TYPE(TerritoryManager)
};

#endif // INCLUDED_ICMPTERRITORYMANAGER
