#include "../include/RTree.h"
#include <limits>
#include <cmath>

RTree::RTree() : height(1), nodeCount(1), geometryCount(0) {
    root = new RTreeNode(true);
}

RTree::~RTree() {
    delete root;
}

void RTree::clear() {
    delete root;
    root = new RTreeNode(true);
    height = 1;
    nodeCount = 1;
    geometryCount = 0;
}

void RTree::insert(Geometry* geom) {
    geometryCount++;

    // Paso 1: Elegir hoja
    RTreeNode* leaf = chooseLeaf(root, geom->mbr);

    // Paso 2: Insertar en la hoja
    leaf->entries.push_back(geom);
    leaf->mbrs.push_back(geom->mbr);
    leaf->updateMBR();

    // Paso 3: Si está llena, dividir
    RTreeNode* splitNode = nullptr;
    if (leaf->isFull()) {
        splitNodeInternal(leaf, &splitNode);
    }

    // Paso 4: Ajustar árbol
    adjustTree(leaf, splitNode);
}

RTreeNode* RTree::chooseLeaf(RTreeNode* node, const Rect& mbr) {
    if (node->isLeaf) {
        return node;
    }

    // Elegir hijo con menor expansión
    double minExpansion = std::numeric_limits<double>::max();
    int bestIndex = 0;

    for (size_t i = 0; i < node->children.size(); i++) {
        double expansion = node->children[i]->mbr.expansionArea(mbr);

        if (expansion < minExpansion) {
            minExpansion = expansion;
            bestIndex = i;
        } else if (expansion == minExpansion) {
            // Desempate por área menor
            if (node->children[i]->mbr.area() < node->children[bestIndex]->mbr.area()) {
                bestIndex = i;
            }
        }
    }

    return chooseLeaf(node->children[bestIndex], mbr);
}

void RTree::splitNodeInternal(RTreeNode* node, RTreeNode** newNode) {
    *newNode = new RTreeNode(node->isLeaf);
    nodeCount++;

    if (node->isLeaf) {
        // Split para hojas
        std::vector<Geometry*> allEntries = node->entries;
        std::vector<Rect> allMbrs = node->mbrs;

        node->entries.clear();
        node->mbrs.clear();

        // Encontrar semillas (los más distantes)
        int seed1 = 0, seed2 = 1;
        double maxDist = 0;

        for (size_t i = 0; i < allEntries.size(); i++) {
            for (size_t j = i + 1; j < allEntries.size(); j++) {
                Rect combined = allMbrs[i];
                combined.expand(allMbrs[j]);
                double waste = combined.area() - allMbrs[i].area() - allMbrs[j].area();

                if (waste > maxDist) {
                    maxDist = waste;
                    seed1 = i;
                    seed2 = j;
                }
            }
        }

        // Asignar semillas
        node->entries.push_back(allEntries[seed1]);
        node->mbrs.push_back(allMbrs[seed1]);
        (*newNode)->entries.push_back(allEntries[seed2]);
        (*newNode)->mbrs.push_back(allMbrs[seed2]);

        // Distribuir resto
        for (size_t i = 0; i < allEntries.size(); i++) {
            if ((int)i == seed1 || (int)i == seed2) continue;

            Rect mbr1 = node->calculateMBR();
            Rect mbr2 = (*newNode)->calculateMBR();

            double exp1 = mbr1.expansionArea(allMbrs[i]);
            double exp2 = mbr2.expansionArea(allMbrs[i]);

            if (exp1 < exp2) {
                node->entries.push_back(allEntries[i]);
                node->mbrs.push_back(allMbrs[i]);
            } else if (exp2 < exp1) {
                (*newNode)->entries.push_back(allEntries[i]);
                (*newNode)->mbrs.push_back(allMbrs[i]);
            } else {
                // Desempate por área
                if (mbr1.area() < mbr2.area()) {
                    node->entries.push_back(allEntries[i]);
                    node->mbrs.push_back(allMbrs[i]);
                } else {
                    (*newNode)->entries.push_back(allEntries[i]);
                    (*newNode)->mbrs.push_back(allMbrs[i]);
                }
            }
        }

    } else {
        // Split para nodos internos
        std::vector<RTreeNode*> allChildren = node->children;
        std::vector<Rect> allMbrs = node->mbrs;

        node->children.clear();
        node->mbrs.clear();

        // Simplificado: dividir por la mitad
        size_t mid = allChildren.size() / 2;

        for (size_t i = 0; i < mid; i++) {
            node->children.push_back(allChildren[i]);
            node->mbrs.push_back(allMbrs[i]);
            allChildren[i]->parent = node;
        }

        for (size_t i = mid; i < allChildren.size(); i++) {
            (*newNode)->children.push_back(allChildren[i]);
            (*newNode)->mbrs.push_back(allMbrs[i]);
            allChildren[i]->parent = *newNode;
        }
    }

    node->updateMBR();
    (*newNode)->updateMBR();
}

void RTree::adjustTree(RTreeNode* node, RTreeNode* splitNode) {
    while (node != root) {
        RTreeNode* parent = node->parent;

        // Actualizar MBR del padre
        for (size_t i = 0; i < parent->children.size(); i++) {
            if (parent->children[i] == node) {
                parent->mbrs[i] = node->mbr;
                break;
            }
        }
        parent->updateMBR();

        // Si hubo split, agregar nuevo nodo al padre
        if (splitNode) {
            parent->children.push_back(splitNode);
            parent->mbrs.push_back(splitNode->mbr);
            splitNode->parent = parent;

            if (parent->isFull()) {
                RTreeNode* newParent = nullptr;
                splitNodeInternal(parent, &newParent);
                splitNode = newParent;
            } else {
                splitNode = nullptr;
            }
        }

        node = parent;
    }

    // Si la raíz se dividió
    if (splitNode) {
        RTreeNode* newRoot = new RTreeNode(false);
        newRoot->children.push_back(root);
        newRoot->children.push_back(splitNode);
        newRoot->mbrs.push_back(root->mbr);
        newRoot->mbrs.push_back(splitNode->mbr);
        root->parent = newRoot;
        splitNode->parent = newRoot;
        newRoot->updateMBR();
        root = newRoot;
        height++;
        nodeCount++;
    }
}

std::vector<Geometry*> RTree::rangeSearch(const Rect& range) {
    std::vector<Geometry*> results;
    rangeSearchRecursive(root, range, results);
    return results;
}

void RTree::rangeSearchRecursive(RTreeNode* node, const Rect& range,
                                  std::vector<Geometry*>& results) {
    if (!node->mbr.intersects(range)) {
        return;
    }

    if (node->isLeaf) {
        for (size_t i = 0; i < node->entries.size(); i++) {
            if (node->mbrs[i].intersects(range)) {
                results.push_back(node->entries[i]);
            }
        }
    } else {
        for (auto* child : node->children) {
            rangeSearchRecursive(child, range, results);
        }
    }
}

std::vector<Geometry*> RTree::kNNSearch(const Point& queryPoint, int k) {
    std::vector<Geometry*> results;

    // Búsqueda simple: obtener todas y ordenar por distancia
    std::vector<std::pair<double, Geometry*>> candidates;

    std::function<void(RTreeNode*)> collect = [&](RTreeNode* node) {
        if (node->isLeaf) {
            for (auto* geom : node->entries) {
                double dist = geom->minDistance(queryPoint);
                candidates.push_back({dist, geom});
            }
        } else {
            for (auto* child : node->children) {
                collect(child);
            }
        }
    };

    collect(root);

    // Ordenar por distancia
    std::sort(candidates.begin(), candidates.end(),
              [](const auto& a, const auto& b) { return a.first < b.first; });

    // Tomar los k primeros
    for (int i = 0; i < k && i < (int)candidates.size(); i++) {
        results.push_back(candidates[i].second);
    }

    return results;
}
