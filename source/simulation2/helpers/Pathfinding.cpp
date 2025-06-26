/* Copyright (C) 2021 Wildfire Games.
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

#include "precompiled.h"

#include "Pathfinding.h"

#include "graphics/Terrain.h"
#include "ps/CLogger.h"
#include "simulation2/helpers/Grid.h"
#include "simulation2/system/ParamNode.h"

 /**
  * 寻路相关的辅助函数实现
  */
namespace Pathfinding
{
	/**
	 * 检查一条移动路线是否穿过不可通行的地形
	 * @param x0, z0 起点坐标
	 * @param x1, z1 终点坐标
	 * @param passClass 通行性类别
	 * @param grid 通行性网格
	 * @return 如果路线有效则返回 true，否则返回 false
	 */
	bool CheckLineMovement(entity_pos_t x0, entity_pos_t z0, entity_pos_t x1, entity_pos_t z1,
		pass_class_t passClass, const Grid<NavcellData>& grid)
	{
		// 我们不应允许线段连接对角相邻的导航单元格。
		// 线段是否精确地沿着一个不可通行导航单元格的边缘移动并不重要。

		// 要光栅化这条线：
		// 如果这条线（例如）朝向右上，那么我们从包含起点的导航单元格开始，
		// 这条线必须要么在该导航单元格内结束，要么沿着上边缘或右边缘退出
		//（或穿过右上角，我们任意地将其视为水平边缘）。
		// 所以我们跳到该边缘对面的相邻导航单元格，然后继续。

		// 为了处理单位卡在不可通行单元格的特殊情况，
		// 我们允许它们从一个不可通行的单元格移动到一个可通行的单元格（但反之不行）。

		u16 i0, j0, i1, j1;
		NearestNavcell(x0, z0, i0, j0, grid.m_W, grid.m_H);
		NearestNavcell(x1, z1, i1, j1, grid.m_W, grid.m_H);

		// 找出线的走向
		int di = (i0 < i1 ? +1 : i1 < i0 ? -1 : 0);
		int dj = (j0 < j1 ? +1 : j1 < j0 ? -1 : 0);

		u16 i = i0;
		u16 j = j0;

		// 记录起点是否在不可通行的单元格上
		bool currentlyOnImpassable = !IS_PASSABLE(grid.get(i0, j0), passClass);

		while (true)
		{
			// 确保我们仍在界限内
			ENSURE(
				((di > 0 && i0 <= i && i <= i1) || (di < 0 && i1 <= i && i <= i0) || (di == 0 && i == i0)) &&
				((dj > 0 && j0 <= j && j <= j1) || (dj < 0 && j1 <= j && j <= j0) || (dj == 0 && j == j0)));

			// 如果我们移动到一个不可通行的导航单元格上，则失败
			bool passable = IS_PASSABLE(grid.get(i, j), passClass);
			if (passable)
				currentlyOnImpassable = false; // 进入了可通行区域
			else if (!currentlyOnImpassable) // 如果之前在可通行区域，现在进入了不可通行区域
				return false; // 失败

			// 如果我们到达了目标，则成功
			if (i == i1 && j == j1)
				return true;

			// 如果我们只能水平/垂直移动，那么就直接朝那个方向移动
			// 如果我们到达了边界，我们可以直接跳到终点
			if (di == 0 || i == i1)
			{
				j += dj;
				continue;
			}
			else if (dj == 0 || j == j1)
			{
				i += di;
				continue;
			}

			// 否则我们需要检查要移动到哪个单元格：

			// 检查线是否与当前导航单元格的水平（上/下）边缘相交。
			// 水平边缘是 (i, j + (dj>0?1:0)) .. (i + 1, j + (dj>0?1:0))
			// 因为我们已经知道线正在从这个导航单元格移动到另一个不同的
			// 导航单元格，我们只需测试边缘的端点不都在线的
			// 同一侧。

			// 如果我们精确地穿过网格的一个顶点，我们会得到 dota 或 dotb 等于
			// 0。在这种情况下，我们任意选择移动 dj。
			// 这只有在我们预先处理了 (i == i1 || j == j1) 的情况下才有效。
			// 否则我们可能会超出j的限制，永远无法到达最终的导航单元格。

			entity_pos_t xia = entity_pos_t::FromInt(i).Multiply(Pathfinding::NAVCELL_SIZE);
			entity_pos_t xib = entity_pos_t::FromInt(i + 1).Multiply(Pathfinding::NAVCELL_SIZE);
			entity_pos_t zj = entity_pos_t::FromInt(j + (dj + 1) / 2).Multiply(Pathfinding::NAVCELL_SIZE);

			CFixedVector2D perp = CFixedVector2D(x1 - x0, z1 - z0).Perpendicular();
			entity_pos_t dota = (CFixedVector2D(xia, zj) - CFixedVector2D(x0, z0)).Dot(perp);
			entity_pos_t dotb = (CFixedVector2D(xib, zj) - CFixedVector2D(x0, z0)).Dot(perp);

			// 如果水平边缘完全在线的一侧，因此线不
			// 与其相交，我们应该改为穿过垂直边缘
			if ((dota < entity_pos_t::Zero() && dotb < entity_pos_t::Zero()) ||
				(dota > entity_pos_t::Zero() && dotb > entity_pos_t::Zero()))
				i += di;
			else
				j += dj;
		}
	}
} // 命名空间 Pathfinding 结束

/**
 * PathfinderPassability 类的构造函数
 * @param mask 通行性类别的位掩码
 * @param node 从 XML 加载数据的参数节点
 */
PathfinderPassability::PathfinderPassability(pass_class_t mask, const CParamNode& node) : m_Mask(mask)
{
	// 从XML节点读取最小水深，如果不存在则设为最小值
	if (node.GetChild("MinWaterDepth").IsOk())
		m_MinDepth = node.GetChild("MinWaterDepth").ToFixed();
	else
		m_MinDepth = std::numeric_limits<fixed>::min();

	// 从XML节点读取最大水深，如果不存在则设为最大值
	if (node.GetChild("MaxWaterDepth").IsOk())
		m_MaxDepth = node.GetChild("MaxWaterDepth").ToFixed();
	else
		m_MaxDepth = std::numeric_limits<fixed>::max();

	// 从XML节点读取最大地形坡度，如果不存在则设为最大值
	if (node.GetChild("MaxTerrainSlope").IsOk())
		m_MaxSlope = node.GetChild("MaxTerrainSlope").ToFixed();
	else
		m_MaxSlope = std::numeric_limits<fixed>::max();

	// 从XML节点读取最小离岸距离，如果不存在则设为最小值
	if (node.GetChild("MinShoreDistance").IsOk())
		m_MinShore = node.GetChild("MinShoreDistance").ToFixed();
	else
		m_MinShore = std::numeric_limits<fixed>::min();

	// 从XML节点读取最大离岸距离，如果不存在则设为最大值
	if (node.GetChild("MaxShoreDistance").IsOk())
		m_MaxShore = node.GetChild("MaxShoreDistance").ToFixed();
	else
		m_MaxShore = std::numeric_limits<fixed>::max();

	// 从XML节点读取单位间隙
	if (node.GetChild("Clearance").IsOk())
	{
		m_Clearance = node.GetChild("Clearance").ToFixed();

		/* 根据设计原始文档的 Philip（见 docs/pathfinder.pdf），
		 * 间隙通常应该是整数，以确保光栅化
		 * 通行性地图时行为一致。
		 * 我对此表示怀疑，而且我的寻路器修复使得0.8的间隙非常方便，
		 * 所以我注释掉了这个检查，但把它留在这里作为文档，以防出现bug。

		 if (!(m_Clearance % Pathfinding::NAVCELL_SIZE).IsZero())
		 {
		 // 如果间隙不是导航单元格的整数倍，那么我们
		 // 在按间隙扩展导航单元格网格时，相对于按间隙
		 // 扩展静态障碍物，可能会得到奇怪的行为
		 LOGWARNING("Pathfinder passability class has clearance %f, should be multiple of %f",
		 m_Clearance.ToFloat(), Pathfinding::NAVCELL_SIZE.ToFloat());
		 }*/
	}
	else
		m_Clearance = fixed::Zero();

	// 从XML节点读取障碍物处理方式
	if (node.GetChild("Obstructions").IsOk())
	{
		const std::string& obstructions = node.GetChild("Obstructions").ToString();
		if (obstructions == "none")
			m_Obstructions = NONE;
		else if (obstructions == "pathfinding")
			m_Obstructions = PATHFINDING;
		else if (obstructions == "foundation")
			m_Obstructions = FOUNDATION;
		else
		{
			LOGERROR("Invalid value for Obstructions in pathfinder.xml for pass class %d", mask);
			m_Obstructions = NONE;
		}
	}
	else
		m_Obstructions = NONE;
}
