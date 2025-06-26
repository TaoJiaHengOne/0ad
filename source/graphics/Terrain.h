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

 /*
  * 通过高度图和 CPatch 数组来描述地面。
  */

#ifndef INCLUDED_TERRAIN
#define INCLUDED_TERRAIN

#include "graphics/HeightMipmap.h"
#include "graphics/SColor.h"
#include "maths/Fixed.h"
#include "maths/Vector3D.h"
#include "ps/CStr.h"

 // 前向声明，避免不必要的头文件包含
class CPatch;
class CMiniPatch;
class CFixedVector3D;
class CBoundingBoxAligned;

///////////////////////////////////////////////////////////////////////////////
// 地形常量:

/// 每个地块在x和z方向上的米数 [世界空间单位]
const ssize_t TERRAIN_TILE_SIZE = 4;

/// 每米包含的u16高度单位数量
const ssize_t HEIGHT_UNITS_PER_METRE = 92;

/// 每个u16高度单位对应的米数
const float HEIGHT_SCALE = 1.f / HEIGHT_UNITS_PER_METRE;

///////////////////////////////////////////////////////////////////////////////
// CTerrain: 主要的地形类；包含描述高程数据的高度图，以及构成地形的更小的子地块
class CTerrain
{
public:
	// 构造函数
	CTerrain();
	// 析构函数
	~CTerrain();

	// 坐标命名约定：世界空间坐标是 float 类型的 x,z；
	// 地块空间坐标是 ssize_t 类型的 i,j。理由：有符号类型可以
	// 更高效地与浮点数相互转换。使用 ssize_t
	// 而不是 int/long 是因为这些是尺寸。

	/**
	 * 初始化地形
	 * @param patchesPerSide 每边的区块（patch）数量
	 * @param ptr 指向高度图数据的指针
	 * @return 如果成功则返回 true
	 */
	bool Initialize(ssize_t patchesPerSide, const u16* ptr);

	// 返回地形边缘的顶点数量
	ssize_t GetVerticesPerSide() const { return m_MapSize; }
	// 返回地形边缘的地块数量
	ssize_t GetTilesPerSide() const { return GetVerticesPerSide() - 1; }
	// 返回地形边缘的区块数量
	ssize_t GetPatchesPerSide() const { return m_MapSizePatches; }

	// 获取X轴最小坐标
	float GetMinX() const { return 0.0f; }
	// 获取Z轴最小坐标
	float GetMinZ() const { return 0.0f; }
	// 获取X轴最大坐标
	float GetMaxX() const { return (float)((m_MapSize - 1) * TERRAIN_TILE_SIZE); }
	// 获取Z轴最大坐标
	float GetMaxZ() const { return (float)((m_MapSize - 1) * TERRAIN_TILE_SIZE); }

	/**
	 * 检查一个点是否在地图上
	 * @param x X坐标
	 * @param z Z坐标
	 * @return 如果在地图上则返回 true
	 */
	bool IsOnMap(float x, float z) const
	{
		return ((x >= GetMinX()) && (x < GetMaxX())
			&& (z >= GetMinZ()) && (z < GetMaxZ()));
	}

	// 获取指定顶点的地面高度
	float GetVertexGroundLevel(ssize_t i, ssize_t j) const;
	// 获取指定顶点的地面高度（定点数版本）
	fixed GetVertexGroundLevelFixed(ssize_t i, ssize_t j) const;
	// 获取指定精确坐标的地面高度（通过插值计算）
	float GetExactGroundLevel(float x, float z) const;
	// 获取指定精确坐标的地面高度（定点数版本）
	fixed GetExactGroundLevelFixed(fixed x, fixed z) const;
	// 获取指定点周围一个半径内经过滤波的地面高度
	float GetFilteredGroundLevel(float x, float z, float radius) const;

	// 获取一个地块的大致坡度
	// (0 = 水平, 0.5 = 30度, 1.0 = 45度, 等等)
	fixed GetSlopeFixed(ssize_t i, ssize_t j) const;

	// 获取一个点的精确坡度，考虑了三角化方向
	fixed GetExactSlopeFixed(fixed x, fixed z) const;

	// 如果地块 (i, j) 的三角化对角线方向应为 (1,-1)，则返回 true；
	// 如果方向为 (1,1)，则返回 false
	bool GetTriangulationDir(ssize_t i, ssize_t j) const;

	// 调整此地形的大小，使每边具有给定的区块数量，
	// 并以区块为单位，相对于源中心进行中心偏移。
	void ResizeAndOffset(ssize_t size, ssize_t horizontalOffset = 0, ssize_t verticalOffset = 0);

	// 从16位数据设置一个新的高度图；假定高度图与当前地形大小匹配
	void SetHeightMap(u16* heightmap);
	// 返回一个指向高度图的指针
	u16* GetHeightMap() const { return m_Heightmap; }

	// 获取给定坐标（以区块空间表示）的区块；如果
	// 坐标代表地图边缘之外的区块，则返回 0
	CPatch* GetPatch(ssize_t i, ssize_t j) const;
	// 获取给定坐标（以地块空间表示）的地块；如果
	// 坐标代表地图边缘之外的地块，则返回 0
	CMiniPatch* GetTile(ssize_t i, ssize_t j) const;

	// 计算给定顶点的三维位置
	void CalcPosition(ssize_t i, ssize_t j, CVector3D& pos) const;
	void CalcPositionFixed(ssize_t i, ssize_t j, CFixedVector3D& pos) const;
	// 计算给定位置下的顶点（向下取整坐标）
	static void CalcFromPosition(const CVector3D& pos, ssize_t& i, ssize_t& j)
	{
		i = (ssize_t)(pos.X / TERRAIN_TILE_SIZE);
		j = (ssize_t)(pos.Z / TERRAIN_TILE_SIZE);
	}
	// 计算给定位置下的顶点（向下取整坐标）
	static void CalcFromPosition(float x, float z, ssize_t& i, ssize_t& j)
	{
		i = (ssize_t)(x / TERRAIN_TILE_SIZE);
		j = (ssize_t)(z / TERRAIN_TILE_SIZE);
	}
	// 计算给定顶点的法线
	void CalcNormal(ssize_t i, ssize_t j, CVector3D& normal) const;
	void CalcNormalFixed(ssize_t i, ssize_t j, CFixedVector3D& normal) const;

	/**
	 * 计算指定精确坐标的法线向量
	 * @param x X坐标
	 * @param z Z坐标
	 * @return 法线向量
	 */
	CVector3D CalcExactNormal(float x, float z) const;

	// 将一个特定的地块正方形区域（包含下界，不包含上界）
	// 标记为“脏” - 在修改高度图后使用此函数。
	// 如果你修改了一个顶点 (i,j)，你应该将从 (i-1, j-1) [包含] 到
	// (i+1, j+1) [不包含] 的地块标记为脏，因为它们的几何形状依赖于该顶点。
	// 如果你修改了一个地块 (i,j)，你应该将从 (i-1, j-1) [包含] 到
	// (i+2, j+2) [不包含] 的地块标记为脏，因为它们的纹理混合依赖于该地块。
	void MakeDirty(ssize_t i0, ssize_t j0, ssize_t i1, ssize_t j1, int dirtyFlags);
	// 将整个地图标记为“脏”
	void MakeDirty(int dirtyFlags);

	/**
	 * 返回一个包含给定顶点范围（包含）的三维包围盒
	 */
	CBoundingBoxAligned GetVertexesBound(ssize_t i0, ssize_t j0, ssize_t i1, ssize_t j1);

	// 获取地形的基础颜色（通常是纯白色 - 其他颜色
	// 会与视野（LOS）产生不良交互 - 但在 Actor Viewer 工具中使用）
	SColor4ub GetBaseColor() const { return m_BaseColor; }
	// 设置地形的基础颜色
	void SetBaseColor(SColor4ub color) { m_BaseColor = color; }

	/**
	 * 获取高度图的多级渐进纹理（mipmap）
	 * @return 对高度图 mipmap 的常量引用
	 */
	const CHeightMipmap& GetHeightMipmap() const { return m_HeightMipmap; }

private:
	// 删除此地形分配的任何数据
	void ReleaseData();
	// 设置区块指针等
	void InitialisePatches();

	// 以顶点为单位，此地图每个方向的大小；即总地块数 = sqr(m_MapSize-1)
	ssize_t m_MapSize;
	// 以区块为单位，此地图每个方向的大小；总区块数 = sqr(m_MapSizePatches)
	ssize_t m_MapSizePatches;
	// 构成此地形的区块数组
	CPatch* m_Patches;
	// 16位高度图数据
	u16* m_Heightmap;
	// 基础颜色（通常为白色）
	SColor4ub m_BaseColor;
	// 高度图的多级渐进纹理
	CHeightMipmap m_HeightMipmap;
};

#endif // INCLUDED_TERRAIN
