#ifndef MESSAGES_H__
#define MESSAGES_H__

#ifndef MESSAGES_SKIP_SETUP
#include "MessagesSetup.h"
#endif

#include <vector>
#include <string>

// TODO: organisation, documentation, etc

//////////////////////////////////////////////////////////////////////////

MESSAGE(Init,
		((bool, initsimulation)) // whether to initialise simulation/game-specific objects
		);

MESSAGE(Shutdown, );

struct eRenderView { enum renderViews { NONE, GAME, ACTOR }; };

MESSAGE(RenderEnable,
		((int, view)) // eRenderView
		);

// SetViewParam: used for hints to the renderer, e.g. to set wireframe mode;
// unrecognised param names are ignored
MESSAGE(SetViewParamB,
		((int, view)) // eRenderView
		((std::wstring, name))
		((bool, value))
		);
MESSAGE(SetViewParamC,
		((int, view)) // eRenderView
		((std::wstring, name))
		((Colour, value))
		);

//////////////////////////////////////////////////////////////////////////

QUERY(Ping, , );

//////////////////////////////////////////////////////////////////////////

MESSAGE(SetContext,
		((void*, context))
		);

MESSAGE(ResizeScreen,
		((int, width))
		((int, height))
		);

//////////////////////////////////////////////////////////////////////////

MESSAGE(GenerateMap,
		((int, size)) // size in number of patches
		);

MESSAGE(LoadMap,
		((std::wstring, filename))
		);

MESSAGE(SaveMap,
		((std::wstring, filename))
		);

//////////////////////////////////////////////////////////////////////////

MESSAGE(RenderStyle,
		((bool, wireframe))
		);

MESSAGE(MessageTrace,
		((bool, enable))
		);

MESSAGE(Screenshot,
		((int, tiles)) // the final image will be (640*tiles)x(480*tiles)
		);

struct sCinemaRecordCB
{
	unsigned char* buffer;
};
SHAREABLE_STRUCT(sCinemaRecordCB);

QUERY(CinemaRecord,
	  ((std::wstring, track))
	  ((int, framerate))
	  ((float, duration))
	  ((Callback<sCinemaRecordCB>, cb))
	  ,
	  );

//////////////////////////////////////////////////////////////////////////

MESSAGE(Brush,
		((int, width)) // number of vertices
		((int, height))
		((std::vector<float>, data)) // width*height array
		);

MESSAGE(BrushPreview,
		((bool, enable))
		((Position, pos)) // only used if enable==true
		);

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

QUERY(GetTerrainGroups,
	  , // no inputs
	  ((std::vector<std::wstring>, groupnames))
	  );

struct sTerrainGroupPreview
{
	Shareable<std::wstring> name;
	Shareable<std::vector<unsigned char> > imagedata; // RGB*size*size
};
SHAREABLE_STRUCT(sTerrainGroupPreview);

QUERY(GetTerrainGroupPreviews,
	  ((std::wstring, groupname))
	  ((int, imagewidth))
	  ((int, imageheight))
	  ,
	  ((std::vector<sTerrainGroupPreview>, previews))
	  );


//////////////////////////////////////////////////////////////////////////

struct sObjectsListItem
{
	Shareable<std::wstring> id;
	Shareable<std::wstring> name;
	Shareable<int> type; // 0 = entity, 1 = actor
};
SHAREABLE_STRUCT(sObjectsListItem);

QUERY(GetObjectsList,
	  , // no inputs
	  ((std::vector<sObjectsListItem>, objects)) // sorted by .name
	  );

struct sObjectSettings
{
	Shareable<int> player;
	Shareable<std::vector<std::wstring> > selections;

	// Some settings are immutable and therefore are ignored (and should be left
	// empty) when passed from the editor to the game:

	Shareable<std::vector<std::vector<std::wstring> > > variantgroups;
};
SHAREABLE_STRUCT(sObjectSettings);

// Preview object in the game world - creates a temporary unit at the given
// position, and removes it when the preview is next changed
MESSAGE(ObjectPreview,
		((std::wstring, id)) // or empty string => disable
		((sObjectSettings, settings))
		((Position, pos))
		((bool, usetarget)) // true => use 'target' for orientation; false => use 'angle'
		((Position, target))
		((float, angle))
		);

COMMAND(CreateObject, NOMERGE,
		((std::wstring, id))
		((sObjectSettings, settings))
		((Position, pos))
		((bool, usetarget)) // true => use 'target' for orientation; false => use 'angle'
		((Position, target))
		((float, angle))
		);

// Set an actor to be previewed on its own (i.e. without the game world).
// (Use RenderEnable to make it visible.)
MESSAGE(SetActorViewer,
		((std::wstring, id))
		((std::wstring, animation))
		((float, speed))
		((bool, flushcache)) // true => unload all actor files before starting the preview (because we don't have proper hotloading yet)
		);

//////////////////////////////////////////////////////////////////////////

QUERY(Exit,,); // no inputs nor outputs

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

struct eScrollConstantDir { enum { FORWARDS, BACKWARDS, LEFT, RIGHT }; };
MESSAGE(ScrollConstant,
		((int, view)) // eRenderView
		((int, dir)) // eScrollConstantDir
		((float, speed)) // set speed 0.0f to stop scrolling
		);

struct eScrollType { enum { FROM, TO }; };
MESSAGE(Scroll, // for scrolling by dragging the mouse FROM somewhere TO elsewhere
		((int, view)) // eRenderView
		((int, type)) // eScrollType
		((Position, pos))
		);

MESSAGE(SmoothZoom,
		((int, view)) // eRenderView
		((float, amount))
		);

struct eRotateAroundType { enum { FROM, TO }; };
MESSAGE(RotateAround,
		((int, view)) // eRenderView
		((int, type)) // eRotateAroundType
		((Position, pos))
		);

MESSAGE(LookAt,
		((int, view)) // eRenderView
		((Position, pos))
		((Position, target))
		);

//////////////////////////////////////////////////////////////////////////

struct sEnvironmentSettings
{
	Shareable<float> waterheight; // range 0..1 corresponds to min..max terrain height; out-of-bounds values allowed
	Shareable<float> watershininess; // range ???
	Shareable<float> waterwaviness; // range ???
	Shareable<float> watermurkiness; // range ???
	
	Shareable<Colour> watercolour;
	Shareable<Colour> watertint;
	Shareable<Colour> waterreflectiontint;
	Shareable<float> waterreflectiontintstrength; // range ???

	Shareable<float> sunrotation; // range -pi..+pi
	Shareable<float> sunelevation; // range -pi/2 .. +pi/2

	Shareable<std::wstring> skyset;

	Shareable<Colour> suncolour;
	Shareable<Colour> terraincolour;
	Shareable<Colour> unitcolour;
};
SHAREABLE_STRUCT(sEnvironmentSettings);

QUERY(GetEnvironmentSettings,
	  // no inputs
	  ,
	  ((sEnvironmentSettings, settings))
	  );

COMMAND(SetEnvironmentSettings, MERGE,
		((sEnvironmentSettings, settings))
		);

QUERY(GetSkySets,
	  // no inputs
	  ,
	  ((std::vector<std::wstring>, skysets))
	  );


//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

COMMAND(AlterElevation, MERGE,
		((Position, pos))
		((float, amount))
		);

COMMAND(FlattenElevation, MERGE,
		((Position, pos))
		((float, amount))
		);

struct ePaintTerrainPriority { enum { HIGH, LOW }; };
COMMAND(PaintTerrain, MERGE,
		((Position, pos))
		((std::wstring, texture))
		((int, priority)) // ePaintTerrainPriority
		);

//////////////////////////////////////////////////////////////////////////

QUERY(PickObject,
	  ((Position, pos))
	  ,
	  ((ObjectID, id))
	  ((int, offsetx))  // offset of object centre from input position
	  ((int, offsety)) //
	  );

COMMAND(MoveObject, MERGE,
		((ObjectID, id))
		((Position, pos))
		);

COMMAND(RotateObject, MERGE,
		((ObjectID, id))
		((bool, usetarget)) // true => use 'target' for orientation; false => use 'angle'
		((Position, target))
		((float, angle))
		);

COMMAND(DeleteObject, NOMERGE,
		((ObjectID, id))
		);

MESSAGE(SetSelectionPreview,
		((std::vector<ObjectID>, ids))
		);

QUERY(GetObjectSettings,
	  ((int, view)) // eRenderView
	  ((ObjectID, id))
	  ,
	  ((sObjectSettings, settings))
	  );

COMMAND(SetObjectSettings, NOMERGE,
		((int, view)) // eRenderView
		((ObjectID, id))
		((sObjectSettings, settings))
		);

//////////////////////////////////////////////////////////////////////////

QUERY(GetCinemaTracks,
	  , // no inputs
	  ((std::vector<AtlasMessage::sCinemaTrack> , tracks))
	  );

QUERY(GetCameraInfo,
	  ,
	  ((AtlasMessage::sCameraInfo, info))
	  );

COMMAND(SetCinemaTracks, NOMERGE,
		((std::vector<AtlasMessage::sCinemaTrack>, tracks))
		);

MESSAGE(CinemaEvent,
		((std::wstring, track))
		((int, mode))
		((float, t))
		((bool, drawAll))
		((bool, drawCurrent))
		((bool, lines))
		);

//////////////////////////////////////////////////////////////////////////

enum eTriggerListType
{
	CINEMA_LIST,
	TRIGGER_LIST,
	TRIG_GROUP_LIST	//list of trigger groups
	// [Eventually include things like entities and areas as the editor progresses...]
};


QUERY(GetTriggerData,
	  , //no inputs
	  ((std::vector<AtlasMessage::sTriggerGroup>, groups))
	  ((std::vector<AtlasMessage::sTriggerSpec>, conditions))
	  ((std::vector<AtlasMessage::sTriggerSpec>, effects))
	  );

QUERY(GetTriggerChoices,
	  ((std::wstring, name)),
	  ((std::vector<std::wstring>, choices))
	  );
		
COMMAND(SetAllTriggers, NOMERGE, 
	  ((std::vector<AtlasMessage::sTriggerGroup>, groups))
	  );


#ifndef MESSAGES_SKIP_SETUP
#include "MessagesSetup.h"
#endif

#endif // MESSAGES_H__
