/* Copyright (C) 2017 Wildfire Games.
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

#ifndef INCLUDED_ICMPTERRAIN
#define INCLUDED_ICMPTERRAIN

#include "simulation2/system/Interface.h"

#include "simulation2/helpers/Position.h"

#include "maths/FixedVector3D.h"

class CTerrain;
class CVector3D;

class ICmpTerrain : public IComponent
{
public:
	virtual bool IsLoaded() const = 0;

	virtual CFixedVector3D CalcNormal(entity_pos_t x, entity_pos_t z) const = 0;

	virtual CVector3D CalcExactNormal(float x, float z) const = 0;

	virtual entity_pos_t GetGroundLevel(entity_pos_t x, entity_pos_t z) const = 0;

	virtual float GetExactGroundLevel(float x, float z) const = 0;

	/**
	 * 返回地形每边的地块（tile）数量。
	 * 返回值永远非零。
	 */
	virtual u16 GetTilesPerSide() const = 0;

	/**
	 * 返回地形每边的顶点（vertex）数量。
	 * 返回值永远非零。
	 */
	virtual u16 GetVerticesPerSide() const = 0;

	/**
	 * 以米（世界空间单位）为单位返回地图大小。
	 */
	virtual u32 GetMapSize() const = 0;

	virtual CTerrain* GetCTerrain() = 0;

	/**
	 * 当底层的 CTerrain 在我们不知情的情况下被修改时调用此函数。
	 * （待办事项：最终我们应该在这个类中管理 CTerrain，
	 * 这样就没有人可以在我们不知情的情况下修改它了）。
	 */
	virtual void ReloadTerrain(bool ReloadWater = true) = 0;

	/**
	 * 表明给定区域内的地形地块已发生变化（该区域范围的下界是包含的，
	 * 而上界是不包含的）。`CMessageTerrainChanged` 消息将被
	 * 发送给所有关注地形变化的组件。
	 */
	virtual void MakeDirty(i32 i0, i32 j0, i32 i1, i32 j1) = 0;

	DECLARE_INTERFACE_TYPE(Terrain)
};

#endif // INCLUDED_ICMPTERRAIN
