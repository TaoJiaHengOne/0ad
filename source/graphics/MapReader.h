/* Copyright (C) 2023 Wildfire Games.
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

#ifndef INCLUDED_MAPREADER
#define INCLUDED_MAPREADER

#include "MapIO.h"

#include "graphics/LightEnv.h"
#include "ps/CStr.h"
#include "ps/FileIo.h"
#include "scriptinterface/ScriptTypes.h"
#include "simulation2/system/Entity.h"

#include <memory>

 // 前向声明，避免不必要的头文件包含
class CTerrain;
class WaterManager;
class SkyManager;
class CLightEnv;
class CCinemaManager;
class CPostprocManager;
class CTriggerManager;
class CSimulation2;
class CSimContext;
class CTerrainTextureEntry;
class CGameView;
class CXMLReader;
class ScriptContext;
class ScriptInterface;

/**
 * 地图读取器类，继承自 CMapIO。
 * 负责从文件（.pmp, .xml）加载地图数据并构建游戏场景。
 */
class CMapReader : public CMapIO
{
	// 将 CXMLReader 声明为友元类，使其可以访问本类的私有成员
	friend class CXMLReader;

public:
	// 构造函数
	CMapReader();
	// 析构函数
	~CMapReader();

	// LoadMap: 尝试从给定的文件加载地图；如果成功，则根据新数据重新初始化场景
	void LoadMap(const VfsPath& pathname, const ScriptContext& cx, JS::HandleValue settings, CTerrain*, WaterManager*, SkyManager*, CLightEnv*, CGameView*,
		CCinemaManager*, CTriggerManager*, CPostprocManager* pPostproc, CSimulation2*, const CSimContext*,
		int playerID, bool skipEntities);

	// 加载随机地图
	void LoadRandomMap(const CStrW& scriptFile, const ScriptContext& cx, JS::HandleValue settings, CTerrain*, WaterManager*, SkyManager*, CLightEnv*, CGameView*, CCinemaManager*, CTriggerManager*, CPostprocManager* pPostproc_, CSimulation2*, int playerID);

private:
	// 为脚本加载脚本设置
	int LoadScriptSettings();

	// 仅加载玩家设置
	int LoadPlayerSettings();

	// 仅加载地图设置
	int LoadMapSettings();

	// UnpackTerrain: 从输入流中解包地形数据
	int UnpackTerrain();
	// UnpackCinema: 从输入流中解包过场动画轨道
	int UnpackCinema();

	// ApplyData: 获取所有输入数据，并据此重建场景
	int ApplyData();
	// 应用地形数据到地形管理器
	int ApplyTerrainData();

	// 从 XML 文件中读取一些杂项数据
	int ReadXML();

	// 从 XML 文件中读取实体数据
	int ReadXMLEntities();

	// 将随机地图设置复制到模拟层
	int LoadRMSettings();

	// 生成随机地图
	int StartMapGeneration(const CStrW& scriptFile);
	// 轮询地图生成状态
	int PollMapGeneration();

	// 将脚本数据解析到地形
	int ParseTerrain();

	// 将脚本数据解析到实体
	int ParseEntities();

	// 将脚本数据解析到环境
	int ParseEnvironment();

	// 将脚本数据解析到相机
	int ParseCamera();


	// 地图每边的地块（patch）数量
	ssize_t m_PatchesPerSide{ 0 };
	// 地图的高度图
	std::vector<u16> m_Heightmap;
	// 地图使用的地形纹理列表
	std::vector<CTerrainTextureEntry*> m_TerrainTextures;
	// 每个地块的描述
	std::vector<STileDesc> m_Tiles;
	// 文件中存储的光照环境
	CLightEnv m_LightEnv;
	// 启动脚本内容
	CStrW m_Script;

	// 随机地图数据
	JS::PersistentRootedValue m_ScriptSettings; // 脚本设置的持久化JS值
	JS::PersistentRootedValue m_MapData;        // 地图数据的持久化JS值

	// 地图生成器状态结构体
	struct GeneratorState;
	std::unique_ptr<GeneratorState> m_GeneratorState; // 指向生成器状态的智能指针

	CFileUnpacker unpacker; // 文件解包器
	CTerrain* pTerrain; // 指向地形对象的指针
	WaterManager* pWaterMan; // 指向水管理器的指针
	SkyManager* pSkyMan; // 指向天空管理器的指针
	CPostprocManager* pPostproc; // 指向后处理管理器的指针
	CLightEnv* pLightEnv; // 指向光照环境的指针
	CGameView* pGameView; // 指向游戏视图的指针
	CCinemaManager* pCinema; // 指向过场动画管理器的指针
	CTriggerManager* pTrigMan; // 指向触发器管理器的指针
	CSimulation2* pSimulation2; // 指向模拟层主对象的指针
	const CSimContext* pSimContext; // 指向模拟层上下文的指针
	int m_PlayerID; // 当前玩家ID
	bool m_SkipEntities; // 是否跳过实体加载
	VfsPath filename_xml; // XML文件的虚拟路径
	bool only_xml; // 是否只加载XML文件
	u32 file_format_version; // 文件格式版本
	entity_id_t m_StartingCameraTarget; // 初始相机目标的实体ID
	CVector3D m_StartingCamera; // 初始相机位置

	// UnpackTerrain 生成器状态
	// 将其初始化为0很重要 - 用于重置生成器状态
	size_t cur_terrain_tex{ 0 }; // 当前地形纹理索引
	size_t num_terrain_tex; // 地形纹理总数

	CXMLReader* xml_reader{ nullptr }; // 指向XML读取器的指针
};

/**
 * 一个受限的地图读取器，返回各种摘要信息
 * 供脚本（特别是GUI）使用。
 */
class CMapSummaryReader
{
public:
	/**
	 * 尝试加载一个地图文件。
	 * @param pathname 指向 .pmp 或 .xml 文件的路径
	 */
	PSRETURN LoadMap(const VfsPath& pathname);

	/**
	 * 返回一个如下形式的值：
	 * @code
	 * {
	 * "settings": { ... 地图的 <ScriptSettings> 内容 ... }
	 * }
	 * @endcode
	 */
	void GetMapSettings(const ScriptInterface& scriptInterface, JS::MutableHandleValue);

private:
	// 存储脚本设置的字符串
	CStr m_ScriptSettings;
};

#endif
