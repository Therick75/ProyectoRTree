#ifndef GRAPH_H
#define GRAPH_H

#include "Geometry.h"
#include <vector>
#include <map>
#include <queue>
#include <set>
#include <limits>
#include <algorithm>
#include <cmath>

// Estructura para representar un nodo en el grafo (intersección)
struct GraphNode {
    int id;
    Point position;
    std::vector<int> neighbors;  // IDs de nodos vecinos
    std::vector<double> weights;  // Pesos (distancias) a vecinos
    
    GraphNode() : id(-1) {}
    GraphNode(int _id, const Point& pos) : id(_id), position(pos) {}
};

// Estructura para el resultado de búsqueda de ruta
struct Route {
    std::vector<int> nodeIds;  // Secuencia de nodos
    std::vector<Point> path;   // Secuencia de puntos
    double totalDistance;
    bool found;
    
    Route() : totalDistance(0), found(false) {}
};

// Clase principal del grafo
class Graph {
private:
    std::map<int, GraphNode> nodes;
    int nextNodeId;
    double snapThreshold;  // Umbral para considerar puntos como el mismo nodo
    
    // Funciones auxiliares
    int findOrCreateNode(const Point& p);
    double heuristic(const Point& a, const Point& b) const;
    
public:
    Graph(double threshold = 0.0001);
    
    // Construcción del grafo
    void buildFromGeometries(const std::vector<Geometry>& geometries);
    void clear();
    
    // Búsqueda de rutas
    Route findShortestPath(const Point& start, const Point& end);
    Route findAStarPath(const Point& start, const Point& end);
    
    // Consultas
    int findNearestNode(const Point& p) const;
    const GraphNode* getNode(int id) const;
    int getNodeCount() const { return nodes.size(); }
    int getEdgeCount() const;
    
    // Acceso a los nodos
    const std::map<int, GraphNode>& getNodes() const { return nodes; }
    
    // Estadísticas
    void printStats() const;
};

// Implementación inline de algunas funciones cortas
inline double Graph::heuristic(const Point& a, const Point& b) const {
    return a.distanceTo(b);
}

#endif // GRAPH_H