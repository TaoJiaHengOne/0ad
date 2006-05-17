/**
 * =========================================================================
 * File        : SkyManager.h
 * Project     : Pyrogenesis
 * Description : Sky settings and texture management
 *
 * @author Matei Zaharia <matei@wildfiregames.com>
 * =========================================================================
 */

#ifndef SKYMANAGER_H
#define SKYMANAGER_H

#include "ps/Overlay.h"

/**
 * Class SkyManager: Maintain sky settings and textures, and render the sky.
 */
class SkyManager
{
public:
	bool m_RenderSky;

public:
	SkyManager();
	~SkyManager();

	/**
	 * LoadSkyTextures: Load sky textures from within the
	 * progressive load framework.
	 *
	 * @return 0 if loading has completed, a value from 1 to 100 (in percent of completion)
	 * if more textures need to be loaded and a negative error value on failure.
	 */
	int LoadSkyTextures();

	/**
	 * UnloadSkyTextures: Free any loaded sky textures and reset the internal state
	 * so that another call to LoadSkyTextures will begin progressive loading.
	 */
	void UnloadSkyTextures();

	/**
	 * RenderSky: Render the sky.
	 */
	void RenderSky();

	/**
	 * GetSkySet(): Return the currently selected sky set name
	 */
	inline CStrW GetSkySet() {
		return m_SkySet;
	}

	/**
	 * GetSkySet(): Set the sky set name, potentially loading the textures
	 */
	void SetSkySet(CStrW name);

private:
	/// Name of current skyset (a directory within art/textures/skies)
	CStrW m_SkySet;

	// Sky textures
	Handle m_SkyTexture[5];

	// Indices into m_SkyTexture
	static const int IMG_FRONT = 0;
	static const int IMG_BACK = 1;
	static const int IMG_RIGHT = 2;
	static const int IMG_LEFT = 3;
	static const int IMG_TOP = 4;

	// Array of image names (defined in SkyManager.cpp), in the order of the IMG_ id's
	static const char* IMAGE_NAMES[5];

	/// State of progressive loading (in # of loaded textures)
	uint cur_loading_tex;
};


#endif // SKYMANAGER_H
