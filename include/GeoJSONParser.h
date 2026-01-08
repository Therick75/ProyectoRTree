#ifndef GEOJSONPARSER_H
#define GEOJSONPARSER_H

#include "Geometry.h"
#include <vector>
#include <string>
#include <fstream>
#include <sstream>

class GeoJSONParser {
private:
    std::vector<Geometry> geometries;

    void parseCoordinates(const std::string& coords, std::vector<Point>& points);
    void parsePolygonCoordinates(const std::string& coords, std::vector<Point>& points);

public:
    bool loadFromFile(const std::string& filename);
    const std::vector<Geometry>& getGeometries() const { return geometries; }

    Rect getBounds() const;

    int getPointCount() const;
    int getLineStringCount() const;
    int getPolygonCount() const;
};

#endif // GEOJSONPARSER_H
