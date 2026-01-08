#ifndef GEOMETRY_H
#define GEOMETRY_H

#include <vector>
#include <cmath>
#include <limits>
#include <algorithm>

struct Point {
    double x, y;

    Point() : x(0), y(0) {}
    Point(double _x, double _y) : x(_x), y(_y) {}

    double distanceTo(const Point& other) const {
        double dx = x - other.x;
        double dy = y - other.y;
        return std::sqrt(dx * dx + dy * dy);
    }
};

// Renombramos Rectangle a Rect para evitar conflicto con WinAPI
struct Rect {
    double minX, minY, maxX, maxY;

    Rect() : minX(0), minY(0), maxX(0), maxY(0) {}

    Rect(double x1, double y1, double x2, double y2)
        : minX(x1), minY(y1), maxX(x2), maxY(y2) {}

    // Constructor desde un punto
    Rect(const Point& p)
        : minX(p.x), minY(p.y), maxX(p.x), maxY(p.y) {}

    double area() const {
        return (maxX - minX) * (maxY - minY);
    }

    double perimeter() const {
        return 2.0 * ((maxX - minX) + (maxY - minY));
    }

    bool intersects(const Rect& other) const {
        return !(maxX < other.minX || minX > other.maxX ||
                maxY < other.minY || minY > other.maxY);
    }

    bool contains(const Point& p) const {
        return p.x >= minX && p.x <= maxX &&
               p.y >= minY && p.y <= maxY;
    }

    bool contains(const Rect& other) const {
        return minX <= other.minX && maxX >= other.maxX &&
               minY <= other.minY && maxY >= other.maxY;
    }

    // Expandir para incluir otro rectángulo
    void expand(const Rect& other) {
        minX = std::min(minX, other.minX);
        minY = std::min(minY, other.minY);
        maxX = std::max(maxX, other.maxX);
        maxY = std::max(maxY, other.maxY);
    }

    // Expandir para incluir un punto
    void expand(const Point& p) {
        minX = std::min(minX, p.x);
        minY = std::min(minY, p.y);
        maxX = std::max(maxX, p.x);
        maxY = std::max(maxY, p.y);
    }

    Point center() const {
        return Point((minX + maxX) / 2.0, (minY + minY) / 2.0);
    }

    // Calcular incremento de área al expandir con otro rectángulo
    double expansionArea(const Rect& other) const {
        Rect expanded = *this;
        expanded.expand(other);
        return expanded.area() - this->area();
    }
};

// Geometría (puede ser punto, línea o polígono)
enum GeometryType {
    GEOM_POINT,
    GEOM_LINESTRING,
    GEOM_POLYGON
};

class Geometry {
public:
    GeometryType type;
    std::vector<Point> points;
    Rect mbr; // Minimum Bounding Rectangle
    int id;

    Geometry() : type(GEOM_POINT), id(-1) {}

    Geometry(GeometryType t, const std::vector<Point>& pts, int _id = -1)
        : type(t), points(pts), id(_id) {
        calculateMBR();
    }

    void calculateMBR() {
        if (points.empty()) return;

        mbr = Rect(points[0]);
        for (size_t i = 1; i < points.size(); i++) {
            mbr.expand(points[i]);
        }
    }

    // Distancia mínima desde un punto a esta geometría
    double minDistance(const Point& p) const {
        if (mbr.contains(p)) return 0.0;

        double minDist = std::numeric_limits<double>::max();

        // Calcular distancia a los bordes del MBR
        double dx = std::max(mbr.minX - p.x, p.x - mbr.maxX);
        double dy = std::max(mbr.minY - p.y, p.y - mbr.maxY);
        dx = std::max(0.0, dx);
        dy = std::max(0.0, dy);

        return std::sqrt(dx * dx + dy * dy);
    }
};

#endif // GEOMETRY_H
