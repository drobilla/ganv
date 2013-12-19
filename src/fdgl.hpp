/* This file is part of Ganv.
 * Copyright 2007-2013 David Robillard <http://drobilla.net>
 *
 * Ganv is free software: you can redistribute it and/or modify it under the
 * terms of the GNU General Public License as published by the Free Software
 * Foundation, either version 3 of the License, or any later version.
 *
 * Ganv is distributed in the hope that it will be useful, but WITHOUT ANY
 * WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 * FOR A PARTICULAR PURPOSE.  See the GNU General Public License for details.
 *
 * You should have received a copy of the GNU General Public License along
 * with Ganv.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <float.h>
#include <math.h>

static const double SPRING_K    = 16.0;
static const double CHARGE_KE   = 40000.0;
static const double AREA_WEIGHT = 0.5;

struct Region {
	Vector pos;
	Vector area;
};

inline Vector
vec_add(const Vector& a, const Vector& b)
{
	const Vector result = { a.x + b.x, a.y + b.y };
	return result;
}

inline Vector
vec_sub(const Vector& a, const Vector& b)
{
	const Vector result = { a.x - b.x, a.y - b.y };
	return result;
}

inline Vector
vec_mult(const Vector& a, double m)
{
	const Vector result = { a.x * m, a.y * m };
	return result;
}

inline double
vec_mult(const Vector& a, const Vector& b)
{
	return a.x * b.x + a.y * b.y;
}

/** Reciprocal of magnitude. */
inline double
vec_rmag(const Vector& vec)
{
	return 1.0 / sqrt(vec.x * vec.x + vec.y * vec.y);
}

/** Hooke's law */
inline Vector
spring_force(const Vector& a, const Vector& b, double length, double k)
{
	const Vector vec          = vec_sub(b, a);
	const double rmag         = vec_rmag(vec);
	const double displacement = length - (1.0 / rmag);
	return vec_mult(vec, rmag * k * displacement * 0.5);
}

/** Spring force with a directional force to align with flow direction. */
static const Vector
edge_force(const Vector& dir,
           const Vector& hpos,
           const Vector& tpos,
           double        length,
           double        k)
{
	return vec_add(dir, spring_force(hpos, tpos, length, k));
}

/** Modified Coulomb's law */
inline Vector
repel_force(const Region& a, const Region& b)
{
	const Vector vec      = vec_mult(vec_sub(a.pos, b.pos), 4.0);
	const double rmag     = vec_rmag(vec);
	const Vector a_weight = vec_mult(a.area, AREA_WEIGHT);
	const Vector b_weight = vec_mult(b.area, AREA_WEIGHT);
	const Vector force    = vec_mult(vec, rmag * rmag * rmag * CHARGE_KE * 0.5);
	return vec_mult(force, vec_mult(a_weight, b_weight));
}
