/*==================================================================
| 
| Name: Sprite.cpp
|
|===================================================================
|
| Author: Ben Vinegar
| Contact: benvinegar () hotmail ! com
|
|
| Last Modified: 03/08/04
|
| Overview: Billboarding sprite class - always faces the camera. It
|           does this by getting the current model view matrix state.
|
|
| Usage: The functions speak for themselves. Instantiate, then be
|        sure to pass a loaded (using tex_load()) texture before 
|        calling Render().
|
| To do: TBA
|
| More Information: TBA
|
==================================================================*/

#include "Sprite.h"
#include "ogl.h"
#include "tex.h"

CSprite::CSprite() :
	m_texture(NULL) 
{

	// default scale 1:1
	m_scale.X = m_scale.Y = m_scale.Z = 1.0f;
	
	// default position (0.0f, 0.0f, 0.0f)
	m_translation.X = m_translation.Y = m_translation.Z = 0.0f;

	// default size 1.0 x 1.0
	SetSize(1.0f, 1.0f);

	// default colour, white
	m_colour[0] = m_colour[1] = m_colour[2] = m_colour[3] = 1.0f;
}

CSprite::~CSprite() 
{
}

void CSprite::Render() 
{	
	BeginBillboard();

	glDisable(GL_CULL_FACE);

	glTranslatef(m_translation.X, m_translation.Y, m_translation.Z);
	glScalef(m_scale.X, m_scale.Y, m_scale.Z);

	tex_bind(m_texture->GetHandle());

	glColor4fv(m_colour);

	glBegin(GL_TRIANGLE_STRIP);
		// bottom left
		glTexCoord2f(0.0f, 0.0f);
		glVertex3fv((GLfloat *) &m_coords[0]);
	
		// top left
		glTexCoord2f(0.0f, 1.0f);
		glVertex3fv((GLfloat *) &m_coords[1]);

		// bottom right
		glTexCoord2f(1.0f, 0.0f);
		glVertex3fv((GLfloat *) &m_coords[2]);

		// top left
		glTexCoord2f(1.0f, 1.0f);
		glVertex3fv((GLfloat *) &m_coords[3]);
	glEnd();

	glEnable(GL_CULL_FACE);

	EndBillboard();
}

int CSprite::SetTexture(CTexture *texture) 
{
	if (texture == NULL) return -1;
	
	m_texture = texture;
	return 0;
}

void CSprite::SetSize(float width, float height) 
{
	m_width = width;
	m_height = height;

	float xOffset = m_width / 2;
	float yOffset = m_height / 2;

	// bottom left
	m_coords[0].X = - (xOffset);
	m_coords[0].Y = - (yOffset);
	m_coords[0].Z = 0.0f;

	// top left
	m_coords[1].X = - (xOffset);
	m_coords[1].Y = yOffset;
	m_coords[1].Z = 0.0f;

	// bottom right
	m_coords[2].X = xOffset;
	m_coords[2].Y = - (yOffset);
	m_coords[2].Z = 0.0f;

	// top right
	m_coords[3].X = xOffset;
	m_coords[3].Y = yOffset;
	m_coords[3].Z = 0.0f;
}

float CSprite::GetWidth() 
{
	return m_width;
}

void CSprite::SetWidth(float width) 
{
	SetSize(width, m_height);	
}

float CSprite::GetHeight() 
{
	return m_height;
}

void CSprite::SetHeight(float height) 
{
	SetSize(m_width, height);
}

CVector3D CSprite::GetTranslation() 
{
	return m_translation;
}

void CSprite::SetTranslation(CVector3D trans) 
{
	m_translation = trans;
}

void CSprite::SetTranslation(float x, float y, float z) 
{
	m_translation.X = x;
	m_translation.Y = y;
	m_translation.Z = z;
}

CVector3D CSprite::GetScale() 
{
	return m_scale;
}

void CSprite::SetScale(CVector3D scale) 
{
	m_scale = scale;
}

void CSprite::SetScale(float x, float y, float z) 
{
	m_scale.X = x;
	m_scale.Y = y;
	m_scale.Z = z;
}

void CSprite::SetColour(float * colour) 
{
	m_colour[0] = colour[0];
	m_colour[1] = colour[1];
	m_colour[2] = colour[2];
	m_colour[3] = colour[3];
}

// should be called before any other gl calls
void CSprite::BeginBillboard() 
{
	float newMatrix[16] = { 1.0f, 0.0f, 0.0f, 0.0f, 
							0.0f, 1.0f, 0.0f, 0.0f, 
							0.0f, 0.0f, 1.0f, 0.0f, 
							0.0f, 0.0f, 0.0f, 1.0f };
    float currentMatrix[16];

	glGetFloatv(GL_MODELVIEW_MATRIX, currentMatrix);
	newMatrix[0] = currentMatrix[0];
	newMatrix[1] = currentMatrix[4];
	newMatrix[2] = currentMatrix[8];
	newMatrix[4] = currentMatrix[1];
	newMatrix[5] = currentMatrix[5];
	newMatrix[6] = currentMatrix[9];
	newMatrix[8] = currentMatrix[2];
	newMatrix[9] = currentMatrix[6];
	newMatrix[10] = currentMatrix[10];

	glPushMatrix();
	glMultMatrixf(newMatrix);
}

void CSprite::EndBillboard() 
{
	glPopMatrix();
}
