#include <Core/Const/const.h>
#include <Core/Math/IntVec.h>
#include <iostream>

using namespace dem;

bool
IntVec::operator==(const IntVec v)
{
  return d_x == v.d_x && d_y == v.d_y && d_z == v.d_z;
}

bool
IntVec::operator==(const int d)
{
  return d_x == d && d_y == d && d_z == d;
}

bool
IntVec::operator!=(const IntVec v)
{
  return d_x != v.d_x || d_y != v.d_y || d_z != v.d_z;
}

void
IntVec::operator+=(IntVec v)
{
  d_x += v.d_x;
  d_y += v.d_y;
  d_z += v.d_z;
}

void
IntVec::operator-=(IntVec v)
{
  d_x -= v.d_x;
  d_y -= v.d_y;
  d_z -= v.d_z;
}

void
IntVec::operator*=(int d)
{
  d_x *= d;
  d_y *= d;
  d_z *= d;
}

IntVec
IntVec::operator+(IntVec v) const
{
  return IntVec(d_x + v.d_x, d_y + v.d_y, d_z + v.d_z);
}

IntVec
IntVec::operator-(IntVec v) const
{
  return IntVec(d_x - v.d_x, d_y - v.d_y, d_z - v.d_z);
}

IntVec IntVec::operator*(int d) const
{
  return IntVec(d_x * d, d_y * d, d_z * d);
}

int IntVec::operator*(IntVec p) const
{
  return (d_x * p.d_x + d_y * p.d_y + d_z * p.d_z);
}

void
IntVec::print(std::ostream& ofs) const
{
  ofs << std::setw(OWID) << d_x << std::setw(OWID) << d_y << std::setw(OWID)
      << d_z;
}

IntVec
IntVec::fromString(const std::string& str)
{

  // Parse out the [num,num,num]
  std::string::size_type i1 = str.find("[");
  std::string::size_type i2 = str.find_first_of(",");
  std::string::size_type i3 = str.find_last_of(",");
  std::string::size_type i4 = str.find("]");

  std::string x_val(str, i1 + 1, i2 - i1 - 1);
  std::string y_val(str, i2 + 1, i3 - i2 - 1);
  std::string z_val(str, i3 + 1, i4 - i3 - 1);

  // Validate the values
  std::string validChars(" -0123456789");
  std::string::size_type xpos = x_val.find_first_not_of(validChars);
  std::string::size_type ypos = y_val.find_first_not_of(validChars);
  std::string::size_type zpos = z_val.find_first_not_of(validChars);

  if (xpos != std::string::npos || ypos != std::string::npos ||
      zpos != std::string::npos) {
    std::cout << "Unexpected non-integer value in IntVector."
              << " Converting to int if possible\n";
  }

  // Create IntVec from string
  IntVec result;
  result.set(std::stoi(x_val), std::stoi(y_val), std::stoi(z_val));
  return result;
}

IntVec operator*(int d, IntVec v)
{
  return IntVec(v.x() * d, v.y() * d, v.z() * d);
}

IntVec
operator-(IntVec v)
{
  return IntVec(-v.x(), -v.y(), -v.z());
}
