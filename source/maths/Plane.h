/**
 * =========================================================================
 * File        : Plane.h
 * Project     : 0 A.D.
 * Description : A Plane in R3 and several utility methods.
 * =========================================================================
 */

// Note that the format used for the plane equation is
// Ax + By + Cz + D = 0, where <A,B,C> is the normal vector.

#ifndef INCLUDED_PLANE
#define INCLUDED_PLANE

#include "Vector3D.h"

enum PLANESIDE
{
	PS_FRONT,
	PS_BACK,
	PS_ON
};

class CPlane
{
	public:
		CPlane ();

		//sets the plane equation from 3 points on that plane
		void Set (const CVector3D &p1, const CVector3D &p2, const CVector3D &p3);

		//sets the plane equation from a normal and a point on 
		//that plane
		void Set (const CVector3D &norm, const CVector3D &point);

		//normalizes the plane equation
		void Normalize ();

		//returns the side of the plane on which this point
		//lies.
		PLANESIDE ClassifyPoint (const CVector3D &point) const;

		//solves the plane equation for a particular point
		float DistanceToPlane (const CVector3D &point) const;

		//calculates the intersection point of a line with this
		//plane. Returns false if there is no intersection
		bool FindLineSegIntersection (const CVector3D &start, const CVector3D &end, CVector3D *intsect);
		bool FindRayIntersection (CVector3D &start, CVector3D &direction, CVector3D *intsect);

	public:
		CVector3D m_Norm;	//normal vector of the plane
		float m_Dist;		//Plane distance (ie D in the plane eq.)
};

#endif
