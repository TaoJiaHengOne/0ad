/* Copyright (C) 2018 Wildfire Games.
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

 // 防止头文件被重复包含的宏保护
#ifndef INCLUDED_PATHGOAL
#define INCLUDED_PATHGOAL

#include "maths/FixedVector2D.h"
#include "simulation2/helpers/Position.h"

/**
 * @brief 寻路目标。
 * 目标可以是一个点、一个圆或一个正方形（矩形）。
 * 对于圆形/方形，形状内的任何点都被视为
 * 目标的一部分。
 * 此外，它也可以是一个“反向”的圆形/方形，即形状外的任何点
 * 都是目标的一部分。
 */
class PathGoal
{
public:
	// 寻路目标的类型枚举
	enum Type {
		POINT,			 // 单个点
		CIRCLE,          // 圆形内部区域
		INVERTED_CIRCLE, // 圆形外部区域
		SQUARE,          // 方形内部区域
		INVERTED_SQUARE  // 方形外部区域
	} type; // 目标类型

	entity_pos_t x, z; // 中心的位置坐标

	entity_pos_t hw, hh; // 如果是 [反向]方形, 则为半宽和半高; 如果是 [反向]圆形, 则 hw 为半径

	CFixedVector2D u, v; // 如果是 [反向]方形, 则为正交的单位轴向量

	entity_pos_t maxdist; // 两个寻路路径点之间期望的最大距离

	/**
	 * @brief 如果给定的导航单元格(navcell)包含了目标区域的一部分，则返回 true。
	 */
	bool NavcellContainsGoal(int i, int j) const;

	/**
	 * @brief 如果任何满足以下条件的导航单元格(i, j)
	 * min(i0,i1) <= i <= max(i0,i1)
	 * min(j0,j1) <= j <= max(j0,j1),
	 * 包含了目标区域的一部分，则返回 true。
	 * 如果为 true，参数 i 和 j (如果不为 NULL) 会被设置为距离 (i0, j0) 最近的目标导航单元格的坐标，
	 * 此处假设矩形的宽度或高度为1。
	 */
	bool NavcellRectContainsGoal(int i0, int j0, int i1, int j1, int* i, int* j) const;

	/**
	 * @brief 如果由 (x0,z0)-(x1,z1) 定义的矩形（包含边界）
	 * 包含了目标区域的一部分，则返回 true。
	 */
	bool RectContainsGoal(entity_pos_t x0, entity_pos_t z0, entity_pos_t x1, entity_pos_t z1) const;

	/**
	 * @brief 返回从点 pos 到目标形状上任意点的最小距离。
	 */
	fixed DistanceToPoint(CFixedVector2D pos) const;

	/**
	 * @brief 返回目标上距离点 pos 最近的点的坐标。
	 */
	CFixedVector2D NearestPointOnGoal(CFixedVector2D pos) const;
};

#endif // INCLUDED_PATHGOAL
