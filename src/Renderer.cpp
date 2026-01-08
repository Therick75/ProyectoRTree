#include "../include/Renderer.h"
#include <algorithm>

Renderer::Renderer(HWND window, int w, int h)
    : hwnd(window), width(w), height(h), zoom(1.0), panOffset(0, 0) {

    // Configurar colores
    colorBackground = RGB(240, 240, 240);
    colorStreet = RGB(50, 50, 50);
    colorHighlight = RGB(255, 0, 0);
    colorMBR = RGB(0, 100, 255);
    colorSearchArea = RGB(0, 200, 0);
    colorRoute = RGB(255, 0, 0);
    colorStartPoint = RGB(0, 255, 0);
    colorEndPoint = RGB(255, 0, 0);
    colorGraphNode = RGB(100, 100, 255);

    // Crear buffer para doble renderizado
    HDC hdc = GetDC(hwnd);
    memDC = CreateCompatibleDC(hdc);
    memBitmap = CreateCompatibleBitmap(hdc, width, height);
    SelectObject(memDC, memBitmap);
    ReleaseDC(hwnd, hdc);
}

Renderer::~Renderer() {
    DeleteObject(memBitmap);
    DeleteDC(memDC);
}

void Renderer::setViewBounds(const Rect& bounds) {
    viewBounds = bounds;

    // Ajustar aspect ratio
    double aspectView = (bounds.maxX - bounds.minX) / (bounds.maxY - bounds.minY);
    double aspectScreen = (double)width / height;

    if (aspectView > aspectScreen) {
        // Expandir verticalmente
        double centerY = (bounds.minY + bounds.maxY) / 2;
        double newHeight = (bounds.maxX - bounds.minX) / aspectScreen;
        viewBounds.minY = centerY - newHeight / 2;
        viewBounds.maxY = centerY + newHeight / 2;
    } else {
        // Expandir horizontalmente
        double centerX = (bounds.minX + bounds.maxX) / 2;
        double newWidth = (bounds.maxY - bounds.minY) * aspectScreen;
        viewBounds.minX = centerX - newWidth / 2;
        viewBounds.maxX = centerX + newWidth / 2;
    }

    // Resetear zoom y pan
    zoom = 1.0;
    panOffset = Point(0, 0);
}

void Renderer::resetView() {
    zoom = 1.0;
    panOffset = Point(0, 0);
}

POINT Renderer::geoToScreen(const Point& p) {
    // Aplicar pan y zoom
    double adjustedX = (p.x - panOffset.x) * zoom;
    double adjustedY = (p.y - panOffset.y) * zoom;

    // Escalar al tamaño de la ventana
    double scaleX = width / (viewBounds.maxX - viewBounds.minX);
    double scaleY = height / (viewBounds.maxY - viewBounds.minY);

    POINT screen;
    screen.x = (int)((adjustedX - viewBounds.minX) * scaleX);
    screen.y = (int)(height - (adjustedY - viewBounds.minY) * scaleY);

    return screen;
}

Point Renderer::screenToGeo(int x, int y) {
    // Escala inversa
    double scaleX = (viewBounds.maxX - viewBounds.minX) / width;
    double scaleY = (viewBounds.maxY - viewBounds.minY) / height;

    Point geo;
    geo.x = viewBounds.minX + x * scaleX;
    geo.y = viewBounds.maxY - y * scaleY;

    // Aplicar pan inverso
    geo.x = geo.x / zoom + panOffset.x;
    geo.y = geo.y / zoom + panOffset.y;

    return geo;
}

Point Renderer::getGeoPoint(int screenX, int screenY) {
    return screenToGeo(screenX, screenY);
}

void Renderer::pan(int dx, int dy) {
    // Convertir desplazamiento de pantalla a coordenadas geográficas
    double scaleX = (viewBounds.maxX - viewBounds.minX) / width / zoom;
    double scaleY = (viewBounds.maxY - viewBounds.minY) / height / zoom;

    panOffset.x -= dx * scaleX;
    panOffset.y += dy * scaleY;
}

void Renderer::zoomIn(int centerX, int centerY) {
    // Guardar el punto central antes del zoom
    Point centerGeo = screenToGeo(centerX, centerY);

    // Aplicar zoom
    zoom *= 1.2;
    if (zoom > 50.0) zoom = 50.0;

    // Ajustar pan para mantener el punto central
    Point newCenterGeo = screenToGeo(centerX, centerY);
    panOffset.x += (centerGeo.x - newCenterGeo.x);
    panOffset.y += (centerGeo.y - newCenterGeo.y);
}

void Renderer::zoomOut(int centerX, int centerY) {
    // Guardar el punto central antes del zoom
    Point centerGeo = screenToGeo(centerX, centerY);

    // Aplicar zoom
    zoom /= 1.2;
    if (zoom < 0.1) zoom = 0.1;

    // Ajustar pan para mantener el punto central
    Point newCenterGeo = screenToGeo(centerX, centerY);
    panOffset.x += (centerGeo.x - newCenterGeo.x);
    panOffset.y += (centerGeo.y - newCenterGeo.y);
}

void Renderer::render(HDC hdc, const std::vector<Geometry>& geometries) {
    // Limpiar fondo
    RECT rect = {0, 0, width, height};
    HBRUSH bgBrush = CreateSolidBrush(colorBackground);
    FillRect(memDC, &rect, bgBrush);
    DeleteObject(bgBrush);

    // Renderizar geometrías
    for (const auto& geom : geometries) {
        renderGeometry(memDC, geom, false);
    }

    // Copiar al DC real
    BitBlt(hdc, 0, 0, width, height, memDC, 0, 0, SRCCOPY);
}

void Renderer::renderGeometry(HDC hdc, const Geometry& geom, bool highlight) {
    HPEN pen = CreatePen(PS_SOLID, highlight ? 3 : 1,
                        highlight ? colorHighlight : colorStreet);
    HPEN oldPen = (HPEN)SelectObject(hdc, pen);

    if (geom.type == GEOM_POINT && !geom.points.empty()) {
        POINT p = geoToScreen(geom.points[0]);
        Ellipse(hdc, p.x - 3, p.y - 3, p.x + 3, p.y + 3);

    } else if (geom.type == GEOM_LINESTRING && geom.points.size() > 1) {
        POINT start = geoToScreen(geom.points[0]);
        MoveToEx(hdc, start.x, start.y, NULL);

        for (size_t i = 1; i < geom.points.size(); i++) {
            POINT p = geoToScreen(geom.points[i]);
            LineTo(hdc, p.x, p.y);
        }

    } else if (geom.type == GEOM_POLYGON && geom.points.size() > 2) {
        std::vector<POINT> points;
        for (const auto& p : geom.points) {
            points.push_back(geoToScreen(p));
        }

        // Usar Polygon de WinAPI para polígonos
        HBRUSH brush = CreateSolidBrush(highlight ? RGB(255, 200, 200) : RGB(200, 200, 200));
        HBRUSH oldBrush = (HBRUSH)SelectObject(hdc, brush);

        Polygon(hdc, points.data(), (int)points.size());

        SelectObject(hdc, oldBrush);
        DeleteObject(brush);
    }

    SelectObject(hdc, oldPen);
    DeleteObject(pen);
}

void Renderer::renderRTreeNodes(HDC hdc, RTreeNode* node, int level) {
    if (!node) return;

    // Color según nivel (más transparente para niveles bajos)
    int colorIntensity = 100 + level * 30;
    if (colorIntensity > 255) colorIntensity = 255;

    COLORREF color = RGB(0, colorIntensity, 255 - level * 20);
    HPEN pen = CreatePen(PS_DASH, 1, color);
    HPEN oldPen = (HPEN)SelectObject(hdc, pen);
    HBRUSH oldBrush = (HBRUSH)SelectObject(hdc, GetStockObject(NULL_BRUSH));

    // Renderizar MBR del nodo
    POINT tl = geoToScreen(Point(node->mbr.minX, node->mbr.maxY));
    POINT br = geoToScreen(Point(node->mbr.maxX, node->mbr.minY));
    Rectangle(hdc, tl.x, tl.y, br.x, br.y);

    // Renderizar hijos recursivamente
    if (!node->isLeaf) {
        for (auto* child : node->children) {
            renderRTreeNodes(hdc, child, level + 1);
        }
    }

    SelectObject(hdc, oldBrush);
    SelectObject(hdc, oldPen);
    DeleteObject(pen);
}

void Renderer::renderSearchArea(HDC hdc, const Rect& area) {
    HPEN pen = CreatePen(PS_SOLID, 2, colorSearchArea);
    HPEN oldPen = (HPEN)SelectObject(hdc, pen);
    HBRUSH oldBrush = (HBRUSH)SelectObject(hdc, GetStockObject(NULL_BRUSH));

    POINT tl = geoToScreen(Point(area.minX, area.maxY));
    POINT br = geoToScreen(Point(area.maxX, area.minY));
    Rectangle(hdc, tl.x, tl.y, br.x, br.y);

    SelectObject(hdc, oldBrush);
    SelectObject(hdc, oldPen);
    DeleteObject(pen);
}

void Renderer::renderSearchResults(HDC hdc, const std::vector<Geometry*>& results) {
    for (auto* geom : results) {
        renderGeometry(hdc, *geom, true);
    }
}

void Renderer::renderGraph(HDC hdc, const Graph& graph) {
    const auto& nodes = graph.getNodes();

    // Renderizar aristas del grafo
    HPEN edgePen = CreatePen(PS_SOLID, 1, RGB(150, 150, 200));
    HPEN oldPen = (HPEN)SelectObject(hdc, edgePen);

    for (const auto& pair : nodes) {
        const GraphNode& node = pair.second;
        POINT p1 = geoToScreen(node.position);

        for (size_t i = 0; i < node.neighbors.size(); i++) {
            int neighborId = node.neighbors[i];
            const GraphNode* neighbor = graph.getNode(neighborId);
            if (neighbor && neighbor->id > node.id) {  // Evitar dibujar dos veces
                POINT p2 = geoToScreen(neighbor->position);
                MoveToEx(hdc, p1.x, p1.y, NULL);
                LineTo(hdc, p2.x, p2.y);
            }
        }
    }

    SelectObject(hdc, oldPen);
    DeleteObject(edgePen);

    // Renderizar nodos del grafo
    HBRUSH nodeBrush = CreateSolidBrush(RGB(100, 100, 255));
    HBRUSH oldBrush = (HBRUSH)SelectObject(hdc, nodeBrush);
    HPEN nodePen = CreatePen(PS_SOLID, 1, RGB(50, 50, 150));
    oldPen = (HPEN)SelectObject(hdc, nodePen);

    for (const auto& pair : nodes) {
        POINT p = geoToScreen(pair.second.position);
        Ellipse(hdc, p.x - 2, p.y - 2, p.x + 2, p.y + 2);
    }

    SelectObject(hdc, oldBrush);
    SelectObject(hdc, oldPen);
    DeleteObject(nodeBrush);
    DeleteObject(nodePen);
}

void Renderer::renderRoute(HDC hdc, const Route& route) {
    if (!route.found || route.path.size() < 2) return;

    // Renderizar línea de la ruta en AZUL (como solicitaste)
    HPEN routePen = CreatePen(PS_SOLID, 5, RGB(0, 100, 255));  // AZUL GRUESO
    HPEN oldPen = (HPEN)SelectObject(hdc, routePen);

    POINT start = geoToScreen(route.path[0]);
    MoveToEx(hdc, start.x, start.y, NULL);

    for (size_t i = 1; i < route.path.size(); i++) {
        POINT p = geoToScreen(route.path[i]);
        LineTo(hdc, p.x, p.y);
    }

    SelectObject(hdc, oldPen);
    DeleteObject(routePen);

    // Renderizar puntos de la ruta en AMARILLO para destacar
    HBRUSH pointBrush = CreateSolidBrush(RGB(255, 200, 0));
    HBRUSH oldBrush = (HBRUSH)SelectObject(hdc, pointBrush);
    HPEN pointPen = CreatePen(PS_SOLID, 2, RGB(200, 150, 0));
    oldPen = (HPEN)SelectObject(hdc, pointPen);

    for (const auto& point : route.path) {
        POINT p = geoToScreen(point);
        Ellipse(hdc, p.x - 5, p.y - 5, p.x + 5, p.y + 5);
    }

    SelectObject(hdc, oldBrush);
    SelectObject(hdc, oldPen);
    DeleteObject(pointBrush);
    DeleteObject(pointPen);
}

void Renderer::renderRoutePoint(HDC hdc, const Point& p, bool isStart) {
    POINT screen = geoToScreen(p);

    // Color según si es inicio o fin
    COLORREF color = isStart ? RGB(0, 255, 0) : RGB(255, 0, 0);  // Verde inicio, Rojo fin
    HBRUSH brush = CreateSolidBrush(color);
    HBRUSH oldBrush = (HBRUSH)SelectObject(hdc, brush);
    HPEN pen = CreatePen(PS_SOLID, 3, RGB(0, 0, 0));
    HPEN oldPen = (HPEN)SelectObject(hdc, pen);

    // Dibujar círculo más grande y visible
    Ellipse(hdc, screen.x - 10, screen.y - 10, screen.x + 10, screen.y + 10);

    // Dibujar letra (S o E)
    SetBkMode(hdc, TRANSPARENT);
    SetTextColor(hdc, RGB(255, 255, 255));
    HFONT font = CreateFont(16, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE,
                            DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                            ANTIALIASED_QUALITY, DEFAULT_PITCH | FF_DONTCARE, "Arial");
    HFONT oldFont = (HFONT)SelectObject(hdc, font);

    const char* letter = isStart ? "S" : "E";
    TextOut(hdc, screen.x - 5, screen.y - 8, letter, 1);

    SelectObject(hdc, oldFont);
    SelectObject(hdc, oldBrush);
    SelectObject(hdc, oldPen);
    DeleteObject(font);
    DeleteObject(brush);
    DeleteObject(pen);
}
