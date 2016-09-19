/* This file is part of Ganv.
 * Copyright 2007-2015 David Robillard <http://drobilla.net>
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

static const double CHARGE_KE = 4000000.0;
static const double EDGE_K    = 16.0;
static const double EDGE_LEN  = 0.1;

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
edge_force(const Vector& dir, const Vector& hpos, const Vector& tpos)
{
	return vec_add(dir, spring_force(hpos, tpos, EDGE_LEN, EDGE_K));
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

inline double
rect_distance(Vector* vec,
              const double ax1, const double ay1,
              const double ax2, const double ay2,
              const double bx1, const double by1,
              const double bx2, const double by2)
{
	vec->x = 0.0;
	vec->y = 0.0;

	if (ax2 <= bx1) {  // A is completely to the left of B
		vec->x = ax2 - bx1;
		if (ay2 <= by1) {  // Top Left
			const double dx = bx1 - ax2;
			const double dy = by1 - ay2;
			vec->y = ay2 - by1;
			return sqrt(dx * dx + dy * dy);
		} else if (ay1 >= by2) {  // Bottom left
			const double dx = bx1 - ax2;
			const double dy = ay1 - by2;
			vec->y = ay1 - by2;
			return sqrt(dx * dx + dy * dy);
		} else {  // Left
			return bx1 - ax2;
		}
	} else if (ax1 >= bx2) {  //  A is completely to the right of B
		vec->x = ax1 - bx2;
		if (ay2 <= by1) {  // Top right
			const double dx = ax1 - bx2;
			const double dy = by1 - ay2;
			vec->y = ay2 - by1;
			return sqrt(dx * dx + dy * dy);
		} else if (ay1 >= by2) {  // Bottom right
			const double dx = ax1 - bx2;
			const double dy = ay1 - by2;
			vec->y = ay1 - by2;
			return sqrt(dx * dx + dy * dy);
		} else {  // Right
			return ax1 - bx2;
		}
	} else if (ay2 <= by1) {  // Top
		vec->y = ay2 - by1;
		return by1 - ay2;
	} else if (ay1 >= by2) {  // Bottom
		vec->y = ay1 - by2;
		return ay1 - by2;
	} else {  // Overlap
		return 0.0;
	}
}

/** Repelling charge force, ala Coulomb's law. */
inline Vector
repel_force(const Region& a, const Region& b)
{
	static const double MIN_DIST = 1.0;

	Vector vec;
	double dist = rect_distance(
		&vec,
		a.pos.x - (a.area.x / 2.0), a.pos.y - (a.area.y / 2.0),
		a.pos.x + (a.area.x / 2.0), a.pos.y + (a.area.y / 2.0),
		b.pos.x - (b.area.x / 2.0), b.pos.y - (b.area.y / 2.0),
		b.pos.x + (b.area.x / 2.0), b.pos.y + (b.area.y / 2.0));

	if (dist <= MIN_DIST) {
		dist = MIN_DIST;
		vec  = vec_sub(a.pos, b.pos);
	}
	return vec_mult(vec, (CHARGE_KE * 0.5 / (vec_mag(vec) * dist * dist)));
}
