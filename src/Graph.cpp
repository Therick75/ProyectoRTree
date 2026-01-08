#include "../include/Graph.h"
#include <iostream>
#include <sstream>
#include <cmath>

Graph::Graph(double threshold) : nextNodeId(0), snapThreshold(threshold) {}

void Graph::clear() {
    nodes.clear();
    nextNodeId = 0;
}

int Graph::findOrCreateNode(const Point& p) {
    // Buscar si ya existe un nodo cercano
    for (auto& pair : nodes) {
        if (pair.second.position.distanceTo(p) < snapThreshold) {
            return pair.first;
        }
    }
    
    // Crear nuevo nodo
    int id = nextNodeId++;
    nodes[id] = GraphNode(id, p);
    return id;
}

void Graph::buildFromGeometries(const std::vector<Geometry>& geometries) {
    clear();
    
    // Construir grafo desde LineStrings (calles)
    for (const auto& geom : geometries) {
        if (geom.type == GEOM_LINESTRING && geom.points.size() >= 2) {
            // Crear nodos para cada punto de la línea
            std::vector<int> lineNodes;
            for (const auto& point : geom.points) {
                int nodeId = findOrCreateNode(point);
                lineNodes.push_back(nodeId);
            }
            
            // Conectar nodos consecutivos
            for (size_t i = 0; i < lineNodes.size() - 1; i++) {
                int from = lineNodes[i];
                int to = lineNodes[i + 1];
                
                // Calcular distancia (peso)
                double dist = nodes[from].position.distanceTo(nodes[to].position);
                
                // Agregar conexión bidireccional
                nodes[from].neighbors.push_back(to);
                nodes[from].weights.push_back(dist);
                
                nodes[to].neighbors.push_back(from);
                nodes[to].weights.push_back(dist);
            }
        }
    }
}

int Graph::findNearestNode(const Point& p) const {
    if (nodes.empty()) return -1;
    
    int nearestId = -1;
    double minDist = std::numeric_limits<double>::max();
    
    for (const auto& pair : nodes) {
        double dist = pair.second.position.distanceTo(p);
        if (dist < minDist) {
            minDist = dist;
            nearestId = pair.first;
        }
    }
    
    return nearestId;
}

const GraphNode* Graph::getNode(int id) const {
    auto it = nodes.find(id);
    return (it != nodes.end()) ? &it->second : nullptr;
}

int Graph::getEdgeCount() const {
    int count = 0;
    for (const auto& pair : nodes) {
        count += pair.second.neighbors.size();
    }
    return count / 2;  // Dividir por 2 porque las aristas son bidireccionales
}

Route Graph::findShortestPath(const Point& start, const Point& end) {
    Route route;
    
    int startNode = findNearestNode(start);
    int endNode = findNearestNode(end);
    
    if (startNode == -1 || endNode == -1) {
        return route;
    }
    
    // Distancias y predecesores
    std::map<int, double> distances;
    std::map<int, int> predecessors;
    std::set<int> visited;
    
    // Inicializar distancias
    for (const auto& pair : nodes) {
        distances[pair.first] = std::numeric_limits<double>::max();
    }
    distances[startNode] = 0;
    
    // Cola de prioridad: (distancia, nodo)
    std::priority_queue<std::pair<double, int>,
                       std::vector<std::pair<double, int>>,
                       std::greater<std::pair<double, int>>> pq;
    pq.push({0, startNode});
    
    // Dijkstra
    while (!pq.empty()) {
        int current = pq.top().second;
        double currentDist = pq.top().first;
        pq.pop();
        
        if (visited.count(current)) continue;
        visited.insert(current);
        
        if (current == endNode) break;
        
        const GraphNode& node = nodes.at(current);
        for (size_t i = 0; i < node.neighbors.size(); i++) {
            int neighbor = node.neighbors[i];
            double weight = node.weights[i];
            double newDist = currentDist + weight;
            
            if (newDist < distances[neighbor]) {
                distances[neighbor] = newDist;
                predecessors[neighbor] = current;
                pq.push({newDist, neighbor});
            }
        }
    }
    
    // Reconstruir camino
    if (distances[endNode] != std::numeric_limits<double>::max()) {
        route.found = true;
        route.totalDistance = distances[endNode];
        
        // Reconstruir desde el final
        int current = endNode;
        while (current != startNode) {
            route.nodeIds.push_back(current);
            route.path.push_back(nodes.at(current).position);
            current = predecessors[current];
        }
        route.nodeIds.push_back(startNode);
        route.path.push_back(nodes.at(startNode).position);
        
        // Invertir para tener el orden correcto
        std::reverse(route.nodeIds.begin(), route.nodeIds.end());
        std::reverse(route.path.begin(), route.path.end());
    }
    
    return route;
}

Route Graph::findAStarPath(const Point& start, const Point& end) {
    Route route;
    
    int startNode = findNearestNode(start);
    int endNode = findNearestNode(end);
    
    if (startNode == -1 || endNode == -1) {
        return route;
    }
    
    // g: costo real desde inicio
    // f: g + heurística (costo estimado total)
    std::map<int, double> gScore;
    std::map<int, double> fScore;
    std::map<int, int> predecessors;
    std::set<int> closedSet;
    
    // Inicializar scores
    for (const auto& pair : nodes) {
        gScore[pair.first] = std::numeric_limits<double>::max();
        fScore[pair.first] = std::numeric_limits<double>::max();
    }
    gScore[startNode] = 0;
    fScore[startNode] = heuristic(nodes.at(startNode).position, nodes.at(endNode).position);
    
    // Cola de prioridad: (fScore, nodo)
    std::priority_queue<std::pair<double, int>,
                       std::vector<std::pair<double, int>>,
                       std::greater<std::pair<double, int>>> openSet;
    openSet.push({fScore[startNode], startNode});
    
    // A*
    while (!openSet.empty()) {
        int current = openSet.top().second;
        openSet.pop();
        
        if (closedSet.count(current)) continue;
        closedSet.insert(current);
        
        if (current == endNode) {
            // Reconstruir camino
            route.found = true;
            route.totalDistance = gScore[endNode];
            
            int node = endNode;
            while (node != startNode) {
                route.nodeIds.push_back(node);
                route.path.push_back(nodes.at(node).position);
                node = predecessors[node];
            }
            route.nodeIds.push_back(startNode);
            route.path.push_back(nodes.at(startNode).position);
            
            std::reverse(route.nodeIds.begin(), route.nodeIds.end());
            std::reverse(route.path.begin(), route.path.end());
            
            return route;
        }
        
        const GraphNode& node = nodes.at(current);
        for (size_t i = 0; i < node.neighbors.size(); i++) {
            int neighbor = node.neighbors[i];
            
            if (closedSet.count(neighbor)) continue;
            
            double tentativeGScore = gScore[current] + node.weights[i];
            
            if (tentativeGScore < gScore[neighbor]) {
                predecessors[neighbor] = current;
                gScore[neighbor] = tentativeGScore;
                fScore[neighbor] = gScore[neighbor] + 
                                  heuristic(nodes.at(neighbor).position, nodes.at(endNode).position);
                openSet.push({fScore[neighbor], neighbor});
            }
        }
    }
    
    return route;
}

void Graph::printStats() const {
    std::cout << "=== Estadisticas del Grafo ===" << std::endl;
    std::cout << "Nodos: " << getNodeCount() << std::endl;
    std::cout << "Aristas: " << getEdgeCount() << std::endl;
    
    // Calcular grado promedio
    int totalDegree = 0;
    for (const auto& pair : nodes) {
        totalDegree += pair.second.neighbors.size();
    }
    double avgDegree = nodes.empty() ? 0 : (double)totalDegree / nodes.size();
    std::cout << "Grado promedio: " << avgDegree << std::endl;
}