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

 /**
  * @file
  * @brief CCmpPathfinder 的基于顶点的寻路算法。
  * * 该算法计算围绕矩形障碍物角落的路径。
  *
  * @note 关于此算法的一个有用的搜索关键词是："points of visibility"（可见性点）。
  *
  * 由于我们有时希望使用此算法来避开移动单位，因此没有
  * 预计算环节 - 整个可见性图在每次寻路时都会被有效地重新生成，
  * 并在该图上执行 A* 搜索。
  *
  * 此算法的性能随障碍物数量的增加而急剧下降，因此应在
  * 有限的范围内使用，并且不应过于频繁地调用。
  */

#include "precompiled.h"

#include "VertexPathfinder.h"

#include "lib/timer.h"
#include "ps/Profile.h"
#include "renderer/Scene.h"
#include "simulation2/components/ICmpObstructionManager.h"
#include "simulation2/helpers/Grid.h"
#include "simulation2/helpers/PriorityQueue.h"
#include "simulation2/helpers/Render.h"
#include "simulation2/system/SimContext.h"

#include <mutex>

namespace
{
	// 全局互斥锁，用于保护调试绘图数据的线程安全
	static std::mutex g_DebugMutex;
}

// 全局的顶点寻路器调试覆盖层对象，用于在屏幕上渲染调试信息
VertexPathfinderDebugOverlay g_VertexPathfinderDebugOverlay;

/* 象限优化:
 * (大致基于 GPG2 "Optimizing Points-of-Visibility Pathfinding" 一文)
 *
 * 考虑一个轴对齐矩形 ("#") 角落处的顶点 ("@"):
 *
 * TL  :  TR   (左上 : 右上)
 *     :
 * ####@ - - -
 * #####
 * #####
 * BL ##  BR   (左下 : 右下)
 *
 * 顶点周围的区域被划分为左上(TopLeft)、右下(BottomRight)等象限。
 *
 * 如果最短路径到达此顶点，它不能继续前往 BL (左下) 象限中的顶点
 * (因为它会被障碍物本身阻挡)。
 * 由于最短路径是紧紧环绕障碍物边缘的，
 * 如果路径从 TL (左上) 象限接近此顶点，
 * 它也不能继续前往 TL (左上) 或 TR (右上) 象限 (否则路径可以跳过此顶点以变得更短)。
 * 因此，它必须继续前往 BR (右下) 象限的顶点 (这样，当前顶点就位于
 * *那个*新顶点的 TL (左上) 象限)。
 *
 * 这使我们能够通过快速丢弃来自错误象限的顶点来显著减少搜索空间。
 *
 * (如果路径起点在障碍物内部，这会导致问题，所以我们为此情况添加了一些特殊处理。)
 *
 * (对于非轴对齐的矩形，进行这种计算更为困难，所以我们
 * 不对它们进行任何丢弃处理。)
 */
static const u8 QUADRANT_NONE = 0;   // 无象限
static const u8 QUADRANT_BL = 1;     // 左下象限 (Bottom-Left)
static const u8 QUADRANT_TR = 2;     // 右上象限 (Top-Right)
static const u8 QUADRANT_TL = 4;     // 左上象限 (Top-Left)
static const u8 QUADRANT_BR = 8;     // 右下象限 (Bottom-Right)
static const u8 QUADRANT_BLTR = QUADRANT_BL | QUADRANT_TR; // 左下和右上组合
static const u8 QUADRANT_TLBR = QUADRANT_TL | QUADRANT_BR; // 左上和右下组合
static const u8 QUADRANT_ALL = QUADRANT_BLTR | QUADRANT_TLBR; // 所有象限

// 当计算要插入搜索图的顶点时，
// 添加一个小的增量，这样边的顶点就不会因为
// 微小的数值误差而被解释为穿越了该边。
static const entity_pos_t EDGE_EXPAND_DELTA = entity_pos_t::FromInt(1) / 16;

/**
 * @brief 检查从 'a' 点到 'b' 点的射线是否穿过任何给定的边。
 * (边是单向的，所以只有从正面到背面的穿越才被认为是交叉。)
 */
inline static bool CheckVisibility(const CFixedVector2D& a, const CFixedVector2D& b, const std::vector<Edge>& edges)
{
	// 计算射线ab的法线向量
	CFixedVector2D abn = (b - a).Perpendicular();

	// 遍历所有非轴对齐的边
	for (size_t i = 0; i < edges.size(); ++i)
	{
		CFixedVector2D p0 = edges[i].p0;
		CFixedVector2D p1 = edges[i].p1;

		// 计算边的法线向量
		CFixedVector2D d = (p1 - p0).Perpendicular();

		// 如果点 'a' 在边的背面，射线不可能从正面穿到背面
		fixed q = (a - p0).Dot(d);
		if (q < fixed::Zero())
			continue;

		// 如果点 'b' 在边的正面，射线不可能从正面穿到背面
		fixed r = (b - p0).Dot(d);
		if (r > fixed::Zero())
			continue;

		// 此时，射线正在穿越无限延伸的边（从正面到背面）。
		// 接下来检查有限长度的边是否也与无限延伸的射线相交。
		// (基于前面的测试，它只能在一个方向上交叉。)
		fixed s = (p0 - a).Dot(abn);
		if (s > fixed::Zero()) // 边的p0点在射线的“右侧”
			continue;

		fixed t = (p1 - a).Dot(abn);
		if (t < fixed::Zero()) // 边的p1点在射线的“左侧”
			continue;

		// 如果边的两个端点在射线的两侧，说明相交
		return false; // 不可见
	}

	return true; // 可见
}

// 为轴对齐的边单独处理（为了性能）：
// (这些是通用非对齐边代码的特化版本。
// 它们假设调用者已经排除了点 'a' 位于
// 错误一侧的边。)

// 检查是否与左侧的垂直边相交
inline static bool CheckVisibilityLeft(const CFixedVector2D& a, const CFixedVector2D& b, const std::vector<EdgeAA>& edges)
{
	// 如果射线向左移动或垂直移动，则不可能与左侧的障碍边相交
	if (a.X >= b.X)
		return true;

	CFixedVector2D abn = (b - a).Perpendicular();

	for (size_t i = 0; i < edges.size(); ++i)
	{
		// 如果目标点b在边的左侧，不可能相交
		if (b.X < edges[i].p0.X)
			continue;

		// 检查边的两个端点是否在射线的两侧
		CFixedVector2D p0(edges[i].p0.X, edges[i].c1);
		fixed s = (p0 - a).Dot(abn);
		if (s > fixed::Zero())
			continue;

		CFixedVector2D p1(edges[i].p0.X, edges[i].p0.Y);
		fixed t = (p1 - a).Dot(abn);
		if (t < fixed::Zero())
			continue;

		return false; // 不可见
	}

	return true; // 可见
}

// 检查是否与右侧的垂直边相交
inline static bool CheckVisibilityRight(const CFixedVector2D& a, const CFixedVector2D& b, const std::vector<EdgeAA>& edges)
{
	// 如果射线向右移动或垂直移动，则不可能与右侧的障碍边相交
	if (a.X <= b.X)
		return true;

	CFixedVector2D abn = (b - a).Perpendicular();

	for (size_t i = 0; i < edges.size(); ++i)
	{
		// 如果目标点b在边的右侧，不可能相交
		if (b.X > edges[i].p0.X)
			continue;

		// 检查边的两个端点是否在射线的两侧
		CFixedVector2D p0(edges[i].p0.X, edges[i].c1);
		fixed s = (p0 - a).Dot(abn);
		if (s > fixed::Zero())
			continue;

		CFixedVector2D p1(edges[i].p0.X, edges[i].p0.Y);
		fixed t = (p1 - a).Dot(abn);
		if (t < fixed::Zero())
			continue;

		return false; // 不可见
	}

	return true; // 可见
}

// 检查是否与底部的水平边相交
inline static bool CheckVisibilityBottom(const CFixedVector2D& a, const CFixedVector2D& b, const std::vector<EdgeAA>& edges)
{
	// 如果射线向上移动或水平移动，则不可能与底部的障碍边相交
	if (a.Y >= b.Y)
		return true;

	CFixedVector2D abn = (b - a).Perpendicular();

	for (size_t i = 0; i < edges.size(); ++i)
	{
		// 如果目标点b在边的下方，不可能相交
		if (b.Y < edges[i].p0.Y)
			continue;

		// 检查边的两个端点是否在射线的两侧
		CFixedVector2D p0(edges[i].p0.X, edges[i].p0.Y);
		fixed s = (p0 - a).Dot(abn);
		if (s > fixed::Zero())
			continue;

		CFixedVector2D p1(edges[i].c1, edges[i].p0.Y);
		fixed t = (p1 - a).Dot(abn);
		if (t < fixed::Zero())
			continue;

		return false; // 不可见
	}

	return true; // 可见
}

// 检查是否与顶部的水平边相交
inline static bool CheckVisibilityTop(const CFixedVector2D& a, const CFixedVector2D& b, const std::vector<EdgeAA>& edges)
{
	// 如果射线向下移动或水平移动，则不可能与顶部的障碍边相交
	if (a.Y <= b.Y)
		return true;

	CFixedVector2D abn = (b - a).Perpendicular();

	for (size_t i = 0; i < edges.size(); ++i)
	{
		// 如果目标点b在边的上方，不可能相交
		if (b.Y > edges[i].p0.Y)
			continue;

		// 检查边的两个端点是否在射线的两侧
		CFixedVector2D p0(edges[i].p0.X, edges[i].p0.Y);
		fixed s = (p0 - a).Dot(abn);
		if (s > fixed::Zero())
			continue;

		CFixedVector2D p1(edges[i].c1, edges[i].p0.Y);
		fixed t = (p1 - a).Dot(abn);
		if (t < fixed::Zero())
			continue;

		return false; // 不可见
	}

	return true; // 可见
}

// 定义用于A*搜索的顶点优先队列
typedef PriorityQueueHeap<u16, fixed, fixed> VertexPriorityQueue;

/**
 * @brief 添加边和顶点来表示可通过和不可通过的导航单元之间的边界（用于不可通过的地形）。
 * * 将会考虑 i0 <= i <= i1, j0 <= j <= j1 范围内的导航单元。
 */
static void AddTerrainEdges(std::vector<Edge>& edges, std::vector<Vertex>& vertexes,
	int i0, int j0, int i1, int j1,
	pass_class_t passClass, const Grid<NavcellData>& grid)
{

	// 将坐标限制在网格内部，这样我们就不会尝试在网格之外采样。
	// (这假设最外圈的导航单元（总是不可通过）
	// 不会与任何可通过的导航单元有边界。TODO: 这是否足够安全？)

	i0 = Clamp(i0, 1, grid.m_W - 2);
	j0 = Clamp(j0, 1, grid.m_H - 2);
	i1 = Clamp(i1, 1, grid.m_W - 2);
	j1 = Clamp(j1, 1, grid.m_H - 2);

	// 遍历指定区域的导航网格
	for (int j = j0; j <= j1; ++j)
	{
		for (int i = i0; i <= i1; ++i)
		{
			// 如果当前单元格可通过，则跳过
			if (IS_PASSABLE(grid.get(i, j), passClass))
				continue;

			// 在不可通行的单元格周围寻找“内凹”的角落，这些角落可以作为寻路图的顶点。
			// 这种角落是三个可通过单元格和一个不可通过单元格汇集的地方。

			// 检查右下角是否是内凹顶点
			if (IS_PASSABLE(grid.get(i + 1, j), passClass) && IS_PASSABLE(grid.get(i, j + 1), passClass) && IS_PASSABLE(grid.get(i + 1, j + 1), passClass))
			{
				Vertex vert;
				vert.status = Vertex::UNEXPLORED;
				vert.quadOutward = QUADRANT_ALL; // 从地形顶点出发可以去任何象限
				vert.quadInward = QUADRANT_BL;   // 接近这个顶点需要从左下方来
				vert.p = CFixedVector2D(fixed::FromInt(i + 1) + EDGE_EXPAND_DELTA, fixed::FromInt(j + 1) + EDGE_EXPAND_DELTA).Multiply(Pathfinding::NAVCELL_SIZE);
				vertexes.push_back(vert);
			}

			// 检查左下角
			if (IS_PASSABLE(grid.get(i - 1, j), passClass) && IS_PASSABLE(grid.get(i, j + 1), passClass) && IS_PASSABLE(grid.get(i - 1, j + 1), passClass))
			{
				Vertex vert;
				vert.status = Vertex::UNEXPLORED;
				vert.quadOutward = QUADRANT_ALL;
				vert.quadInward = QUADRANT_BR;
				vert.p = CFixedVector2D(fixed::FromInt(i) - EDGE_EXPAND_DELTA, fixed::FromInt(j + 1) + EDGE_EXPAND_DELTA).Multiply(Pathfinding::NAVCELL_SIZE);
				vertexes.push_back(vert);
			}

			// 检查右上角
			if (IS_PASSABLE(grid.get(i + 1, j), passClass) && IS_PASSABLE(grid.get(i, j - 1), passClass) && IS_PASSABLE(grid.get(i + 1, j - 1), passClass))
			{
				Vertex vert;
				vert.status = Vertex::UNEXPLORED;
				vert.quadOutward = QUADRANT_ALL;
				vert.quadInward = QUADRANT_TL;
				vert.p = CFixedVector2D(fixed::FromInt(i + 1) + EDGE_EXPAND_DELTA, fixed::FromInt(j) - EDGE_EXPAND_DELTA).Multiply(Pathfinding::NAVCELL_SIZE);
				vertexes.push_back(vert);
			}

			// 检查左上角
			if (IS_PASSABLE(grid.get(i - 1, j), passClass) && IS_PASSABLE(grid.get(i, j - 1), passClass) && IS_PASSABLE(grid.get(i - 1, j - 1), passClass))
			{
				Vertex vert;
				vert.status = Vertex::UNEXPLORED;
				vert.quadOutward = QUADRANT_ALL;
				vert.quadInward = QUADRANT_TR;
				vert.p = CFixedVector2D(fixed::FromInt(i) - EDGE_EXPAND_DELTA, fixed::FromInt(j) - EDGE_EXPAND_DELTA).Multiply(Pathfinding::NAVCELL_SIZE);
				vertexes.push_back(vert);
			}
		}
	}

	// XXX 重写这部分代码
	// 这部分代码通过扫描行和列来合并连续的不可通行单元格，形成更长的边界边。
	// 这比为每个单元格创建单独的边更有效率。

	// 处理水平方向的边
	std::vector<u16> segmentsR; // 面向右侧的边段（即，下方可通过，上方不可通过）
	std::vector<u16> segmentsL; // 面向左侧的边段（即，上方可通过，下方不可通过）
	for (int j = j0; j < j1; ++j)
	{
		segmentsR.clear();
		segmentsL.clear();
		for (int i = i0; i <= i1; ++i)
		{
			bool a = IS_PASSABLE(grid.get(i, j + 1), passClass); // 上方单元格
			bool b = IS_PASSABLE(grid.get(i, j), passClass);   // 下方单元格
			if (a && !b)
				segmentsL.push_back(i);
			if (b && !a)
				segmentsR.push_back(i);
		}

		// 合并连续的 R 段
		if (!segmentsR.empty())
		{
			segmentsR.push_back(0); // 使用哨兵值来简化循环逻辑
			u16 ia = segmentsR[0];
			u16 ib = ia + 1;
			for (size_t n = 1; n < segmentsR.size(); ++n)
			{
				if (segmentsR[n] == ib)
					++ib; // 连续的段，扩展结束点
				else
				{
					// 段已结束，创建边
					CFixedVector2D v0 = CFixedVector2D(fixed::FromInt(ia), fixed::FromInt(j + 1)).Multiply(Pathfinding::NAVCELL_SIZE);
					CFixedVector2D v1 = CFixedVector2D(fixed::FromInt(ib), fixed::FromInt(j + 1)).Multiply(Pathfinding::NAVCELL_SIZE);
					edges.emplace_back(Edge{ v0, v1 });

					// 开始新段
					ia = segmentsR[n];
					ib = ia + 1;
				}
			}
		}

		// 合并连续的 L 段
		if (!segmentsL.empty())
		{
			segmentsL.push_back(0); // 哨兵值
			u16 ia = segmentsL[0];
			u16 ib = ia + 1;
			for (size_t n = 1; n < segmentsL.size(); ++n)
			{
				if (segmentsL[n] == ib)
					++ib;
				else
				{
					CFixedVector2D v0 = CFixedVector2D(fixed::FromInt(ib), fixed::FromInt(j + 1)).Multiply(Pathfinding::NAVCELL_SIZE);
					CFixedVector2D v1 = CFixedVector2D(fixed::FromInt(ia), fixed::FromInt(j + 1)).Multiply(Pathfinding::NAVCELL_SIZE);
					edges.emplace_back(Edge{ v0, v1 });

					ia = segmentsL[n];
					ib = ia + 1;
				}
			}
		}
	}
	// 处理垂直方向的边
	std::vector<u16> segmentsU; // 面向上方的边段 (右侧可通过，左侧不可通过)
	std::vector<u16> segmentsD; // 面向下方的边段 (左侧可通过，右侧不可通过)
	for (int i = i0; i < i1; ++i)
	{
		segmentsU.clear();
		segmentsD.clear();
		for (int j = j0; j <= j1; ++j)
		{
			bool a = IS_PASSABLE(grid.get(i + 1, j), passClass); // 右侧单元格
			bool b = IS_PASSABLE(grid.get(i, j), passClass);   // 左侧单元格
			if (a && !b)
				segmentsU.push_back(j);
			if (b && !a)
				segmentsD.push_back(j);
		}

		// 合并连续的 U 段
		if (!segmentsU.empty())
		{
			segmentsU.push_back(0); // 哨兵值
			u16 ja = segmentsU[0];
			u16 jb = ja + 1;
			for (size_t n = 1; n < segmentsU.size(); ++n)
			{
				if (segmentsU[n] == jb)
					++jb;
				else
				{
					CFixedVector2D v0 = CFixedVector2D(fixed::FromInt(i + 1), fixed::FromInt(ja)).Multiply(Pathfinding::NAVCELL_SIZE);
					CFixedVector2D v1 = CFixedVector2D(fixed::FromInt(i + 1), fixed::FromInt(jb)).Multiply(Pathfinding::NAVCELL_SIZE);
					edges.emplace_back(Edge{ v0, v1 });

					ja = segmentsU[n];
					jb = ja + 1;
				}
			}
		}

		// 合并连续的 D 段
		if (!segmentsD.empty())
		{
			segmentsD.push_back(0); // 哨兵值
			u16 ja = segmentsD[0];
			u16 jb = ja + 1;
			for (size_t n = 1; n < segmentsD.size(); ++n)
			{
				if (segmentsD[n] == jb)
					++jb;
				else
				{
					CFixedVector2D v0 = CFixedVector2D(fixed::FromInt(i + 1), fixed::FromInt(jb)).Multiply(Pathfinding::NAVCELL_SIZE);
					CFixedVector2D v1 = CFixedVector2D(fixed::FromInt(i + 1), fixed::FromInt(ja)).Multiply(Pathfinding::NAVCELL_SIZE);
					edges.emplace_back(Edge{ v0, v1 });

					ja = segmentsD[n];
					jb = ja + 1;
				}
			}
		}
	}
}

/**
 * @brief 根据一个给定的点 a，将障碍边分割成不同的类别，用于优化可见性检查。
 * * @param a 当前正在检查的顶点位置。
 * @param edges 所有的边。
 * @param squares 所有轴对齐的方形障碍物。
 * @param edgesUnaligned [输出] 非轴对齐的边。
 * @param edgesLeft [输出] 位于 a 左侧的垂直边。
 * @param edgesRight [输出] 位于 a 右侧的垂直边。
 * @param edgesBottom [输出] 位于 a下方的水平边。
 * @param edgesTop [输出] 位于 a 上方的水平边。
 */
static void SplitAAEdges(const CFixedVector2D& a,
	const std::vector<Edge>& edges,
	const std::vector<Square>& squares,
	std::vector<Edge>& edgesUnaligned,
	std::vector<EdgeAA>& edgesLeft, std::vector<EdgeAA>& edgesRight,
	std::vector<EdgeAA>& edgesBottom, std::vector<EdgeAA>& edgesTop)
{

	// 遍历所有轴对齐的方形障碍物
	for (const Square& square : squares)
	{
		// 如果点a在方形的左侧或等X坐标，则方形的左边可能阻挡视线
		if (a.X <= square.p0.X)
			edgesLeft.emplace_back(EdgeAA{ square.p0, square.p1.Y });
		// 如果点a在方形的右侧或等X坐标，则方形的右边可能阻挡视线
		if (a.X >= square.p1.X)
			edgesRight.emplace_back(EdgeAA{ square.p1, square.p0.Y });
		// 如果点a在方形的下方或等Y坐标，则方形的底边可能阻挡视线
		if (a.Y <= square.p0.Y)
			edgesBottom.emplace_back(EdgeAA{ square.p0, square.p1.X });
		// 如果点a在方形的上方或等Y坐标，则方形的顶边可能阻挡视线
		if (a.Y >= square.p1.Y)
			edgesTop.emplace_back(EdgeAA{ square.p1, square.p0.X });
	}

	// 遍历所有地形等生成的边
	for (const Edge& edge : edges)
	{
		if (edge.p0.X == edge.p1.X) // 垂直边
		{
			if (edge.p1.Y < edge.p0.Y) // 边朝向右方（法线向右）
			{
				if (!(a.X <= edge.p0.X))
					continue;
				edgesLeft.emplace_back(EdgeAA{ edge.p1, edge.p0.Y });
			}
			else // 边朝向左方（法线向左）
			{
				if (!(a.X >= edge.p0.X))
					continue;
				edgesRight.emplace_back(EdgeAA{ edge.p1, edge.p0.Y });
			}
		}
		else if (edge.p0.Y == edge.p1.Y) // 水平边
		{
			if (edge.p0.X < edge.p1.X) // 边朝向上方（法线向上）
			{
				if (!(a.Y <= edge.p0.Y))
					continue;
				edgesBottom.emplace_back(EdgeAA{ edge.p0, edge.p1.X });
			}
			else // 边朝向下方（法线向下）
			{
				if (!(a.Y >= edge.p0.Y))
					continue;
				edgesTop.emplace_back(EdgeAA{ edge.p0, edge.p1.X });
			}
		}
		else
			// 非轴对齐的边
			edgesUnaligned.push_back(edge);
	}
}

/**
 * @brief 用于根据与一个固定点的近似距离对边-正方形（edge-squares）进行排序的仿函数。
 */
struct SquareSort
{
	CFixedVector2D src;
	SquareSort(CFixedVector2D src) : src(src) { }
	bool operator()(const Square& a, const Square& b) const
	{
		return (a.p0 - src).CompareLength(b.p0 - src) < 0;
	}
};

/**
 * @brief 用于根据与一个固定点的近似距离对非对齐边进行排序的仿函数。
 */
struct UnalignedEdgesSort
{
	CFixedVector2D src;
	UnalignedEdgesSort(CFixedVector2D src) : src(src) { }
	bool operator()(const Edge& a, const Edge& b) const
	{
		return (a.p0 - src).CompareLength(b.p0 - src) < 0;
	}
};

WaypointPath VertexPathfinder::ComputeShortPath(const ShortPathRequest& request, CmpPtr<ICmpObstructionManager> cmpObstructionManager) const
{
	// 性能分析器标签: "计算短路径"
	PROFILE2("ComputeShortPath");

	// 调试渲染目标位置
	g_VertexPathfinderDebugOverlay.DebugRenderGoal(cmpObstructionManager->GetSimContext(), request.goal);

	// 在最大搜索范围边界创建不可通行的边，这样路径就不会逃出我们预定的搜索区域。
	fixed rangeXMin = request.x0 - request.range;
	fixed rangeXMax = request.x0 + request.range;
	fixed rangeZMin = request.z0 - request.range;
	fixed rangeZMax = request.z0 + request.range;

	// 如果目标在边界之外，将搜索空间的中心稍微向目标移动一些，
	// 因为顶点寻路器通常用于绕过前方的实体。
	// (这使得可以使用更小的搜索范围，但仍然能找到好的路径)。
	// 不要对最大的搜索范围这样做：这会使回溯更加困难，而大的搜索域
	// 表明单位可能已经陷入困境，这意味着可能需要回溯。
	// (保持这个值与 unitMotion 的最大搜索范围略小一些)。
	// (这也确保了目标在最大搜索范围内外的对称行为)。
	CFixedVector2D toGoal = CFixedVector2D(request.goal.x, request.goal.z) - CFixedVector2D(request.x0, request.z0);
	if (toGoal.CompareLength(request.range) >= 0 && request.range < Pathfinding::NAVCELL_SIZE * 46)
	{
		fixed toGoalLength = toGoal.Length();
		fixed inv = fixed::FromInt(1) / toGoalLength;
		rangeXMin += (toGoal.Multiply(std::min(toGoalLength / 2, request.range * 3 / 5)).Multiply(inv)).X;
		rangeXMax += (toGoal.Multiply(std::min(toGoalLength / 2, request.range * 3 / 5)).Multiply(inv)).X;
		rangeZMin += (toGoal.Multiply(std::min(toGoalLength / 2, request.range * 3 / 5)).Multiply(inv)).Y;
		rangeZMax += (toGoal.Multiply(std::min(toGoalLength / 2, request.range * 3 / 5)).Multiply(inv)).Y;
	}

	// 添加搜索区域的边界边
	// (这是一个由内向外的正方形，所以边的方向与通常相反。)
	m_Edges.emplace_back(Edge{ CFixedVector2D(rangeXMin, rangeZMin), CFixedVector2D(rangeXMin, rangeZMax) });
	m_Edges.emplace_back(Edge{ CFixedVector2D(rangeXMin, rangeZMax), CFixedVector2D(rangeXMax, rangeZMax) });
	m_Edges.emplace_back(Edge{ CFixedVector2D(rangeXMax, rangeZMax), CFixedVector2D(rangeXMax, rangeZMin) });
	m_Edges.emplace_back(Edge{ CFixedVector2D(rangeXMax, rangeZMin), CFixedVector2D(rangeXMin, rangeZMin) });


	// 将起点添加到图中
	CFixedVector2D posStart(request.x0, request.z0);
	fixed hStart = (posStart - request.goal.NearestPointOnGoal(posStart)).Length();
	Vertex start = { posStart, fixed::Zero(), hStart, 0, Vertex::OPEN, QUADRANT_NONE, QUADRANT_ALL };
	m_Vertexes.push_back(start);
	const size_t START_VERTEX_ID = 0;

	// 将目标顶点添加到图中。
	// 由于目标不总是一个点，这是一个特殊的神奇虚拟顶点，它可以移动——每当
	// 我们从另一个顶点观察它时，它会被移动到目标形状上离该顶点最近的点。
	Vertex end = { CFixedVector2D(request.goal.x, request.goal.z), fixed::Zero(), fixed::Zero(), 0, Vertex::UNEXPLORED, QUADRANT_NONE, QUADRANT_ALL };
	m_Vertexes.push_back(end);
	const size_t GOAL_VERTEX_ID = 1;

	// 找到所有可能影响我们的障碍物方形
	std::vector<ICmpObstructionManager::ObstructionSquare> squares;
	size_t staticShapesNb = 0;
	ControlGroupMovementObstructionFilter filter(request.avoidMovingUnits, request.group);
	// 获取静态障碍物
	cmpObstructionManager->GetStaticObstructionsInRange(filter, rangeXMin - request.clearance, rangeZMin - request.clearance, rangeXMax + request.clearance, rangeZMax + request.clearance, squares);
	staticShapesNb = squares.size();
	// 获取动态单位障碍物
	cmpObstructionManager->GetUnitObstructionsInRange(filter, rangeXMin - request.clearance, rangeZMin - request.clearance, rangeXMax + request.clearance, rangeZMax + request.clearance, squares);

	// 更改数组容量以减少内存重分配
	m_Vertexes.reserve(m_Vertexes.size() + squares.size() * 4);
	m_EdgeSquares.reserve(m_EdgeSquares.size() + squares.size()); // (假设大多数方形是轴对齐的)

	entity_pos_t pathfindClearance = request.clearance;

	// 将每个障碍物方形转换为碰撞边和搜索图顶点
	for (size_t i = 0; i < squares.size(); ++i)
	{
		CFixedVector2D center(squares[i].x, squares[i].z);
		CFixedVector2D u = squares[i].u; // x轴方向向量
		CFixedVector2D v = squares[i].v; // z轴方向向量 (在2D中是y轴)

		// 对动态单位使用稍小的间隙
		if (i >= staticShapesNb)
			pathfindClearance = request.clearance - entity_pos_t::FromInt(1) / 2;

		// 将顶点按移动单位的碰撞半径扩展，以找到
		// 我们可以到达的离它最近的点

		CFixedVector2D hd0(squares[i].hw + pathfindClearance + EDGE_EXPAND_DELTA, squares[i].hh + pathfindClearance + EDGE_EXPAND_DELTA);
		CFixedVector2D hd1(squares[i].hw + pathfindClearance + EDGE_EXPAND_DELTA, -(squares[i].hh + pathfindClearance + EDGE_EXPAND_DELTA));

		// 检查这是否是一个轴对齐的方形
		bool aa = (u.X == fixed::FromInt(1) && u.Y == fixed::Zero() && v.X == fixed::Zero() && v.Y == fixed::FromInt(1));

		// 为每个障碍物的角创建寻路图顶点
		Vertex vert;
		vert.status = Vertex::UNEXPLORED;
		vert.quadInward = QUADRANT_NONE;
		vert.quadOutward = QUADRANT_ALL;

		// 左上角顶点
		vert.p.X = center.X - hd0.Dot(u);
		vert.p.Y = center.Y + hd0.Dot(v);
		if (aa)
		{
			vert.quadInward = QUADRANT_BR; // 要想到达这个角，必须从右下象限来
			vert.quadOutward = (~vert.quadInward) & 0xF; // 可以去往其他三个象限
		}
		if (vert.p.X >= rangeXMin && vert.p.Y >= rangeZMin && vert.p.X <= rangeXMax && vert.p.Y <= rangeZMax)
			m_Vertexes.push_back(vert);

		// 右上角顶点
		vert.p.X = center.X - hd1.Dot(u);
		vert.p.Y = center.Y + hd1.Dot(v);
		if (aa)
		{
			vert.quadInward = QUADRANT_TR;
			vert.quadOutward = (~vert.quadInward) & 0xF;
		}
		if (vert.p.X >= rangeXMin && vert.p.Y >= rangeZMin && vert.p.X <= rangeXMax && vert.p.Y <= rangeZMax)
			m_Vertexes.push_back(vert);

		// 左下角顶点
		vert.p.X = center.X + hd0.Dot(u);
		vert.p.Y = center.Y - hd0.Dot(v);
		if (aa)
		{
			vert.quadInward = QUADRANT_TL;
			vert.quadOutward = (~vert.quadInward) & 0xF;
		}
		if (vert.p.X >= rangeXMin && vert.p.Y >= rangeZMin && vert.p.X <= rangeXMax && vert.p.Y <= rangeZMax)
			m_Vertexes.push_back(vert);

		// 右下角顶点
		vert.p.X = center.X + hd1.Dot(u);
		vert.p.Y = center.Y - hd1.Dot(v);
		if (aa)
		{
			vert.quadInward = QUADRANT_BL;
			vert.quadOutward = (~vert.quadInward) & 0xF;
		}
		if (vert.p.X >= rangeXMin && vert.p.Y >= rangeZMin && vert.p.X <= rangeXMax && vert.p.Y <= rangeZMax)
			m_Vertexes.push_back(vert);

		// 添加边:

		CFixedVector2D h0(squares[i].hw + pathfindClearance, squares[i].hh + pathfindClearance);
		CFixedVector2D h1(squares[i].hw + pathfindClearance, -(squares[i].hh + pathfindClearance));

		CFixedVector2D ev0(center.X - h0.Dot(u), center.Y + h0.Dot(v));
		CFixedVector2D ev1(center.X - h1.Dot(u), center.Y + h1.Dot(v));
		CFixedVector2D ev2(center.X + h0.Dot(u), center.Y - h0.Dot(v));
		CFixedVector2D ev3(center.X + h1.Dot(u), center.Y - h1.Dot(v));
		if (aa)
			// 对于轴对齐的方形，直接存储为特殊结构以优化
			m_EdgeSquares.emplace_back(Square{ ev1, ev3 });
		else
		{
			// 对于非轴对齐的，存储为四条独立的边
			m_Edges.emplace_back(Edge{ ev0, ev1 });
			m_Edges.emplace_back(Edge{ ev1, ev2 });
			m_Edges.emplace_back(Edge{ ev2, ev3 });
			m_Edges.emplace_back(Edge{ ev3, ev0 });
		}

	}

	// 添加地形障碍物
	{
		u16 i0, j0, i1, j1;
		Pathfinding::NearestNavcell(rangeXMin, rangeZMin, i0, j0, m_GridSize, m_GridSize);
		Pathfinding::NearestNavcell(rangeXMax, rangeZMax, i1, j1, m_GridSize, m_GridSize);
		AddTerrainEdges(m_Edges, m_Vertexes, i0, j0, i1, j1, request.passClass, *m_TerrainOnlyGrid);
	}

	// 剔除位于 edgeSquare 内部的顶点（即，显然无法到达的）
	for (size_t i = 2; i < m_EdgeSquares.size(); ++i)
	{
		// 如果起点在方形内部，则忽略该方形（允许从内部出来）
		if (start.p.X >= m_EdgeSquares[i].p0.X &&
			start.p.Y >= m_EdgeSquares[i].p0.Y &&
			start.p.X <= m_EdgeSquares[i].p1.X &&
			start.p.Y <= m_EdgeSquares[i].p1.Y)
			continue;

		// 移除所有在 edgeSquare 内部的非起点/目标顶点；
		// 由于 remove() 效率低下，我们只是将其状态标记为已关闭。
		for (size_t j = 2; j < m_Vertexes.size(); ++j)
			if (m_Vertexes[j].p.X >= m_EdgeSquares[i].p0.X &&
				m_Vertexes[j].p.Y >= m_EdgeSquares[i].p0.Y &&
				m_Vertexes[j].p.X <= m_EdgeSquares[i].p1.X &&
				m_Vertexes[j].p.Y <= m_EdgeSquares[i].p1.Y)
				m_Vertexes[j].status = Vertex::CLOSED;
	}

	// 确保顶点数量没有超过u16的最大值，因为我们用u16存储数组索引
	ENSURE(m_Vertexes.size() < 65536);

	// 调试渲染寻路图
	g_VertexPathfinderDebugOverlay.DebugRenderGraph(cmpObstructionManager->GetSimContext(), m_Vertexes, m_Edges, m_EdgeSquares);

	// 在顶点/可见性图上执行 A* 搜索:

	// 因为我们只是测量欧几里得距离，启发函数是可采纳的（admissible），
	// 所以我们永远不需要在节点被移到关闭集后重新检查它。

	// 添加一个权重，因为我们极度偏好速度而非最优性。
	// 一般来说，如果我们正在运行顶点寻路器，
	// 这意味着在宏观层面上我们已经处于次优情况。
	// 找到的路径最多会比最优路径差这么多倍，这可能是可以接受的。
	fixed heuristicWeight = fixed::FromInt(1);
	// 如果我们有很多边要检查，就更大幅度地放宽约束。
	if (m_Edges.size() > 100 || m_EdgeSquares.size() > 100)
		heuristicWeight = heuristicWeight.MulDiv(fixed::FromInt(5), fixed::FromInt(3));
	else
		heuristicWeight = heuristicWeight.MulDiv(fixed::FromInt(4), fixed::FromInt(3));

	// 为了在常见情况下节省时间，我们不预先计算顶点之间有效边的图；
	// 我们是懒加载地进行。当搜索算法到达一个顶点时，我们检查所有其他
	// 顶点，看是否可以在不碰到任何碰撞边的情况下到达它，并忽略那些
	// 我们无法到达的。由于算法只能到达一个顶点一次（然后它会被标记为
	// 已关闭），我们不会进行任何冗余的可见性计算。

	VertexPriorityQueue open; // A* 的开放列表
	VertexPriorityQueue::Item qiStart = { START_VERTEX_ID, start.h, start.h };
	open.push(qiStart);

	u16 idBest = START_VERTEX_ID; // 记录最佳顶点的ID，以防找不到目标
	fixed hBest = start.h;        // 记录最佳顶点的启发值

	while (!open.empty())
	{
		// 将开放列表中最好的节点（成本最低）移到关闭列表
		VertexPriorityQueue::Item curr = open.pop();
		m_Vertexes[curr.id].status = Vertex::CLOSED;

		// 如果我们已经到达目的地，停止搜索
		if (curr.id == GOAL_VERTEX_ID)
		{
			idBest = curr.id;
			break;
		}

		// 按距离对边进行排序，以便首先检查那些最有可能阻挡射线的边。
		// 基于距离的启发式非常粗略，特别是对于较远的方形障碍物；
		// 我们也只对最近的方形感兴趣，因为只有它们才会阻挡大量的射线。
		// 因此，我们只进行部分排序；阈值只是一个比较合理的值。
		if (m_EdgeSquares.size() > 8)
			std::partial_sort(m_EdgeSquares.begin(), m_EdgeSquares.begin() + 8, m_EdgeSquares.end(), SquareSort(m_Vertexes[curr.id].p));

		// 清空并重新填充用于优化可见性检查的边列表
		m_EdgesUnaligned.clear();
		m_EdgesLeft.clear();
		m_EdgesRight.clear();
		m_EdgesBottom.clear();
		m_EdgesTop.clear();
		SplitAAEdges(m_Vertexes[curr.id].p, m_Edges, m_EdgeSquares, m_EdgesUnaligned, m_EdgesLeft, m_EdgesRight, m_EdgesBottom, m_EdgesTop);

		// 对非对齐边也进行同样的部分排序，这在例如森林中很有帮助。
		// (使用更高的阈值，因为这里我们需要考虑所有4条边，
		// 而且检查非对齐可见性非常慢，所以这样做可能是值得的)。
		if (m_EdgesUnaligned.size() > 28)
			std::partial_sort(m_EdgesUnaligned.begin(), m_EdgesUnaligned.begin() + 28, m_EdgesUnaligned.end(), UnalignedEdgesSort(m_Vertexes[curr.id].p));

		// 检查到所有其他顶点的连线
		for (size_t n = 0; n < m_Vertexes.size(); ++n)
		{
			if (m_Vertexes[n].status == Vertex::CLOSED)
				continue;

			// 如果这是那个神奇的目标顶点，将它移动到当前顶点附近
			CFixedVector2D npos;
			if (n == GOAL_VERTEX_ID)
			{
				// 获取目标形状上离当前顶点最近的点
				npos = request.goal.NearestPointOnGoal(m_Vertexes[curr.id].p);

				// 为防止稍后的整数溢出，我们需要确保所有顶点都
				// '靠近' 源点。目标可能很远（这不是个好主意，但
				// 有时会发生），所以将其限制在当前搜索范围内。
				npos.X = Clamp(npos.X, rangeXMin + EDGE_EXPAND_DELTA, rangeXMax - EDGE_EXPAND_DELTA);
				npos.Y = Clamp(npos.Y, rangeZMin + EDGE_EXPAND_DELTA, rangeZMax - EDGE_EXPAND_DELTA);
			}
			else
				npos = m_Vertexes[n].p;

			// 计算出我们从哪个象限接近新顶点
			u8 quad = 0;
			if (m_Vertexes[curr.id].p.X <= npos.X && m_Vertexes[curr.id].p.Y <= npos.Y) quad |= QUADRANT_BL;
			if (m_Vertexes[curr.id].p.X >= npos.X && m_Vertexes[curr.id].p.Y >= npos.Y) quad |= QUADRANT_TR;
			if (m_Vertexes[curr.id].p.X <= npos.X && m_Vertexes[curr.id].p.Y >= npos.Y) quad |= QUADRANT_TL;
			if (m_Vertexes[curr.id].p.X >= npos.X && m_Vertexes[curr.id].p.Y <= npos.Y) quad |= QUADRANT_BR;

			// 检查新顶点是否在旧顶点的正确象限内
			if (!(m_Vertexes[curr.id].quadOutward & quad) && curr.id != START_VERTEX_ID)
			{
				// 特殊处理：如果可能，总是朝向目标前进，以避免在目标
				// 在另一个单位内部时错过它。
				if (n != GOAL_VERTEX_ID)
					continue;
			}

			// 检查可见性
			bool visible =
				CheckVisibilityLeft(m_Vertexes[curr.id].p, npos, m_EdgesLeft) &&
				CheckVisibilityRight(m_Vertexes[curr.id].p, npos, m_EdgesRight) &&
				CheckVisibilityBottom(m_Vertexes[curr.id].p, npos, m_EdgesBottom) &&
				CheckVisibilityTop(m_Vertexes[curr.id].p, npos, m_EdgesTop) &&
				CheckVisibility(m_Vertexes[curr.id].p, npos, m_EdgesUnaligned);

			// 渲染我们检查的边（用于调试）
			g_VertexPathfinderDebugOverlay.DebugRenderEdges(cmpObstructionManager->GetSimContext(), visible, m_Vertexes[curr.id].p, npos);

			if (visible)
			{
				// g是实际代价：从起点到当前顶点的代价 + 从当前顶点到邻居n的代价
				fixed g = m_Vertexes[curr.id].g + (m_Vertexes[curr.id].p - npos).Length();

				// 如果这是一个新节点，计算其启发式距离
				if (m_Vertexes[n].status == Vertex::UNEXPLORED)
				{
					// 将其添加到开放列表：
					m_Vertexes[n].status = Vertex::OPEN;
					m_Vertexes[n].g = g; // 实际代价
					m_Vertexes[n].h = request.goal.DistanceToPoint(npos).Multiply(heuristicWeight); // 启发代价
					m_Vertexes[n].pred = curr.id; // 设置前驱节点

					if (n == GOAL_VERTEX_ID)
						m_Vertexes[n].p = npos; // 记住新的最佳目标位置

					VertexPriorityQueue::Item t = { (u16)n, g + m_Vertexes[n].h, m_Vertexes[n].h };
					open.push(t);

					// 记住我们目前看到的启发式最佳顶点，以防我们永远无法真正到达目标
					if (m_Vertexes[n].h < hBest)
					{
						idBest = (u16)n;
						hBest = m_Vertexes[n].h;
					}
				}
				else // 否则它必定在开放列表中 (OPEN)
				{
					// 如果我们已经见过这个节点，并且新路径到此节点的代价不更优，则停止
					if (g >= m_Vertexes[n].g)
						continue;

					// 否则，我们有了一条更好的路径，所以用新的代价/父节点替换旧的
					fixed gprev = m_Vertexes[n].g;
					m_Vertexes[n].g = g;
					m_Vertexes[n].pred = curr.id;

					if (n == GOAL_VERTEX_ID)
						m_Vertexes[n].p = npos; // 记住新的最佳目标位置

					// 在优先队列中提升该节点的位置
					open.promote((u16)n, gprev + m_Vertexes[n].h, g + m_Vertexes[n].h, m_Vertexes[n].h);
				}
			}
		}
	}

	// （反向）重构路径
	WaypointPath path;
	for (u16 id = idBest; id != START_VERTEX_ID; id = m_Vertexes[id].pred)
		path.m_Waypoints.emplace_back(Waypoint{ m_Vertexes[id].p.X, m_Vertexes[id].p.Y });

	// 清理本次寻路使用的临时数据
	m_Edges.clear();
	m_EdgeSquares.clear();
	m_Vertexes.clear();

	m_EdgesUnaligned.clear();
	m_EdgesLeft.clear();
	m_EdgesRight.clear();
	m_EdgesBottom.clear();
	m_EdgesTop.clear();

	return path;
}

void VertexPathfinderDebugOverlay::DebugRenderGoal(const CSimContext& simContext, const PathGoal& goal)
{
	// 如果未启用调试覆盖层，则直接返回
	if (!m_DebugOverlay)
		return;

	// 加锁以保证线程安全
	std::lock_guard<std::mutex> lock(g_DebugMutex);

	// 清空旧的调试线段
	g_VertexPathfinderDebugOverlay.m_DebugOverlayShortPathLines.clear();

	// 渲染目标形状
	m_DebugOverlayShortPathLines.push_back(SOverlayLine());
	m_DebugOverlayShortPathLines.back().m_Color = CColor(1, 0, 0, 1); // 红色
	switch (goal.type)
	{
	case PathGoal::POINT:
	{
		SimRender::ConstructCircleOnGround(simContext, goal.x.ToFloat(), goal.z.ToFloat(), 0.2f, m_DebugOverlayShortPathLines.back(), true);
		break;
	}
	case PathGoal::CIRCLE:
	case PathGoal::INVERTED_CIRCLE:
	{
		SimRender::ConstructCircleOnGround(simContext, goal.x.ToFloat(), goal.z.ToFloat(), goal.hw.ToFloat(), m_DebugOverlayShortPathLines.back(), true);
		break;
	}
	case PathGoal::SQUARE:
	case PathGoal::INVERTED_SQUARE:
	{
		float a = atan2f(goal.v.X.ToFloat(), goal.v.Y.ToFloat());
		SimRender::ConstructSquareOnGround(simContext, goal.x.ToFloat(), goal.z.ToFloat(), goal.hw.ToFloat() * 2, goal.hh.ToFloat() * 2, a, m_DebugOverlayShortPathLines.back(), true);
		break;
	}
	}
}

void VertexPathfinderDebugOverlay::DebugRenderGraph(const CSimContext& simContext, const std::vector<Vertex>& vertexes, const std::vector<Edge>& edges, const std::vector<Square>& edgeSquares)
{
	if (!m_DebugOverlay)
		return;

	std::lock_guard<std::mutex> lock(g_DebugMutex);

#define PUSH_POINT(p) STMT(xz.push_back(p.X.ToFloat()); xz.push_back(p.Y.ToFloat()))
	// 将顶点渲染成小小的吃豆人形状，以指示象限方向
	for (size_t i = 0; i < vertexes.size(); ++i)
	{
		m_DebugOverlayShortPathLines.emplace_back();
		m_DebugOverlayShortPathLines.back().m_Color = CColor(1, 1, 0, 1); // 黄色

		float x = vertexes[i].p.X.ToFloat();
		float z = vertexes[i].p.Y.ToFloat();

		float a0 = 0, a1 = 0;
		// 根据象限（如果有）获取圆弧的起始/结束角度
		if(vertexes[i].quadInward == QUADRANT_BL) { a0 = -0.25f; a1 = 0.50f; }
		else if (vertexes[i].quadInward == QUADRANT_TR) { a0 = 0.25f; a1 = 1.00f; }
		else if (vertexes[i].quadInward == QUADRANT_TL) { a0 = -0.50f; a1 = 0.25f; }
		else if (vertexes[i].quadInward == QUADRANT_BR) { a0 = 0.00f; a1 = 0.75f; }

	// 如果没有特定象限，画一个完整的圆
	if (a0 == a1)
		SimRender::ConstructCircleOnGround(simContext, x, z, 0.5f,
			m_DebugOverlayShortPathLines.back(), true);
	else
		// 否则画一个带缺口的圆（吃豆人）
		SimRender::ConstructClosedArcOnGround(simContext, x, z, 0.5f,
			a0 * ((float)M_PI * 2.0f), a1 * ((float)M_PI * 2.0f),
			m_DebugOverlayShortPathLines.back(), true);
	}

	// 渲染边
	for (size_t i = 0; i < edges.size(); ++i)
	{
		m_DebugOverlayShortPathLines.emplace_back();
		m_DebugOverlayShortPathLines.back().m_Color = CColor(0, 1, 1, 1); // 青色
		std::vector<float> xz;
		PUSH_POINT(edges[i].p0);
		PUSH_POINT(edges[i].p1);

		// 添加一个箭头以指示边的方向
		CFixedVector2D d = edges[i].p1 - edges[i].p0;
		d.Normalize(fixed::FromInt(1) / 8);
		CFixedVector2D p2 = edges[i].p1 - d * 2;
		CFixedVector2D p3 = p2 + d.Perpendicular();
		CFixedVector2D p4 = p2 - d.Perpendicular();
		PUSH_POINT(p3);
		PUSH_POINT(p4);
		PUSH_POINT(edges[i].p1);

		SimRender::ConstructLineOnGround(simContext, xz, m_DebugOverlayShortPathLines.back(), true);
	}
#undef PUSH_POINT

	// 渲染轴对齐的方形
	for (size_t i = 0; i < edgeSquares.size(); ++i)
	{
		m_DebugOverlayShortPathLines.push_back(SOverlayLine());
		m_DebugOverlayShortPathLines.back().m_Color = CColor(0, 1, 1, 1); // 青色
		std::vector<float> xz;
		Square s = edgeSquares[i];
		xz.push_back(s.p0.X.ToFloat());
		xz.push_back(s.p0.Y.ToFloat());
		xz.push_back(s.p0.X.ToFloat());
		xz.push_back(s.p1.Y.ToFloat());
		xz.push_back(s.p1.X.ToFloat());
		xz.push_back(s.p1.Y.ToFloat());
		xz.push_back(s.p1.X.ToFloat());
		xz.push_back(s.p0.Y.ToFloat());
		xz.push_back(s.p0.X.ToFloat());
		xz.push_back(s.p0.Y.ToFloat());
		SimRender::ConstructLineOnGround(simContext, xz, m_DebugOverlayShortPathLines.back(), true);
	}
}

void VertexPathfinderDebugOverlay::DebugRenderEdges(const CSimContext&, bool UNUSED(visible),
	CFixedVector2D UNUSED(curr), CFixedVector2D UNUSED(npos))
{
	if (!m_DebugOverlay)
		return;

	std::lock_guard<std::mutex> lock(g_DebugMutex);

	// 默认禁用此调试绘图，因为它会产生大量视觉噪音。
	/*
	m_DebugOverlayShortPathLines.push_back(SOverlayLine());
	// 可见为绿色，不可见为红色
	m_DebugOverlayShortPathLines.back().m_Color = visible ? CColor(0, 1, 0, 0.5) : CColor(1, 0, 0, 0.5);
	m_DebugOverlayShortPathLines.push_back(SOverlayLine());
	m_DebugOverlayShortPathLines.back().m_Color = visible ? CColor(0, 1, 0, 0.5) : CColor(1, 0, 0, 0.5);
	std::vector<float> xz;
	xz.push_back(curr.X.ToFloat());
	xz.push_back(curr.Y.ToFloat());
	xz.push_back(npos.X.ToFloat());
	xz.push_back(npos.Y.ToFloat());
	SimRender::ConstructLineOnGround(simContext, xz, m_DebugOverlayShortPathLines.back(), false);
	SimRender::ConstructLineOnGround(simContext, xz, m_DebugOverlayShortPathLines.back(), false);
	*/
}

void VertexPathfinderDebugOverlay::RenderSubmit(SceneCollector& collector)
{
	if (!m_DebugOverlay)
		return;

	// 加锁并交换缓冲区，将调试线段提交给渲染器
	std::lock_guard<std::mutex> lock(g_DebugMutex);
	m_DebugOverlayShortPathLines.swap(m_DebugOverlayShortPathLinesSubmitted);
	for (size_t i = 0; i < m_DebugOverlayShortPathLinesSubmitted.size(); ++i)
		collector.Submit(&m_DebugOverlayShortPathLinesSubmitted[i]);
}
