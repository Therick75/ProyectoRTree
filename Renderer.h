#ifndef RENDERER_H
#define RENDERER_H

#include "Geometry.h"
#include "RTree.h"
#include <windows.h>
#include <vector>

class Renderer {
private:
    HWND hwnd;
    int width, height;
    Rectangle viewBounds;
    double zoom;
    Point panOffset;
    
    // Buffer de doble renderizado
    HDC memDC;
    HBITMAP memBitmap;
    
    // Colores
    COLORREF colorBackground;
    COLORREF colorStreet;
    COLORREF colorHighlight;
    COLORREF colorMBR;
    COLORREF colorSearchArea;
    
    // Conversión de coordenadas geográficas a pantalla
    POINT geoToScreen(const Point& p);
    Point screenToGeo(int x, int y);
    
public:
    Renderer(HWND window, int w, int h);
    ~Renderer();
    
    // Configurar vista
    void setViewBounds(const Rectangle& bounds);
    void resetView();
    void pan(int dx, int dy);
    void zoomIn(int centerX, int centerY);
    void zoomOut(int centerX, int centerY);
    
    // Renderizado
    void render(HDC hdc, const std::vector<Geometry>& geometries);
    void renderRTreeNodes(HDC hdc, RTreeNode* node, int level);
    void renderGeometry(HDC hdc, const Geometry& geom, bool highlight = false);
    void renderSearchArea(HDC hdc, const Rectangle& area);
    void renderSearchResults(HDC hdc, const std::vector<Geometry*>& results);
    
    // Obtener punto en coordenadas geográficas desde pantalla
    Point getGeoPoint(int screenX, int screenY);
    
    // Información
    Rectangle getViewBounds() const { return viewBounds; }
    double getZoom() const { return zoom; }
};

#endif // RENDERER_H