#include "../include/GeoJSONParser.h"
#include <algorithm>
#include <cctype>

bool GeoJSONParser::loadFromFile(const std::string& filename) {
    std::ifstream file(filename);
    if (!file.is_open()) {
        return false;
    }

    std::stringstream buffer;
    buffer << file.rdbuf();
    std::string json = buffer.str();
    file.close();

    // Limpiar geometrías anteriores
    geometries.clear();

    size_t pos = 0;
    int geomId = 0;

    // Buscar cada Feature
    while ((pos = json.find("\"type\": \"Feature\"", pos)) != std::string::npos ||
           (pos = json.find("\"type\":\"Feature\"", pos)) != std::string::npos) {

        Geometry geom;
        geom.id = geomId++;

        // Buscar el geometry object
        size_t geomPos = json.find("\"geometry\"", pos);
        if (geomPos == std::string::npos || geomPos > pos + 2000) {
            pos += 50;
            continue;
        }

        // Buscar tipo de geometría dentro del geometry object
        size_t typePos = json.find("\"type\"", geomPos);
        if (typePos != std::string::npos && typePos < geomPos + 200) {
            size_t typeStart = json.find("\"", typePos + 7) + 1;
            size_t typeEnd = json.find("\"", typeStart);
            std::string geomType = json.substr(typeStart, typeEnd - typeStart);

            if (geomType == "Point") {
                geom.type = GEOM_POINT;
            } else if (geomType == "LineString") {
                geom.type = GEOM_LINESTRING;
            } else if (geomType == "Polygon") {
                geom.type = GEOM_POLYGON;
            } else {
                // Tipo no soportado
                pos = geomPos + 50;
                continue;
            }
        }

        // Buscar coordenadas
        size_t coordPos = json.find("\"coordinates\"", geomPos);
        if (coordPos != std::string::npos && coordPos < geomPos + 300) {
            // Encontrar el inicio del array de coordenadas
            size_t startBracket = json.find('[', coordPos);
            if (startBracket == std::string::npos) {
                pos = coordPos + 50;
                continue;
            }

            // Contar corchetes para encontrar el cierre
            int bracketCount = 1;
            size_t endBracket = startBracket + 1;

            while (bracketCount > 0 && endBracket < json.length()) {
                if (json[endBracket] == '[') bracketCount++;
                if (json[endBracket] == ']') bracketCount--;
                endBracket++;
            }

            if (bracketCount == 0) {
                std::string coordStr = json.substr(startBracket, endBracket - startBracket);

                // Para polígonos, necesitamos el primer anillo
                if (geom.type == GEOM_POLYGON) {
                    parsePolygonCoordinates(coordStr, geom.points);
                } else {
                    parseCoordinates(coordStr, geom.points);
                }

                if (!geom.points.empty()) {
                    geom.calculateMBR();
                    geometries.push_back(geom);
                }
            }
        }

        pos = geomPos + 100;
    }

    return !geometries.empty();
}

void GeoJSONParser::parseCoordinates(const std::string& coords, std::vector<Point>& points) {
    size_t pos = 0;

    while (pos < coords.length()) {
        // Buscar siguiente par [lon, lat]
        size_t start = coords.find('[', pos);
        if (start == std::string::npos) break;

        // Verificar si es un punto (no hay otro [ inmediatamente después)
        size_t nextChar = start + 1;
        while (nextChar < coords.length() && (coords[nextChar] == ' ' || coords[nextChar] == '\n')) {
            nextChar++;
        }

        if (nextChar < coords.length() && coords[nextChar] == '[') {
            // Es un array anidado, saltar
            pos = start + 1;
            continue;
        }

        size_t end = coords.find(']', start);
        if (end == std::string::npos) break;

        std::string pair = coords.substr(start + 1, end - start - 1);

        // Separar lon y lat
        size_t comma = pair.find(',');
        if (comma != std::string::npos) {
            try {
                // Limpiar espacios
                std::string lonStr = pair.substr(0, comma);
                std::string latStr = pair.substr(comma + 1);

                // Remover espacios en blanco
                lonStr.erase(std::remove_if(lonStr.begin(), lonStr.end(), ::isspace), lonStr.end());
                latStr.erase(std::remove_if(latStr.begin(), latStr.end(), ::isspace), latStr.end());

                double lon = std::stod(lonStr);
                double lat = std::stod(latStr);
                points.push_back(Point(lon, lat));
            } catch (const std::exception& e) {
                // Ignorar errores de conversión
            }
        }

        pos = end + 1;
    }
}

void GeoJSONParser::parsePolygonCoordinates(const std::string& coords, std::vector<Point>& points) {
    // Para polígonos, GeoJSON tiene: [[[lon,lat], [lon,lat], ...]]
    // Necesitamos el primer anillo (exterior)

    size_t firstBracket = coords.find('[');
    if (firstBracket == std::string::npos) return;

    size_t secondBracket = coords.find('[', firstBracket + 1);
    if (secondBracket == std::string::npos) return;

    // Encontrar el cierre del primer anillo
    int bracketCount = 1;
    size_t pos = secondBracket + 1;

    while (bracketCount > 0 && pos < coords.length()) {
        if (coords[pos] == '[') bracketCount++;
        if (coords[pos] == ']') bracketCount--;
        pos++;
    }

    if (bracketCount == 0) {
        std::string ring = coords.substr(secondBracket, pos - secondBracket);
        parseCoordinates(ring, points);
    }
}

Rect GeoJSONParser::getBounds() const {
    if (geometries.empty()) return Rect();

    Rect bounds = geometries[0].mbr;
    for (const auto& geom : geometries) {
        bounds.expand(geom.mbr);
    }
    return bounds;
}

int GeoJSONParser::getPointCount() const {
    return std::count_if(geometries.begin(), geometries.end(),
        [](const Geometry& g) { return g.type == GEOM_POINT; });
}

int GeoJSONParser::getLineStringCount() const {
    return std::count_if(geometries.begin(), geometries.end(),
        [](const Geometry& g) { return g.type == GEOM_LINESTRING; });
}

int GeoJSONParser::getPolygonCount() const {
    return std::count_if(geometries.begin(), geometries.end(),
        [](const Geometry& g) { return g.type == GEOM_POLYGON; });
}
