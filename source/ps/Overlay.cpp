/*
COverlay
by Rich Cross
rich@0ad.wildfiregames.com
*/

#include "precompiled.h"

#include "Overlay.h"

COverlay::COverlay()
	: m_Rect(CRect(0.f,0.f,0.f,0.f)), m_Z(0), m_Color(CColor(0,0,0,0)), m_Texture(""), m_HasBorder(false), m_BorderColor(CColor(0,0,0,0))
{
}


COverlay::COverlay(const CRect& rect,int z,const CColor& color,const char* texturename,
				   bool hasBorder,const CColor& bordercolor)
	: m_Rect(rect), m_Z(z), m_Color(color), m_Texture(texturename), m_HasBorder(hasBorder), m_BorderColor(bordercolor)
{
}

COverlay::~COverlay()
{
}

/*************************************************************************/

CRect::CRect() : 
	left(0.f), top(0.f), right(0.f), bottom(0.f) 
{
}

CRect::CRect(const CPos &pos) :
	left(pos.x), top(pos.y), right(pos.x), bottom(pos.y)
{
}

CRect::CRect(const CSize &size) :
	left(0.f), top(0.f), right(size.cx), bottom(size.cy)
{
}

CRect::CRect(const CPos &upperleft, const CPos &bottomright) :
	left(upperleft.x), top(upperleft.y), right(bottomright.x), bottom(bottomright.y)
{
}

CRect::CRect(const CPos &pos, const CSize &size) :
	left(pos.x), top(pos.y), right((pos.x+size.cx)), bottom((pos.y+size.cy))
{
}

CRect::CRect(const float &_l, const float &_t, const float &_r, const float &_b) : 
	left(_l), top(_t), right(_r), bottom(_b) 
{
}

// =
void CRect::operator = (const CRect& a)
{
	left = a.left;
	top = a.top;
	right = a.right;
	bottom = a.bottom;
}

// ==
bool CRect::operator ==(const CRect &a) const
{
	return	(left==a.left &&
			 top==a.top &&
			 right==a.right &&
			 bottom==a.bottom);
}

// !=
bool CRect::operator != (const CRect& a) const
{
	return !(*this==a);
}

// - (the unary operator)
CRect CRect::operator - (void) const
{
	return CRect(-left, -top, -right, -bottom);
}

// + (the unary operator)
CRect CRect::operator + (void) const
{
	return *this;
}

// +
CRect CRect::operator + (const CRect& a) const
{
	return CRect(left+a.left, top+a.top, right+a.right, bottom+a.bottom);
}

// +
CRect CRect::operator + (const CPos& a) const
{
	return CRect(left+a.x, top+a.y, right+a.x, bottom+a.y);
}

// +
CRect CRect::operator + (const CSize& a) const
{
	return CRect(left+a.cx, top+a.cy, right+a.cx, bottom+a.cy);
}

// -
CRect CRect::operator - (const CRect& a) const
{ 
	return CRect(left-a.left, top-a.top, right-a.right, bottom-a.bottom);
}

// -
CRect CRect::operator - (const CPos& a) const
{ 
	return CRect(left-a.x, top-a.y, right-a.x, bottom-a.y);
}

// -
CRect CRect::operator - (const CSize& a) const
{ 
	return CRect(left-a.cx, top-a.cy, right-a.cx, bottom-a.cy);
}

// +=
void CRect::operator +=(const CRect& a)
{
	left += a.left;
	top += a.top;
	right += a.right;
	bottom += a.bottom;
}

// +=
void CRect::operator +=(const CPos& a)
{
	left += a.x;
	top += a.y;
	right += a.x;
	bottom += a.y;
}

// +=
void CRect::operator +=(const CSize& a)
{
	left += a.cx;
	top += a.cy;
	right += a.cx;
	bottom += a.cy;
}

// -=
void CRect::operator -=(const CRect& a)
{
	left -= a.left;
	top -= a.top;
	right -= a.right;
	bottom -= a.bottom;
}

// -=
void CRect::operator -=(const CPos& a)
{
	left -= a.x;
	top -= a.y;
	right -= a.x;
	bottom -= a.y;
}

// -=
void CRect::operator -=(const CSize& a)
{
	left -= a.cx;
	top -= a.cy;
	right -= a.cx;
	bottom -= a.cy;
}

float CRect::GetWidth() const 
{
	return right-left;
}

float CRect::GetHeight() const
{
	return bottom-top;
}

CSize CRect::GetSize() const
{
	return CSize(right-left, bottom-top);
}

CPos CRect::TopLeft() const
{
	return CPos(left, top);
}

CPos CRect::BottomRight() const
{
	return CPos(right, right);
}

CPos CRect::CenterPoint() const
{
	return CPos((left+right)/2.f, (top+bottom/2.f));
}

bool CRect::PointInside(const CPos &point) const
{
	return (point.x >= left &&
			point.x <= right &&
			point.y >= top &&
			point.y <= bottom);
}

/*************************************************************************/

CPos::CPos() : x(0.f), y(0.f)
{
}

CPos::CPos(const float &_x, const float &_y) : x(_x), y(_y)
{
}

// =
void CPos::operator = (const CPos& a)
{
	x = a.x;
	y = a.y;
}

// ==
bool CPos::operator ==(const CPos &a) const
{
	return	(x==a.x && y==a.y);
}

// !=
bool CPos::operator != (const CPos& a) const
{
	return !(*this==a);
}

// - (the unary operator)
CPos CPos::operator - (void) const
{
	return CPos(-x, -y);
}

// + (the unary operator)
CPos CPos::operator + (void) const
{
	return *this;
}

// +
CPos CPos::operator + (const CPos& a) const
{
	return CPos(x+a.x, y+a.y);
}

// +
CPos CPos::operator + (const CSize& a) const
{
	return CPos(x+a.cx, y+a.cy);
}

// -
CPos CPos::operator - (const CPos& a) const
{ 
	return CPos(x-a.x, y-a.y);
}

// -
CPos CPos::operator - (const CSize& a) const
{ 
	return CPos(x-a.cx, y-a.cy);
}

// +=
void CPos::operator +=(const CPos& a)
{
	x += a.x;
	y += a.y;
}

// +=
void CPos::operator +=(const CSize& a)
{
	x += a.cx;
	y += a.cy;
}

// -=
void CPos::operator -=(const CPos& a)
{
	x -= a.x;
	y -= a.y;
}

// -=
void CPos::operator -=(const CSize& a)
{
	x -= a.cx;
	y -= a.cy;
}

/*************************************************************************/

CSize::CSize() : cx(0.f), cy(0.f) 
{
}

CSize::CSize(const CRect &rect) : cx(rect.GetWidth()), cy(rect.GetHeight()) 
{
}

CSize::CSize(const CPos &pos) : cx(pos.x), cy(pos.y) 
{
}

CSize::CSize(const float &_cx, const float &_cy) : cx(_cx), cy(_cy)
{
}

// =
void CSize::operator = (const CSize& a)
{
	cx = a.cx;
	cy = a.cy;
}

// ==
bool CSize::operator ==(const CSize &a) const
{
	return	(cx==a.cx && cy==a.cy);
}

// !=
bool CSize::operator != (const CSize& a) const
{
	return !(*this==a);
}

// - (the unary operator)
CSize CSize::operator - (void) const
{
	return CSize(-cx, -cy);
}

// + (the unary operator)
CSize CSize::operator + (void) const
{
	return *this;
}

// +
CSize CSize::operator + (const CSize& a) const
{
	return CSize(cx+a.cx, cy+a.cy);
}

// -
CSize CSize::operator - (const CSize& a) const
{ 
	return CSize(cx-a.cx, cy-a.cy);
}

// /
CSize CSize::operator / (const float& a) const
{
	return CSize(cx/a, cy/a);
}

// *
CSize CSize::operator * (const float& a) const
{ 
	return CSize(cx*a, cy*a);
}

// +=
void CSize::operator +=(const CSize& a)
{
	cx += a.cx;
	cy += a.cy;
}

// -=
void CSize::operator -=(const CSize& a)
{
	cx -= a.cx;
	cy -= a.cy;
}

// /=
void CSize::operator /=(const float& a)
{
	cx /= a;
	cy /= a;
}

// *=
void CSize::operator *=(const float& a)
{
	cx *= a;
	cy *= a;
}
