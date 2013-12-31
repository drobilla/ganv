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

static const double CHARGE_KE = 200000000.0;

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

/** Magnitude. */
inline double
vec_mag(const Vector& vec)
{
	return sqrt(vec.x * vec.x + vec.y * vec.y);
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
	const double mag          = vec_mag(vec);
	const double displacement = length - mag;
	return vec_mult(vec, k * displacement * 0.5 / mag);
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

/** Constant tide force, does not vary with distance. */
inline Vector
tide_force(const Vector& a, const Vector& b, double power)
{
	static const double G   = 0.0000000000667;
	const Vector        vec = vec_sub(a, b);
	const double        mag = vec_mag(vec);
	return vec_mult(vec, G * power / mag);
}

/**
   Repelling charge force.

   Many FDGL algorithms use charge according to Coulomb's law, but here we use
   an inverse cube (not squared) law so influence drops off more rapidly with
   distance.  This, in conjunction with a tide, keeps the layout compact.
*/
inline Vector
repel_force(const Region& a, const Region& b)
{
	const Vector vec   = vec_sub(a.pos, b.pos);
	const double mag   = vec_mag(vec);
	const Vector force = vec_mult(
		vec, (CHARGE_KE * 0.5 / (mag * mag * mag * mag * mag)));
	const Vector dforce = { force.x * (a.area.x * b.area.x),
	                        force.y * (a.area.y * b.area.y) };
	return dforce;
}
