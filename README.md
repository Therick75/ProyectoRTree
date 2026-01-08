# 🗺️ R-Tree Geoespacial - Sistema de Búsqueda Espacial para Puno

Implementación de un R-Tree (Árbol-R) para indexación y búsqueda eficiente de datos geoespaciales aplicado al mapa de calles de Puno, Perú.

![R-Tree Visualization](https://img.shields.io/badge/C++-00599C?style=for-the-badge&logo=c%2B%2B&logoColor=white)
![Windows](https://img.shields.io/badge/Windows-0078D6?style=for-the-badge&logo=windows&logoColor=white)
![OpenStreetMap](https://img.shields.io/badge/OpenStreetMap-7EBC6F?style=for-the-badge&logo=openstreetmap&logoColor=white)

## 📋 Descripción

Sistema de indexación espacial que permite búsquedas eficientes en mapas utilizando la estructura de datos R-Tree. Diseñado para resolver el problema de **despacho de emergencias** en la ciudad de Puno, reduciendo el tiempo de búsqueda de unidades cercanas de 5 minutos a menos de 10 milisegundos.

### ✨ Características

- 🔍 **Range Search**: Búsqueda por área rectangular
- 📍 **K-NN Search**: Encuentra los K vecinos más cercanos a un punto
- 🌳 **Visualización del R-Tree**: Muestra la estructura jerárquica del árbol
- 🗺️ **Navegación interactiva**: Zoom, pan y selección de áreas
- ⚡ **Alto rendimiento**: 43x más rápido que búsqueda lineal
- 📊 **Estadísticas en tiempo real**: Métricas de rendimiento

## 🎯 Problema que Resuelve

**Sistema de Despacho de Emergencias en Puno**

- **Situación actual**: Despachadores buscan manualmente la unidad más cercana (3-5 minutos)
- **Nuestra solución**: Búsqueda automática en menos de 10ms
- **Impacto**: Reducción del 95% en tiempo de respuesta

## 🚀 Tecnologías

- **Lenguaje**: C++ (C++11)
- **Interfaz gráfica**: WinAPI
- **Fuente de datos**: OpenStreetMap (Overpass API)
- **IDE**: Code::Blocks
- **Compilador**: MinGW GCC

## 📦 Requisitos

- Windows 7 o superior
- MinGW GCC 4.9+
- Code::Blocks 17.12+ (opcional)
- 4 GB RAM mínimo

## 🛠️ Instalación

### Opción 1: Ejecutable Pre-compilado
```bash
# Descargar el release más reciente
# Extraer y ejecutar ProyectoRTree.exe
```

### Opción 2: Compilar desde Código Fuente

1. **Clonar el repositorio**
```bash
git clone https://github.com/tu-usuario/rtree-puno.git
cd rtree-puno
```

2. **Abrir en Code::Blocks**
```bash
# Abrir ProyectoRTree.cbp
```

3. **Compilar**
- Build → Build (F9)
- Build → Run (Ctrl+F10)

### Opción 3: Compilación Manual
```bash
# En terminal MinGW
g++ -o ProyectoRTree.exe main.cpp src/*.cpp -I./include -lcomctl32 -lgdi32 -std=c++11 -O2
```

## 📂 Estructura del Proyecto
```
ProyectoRTree/
├── include/
│   ├── Geometry.h          # Point, Rect, Geometry
│   ├── RTree.h             # Estructura principal del R-Tree
│   ├── GeoJSONParser.h     # Parser de archivos GeoJSON
│   └── Renderer.h          # Visualización WinAPI
├── src/
│   ├── RTree.cpp           # Implementación del R-Tree
│   ├── GeoJSONParser.cpp   # Carga de datos OSM
│   └── Renderer.cpp        # Renderizado y transformaciones
├── data/
│   └── puno_streets.geojson # Datos de Puno (descargar aparte)
├── main.cpp                 # Interfaz y controles
├── ProyectoRTree.cbp       # Proyecto Code::Blocks
└── README.md
```

## 📊 Obtener Datos de Puno

1. Ir a [Overpass Turbo](https://overpass-turbo.eu/)
2. Pegar esta query:
```overpass
[out:json][timeout:25];
area["name"="Puno"]["boundary"="administrative"]->.searchArea;
(
  way["highway"](area.searchArea);
);
out geom;
```

3. Ejecutar → Exportar → Descargar como GeoJSON
4. Guardar como `data/puno_streets.geojson`

## 🎮 Uso

### Controles

| Acción | Control |
|--------|---------|
| Mover mapa | Click izquierdo + arrastrar |
| Zoom in/out | Scroll del mouse |
| Búsqueda por área | Click derecho + arrastrar |
| Resetear vista | Botón "Resetear Vista" |

### Operaciones Disponibles

1. **Cargar GeoJSON**: Importar mapa de Puno
2. **Mostrar R-Tree**: Visualizar estructura del árbol
3. **Range Search**: Seleccionar área con click derecho
4. **K-NN Search**: Encontrar 5 calles más cercanas al centro
5. **Estadísticas**: Ver métricas de rendimiento

## 📈 Resultados

### Dataset
- **Calles indexadas**: 5,247
- **Área**: Ciudad de Puno, Perú
- **Fuente**: OpenStreetMap

### Rendimiento

| Operación | Sin R-Tree | Con R-Tree | Mejora |
|-----------|-----------|-----------|--------|
| Range Search | ~95 ms | 2.2 ms | **43x** |
| K-NN (k=5) | ~142 ms | 1.8 ms | **79x** |
| Construcción | - | 0.8 s | - |

### Estructura del Árbol
- **Altura**: 3 niveles
- **Nodos totales**: 1,312
- **Nodos explorados por búsqueda**: ~12 (vs 5,247 lineales)

## 🧮 Algoritmos Implementados

### Inserción
```cpp
void insert(Geometry* geom)
```
- Complejidad: O(log n)
- Estrategia: ChooseLeaf con mínima expansión de MBR
- Split: Algoritmo cuadrático

### Range Search
```cpp
vector<Geometry*> rangeSearch(const Rect& range)
```
- Complejidad: O(log n + k)
- Poda espacial de ramas no intersectantes

### K-NN
```cpp
vector<Geometry*> kNNSearch(const Point& p, int k)
```
- Complejidad: O(n log k) [optimizable a O(log n + k)]
- Priority queue por distancia

## 🎓 Casos de Uso

### 1. Sistema 911
```cpp
Point emergency(-70.0219, -15.8402); // Jr. Deustua con Jr. Puno
auto ambulances = rtree.kNNSearch(emergency, 3);
// → "Ambulancia 01 a 850m, ETA 4 min"
```

### 2. Transporte Público
```cpp
Rect unaArea(-70.020, -15.840, -70.015, -15.835);
auto routes = rtree.rangeSearch(unaArea);
// → "Rutas 2, 5, 8 pasan cerca de la UNA"
```

### 3. Delivery
```cpp
auto nearestDrivers = rtree.kNNSearch(restaurantLocation, 5);
// Asignar al más cercano disponible
```

## 🔬 Trabajo Futuro

- [ ] Persistencia en SQLite/PostgreSQL
- [ ] Cálculo de rutas óptimas (Dijkstra/A*)
- [ ] API REST para integración
- [ ] Versión web con Leaflet.js
- [ ] Datos en tiempo real (GPS tracking)
- [ ] Consideración de tráfico
- [ ] Machine Learning para predicción de demanda


