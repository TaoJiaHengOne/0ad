/**
 * =========================================================================
 * File        : RenderModifiers.h
 * Project     : Pyrogenesis
 * Description : RenderModifiers can affect the fragment stage behaviour
 *             : of some ModelRenderers. This file defines some common
 *             : RenderModifiers in addition to the base class.
 *
 * @author Nicolai Hähnle <nicolai@wildfiregames.com>
 * =========================================================================
 */

#ifndef RENDERMODIFIERS_H
#define RENDERMODIFIERS_H

#include "ModelRenderer.h"


class CLightEnv;
class CMatrix3D;
class CModel;
class CTexture;
class ShadowMap;

/**
 * Class RenderModifier: Some ModelRenderer implementations provide vertex
 * management behaviour but allow fragment stages to be modified by a plugged in
 * RenderModifier.
 *
 * You should use RenderModifierPtr when referencing RenderModifiers.
 */
class RenderModifier
{
public:
	RenderModifier() { }
	virtual ~RenderModifier() { }

	/**
	 * BeginPass: Setup OpenGL for the given rendering pass.
	 *
	 * Must be implemented by derived classes.
	 *
	 * @param pass The current pass number (pass == 0 is the first pass)
	 *
	 * @return The streamflags that indicate which vertex components
	 * are required by the fragment stages (see STREAM_XYZ constants).
	 */
	virtual u32 BeginPass(uint pass) = 0;

	/**
	 * GetTexGenMatrix: If BeginPass returns STREAM_TEXGENTOUVx, the caller must
	 * call this function to query the texture matrix that will be used to transform
	 * vertex positions into texture coordinates.
	 *
	 * Default implementation raises a runtime error.
	 *
	 * @param pass the current pass number (pass == 0 is the first pass)
	 *
	 * @return a pointer to the texture matrix for the given pass
	 */
	virtual const CMatrix3D* GetTexGenMatrix(uint pass);

	/**
	 * EndPass: Cleanup OpenGL state after the given pass. This function
	 * may indicate that additional passes are needed.
	 *
	 * Must be implemented by derived classes.
	 *
	 * @param pass The current pass number (pass == 0 is the first pass)
	 *
	 * @return true if rendering is complete, false if an additional pass
	 * is needed. If false is returned, BeginPass is then called with an
	 * increased pass number.
	 */
	virtual bool EndPass(uint pass) = 0;

	/**
	 * PrepareTexture: Called before rendering models that use the given
	 * texture.
	 *
	 * Must be implemented by derived classes.
	 *
	 * @param pass The current pass number (pass == 0 is the first pass)
	 * @param texture The texture used by subsequent models
	 */
	virtual void PrepareTexture(uint pass, CTexture* texture) = 0;

	/**
	 * PrepareModel: Called before rendering the given model.
	 *
	 * Default behaviour does nothing.
	 *
	 * @param pass The current pass number (pass == 0 is the first pass)
	 * @param model The model that is about to be rendered.
	 */
	virtual void PrepareModel(uint pass, CModel* model);
};


/**
 * Class LitRenderModifier: Abstract base class for RenderModifiers that apply
 * a shadow map.
 * LitRenderModifiers expect the diffuse brightness in the primary color (instead of ambient + diffuse).
 */
class LitRenderModifier : public RenderModifier
{
public:
	LitRenderModifier();
	~LitRenderModifier();

	/**
	 * SetShadowMap: Set the shadow map that will be used for rendering.
	 * Must be called by the user of the RenderModifier.
	 *
	 * The shadow map must be non-null and use depth texturing, or subsequent rendering
	 * using this RenderModifier will fail.
	 *
	 * @param shadow the shadow map
	 */
	void SetShadowMap(const ShadowMap* shadow);

	/**
	 * SetLightEnv: Set the light environment that will be used for rendering.
	 * Must be called by the user of the RenderModifier.
	 *
	 * @param lightenv the light environment (must be non-null)
	 */
	void SetLightEnv(const CLightEnv* lightenv);

	const ShadowMap* GetShadowMap() const { return m_Shadow; }
	const CLightEnv* GetLightEnv() const { return m_LightEnv; }

private:
	const ShadowMap* m_Shadow;
	const CLightEnv* m_LightEnv;
};


/**
 * Class RenderModifierRenderer: Interface to a model renderer that can render
 * its models via a RenderModifier that sets up fragment stages.
 */
class RenderModifierRenderer : public ModelRenderer
{
public:
	RenderModifierRenderer() { }
	virtual ~RenderModifierRenderer();
};


/**
 * Class PlainRenderModifier: RenderModifier that simply uses opaque textures
 * modulated by primary color. It is used for normal, no-frills models.
 */
class PlainRenderModifier : public RenderModifier
{
public:
	PlainRenderModifier();
	~PlainRenderModifier();

	// Implementation
	u32 BeginPass(uint pass);
	bool EndPass(uint pass);
	void PrepareTexture(uint pass, CTexture* texture);
};


/**
 * Class PlainLitRenderModifier: Use an opaque color texture for a lit object that is shadowed
 * using a depth texture.
 * Expects the diffuse brightness in the primary color.
 */
class PlainLitRenderModifier : public LitRenderModifier
{
public:
	PlainLitRenderModifier();
	~PlainLitRenderModifier();

	// Implementation
	u32 BeginPass(uint pass);
	const CMatrix3D* GetTexGenMatrix(uint pass);
	bool EndPass(uint pass);
	void PrepareTexture(uint pass, CTexture* texture);
};


/**
 * Class WireframeRenderModifier: RenderModifier that renders wireframe models.
 */
class WireframeRenderModifier : public RenderModifier
{
public:
	WireframeRenderModifier();
	~WireframeRenderModifier();

	// Implementation
	u32 BeginPass(uint pass);
	bool EndPass(uint pass);
	void PrepareTexture(uint pass, CTexture* texture);
	void PrepareModel(uint pass, CModel* model);
};


/**
 * Class SolidColorRenderModifier: Render all models using the same
 * solid color without lighting.
 *
 * You have to specify the color using a glColor*() calls before rendering.
 */
class SolidColorRenderModifier : public RenderModifier
{
public:
	SolidColorRenderModifier();
	~SolidColorRenderModifier();

	// Implementation
	u32 BeginPass(uint pass);
	bool EndPass(uint pass);
	void PrepareTexture(uint pass, CTexture* texture);
	void PrepareModel(uint pass, CModel* model);
};

#endif // RENDERMODIFIERS_H
