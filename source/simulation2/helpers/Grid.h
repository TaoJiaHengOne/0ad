/* Copyright (C) 2020 Wildfire Games.
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

#ifndef INCLUDED_GRID
#define INCLUDED_GRID

#include "simulation2/serialization/SerializeTemplates.h"

#include <cstring>

 // 预处理器宏，用于在非调试(NDEBUG)版本中禁用格子图边界检查，以提高性能
#ifdef NDEBUG
#define GRID_BOUNDS_DEBUG 0
#else
#define GRID_BOUNDS_DEBUG 1
#endif

/**
 * 基础的二维数组，旨在存储地块数据，并支持由 ICmpObstructionManager 进行的延迟更新。
 * @c T 必须是一个可以用 0 初始化的 POD (Plain Old Data，普通旧数据) 类型。
 */
template<typename T>
class Grid
{
	friend struct SerializeHelper<Grid<T>>; // 序列化辅助类可以访问本类的私有成员
protected:
	// 用于方便起见的标签分发（Tag-dispatching）内部工具。
	struct default_type {}; // 默认类型标签
	struct is_pod { operator default_type() { return default_type{}; } }; // POD类型标签
	struct is_container { operator default_type() { return default_type{}; } }; // 容器类型标签

	// 用于检测类型是否拥有 value_type 成员的辅助模板
	template <typename U, typename = int> struct has_value_type : std::false_type { };
	template <typename U> struct has_value_type <U, decltype(std::declval<typename U::value_type>(), 0)> : std::true_type { };

	// std::conditional 的别名，用于根据条件选择类型
	template <typename U, typename A, typename B> using if_ = typename std::conditional<U::value, A, B>::type;

	// 根据类型 U 的特性（是否为POD，是否为容器）选择分发标签
	template<typename U>
	using dispatch = if_< std::is_pod<U>, is_pod,
		if_<has_value_type<U>, is_container,
		default_type>>;

public:
	// 默认构造函数
	Grid() : m_W(0), m_H(0), m_Data(NULL)
	{
	}

	// 带宽和高参数的构造函数
	Grid(u16 w, u16 h) : m_W(w), m_H(h), m_Data(NULL)
	{
		resize(w, h);
	}

	// 拷贝构造函数
	Grid(const Grid& g) : m_W(0), m_H(0), m_Data(NULL)
	{
		*this = g;
	}

	// 类型别名，表示格子图中存储的元素类型
	using value_type = T;
public:

	// 确保在调用之前，o 和 this 的大小相同。
	// 针对非POD类型的通用数据拷贝方法
	void copy_data(T* o, default_type) { std::copy(o, o + m_H * m_W, &m_Data[0]); }
	// 针对POD类型的优化数据拷贝方法，使用 memcpy 更快
	void copy_data(T* o, is_pod) { memcpy(m_Data, o, m_W * m_H * sizeof(T)); }

	// 拷贝赋值运算符
	Grid& operator=(const Grid& g)
	{
		if (this == &g)
			return *this;

		if (m_W == g.m_W && m_H == g.m_H)
		{
			copy_data(g.m_Data, dispatch<T>{});
			return *this;
		}

		m_W = g.m_W;
		m_H = g.m_H;
		SAFE_ARRAY_DELETE(m_Data);
		if (g.m_Data)
		{
			m_Data = new T[m_W * m_H];
			copy_data(g.m_Data, dispatch<T>{});
		}
		return *this;
	}

	// 交换两个 Grid 对象的内容
	void swap(Grid& g)
	{
		std::swap(m_Data, g.m_Data);
		std::swap(m_H, g.m_H);
		std::swap(m_W, g.m_W);
	}

	// 析构函数，释放内存
	~Grid()
	{
		SAFE_ARRAY_DELETE(m_Data);
	}

	// 确保在调用之前，o 和 this 的大小相同。
	// 针对非POD类型的通用数据比较方法
	bool compare_data(T* o, default_type) const { return std::equal(&m_Data[0], &m_Data[m_W * m_H], o); }
	// 针对POD类型的优化数据比较方法
	bool compare_data(T* o, is_pod) const { return memcmp(m_Data, o, m_W * m_H * sizeof(T)) == 0; }

	// 等于运算符
	bool operator==(const Grid& g) const
	{
		if (!compare_sizes(&g))
			return false;

		return compare_data(g.m_Data, dispatch<T>{});
	}
	// 不等于运算符
	bool operator!=(const Grid& g) const { return !(*this == g); }

	// 检查格子图是否为空（无尺寸）
	bool blank() const
	{
		return m_W == 0 && m_H == 0;
	}

	// 获取宽度
	u16 width() const { return m_W; };
	// 获取高度
	u16 height() const { return m_H; };


	// 检查矩形区域内是否有任何设置的值（通用版本，未实现）
	bool _any_set_in_square(int, int, int, int, default_type) const
	{
		static_assert(!std::is_same<T, T>::value, "Not implemented.");
		return false; // 修复警告
	}
	// 检查矩形区域内是否有任何设置的值（POD优化版本）
	bool _any_set_in_square(int i0, int j0, int i1, int j1, is_pod) const
	{
#if GRID_BOUNDS_DEBUG
		ENSURE(i0 >= 0 && j0 >= 0 && i1 <= m_W && j1 <= m_H);
#endif
		for (int j = j0; j < j1; ++j)
		{
			int sum = 0;
			for (int i = i0; i < i1; ++i)
				sum += m_Data[j * m_W + i];
			if (sum > 0)
				return true;
		}
		return false;
	}

	// 公共接口：检查矩形区域内是否有任何设置的值
	bool any_set_in_square(int i0, int j0, int i1, int j1) const
	{
		return _any_set_in_square(i0, j0, i1, j1, dispatch<T>{});
	}

	// 重置数据（通用版本）
	void reset_data(default_type) { std::fill(&m_Data[0], &m_Data[m_H * m_W], T{}); }
	// 重置数据（POD优化版本）
	void reset_data(is_pod) { memset(m_Data, 0, m_W * m_H * sizeof(T)); }

	// 将数据重置为其默认构造值（通常是0），不改变大小。
	void reset()
	{
		if (m_Data)
			reset_data(dispatch<T>{});
	}

	// 清空格子图，将大小设置为0并释放任何数据。
	void clear()
	{
		resize(0, 0);
	}

	// 重新设置格子图的大小
	void resize(u16 w, u16 h)
	{
		SAFE_ARRAY_DELETE(m_Data);
		m_W = w;
		m_H = h;

		if (!m_W && !m_H)
			return;

		m_Data = new T[m_W * m_H];
		ENSURE(m_Data);
		reset();
	}

	// 将两个相同大小的格子图相加
	void add(const Grid& g)
	{
#if GRID_BOUNDS_DEBUG
		ENSURE(g.m_W == m_W && g.m_H == m_H);
#endif
		for (int i = 0; i < m_H * m_W; ++i)
			m_Data[i] += g.m_Data[i];
	}

	// 对两个格子图进行按位或操作
	void bitwise_or(const Grid& g)
	{
		if (this == &g)
			return;

#if GRID_BOUNDS_DEBUG
		ENSURE(g.m_W == m_W && g.m_H == m_H);
#endif
		for (int i = 0; i < m_H * m_W; ++i)
			m_Data[i] |= g.m_Data[i];
	}

	// 设置指定位置的值
	void set(int i, int j, const T& value)
	{
#if GRID_BOUNDS_DEBUG
		ENSURE(0 <= i && i < m_W && 0 <= j && j < m_H);
#endif
		m_Data[j * m_W + i] = value;
	}

	// 通过坐标对访问元素（重载[]运算符）
	T& operator[](std::pair<u16, u16> coords) { return get(coords.first, coords.second); }
	T& get(std::pair<u16, u16> coords) { return get(coords.first, coords.second); }

	// 通过坐标对访问元素（const版本）
	T& operator[](std::pair<u16, u16> coords) const { return get(coords.first, coords.second); }
	T& get(std::pair<u16, u16> coords) const { return get(coords.first, coords.second); }

	// 通过i, j坐标访问元素
	T& get(int i, int j)
	{
#if GRID_BOUNDS_DEBUG
		ENSURE(0 <= i && i < m_W && 0 <= j && j < m_H);
#endif
		return m_Data[j * m_W + i];
	}

	// 通过i, j坐标访问元素（const版本）
	T& get(int i, int j) const
	{
#if GRID_BOUNDS_DEBUG
		ENSURE(0 <= i && i < m_W && 0 <= j && j < m_H);
#endif
		return m_Data[j * m_W + i];
	}

	// 比较两个格子图的大小
	template<typename U>
	bool compare_sizes(const Grid<U>* g) const
	{
		return g && m_W == g->m_W && m_H == g->m_H;
	}

	u16 m_W, m_H; // 格子图的宽度和高度
	T* m_Data; // 指向格子图数据的指针
};


/**
 * 序列化一个格子图，应用一个假定高效的简单 RLE (行程长度编码) 压缩。
 */
template<typename T>
struct SerializeHelper<Grid<T>>
{
	// 序列化操作符
	void operator()(ISerializer& serialize, const char* name, Grid<T>& value)
	{
		size_t len = value.m_H * value.m_W;
		serialize.NumberU16_Unbounded("width", value.m_W);
		serialize.NumberU16_Unbounded("height", value.m_H);
		if (len == 0)
			return;
		u32 count = 1;
		T prevVal = value.m_Data[0];
		for (size_t i = 1; i < len; ++i)
		{
			if (prevVal == value.m_Data[i])
			{
				count++;
				continue;
			}
			serialize.NumberU32_Unbounded("#", count);
			Serializer(serialize, name, prevVal);
			count = 1;
			prevVal = value.m_Data[i];
		}
		serialize.NumberU32_Unbounded("#", count);
		Serializer(serialize, name, prevVal);
	}

	// 反序列化操作符
	void operator()(IDeserializer& deserialize, const char* name, Grid<T>& value)
	{
		u16 w, h;
		deserialize.NumberU16_Unbounded("width", w);
		deserialize.NumberU16_Unbounded("height", h);
		u32 len = h * w;
		value.resize(w, h);
		for (size_t i = 0; i < len;)
		{
			u32 count;
			deserialize.NumberU32_Unbounded("#", count);
			T el;
			Serializer(deserialize, name, el);
			std::fill(&value.m_Data[i], &value.m_Data[i + count], el);
			i += count;
		}
	}
};


/**
 * 与 Grid 类似，但针对稀疏使用进行了优化（格子图被细分为
 * 存储桶，其内容仅在需要时才初始化，以节省 memset 的开销）。
 */
template<typename T>
class SparseGrid
{
	NONCOPYABLE(SparseGrid); // 使该类不可拷贝

	// 定义存储桶的位大小和尺寸
	enum { BucketBits = 4, BucketSize = 1 << BucketBits };

	// 获取指定位置的存储桶
	T* GetBucket(int i, int j)
	{
		size_t b = (j >> BucketBits) * m_BW + (i >> BucketBits);
		if (!m_Data[b])
		{
			// 如果存储桶不存在，则动态创建
			m_Data[b] = new T[BucketSize * BucketSize]();
		}
		return m_Data[b];
	}

public:
	// 构造函数
	SparseGrid(u16 w, u16 h) : m_W(w), m_H(h), m_DirtyID(0)
	{
		ENSURE(m_W && m_H);

		m_BW = (u16)((m_W + BucketSize - 1) >> BucketBits);
		m_BH = (u16)((m_H + BucketSize - 1) >> BucketBits);

		m_Data = new T * [m_BW * m_BH]();
	}

	// 析构函数
	~SparseGrid()
	{
		reset();
		SAFE_ARRAY_DELETE(m_Data);
	}

	// 重置稀疏格子图，清空所有存储桶
	void reset()
	{
		for (size_t i = 0; i < (size_t)(m_BW * m_BH); ++i)
			SAFE_ARRAY_DELETE(m_Data[i]);

		// 通过就地放置new，用值构造来重置 m_Data。
		m_Data = new (m_Data) T * [m_BW * m_BH]();
	}

	// 设置指定位置的值
	void set(int i, int j, const T& value)
	{
#if GRID_BOUNDS_DEBUG
		ENSURE(0 <= i && i < m_W && 0 <= j && j < m_H);
#endif
		GetBucket(i, j)[(j % BucketSize) * BucketSize + (i % BucketSize)] = value;
	}

	// 获取指定位置的值的引用
	T& get(int i, int j)
	{
#if GRID_BOUNDS_DEBUG
		ENSURE(0 <= i && i < m_W && 0 <= j && j < m_H);
#endif
		return GetBucket(i, j)[(j % BucketSize) * BucketSize + (i % BucketSize)];
	}

	u16 m_W, m_H; // 格子图的宽度和高度
	u16 m_BW, m_BH; // 以存储桶为单位的宽度和高度
	T** m_Data; // 指向存储桶指针数组的指针

	size_t m_DirtyID; // 如果此值 < ICmpObstructionManager 维护的ID，则表示需要更新
};

/**
 * 存储格子图“脏”信息的结构体，用于智能更新。
 */
struct GridUpdateInformation
{
	bool dirty; // 标志位，表示是否有任何部分是“脏”的
	bool globallyDirty; // 标志位，表示是否整个格子图都是“脏”的
	Grid<u8> dirtinessGrid; // 存储“脏”区域的格子图

	/**
	 * 将额外需要的更新信息合并进来，然后清除添加的源。
	 * 这通常可以通过仔细的内存管理来优化。
	 */
	void MergeAndClear(GridUpdateInformation& b)
	{
		ENSURE(dirtinessGrid.compare_sizes(&b.dirtinessGrid));

		bool wasDirty = dirty;

		dirty |= b.dirty;
		globallyDirty |= b.globallyDirty;

		// 如果当前格子图无用，则交换它
		if (!wasDirty)
			dirtinessGrid.swap(b.dirtinessGrid);
		// 如果新的格子图未被使用，则不必费心更新它
		else if (dirty && !globallyDirty)
			dirtinessGrid.bitwise_or(b.dirtinessGrid);

		b.Clean();
	}

	/**
	 * 将所有内容标记为干净。
	 */
	void Clean()
	{
		dirty = false;
		globallyDirty = false;
		dirtinessGrid.reset();
	}
};

#endif // INCLUDED_GRID
