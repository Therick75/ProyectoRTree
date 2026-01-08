#include <windows.h>
#include <commctrl.h>
#include <string>
#include <sstream>
#include <chrono>
#include "../include/Geometry.h"
#include "../include/RTree.h"
#include "../include/GeoJSONParser.h"
#include "../include/Renderer.h"
#include "../include/Graph.h"

// Variables globales
HINSTANCE hInst;
HWND hwndMain, hwndStatus, hwndToolbar;
GeoJSONParser parser;
RTree rtree;
Graph roadGraph;
Renderer* renderer = nullptr;
std::vector<Geometry*> searchResults;
bool showRTreeNodes = false;
bool showGraph = false;
bool isDragging = false;
POINT lastMousePos;
Rect searchArea;
bool isDrawingSearchArea = false;

// Modo de selección de ruta
enum RouteMode {
    ROUTE_NONE,
    ROUTE_SELECT_START,
    ROUTE_SELECT_END
};

RouteMode routeMode = ROUTE_NONE;
Point routeStart, routeEnd;
bool hasRouteStart = false, hasRouteEnd = false;
Route currentRoute;

// Estadísticas de rendimiento
struct Stats {
    int totalGeometries;
    int treeHeight;
    int nodeCount;
    double loadTime;
    double lastSearchTime;
    int lastResultCount;
    int graphNodes;
    int graphEdges;
    double routeDistance;
    double routeTime;
} stats;

// Declaraciones de funciones
LRESULT CALLBACK WndProc(HWND, UINT, WPARAM, LPARAM);
void InitializeApplication(HWND hwnd);
void LoadGeoJSONFile(HWND hwnd);
void PerformRangeSearch(HWND hwnd, const Rect& range);
void PerformKNNSearch(HWND hwnd, const Point& p, int k);
void UpdateStatusBar();
void ShowStatistics(HWND hwnd);
void CreateToolbar(HWND hwnd);
void BuildGraph(HWND hwnd);
void StartRouteSelection(HWND hwnd);
void CalculateRoute(HWND hwnd);
void ClearRoute();

// Entrada principal
int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance,
                   LPSTR lpCmdLine, int nCmdShow) {
    hInst = hInstance;

    InitCommonControls();

    // Registrar clase de ventana
    WNDCLASSEX wc = {0};
    wc.cbSize = sizeof(WNDCLASSEX);
    wc.style = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInstance;
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    wc.lpszClassName = "RTreeViewer";
    wc.hIcon = LoadIcon(NULL, IDI_APPLICATION);
    wc.hIconSm = LoadIcon(NULL, IDI_APPLICATION);

    if (!RegisterClassEx(&wc)) {
        MessageBox(NULL, "Error al registrar la clase de ventana", "Error", MB_OK | MB_ICONERROR);
        return 0;
    }

    // Crear ventana principal
    hwndMain = CreateWindowEx(
        0,
        "RTreeViewer",
        "Sistema de Rutas Optimas - Puno (R-Tree + A*)",
        WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT, CW_USEDEFAULT,
        1400, 900,
        NULL, NULL, hInstance, NULL
    );

    if (!hwndMain) {
        MessageBox(NULL, "Error al crear la ventana", "Error", MB_OK | MB_ICONERROR);
        return 0;
    }

    ShowWindow(hwndMain, nCmdShow);
    UpdateWindow(hwndMain);

    // Loop de mensajes
    MSG msg;
    while (GetMessage(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    return (int)msg.wParam;
}

LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
        case WM_CREATE:
            InitializeApplication(hwnd);
            break;

        case WM_SIZE: {
            int width = LOWORD(lParam);
            int height = HIWORD(lParam);

            SendMessage(hwndToolbar, TB_AUTOSIZE, 0, 0);

            RECT rcToolbar;
            GetWindowRect(hwndToolbar, &rcToolbar);
            int toolbarHeight = rcToolbar.bottom - rcToolbar.top;

            SendMessage(hwndStatus, WM_SIZE, 0, 0);

            RECT rcStatus;
            GetWindowRect(hwndStatus, &rcStatus);
            int statusHeight = rcStatus.bottom - rcStatus.top;

            if (renderer) delete renderer;
            renderer = new Renderer(hwnd, width, height - toolbarHeight - statusHeight);

            if (!parser.getGeometries().empty()) {
                renderer->setViewBounds(parser.getBounds());
            }

            InvalidateRect(hwnd, NULL, TRUE);
            break;
        }

        case WM_PAINT: {
            PAINTSTRUCT ps;
            HDC hdc = BeginPaint(hwnd, &ps);

            if (renderer) {
                renderer->render(hdc, parser.getGeometries());

                if (showGraph && roadGraph.getNodeCount() > 0) {
                    renderer->renderGraph(hdc, roadGraph);
                }

                if (showRTreeNodes && rtree.getRoot()) {
                    renderer->renderRTreeNodes(hdc, rtree.getRoot(), 0);
                }

                if (!searchResults.empty()) {
                    renderer->renderSearchResults(hdc, searchResults);
                }

                if (isDrawingSearchArea) {
                    renderer->renderSearchArea(hdc, searchArea);
                }

                // Renderizar ruta y puntos
                if (currentRoute.found) {
                    renderer->renderRoute(hdc, currentRoute);
                }
                if (hasRouteStart) {
                    renderer->renderRoutePoint(hdc, routeStart, true);
                }
                if (hasRouteEnd) {
                    renderer->renderRoutePoint(hdc, routeEnd, false);
                }
            }

            EndPaint(hwnd, &ps);
            break;
        }

        case WM_LBUTTONDOWN: {
            int x = LOWORD(lParam);
            int y = HIWORD(lParam);

            if (routeMode == ROUTE_SELECT_START) {
                routeStart = renderer->getGeoPoint(x, y);
                hasRouteStart = true;
                routeMode = ROUTE_SELECT_END;
                SetWindowText(hwndStatus, "Seleccione el punto FINAL (clic izquierdo)");
                InvalidateRect(hwnd, NULL, TRUE);
            } else if (routeMode == ROUTE_SELECT_END) {
                routeEnd = renderer->getGeoPoint(x, y);
                hasRouteEnd = true;
                routeMode = ROUTE_NONE;
                CalculateRoute(hwnd);
                InvalidateRect(hwnd, NULL, TRUE);
            } else {
                isDragging = true;
                lastMousePos.x = x;
                lastMousePos.y = y;
                SetCapture(hwnd);
            }
            break;
        }

        case WM_LBUTTONUP: {
            isDragging = false;
            ReleaseCapture();
            break;
        }

        case WM_RBUTTONDOWN: {
            if (routeMode == ROUTE_NONE) {
                isDrawingSearchArea = true;
                POINT p;
                p.x = LOWORD(lParam);
                p.y = HIWORD(lParam);
                Point geo = renderer->getGeoPoint(p.x, p.y);
                searchArea.minX = searchArea.maxX = geo.x;
                searchArea.minY = searchArea.maxY = geo.y;
                SetCapture(hwnd);
            }
            break;
        }

        case WM_RBUTTONUP: {
            if (isDrawingSearchArea) {
                isDrawingSearchArea = false;
                ReleaseCapture();

                if (searchArea.minX > searchArea.maxX) {
                    double temp = searchArea.minX;
                    searchArea.minX = searchArea.maxX;
                    searchArea.maxX = temp;
                }
                if (searchArea.minY > searchArea.maxY) {
                    double temp = searchArea.minY;
                    searchArea.minY = searchArea.maxY;
                    searchArea.maxY = temp;
                }

                PerformRangeSearch(hwnd, searchArea);
                InvalidateRect(hwnd, NULL, TRUE);
            }
            break;
        }

        case WM_MOUSEMOVE: {
            int x = LOWORD(lParam);
            int y = HIWORD(lParam);

            if (isDragging && renderer) {
                int dx = x - lastMousePos.x;
                int dy = y - lastMousePos.y;
                renderer->pan(dx, dy);
                lastMousePos.x = x;
                lastMousePos.y = y;
                InvalidateRect(hwnd, NULL, TRUE);
            }

            if (isDrawingSearchArea && renderer) {
                Point geo = renderer->getGeoPoint(x, y);
                searchArea.maxX = geo.x;
                searchArea.maxY = geo.y;
                InvalidateRect(hwnd, NULL, TRUE);
            }
            break;
        }

        case WM_MOUSEWHEEL: {
            int delta = GET_WHEEL_DELTA_WPARAM(wParam);
            POINT p;
            p.x = LOWORD(lParam);
            p.y = HIWORD(lParam);
            ScreenToClient(hwnd, &p);

            if (renderer) {
                if (delta > 0) {
                    renderer->zoomIn(p.x, p.y);
                } else {
                    renderer->zoomOut(p.x, p.y);
                }
                InvalidateRect(hwnd, NULL, TRUE);
            }
            break;
        }

        case WM_COMMAND: {
            switch (LOWORD(wParam)) {
                case 1: // Cargar archivo
                    LoadGeoJSONFile(hwnd);
                    break;
                case 2: // Mostrar/ocultar nodos R-Tree
                    showRTreeNodes = !showRTreeNodes;
                    InvalidateRect(hwnd, NULL, TRUE);
                    break;
                case 3: // Resetear vista
                    if (renderer && !parser.getGeometries().empty()) {
                        renderer->resetView();
                        renderer->setViewBounds(parser.getBounds());
                        InvalidateRect(hwnd, NULL, TRUE);
                    }
                    break;
                case 4: // Mostrar estadísticas
                    ShowStatistics(hwnd);
                    break;
                case 5: // Búsqueda K-NN
                    if (!parser.getGeometries().empty()) {
                        Point center = parser.getBounds().center();
                        PerformKNNSearch(hwnd, center, 5);
                    }
                    break;
                case 6: // Construir grafo
                    BuildGraph(hwnd);
                    break;
                case 7: // Mostrar/ocultar grafo
                    showGraph = !showGraph;
                    InvalidateRect(hwnd, NULL, TRUE);
                    break;
                case 8: // Seleccionar ruta
                    StartRouteSelection(hwnd);
                    break;
                case 9: // Limpiar ruta
                    ClearRoute();
                    InvalidateRect(hwnd, NULL, TRUE);
                    break;
            }
            break;
        }

        case WM_DESTROY:
            if (renderer) delete renderer;
            PostQuitMessage(0);
            break;

        default:
            return DefWindowProc(hwnd, msg, wParam, lParam);
    }
    return 0;
}

void InitializeApplication(HWND hwnd) {
    CreateToolbar(hwnd);

    hwndStatus = CreateWindowEx(
        0, STATUSCLASSNAME, NULL,
        WS_CHILD | WS_VISIBLE | SBARS_SIZEGRIP,
        0, 0, 0, 0,
        hwnd, NULL, hInst, NULL
    );

    stats.totalGeometries = 0;
    stats.treeHeight = 0;
    stats.nodeCount = 0;
    stats.loadTime = 0;
    stats.lastSearchTime = 0;
    stats.lastResultCount = 0;
    stats.graphNodes = 0;
    stats.graphEdges = 0;
    stats.routeDistance = 0;
    stats.routeTime = 0;

    UpdateStatusBar();
}

void CreateToolbar(HWND hwnd) {
    hwndToolbar = CreateWindowEx(
        0, TOOLBARCLASSNAME, NULL,
        WS_CHILD | WS_VISIBLE,
        0, 0, 0, 0,
        hwnd, NULL, hInst, NULL
    );

    SendMessage(hwndToolbar, TB_BUTTONSTRUCTSIZE, sizeof(TBBUTTON), 0);

    TBBUTTON buttons[] = {
        {0, 1, TBSTATE_ENABLED, TBSTYLE_BUTTON, {0}, 0, (INT_PTR)"Cargar GeoJSON"},
        {0, 2, TBSTATE_ENABLED, TBSTYLE_CHECK, {0}, 0, (INT_PTR)"R-Tree"},
        {0, 3, TBSTATE_ENABLED, TBSTYLE_BUTTON, {0}, 0, (INT_PTR)"Reset Vista"},
        {0, 4, TBSTATE_ENABLED, TBSTYLE_BUTTON, {0}, 0, (INT_PTR)"Estadisticas"},
        {0, 5, TBSTATE_ENABLED, TBSTYLE_BUTTON, {0}, 0, (INT_PTR)"K-NN (5)"},
        {0, 0, TBSTATE_ENABLED, TBSTYLE_SEP, {0}, 0, 0},
        {0, 6, TBSTATE_ENABLED, TBSTYLE_BUTTON, {0}, 0, (INT_PTR)"Construir Grafo"},
        {0, 7, TBSTATE_ENABLED, TBSTYLE_CHECK, {0}, 0, (INT_PTR)"Ver Grafo"},
        {0, 8, TBSTATE_ENABLED, TBSTYLE_BUTTON, {0}, 0, (INT_PTR)"Calcular Ruta"},
        {0, 9, TBSTATE_ENABLED, TBSTYLE_BUTTON, {0}, 0, (INT_PTR)"Limpiar Ruta"}
    };

    SendMessage(hwndToolbar, TB_ADDBUTTONS, 10, (LPARAM)buttons);
    SendMessage(hwndToolbar, TB_AUTOSIZE, 0, 0);
}

void LoadGeoJSONFile(HWND hwnd) {
    OPENFILENAME ofn = {0};
    char szFile[260] = {0};

    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = hwnd;
    ofn.lpstrFile = szFile;
    ofn.nMaxFile = sizeof(szFile);
    ofn.lpstrFilter = "GeoJSON Files\0*.geojson;*.json\0All Files\0*.*\0";
    ofn.nFilterIndex = 1;
    ofn.Flags = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST;

    if (GetOpenFileName(&ofn)) {
        auto start = std::chrono::high_resolution_clock::now();

        if (parser.loadFromFile(szFile)) {
            rtree.clear();
            searchResults.clear();
            roadGraph.clear();
            ClearRoute();

            const std::vector<Geometry>& geoms = parser.getGeometries();
            for (size_t i = 0; i < geoms.size(); i++) {
                rtree.insert(const_cast<Geometry*>(&geoms[i]));
            }

            auto end = std::chrono::high_resolution_clock::now();
            stats.loadTime = std::chrono::duration<double>(end - start).count();
            stats.totalGeometries = geoms.size();
            stats.treeHeight = rtree.getHeight();
            stats.nodeCount = rtree.getNodeCount();

            if (renderer) {
                renderer->setViewBounds(parser.getBounds());
            }

            UpdateStatusBar();
            InvalidateRect(hwnd, NULL, TRUE);

            std::stringstream ss;
            ss << "Cargadas " << stats.totalGeometries
               << " geometrias en " << stats.loadTime << " segundos\n"
               << "Use 'Construir Grafo' para habilitar rutas";

            MessageBox(hwnd, ss.str().c_str(), "Exito", MB_OK | MB_ICONINFORMATION);
        } else {
            MessageBox(hwnd, "Error al cargar el archivo GeoJSON", "Error", MB_OK | MB_ICONERROR);
        }
    }
}

void BuildGraph(HWND hwnd) {
    if (parser.getGeometries().empty()) {
        MessageBox(hwnd, "Primero cargue un archivo GeoJSON", "Aviso", MB_OK | MB_ICONWARNING);
        return;
    }

    auto start = std::chrono::high_resolution_clock::now();

    roadGraph.buildFromGeometries(parser.getGeometries());

    auto end = std::chrono::high_resolution_clock::now();
    double buildTime = std::chrono::duration<double>(end - start).count();

    stats.graphNodes = roadGraph.getNodeCount();
    stats.graphEdges = roadGraph.getEdgeCount();

    UpdateStatusBar();
    InvalidateRect(hwnd, NULL, TRUE);

    std::stringstream ss;
    ss << "Grafo construido exitosamente:\n"
       << "Nodos: " << stats.graphNodes << "\n"
       << "Aristas: " << stats.graphEdges << "\n"
       << "Tiempo: " << buildTime << " segundos\n\n"
       << "Ahora puede calcular rutas";

    MessageBox(hwnd, ss.str().c_str(), "Grafo Construido", MB_OK | MB_ICONINFORMATION);
}

void StartRouteSelection(HWND hwnd) {
    if (roadGraph.getNodeCount() == 0) {
        MessageBox(hwnd, "Primero construya el grafo de rutas", "Aviso", MB_OK | MB_ICONWARNING);
        return;
    }

    ClearRoute();
    routeMode = ROUTE_SELECT_START;
    SetWindowText(hwndStatus, "Seleccione el punto INICIAL (clic izquierdo)");
}

void CalculateRoute(HWND hwnd) {
    auto start = std::chrono::high_resolution_clock::now();

    // Usar A* para encontrar la ruta
    currentRoute = roadGraph.findAStarPath(routeStart, routeEnd);

    auto end = std::chrono::high_resolution_clock::now();
    stats.routeTime = std::chrono::duration<double>(end - start).count() * 1000;

    if (currentRoute.found) {
        stats.routeDistance = currentRoute.totalDistance;

        std::stringstream ss;
        ss << "Ruta encontrada:\n"
           << "Distancia: " << stats.routeDistance << " unidades\n"
           << "Nodos visitados: " << currentRoute.path.size() << "\n"
           << "Tiempo de calculo: " << stats.routeTime << " ms";

        MessageBox(hwnd, ss.str().c_str(), "Ruta Calculada", MB_OK | MB_ICONINFORMATION);
    } else {
        MessageBox(hwnd, "No se encontro una ruta entre los puntos seleccionados",
                   "Ruta no encontrada", MB_OK | MB_ICONWARNING);
    }

    UpdateStatusBar();
}

void ClearRoute() {
    hasRouteStart = false;
    hasRouteEnd = false;
    routeMode = ROUTE_NONE;
    currentRoute = Route();
    stats.routeDistance = 0;
    stats.routeTime = 0;
}

void PerformRangeSearch(HWND hwnd, const Rect& range) {
    auto start = std::chrono::high_resolution_clock::now();

    searchResults.clear();
    searchResults = rtree.rangeSearch(range);

    auto end = std::chrono::high_resolution_clock::now();
    stats.lastSearchTime = std::chrono::duration<double>(end - start).count() * 1000;
    stats.lastResultCount = searchResults.size();

    UpdateStatusBar();
}

void PerformKNNSearch(HWND hwnd, const Point& p, int k) {
    auto start = std::chrono::high_resolution_clock::now();

    searchResults = rtree.kNNSearch(p, k);

    auto end = std::chrono::high_resolution_clock::now();
    stats.lastSearchTime = std::chrono::duration<double>(end - start).count() * 1000;
    stats.lastResultCount = searchResults.size();

    UpdateStatusBar();
    InvalidateRect(hwnd, NULL, TRUE);
}

void UpdateStatusBar() {
    std::stringstream ss;
    ss << "Geometrias: " << stats.totalGeometries
       << " | Altura R-Tree: " << stats.treeHeight
       << " | Nodos: " << stats.nodeCount;

    if (stats.graphNodes > 0) {
        ss << " | Grafo: " << stats.graphNodes << " nodos, "
           << stats.graphEdges << " aristas";
    }

    if (stats.lastResultCount > 0) {
        ss << " | Busqueda: " << stats.lastResultCount
           << " (" << stats.lastSearchTime << " ms)";
    }

    if (currentRoute.found) {
        ss << " | Ruta: " << stats.routeDistance << " unidades ("
           << stats.routeTime << " ms)";
    }

    SetWindowText(hwndStatus, ss.str().c_str());
}

void ShowStatistics(HWND hwnd) {
    std::stringstream ss;
    ss << "=== Estadisticas del Sistema ===" << "\n\n"
       << "--- R-Tree ---" << "\n"
       << "Total de geometrias: " << stats.totalGeometries << "\n"
       << "Altura del arbol: " << stats.treeHeight << "\n"
       << "Numero de nodos: " << stats.nodeCount << "\n"
       << "Tiempo de carga: " << stats.loadTime << " segundos\n\n"
       << "--- Grafo de Rutas ---" << "\n"
       << "Nodos del grafo: " << stats.graphNodes << "\n"
       << "Aristas: " << stats.graphEdges << "\n\n"
       << "--- Ultima Busqueda ---" << "\n"
       << "Resultados: " << stats.lastResultCount << "\n"
       << "Tiempo: " << stats.lastSearchTime << " ms\n\n"
       << "--- Ruta Actual ---" << "\n";

    if (currentRoute.found) {
        ss << "Distancia: " << stats.routeDistance << " unidades\n"
           << "Nodos en ruta: " << currentRoute.path.size() << "\n"
           << "Tiempo de calculo: " << stats.routeTime << " ms\n";
    } else {
        ss << "No hay ruta calculada\n";
    }

    MessageBox(hwnd, ss.str().c_str(), "Estadisticas", MB_OK | MB_ICONINFORMATION);
}
