/* Copyright (C) 2024 Wildfire Games.
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

#include "lib/self_test.h"

#include "maths/Matrix3D.h"
#include "maths/Quaternion.h"

#include <cmath>
#include <cstdlib>
#include <random>

class TestMatrix : public CxxTest::TestSuite
{
	std::mt19937 m_Engine;
	const float m_Epsilon{0.0001f};

public:
	void setUp()
	{
		m_Engine = std::mt19937(42);
	}

	void test_inverse()
	{
		CMatrix3D m;
		std::uniform_real_distribution<float> distribution01(0.0f, std::nextafter(1.0f, 2.0f));
		for (int i = 0; i < 4; ++i)
		{
			for (int j = 0; j < 16; ++j)
			{
				m._data[j] = -1.0f + 2.0f * distribution01(m_Engine);
			}
			CMatrix3D n;
			m.GetInverse(n);
			m.Concatenate(n);
			// verify identity has 1s on diagonal and 0 otherwise
			for (int x = 0; x < 4; ++x)
			{
				for (int y = 0; y < 4; ++y)
				{
					const float expected = (x==y)? 1.0f : 0.0f;
					TS_ASSERT_DELTA(m(x,y), expected, m_Epsilon);
				}
			}
		}
	}

	void test_compoundMultiplication()
	{
		const float invertibleData[16] = {
			2.f, -3.f, 0.f, 1.f,
			-2.f, 3.f, 0.f, 0.f,
			1.f, -2.f, 1.f, 0.f,
			1.f, -1.f, 0.f, 0.f
		};

		// Invertible matrix.
		CMatrix3D a(invertibleData);
		CMatrix3D n;
		a.GetInverse(n);
		a *= n;
		CMatrix3D a2(invertibleData);
		n *= a2;

		TS_ASSERT_MATRIX_EQUALS_DELTA(a, n, 16, m_Epsilon);

		// Non invertible matrix.
		const float nonInvertibleData[16] = {
			2.f, -3.f, 0.f, 1.f,
			-2.f, 3.f, 0.f, 0.f,
			1.f, -2.f, 1.f, 0.f,
			0.f, 0.f, 0.f, 0.f
		};

		CMatrix3D b(nonInvertibleData);
		b.GetInverse(n);
		b *= n;
		CMatrix3D b2(nonInvertibleData);
		n *= b2;

		TS_ASSERT_MATRIX_DIFFERS_DELTA(b, n, 16, m_Epsilon);
	}

	void test_multiplication()
	{
		const float data1[16] = {
			2.f, -3.f, 0.f, 1.f,
			-2.f, 3.f, 0.f, 0.f,
			1.f, -2.f, 1.f, 0.f,
			1.f, -1.f, 0.f, 0.f
		};

		const float data2[16] = {
			22.f, -3.f, 0.f, 1.f,
			-2.f, 3.f, 8.f, 12.f,
			1.f, -2.f, 1.f, 0.f,
			1.f, -1.f, 16.f, 0.f
		};

		CMatrix3D mat1(data1);
		CMatrix3D mat2(data2);

		CMatrix3D mat3 = mat2 * mat1;

		const float result[16] = {
			51.f, -16.f, -8.f, -34.f,
			-50.f, 15.f, 24.f, 34.f,
			27.f, -11.f, -15.f, -23.f,
			24.f, -6.f, -8.f, -11.f
		};

		CMatrix3D resultMat3(result);

		TS_ASSERT_MATRIX_EQUALS_DELTA(mat3, resultMat3, 16, m_Epsilon);

		const float result2[16] = {
			51.f, -76.f, 0.f, 22.f,
			10.f, -13.f, 8.f, -2.f,
			7.f, -11.f, 1.f, 1.f,
			20.f, -38.f, 16.f, 1.f
		};

		CMatrix3D resultMat4(result2);

		CMatrix3D mat4 = mat1 * mat2;

		TS_ASSERT_MATRIX_EQUALS_DELTA(mat4, resultMat4, 16, m_Epsilon);

		mat1.Concatenate(mat2);

		TS_ASSERT_MATRIX_EQUALS_DELTA(mat1, resultMat3, 16, m_Epsilon);

		mat1 = CMatrix3D(data1);

		mat2.Concatenate(mat1);

		TS_ASSERT_MATRIX_EQUALS_DELTA(mat2, resultMat4, 16, m_Epsilon);
	}

	void test_nonCommutative()
	{
		const float data1[16] = {
			1.f, 1.f, 1.f, 1.f,
			0.f, 0.f, 0.f, 0.f,
			0.f, 0.f, 0.f, 0.f,
			0.f, 0.f, 0.f, 0.f
		};

		const float data2[16] = {
			1.f, 0.f, 0.f, 0.f,
			1.f, 0.f, 0.f, 0.f,
			1.f, 0.f, 0.f, 0.f,
			1.f, 0.f, 0.f, 0.f
		};

		CMatrix3D mat1(data1);
		CMatrix3D mat2(data2);

		mat1 *= mat2;

		const float result[16] = {
			1.f, 1.f, 1.f, 1.f,
			1.f, 1.f, 1.f, 1.f,
			1.f, 1.f, 1.f, 1.f,
			1.f, 1.f, 1.f, 1.f
		};

		CMatrix3D resultMat3(result);

		TS_ASSERT_MATRIX_EQUALS_DELTA(mat1, resultMat3, 16, m_Epsilon);

		const float result2[16] = {
			4.f, 0.f, 0.f, 0.f,
			0.f, 0.f, 0.f, 0.f,
			0.f, 0.f, 0.f, 0.f,
			0.f, 0.f, 0.f, 0.f
		};

		CMatrix3D mat3(data1);
		CMatrix3D resultMat4(result2);
		mat2 *= mat3;

		TS_ASSERT_MATRIX_EQUALS_DELTA(mat2, resultMat4, 16, m_Epsilon);
	}

	void test_quats()
	{
		std::uniform_real_distribution<float> distribution01(0.0f, std::nextafter(1.0f, 2.0f));
		for (int i = 0; i < 4; ++i)
		{
			CQuaternion q;
			q.FromEulerAngles(
				-6.28f + 12.56f * distribution01(m_Engine),
				-6.28f + 12.56f * distribution01(m_Engine),
				-6.28f + 12.56f * distribution01(m_Engine)
				);
			CMatrix3D m;
			q.ToMatrix(m);
			CQuaternion q2 = m.GetRotation();

			// Quaternions (x,y,z,w) and (-x,-y,-z,-w) are equivalent when
			// interpreted as rotations, so it doesn't matter which we get
			const bool ok_oneway =
				feq(q2.m_W, q.m_W) &&
				feq(q2.m_V.X, q.m_V.X) &&
				feq(q2.m_V.Y, q.m_V.Y) &&
				feq(q2.m_V.Z, q.m_V.Z);
			const bool ok_otherway =
				feq(q2.m_W, -q.m_W) &&
				feq(q2.m_V.X, -q.m_V.X) &&
				feq(q2.m_V.Y, -q.m_V.Y) &&
				feq(q2.m_V.Z, -q.m_V.Z);
			TS_ASSERT(ok_oneway ^ ok_otherway);
		}
	}

	void test_rotate()
	{
		std::uniform_real_distribution<float> distribution01(0.0f, std::nextafter(1.0f, 2.0f));

		CMatrix3D m;
		for (int j = 0; j < 16; ++j)
			m._data[j] = -1.0f + 2.0f * distribution01(m_Engine);

		CMatrix3D r, a, b;

		a = m;
		b = m;
		a.RotateX(1.0f);
		r.SetXRotation(1.0f);
		b.Concatenate(r);

		for (int x = 0; x < 4; ++x)
			for (int y = 0; y < 4; ++y)
				TS_ASSERT_DELTA(a(x,y), b(x,y), m_Epsilon);

		a = m;
		b = m;
		a.RotateY(1.0f);
		r.SetYRotation(1.0f);
		b.Concatenate(r);

		for (int x = 0; x < 4; ++x)
			for (int y = 0; y < 4; ++y)
				TS_ASSERT_DELTA(a(x,y), b(x,y), m_Epsilon);

		a = m;
		b = m;
		a.RotateZ(1.0f);
		r.SetZRotation(1.0f);
		b.Concatenate(r);

		for (int x = 0; x < 4; ++x)
			for (int y = 0; y < 4; ++y)
				TS_ASSERT_DELTA(a(x,y), b(x,y), m_Epsilon);
	}

	void test_getRotation()
	{
		std::uniform_real_distribution<float> distribution01(0.0f, std::nextafter(1.0f, 2.0f));

		CMatrix3D m;

		m.SetZero();
		TS_ASSERT_EQUALS(m.GetYRotation(), 0.f);

		m.SetIdentity();
		TS_ASSERT_EQUALS(m.GetYRotation(), 0.f);

		for (int j = 0; j < 16; ++j)
		{
			float a = 2 * M_PI * distribution01(m_Engine) - M_PI;
			m.SetYRotation(a);
			TS_ASSERT_DELTA(m.GetYRotation(), a, m_Epsilon);
		}
	}

	void test_scale()
	{
		std::uniform_real_distribution<float> distribution01(0.0f, std::nextafter(1.0f, 2.0f));

		CMatrix3D m;

		for (int j = 0; j < 16; ++j)
			m._data[j] = -1.0f + 2.0f * distribution01(m_Engine);

		CMatrix3D s, a, b;

		a = m;
		b = m;
		a.Scale(0.5f, 2.0f, 3.0f);
		s.SetScaling(0.5f, 2.0f, 3.0f);
		b.Concatenate(s);

		for (int x = 0; x < 4; ++x)
			for (int y = 0; y < 4; ++y)
				TS_ASSERT_DELTA(a(x,y), b(x,y), m_Epsilon);
	}
};
