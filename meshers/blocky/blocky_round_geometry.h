#ifndef BLOCKY_ROUND_GEOMETRY_H
#define BLOCKY_ROUND_GEOMETRY_H

// Material-independent rounded unit-cube geometry. All axes use the same policy.
// Shape data does not consume voxel IDs, and never changes the input voxel buffer.
#include "blocky_round_geometry_data.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <utility>

namespace zylann::voxel::blocky::round_geometry {
using P = StdVector<V>;
constexpr double PI = 3.14159265358979323846;
struct Edge {
	int a, b, along, sa, sb;
};
// Four sign combinations for each pair of axes: XY, XZ, YZ.
inline Edge edge(unsigned int i) {
	const int pair = i / 4;
	const int a = pair == 2 ? 1 : 0;
	const int b = pair == 0 ? 1 : 2;
	return { a, b, 3 - a - b, (i & 1) ? 1 : -1, (i & 2) ? 1 : -1 };
}
inline int side(int axis, int sign) {
	return 2 * axis + (sign > 0 ? 1 : 0);
}

// Kind: 0 = empty space/fluid, 1 = supported enabled cube, 2 = another model.
// Only a one-voxel halo is read. A blocked continuation tapers to an exact square
// at the contact plane; this closes concave and mixed rounded/square joins.
template <typename ReadKind>
uint64_t shape_key(ReadKind read) {
	uint64_t key = 0;
	std::array<int, 6> neighbors{};
	for (int axis = 0; axis < 3; ++axis) {
		for (int sign : { -1, 1 }) {
			std::array<int, 3> offset{};
			offset[axis] = sign;
			neighbors[side(axis, sign)] = read(offset[0], offset[1], offset[2]);
		}
	}
	for (unsigned int i = 0; i < 12; ++i) {
		const Edge e = edge(i);
		if (neighbors[side(e.a, e.sa)] != 0 || neighbors[side(e.b, e.sb)] != 0) {
			continue;
		}
		key |= uint64_t(1) << i;
		for (unsigned int end = 0; end < 2; ++end) {
			std::array<int, 3> t{};
			t[e.along] = end == 0 ? -1 : 1;
			const int neighbor = neighbors[side(e.along, end == 0 ? -1 : 1)];
			if (neighbor == 0) {
				continue;
			}
			bool protect = neighbor != 1;
			t[e.a] = e.sa;
			protect |= read(t[0], t[1], t[2]) != 0;
			t[e.a] = 0;
			t[e.b] = e.sb;
			protect |= read(t[0], t[1], t[2]) != 0;
			if (protect) {
				key |= uint64_t(1) << (12 + 2 * i + end);
			}
		}
	}
	return key;
}
inline V add(V a, V b) {
	for (int i = 0; i < 3; ++i)
		a[i] += b[i];
	return a;
}
inline V sub(V a, V b) {
	for (int i = 0; i < 3; ++i)
		a[i] -= b[i];
	return a;
}
inline V mul(V a, double b) {
	for (double &v : a)
		v *= b;
	return a;
}
inline double dot(V a, V b) {
	return a[0] * b[0] + a[1] * b[1] + a[2] * b[2];
}
inline V cross(V a, V b) {
	return { a[1] * b[2] - a[2] * b[1], a[2] * b[0] - a[0] * b[2], a[0] * b[1] - a[1] * b[0] };
}
inline V norm(V a) {
	double d = std::sqrt(dot(a, a));
	return d > 0 ? mul(a, 1 / d) : V{ 0, 1, 0 };
}
inline V key(V p) {
	for (double &v : p)
		v = std::round(v * 1e9) / 1e9;
	return p;
}
inline P clean(const P &p) {
	P o;
	for (V v : p)
		if (o.empty() || key(v) != key(o.back()))
			o.push_back(v);
	if (o.size() > 1 && key(o.front()) == key(o.back()))
		o.pop_back();
	return o;
}
inline std::pair<V, V> basis(V n) {
	V u = norm(cross(std::abs(n[1]) < .8 ? V{ 0, 1, 0 } : V{ 1, 0, 0 }, n));
	return { u, cross(n, u) };
}
inline V midpoint(const P &p) {
	V c{};
	for (V v : p)
		c = add(c, v);
	return mul(c, 1. / p.size());
}
inline P sorted_plane(const P &points, V n) {
	P p;
	StdMap<V, size_t> seen;
	for (V v : points) {
		auto r = seen.emplace(key(v), p.size());
		if (r.second)
			p.push_back(v);
		else
			p[r.first->second] = v;
	}
	V c = midpoint(p);
	auto [u, v] = basis(n);
	std::stable_sort(p.begin(), p.end(), [&](V a, V b) {
		return std::atan2(dot(sub(a, c), v), dot(sub(a, c), u)) < std::atan2(dot(sub(b, c), v), dot(sub(b, c), u));
	});
	return p;
}
struct Face {
	V n{};
	double d = 0;
	int edge_index = -1;
	int source_face = 0;
	V center{};
	P p;
};
inline StdVector<Face> planes(uint64_t shape, const Settings &s) {
	StdVector<Face> cuts;
	for (unsigned int i = 0; i < 12; ++i) {
		if ((shape & (uint64_t(1) << i)) == 0) {
			continue;
		}
		const Edge e = edge(i);
		V c{ .5, .5, .5 };
		c[e.a] = e.sa < 0 ? s.radius : 1 - s.radius;
		c[e.b] = e.sb < 0 ? s.radius : 1 - s.radius;
		for (int segment = 0; segment < 2; ++segment) {
			const double angle = (segment + .5) * PI / 4;
			V n{};
			n[e.a] = e.sa * std::cos(angle);
			n[e.b] = e.sb * std::sin(angle);
			const int f = segment < 1 ? side(e.a, e.sa) : side(e.b, e.sb);
			cuts.push_back({ n, dot(n, c) + s.radius * std::cos(PI / 8), int(i), f, c, {} });
		}
	}
	return cuts;
}
inline StdVector<Face> base_faces(const StdVector<Face> &cuts) {
	StdVector<Face> faces;
	for (int f = 0; f < 6; ++f) {
		const int axis = f / 2, sign = (f & 1) ? 1 : -1;
		V n{};
		n[axis] = sign;
		const double value = sign < 0 ? 0 : 1;
		P p;
		for (int a = 0; a < 2; ++a) {
			for (int b = 0; b < 2; ++b) {
				V q{};
				q[axis] = value;
				q[(axis + 1) % 3] = a;
				q[(axis + 2) % 3] = b;
				p.push_back(q);
			}
		}
		faces.push_back({ n, value * sign, -1, f, {}, sorted_plane(p, n) });
	}
	for (const Face &plane : cuts) {
		StdVector<Face> next;
		P intersections;
		for (Face f : faces) {
			P out;
			for (size_t j = 0; j < f.p.size(); ++j) {
				V p = f.p[j], q = f.p[(j + 1) % f.p.size()];
				double dp = dot(plane.n, p) - plane.d, dq = dot(plane.n, q) - plane.d;
				bool pin = dp <= 1e-10, qin = dq <= 1e-10;
				if (pin)
					out.push_back(p);
				if (pin != qin) {
					V hit = add(p, mul(sub(q, p), dp / (dp - dq)));
					out.push_back(hit);
					intersections.push_back(hit);
				}
			}
			f.p = clean(out);
			if (f.p.size() >= 3)
				next.push_back(f);
		}
		StdMap<V, bool> unique;
		for (V p : intersections)
			unique[key(p)] = true;
		if (unique.size() >= 3) {
			Face f = plane;
			f.p = sorted_plane(intersections, plane.n);
			next.push_back(f);
		}
		faces = std::move(next);
	}
	return faces;
}

struct Factor {
	double value = 1, derivative = 0;
};
inline Factor profile(double t, unsigned int mode) {
	if (t >= 1) {
		return {};
	}
	if (t < 0) {
		t = 0;
	}
	switch (mode) {
		case 1:
			return { t * t, 2 * t };
		case 2:
			return { t * (2 - t), 2 - 2 * t };
		case 3:
			return { t * t * (3 - 2 * t), 6 * t * (1 - t) };
		default:
			return { t, 1 };
	}
}
inline Factor factor(uint64_t shape, unsigned int i, double position, const Settings &s) {
	Factor out;
	// Product makes overlapping endpoint tapers symmetric and continuous, even
	// when each length exceeds half the edge. No overwrite or central cusp.
	for (unsigned int end = 0; end < 2; ++end) {
		if ((shape & (uint64_t(1) << (12 + 2 * i + end))) == 0) {
			continue;
		}
		Factor f = profile((end == 0 ? position : 1 - position) / s.taper_distance, s.profile);
		f.derivative *= (end == 0 ? 1 : -1) / s.taper_distance;
		out.derivative = out.derivative * f.value + out.value * f.derivative;
		out.value *= f.value;
	}
	return out;
}
inline double distance(double p, int sign) {
	return sign < 0 ? p : 1 - p;
}
inline V move(uint64_t shape, V p, const Settings &s) {
	V q = p;
	for (unsigned int i = 0; i < 12; ++i) {
		if (((shape >> (12 + 2 * i)) & 3) == 0) {
			continue;
		}
		const Edge e = edge(i);
		double a = distance(p[e.a], e.sa), b = distance(p[e.b], e.sb);
		if (a > s.radius + 1e-8 || b > s.radius + 1e-8) {
			continue;
		}
		const Factor f = factor(shape, i, p[e.along], s);
		q[e.a] = e.sa < 0 ? a * f.value : 1 - a * f.value;
		q[e.b] = e.sb < 0 ? b * f.value : 1 - b * f.value;
	}
	return key(q);
}
inline V normal(uint64_t shape, const Face &face, V p, const StdVector<Face> &cuts, const Settings &s) {
	if (face.edge_index < 0) {
		return face.n;
	}
	const Edge e = edge(face.edge_index);
	const Factor f = factor(shape, face.edge_index, p[e.along], s);
	if (f.value < 1) {
		double a = distance(p[e.a], e.sa), b = distance(p[e.b], e.sb);
		V n{};
		n[e.a] = (1 - a / s.radius) * e.sa;
		n[e.b] = (1 - b / s.radius) * e.sb;
		n[e.along] = s.radius * f.derivative * (1 - (a + b) / s.radius);
		return norm(n);
	}
	StdMap<int, V> gradients;
	for (const Face &cut : cuts) {
		if (std::abs(dot(cut.n, p) - cut.d) < 1e-7) {
			const Edge ce = edge(cut.edge_index);
			V radial{};
			radial[ce.a] = p[ce.a] - cut.center[ce.a];
			radial[ce.b] = p[ce.b] - cut.center[ce.b];
			gradients[cut.edge_index] = norm(radial);
		}
	}
	V n{};
	for (const auto &entry : gradients) {
		n = add(n, entry.second);
	}
	return norm(n);
}
inline StdVector<P> split(const P &p, int axis, double value) {
	StdVector<P> out;
	for (int sign : { -1, 1 }) {
		P o;
		for (size_t j = 0; j < p.size(); ++j) {
			V a = p[j], b = p[(j + 1) % p.size()];
			double da = sign * (a[axis] - value), db = sign * (b[axis] - value);
			bool ai = da <= 1e-10, bi = db <= 1e-10;
			if (ai)
				o.push_back(a);
			if (ai != bi)
				o.push_back(add(a, mul(sub(b, a), da / (da - db))));
		}
		o = clean(o);
		if (o.size() >= 3)
			out.push_back(o);
	}
	return out;
}
inline bool triangulate(const P &p, V n, StdVector<std::array<int, 3>> &out) {
	auto [u, v] = basis(n);
	StdVector<std::array<double, 2>> a;
	for (V q : p)
		a.push_back({ dot(q, u), dot(q, v) });
	double area = 0;
	for (size_t i = 0; i < a.size(); ++i) {
		auto x = a[i], y = a[(i + 1) % a.size()];
		area += x[0] * y[1] - y[0] * x[1];
	}
	StdVector<int> order;
	for (int i = 0; i < int(a.size()); ++i)
		order.push_back(area > 0 ? i : int(a.size()) - 1 - i);
	auto turn = [&](int x, int y, int z) {
		return (a[y][0] - a[x][0]) * (a[z][1] - a[x][1]) - (a[y][1] - a[x][1]) * (a[z][0] - a[x][0]);
	};
	while (order.size() > 3) {
		bool found = false;
		for (size_t j = 0; j < order.size(); ++j) {
			int x = order[(j + order.size() - 1) % order.size()], y = order[j], z = order[(j + 1) % order.size()];
			if (turn(x, y, z) <= 1e-12)
				continue;
			bool blocked = false;
			for (int q : order)
				if (q != x && q != y && q != z && std::min({ turn(x, y, q), turn(y, z, q), turn(z, x, q) }) >= -1e-12) {
					blocked = true;
					break;
				}
			if (blocked)
				continue;
			out.push_back({ x, y, z });
			order.erase(order.begin() + j);
			found = true;
			break;
		}
		if (!found)
			return false;
	}
	out.push_back({ order[0], order[1], order[2] });
	return true;
}

inline Mesh generate_legacy(uint64_t shape, const Settings &settings) {
	// Below the inspector's minimum step, float mesh coordinates cannot retain
	// the thinnest taper triangles reliably. Resolve these values to a cube.
	if (settings.radius < .001) {
		shape = 0;
	}
	const auto cuts = planes(shape, settings);
	const auto faces = base_faces(cuts);
	StdVector<Face> split_faces;
	for (const Face &f : faces) {
		StdVector<P> polygons{ f.p };
		if (f.edge_index >= 0) {
			const unsigned int i = f.edge_index;
			const int axis = edge(i).along;
			for (unsigned int end = 0; end < 2; ++end) {
				if ((shape & (uint64_t(1) << (12 + 2 * i + end))) == 0) {
					continue;
				}
				for (double fraction : { .125, .25, .5, .75, .875, 1. }) {
					double cut = settings.taper_distance * fraction;
					if (end != 0) {
						cut = 1 - cut;
					}
					if (cut <= 1e-9 || cut >= 1 - 1e-9) {
						continue;
					}
					StdVector<P> next;
					for (const P &p : polygons) {
						// A cut coincident with a polygon edge must not duplicate it.
						double lo = 1, hi = 0;
						for (V q : p) {
							lo = std::min(lo, q[axis]);
							hi = std::max(hi, q[axis]);
						}
						if (cut <= lo + 1e-9 || cut >= hi - 1e-9) {
							next.push_back(p);
							continue;
						}
						auto pieces = split(p, axis, cut);
						next.insert(next.end(), pieces.begin(), pieces.end());
					}
					polygons = std::move(next);
				}
			}
		}
		for (const P &p : polygons) {
			Face copy = f;
			copy.p = p;
			split_faces.push_back(copy);
		}
	}
	P candidates;
	StdMap<V, size_t> seen;
	for (auto &f : split_faces)
		for (V p : f.p) {
			auto r = seen.emplace(key(p), candidates.size());
			if (r.second)
				candidates.push_back(p);
			else
				candidates[r.first->second] = p;
		}
	Mesh mesh;
	StdMap<std::array<double, 10>, int> vertices;
	for (const Face &face : split_faces) {
		P augmented;
		if (shape >> 12) {
			for (size_t j = 0; j < face.p.size(); ++j) {
				V a = face.p[j], b = face.p[(j + 1) % face.p.size()], ab = sub(b, a);
				double den = dot(ab, ab);
				if (den < 1e-20)
					continue;
				StdVector<std::pair<double, V>> points{ { 0, a } };
				for (V p : candidates) {
					double t = dot(sub(p, a), ab) / den;
					V d = sub(p, add(a, mul(ab, t)));
					if (t > 1e-8 && t < 1 - 1e-8 && dot(d, d) < 1e-17)
						points.push_back({ t, p });
				}
				std::stable_sort(points.begin(), points.end(), [](auto a, auto b) { return a.first < b.first; });
				for (auto &kv : points)
					augmented.push_back(kv.second);
			}
		} else
			for (V p : face.p)
				augmented.push_back(key(p));
		StdVector<std::pair<V, V>> mapped;
		for (V p : augmented) {
			V q = shape >> 12 ? move(shape, p, settings) : p;
			if (mapped.empty() || q != mapped.back().second)
				mapped.push_back({ p, q });
		}
		if (mapped.size() > 1 && mapped.front().second == mapped.back().second)
			mapped.pop_back();
		if (mapped.size() < 3)
			continue;
		P points;
		for (auto &pq : mapped)
			points.push_back(pq.second);
		V area{};
		for (size_t i = 0; i < points.size(); ++i)
			area = add(area, cross(points[i], points[(i + 1) % points.size()]));
		if (dot(area, area) < 1e-18)
			continue;
		V n = shape >> 12 ? norm(area) : face.n;
		StdVector<int> ids;
		for (auto [p, q] : mapped) {
			V vertex_normal = key(normal(shape, face, p, cuts, settings));
			const int source_face = face.source_face;
			V source_p = q;
			source_p[source_face / 2] = (source_face & 1) ? 1 : 0;
			source_p = key(source_p);
			std::array<double, 10> k{
				q[0],		 q[1],		  q[2],		   vertex_normal[0],   vertex_normal[1], vertex_normal[2],
				source_p[0], source_p[1], source_p[2], double(source_face)
			};
			auto r = vertices.emplace(k, int(mesh.vertices.size()));
			if (r.second)
				mesh.vertices.push_back({ q, vertex_normal, source_p, source_face });
			ids.push_back(r.first->second);
		}
		StdVector<std::array<int, 3>> tris;
		if (!triangulate(points, n, tris)) {
			mesh.valid = false;
			return mesh;
		}
		for (auto tri : tris) {
			if (dot(cross(sub(points[tri[1]], points[tri[0]]), sub(points[tri[2]], points[tri[0]])), n) < 0)
				std::swap(tri[1], tri[2]);
			mesh.triangles.push_back({ { ids[tri[0]], ids[tri[1]], ids[tri[2]] },
									   face.edge_index < 0 ? face.source_face : -1 });
		}
	}
	return mesh;
}
constexpr double CORNER_RADIUS_RATIO = .65;
constexpr double CORNER_EXTENT_RATIO = .65;
struct Corner {
	std::array<int, 3> signs;
	std::array<unsigned int, 3> edges; // Indexed by the direction along the edge.
};
inline StdVector<Corner> free_corners(uint64_t shape) {
	StdVector<Corner> corners;
	for (unsigned int bits = 0; bits < 8; ++bits) {
		Corner c;
		for (int axis = 0; axis < 3; ++axis)
			c.signs[axis] = (bits & (1 << axis)) ? 1 : -1;
		bool complete = true;
		for (unsigned int i = 0; i < 12; ++i) {
			const Edge e = edge(i);
			if (e.sa != c.signs[e.a] || e.sb != c.signs[e.b])
				continue;
			c.edges[e.along] = i;
			const unsigned int end = c.signs[e.along] > 0 ? 1 : 0;
			if ((shape & (uint64_t(1) << i)) == 0 || (shape & (uint64_t(1) << (12 + 2 * i + end))) != 0)
				complete = false;
		}
		if (complete)
			corners.push_back(c);
	}
	return corners;
}
inline V corner_point(const Corner &corner, V distances) {
	for (int a = 0; a < 3; ++a)
		distances[a] = corner.signs[a] < 0 ? distances[a] : 1 - distances[a];
	return key(distances);
}

inline bool append_corner_polygon(
		Mesh &mesh,
		StdMap<std::array<double, 10>, int> &vertices,
		const P &input_points,
		const P &input_normals,
		int source_face,
		bool flat
) {
	P points, normals;
	for (size_t i = 0; i < input_points.size(); ++i) {
		if (points.empty() || key(points.back()) != key(input_points[i])) {
			points.push_back(input_points[i]);
			normals.push_back(input_normals[i]);
		}
	}
	if (points.size() > 1 && key(points.front()) == key(points.back())) {
		points.pop_back();
		normals.pop_back();
	}
	if (points.size() < 3)
		return true;
	V area{};
	for (size_t i = 0; i < points.size(); ++i)
		area = add(area, cross(points[i], points[(i + 1) % points.size()]));
	if (dot(area, area) < 1e-20)
		return true;
	V expected{};
	for (V n : normals)
		expected = add(expected, n);
	StdVector<std::array<int, 3>> triangles;
	if (!triangulate(points, norm(expected), triangles))
		return false;
	StdVector<int> ids;
	for (size_t i = 0; i < points.size(); ++i) {
		const V p = key(points[i]), n = key(norm(normals[i]));
		V source_p = p;
		source_p[source_face / 2] = (source_face & 1) ? 1 : 0;
		const std::array<double, 10> k{ p[0], p[1],		   p[2],		n[0],		 n[1],
										n[2], source_p[0], source_p[1], source_p[2], double(source_face) };
		auto r = vertices.emplace(k, int(mesh.vertices.size()));
		if (r.second)
			mesh.vertices.push_back({ p, n, source_p, source_face });
		ids.push_back(r.first->second);
	}
	for (auto tri : triangles) {
		if (dot(cross(sub(points[tri[1]], points[tri[0]]), sub(points[tri[2]], points[tri[0]])), expected) < 0)
			std::swap(tri[1], tri[2]);
		mesh.triangles.push_back({ { ids[tri[0]], ids[tri[1]], ids[tri[2]] }, flat ? source_face : -1 });
	}
	return true;
}

struct ConnectedEdge {
	bool cap_start = false, cap_end = false;
	StdVector<double> stations;
};
using ConnectedEdges = std::array<ConnectedEdge, 12>;
inline unsigned int edge_index(int a, int sa, int b, int sb) {
	if (a > b) {
		std::swap(a, b);
		std::swap(sa, sb);
	}
	return (a == 1 ? 8 : (b == 1 ? 0 : 4)) + (sa > 0 ? 1 : 0) + (sb > 0 ? 2 : 0);
}
inline Factor connected_radius(
		uint64_t shape,
		unsigned int index,
		const ConnectedEdge &ce,
		double t,
		const Settings &s
) {
	const double cap_radius = CORNER_RADIUS_RATIO * s.radius;
	if (ce.cap_start && ce.cap_end)
		return { cap_radius, 0 };
	const Factor f = factor(shape, index, t, s);
	Factor target{ s.radius * f.value, s.radius * f.derivative };
	if (!ce.cap_start && !ce.cap_end)
		return target;
	const double direction = ce.cap_start ? 1 : -1;
	const double d = ce.cap_start ? t : 1 - t;
	const double u = std::clamp((d - cap_radius) / (1 - cap_radius), 0., 1.);
	const double h = u * u * (3 - 2 * u), dh = direction * 6 * u * (1 - u) / (1 - cap_radius);
	return { cap_radius + h * (target.value - cap_radius), dh * (target.value - cap_radius) + h * target.derivative };
}
inline void sample_connected_interval(
		ConnectedEdge &ce,
		uint64_t shape,
		unsigned int index,
		double a,
		double b,
		const Settings &s,
		int depth = 0
) {
	const double ra = connected_radius(shape, index, ce, a, s).value,
				 rb = connected_radius(shape, index, ce, b, s).value;
	double error = 0;
	for (double f : { .25, .5, .75 }) {
		const double t = a + (b - a) * f;
		error = std::max(error, std::abs(connected_radius(shape, index, ce, t, s).value - (ra + (rb - ra) * f)));
	}
	if (depth < 6 && error > s.radius * 0.02) {
		const double mid = (a + b) * .5;
		sample_connected_interval(ce, shape, index, a, mid, s, depth + 1);
		sample_connected_interval(ce, shape, index, mid, b, s, depth + 1);
	} else
		ce.stations.push_back(b);
}
inline ConnectedEdges connected_edges(uint64_t shape, const StdVector<Corner> &corners, const Settings &s) {
	ConnectedEdges edges;
	for (const Corner &c : corners)
		for (int a = 0; a < 3; ++a) {
			auto &ce = edges[c.edges[a]];
			(c.signs[a] < 0 ? ce.cap_start : ce.cap_end) = true;
		}
	for (unsigned int i = 0; i < 12; ++i) {
		if (!(shape & (uint64_t(1) << i)))
			continue;
		auto &ce = edges[i];
		const double a = ce.cap_start ? CORNER_RADIUS_RATIO * s.radius : 0;
		const double b = ce.cap_end ? 1 - CORNER_RADIUS_RATIO * s.radius : 1;
		StdVector<double> breaks{ a, b };
		for (unsigned int end = 0; end < 2; ++end) {
			if (!(shape & (uint64_t(1) << (12 + 2 * i + end))))
				continue;
			const double t = end == 0 ? s.taper_distance : 1 - s.taper_distance;
			if (t > a + 1e-9 && t < b - 1e-9)
				breaks.push_back(t);
		}
		std::sort(breaks.begin(), breaks.end());
		breaks.erase(std::unique(breaks.begin(), breaks.end()), breaks.end());
		ce.stations.push_back(a);
		for (size_t j = 1; j < breaks.size(); ++j)
			sample_connected_interval(ce, shape, i, breaks[j - 1], breaks[j], s);
	}
	return edges;
}
inline bool supports_connected_edges(uint64_t shape) {
	for (unsigned int bits = 0; bits < 8; ++bits) {
		int count = 0;
		bool protected_end = false;
		for (unsigned int i = 0; i < 12; ++i) {
			const Edge e = edge(i);
			if (e.sa != ((bits & (1 << e.a)) ? 1 : -1) || e.sb != ((bits & (1 << e.b)) ? 1 : -1))
				continue;
			count += (shape >> i) & 1;
			const unsigned int end = (bits & (1 << e.along)) ? 1 : 0;
			protected_end |= (shape >> (12 + 2 * i + end)) & 1;
		}
		if (count == 2 || (count == 3 && protected_end))
			return false;
	}
	return true;
}
inline P connected_face_corner(
		int axis,
		int incoming_fixed,
		const Corner &c,
		uint64_t shape,
		const ConnectedEdges &edges,
		const Settings &s
) {
	int count = 0, active = -1;
	for (unsigned int i = 0; i < 12; ++i) {
		const Edge e = edge(i);
		if (e.sa == c.signs[e.a] && e.sb == c.signs[e.b] && (shape & (uint64_t(1) << i))) {
			++count;
			active = i;
		}
	}
	V d{};
	if (count == 3) {
		d[(axis + 1) % 3] = CORNER_RADIUS_RATIO * s.radius;
		d[(axis + 2) % 3] = CORNER_RADIUS_RATIO * s.radius;
	} else if (count == 1) {
		const Edge e = edge(active);
		const double t = c.signs[e.along] < 0 ? 0 : 1;
		const double radius = connected_radius(shape, active, edges[active], t, s).value;
		if (e.along == axis) {
			P arc;
			const int outgoing_fixed = 3 - axis - incoming_fixed;
			for (int j = 0; j <= 2; ++j) {
				const double angle = j * PI / 4;
				d[incoming_fixed] = radius * (1 - std::cos(angle));
				d[outgoing_fixed] = radius * (1 - std::sin(angle));
				arc.push_back(corner_point(c, d));
			}
			return arc;
		}
		d[e.a == axis ? e.b : e.a] = radius;
	}
	return { corner_point(c, d) };
}
inline Mesh generate_connected(uint64_t shape, const Settings &s) {
	Mesh mesh;
	StdMap<std::array<double, 10>, int> vertices;
	const auto corners = free_corners(shape);
	const auto edges = connected_edges(shape, corners, s);
	const double cap_radius = CORNER_RADIUS_RATIO * s.radius;
	for (int face = 0; face < 6; ++face) {
		const int axis = face / 2, sign = (face & 1) ? 1 : -1, u = (axis + 1) % 3, v = (axis + 2) % 3;
		const std::array<std::array<int, 2>, 4> perimeter{ { { -1, -1 }, { 1, -1 }, { 1, 1 }, { -1, 1 } } };
		P points;
		for (int j = 0; j < 4; ++j) {
			Corner c{};
			c.signs[axis] = sign;
			c.signs[u] = perimeter[j][0];
			c.signs[v] = perimeter[j][1];
			const int incoming_fixed = (j & 1) ? v : u, fixed = 3 - axis - incoming_fixed, along = incoming_fixed;
			const auto cp = connected_face_corner(axis, incoming_fixed, c, shape, edges, s);
			points.insert(points.end(), cp.begin(), cp.end());
			const unsigned int i = edge_index(axis, sign, fixed, c.signs[fixed]);
			if (!(shape & (uint64_t(1) << i)))
				continue;
			auto stations = edges[i].stations;
			if (c.signs[along] > 0)
				std::reverse(stations.begin(), stations.end());
			for (size_t k = 1; k + 1 < stations.size(); ++k) {
				const double t = stations[k], radius = connected_radius(shape, i, edges[i], t, s).value;
				V p{};
				p[axis] = sign < 0 ? 0 : 1;
				p[fixed] = c.signs[fixed] < 0 ? radius : 1 - radius;
				p[along] = t;
				points.push_back(key(p));
			}
		}
		V n{};
		n[axis] = sign;
		if (!append_corner_polygon(mesh, vertices, points, P(points.size(), n), face, true)) {
			mesh.valid = false;
			return mesh;
		}
	}
	for (unsigned int i = 0; i < 12; ++i) {
		if (!(shape & (uint64_t(1) << i)))
			continue;
		const Edge e = edge(i);
		const auto &ce = edges[i];
		for (size_t j = 1; j < ce.stations.size(); ++j)
			for (int segment = 0; segment < 2; ++segment) {
				P points, normals;
				for (auto pair : { std::pair<size_t, int>{ j - 1, segment },
								   { j, segment },
								   { j, segment + 1 },
								   { j - 1, segment + 1 } }) {
					const double t = ce.stations[pair.first], angle = pair.second * PI / 4;
					const double ca = std::cos(angle), cb = std::sin(angle);
					const Factor radius = connected_radius(shape, i, ce, t, s);
					V p{}, n{};
					p[e.along] = t;
					const double da = radius.value * (1 - ca), db = radius.value * (1 - cb);
					p[e.a] = e.sa < 0 ? da : 1 - da;
					p[e.b] = e.sb < 0 ? db : 1 - db;
					n[e.a] = e.sa * ca;
					n[e.b] = e.sb * cb;
					n[e.along] = radius.derivative * (ca + cb - 1);
					points.push_back(key(p));
					normals.push_back(norm(n));
				}
				const int source = segment < 1 ? side(e.a, e.sa) : side(e.b, e.sb);
				if (!append_corner_polygon(mesh, vertices, points, normals, source, false)) {
					mesh.valid = false;
					return mesh;
				}
			}
	}
	for (const Corner &c : corners) {
		// Three material-consistent spherical patches. Their boundary samples
		// match the two-segment quarter-circle bevels, including bottom corners.
		for (int axis = 0; axis < 3; ++axis) {
			const int a = (axis + 1) % 3, b = (axis + 2) % 3;
			P points, normals;
			for (auto ij : { std::pair<int, int>{ 0, 0 }, { 1, 0 }, { 1, 1 }, { 0, 1 } }) {
				V radial{};
				radial[axis] = 1;
				radial[a] = std::tan(ij.first * PI / 4);
				radial[b] = std::tan(ij.second * PI / 4);
				radial = norm(radial);
				V d{}, n{};
				for (int k = 0; k < 3; ++k) {
					d[k] = cap_radius * (1 - radial[k]);
					n[k] = c.signs[k] * radial[k];
				}
				points.push_back(corner_point(c, d));
				normals.push_back(n);
			}
			if (!append_corner_polygon(mesh, vertices, points, normals, side(axis, c.signs[axis]), false)) {
				mesh.valid = false;
				return mesh;
			}
		}
	}
	return mesh;
}
inline Mesh generate(uint64_t shape, const Settings &settings) {
	if (settings.radius < .001)
		shape = 0;
	if (!supports_connected_edges(shape))
		return generate_legacy(shape, settings);
	return generate_connected(shape, settings);
}

} // namespace zylann::voxel::blocky::round_geometry
#endif
