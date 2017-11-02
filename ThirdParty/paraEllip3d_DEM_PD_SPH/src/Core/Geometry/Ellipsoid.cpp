#include <Core/Const/Constants.h>
#include <Core/Geometry/Ellipsoid.h>
#include <Core/Geometry/OrientedBox.h>
#include <Core/Math/Matrix3.h>
#include <Core/Math/Vec.h>
#include <iostream>

namespace dem {

void 
Ellipsoid::normalize_axes() {
  d_axes[0].normalizeInPlace();
  d_axes[1].normalizeInPlace();
  d_axes[2].normalizeInPlace();
}

OrientedBox
Ellipsoid::getOrientedBoundingBox() const
{
  OrientedBox box(d_center, d_axes[0], d_axes[1], d_axes[2],
                  d_radii[0], d_radii[1], d_radii[2]);
  return box;
}

Matrix3 
Ellipsoid::toUnitSphereTransformationMatrix() const
{
  Matrix3 mat;
  for (auto ii = 0; ii < 3; ++ii) {
    for (auto jj = 0; jj < 3; ++jj) {
      mat(ii, jj) = d_axes[ii][jj]/d_radii[ii];
    }
  }
  return mat;
}

bool 
Ellipsoid::containsPoint(const Vec& point) const
{
  Matrix3 N = toUnitSphereTransformationMatrix();
  Vec pp = N*(point - d_center) ;
  if (pp.length() > 1.0) {
    return false;
  }
  return true;
}

bool 
Ellipsoid::intersects(const OrientedBox& box) const
{
  OrientedBox boundingBox = getOrientedBoundingBox();
  if (!boundingBox.intersects(box)) return false;

  // Identify the visible faces
  Vec ww = d_center - box.center();
  std::vector<std::pair<int, float>> visibleFaces;
  for (int ii = 0; ii < 3; ++ii) {
    REAL dd = dot(ww, box.axis(ii));
    if (std::abs(dd) > box.extent(ii)) {
      visibleFaces.push_back(std::make_pair(ii, std::copysign(1, dd)));
    }
  }
  // No face chosen; center of ellipsoid is inside box
  if (visibleFaces.size() < 1) return true;
                            
  // Compute transformation matrix
  Matrix3 N = toUnitSphereTransformationMatrix();

  // Transform the oriented box into a parallelepiped
  Vec center = N * (-ww);
  std::array<Vec, 3> axes;
  for (auto ii = 0u; ii < 3; ii++) {
    axes[ii] = N * (box.axis(ii) * box.extent(ii));
  }

  // Compute the vertex locations
  std::vector<Vec> vertices; vertices.reserve(8);
  vertices.push_back(center - axes[0] - axes[1] - axes[2]);
  vertices.push_back(center + axes[0] - axes[1] - axes[2]);
  vertices.push_back(center + axes[0] + axes[1] - axes[2]);
  vertices.push_back(center - axes[0] + axes[1] - axes[2]);
  vertices.push_back(center - axes[0] - axes[1] + axes[2]);
  vertices.push_back(center + axes[0] - axes[1] + axes[2]);
  vertices.push_back(center + axes[0] + axes[1] + axes[2]);
  vertices.push_back(center - axes[0] + axes[1] + axes[2]);

  // Check if the vertices are inside the transformed ellipsoid
  // Note that the transformed ellipsoid is now a sphere centered at (0,0,0)
  for (const auto& vertex : vertices) {
    if (vertex.lengthSq() < 1.0) return true;
  }

  // Set up the edges
  /*
  constexpr std::array<std::array<int, 2>, 12> edges = {{
    {{0, 1}}, {{1, 2}}, {{2, 3}}, {{3, 0}},
    {{4, 5}}, {{5, 6}}, {{6, 7}}, {{7, 4}},
    {{0, 4}}, {{1, 5}}, {{2, 6}}, {{3, 7}},
    }};
  */
  
  // Check sphere edge intersections
  /*
  for (const auto& edge : edges) {
  }
  */

  // Set up the faces
  /*
  constexpr std::array<std::array<int, 4>, 6> faces = {{
     {{0, 4, 7, 3}}, // x-
     {{1, 2, 6, 5}}, // x+
     {{0, 1, 5, 4}}, // y-
     {{3, 7, 6, 2}}, // y+
     {{0, 3, 2, 1}}, // z-
     {{4, 5, 6, 7}}, // z+
    }};
  */
  /*
  for (const auto& face : faces) {
    int v0 = face[0];
    int v1 = face[1];
    int v2 = face[2];
    Vec normal = cross((vertices[v1] - vertices[v0]),(vertices[v2] - vertices[v0]))
                 .normalizeInPlace();
    REAL dist = std::abs(dot(normal, vertices[v1]));
    if (dist > 1.0) return false;
  }
  */

  return true;
}

void 
Ellipsoid::rotate(REAL angle, const Vec& axis)
{
  Matrix3 rot(angle, axis);
  d_axes[0] = rot*d_axes[0];
  d_axes[1] = rot*d_axes[1];
  d_axes[2] = rot*d_axes[2];
}

void 
Ellipsoid::translate(const Vec& dist)
{
  d_center += dist;
}

std::ostream&
operator<<(std::ostream& os, const Ellipsoid& b)
{
  os << " center = " << b.d_center 
     << " axis_a = " << b.d_axes[0] 
     << " axis_b = " << b.d_axes[1] 
     << " axis_c = " << b.d_axes[2] 
     << " radius_a = " << b.d_radii[0] 
     << " radius_b = " << b.d_radii[1] 
     << " radius_c = " << b.d_radii[2] 
     << "\n";
  return os;
}

} // namespace dem
