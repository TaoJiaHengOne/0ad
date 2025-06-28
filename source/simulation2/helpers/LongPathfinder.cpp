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

#include "precompiled.h"

#include "LongPathfinder.h"

#include "lib/bits.h"
#include "ps/Profile.h"

#include "Geometry.h"
#include "HierarchicalPathfinder.h"

#include <mutex>

namespace
{
	// 调试输出所使用的全局互斥锁，防止多线程同时写入造成混乱。
	static std::mutex g_DebugMutex;
}

/**
 * Jump point cache.
 * // 跳点缓存。
 *
 * The JPS algorithm wants to efficiently either find the first jump point
 * in some direction from some cell (not counting the cell itself),
 * if it is reachable without crossing any impassable cells;
 * or know that there is no such reachable jump point.
 * The jump point is always on a passable cell.
 * We cache that data to allow fast lookups, which helps performance
 * significantly (especially on sparse maps).
 * Recalculation might be expensive but the underlying passability data
 * changes relatively rarely.
 * // JPS（跳点搜索）算法的核心需求是：从某个单元格（不含其自身）出发，沿着某个方向，
 * // 高效地找到第一个可到达的跳点（期间不能穿过任何不可通行的单元格），
 * // 或者确认该方向上不存在这样的跳点。
 * // 跳点本身永远位于一个可通行的单元格上。
 * // 我们将这些数据缓存起来以实现快速查找，这能极大地提升性能（尤其是在障碍物稀疏的地图上）。
 * // 重新计算缓存可能会很耗时，但底层的通行性数据变化相对较少。
 *
 * To allow the algorithm to detect goal cells, we want to treat them as
 * jump points too. (That means the algorithm will push those cells onto
 * its open queue, and will eventually pop a goal cell and realise it's done.)
 * (Goals might be circles/squares/etc, not just a single cell.)
 * But the goal generally changes for every path request, so we can't cache
 * it like the normal jump points.
 * // 为了让算法能够检测到目标单元格，我们希望也将它们视为跳点。
 * // (这意味着算法会将这些目标单元格推入其开放列表，并最终在弹出目标单元格时意识到寻路已完成。)
 * // (目标点可能是圆形、方形等区域，而不仅仅是单个单元格。)
 * // 但是，目标点对于每次寻路请求都可能不同，所以我们不能像缓存普通跳点那样缓存它。
 *
 * Instead, if there's no jump point from some cell then we'll cache the
 * first impassable cell as an 'obstruction jump point'
 * (with a flag to distinguish from a real jump point), and then the caller
 * can test whether the goal includes a cell that's closer than the first
 * (obstruction or real) jump point,
 * and treat the goal cell as a jump point in that case.
 * // 作为替代方案，如果某个单元格出发没有跳点，我们会将遇到的第一个不可通行单元格缓存为“障碍物跳点”
 * // （并用一个标志位来与真正的跳点区分）。
 * // 这样，调用者就可以测试目标点是否比遇到的第一个跳点（无论是障碍物跳点还是真跳点）更近，
 * // 如果是，就可以将那个目标单元格当作一个临时的跳点来处理。
 *
 * We only ever need to find the jump point relative to a passable cell;
 * the cache is allowed to return bogus values for impassable cells.
 * // 我们只需要查找从可通行单元格出发的跳点；
 * // 因此，对于不可通行的单元格，缓存可以返回无效值。
 */
class JumpPointCache
{
	/**
	 * Simple space-inefficient row storage.
	 * // 一种简单的、空间效率较低的行存储方式。
	 */
	struct RowRaw
	{
		// 存储一行的数据
		std::vector<u16> data;

		// 获取该行数据所占用的内存大小
		size_t GetMemoryUsage() const
		{
			return data.capacity() * sizeof(u16);
		}

		// 构造函数，初始化指定长度的行
		RowRaw(int length)
		{
			data.resize(length);
		}

		/**
		 * Set cells x0 <= x < x1 to have jump point x1.
		 * // 将范围从 x0 (含)到 x1 (不含)的所有单元格的跳点设置为 x1。
		 */
		void SetRange(int x0, int x1, bool obstruction)
		{
			ENSURE(0 <= x0 && x0 <= x1 && x1 < (int)data.size());
			for (int x = x0; x < x1; ++x)
				// 将 x1 左移一位作为跳点坐标，最低位用作标志位（1表示障碍物，0表示真跳点）。
				data[x] = (x1 << 1) | (obstruction ? 1 : 0);
		}

		/**
		 * Returns the coordinate of the next jump point xp (where x < xp),
		 * and whether it's an obstruction point or jump point.
		 * // 返回下一个跳点的坐标 xp (其中 x < xp)，
		 * // 并通过引用返回它是一个障碍物点还是一个真跳点。
		 */
		void Get(int x, int& xp, bool& obstruction) const
		{
			ENSURE(0 <= x && x < (int)data.size());
			// 右移一位获取坐标
			xp = data[x] >> 1;
			// 用最低位判断是否为障碍物
			obstruction = data[x] & 1;
		}

		// 完成该行的处理（在此实现中无操作）
		void Finish() { }
	};

	// 一种使用二叉树优化的行存储结构（当前代码中未使用，但保留了实现）
	struct RowTree
	{
		/**
		 * Represents an interval [u15 x0, u16 x1)
		 * with a boolean obstruction flag,
		 * packed into a single u32.
		 * // 代表一个区间 [u15 x0, u16 x1)，并附带一个布尔类型的障碍物标志，
		 * // 所有这些信息被打包到一个 u32 整数中。
		 */
		struct Interval
		{
			// 默认构造
			Interval() : data(0) { }

			// 构造函数，将区间和标志位打包
			Interval(int x0, int x1, bool obstruction)
			{
				ENSURE(0 <= x0 && x0 < 0x8000);
				ENSURE(0 <= x1 && x1 < 0x10000);
				data = ((u32)x0 << 17) | (u32)(obstruction ? 0x10000 : 0) | (u32)x1;
			}

			// 解包获取起始坐标 x0
			int x0() { return data >> 17; }
			// 解包获取结束坐标 x1
			int x1() { return data & 0xFFFF; }
			// 解包获取障碍物标志
			bool obstruction() { return (data & 0x10000) != 0; }

			u32 data;
		};

		// 存储区间数据的向量
		std::vector<Interval> data;

		// 获取内存使用量
		size_t GetMemoryUsage() const
		{
			return data.capacity() * sizeof(Interval);
		}

		// 构造函数
		RowTree(int UNUSED(length))
		{
		}

		// 设置一个区间范围
		void SetRange(int x0, int x1, bool obstruction)
		{
			ENSURE(0 <= x0 && x0 <= x1);
			data.emplace_back(x0, x1, obstruction);
		}

		/**
		 * Recursive helper function for Finish().
		 * Given two ranges [x0, pivot) and [pivot, x1) in the sorted array 'data',
		 * the pivot element is added onto the binary tree (stored flattened in an
		 * array), and then each range is split into two sub-ranges with a pivot in
		 * the middle (to ensure the tree remains balanced) and ConstructTree recurses.
		 * // Finish() 的递归辅助函数。
		 * // 给定已排序数组 'data' 中的两个范围 [x0, pivot) 和 [pivot, x1)，
		 * // 首先将枢轴元素添加到二叉树中（该树以扁平化数组形式存储），
		 * // 然后将每个范围再分裂成两个以中间点为枢轴的子范围（以确保树的平衡），并递归调用 ConstructTree。
		 */
		void ConstructTree(std::vector<Interval>& tree, size_t x0, size_t pivot, size_t x1, size_t idx_tree)
		{
			ENSURE(x0 < data.size());
			ENSURE(x1 <= data.size());
			ENSURE(x0 <= pivot);
			ENSURE(pivot < x1);
			ENSURE(idx_tree < tree.size());

			tree[idx_tree] = data[pivot];

			if (x0 < pivot)
				ConstructTree(tree, x0, (x0 + pivot) / 2, pivot, (idx_tree << 1) + 1);
			if (pivot + 1 < x1)
				ConstructTree(tree, pivot + 1, (pivot + x1) / 2, x1, (idx_tree << 1) + 2);
		}

		// 完成该行的处理
		void Finish()
		{
			// Convert the sorted interval list into a balanced binary tree
			// 将已排序的区间列表转换为一棵平衡二叉树
			std::vector<Interval> tree;

			if (!data.empty())
			{
				size_t depth = ceil_log2(data.size() + 1);
				tree.resize((1 << depth) - 1);
				ConstructTree(tree, 0, data.size() / 2, data.size(), 0);
			}

			data.swap(tree);
		}

		// 获取指定点的跳点信息
		void Get(int x, int& xp, bool& obstruction) const
		{
			// Search the binary tree for an interval which contains x
			// 在二叉树中搜索包含 x 的区间
			int i = 0;
			while (true)
			{
				ENSURE(i < (int)data.size());
				Interval interval = data[i];
				if (x < interval.x0())
					i = (i << 1) + 1; // 搜索左子树
				else if (x >= interval.x1())
					i = (i << 1) + 2; // 搜索右子树
				else
				{
					// 找到包含 x 的区间
					ENSURE(interval.x0() <= x && x < interval.x1());
					xp = interval.x1();
					obstruction = interval.obstruction();
					return;
				}
			}
		}
	};

	// Pick one of the row implementations
	// // 在此选择一种行的实现方式
	typedef RowRaw Row;

public:
	int m_Width; // 网格宽度
	int m_Height; // 网格高度
	// 分别存储向右、向左、向上、向下四个方向的跳点缓存
	std::vector<Row> m_JumpPointsRight;
	std::vector<Row> m_JumpPointsLeft;
	std::vector<Row> m_JumpPointsUp;
	std::vector<Row> m_JumpPointsDown;

	/**
	 * Compute the cached obstruction/jump points for each cell,
	 * in a single direction. By default the code assumes the rightwards
	 * (+i) direction; set 'transpose' to switch to upwards (+j),
	 * and/or set 'mirror' to reverse the direction.
	 * // 为每个单元格计算并缓存其在单一方向上的障碍物/跳点。
	 * // 默认情况下，代码假设是向右（+i）的方向；
	 * // 设置 'transpose'（转置）可以切换到向上（+j）方向，
	 * // 设置 'mirror'（镜像）可以反转方向。
	 */
	void ComputeRows(std::vector<Row>& rows,
		const Grid<NavcellData>& terrain, pass_class_t passClass,
		bool transpose, bool mirror)
	{
		int w = terrain.m_W;
		int h = terrain.m_H;

		// 如果是转置模式（计算垂直方向），则交换宽高
		if (transpose)
			std::swap(w, h);

		// Check the terrain passability, adjusted for transpose/mirror
		// // 检查地形通行性，根据 transpose/mirror 参数进行调整
		// 宏定义一个函数，用于根据转置和镜像状态获取正确的地块通行性
#define TERRAIN_IS_PASSABLE(i, j) \
	IS_PASSABLE( \
		mirror \
		? (transpose ? terrain.get((j), w-1-(i)) : terrain.get(w-1-(i), (j))) \
		: (transpose ? terrain.get((j), (i)) : terrain.get((i), (j))) \
	, passClass)

		// 为所有行预留空间并构造
		rows.reserve(h);
		for (int j = 0; j < h; ++j)
			rows.emplace_back(w);

		// 遍历每一行（或列），但不包括地图边缘（因为边缘默认为不可通行）
		for (int j = 1; j < h - 1; ++j)
		{
			// Find the first passable cell.
			// Then, find the next jump/obstruction point after that cell,
			// and store that point for the passable range up to that cell,
			// then repeat.
			// // 找到第一个可通行的单元格。
			// // 然后，找到该单元格之后的下一个跳点/障碍物点，
			// // 并将该点的信息存储给从起始到该点之间的所有可通行单元格，
			// // 然后重复此过程。

			int i = 0;
			while (i < w)
			{
				// Restart the 'while' loop until we reach a passable cell
				// // 循环直到我们找到一个可通行的单元格
				if (!TERRAIN_IS_PASSABLE(i, j))
				{
					++i;
					continue;
				}

				// i is now a passable cell; find the next jump/obstruction point.
				// (We assume the map is surrounded by impassable cells, so we don't
				// need to explicitly check for world bounds here.)
				// // 此刻 i 是一个可通行的单元格；现在开始寻找下一个跳点或障碍物点。
				// // (我们假设地图被不可通行的单元格包围，所以在这里不需要显式地检查世界边界。)

				int i0 = i; // 记录当前连续可通行区段的起始点
				while (true)
				{
					++i;

					// Check if we hit an obstructed tile
					// // 检查是否遇到了障碍物
					if (!TERRAIN_IS_PASSABLE(i, j))
					{
						// 遇到障碍物，将 [i0, i) 区间的跳点设为 i (作为障碍物点)
						rows[j].SetRange(i0, i, true);
						break;
					}

					// Check if we reached a jump point
					// // 检查是否到达了一个跳点 (存在“强迫邻居”)
					// JPS核心规则：如果一个单元格的邻居（如左上）不可通行，但这个邻居的邻居（如正上）却可通行，
					// 那么当前单元格就是一个跳点，因为从父节点到这里可能存在更优的对角线路径。
					if ((!TERRAIN_IS_PASSABLE(i - 1, j - 1) && TERRAIN_IS_PASSABLE(i, j - 1)) ||
						(!TERRAIN_IS_PASSABLE(i - 1, j + 1) && TERRAIN_IS_PASSABLE(i, j + 1)))
					{
						// 找到跳点，将 [i0, i) 区间的跳点设为 i (作为真跳点)
						rows[j].SetRange(i0, i, false);
						break;
					}
				}
			}

			// 完成该行的处理（例如构建RowTree）
			rows[j].Finish();
		}
		// 取消宏定义，避免污染
#undef TERRAIN_IS_PASSABLE
	}

	// 重置/重新计算整个缓存
	void reset(const Grid<NavcellData>* terrain, pass_class_t passClass)
	{
		PROFILE2("JumpPointCache reset");
		TIMER(L"JumpPointCache reset");

		m_Width = terrain->m_W;
		m_Height = terrain->m_H;

		// 分别为四个方向计算跳点缓存
		ComputeRows(m_JumpPointsRight, *terrain, passClass, false, false); // 右
		ComputeRows(m_JumpPointsLeft, *terrain, passClass, false, true);  // 左 (镜像)
		ComputeRows(m_JumpPointsUp, *terrain, passClass, true, false);   // 上 (转置)
		ComputeRows(m_JumpPointsDown, *terrain, passClass, true, true);    // 下 (转置+镜像)
	}

	// 获取缓存所占用的总内存大小
	size_t GetMemoryUsage() const
	{
		size_t bytes = 0;
		for (int i = 0; i < m_Width; ++i)
		{
			bytes += m_JumpPointsUp[i].GetMemoryUsage();
			bytes += m_JumpPointsDown[i].GetMemoryUsage();
		}
		for (int j = 0; j < m_Height; ++j)
		{
			bytes += m_JumpPointsRight[j].GetMemoryUsage();
			bytes += m_JumpPointsLeft[j].GetMemoryUsage();
		}
		return bytes;
	}

	/**
	 * Returns the next jump point (or goal point) to explore,
	 * at (ip, j) where i < ip.
	 * Returns i if there is no such point.
	 * // 返回下一个要探索的跳点（或目标点），其坐标为 (ip, j)，其中 i < ip。
	 * // 如果不存在这样的点，则返回 i。
	 */
	int GetJumpPointRight(int i, int j, const PathGoal& goal) const
	{
		int ip;
		bool obstruction;
		// 从缓存中获取预计算的跳点
		m_JumpPointsRight[j].Get(i, ip, obstruction);
		// Adjust ip to be a goal cell, if there is one closer than the jump point;
		// and then return the new ip if there is a goal,
		// or the old ip if there is a (non-obstruction) jump point
		// // 如果有目标点比缓存的跳点更近，则将 ip 调整为目标单元格的坐标；
		// // 然后，如果存在更近的目标点，则返回新的 ip，
		// // 或者，如果缓存的是一个真正的（非障碍物）跳点，则返回旧的 ip。
		if (goal.NavcellRectContainsGoal(i + 1, j, ip - 1, j, &ip, NULL) || !obstruction)
			return ip;
		return i; // 否则，表示前方是障碍物且没有更近的目标，返回原位
	}

	// 获取向左的跳点，逻辑与向右类似，但需要处理镜像坐标转换
	int GetJumpPointLeft(int i, int j, const PathGoal& goal) const
	{
		int mip; // mirrored value, because m_JumpPointsLeft is generated from a mirrored map
		// // 镜像坐标值，因为 m_JumpPointsLeft 是从镜像地图生成的
		bool obstruction;
		m_JumpPointsLeft[j].Get(m_Width - 1 - i, mip, obstruction);
		int ip = m_Width - 1 - mip; // 将镜像坐标转换回来
		if (goal.NavcellRectContainsGoal(i - 1, j, ip + 1, j, &ip, NULL) || !obstruction)
			return ip;
		return i;
	}

	// 获取向上的跳点，逻辑与向右类似，但使用了转置的缓存
	int GetJumpPointUp(int i, int j, const PathGoal& goal) const
	{
		int jp;
		bool obstruction;
		m_JumpPointsUp[i].Get(j, jp, obstruction);
		if (goal.NavcellRectContainsGoal(i, j + 1, i, jp - 1, NULL, &jp) || !obstruction)
			return jp;
		return j;
	}

	// 获取向下的跳点，逻辑与向右类似，但使用了转置+镜像的缓存
	int GetJumpPointDown(int i, int j, const PathGoal& goal) const
	{
		int mjp; // mirrored value // 镜像坐标值
		bool obstruction;
		m_JumpPointsDown[i].Get(m_Height - 1 - j, mjp, obstruction);
		int jp = m_Height - 1 - mjp; // 坐标转换
		if (goal.NavcellRectContainsGoal(i, j - 1, i, jp + 1, NULL, &jp) || !obstruction)
			return jp;
		return j;
	}
};

//////////////////////////////////////////////////////////

// 长距离寻路器的构造函数
LongPathfinder::LongPathfinder() :
	m_UseJPSCache(false), // 默认不使用JPS缓存
	m_Grid(NULL), m_GridSize(0)
{
}

// 定义一个宏，方便地检查指定坐标(i, j)的单元格对于当前寻路状态是否可通行
#define PASSABLE(i, j) IS_PASSABLE(state.terrain->get(i, j), state.passClass)

// Calculate heuristic cost from tile i,j to goal
// (This ought to be an underestimate for correctness)
// // 计算从单元格 (i,j) 到目标的启发式成本（估价函数 H 值）。
// // (为了保证A*算法的正确性，这个值必须是实际成本的低估值。)
// 使用曼哈顿距离和对角线距离结合的八叉距离（Octile distance）
PathCost LongPathfinder::CalculateHeuristic(int i, int j, int iGoal, int jGoal) const
{
	int di = abs(i - iGoal);
	int dj = abs(j - jGoal);
	int diag = std::min(di, dj);
	// 返回 (直线步数, 对角线步数)
	return PathCost(di - diag + dj - diag, diag);
}

// Do the A* processing for a neighbour tile i,j.
// // 对邻居单元格 (i,j) 进行 A* 算法的处理。
void LongPathfinder::ProcessNeighbour(int pi, int pj, int i, int j, PathCost pg, PathfinderState& state) const
{
	// Reject impassable tiles
	// // 拒绝不可通行的单元格
	if (!PASSABLE(i, j))
		return;

	// 获取邻居单元格的寻路数据
	PathfindTile& n = state.tiles->get(i, j);

	// 如果邻居已在关闭列表中，则忽略
	if (n.IsClosed())
		return;

	// 计算从父节点 (pi, pj) 到当前邻居 (i, j) 的实际成本 (Δg)
	PathCost dg;
	if (pi == i) // 垂直移动
		dg = PathCost::horizvert(abs(pj - j));
	else if (pj == j) // 水平移动
		dg = PathCost::horizvert(abs(pi - i));
	else // 对角线移动
	{
		ASSERT(abs((int)pi - (int)i) == abs((int)pj - (int)j)); // must be 45 degrees // 必须是45度
		dg = PathCost::diag(abs((int)pi - (int)i));
	}

	// 计算到达该邻居的总成本 G = 到达父节点的成本 + Δg
	PathCost g = pg + dg;

	// 计算该邻居到目标的启发式成本 H
	PathCost h = CalculateHeuristic(i, j, state.iGoal, state.jGoal);

	// If this is a new tile, compute the heuristic distance
	// // 如果这是一个从未访问过的新单元格
	if (n.IsUnexplored())
	{
		// Remember the best tile we've seen so far, in case we never actually reach the target
		// // 记录下我们见过的最好的单元格（H值最小），以防万一我们永远到不了目标点
		if (h < state.hBest)
		{
			state.hBest = h;
			state.iBest = i;
			state.jBest = j;
		}
	}
	else
	{
		// If we've already seen this tile, and the new path to this tile does not have a
		// better cost, then stop now
		// // 如果我们已经访问过这个单元格，并且新路径的成本并不更优，则停止处理
		if (g >= n.GetCost())
			return;

		// Otherwise, we have a better path.
		// // 否则，我们找到了一条更优的路径。

		// If we've already added this tile to the open list:
		// // 如果这个单元格已在开放列表中：
		if (n.IsOpen())
		{
			// This is a better path, so replace the old one with the new cost/parent
			// // 这是一条更优路径，所以用新的成本和父节点替换旧的
			PathCost gprev = n.GetCost();
			n.SetCost(g);
			n.SetPred(pi, pj, i, j);
			// 更新它在优先队列中的位置
			state.open.promote(TileID(i, j), gprev + h, g + h, h);
			return;
		}
	}

	// Add it to the open list:
	// // 将其添加到开放列表：
	n.SetStatusOpen();
	n.SetCost(g);
	n.SetPred(pi, pj, i, j);
	PriorityQueue::Item t = { TileID(i, j), g + h, h }; // F = g + h
	state.open.push(t);
}

/*
 * In the JPS algorithm, after a tile is taken off the open queue,
 * we don't process every adjacent neighbour (as in standard A*).
 * Instead we only move in a subset of directions (depending on the
 * direction from the predecessor); and instead of moving by a single
 * cell, we move up to the next jump point in that direction.
 * The AddJumped... functions do this by calling ProcessNeighbour
 * on the jump point (if any) in a certain direction.
 * The HasJumped... functions return whether there is any jump point
 * in that direction.
 * // 在 JPS 算法中，当一个单元格从开放队列中取出后，
 * // 我们不像标准 A* 那样处理所有相邻的邻居。
 * // 相反，我们只朝一个方向子集移动（具体方向取决于其父节点的方向）；
 * // 并且我们不是移动单个单元格，而是一直跳跃到该方向的下一个跳点。
 * // AddJumped... 系列函数通过在特定方向的跳点（如果存在）上调用 ProcessNeighbour 来实现这一点。
 * // HasJumped... 系列函数则返回该方向上是否存在跳点。
 */

 // JPS functions scan navcells towards one direction
 // OnTheWay tests whether we are scanning towards the right direction, to avoid useless scans
 // // JPS 函数会朝一个方向扫描导航单元格
 // // OnTheWay 用于测试我们是否正朝着正确的方向（目标方向）扫描，以避免无用的扫描
inline bool OnTheWay(int i, int j, int di, int dj, int iGoal, int jGoal)
{
	if (dj != 0)
	{
		// We're not moving towards the goal
		// // 垂直方向上没有朝目标移动
		if ((jGoal - j) * dj < 0)
			return false;
	}
	else if (j != jGoal) // 如果是纯水平移动，但行号与目标不同，说明方向错误
		return false;

	if (di != 0)
	{
		// We're not moving towards the goal
		// // 水平方向上没有朝目标移动
		if ((iGoal - i) * di < 0)
			return false;
	}
	else if (i != iGoal) // 如果是纯垂直移动，但列号与目标不同，说明方向错误
		return false;

	return true; // 所有检查通过，方向正确
}

// 在水平方向上寻找并处理跳点
void LongPathfinder::AddJumpedHoriz(int i, int j, int di, PathCost g, PathfinderState& state, bool detectGoal) const
{
	// 如果使用缓存
	if (m_UseJPSCache)
	{
		int jump;
		if (di > 0) // 向右
			jump = state.jpc->GetJumpPointRight(i, j, state.goal);
		else // 向左
			jump = state.jpc->GetJumpPointLeft(i, j, state.goal);

		if (jump != i) // 如果找到了跳点 (不是原地)
			ProcessNeighbour(i, j, jump, j, g, state);
	}
	else // 如果不使用缓存，则进行线性扫描
	{
		ASSERT(di == 1 || di == -1);
		int ni = i + di;
		while (true)
		{
			// 遇到障碍物，停止扫描
			if (!PASSABLE(ni, j))
				break;

			// 如果需要检测目标且当前点是目标
			if (detectGoal && state.goal.NavcellContainsGoal(ni, j))
			{
				// 清空开放列表，因为我们直接找到了通往目标的路径，优先处理它
				state.open.clear();
				ProcessNeighbour(i, j, ni, j, g, state);
				break;
			}

			// 检查是否为跳点（强迫邻居规则）
			if ((!PASSABLE(ni - di, j - 1) && PASSABLE(ni, j - 1)) ||
				(!PASSABLE(ni - di, j + 1) && PASSABLE(ni, j + 1)))
			{
				ProcessNeighbour(i, j, ni, j, g, state);
				break;
			}

			// 继续向同方向前进
			ni += di;
		}
	}
}

// Returns the i-coordinate of the jump point if it exists, else returns i
// // 如果水平方向存在跳点，返回其 i 坐标，否则返回 i
int LongPathfinder::HasJumpedHoriz(int i, int j, int di, PathfinderState& state, bool detectGoal) const
{
	if (m_UseJPSCache)
	{
		int jump;
		if (di > 0)
			jump = state.jpc->GetJumpPointRight(i, j, state.goal);
		else
			jump = state.jpc->GetJumpPointLeft(i, j, state.goal);

		return jump;
	}
	else
	{
		ASSERT(di == 1 || di == -1);
		int ni = i + di;
		while (true)
		{
			if (!PASSABLE(ni, j))
				return i;

			if (detectGoal && state.goal.NavcellContainsGoal(ni, j))
			{
				state.open.clear();
				return ni;
			}

			if ((!PASSABLE(ni - di, j - 1) && PASSABLE(ni, j - 1)) ||
				(!PASSABLE(ni - di, j + 1) && PASSABLE(ni, j + 1)))
				return ni;

			ni += di;
		}
	}
}

// 在垂直方向上寻找并处理跳点
void LongPathfinder::AddJumpedVert(int i, int j, int dj, PathCost g, PathfinderState& state, bool detectGoal) const
{
	if (m_UseJPSCache)
	{
		int jump;
		if (dj > 0) // 向上
			jump = state.jpc->GetJumpPointUp(i, j, state.goal);
		else // 向下
			jump = state.jpc->GetJumpPointDown(i, j, state.goal);

		if (jump != j)
			ProcessNeighbour(i, j, i, jump, g, state);
	}
	else // 线性扫描
	{
		ASSERT(dj == 1 || dj == -1);
		int nj = j + dj;
		while (true)
		{
			if (!PASSABLE(i, nj))
				break;

			if (detectGoal && state.goal.NavcellContainsGoal(i, nj))
			{
				state.open.clear();
				ProcessNeighbour(i, j, i, nj, g, state);
				break;
			}

			// 强迫邻居规则
			if ((!PASSABLE(i - 1, nj - dj) && PASSABLE(i - 1, nj)) ||
				(!PASSABLE(i + 1, nj - dj) && PASSABLE(i + 1, nj)))
			{
				ProcessNeighbour(i, j, i, nj, g, state);
				break;
			}

			nj += dj;
		}
	}
}

// Returns the j-coordinate of the jump point if it exists, else returns j
// // 如果垂直方向存在跳点，返回其 j 坐标，否则返回 j
int LongPathfinder::HasJumpedVert(int i, int j, int dj, PathfinderState& state, bool detectGoal) const
{
	if (m_UseJPSCache)
	{
		int jump;
		if (dj > 0)
			jump = state.jpc->GetJumpPointUp(i, j, state.goal);
		else
			jump = state.jpc->GetJumpPointDown(i, j, state.goal);

		return jump;
	}
	else
	{
		ASSERT(dj == 1 || dj == -1);
		int nj = j + dj;
		while (true)
		{
			if (!PASSABLE(i, nj))
				return j;

			if (detectGoal && state.goal.NavcellContainsGoal(i, nj))
			{
				state.open.clear();
				return nj;
			}

			if ((!PASSABLE(i - 1, nj - dj) && PASSABLE(i - 1, nj)) ||
				(!PASSABLE(i + 1, nj - dj) && PASSABLE(i + 1, nj)))
				return nj;

			nj += dj;
		}
	}
}

/*
 * We never cache diagonal jump points - they're usually so frequent that
 * a linear search is about as cheap and avoids the setup cost and memory cost.
 * // 我们从不缓存对角线跳点——它们通常出现得非常频繁，
 * // 以至于线性搜索的成本与使用缓存相差无几，还能避免设置和内存开销。
 */
void LongPathfinder::AddJumpedDiag(int i, int j, int di, int dj, PathCost g, PathfinderState& state) const
{
	ASSERT(di == 1 || di == -1);
	ASSERT(dj == 1 || dj == -1);

	int ni = i + di;
	int nj = j + dj;
	// 检查对角线方向是否朝向目标
	bool detectGoal = OnTheWay(i, j, di, dj, state.iGoal, state.jGoal);
	while (true)
	{
		// Stop if we hit an obstructed cell
		// // 如果遇到障碍物，停止
		if (!PASSABLE(ni, nj))
			return;

		// Stop if moving onto this cell caused us to
		// touch the corner of an obstructed cell
		// // 如果移动到这个单元格导致我们“切角”（碰到了障碍物的角），则停止。
		// 这是为了防止单位穿墙。
		if (!PASSABLE(ni - di, nj) || !PASSABLE(ni, nj - dj))
			return;

		// Process this cell if it's at the goal
		// // 如果当前单元格是目标点，则处理它
		if (detectGoal && state.goal.NavcellContainsGoal(ni, nj))
		{
			state.open.clear();
			ProcessNeighbour(i, j, ni, nj, g, state);
			return;
		}

		// 对角线移动的JPS核心：在对角线移动的每一步，都要检查其对应的水平和垂直方向上是否存在跳点。
		int fi = HasJumpedHoriz(ni, nj, di, state, detectGoal && OnTheWay(ni, nj, di, 0, state.iGoal, state.jGoal));
		int fj = HasJumpedVert(ni, nj, dj, state, detectGoal && OnTheWay(ni, nj, 0, dj, state.iGoal, state.jGoal));
		// 如果在水平或垂直方向上找到了跳点
		if (fi != ni || fj != nj)
		{
			// 则当前对角线点 (ni, nj) 本身就是一个跳点，处理它
			ProcessNeighbour(i, j, ni, nj, g, state);

			// 更新G值，为处理其子跳点做准备
			g += PathCost::diag(abs(ni - i));

			// 如果水平方向有跳点，把它也作为(ni,nj)的邻居处理
			if (fi != ni)
				ProcessNeighbour(ni, nj, fi, nj, g, state);

			// 如果垂直方向有跳点，也处理
			if (fj != nj)
				ProcessNeighbour(ni, nj, ni, fj, g, state);

			// 找到跳点后就返回，不再沿对角线继续扫描
			return;
		}

		// 如果没找到跳点，则继续沿对角线前进
		ni += di;
		nj += dj;
	}
}

// 计算JPS路径的主函数
void LongPathfinder::ComputeJPSPath(const HierarchicalPathfinder& hierPath, entity_pos_t x0, entity_pos_t z0, const PathGoal& origGoal, pass_class_t passClass, WaypointPath& path) const
{
	PROFILE2("ComputePathJPS");
	PathfinderState state = { 0 }; // 初始化寻路状态

	if (m_UseJPSCache)
	{
		// Needs to lock for construction, or several threads might try doing that at the same time.
		// // 需要加锁构造，否则多个线程可能同时尝试构造缓存。
		static std::mutex JPCMutex;
		std::unique_lock<std::mutex> lock(JPCMutex);
		// 查找该通行性类别的缓存是否存在
		std::map<pass_class_t, std::shared_ptr<JumpPointCache>>::const_iterator it = m_JumpPointCache.find(passClass);
		if (it != m_JumpPointCache.end())
			state.jpc = it->second.get();

		// 如果缓存不存在，则创建一个新的
		if (!state.jpc)
		{
			m_JumpPointCache[passClass] = std::make_shared<JumpPointCache>();
			m_JumpPointCache[passClass]->reset(m_Grid, passClass);
			debug_printf("PATHFINDER: JPC memory: %d kB\n", (int)state.jpc->GetMemoryUsage() / 1024);
			state.jpc = m_JumpPointCache[passClass].get();
		}
	}

	// Convert the start coordinates to tile indexes
	// // 将起始坐标转换为单元格索引
	u16 i0, j0;
	Pathfinding::NearestNavcell(x0, z0, i0, j0, m_GridSize, m_GridSize);

	if (!IS_PASSABLE(m_Grid->get(i0, j0), passClass))
	{
		// The JPS pathfinder requires units to be on passable tiles
		// (otherwise it might crash), so handle the supposedly-invalid
		// state specially
		// // JPS寻路器要求单位必须在可通行的单元格上（否则可能崩溃），
		// // 所以对这种理论上无效的状态进行特殊处理。
		// 寻找最近的一个可通行单元格作为新的起点
		hierPath.FindNearestPassableNavcell(i0, j0, passClass);
	}

	state.goal = origGoal;

	// Make the goal reachable. This includes shortening the path if the goal is in a non-passable
	// region, transforming non-point goals to reachable point goals, etc.
	// // 使目标可达。这包括如果目标在不可通行区域则缩短路径，
	// // 将非点状目标转换为可达的点状目标等。
	hierPath.MakeGoalReachable(i0, j0, state.goal, passClass);

	ENSURE(state.goal.type == PathGoal::POINT);

	// If we're already at the goal tile, then move directly to the exact goal coordinates
	// // 如果我们已经在目标单元格内，则直接移动到精确的目标坐标
	if (state.goal.NavcellContainsGoal(i0, j0))
	{
		path.m_Waypoints.emplace_back(Waypoint{ state.goal.x, state.goal.z });
		return;
	}

	// 获取目标点的单元格索引
	Pathfinding::NearestNavcell(state.goal.x, state.goal.z, state.iGoal, state.jGoal, m_GridSize, m_GridSize);

	ENSURE((state.goal.x / Pathfinding::NAVCELL_SIZE).ToInt_RoundToNegInfinity() == state.iGoal);
	ENSURE((state.goal.z / Pathfinding::NAVCELL_SIZE).ToInt_RoundToNegInfinity() == state.jGoal);

	state.passClass = passClass;

	state.steps = 0; // 寻路步数计数

	// 初始化用于存储寻路过程中间数据的网格
	state.tiles = new PathfindTileGrid(m_Grid->m_W, m_Grid->m_H);
	state.terrain = m_Grid; // 引用地形数据

	// 初始化最佳点为起点
	state.iBest = i0;
	state.jBest = j0;
	state.hBest = CalculateHeuristic(i0, j0, state.iGoal, state.jGoal);

	// 将起点加入开放列表
	PriorityQueue::Item start = { TileID(i0, j0), PathCost() };
	state.open.push(start);
	state.tiles->get(i0, j0).SetStatusOpen();
	state.tiles->get(i0, j0).SetPred(i0, j0, i0, j0);
	state.tiles->get(i0, j0).SetCost(PathCost());

	// A* 主循环
	while (true)
	{
		++state.steps;

		// If we ran out of tiles to examine, give up
		// // 如果开放列表为空，说明无路可走，放弃
		if (state.open.empty())
			break;

		// Move best tile from open to closed
		// // 从开放列表中取出成本最低的单元格，并移入关闭列表
		PriorityQueue::Item curr = state.open.pop();
		u16 i = curr.id.i();
		u16 j = curr.id.j();
		state.tiles->get(i, j).SetStatusClosed();

		// If we've reached the destination, stop
		// // 如果到达了目标点，停止搜索
		if (state.goal.NavcellContainsGoal(i, j))
		{
			state.iBest = i;
			state.jBest = j;
			state.hBest = PathCost();
			break;
		}

		PathfindTile tile = state.tiles->get(i, j);
		PathCost g = tile.GetCost();

		// Get the direction of the predecessor tile from this tile
		// // 获取从父节点到当前节点的方向
		int dpi = tile.GetPredDI();
		int dpj = tile.GetPredDJ();
		dpi = (dpi < 0 ? -1 : dpi > 0 ? 1 : 0); // 归一化为 -1, 0, 1
		dpj = (dpj < 0 ? -1 : dpj > 0 ? 1 : 0);

		// JPS 剪枝规则：根据来的方向，决定要探索哪些“自然邻居”和“强迫邻居”
		if (dpi != 0 && dpj == 0) // 从水平方向来
		{
			// Moving horizontally from predecessor
			if (!PASSABLE(i + dpi, j - 1)) // 检查强迫邻居
			{
				AddJumpedDiag(i, j, -dpi, -1, g, state);
				AddJumpedVert(i, j, -1, g, state, OnTheWay(i, j, 0, -1, state.iGoal, state.jGoal));
			}
			if (!PASSABLE(i + dpi, j + 1)) // 检查强迫邻居
			{
				AddJumpedDiag(i, j, -dpi, +1, g, state);
				AddJumpedVert(i, j, +1, g, state, OnTheWay(i, j, 0, +1, state.iGoal, state.jGoal));
			}
			AddJumpedHoriz(i, j, -dpi, g, state, OnTheWay(i, j, -dpi, 0, state.iGoal, state.jGoal)); // 探索自然邻居
		}
		else if (dpi == 0 && dpj != 0) // 从垂直方向来
		{
			// Moving vertically from predecessor
			if (!PASSABLE(i - 1, j + dpj)) // 检查强迫邻居
			{
				AddJumpedDiag(i, j, -1, -dpj, g, state);
				AddJumpedHoriz(i, j, -1, g, state, OnTheWay(i, j, -1, 0, state.iGoal, state.jGoal));
			}
			if (!PASSABLE(i + 1, j + dpj)) // 检查强迫邻居
			{
				AddJumpedDiag(i, j, +1, -dpj, g, state);
				AddJumpedHoriz(i, j, +1, g, state, OnTheWay(i, j, +1, 0, state.iGoal, state.jGoal));
			}
			AddJumpedVert(i, j, -dpj, g, state, OnTheWay(i, j, 0, -dpj, state.iGoal, state.jGoal)); // 探索自然邻居
		}
		else if (dpi != 0 && dpj != 0) // 从对角线方向来
		{
			// Moving diagonally from predecessor
			// 探索水平和垂直两个自然邻居，以及对角线方向
			AddJumpedHoriz(i, j, -dpi, g, state, OnTheWay(i, j, -dpi, 0, state.iGoal, state.jGoal));
			AddJumpedVert(i, j, -dpj, g, state, OnTheWay(i, j, 0, -dpj, state.iGoal, state.jGoal));
			AddJumpedDiag(i, j, -dpi, -dpj, g, state);
		}
		else
		{
			// No predecessor, i.e. the start tile
			// // 没有父节点，说明是起点
			// Start searching in every direction
			// // 向所有八个方向进行搜索（标准A*的起点行为）
			bool passl = PASSABLE(i - 1, j);
			bool passr = PASSABLE(i + 1, j);
			bool passd = PASSABLE(i, j - 1);
			bool passu = PASSABLE(i, j + 1);

			if (passl && passd)
				ProcessNeighbour(i, j, i - 1, j - 1, g, state);
			if (passr && passd)
				ProcessNeighbour(i, j, i + 1, j - 1, g, state);
			if (passl && passu)
				ProcessNeighbour(i, j, i - 1, j + 1, g, state);
			if (passr && passu)
				ProcessNeighbour(i, j, i + 1, j + 1, g, state);
			if (passl)
				ProcessNeighbour(i, j, i - 1, j, g, state);
			if (passr)
				ProcessNeighbour(i, j, i + 1, j, g, state);
			if (passd)
				ProcessNeighbour(i, j, i, j - 1, g, state);
			if (passu)
				ProcessNeighbour(i, j, i, j + 1, g, state);
		}
	}

	// Reconstruct the path (in reverse)
	// // (反向)重构路径
	u16 ip = state.iBest, jp = state.jBest;
	while (ip != i0 || jp != j0)
	{
		PathfindTile& n = state.tiles->get(ip, jp);
		entity_pos_t x, z;
		// 获取单元格中心点坐标作为路径点
		Pathfinding::NavcellCenter(ip, jp, x, z);
		path.m_Waypoints.emplace_back(Waypoint{ x, z });

		// Follow the predecessor link
		// // 跟随父节点指针回溯
		ip = n.GetPredI(ip);
		jp = n.GetPredJ(jp);
	}
	// The last waypoint is slightly incorrect (it's not the goal but the center
	// of the navcell of the goal), so replace it
	// // 最后一个路径点是不精确的（它是目标单元格的中心，而不是精确的目标点），所以替换它
	if (!path.m_Waypoints.empty())
		path.m_Waypoints.front() = { state.goal.x, state.goal.z };

	// 优化路径点，使其更平滑（拉直）
	ImprovePathWaypoints(path, passClass, origGoal.maxdist, x0, z0);

	// Save this grid for debug display
	// // 为调试显示保存此网格
	if (m_Debug.Overlay)
	{
		std::lock_guard<std::mutex> lock(g_DebugMutex);
		delete m_Debug.Grid;
		m_Debug.Grid = state.tiles;
		m_Debug.Steps = state.steps;
		m_Debug.Goal = state.goal;
	}
	else
		SAFE_DELETE(state.tiles);
}

#undef PASSABLE

// 优化路径点，移除不必要的拐点，使路径更直
void LongPathfinder::ImprovePathWaypoints(WaypointPath& path, pass_class_t passClass, entity_pos_t maxDist, entity_pos_t x0, entity_pos_t z0) const
{
	if (path.m_Waypoints.empty())
		return;

	// 如果设置了最大距离，检查第一个路径点是否太远，如果是，则在中间插入一个点
	if (maxDist > fixed::Zero())
	{
		CFixedVector2D start(x0, z0);
		CFixedVector2D first(path.m_Waypoints.back().x, path.m_Waypoints.back().z);
		CFixedVector2D offset = first - start;
		if (offset.CompareLength(maxDist) > 0)
		{
			offset.Normalize(maxDist);
			path.m_Waypoints.emplace_back(Waypoint{ (start + offset).X, (start + offset).Y });
		}
	}

	if (path.m_Waypoints.size() < 2)
		return;

	std::vector<Waypoint>& waypoints = path.m_Waypoints;
	std::vector<Waypoint> newWaypoints;

	// 拉绳子/字符串平滑算法
	// 从一个点出发，尝试直接连接到后续的路径点，只要中间没有障碍物，就跳过中间的点
	CFixedVector2D prev(waypoints.front().x, waypoints.front().z);
	newWaypoints.push_back(waypoints.front());
	for (size_t k = 1; k < waypoints.size() - 1; ++k)
	{
		CFixedVector2D ahead(waypoints[k + 1].x, waypoints[k + 1].z);
		CFixedVector2D curr(waypoints[k].x, waypoints[k].z);

		// 如果两个路径点之间距离过大，在中间插入一个点以保证路径平滑
		if (maxDist > fixed::Zero() && (curr - prev).CompareLength(maxDist) > 0)
		{
			prev = prev + (curr - prev) / 2;
			newWaypoints.emplace_back(Waypoint{ prev.X, prev.Y });
		}

		// If we're mostly straight, don't even bother.
		// // 如果路径基本是直的，就不用费心检查了。
		if ((ahead - curr).Perpendicular().Dot(curr - prev).Absolute() <= fixed::Epsilon() * 100)
			continue;

		// If we can't move directly from 'prev' to 'ahead', we need 'curr' as an intermediate waypoint
		// // 如果我们无法从 'prev' 直接移动到 'ahead'，那么我们就需要 'curr' 作为中间路径点
		if (!Pathfinding::CheckLineMovement(prev.X, prev.Y, ahead.X, ahead.Y, passClass, *m_Grid))
		{
			prev = CFixedVector2D(waypoints[k].x, waypoints[k].z);
			newWaypoints.push_back(waypoints[k]);
		}
	}
	// 添加最后一个路径点
	newWaypoints.push_back(waypoints.back());
	path.m_Waypoints.swap(newWaypoints);
}

// 获取JPS寻路的调试数据
void LongPathfinder::GetDebugDataJPS(u32& steps, double& time, Grid<u8>& grid) const
{
	steps = m_Debug.Steps;
	time = m_Debug.Time;

	if (!m_Debug.Grid)
		return;

	std::lock_guard<std::mutex> lock(g_DebugMutex);

	u16 iGoal, jGoal;
	Pathfinding::NearestNavcell(m_Debug.Goal.x, m_Debug.Goal.z, iGoal, jGoal, m_GridSize, m_GridSize);

	// 创建一个用于显示的调试网格
	grid = Grid<u8>(m_Debug.Grid->m_W, m_Debug.Grid->m_H);
	for (u16 j = 0; j < grid.m_H; ++j)
	{
		for (u16 i = 0; i < grid.m_W; ++i)
		{
			if (i == iGoal && j == jGoal)
				continue;
			PathfindTile t = m_Debug.Grid->get(i, j);
			// 1: 开放列表中的点, 2: 关闭列表中的点
			grid.set(i, j, (t.IsOpen() ? 1 : 0) | (t.IsClosed() ? 2 : 0));
		}
	}
}

// 计算路径的公共入口点
void LongPathfinder::ComputePath(const HierarchicalPathfinder& hierPath, entity_pos_t x0, entity_pos_t z0, const PathGoal& origGoal,
	pass_class_t passClass, WaypointPath& path) const
{
	if (!m_Grid)
	{
		LOGERROR("The pathfinder grid hasn't been setup yet, aborting ComputeJPSPath");
		return;
	}

	ComputeJPSPath(hierPath, x0, z0, origGoal, passClass, path);
}

// 计算带有动态排除区域的路径
void LongPathfinder::ComputePath(const HierarchicalPathfinder& hierPath, entity_pos_t x0, entity_pos_t z0, const PathGoal& origGoal,
	pass_class_t passClass, std::vector<CircularRegion> excludedRegions, WaypointPath& path)
{
	// 生成一个临时的特殊地图，其中包含排除区域作为障碍物
	GenerateSpecialMap(passClass, excludedRegions);
	// 在这个特殊地图上进行寻路
	ComputeJPSPath(hierPath, x0, z0, origGoal, SPECIAL_PASS_CLASS, path);
}

// 判断一个单元格是否在给定的圆形区域内
inline bool InRegion(u16 i, u16 j, CircularRegion region)
{
	fixed cellX = Pathfinding::NAVCELL_SIZE * i;
	fixed cellZ = Pathfinding::NAVCELL_SIZE * j;

	return CFixedVector2D(cellX - region.x, cellZ - region.z).CompareLength(region.r) <= 0;
}

// 生成一个临时的特殊地图，将排除区域标记为不可通行
void LongPathfinder::GenerateSpecialMap(pass_class_t passClass, std::vector<CircularRegion> excludedRegions)
{
	for (u16 j = 0; j < m_Grid->m_H; ++j)
	{
		for (u16 i = 0; i < m_Grid->m_W; ++i)
		{
			NavcellData n = m_Grid->get(i, j);
			// 如果本来就不可通行，直接标记
			if (!IS_PASSABLE(n, passClass))
			{
				n |= SPECIAL_PASS_CLASS;
				m_Grid->set(i, j, n);
				continue;
			}

			// 检查是否在任何一个排除区域内
			for (CircularRegion& region : excludedRegions)
			{
				if (!InRegion(i, j, region))
					continue;

				// 如果在，标记为特殊不可通行
				n |= SPECIAL_PASS_CLASS;
				break;
			}
			m_Grid->set(i, j, n);
		}
	}
}

/**
 * Terrain overlay for pathfinder debugging.
 * Renders a representation of the most recent pathfinding operation.
 * // 用于寻路调试的地形覆盖层。
 * // 渲染最近一次寻路操作的可视化表示。
 */
class LongOverlay : public TerrainTextureOverlay
{
public:
	LongPathfinder& m_Pathfinder;

	LongOverlay(LongPathfinder& pathfinder) :
		TerrainTextureOverlay(Pathfinding::NAVCELLS_PER_TERRAIN_TILE), m_Pathfinder(pathfinder)
	{
	}

	// 构建用于显示的RGBA纹理
	virtual void BuildTextureRGBA(u8* data, size_t w, size_t h)
	{
		// Grab the debug data for the most recently generated path
		// // 获取最近一次生成路径的调试数据
		u32 steps;
		double time;
		Grid<u8> debugGrid;
		m_Pathfinder.GetDebugData(steps, time, debugGrid);

		// Render navcell passability
		// // 渲染导航单元格的通行性
		u8* p = data;
		for (size_t j = 0; j < h; ++j)
		{
			for (size_t i = 0; i < w; ++i)
			{
				SColor4ub color(0, 0, 0, 0); // 默认透明
				// 不可通行区域显示为红色
				if (!IS_PASSABLE(m_Pathfinder.m_Grid->get((int)i, (int)j), m_Pathfinder.m_Debug.PassClass))
					color = SColor4ub(255, 0, 0, 127);

				// 如果有调试网格数据
				if (debugGrid.m_W && debugGrid.m_H)
				{
					u8 n = debugGrid.get((int)i, (int)j);

					// 开放列表中的点为黄色
					if (n == 1)
						color = SColor4ub(255, 255, 0, 127);
					// 关闭列表中的点为绿色
					else if (n == 2)
						color = SColor4ub(0, 255, 0, 127);

					// 目标点为蓝色
					if (m_Pathfinder.m_Debug.Goal.NavcellContainsGoal(i, j))
						color = SColor4ub(0, 0, 255, 127);
				}

				// 写入像素颜色
				*p++ = color.R;
				*p++ = color.G;
				*p++ = color.B;
				*p++ = color.A;
			}
		}

		// Render the most recently generated path
		// // 渲染最近生成的路径
		if (m_Pathfinder.m_Debug.Path && !m_Pathfinder.m_Debug.Path->m_Waypoints.empty())
		{
			std::vector<Waypoint>& waypoints = m_Pathfinder.m_Debug.Path->m_Waypoints;
			u16 ip = 0, jp = 0;
			for (size_t k = 0; k < waypoints.size(); ++k)
			{
				u16 i, j;
				Pathfinding::NearestNavcell(waypoints[k].x, waypoints[k].z, i, j, m_Pathfinder.m_GridSize, m_Pathfinder.m_GridSize);
				if (k == 0)
				{
					ip = i;
					jp = j;
				}
				else
				{
					// 在两个路径点之间画一条线
					bool firstCell = true;
					do
					{
						if (data[(jp * w + ip) * 4 + 3] == 0) // 只在透明像素上画
						{
							data[(jp * w + ip) * 4 + 0] = 0xFF; // 白色
							data[(jp * w + ip) * 4 + 1] = 0xFF;
							data[(jp * w + ip) * 4 + 2] = 0xFF;
							data[(jp * w + ip) * 4 + 3] = firstCell ? 0xA0 : 0x60; // 路径点本身更不透明
						}
						// 使用Bresenham算法的简化形式来画线
						ip = ip < i ? ip + 1 : ip > i ? ip - 1 : ip;
						jp = jp < j ? jp + 1 : jp > j ? jp - 1 : jp;
						firstCell = false;
					} while (ip != i || jp != j);
				}
			}
		}
	}
};

// These two functions must come below LongOverlay's definition.
// // 这两个函数必须在 LongOverlay 定义之后。
void LongPathfinder::SetDebugOverlay(bool enabled)
{
	if (enabled && !m_Debug.Overlay)
		m_Debug.Overlay = new LongOverlay(*this);
	else if (!enabled && m_Debug.Overlay)
		SAFE_DELETE(m_Debug.Overlay);
}

// 析构函数，清理调试用的资源
LongPathfinder::~LongPathfinder()
{
	SAFE_DELETE(m_Debug.Overlay);
	SAFE_DELETE(m_Debug.Grid);
	SAFE_DELETE(m_Debug.Path);
}
