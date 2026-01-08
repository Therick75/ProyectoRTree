#ifndef RTREE_H
#define RTREE_H

#include "Geometry.h"
#include <vector>
#include <algorithm>
#include <queue>
#include <memory>

const int MAX_ENTRIES = 4;
const int MIN_ENTRIES = 2;

struct RTreeNode {
    bool isLeaf;
    std::vector<Geometry*> entries;
    std::vector<RTreeNode*> children;
    std::vector<Rect> mbrs;
    RTreeNode* parent;
    Rect mbr;  // MBR del nodo completo

    RTreeNode(bool leaf = true) : isLeaf(leaf), parent(nullptr) {}

    ~RTreeNode() {
        if (!isLeaf) {
            for (auto child : children) {
                delete child;
            }
        }
    }

    Rect calculateMBR() const {
        if (mbrs.empty()) return Rect();
        Rect result = mbrs[0];
        for (size_t i = 1; i < mbrs.size(); i++) {
            result.expand(mbrs[i]);
        }
        return result;
    }

    void updateMBR() {
        mbr = calculateMBR();
    }

    bool isFull() const {
        return (isLeaf && entries.size() >= MAX_ENTRIES) ||
               (!isLeaf && children.size() >= MAX_ENTRIES);
    }
};

class RTree {
private:
    RTreeNode* root;
    int height;
    int nodeCount;
    int geometryCount;

    RTreeNode* chooseLeaf(RTreeNode* node, const Rect& mbr);
    void splitNodeInternal(RTreeNode* node, RTreeNode** newNode);
    void adjustTree(RTreeNode* node, RTreeNode* splitNode);

    void rangeSearchRecursive(RTreeNode* node, const Rect& range,
                             std::vector<Geometry*>& results);

public:
    RTree();
    ~RTree();

    void insert(Geometry* geom);
    std::vector<Geometry*> rangeSearch(const Rect& range);
    std::vector<Geometry*> kNNSearch(const Point& queryPoint, int k);

    RTreeNode* getRoot() const { return root; }
    int getHeight() const { return height; }
    int getNodeCount() const { return nodeCount; }
    int getGeometryCount() const { return geometryCount; }

    void clear();
};

#endif // RTREE_H
