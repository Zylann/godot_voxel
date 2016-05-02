#ifndef VOXEL_VECTOR3I_H
#define VOXEL_VECTOR3I_H

struct Vector3i {

    int x;
    int y;
    int z;

    Vector3i() : x(0), y(0), z(0) {}

    Vector3i(int px, int py, int pz) : x(px), y(py), z(pz) {}

    Vector3i(const Vector3i & other) {
        *this = other;
    }

    Vector3i & operator=(const Vector3i & other) {
        x = other.x;
        y = other.y;
        z = other.z;
        return *this;
    }

    Vector3i operator+(const Vector3i & other) {
        return Vector3i(x + other.x, y + other.y, z + other.z);
    }

    Vector3i operator-(const Vector3i & other) {
        return Vector3i(x - other.x, y - other.y, z - other.z);
    }

    Vector3i operator*(int n) {
        return Vector3i(x * n, y * n, z * n);
    }

    Vector3i operator/(int n) {
        return Vector3i(x / n, y / n, z / n);
    }

    bool operator==(const Vector3i & other) {
        return x == other.x && y == other.y && z == other.z;
    }

    bool operator!=(const Vector3i & other) {
        return x != other.x && y != other.y && z != other.z;
    }

};

#endif // VOXEL_VECTOR3I_H

