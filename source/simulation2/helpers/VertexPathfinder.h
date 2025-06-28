/* Copyright (C) 2021 Wildfire Games.
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

#ifndef INCLUDED_VERTEXPATHFINDER
#define INCLUDED_VERTEXPATHFINDER

#include "graphics/Overlay.h"
#include "simulation2/helpers/Pathfinding.h"
#include "simulation2/system/CmpPtr.h"

#include <atomic>
#include <vector>

 // A vertex around the corners of an obstruction
 // (paths will be sequences of these vertexes)
 // // 一个围绕障碍物拐角的顶点
 // // (路径将是这些顶点的序列)
struct Vertex
{
	enum
	{
		UNEXPLORED, // 未探索
		OPEN,       // 在开放列表中
		CLOSED,     // 在关闭列表中
	};

	CFixedVector2D p;    // 顶点的二维坐标
	fixed g, h;          // A*算法的G值(实际成本)和H值(启发式成本)
	u16 pred;            // 前驱顶点的索引
	u8 status;           // 状态 (UNEXPLORED, OPEN, CLOSED)
	u8 quadInward : 4;   // the quadrant which is inside the shape (or NONE)
	// // 朝向形状内部的象限 (或无)
	u8 quadOutward : 4;  // the quadrants of the next point on the path which this vertex must be in, given 'pred'
	// // 给定父节点'pred'时，路径上下一顶点必须位于的象限
};

// Obstruction edges (paths will not cross any of these).
// Defines the two points of the edge.
// // 障碍物边缘 (路径绝不能穿过这些边缘)。
// // 定义了边缘的两个端点。
struct Edge
{
	CFixedVector2D p0, p1;
};

// Axis-aligned obstruction squares (paths will not cross any of these).
// Defines the opposing corners of an axis-aligned square
// (from which four individual edges can be trivially computed), requiring p0 <= p1
// // 轴对齐的障碍物正方形 (路径绝不能穿过这些正方形)。
// // 定义了一个轴对齐正方形的对角顶点
// // (由此可以轻易计算出四条独立的边), 要求 p0 <= p1。
struct Square
{
	CFixedVector2D p0, p1;
};

// Axis-aligned obstruction edges.
// p0 defines one end; c1 is either the X or Y coordinate of the other end,
// depending on the context in which this is used.
// // 轴对齐的障碍物边缘。
// // p0 定义了一个端点；c1 是另一个端点的X或Y坐标,
// // 具体是哪个坐标取决于其使用的上下文。
struct EdgeAA
{
	CFixedVector2D p0;
	fixed c1;
};

class ICmpObstructionManager;
class CSimContext;
class SceneCollector;

/**
 * @brief 顶点寻路器类，负责在复杂的障碍物环境中进行短距离、高精度的寻路。
 */
class VertexPathfinder
{
public:
	VertexPathfinder(const u16& gridSize, Grid<NavcellData>* const& terrainOnlyGrid) : m_GridSize(gridSize), m_TerrainOnlyGrid(terrainOnlyGrid) {};
	VertexPathfinder(const VertexPathfinder&) = delete; // 禁止拷贝构造
	VertexPathfinder(VertexPathfinder&& o) : m_GridSize(o.m_GridSize), m_TerrainOnlyGrid(o.m_TerrainOnlyGrid) {} // 允许移动构造

	/**
	 * Compute a precise path from the given point to the goal, and return the set of waypoints.
	 * The path is based on the full set of obstructions that pass the filter, such that
	 * a unit of clearance 'clearance' will be able to follow the path with no collisions.
	 * The path is restricted to a box of radius 'range' from the starting point.
	 * Defined in CCmpPathfinder_Vertex.cpp
	 * // 计算从给定点到目标的精确路径，并返回一组路径点。
	 * // 该路径基于通过筛选器的完整障碍物集合，
	 * // 使得一个拥有'clearance'间隙的单位能够无碰撞地沿着该路径移动。
	 * // 路径被限制在以起点为中心、半径为'range'的矩形区域内。
	 * // 在 CCmpPathfinder_Vertex.cpp 中定义。
	 */
	WaypointPath ComputeShortPath(const ShortPathRequest& request, CmpPtr<ICmpObstructionManager> cmpObstructionManager) const;

private:

	// References to the Pathfinder for convenience.
	// // 为方便起见，引用探路器的相关数据。
	const u16& m_GridSize; // 引用网格大小
	Grid<NavcellData>* const& m_TerrainOnlyGrid; // 引用只包含地形信息的通行性网格

	// These vectors are expensive to recreate on every call, so we cache them here.
	// They are made mutable to allow using them in the otherwise const ComputeShortPath.
	// // 这些向量在每次调用时重新创建都非常耗时，所以我们在这里缓存它们。
	// // 它们被声明为 mutable，以便在原本为 const 的 ComputeShortPath 方法中使用。
	mutable std::vector<Edge> m_EdgesUnaligned; // 缓存非轴对齐的边
	mutable std::vector<EdgeAA> m_EdgesLeft;   // 缓存左边缘 (垂直)
	mutable std::vector<EdgeAA> m_EdgesRight;  // 缓存右边缘 (垂直)
	mutable std::vector<EdgeAA> m_EdgesBottom; // 缓存底边缘 (水平)
	mutable std::vector<EdgeAA> m_EdgesTop;    // 缓存顶边缘 (水平)

	// List of obstruction vertexes (plus start/end points); we'll try to find paths through
	// the graph defined by these vertexes.
	// // 障碍物顶点列表 (外加起点/终点)；我们将尝试寻找穿过
	// // 由这些顶点定义的图的路径。
	mutable std::vector<Vertex> m_Vertexes;

	// List of collision edges - paths must never cross these.
	// (Edges are one-sided so intersections are fine in one direction, but not the other direction.)
	// // 碰撞边缘列表 - 路径绝不能穿过它们。
	// // (边缘是单向的，因此在一个方向上相交是允许的，但在另一个方向上则不行。)
	mutable std::vector<Edge> m_Edges;
	// Axis-aligned squares; equivalent to 4 edges.
	// // 轴对齐的正方形；等同于4条边。
	mutable std::vector<Square> m_EdgeSquares;
};

/**
 * There are several vertex pathfinders running asynchronously, so their debug output
 * might conflict. To remain thread-safe, this single class will handle the debug data.
 * NB: though threadsafe, the way things are setup means you can have a few
 * more graphs and edges than you'd expect showing up in the rendered graph.
 * // 有多个顶点寻路器在异步运行，因此它们的调试输出可能会发生冲突。
 * // 为保持线程安全，由这一个单独的类来处理调试数据。
 * // 注意：尽管是线程安全的，但当前的设置方式可能导致在渲染出的图中，
 * // 显示的图和边比预期的要多一些。
 */
class VertexPathfinderDebugOverlay
{
	friend class VertexPathfinder; // 允许 VertexPathfinder 访问其成员
public:
	// 设置是否启用调试覆盖层
	void SetDebugOverlay(bool enabled) { m_DebugOverlay = enabled; }
	// 提交调试信息以供渲染
	void RenderSubmit(SceneCollector& collector);
protected:

	// 调试渲染目标点
	void DebugRenderGoal(const CSimContext& simContext, const PathGoal& goal);
	// 调试渲染寻路图 (顶点和边)
	void DebugRenderGraph(const CSimContext& simContext, const std::vector<Vertex>& vertexes, const std::vector<Edge>& edges, const std::vector<Square>& edgeSquares);
	// 调试渲染可见性检查的边
	void DebugRenderEdges(const CSimContext& simContext, bool visible, CFixedVector2D curr, CFixedVector2D npos);

	// 原子布尔值，用于线程安全地控制调试覆盖层的开关
	std::atomic<bool> m_DebugOverlay = false;
	// The data is double buffered: the first is the 'work-in-progress' state, the second the last RenderSubmit state.
	// // 数据采用双缓冲：第一个是“处理中”状态，第二个是上一次提交渲染的状态。
	std::vector<SOverlayLine> m_DebugOverlayShortPathLines; // 当前正在生成的调试线段
	std::vector<SOverlayLine> m_DebugOverlayShortPathLinesSubmitted; // 已提交用于渲染的调试线段
};

// 全局的顶点寻路器调试覆盖层实例
extern VertexPathfinderDebugOverlay g_VertexPathfinderDebugOverlay;

#endif // INCLUDED_VERTEXPATHFINDER
