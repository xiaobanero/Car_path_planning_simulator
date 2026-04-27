#include <SDL.h>
#include <SDL_ttf.h>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstring>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <functional>
#include <map>
#include <memory>
#include <queue>
#include <set>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <commdlg.h>
#endif

namespace {

constexpr int WINDOW_WIDTH = 1620;
constexpr int WINDOW_HEIGHT = 900;
constexpr int CANVAS_WIDTH = 1200;
constexpr int CANVAS_HEIGHT = 800;
constexpr int SIDEBAR_WIDTH = 420;
constexpr int GRID_SIZE = 40;
constexpr int COLUMNS = CANVAS_WIDTH / GRID_SIZE;
constexpr int ROWS = CANVAS_HEIGHT / GRID_SIZE;

constexpr SDL_Color BACKGROUND_COLOR = {232, 237, 247, 255};
constexpr SDL_Color SIDEBAR_COLOR = {236, 240, 248, 255};
constexpr SDL_Color GRID_COLOR = {208, 214, 228, 255};
constexpr SDL_Color PATH_GRAY = {110, 118, 132, 255};
constexpr SDL_Color NODE_COLOR = PATH_GRAY;
constexpr int NODE_RADIUS = 6;
constexpr int PATH_LINE_HALF_WIDTH = 5;
constexpr SDL_Color HOVER_COLOR = {120, 160, 235, 255};
constexpr SDL_Color CAR_WAIT_COLOR = {230, 45, 45, 255};
constexpr int CAR_HIT_R = 12;
constexpr SDL_Color WAIT_TEXT_COLOR = {255, 0, 0, 255};
constexpr SDL_Color TEXT_COLOR = {255, 255, 255, 255};
constexpr SDL_Color SIDEBAR_BORDER_COLOR = {165, 176, 198, 255};
constexpr SDL_Color SIDEBAR_CARD_COLOR = {248, 250, 255, 255};
constexpr SDL_Color SIDEBAR_CARD_BORDER = {196, 204, 222, 255};
constexpr SDL_Color PANEL_TITLE_COLOR = {46, 58, 82, 255};
constexpr SDL_Color PANEL_BODY_COLOR = {73, 84, 106, 255};
constexpr SDL_Color BUTTON_COLOR = {121, 137, 171, 255};
constexpr SDL_Color BUTTON_HOVER_COLOR = {137, 153, 190, 255};
constexpr SDL_Color BUTTON_ACTIVE_COLOR = {86, 162, 116, 255};
constexpr SDL_Color BUTTON_TEXT_COLOR = {248, 250, 255, 255};

constexpr int LOWER_PANEL_X = CANVAS_WIDTH + 8;
constexpr int LOWER_PANEL_W = SIDEBAR_WIDTH - 16;
constexpr int MAIN_CONTROLS_BOTTOM_Y = 426 + 38;
constexpr int NONE_MODE_LOWER_TOP = MAIN_CONTROLS_BOTTOM_Y + 8;
constexpr int BUILDING_TITLE_Y = NONE_MODE_LOWER_TOP + 10;
constexpr int BUILDING_BTN_Y0 = BUILDING_TITLE_Y + 30;
constexpr int BUILDING_BTN_ROW_H = 44;
constexpr int BUILDING_BTN_H = 34;
constexpr int BUILDING_BTN_GAP_X = 8;
constexpr int BUILDING_BTN_W = (LOWER_PANEL_W - BUILDING_BTN_GAP_X - 12) / 2;
constexpr int BUILDING_ZONE_BOTTOM = BUILDING_BTN_Y0 + 4 * BUILDING_BTN_ROW_H;
constexpr int BUILDING_MODE_LOWER_TOP = BUILDING_ZONE_BOTTOM + 8;
constexpr int MONITOR_WIN_W = 940;
constexpr int MONITOR_WIN_H = 620;
constexpr int MONITOR_ROW_H = 58;
constexpr int MONITOR_LIST_TOP = 118;
constexpr const char* SAVE_DIR = "res_map";

enum Mode : int { MODE_NONE = 0, MODE_SET_START = 1, MODE_SET_END = 2 };

void setDrawColor(SDL_Renderer* r, const SDL_Color& c) {
  SDL_SetRenderDrawColor(r, c.r, c.g, c.b, c.a);
}

void appendDebugLog(const char* run_id, const char* hypothesis_id, const char* location, const char* message,
                    const char* data_json) {
  std::ofstream dbg("debug-8f043c.log", std::ios::app);
  if (!dbg) return;
  auto ts = std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::system_clock::now().time_since_epoch())
                .count();
  dbg << "{\"sessionId\":\"8f043c\",\"runId\":\"" << run_id << "\",\"hypothesisId\":\"" << hypothesis_id
      << "\",\"location\":\"" << location << "\",\"message\":\"" << message << "\",\"data\":" << data_json
      << ",\"timestamp\":" << ts << "}\n";
}

#ifdef _WIN32
bool callGetSaveFileNameA(OPENFILENAMEA* ofn) {
  HMODULE h = LoadLibraryA("comdlg32.dll");
  if (!h) return false;
  using Fn = BOOL(WINAPI*)(LPOPENFILENAMEA);
  auto fn = reinterpret_cast<Fn>(GetProcAddress(h, "GetSaveFileNameA"));
  bool ok = (fn && fn(ofn));
  FreeLibrary(h);
  return ok;
}

bool callGetOpenFileNameA(OPENFILENAMEA* ofn) {
  HMODULE h = LoadLibraryA("comdlg32.dll");
  if (!h) return false;
  using Fn = BOOL(WINAPI*)(LPOPENFILENAMEA);
  auto fn = reinterpret_cast<Fn>(GetProcAddress(h, "GetOpenFileNameA"));
  bool ok = (fn && fn(ofn));
  FreeLibrary(h);
  return ok;
}
#endif

void fillCircle(SDL_Renderer* r, int cx, int cy, int radius) {
  for (int dy = -radius; dy <= radius; ++dy) {
    for (int dx = -radius; dx <= radius; ++dx) {
      if (dx * dx + dy * dy <= radius * radius) {
        SDL_RenderDrawPoint(r, cx + dx, cy + dy);
      }
    }
  }
}

void fillCircleF(SDL_Renderer* r, float cx, float cy, float radius) {
  const int ir = static_cast<int>(std::ceil(radius)) + 1;
  for (int dy = -ir; dy <= ir; ++dy) {
    for (int dx = -ir; dx <= ir; ++dx) {
      if (dx * dx + dy * dy <= radius * radius) {
        SDL_RenderDrawPoint(r, static_cast<int>(cx + dx), static_cast<int>(cy + dy));
      }
    }
  }
}

const std::vector<std::pair<int, int>>& buildingFootprint(int preset_id) {
  static const std::vector<std::pair<int, int>> shapes[8] = {
      {{0, 0}},
      {{0, 0}, {1, 0}},
      {{0, 0}, {0, 1}},
      {{0, 0}, {1, 0}, {0, 1}, {1, 1}},
      {{0, 0}, {1, 0}, {2, 0}, {0, 1}, {1, 1}, {2, 1}},
      {{0, 0}, {0, 1}, {0, 2}, {1, 0}, {1, 1}, {1, 2}},
      {{0, 0}, {1, 0}, {0, 1}},
      {{0, 0}, {1, 0}, {2, 0}, {1, 1}, {1, 2}},
  };
  static const std::vector<std::pair<int, int>> fb{{0, 0}};
  if (preset_id < 0 || preset_id >= 8) return fb;
  return shapes[preset_id];
}

struct BuildingStyle {
  SDL_Color base;
  SDL_Color trim;
  const char* name;
};

const BuildingStyle& buildingStyle(int preset_id) {
  static const BuildingStyle styles[8] = {
      {{190, 130, 95}, {140, 85, 55}, "岗亭"},
      {{105, 135, 175}, {70, 100, 140}, "横向库"},
      {{135, 165, 115}, {95, 125, 80}, "竖向楼"},
      {{155, 145, 185}, {115, 105, 150}, "方块楼"},
      {{165, 140, 110}, {125, 100, 75}, "厂区"},
      {{115, 120, 165}, {80, 85, 125}, "高塔"},
      {{150, 155, 120}, {110, 115, 85}, "L型馆"},
      {{125, 150, 160}, {90, 110, 120}, "T型楼"},
  };
  return styles[preset_id < 0 || preset_id >= 8 ? 0 : preset_id];
}

void drawPathEdge(SDL_Renderer* r, float x0, float y0, float x1, float y1, int half_width,
                  const SDL_Color& col) {
  const int ix0 = static_cast<int>(x0);
  const int iy0 = static_cast<int>(y0);
  const int ix1 = static_cast<int>(x1);
  const int iy1 = static_cast<int>(y1);
  int dx = ix1 - ix0;
  int dy = iy1 - iy0;
  int len2 = dx * dx + dy * dy;
  float nx = 0.f, ny = 1.f;
  if (len2 > 0) {
    const float len = std::sqrt(static_cast<float>(len2));
    nx = -dy / len;
    ny = dx / len;
  }
  SDL_SetRenderDrawColor(r, col.r, col.g, col.b, col.a);
  for (int k = -half_width; k <= half_width; ++k) {
    const int ox = static_cast<int>(nx * static_cast<float>(k) + (k >= 0 ? 0.5f : -0.5f));
    const int oy = static_cast<int>(ny * static_cast<float>(k) + (k >= 0 ? 0.5f : -0.5f));
    SDL_RenderDrawLine(r, ix0 + ox, iy0 + oy, ix1 + ox, iy1 + oy);
  }
}

void fillConvexPoly(SDL_Renderer* r, const std::vector<SDL_FPoint>& pts, SDL_Color fill) {
  if (pts.size() < 3) return;
  float cx = 0, cy = 0;
  for (const auto& p : pts) {
    cx += p.x;
    cy += p.y;
  }
  cx /= static_cast<float>(pts.size());
  cy /= static_cast<float>(pts.size());
  SDL_Vertex tri[3];
  tri[0].color = {fill.r, fill.g, fill.b, fill.a};
  tri[0].tex_coord = {0, 0};
  tri[1] = tri[0];
  tri[2] = tri[0];
  tri[0].position = {cx, cy};
  for (size_t i = 0; i < pts.size(); ++i) {
    size_t j = (i + 1) % pts.size();
    tri[1].position = pts[i];
    tri[2].position = pts[j];
    SDL_RenderGeometry(r, nullptr, tri, 3, nullptr, 0);
  }
}

SDL_Color hashBrightBody(Uint32 seed, int salt) {
  Uint32 x = seed + static_cast<Uint32>(salt) * 0x9E3779B9u;
  x ^= x >> 17;
  x *= 2246822519u;
  return {static_cast<Uint8>(65u + (x & 165u)), static_cast<Uint8>(65u + ((x >> 9) & 165u)),
          static_cast<Uint8>(65u + ((x >> 18) & 165u)), 255};
}

void drawCarShape(SDL_Renderer* r, float cx, float cy, float heading, SDL_Color body, SDL_Color roof) {
  const float L = GRID_SIZE * 0.88f;
  const float W = GRID_SIZE * 0.42f;
  const float ch = std::cos(heading);
  const float sh = std::sin(heading);
  auto R = [ch, sh, cx, cy](float lx, float ly) {
    return SDL_FPoint{cx + lx * ch - ly * sh, cy + lx * sh + ly * ch};
  };
  std::vector<SDL_FPoint> hull = {
      R(-L * 0.42f, -W * 0.48f), R(L * 0.48f, -W * 0.42f), R(L * 0.52f, -W * 0.08f),
      R(L * 0.45f, W * 0.42f),  R(-L * 0.38f, W * 0.48f), R(-L * 0.5f, W * 0.1f),
      R(-L * 0.48f, -W * 0.12f),
  };
  fillConvexPoly(r, hull, body);
  std::vector<SDL_FPoint> cabin = {R(-L * 0.05f, -W * 0.32f), R(L * 0.32f, -W * 0.28f),
                                    R(L * 0.28f, W * 0.05f), R(-L * 0.08f, W * 0.1f)};
  fillConvexPoly(r, cabin, roof);
  SDL_SetRenderDrawColor(r, 40, 40, 50, 255);
  for (size_t i = 0; i < hull.size(); ++i) {
    size_t j = (i + 1) % hull.size();
    SDL_RenderDrawLine(r, static_cast<int>(hull[i].x), static_cast<int>(hull[i].y),
                       static_cast<int>(hull[j].x), static_cast<int>(hull[j].y));
  }
}

struct PathNode {
  int grid_x = 0;
  int grid_y = 0;
  float x = 0;
  float y = 0;
  std::vector<PathNode*> neighbors;
  bool is_start = false;
  bool is_end = false;
  int start_id = 0;
  int end_id = 0;
  int building_preset = -1;

  PathNode(int gx, int gy) : grid_x(gx), grid_y(gy) {
    x = static_cast<float>(gx * GRID_SIZE + GRID_SIZE / 2);
    y = static_cast<float>(gy * GRID_SIZE + GRID_SIZE / 2);
  }

  void addNeighbor(PathNode* n) {
    if (!n || n == this) return;
    if (std::find(neighbors.begin(), neighbors.end(), n) != neighbors.end()) return;
    neighbors.push_back(n);
    if (std::find(n->neighbors.begin(), n->neighbors.end(), this) == n->neighbors.end())
      n->neighbors.push_back(this);
  }

  void removeNeighbor(PathNode* n) {
    if (!n) return;
    neighbors.erase(std::remove(neighbors.begin(), neighbors.end(), n), neighbors.end());
    n->neighbors.erase(std::remove(n->neighbors.begin(), n->neighbors.end(), this),
                        n->neighbors.end());
  }

  double distanceTo(const PathNode* o) const {
    if (!o) return 0;
    const double dx = static_cast<double>(x - o->x);
    const double dy = static_cast<double>(y - o->y);
    return std::sqrt(dx * dx + dy * dy);
  }

  bool orderLess(const PathNode* o) const {
    if (!o) return false;
    if (grid_x != o->grid_x) return grid_x < o->grid_x;
    return grid_y < o->grid_y;
  }

  bool footprintContains(int gx, int gy) const {
    if (building_preset < 0) return gx == grid_x && gy == grid_y;
    for (const auto& d : buildingFootprint(building_preset)) {
      if (grid_x + d.first == gx && grid_y + d.second == gy) return true;
    }
    return false;
  }

  bool coversGrid(int gx, int gy) const {
    if (!is_start && !is_end) return false;
    return footprintContains(gx, gy);
  }
};

class Graph {
 public:
  std::map<std::pair<int, int>, std::unique_ptr<PathNode>> nodes;
  std::vector<PathNode*> start_nodes;
  std::vector<PathNode*> end_nodes;

  PathNode* cellInsideBuildingFootprintNotAnchor(int gx, int gy) const {
    for (const auto& pr : nodes) {
      PathNode* n = pr.second.get();
      if (n->building_preset < 0) continue;
      if (n->footprintContains(gx, gy) && (gx != n->grid_x || gy != n->grid_y)) return n;
    }
    return nullptr;
  }

  bool cellOccupiedByAnyBuildingFootprint(int gx, int gy) const {
    for (const auto& pr : nodes) {
      PathNode* n = pr.second.get();
      if (n->building_preset < 0) continue;
      if (n->footprintContains(gx, gy)) return true;
    }
    return false;
  }

  bool canPlaceBuildingAt(int anchor_gx, int anchor_gy, int preset_id) const {
    if (preset_id < 0 || preset_id >= 8) preset_id = 0;
    const auto& fp = buildingFootprint(preset_id);
    for (const auto& d : fp) {
      const int gx = anchor_gx + d.first;
      const int gy = anchor_gy + d.second;
      if (gx < 0 || gx >= COLUMNS || gy < 0 || gy >= ROWS) return false;
      if (nodes.find({gx, gy}) != nodes.end()) return false;
      if (cellOccupiedByAnyBuildingFootprint(gx, gy)) return false;
    }
    return true;
  }

  PathNode* addNode(int grid_x, int grid_y) {
    if (cellInsideBuildingFootprintNotAnchor(grid_x, grid_y)) return nullptr;
    const auto key = std::make_pair(grid_x, grid_y);
    auto it = nodes.find(key);
    if (it != nodes.end()) return it->second.get();
    auto node = std::make_unique<PathNode>(grid_x, grid_y);
    PathNode* raw = node.get();
    nodes.emplace(key, std::move(node));
    connectToNeighbors(raw);
    return raw;
  }

  void connectToNeighbors(PathNode* node) {
    const int x = node->grid_x;
    const int y = node->grid_y;
    const int dirs[4][2] = {{-1, 0}, {1, 0}, {0, -1}, {0, 1}};
    for (auto& d : dirs) {
      const int nx = x + d[0];
      const int ny = y + d[1];
      auto it = nodes.find({nx, ny});
      if (it != nodes.end()) node->addNeighbor(it->second.get());
    }
  }

  bool removeNode(int grid_x, int grid_y) {
    const auto key = std::make_pair(grid_x, grid_y);
    auto it = nodes.find(key);
    if (it == nodes.end()) return false;
    PathNode* node = it->second.get();
    if (node->is_start) {
      start_nodes.erase(std::remove(start_nodes.begin(), start_nodes.end(), node), start_nodes.end());
      for (size_t i = 0; i < start_nodes.size(); ++i) start_nodes[i]->start_id = static_cast<int>(i) + 1;
    }
    if (node->is_end) {
      end_nodes.erase(std::remove(end_nodes.begin(), end_nodes.end(), node), end_nodes.end());
      for (size_t i = 0; i < end_nodes.size(); ++i) end_nodes[i]->end_id = static_cast<int>(i) + 1;
    }
    std::vector<PathNode*> copy = node->neighbors;
    for (PathNode* nb : copy) node->removeNeighbor(nb);
    nodes.erase(it);
    return true;
  }

  PathNode* setStartNode(int grid_x, int grid_y) {
    auto it = nodes.find({grid_x, grid_y});
    if (it == nodes.end()) return nullptr;
    PathNode* node = it->second.get();
    if (node->is_start) return node;
    node->is_start = true;
    node->start_id = static_cast<int>(start_nodes.size()) + 1;
    start_nodes.push_back(node);
    return node;
  }

  PathNode* tryPlaceEndBuilding(int anchor_gx, int anchor_gy, int preset_id) {
    if (!canPlaceBuildingAt(anchor_gx, anchor_gy, preset_id)) return nullptr;
    PathNode* raw = addNode(anchor_gx, anchor_gy);
    if (!raw) return nullptr;
    raw->is_end = true;
    raw->building_preset = preset_id;
    raw->end_id = static_cast<int>(end_nodes.size()) + 1;
    end_nodes.push_back(raw);
    return raw;
  }

  PathNode* tryPlaceStartBuilding(int anchor_gx, int anchor_gy, int preset_id) {
    if (!canPlaceBuildingAt(anchor_gx, anchor_gy, preset_id)) return nullptr;
    PathNode* raw = addNode(anchor_gx, anchor_gy);
    if (!raw) return nullptr;
    raw->is_start = true;
    raw->building_preset = preset_id;
    raw->start_id = static_cast<int>(start_nodes.size()) + 1;
    start_nodes.push_back(raw);
    return raw;
  }

  PathNode* findStartOrEndCoveringGrid(int gx, int gy) {
    for (PathNode* s : start_nodes) {
      if (s->coversGrid(gx, gy)) return s;
    }
    for (PathNode* en : end_nodes) {
      if (en->coversGrid(gx, gy)) return en;
    }
    return nullptr;
  }

  bool removeEndCovering(int gx, int gy) {
    PathNode* e = findStartOrEndCoveringGrid(gx, gy);
    if (!e || !e->is_end) return false;
    return removeEndNode(e->grid_x, e->grid_y);
  }

  bool removeStartNode(int grid_x, int grid_y) {
    auto it = nodes.find({grid_x, grid_y});
    if (it == nodes.end()) return false;
    PathNode* node = it->second.get();
    if (!node->is_start) return false;
    node->is_start = false;
    node->building_preset = -1;
    start_nodes.erase(std::remove(start_nodes.begin(), start_nodes.end(), node), start_nodes.end());
    for (size_t i = 0; i < start_nodes.size(); ++i) start_nodes[i]->start_id = static_cast<int>(i) + 1;
    return true;
  }

  bool removeEndNode(int grid_x, int grid_y) {
    auto it = nodes.find({grid_x, grid_y});
    if (it == nodes.end()) return false;
    PathNode* node = it->second.get();
    if (!node->is_end) return false;
    node->is_end = false;
    node->building_preset = -1;
    end_nodes.erase(std::remove(end_nodes.begin(), end_nodes.end(), node), end_nodes.end());
    for (size_t i = 0; i < end_nodes.size(); ++i) end_nodes[i]->end_id = static_cast<int>(i) + 1;
    return true;
  }

  std::vector<PathNode*> dijkstra(PathNode* start_node, PathNode* end_node) {
    if (!start_node || !end_node) return {};
    std::unordered_map<PathNode*, double> dist;
    std::unordered_map<PathNode*, PathNode*> prev;
    for (const auto& p : nodes) {
      dist[p.second.get()] = 1e300;
      prev[p.second.get()] = nullptr;
    }
    if (dist.find(start_node) == dist.end()) return {};
    dist[start_node] = 0;

    using P = std::pair<double, PathNode*>;
    struct Cmp {
      bool operator()(const P& a, const P& b) const { return a.first > b.first; }
    };
    std::priority_queue<P, std::vector<P>, Cmp> heap;
    heap.push({0, start_node});
    std::set<PathNode*> visited;

    while (!heap.empty()) {
      auto [current_dist, current] = heap.top();
      heap.pop();
      if (visited.count(current)) continue;
      visited.insert(current);
      if (current == end_node) {
        std::vector<PathNode*> path;
        for (PathNode* c = current; c; c = prev[c]) path.push_back(c);
        std::reverse(path.begin(), path.end());
        return path;
      }
      for (PathNode* neighbor : current->neighbors) {
        if (visited.count(neighbor)) continue;
        const double nd = current_dist + current->distanceTo(neighbor);
        if (nd < dist[neighbor]) {
          dist[neighbor] = nd;
          prev[neighbor] = current;
          heap.push({nd, neighbor});
        }
      }
    }
    return {};
  }

  PathNode* getNodeAtPixel(int pixel_x, int pixel_y) {
    const int gx = pixel_x / GRID_SIZE;
    const int gy = pixel_y / GRID_SIZE;
    auto it = nodes.find({gx, gy});
    if (it == nodes.end()) return nullptr;
    return it->second.get();
  }

  void clear() {
    nodes.clear();
    start_nodes.clear();
    end_nodes.clear();
  }
};

struct Car {
  static constexpr double kDefaultSpeed = 5.0 / 3.0;

  int id = 0;
  Graph* graph = nullptr;
  PathNode* spawn_start = nullptr;
  float cur_x = 0;
  float cur_y = 0;
  PathNode* target_node = nullptr;
  std::vector<PathNode*> path;
  int path_index = 0;
  double speed = kDefaultSpeed;
  double base_speed = kDefaultSpeed;
  std::string state = "moving";
  Uint32 wait_start_time = 0;
  const Uint32 wait_duration = 1000;
  int target_end_id = 0;
  double priority = 0;
  Uint32 flash_seed = 0;

  Car(int car_id, PathNode* start_node, PathNode* end_node, Graph* g)
      : id(car_id), graph(g), spawn_start(start_node), target_node(end_node) {
    flash_seed = static_cast<Uint32>(car_id) * 4042322161u + 2463534082u;
    if (start_node) {
      cur_x = start_node->x;
      cur_y = start_node->y;
    }
    if (end_node && start_node) {
      path = g->dijkstra(start_node, end_node);
    }
    if (end_node) target_end_id = end_node->end_id;
  }

  void updatePriority() {
    if (path.empty() || path_index >= static_cast<int>(path.size()) - 1) {
      priority = 0;
      return;
    }
    double remaining = 0;
    for (int i = path_index; i < static_cast<int>(path.size()) - 1; ++i)
      remaining += path[i]->distanceTo(path[i + 1]);
    priority = remaining;
  }

  void updatePosition(Uint32 now_ticks, double time_scale) {
    if (time_scale < 0.01) time_scale = 0.01;
    if (state == "waiting") {
      const double scaled_wait = static_cast<double>(now_ticks - wait_start_time) * time_scale;
      if (scaled_wait >= static_cast<double>(wait_duration)) {
        state = "moving";
        speed = base_speed;
      }
      return;
    }
    if (path.empty() || path_index >= static_cast<int>(path.size()) - 1) {
      state = "arrived";
      return;
    }
    PathNode* target = path[path_index + 1];
    const float tx = target->x;
    const float ty = target->y;
    const float dx = tx - cur_x;
    const float dy = ty - cur_y;
    const double distance = std::sqrt(static_cast<double>(dx * dx + dy * dy));
    const double step = speed * time_scale;
    if (distance < step) {
      cur_x = tx;
      cur_y = ty;
      path_index++;
    } else {
      cur_x += static_cast<float>((dx / distance) * step);
      cur_y += static_cast<float>((dy / distance) * step);
    }
  }

  void startWaiting(Uint32 now_ticks) {
    if (state != "waiting") {
      state = "waiting";
      speed = 0;
      wait_start_time = now_ticks;
    }
  }

  PathNode* getClosestNode() {
    double min_d = 1e300;
    PathNode* closest = nullptr;
    for (const auto& p : graph->nodes) {
      PathNode* n = p.second.get();
      const double dx = static_cast<double>(n->x - cur_x);
      const double dy = static_cast<double>(n->y - cur_y);
      const double d = std::sqrt(dx * dx + dy * dy);
      if (d < min_d) {
        min_d = d;
        closest = n;
      }
    }
    if (min_d < GRID_SIZE) return closest;
    return nullptr;
  }

  bool checkPathValidity() {
    bool target_ok = target_node != nullptr;
    if (target_ok) {
      target_ok = false;
      for (const auto& pr : graph->nodes)
        if (pr.second.get() == target_node) {
          target_ok = true;
          break;
        }
    }
    if (path.empty() || !target_ok) {
      state = "stopped";
      std::printf("小车%d：目标终点不存在，停止移动\n", id);
      return false;
    }
    PathNode* current_node = graph->getNodeAtPixel(static_cast<int>(cur_x), static_cast<int>(cur_y));
    if (!current_node) {
      current_node = getClosestNode();
      if (!current_node) {
        state = "stopped";
        std::printf("小车%d：无法找到有效节点，停止移动\n", id);
        return false;
      }
      std::vector<PathNode*> np = graph->dijkstra(current_node, target_node);
      if (!np.empty()) {
        path = std::move(np);
        path_index = 0;
        cur_x = current_node->x;
        cur_y = current_node->y;
        std::printf("小车%d：路径已重新规划\n", id);
        return true;
      }
      state = "stopped";
      std::printf("小车%d：无法找到到终点的路径，停止移动\n", id);
      return false;
    }
    return true;
  }

  double distanceTo(const Car& o) const {
    const double dx = static_cast<double>(cur_x - o.cur_x);
    const double dy = static_cast<double>(cur_y - o.cur_y);
    return std::sqrt(dx * dx + dy * dy);
  }

  float headingAngle() const {
    if (path_index + 1 < static_cast<int>(path.size())) {
      const float dx = path[path_index + 1]->x - cur_x;
      const float dy = path[path_index + 1]->y - cur_y;
      if (dx * dx + dy * dy > 1e-4f) return std::atan2(dy, dx);
    }
    return 0.f;
  }

  void setNewDestination(PathNode* new_end) {
    if (!graph || !new_end || !new_end->is_end) return;
    PathNode* from = graph->getNodeAtPixel(static_cast<int>(cur_x), static_cast<int>(cur_y));
    if (!from) from = getClosestNode();
    if (!from) {
      std::printf("小车%d：无法确定当前节点，不改目的地\n", id);
      return;
    }
    std::vector<PathNode*> np = graph->dijkstra(from, new_end);
    if (np.empty()) {
      std::printf("小车%d：无法规划到新终点\n", id);
      return;
    }
    target_node = new_end;
    target_end_id = new_end->end_id;
    path = std::move(np);
    path_index = 0;
    if (state == "waiting_for_end") state = "moving";
    if (state == "stopped") state = "moving";
    if (state == "waiting") {
      state = "moving";
      speed = base_speed;
    }
    std::printf("小车%d：目的地改为终点%d\n", id, target_end_id);
  }
};

class CarManager {
 public:
  std::vector<std::unique_ptr<Car>> cars;
  int next_car_id = 1;
  Graph* graph = nullptr;
  std::unordered_map<PathNode*, std::vector<Car*>> queue_nodes;

  explicit CarManager(Graph* g) : graph(g) {}

  Car* addCar(PathNode* start_node, PathNode* end_node) {
    auto car = std::make_unique<Car>(next_car_id++, start_node, end_node, graph);
    car->updatePriority();
    Car* raw = car.get();
    cars.push_back(std::move(car));
    return raw;
  }

  void removeCar(Car* car) {
    cars.erase(std::remove_if(cars.begin(), cars.end(),
                              [car](const std::unique_ptr<Car>& c) { return c.get() == car; }),
               cars.end());
  }

  Car* findCarAtPosition(int pixel_x, int pixel_y) {
    for (auto& c : cars) {
      const double dx = static_cast<double>(c->cur_x - pixel_x);
      const double dy = static_cast<double>(c->cur_y - pixel_y);
      if (std::sqrt(dx * dx + dy * dy) < CAR_HIT_R) return c.get();
    }
    return nullptr;
  }

  void checkWaitingCars() {
    for (auto& cp : cars) {
      Car* car = cp.get();
      if (car->state != "waiting_for_end") continue;
      PathNode* target_end = nullptr;
      for (PathNode* en : graph->end_nodes) {
        if (en->end_id == car->target_end_id) {
          target_end = en;
          break;
        }
      }
      if (!target_end) continue;
      PathNode* from = car->spawn_start;
      if (!from && !car->path.empty()) from = car->path[0];
      if (!from) continue;
      car->target_node = target_end;
      car->path = graph->dijkstra(from, target_end);
      car->path_index = 0;
      car->state = "moving";
      std::printf("小车 %d：对应终点出现，开始前往终点%d\n", car->id, target_end->end_id);
    }
  }

  void detectCollisions(Uint32 now_ticks) {
    const size_t n = cars.size();
    for (size_t i = 0; i < n; ++i) {
      Car* c1 = cars[i].get();
      if (c1->state == "stopped" || c1->state == "arrived" || c1->state == "waiting_for_end")
        continue;
      for (size_t j = i + 1; j < n; ++j) {
        Car* c2 = cars[j].get();
        if (c2->state == "stopped" || c2->state == "arrived" || c2->state == "waiting_for_end")
          continue;
        if (c1->distanceTo(*c2) < GRID_SIZE * 2.0) {
          if (c1->id < c2->id)
            c2->startWaiting(now_ticks);
          else
            c1->startWaiting(now_ticks);
        }
      }
    }
  }

  void handleQueues(Uint32 now_ticks) {
    queue_nodes.clear();
    for (auto& cp : cars) {
      Car* car = cp.get();
      if (car->state != "moving" || car->path_index >= static_cast<int>(car->path.size()) - 1)
        continue;
      PathNode* cur = car->path[car->path_index];
      queue_nodes[cur].push_back(car);
    }
    for (auto& kv : queue_nodes) {
      auto& q = kv.second;
      if (q.size() <= 1) continue;
      std::sort(q.begin(), q.end(), [](Car* a, Car* b) { return a->id < b->id; });
      for (size_t k = 1; k < q.size(); ++k) q[k]->startWaiting(now_ticks);
    }
  }

  void cleanupArrivedCars() {
    std::vector<Car*> to_remove;
    for (auto& c : cars)
      if (c->state == "arrived") to_remove.push_back(c.get());
    for (Car* c : to_remove) {
      std::printf("小车 %d 已到达终点，自动移除\n", c->id);
      removeCar(c);
    }
  }

  void updateAllCars(Uint32 now_ticks, double time_scale) {
    checkWaitingCars();
    for (auto& c : cars) {
      if (c->state != "stopped" && c->state != "arrived" && c->state != "waiting_for_end")
        c->checkPathValidity();
    }
    detectCollisions(now_ticks);
    for (auto& c : cars) c->updatePriority();
    for (auto& c : cars) {
      if (c->state == "moving" || c->state == "waiting") c->updatePosition(now_ticks, time_scale);
    }
    handleQueues(now_ticks);
    cleanupArrivedCars();
  }

  void clearAllCars() {
    cars.clear();
    next_car_id = 1;
    queue_nodes.clear();
  }
};

struct Button {
  SDL_Rect rect{};
  std::string text;
  std::function<void()> callback;
  bool is_toggle = false;
  bool is_active = false;
  bool is_hovered = false;
  TTF_Font* font = nullptr;

  Button(int x, int y, int w, int h, std::string t, std::function<void()> cb, bool toggle = false,
         TTF_Font* f = nullptr)
      : text(std::move(t)), callback(std::move(cb)), is_toggle(toggle), font(f) {
    rect = {x, y, w, h};
  }

  void draw(SDL_Renderer* r) {
    SDL_Color color = BUTTON_COLOR;
    if (is_active)
      color = BUTTON_ACTIVE_COLOR;
    else if (is_hovered)
      color = BUTTON_HOVER_COLOR;
    setDrawColor(r, color);
    SDL_RenderFillRect(r, &rect);
    setDrawColor(r, {50, 50, 50, 255});
    SDL_RenderDrawRect(r, &rect);

    if (!font) return;
    SDL_Surface* surf = TTF_RenderUTF8_Blended(font, text.c_str(), BUTTON_TEXT_COLOR);
    if (!surf) return;
    SDL_Texture* tex = SDL_CreateTextureFromSurface(r, surf);
    if (tex) {
      SDL_Rect tr = {rect.x + (rect.w - surf->w) / 2, rect.y + (rect.h - surf->h) / 2, surf->w,
                     surf->h};
      SDL_RenderCopy(r, tex, nullptr, &tr);
      SDL_DestroyTexture(tex);
    }
    SDL_FreeSurface(surf);
  }

  bool handleEvent(const SDL_Event& e) {
    if (e.type == SDL_MOUSEMOTION) {
      is_hovered =
          (e.motion.x >= rect.x && e.motion.x < rect.x + rect.w && e.motion.y >= rect.y &&
           e.motion.y < rect.y + rect.h);
    } else if (e.type == SDL_MOUSEBUTTONDOWN && e.button.button == SDL_BUTTON_LEFT) {
      const int mx = e.button.x;
      const int my = e.button.y;
      if (mx >= rect.x && mx < rect.x + rect.w && my >= rect.y && my < rect.y + rect.h) {
        if (is_toggle) is_active = !is_active;
        if (callback) callback();
        return true;
      }
    }
    return false;
  }

  void setActive(bool a) {
    if (is_toggle) is_active = a;
  }
};

class Game {
 public:
  SDL_Window* window = nullptr;
  SDL_Renderer* renderer = nullptr;
  TTF_Font* font_title = nullptr;
  TTF_Font* font_body = nullptr;
  TTF_Font* font_small = nullptr;
  TTF_Font* font_button = nullptr;
  TTF_Font* id_font = nullptr;
  bool running = true;

  Graph graph;
  CarManager car_manager{&graph};
  Mode current_mode = MODE_NONE;
  int hover_gx = 0;
  int hover_gy = 0;

  bool show_menu = false;
  int menu_x = 0;
  int menu_y = 0;
  PathNode* selected_node = nullptr;

  bool left_button_down = false;
  std::pair<int, int> last_grid{-1, -1};

  std::vector<Button*> buttons;
  std::vector<Button*> building_preset_buttons;
  Button* btn_none = nullptr;
  Button* btn_set_start = nullptr;
  Button* btn_set_end = nullptr;
  Button* btn_reset = nullptr;
  Button* btn_pause = nullptr;
  Button* btn_speed_down = nullptr;
  Button* btn_speed_up = nullptr;
  Button* btn_save = nullptr;
  Button* btn_load_prev = nullptr;
  int selected_building_preset = 0;
  bool paused = false;
  double time_scale = 1.0;
  bool dragging_time_slider = false;
  bool monitor_end_input_active = false;
  int monitor_input_car_id = -1;
  std::string monitor_end_input_text;
  int monitor_selected_start_id = -1;
  SDL_Window* monitor_window = nullptr;
  SDL_Renderer* monitor_renderer = nullptr;
  Uint32 monitor_window_id = 0;

  Uint32 main_window_id = 0;

  SDL_Rect timeSliderTrackRect() const {
    const int button_x = CANVAS_WIDTH + 20;
    const int button_width = SIDEBAR_WIDTH - 40;
    return {button_x + 8, 474, button_width - 16, 10};
  }

  double sliderRatioFromTimeScale() const { return (time_scale - 0.25) / (4.0 - 0.25); }

  void setTimeScaleFromSliderX(int mouse_x) {
    SDL_Rect tr = timeSliderTrackRect();
    double t = static_cast<double>(mouse_x - tr.x) / static_cast<double>(tr.w);
    if (t < 0.0) t = 0.0;
    if (t > 1.0) t = 1.0;
    time_scale = 0.25 + t * (4.0 - 0.25);
    std::printf("时间流速: %.2fx\n", time_scale);
  }

  void destroyMonitorWindow() {
    if (monitor_end_input_active) SDL_StopTextInput();
    monitor_end_input_active = false;
    monitor_input_car_id = -1;
    monitor_end_input_text.clear();
    if (monitor_renderer) {
      SDL_DestroyRenderer(monitor_renderer);
      monitor_renderer = nullptr;
    }
    if (monitor_window) {
      SDL_DestroyWindow(monitor_window);
      monitor_window = nullptr;
    }
    monitor_window_id = 0;
  }

  bool initMonitorWindow() {
    monitor_window = SDL_CreateWindow("小车控制与监控", SDL_WINDOWPOS_CENTERED + 120,
                                      SDL_WINDOWPOS_CENTERED + 70, MONITOR_WIN_W, MONITOR_WIN_H,
                                      SDL_WINDOW_SHOWN);
    if (!monitor_window) return false;
    monitor_renderer =
        SDL_CreateRenderer(monitor_window, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
    if (!monitor_renderer) {
      SDL_DestroyWindow(monitor_window);
      monitor_window = nullptr;
      return false;
    }
    monitor_window_id = SDL_GetWindowID(monitor_window);
    return true;
  }

  std::string resMapDirPath() const {
    return (std::filesystem::current_path() / SAVE_DIR).string();
  }

  bool ensureResMapDir() const {
    std::error_code ec;
    std::filesystem::create_directories(resMapDirPath(), ec);
    return !ec;
  }

  std::string chooseSaveMapPathViaDialog() const {
#ifdef _WIN32
    // #region agent log
    appendDebugLog("run-compile-fix", "H2", "main.cpp:chooseSaveMapPathViaDialog",
                   "enter save file dialog", "{\"api\":\"GetSaveFileNameA\"}");
    // #endregion
    char file_name[MAX_PATH] = "map_state.bin";
    char full_path[MAX_PATH] = {};
    std::string init_dir = resMapDirPath();
    OPENFILENAMEA ofn{};
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = nullptr;
    ofn.lpstrFilter = "地图二进制文件 (*.bin)\0*.bin\0所有文件 (*.*)\0*.*\0";
    ofn.lpstrFile = full_path;
    ofn.nMaxFile = MAX_PATH;
    ofn.lpstrInitialDir = init_dir.c_str();
    ofn.lpstrDefExt = "bin";
    std::snprintf(full_path, MAX_PATH, "%s\\%s", init_dir.c_str(), file_name);
    ofn.Flags = OFN_OVERWRITEPROMPT | OFN_PATHMUSTEXIST;
    if (callGetSaveFileNameA(&ofn)) {
      // #region agent log
      appendDebugLog("run-compile-fix", "H2", "main.cpp:chooseSaveMapPathViaDialog",
                     "save file dialog success", "{\"ok\":true}");
      // #endregion
      return std::string(full_path);
    }
    // #region agent log
    appendDebugLog("run-compile-fix", "H2", "main.cpp:chooseSaveMapPathViaDialog",
                   "save file dialog canceled or failed", "{\"ok\":false}");
    // #endregion
#endif
    return "";
  }

  std::string chooseOpenMapPathViaDialog() const {
#ifdef _WIN32
    // #region agent log
    appendDebugLog("run-compile-fix", "H3", "main.cpp:chooseOpenMapPathViaDialog",
                   "enter open file dialog", "{\"api\":\"GetOpenFileNameA\"}");
    // #endregion
    char full_path[MAX_PATH] = {};
    std::string init_dir = resMapDirPath();
    OPENFILENAMEA ofn{};
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = nullptr;
    ofn.lpstrFilter = "地图二进制文件 (*.bin)\0*.bin\0所有文件 (*.*)\0*.*\0";
    ofn.lpstrFile = full_path;
    ofn.nMaxFile = MAX_PATH;
    ofn.lpstrInitialDir = init_dir.c_str();
    ofn.lpstrDefExt = "bin";
    ofn.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST;
    if (callGetOpenFileNameA(&ofn)) {
      // #region agent log
      appendDebugLog("run-compile-fix", "H3", "main.cpp:chooseOpenMapPathViaDialog",
                     "open file dialog success", "{\"ok\":true}");
      // #endregion
      return std::string(full_path);
    }
    // #region agent log
    appendDebugLog("run-compile-fix", "H3", "main.cpp:chooseOpenMapPathViaDialog",
                   "open file dialog canceled or failed", "{\"ok\":false}");
    // #endregion
#endif
    return "";
  }

  PathNode* findEndById(int end_id) {
    for (PathNode* e : graph.end_nodes)
      if (e->end_id == end_id) return e;
    return nullptr;
  }

  PathNode* findStartById(int start_id) {
    for (PathNode* s : graph.start_nodes)
      if (s->start_id == start_id) return s;
    return nullptr;
  }

  void ensureMonitorStartSelection() {
    if (graph.start_nodes.empty()) {
      monitor_selected_start_id = -1;
      return;
    }
    if (monitor_selected_start_id > 0 && findStartById(monitor_selected_start_id)) return;
    std::vector<PathNode*> starts = graph.start_nodes;
    std::sort(starts.begin(), starts.end(), [](PathNode* a, PathNode* b) { return a->start_id < b->start_id; });
    monitor_selected_start_id = starts.front()->start_id;
  }

  void shiftMonitorStartSelection(int delta) {
    ensureMonitorStartSelection();
    if (graph.start_nodes.empty()) return;
    std::vector<PathNode*> starts = graph.start_nodes;
    std::sort(starts.begin(), starts.end(), [](PathNode* a, PathNode* b) { return a->start_id < b->start_id; });
    int idx = 0;
    for (size_t i = 0; i < starts.size(); ++i) {
      if (starts[i]->start_id == monitor_selected_start_id) {
        idx = static_cast<int>(i);
        break;
      }
    }
    const int n = static_cast<int>(starts.size());
    idx = ((idx + delta) % n + n) % n;
    monitor_selected_start_id = starts[static_cast<size_t>(idx)]->start_id;
  }

  void spawnCarFromMonitorSelectedStart() {
    ensureMonitorStartSelection();
    PathNode* st = findStartById(monitor_selected_start_id);
    if (!st) {
      std::printf("无可用起点，无法生成小车\n");
      return;
    }
    selected_node = st;
    spawnCarFromMenu();
  }

  struct SaveNodeRecord {
    int32_t gx = 0;
    int32_t gy = 0;
    uint8_t flags = 0;  // bit0 start, bit1 end
    int32_t start_id = 0;
    int32_t end_id = 0;
    int32_t building_preset = -1;
  };

  bool saveMapToFile(const char* file_path) {
    std::vector<SaveNodeRecord> recs;
    recs.reserve(graph.nodes.size());
    for (const auto& pr : graph.nodes) {
      PathNode* n = pr.second.get();
      SaveNodeRecord r;
      r.gx = n->grid_x;
      r.gy = n->grid_y;
      r.flags = static_cast<uint8_t>((n->is_start ? 1 : 0) | (n->is_end ? 2 : 0));
      r.start_id = n->start_id;
      r.end_id = n->end_id;
      r.building_preset = n->building_preset;
      recs.push_back(r);
    }
    std::ofstream out(file_path, std::ios::binary | std::ios::trunc);
    if (!out) return false;
    const char magic[8] = {'P', 'P', 'M', 'A', 'P', '0', '1', '\0'};
    uint32_t count = static_cast<uint32_t>(recs.size());
    out.write(magic, sizeof(magic));
    out.write(reinterpret_cast<const char*>(&count), sizeof(count));
    if (!recs.empty())
      out.write(reinterpret_cast<const char*>(recs.data()),
                static_cast<std::streamsize>(recs.size() * sizeof(SaveNodeRecord)));
    return out.good();
  }

  bool loadMapFromFile(const char* file_path) {
    std::ifstream in(file_path, std::ios::binary);
    if (!in) return false;
    char magic[8] = {};
    uint32_t count = 0;
    in.read(magic, sizeof(magic));
    in.read(reinterpret_cast<char*>(&count), sizeof(count));
    const char expected[8] = {'P', 'P', 'M', 'A', 'P', '0', '1', '\0'};
    if (std::memcmp(magic, expected, sizeof(magic)) != 0) return false;
    std::vector<SaveNodeRecord> recs(count);
    if (count > 0)
      in.read(reinterpret_cast<char*>(recs.data()),
              static_cast<std::streamsize>(recs.size() * sizeof(SaveNodeRecord)));
    if (!in.good() && count > 0) return false;

    graph.clear();
    car_manager.clearAllCars();
    selected_node = nullptr;
    monitor_end_input_active = false;
    monitor_input_car_id = -1;
    monitor_end_input_text.clear();
    SDL_StopTextInput();

    for (const auto& r : recs) {
      if (r.gx < 0 || r.gx >= COLUMNS || r.gy < 0 || r.gy >= ROWS) continue;
      graph.addNode(r.gx, r.gy);
    }
    std::vector<std::pair<int, PathNode*>> starts;
    std::vector<std::pair<int, PathNode*>> ends;
    for (const auto& r : recs) {
      auto it = graph.nodes.find({r.gx, r.gy});
      if (it == graph.nodes.end()) continue;
      PathNode* n = it->second.get();
      n->building_preset = r.building_preset;
      n->is_start = false;
      n->is_end = false;
      n->start_id = 0;
      n->end_id = 0;
      if (r.flags & 1) starts.push_back({r.start_id, n});
      if (r.flags & 2) ends.push_back({r.end_id, n});
    }
    std::sort(starts.begin(), starts.end(),
              [](const auto& a, const auto& b) { return a.first < b.first; });
    std::sort(ends.begin(), ends.end(), [](const auto& a, const auto& b) { return a.first < b.first; });
    graph.start_nodes.clear();
    graph.end_nodes.clear();
    int sid = 1;
    for (auto& s : starts) {
      s.second->is_start = true;
      s.second->start_id = sid++;
      graph.start_nodes.push_back(s.second);
    }
    int eid = 1;
    for (auto& e : ends) {
      e.second->is_end = true;
      e.second->end_id = eid++;
      graph.end_nodes.push_back(e.second);
    }
    ensureMonitorStartSelection();
    return true;
  }

  void saveCurrentMap() {
    if (!ensureResMapDir()) {
      std::printf("无法创建目录: %s\n", SAVE_DIR);
      return;
    }
    std::string save_path = chooseSaveMapPathViaDialog();
    if (save_path.empty()) return;
    if (saveMapToFile(save_path.c_str()))
      std::printf("已保存: %s\n", save_path.c_str());
    else
      std::printf("保存失败: %s\n", save_path.c_str());
  }

  void loadPreviousMap() {
    if (!ensureResMapDir()) {
      std::printf("无法创建目录: %s\n", SAVE_DIR);
      return;
    }
    std::string open_path = chooseOpenMapPathViaDialog();
    if (open_path.empty()) return;
    if (loadMapFromFile(open_path.c_str()))
      std::printf("已打开: %s\n", open_path.c_str());
    else
      std::printf("打开失败: %s\n", open_path.c_str());
  }

  void applyMonitorEndInput() {
    if (!monitor_end_input_active || monitor_input_car_id <= 0) return;
    if (monitor_end_input_text.empty()) return;
    int end_id = std::atoi(monitor_end_input_text.c_str());
    if (end_id <= 0) return;
    PathNode* en = findEndById(end_id);
    if (!en) {
      std::printf("无效终点编号: %d\n", end_id);
      return;
    }
    for (auto& p : car_manager.cars) {
      Car* c = p.get();
      if (c->id == monitor_input_car_id) {
        c->setNewDestination(en);
        return;
      }
    }
  }

  void togglePause() {
    paused = !paused;
    if (btn_pause) btn_pause->text = paused ? "继续运行" : "全局暂停";
    std::printf(paused ? "已全局暂停\n" : "已继续运行\n");
  }

  void adjustTimeScale(double delta) {
    time_scale += delta;
    if (time_scale < 0.25) time_scale = 0.25;
    if (time_scale > 4.0) time_scale = 4.0;
    std::printf("时间流速: %.2fx\n", time_scale);
  }

  PathNode* cyclicEndByDelta(Car* car, int delta) {
    if (graph.end_nodes.empty()) return nullptr;
    std::vector<PathNode*> ends = graph.end_nodes;
    std::sort(ends.begin(), ends.end(),
              [](PathNode* a, PathNode* b) { return a->end_id < b->end_id; });
    int idx = 0;
    for (size_t i = 0; i < ends.size(); ++i) {
      if (ends[i]->end_id == car->target_end_id || ends[i] == car->target_node) {
        idx = static_cast<int>(i);
        break;
      }
    }
    const int n = static_cast<int>(ends.size());
    idx = ((idx + delta) % n + n) % n;
    return ends[static_cast<size_t>(idx)];
  }

  void handleCarMonitorPanelClick(int local_x, int local_y) {
    constexpr int headY = 42;
    constexpr int headH = 30;
    constexpr int prevStartX = 320;
    constexpr int nextStartX = 452;
    constexpr int spawnX = 584;
    constexpr int startBtnW = 122;
    constexpr int spawnW = 120;
    if (local_y >= headY && local_y < headY + headH) {
      if (local_x >= prevStartX && local_x < prevStartX + startBtnW) {
        shiftMonitorStartSelection(-1);
        return;
      }
      if (local_x >= nextStartX && local_x < nextStartX + startBtnW) {
        shiftMonitorStartSelection(1);
        return;
      }
      if (local_x >= spawnX && local_x < spawnX + spawnW) {
        spawnCarFromMonitorSelectedStart();
        return;
      }
    }

    std::vector<Car*> list;
    list.reserve(car_manager.cars.size());
    for (auto& p : car_manager.cars) list.push_back(p.get());
    std::sort(list.begin(), list.end(), [](Car* a, Car* b) { return a->id < b->id; });
    const int n = static_cast<int>(list.size());
    if (n == 0) return;
    if (local_y < MONITOR_LIST_TOP) return;
    const int row = (local_y - MONITOR_LIST_TOP) / MONITOR_ROW_H;
    if (row < 0 || row >= n) return;
    Car* car = list[static_cast<size_t>(row)];
    const int rowY = MONITOR_LIST_TOP + row * MONITOR_ROW_H;
    constexpr int btnH = 28;
    constexpr int btnW = 44;
    constexpr int inputW = 72;
    constexpr int resetW = 88;
    constexpr int targetW = 112;
    const int btnY = rowY + 24;
    const int inputX = MONITOR_WIN_W - 528;
    const int resetX = MONITOR_WIN_W - 450;
    const int speedDownX = MONITOR_WIN_W - 356;
    const int speedUpX = MONITOR_WIN_W - 306;
    const int prevX = MONITOR_WIN_W - 250;
    const int nextX = MONITOR_WIN_W - 132;
    if (local_y < btnY || local_y >= btnY + btnH) return;
    if (local_x >= inputX && local_x < inputX + inputW) {
      monitor_end_input_active = true;
      monitor_input_car_id = car->id;
      monitor_end_input_text = std::to_string(car->target_end_id);
      SDL_StartTextInput();
    } else if (local_x >= resetX && local_x < resetX + resetW) {
      car->base_speed = Car::kDefaultSpeed;
      if (car->state == "moving") car->speed = car->base_speed;
    } else if (local_x >= speedDownX && local_x < speedDownX + btnW) {
      car->base_speed = std::max(0.2, car->base_speed - 0.2);
      if (car->state == "moving") car->speed = car->base_speed;
    } else if (local_x >= speedUpX && local_x < speedUpX + btnW) {
      car->base_speed = std::min(8.0, car->base_speed + 0.2);
      if (car->state == "moving") car->speed = car->base_speed;
    } else if (local_x >= prevX && local_x < prevX + targetW && !graph.end_nodes.empty()) {
      PathNode* en = cyclicEndByDelta(car, -1);
      if (en) car->setNewDestination(en);
    } else if (local_x >= nextX && local_x < nextX + targetW && !graph.end_nodes.empty()) {
      PathNode* en = cyclicEndByDelta(car, 1);
      if (en) car->setNewDestination(en);
    }
  }

  void drawMonitorWindow() {
    if (!monitor_renderer || !font_body) return;
    ensureMonitorStartSelection();
    setDrawColor(monitor_renderer, {246, 248, 253, 255});
    SDL_RenderClear(monitor_renderer);
    const SDL_Color title_c = PANEL_TITLE_COLOR;
    const SDL_Color body_c = PANEL_BODY_COLOR;
    const SDL_Color btn_bg = {186, 194, 210, 255};
    blitUtf8(monitor_renderer, 12, 10, "小车坐标与控制", font_title ? font_title : font_body, title_c);
    blitUtf8(monitor_renderer, 12, 42, "每车可调速度与终点（上一个终点/下一个终点）",
             font_small ? font_small : font_body, body_c);
    char sbuf[96];
    std::snprintf(sbuf, sizeof(sbuf), "当前起点: %s", monitor_selected_start_id > 0 ? "" : "无");
    blitUtf8(monitor_renderer, 12, 74, sbuf, font_small ? font_small : font_body, body_c);
    if (monitor_selected_start_id > 0) {
      char sid[32];
      std::snprintf(sid, sizeof(sid), "S%d", monitor_selected_start_id);
      blitUtf8(monitor_renderer, 96, 74, sid, font_small ? font_small : font_body, title_c);
    }
    constexpr int headY = 42;
    constexpr int headH = 30;
    constexpr int prevStartX = 320;
    constexpr int nextStartX = 452;
    constexpr int spawnX = 584;
    constexpr int startBtnW = 122;
    constexpr int spawnW = 120;
    SDL_Rect hprev = {prevStartX, headY, startBtnW, headH};
    SDL_Rect hnext = {nextStartX, headY, startBtnW, headH};
    SDL_Rect hspawn = {spawnX, headY, spawnW, headH};
    setDrawColor(monitor_renderer, btn_bg);
    SDL_RenderFillRect(monitor_renderer, &hprev);
    SDL_RenderFillRect(monitor_renderer, &hnext);
    SDL_RenderFillRect(monitor_renderer, &hspawn);
    setDrawColor(monitor_renderer, {70, 76, 94, 255});
    SDL_RenderDrawRect(monitor_renderer, &hprev);
    SDL_RenderDrawRect(monitor_renderer, &hnext);
    SDL_RenderDrawRect(monitor_renderer, &hspawn);
    if (monitor_selected_start_id > 0) {
      setDrawColor(monitor_renderer, {248, 186, 68, 255});
      SDL_Rect hl = {prevStartX - 2, headY - 2, startBtnW * 2 + 6, headH + 4};
      SDL_RenderDrawRect(monitor_renderer, &hl);
    }
    blitUtf8(monitor_renderer, prevStartX + 10, headY + 5, "上一个起点", font_small ? font_small : font_body,
             title_c);
    blitUtf8(monitor_renderer, nextStartX + 10, headY + 5, "下一个起点", font_small ? font_small : font_body,
             title_c);
    blitUtf8(monitor_renderer, spawnX + 10, headY + 5, "生成小车", font_small ? font_small : font_body,
             title_c);
    if (monitor_end_input_active) {
      char ibuf[96];
      std::snprintf(ibuf, sizeof(ibuf), "输入终点编号 车%d: %s (回车确认 Esc取消)", monitor_input_car_id,
                    monitor_end_input_text.c_str());
      blitUtf8(monitor_renderer, 12, 62, ibuf, font_small ? font_small : font_body, {84, 62, 150, 255});
    }

    std::vector<Car*> list;
    list.reserve(car_manager.cars.size());
    for (auto& p : car_manager.cars) list.push_back(p.get());
    std::sort(list.begin(), list.end(), [](Car* a, Car* b) { return a->id < b->id; });

    char line[256];
    int y = MONITOR_LIST_TOP;
    if (list.empty()) {
      blitUtf8(monitor_renderer, 12, y, "当前无小车", font_body, body_c);
    } else {
      for (Car* car : list) {
        const char* st = "其它";
        if (car->state == "moving") st = "行驶";
        else if (car->state == "waiting") st = "等待";
        else if (car->state == "waiting_for_end") st = "等终点";
        else if (car->state == "stopped") st = "停止";
        else if (car->state == "arrived") st = "到达";
        if (monitor_end_input_active && car->id == monitor_input_car_id) {
          setDrawColor(monitor_renderer, {248, 186, 68, 255});
          SDL_Rect row_hl = {8, y + 1, MONITOR_WIN_W - 16, MONITOR_ROW_H - 4};
          SDL_RenderDrawRect(monitor_renderer, &row_hl);
        }
        std::snprintf(line, sizeof(line), "车%-3d X:%.1f Y:%.1f 速:%.2f 终点:%d 状态:%s", car->id,
                      static_cast<double>(car->cur_x), static_cast<double>(car->cur_y), car->base_speed,
                      car->target_end_id, st);
        blitUtf8(monitor_renderer, 12, y + 4, line, font_small ? font_small : font_body, body_c);
        constexpr int btnH = 28;
        constexpr int btnW = 44;
        constexpr int inputW = 72;
        constexpr int resetW = 88;
        constexpr int targetW = 112;
        const int btnY = y + 24;
        const int inputX = MONITOR_WIN_W - 528;
        const int resetX = MONITOR_WIN_W - 450;
        const int speedDownX = MONITOR_WIN_W - 356;
        const int speedUpX = MONITOR_WIN_W - 306;
        const int prevX = MONITOR_WIN_W - 250;
        const int nextX = MONITOR_WIN_W - 132;
        SDL_Rect rin = {inputX, btnY, inputW, btnH};
        SDL_Rect rrs = {resetX, btnY, resetW, btnH};
        SDL_Rect rsd = {speedDownX, btnY, btnW, btnH};
        SDL_Rect rsu = {speedUpX, btnY, btnW, btnH};
        SDL_Rect rprev = {prevX, btnY, targetW, btnH};
        SDL_Rect rnext = {nextX, btnY, targetW, btnH};
        setDrawColor(monitor_renderer, btn_bg);
        SDL_RenderFillRect(monitor_renderer, &rin);
        SDL_RenderFillRect(monitor_renderer, &rrs);
        SDL_RenderFillRect(monitor_renderer, &rsd);
        SDL_RenderFillRect(monitor_renderer, &rsu);
        SDL_RenderFillRect(monitor_renderer, &rprev);
        SDL_RenderFillRect(monitor_renderer, &rnext);
        setDrawColor(monitor_renderer, {70, 76, 94, 255});
        SDL_RenderDrawRect(monitor_renderer, &rin);
        SDL_RenderDrawRect(monitor_renderer, &rrs);
        SDL_RenderDrawRect(monitor_renderer, &rsd);
        SDL_RenderDrawRect(monitor_renderer, &rsu);
        SDL_RenderDrawRect(monitor_renderer, &rprev);
        SDL_RenderDrawRect(monitor_renderer, &rnext);
        blitUtf8(monitor_renderer, inputX + 8, btnY + 5, "终点编号", font_small ? font_small : font_body,
                 title_c);
        blitUtf8(monitor_renderer, resetX + 8, btnY + 5, "恢复默认速度", font_small ? font_small : font_body,
                 title_c);
        blitUtf8(monitor_renderer, speedDownX + 14, btnY + 5, "-", font_body, title_c);
        blitUtf8(monitor_renderer, speedUpX + 14, btnY + 5, "+", font_body, title_c);
        blitUtf8(monitor_renderer, prevX + 8, btnY + 5, "上一个终点", font_small ? font_small : font_body,
                 title_c);
        blitUtf8(monitor_renderer, nextX + 8, btnY + 5, "下一个终点", font_small ? font_small : font_body,
                 title_c);
        y += MONITOR_ROW_H;
        if (y > MONITOR_WIN_H - 52) break;
      }
    }
    SDL_RenderPresent(monitor_renderer);
  }

  bool init() {
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER) < 0) {
      std::fprintf(stderr, "SDL_Init 失败: %s\n", SDL_GetError());
      return false;
    }
    if (TTF_Init() < 0) {
      std::fprintf(stderr, "TTF_Init 失败: %s\n", TTF_GetError());
      SDL_Quit();
      return false;
    }
    window = SDL_CreateWindow("路径规划模拟 - 无模式", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
                              WINDOW_WIDTH, WINDOW_HEIGHT, SDL_WINDOW_SHOWN);
    if (!window) {
      std::fprintf(stderr, "SDL_CreateWindow 失败: %s\n", SDL_GetError());
      TTF_Quit();
      SDL_Quit();
      return false;
    }
    renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
    if (!renderer) {
      std::fprintf(stderr, "SDL_CreateRenderer 失败: %s\n", SDL_GetError());
      SDL_DestroyWindow(window);
      window = nullptr;
      TTF_Quit();
      SDL_Quit();
      return false;
    }
    font_title = loadChineseFont(24);
    font_body = loadChineseFont(20);
    font_small = loadChineseFont(17);
    font_button = loadChineseFont(18);
    if (!font_body) font_body = loadChineseFont(20);
    if (!font_title) font_title = font_body;
    if (!font_small) font_small = font_body;
    if (!font_button) font_button = font_body;
    id_font = font_body;

    initButtons();
    initBuildingPresetButtons();
    main_window_id = SDL_GetWindowID(window);
    initMonitorWindow();
    return true;
  }

  void selectBuildingPreset(int idx) {
    if (idx < 0 || idx >= 8) idx = 0;
    selected_building_preset = idx;
    for (size_t k = 0; k < building_preset_buttons.size(); ++k)
      building_preset_buttons[k]->is_active = (static_cast<int>(k) == idx);
  }

  void initBuildingPresetButtons() {
    for (Button* b : building_preset_buttons) delete b;
    building_preset_buttons.clear();
    const int bx0 = LOWER_PANEL_X + 6;
    for (int i = 0; i < 8; ++i) {
      const int row = i / 2;
      const int col = i % 2;
      char label[40];
      std::snprintf(label, sizeof(label), "%d·%s", i + 1, buildingStyle(i).name);
      const int bx = bx0 + col * (BUILDING_BTN_W + BUILDING_BTN_GAP_X);
      const int by = BUILDING_BTN_Y0 + row * BUILDING_BTN_ROW_H;
      auto* bt = new Button(bx, by, BUILDING_BTN_W, BUILDING_BTN_H, label,
                            [this, i]() { selectBuildingPreset(i); }, false, font_button);
      building_preset_buttons.push_back(bt);
    }
    selectBuildingPreset(0);
  }

  bool canPlaceBuildingPreview(int anchor_gx, int anchor_gy) const {
    return graph.canPlaceBuildingAt(anchor_gx, anchor_gy, selected_building_preset);
  }

  static TTF_Font* loadChineseFont(int size) {
    static const char* paths[] = {
        "C:/Windows/Fonts/msyh.ttc",
        "C:/Windows/Fonts/msyhbd.ttc",
        "C:/Windows/Fonts/simhei.ttf",
        "C:/Windows/Fonts/simsun.ttc",
        "C:/Windows/Fonts/msyh.ttf",
    };
    for (const char* p : paths) {
      TTF_Font* f = TTF_OpenFont(p, size);
      if (f) return f;
    }
    return nullptr;
  }

  void initButtons() {
    const int button_x = CANVAS_WIDTH + 20;
    const int button_width = SIDEBAR_WIDTH - 40;
    const int button_height = 38;

    btn_none = new Button(button_x, 42, button_width, button_height, "无模式",
                          [this] { setModeNone(); }, true, font_button);
    btn_set_start = new Button(button_x, 90, button_width, button_height, "设置起点",
                               [this] { setModeStart(); }, true, font_button);
    btn_set_end = new Button(button_x, 138, button_width, button_height, "设置终点",
                             [this] { setModeEnd(); }, true, font_button);
    btn_reset = new Button(button_x, 186, button_width, button_height, "重置所有",
                           [this] { resetAll(); }, false, font_button);
    btn_pause = new Button(button_x, 234, button_width, button_height, "全局暂停",
                           [this] { togglePause(); }, false, font_button);
    btn_speed_down =
        new Button(button_x, 282, (button_width - 12) / 2, button_height, "减速 -",
                   [this] { adjustTimeScale(-0.25); }, false, font_button);
    btn_speed_up = new Button(button_x + (button_width - 12) / 2 + 12, 282, (button_width - 12) / 2,
                              button_height, "加速 +", [this] { adjustTimeScale(0.25); }, false,
                              font_button);
    btn_save = new Button(button_x, 330, button_width, button_height, "保存当前",
                          [this] { saveCurrentMap(); }, false, font_button);
    btn_load_prev = new Button(button_x, 378, button_width, button_height, "打开之前",
                               [this] { loadPreviousMap(); }, false, font_button);

    buttons = {btn_none,      btn_set_start, btn_set_end,  btn_reset,     btn_pause,
               btn_speed_down, btn_speed_up,  btn_save,     btn_load_prev};
    btn_none->is_active = true;
  }

  void setModeNone() {
    current_mode = MODE_NONE;
    btn_none->setActive(true);
    btn_none->is_active = true;
    btn_set_start->is_active = false;
    btn_set_end->is_active = false;
    SDL_SetWindowTitle(window, "路径规划模拟 - 无模式");
  }

  void setModeStart() {
    current_mode = MODE_SET_START;
    btn_none->is_active = false;
    btn_set_start->is_active = true;
    btn_set_end->is_active = false;
    selectBuildingPreset(selected_building_preset);
    SDL_SetWindowTitle(window, "路径规划模拟 - 设置起点(建筑物)");
  }

  void setModeEnd() {
    current_mode = MODE_SET_END;
    btn_none->is_active = false;
    btn_set_start->is_active = false;
    btn_set_end->is_active = true;
    selectBuildingPreset(selected_building_preset);
    SDL_SetWindowTitle(window, "路径规划模拟 - 设置终点(建筑物)");
  }

  void resetAll() {
    graph.clear();
    car_manager.clearAllCars();
    setModeNone();
    std::printf("已重置所有内容\n");
  }

  void spawnCarFromMenu() {
    if (!selected_node || !selected_node->is_start) {
      std::printf("无法生成小车：选择的节点不是起点\n");
      return;
    }
    PathNode* start_node = selected_node;
    PathNode* target_end = nullptr;
    for (PathNode* en : graph.end_nodes) {
      if (en->end_id == start_node->start_id) {
        target_end = en;
        break;
      }
    }
    if (target_end) {
      Car* car = car_manager.addCar(start_node, target_end);
      std::printf("生成小车 %d，从起点%d出发，目标终点 %d\n", car->id, start_node->start_id,
                  target_end->end_id);
    } else {
      PathNode* temp_end = graph.end_nodes.empty() ? nullptr : graph.end_nodes[0];
      Car* car = car_manager.addCar(start_node, temp_end);
      if (!temp_end) {
        car->path.clear();
        car->path_index = 0;
        car->target_node = nullptr;
      }
      car->state = "waiting_for_end";
      car->target_end_id = start_node->start_id;
      std::printf("生成小车 %d，从起点%d出发，等待对应终点出现\n", car->id, start_node->start_id);
    }
    show_menu = false;
  }

  void blitUtf8(SDL_Renderer* target, int x, int y, const char* utf8, TTF_Font* font,
                SDL_Color color) {
    if (!font || !target) return;
    SDL_Surface* surf = TTF_RenderUTF8_Blended(font, utf8, color);
    if (!surf) return;
    SDL_Texture* tex = SDL_CreateTextureFromSurface(target, surf);
    SDL_Rect dst = {x, y, surf->w, surf->h};
    SDL_RenderCopy(target, tex, nullptr, &dst);
    SDL_DestroyTexture(tex);
    SDL_FreeSurface(surf);
  }

  void draw() {
    setDrawColor(renderer, BACKGROUND_COLOR);
    SDL_RenderClear(renderer);

    SDL_Rect sidebar = {CANVAS_WIDTH, 0, SIDEBAR_WIDTH, WINDOW_HEIGHT};
    setDrawColor(renderer, SIDEBAR_COLOR);
    SDL_RenderFillRect(renderer, &sidebar);
    setDrawColor(renderer, SIDEBAR_BORDER_COLOR);
    SDL_RenderDrawLine(renderer, CANVAS_WIDTH, 0, CANVAS_WIDTH, WINDOW_HEIGHT);

    SDL_Rect top_card = {LOWER_PANEL_X, 10, LOWER_PANEL_W, 520};
    setDrawColor(renderer, SIDEBAR_CARD_COLOR);
    SDL_RenderFillRect(renderer, &top_card);
    setDrawColor(renderer, SIDEBAR_CARD_BORDER);
    SDL_RenderDrawRect(renderer, &top_card);

    setDrawColor(renderer, GRID_COLOR);
    for (int x = 0; x <= CANVAS_WIDTH; x += GRID_SIZE)
      SDL_RenderDrawLine(renderer, x, 0, x, CANVAS_HEIGHT);
    for (int y = 0; y <= CANVAS_HEIGHT; y += GRID_SIZE)
      SDL_RenderDrawLine(renderer, 0, y, CANVAS_WIDTH, y);

    if (hover_gx >= 0 && hover_gx < COLUMNS && hover_gy >= 0 && hover_gy < ROWS) {
      SDL_Rect hr = {hover_gx * GRID_SIZE, hover_gy * GRID_SIZE, GRID_SIZE, GRID_SIZE};
      setDrawColor(renderer, HOVER_COLOR);
      SDL_RenderDrawRect(renderer, &hr);
    }

    if ((current_mode == MODE_SET_END || current_mode == MODE_SET_START) && hover_gx >= 0 &&
        hover_gx < COLUMNS && hover_gy >= 0 && hover_gy < ROWS) {
      const bool ok = canPlaceBuildingPreview(hover_gx, hover_gy);
      SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
      for (const auto& d : buildingFootprint(selected_building_preset)) {
        const int gx = hover_gx + d.first;
        const int gy = hover_gy + d.second;
        if (gx < 0 || gx >= COLUMNS || gy < 0 || gy >= ROWS) continue;
        SDL_Rect pr = {gx * GRID_SIZE + 1, gy * GRID_SIZE + 1, GRID_SIZE - 2, GRID_SIZE - 2};
        if (ok)
          SDL_SetRenderDrawColor(renderer, 70, 190, 130, 100);
        else
          SDL_SetRenderDrawColor(renderer, 240, 70, 70, 115);
        SDL_RenderFillRect(renderer, &pr);
      }
      SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_NONE);
    }

    for (const auto& pr : graph.nodes) {
      PathNode* e = pr.second.get();
      if (e->building_preset < 0) continue;
      const BuildingStyle& st = buildingStyle(e->building_preset);
      for (const auto& d : buildingFootprint(e->building_preset)) {
        const int gx = e->grid_x + d.first;
        const int gy = e->grid_y + d.second;
        SDL_Rect cell = {gx * GRID_SIZE + 1, gy * GRID_SIZE + 1, GRID_SIZE - 2, GRID_SIZE - 2};
        setDrawColor(renderer, st.base);
        SDL_RenderFillRect(renderer, &cell);
        setDrawColor(renderer, st.trim);
        SDL_RenderDrawRect(renderer, &cell);
        if (d.first == 0 && d.second == 0) {
          SDL_Rect mark = {gx * GRID_SIZE + 4, gy * GRID_SIZE + 4, GRID_SIZE - 8, GRID_SIZE - 8};
          setDrawColor(renderer, {255, 220, 60, 255});
          SDL_RenderDrawRect(renderer, &mark);
          SDL_Rect mark2 = {gx * GRID_SIZE + 6, gy * GRID_SIZE + 6, GRID_SIZE - 12, GRID_SIZE - 12};
          SDL_RenderDrawRect(renderer, &mark2);
          if (font_small)
            blitUtf8(renderer, gx * GRID_SIZE + GRID_SIZE / 2 - 8, gy * GRID_SIZE + GRID_SIZE / 2 - 7,
                     "连", font_small, {30, 30, 10, 255});
        }
      }
    }

    for (const auto& pr : graph.nodes) {
      PathNode* node = pr.second.get();
      for (PathNode* nb : node->neighbors) {
        if (!node->orderLess(nb)) continue;
        drawPathEdge(renderer, node->x, node->y, nb->x, nb->y, PATH_LINE_HALF_WIDTH, PATH_GRAY);
      }
    }

    for (const auto& pr : graph.nodes) {
      PathNode* node = pr.second.get();
      const int deg = static_cast<int>(node->neighbors.size());
      if (deg > 0) {
        const int jr = deg >= 3 ? 9 : 7;
        setDrawColor(renderer, PATH_GRAY);
        fillCircle(renderer, static_cast<int>(node->x), static_cast<int>(node->y), jr);
      }
    }

    for (const auto& pr : graph.nodes) {
      PathNode* node = pr.second.get();
      if (node->building_preset >= 0) {
        TTF_Font* nf = id_font ? id_font : font_body;
        if (nf) {
          char buf[20];
          if (node->is_end)
            std::snprintf(buf, sizeof(buf), "E%d", node->end_id);
          else if (node->is_start)
            std::snprintf(buf, sizeof(buf), "S%d", node->start_id);
          else
            buf[0] = '\0';
          if (buf[0])
            blitUtf8(renderer, static_cast<int>(node->x) - 8, static_cast<int>(node->y) - 8, buf, nf,
                     TEXT_COLOR);
        }
        continue;
      }
      SDL_Color color = NODE_COLOR;
      int size = NODE_RADIUS;
      if (node->is_start) size = NODE_RADIUS + 2;
      if (node->is_end) size = NODE_RADIUS + 2;
      setDrawColor(renderer, color);
      if (node->is_end) {
        SDL_Rect rr = {static_cast<int>(node->x) - size, static_cast<int>(node->y) - size, size * 2,
                       size * 2};
        SDL_RenderFillRect(renderer, &rr);
      } else {
        fillCircle(renderer, static_cast<int>(node->x), static_cast<int>(node->y), size);
      }
      TTF_Font* nf = id_font ? id_font : font_body;
      if (node->is_start && nf) {
        char buf[16];
        std::snprintf(buf, sizeof(buf), "%d", node->start_id);
        blitUtf8(renderer, static_cast<int>(node->x) - 4, static_cast<int>(node->y) - 6, buf, nf,
                 TEXT_COLOR);
      } else if (node->is_end && nf) {
        char buf[16];
        std::snprintf(buf, sizeof(buf), "%d", node->end_id);
        blitUtf8(renderer, static_cast<int>(node->x) - 4, static_cast<int>(node->y) - 6, buf, nf,
                 TEXT_COLOR);
      }
    }

    const Uint32 tick = SDL_GetTicks();
    TTF_Font* car_font = font_small ? font_small : id_font;
    for (auto& cp : car_manager.cars) {
      Car* car = cp.get();
      const float ix = car->cur_x;
      const float iy = car->cur_y;
      SDL_Color body = {110, 115, 125, 255};
      SDL_Color roof = {72, 76, 86, 255};
      if (car->state == "waiting" || car->state == "waiting_for_end") {
        body = CAR_WAIT_COLOR;
        roof = {140, 25, 25, 255};
      } else if (car->state == "moving") {
        const SDL_Color c1 = hashBrightBody(car->flash_seed, static_cast<int>(tick / 700));
        const SDL_Color c2 = hashBrightBody(car->flash_seed ^ 0xA5C3F17Du, static_cast<int>(tick / 700) + 3);
        body = ((tick / 100) & 1u) ? c1 : c2;
        roof = {static_cast<Uint8>((static_cast<int>(body.r) * 2) / 3),
                static_cast<Uint8>((static_cast<int>(body.g) * 2) / 3),
                static_cast<Uint8>((static_cast<int>(body.b) * 2) / 3), 255};
      }
      drawCarShape(renderer, ix, iy, car->headingAngle(), body, roof);
      if (car_font) {
        char buf[32];
        std::snprintf(buf, sizeof(buf), "%d", car->id);
        blitUtf8(renderer, static_cast<int>(ix) - 4, static_cast<int>(iy) - 6, buf, car_font,
                 TEXT_COLOR);
        std::snprintf(buf, sizeof(buf), "→%d", car->target_end_id);
        blitUtf8(renderer, static_cast<int>(ix) - 10, static_cast<int>(iy) - GRID_SIZE - 4, buf,
                 car_font, {30, 30, 40, 255});
        if (car->state == "waiting")
          blitUtf8(renderer, static_cast<int>(ix) - 14, static_cast<int>(iy) + GRID_SIZE / 2 + 2,
                   "等待", car_font, WAIT_TEXT_COLOR);
        else if (car->state == "waiting_for_end")
          blitUtf8(renderer, static_cast<int>(ix) - 22, static_cast<int>(iy) + GRID_SIZE / 2 + 2,
                   "等待终点", car_font, WAIT_TEXT_COLOR);
      }
    }

    for (Button* b : buttons) b->draw(renderer);
    SDL_Rect tr = timeSliderTrackRect();
    setDrawColor(renderer, {180, 188, 206, 255});
    SDL_RenderFillRect(renderer, &tr);
    setDrawColor(renderer, {132, 142, 166, 255});
    SDL_RenderDrawRect(renderer, &tr);
    const double ratio = sliderRatioFromTimeScale();
    const int knob_cx = tr.x + static_cast<int>(ratio * tr.w);
    const int knob_cy = tr.y + tr.h / 2;
    setDrawColor(renderer, {76, 112, 206, 255});
    fillCircle(renderer, knob_cx, knob_cy, 8);
    blitUtf8(renderer, tr.x, tr.y - 22, "拖动滑条调速", font_small ? font_small : font_body,
             PANEL_BODY_COLOR);
    char speed_buf[48];
    std::snprintf(speed_buf, sizeof(speed_buf), "当前 %.2fx", time_scale);
    blitUtf8(renderer, tr.x + tr.w - 84, tr.y - 22, speed_buf, font_small ? font_small : font_body,
             PANEL_BODY_COLOR);
    if (current_mode == MODE_SET_END || current_mode == MODE_SET_START) {
      blitUtf8(renderer, LOWER_PANEL_X + 8, BUILDING_TITLE_Y, "建筑物预设（「连」格接路径）",
               font_body ? font_body : id_font, PANEL_TITLE_COLOR);
      for (Button* b : building_preset_buttons) b->draw(renderer);
      setDrawColor(renderer, {160, 162, 175, 255});
      const int div_y = BUILDING_MODE_LOWER_TOP - 1;
      SDL_RenderDrawLine(renderer, LOWER_PANEL_X, div_y, LOWER_PANEL_X + LOWER_PANEL_W - 1, div_y);
    }

    drawStatusInfo();
    SDL_RenderPresent(renderer);
    drawMonitorWindow();
  }

  void drawStatusInfo() {
    TTF_Font* font = font_body;
    TTF_Font* small = font_small ? font_small : font_body;
    if (!font) return;
    const SDL_Color title_c = PANEL_TITLE_COLOR;
    const SDL_Color body_c = PANEL_BODY_COLOR;
    const int x_title = CANVAS_WIDTH + 20;
    blitUtf8(renderer, x_title, 16, "控制面板", font_title ? font_title : font, title_c);

    const int x0 = LOWER_PANEL_X + 12;
    const int x1 = LOWER_PANEL_X + LOWER_PANEL_W - 1;
    constexpr int stats_bottom_pad = 20;
    constexpr int stats_line_step = 27;
    constexpr int stats_title_step = 30;
    constexpr int stats_sep_above_title = 16;
    const int content_bottom = WINDOW_HEIGHT - stats_bottom_pad;
    int y = content_bottom - stats_line_step;
    char line[128];
    std::snprintf(line, sizeof(line), "小车数: %zu", car_manager.cars.size());
    blitUtf8(renderer, x0, y, line, small ? small : font, body_c);
    y -= stats_line_step;
    std::snprintf(line, sizeof(line), "时间流速: %.2fx", time_scale);
    blitUtf8(renderer, x0, y, line, small ? small : font, body_c);
    y -= stats_line_step;
    std::snprintf(line, sizeof(line), "全局状态: %s", paused ? "暂停" : "运行");
    blitUtf8(renderer, x0, y, line, small ? small : font, body_c);
    y -= stats_line_step;
    std::snprintf(line, sizeof(line), "终点数: %zu", graph.end_nodes.size());
    blitUtf8(renderer, x0, y, line, small ? small : font, body_c);
    y -= stats_line_step;
    std::snprintf(line, sizeof(line), "起点数: %zu", graph.start_nodes.size());
    blitUtf8(renderer, x0, y, line, small ? small : font, body_c);
    y -= stats_line_step;
    std::snprintf(line, sizeof(line), "路径点: %zu", graph.nodes.size());
    blitUtf8(renderer, x0, y, line, small ? small : font, body_c);
    y -= stats_title_step;
    blitUtf8(renderer, x0, y, "统计信息", font, title_c);
    y -= stats_sep_above_title;
    setDrawColor(renderer, {180, 180, 180, 255});
    SDL_RenderDrawLine(renderer, x0, y, x1, y);
    y -= 24;
    blitUtf8(renderer, x0, y, "快捷键: 空格暂停  -/+= 调速", small ? small : font, body_c);
  }

  Uint32 eventWindowId(const SDL_Event& ev) const {
    switch (ev.type) {
      case SDL_MOUSEMOTION:
        return ev.motion.windowID;
      case SDL_MOUSEBUTTONDOWN:
      case SDL_MOUSEBUTTONUP:
        return ev.button.windowID;
      case SDL_MOUSEWHEEL:
        return ev.wheel.windowID;
      case SDL_KEYDOWN:
      case SDL_KEYUP:
        return ev.key.windowID;
      case SDL_TEXTINPUT:
        return ev.text.windowID;
      default:
        return main_window_id;
    }
  }

  void handleEvents() {
    SDL_Event e;
    while (SDL_PollEvent(&e)) {
      if (e.type == SDL_QUIT) {
        running = false;
        continue;
      }
      if (e.type == SDL_WINDOWEVENT && e.window.event == SDL_WINDOWEVENT_CLOSE) {
        if (e.window.windowID == monitor_window_id) {
          destroyMonitorWindow();
          continue;
        }
        if (e.window.windowID == main_window_id) {
          running = false;
          continue;
        }
      }

      if (monitor_window_id && eventWindowId(e) == monitor_window_id) {
        if (e.type == SDL_MOUSEBUTTONDOWN && e.button.button == SDL_BUTTON_LEFT)
          handleCarMonitorPanelClick(e.button.x, e.button.y);
        else if (e.type == SDL_TEXTINPUT && monitor_end_input_active) {
          const char* t = e.text.text;
          for (int i = 0; t[i]; ++i) {
            if (t[i] >= '0' && t[i] <= '9') monitor_end_input_text.push_back(t[i]);
          }
        } else if (e.type == SDL_KEYDOWN && monitor_end_input_active) {
          const SDL_Keycode k = e.key.keysym.sym;
          if (k == SDLK_BACKSPACE) {
            if (!monitor_end_input_text.empty()) monitor_end_input_text.pop_back();
          } else if (k == SDLK_RETURN || k == SDLK_KP_ENTER) {
            applyMonitorEndInput();
            monitor_end_input_active = false;
            monitor_input_car_id = -1;
            monitor_end_input_text.clear();
            SDL_StopTextInput();
          } else if (k == SDLK_ESCAPE) {
            monitor_end_input_active = false;
            monitor_input_car_id = -1;
            monitor_end_input_text.clear();
            SDL_StopTextInput();
          }
        }
        continue;
      }

      bool button_clicked = false;
      for (Button* b : buttons) {
        if (b->handleEvent(e)) {
          button_clicked = true;
          break;
        }
      }
      if (!button_clicked && (current_mode == MODE_SET_END || current_mode == MODE_SET_START)) {
        for (Button* b : building_preset_buttons) {
          if (b->handleEvent(e)) {
            button_clicked = true;
            break;
          }
        }
      }
      if (button_clicked) continue;

      if (e.type == SDL_KEYDOWN) {
        if (eventWindowId(e) != main_window_id) continue;
        const SDL_Keycode k = e.key.keysym.sym;
        if (k == SDLK_s) {
          setModeStart();
        } else if (k == SDLK_e) {
          setModeEnd();
        } else if (k == SDLK_r) {
          graph.clear();
          car_manager.clearAllCars();
          setModeNone();
        } else if (k == SDLK_SPACE) {
          togglePause();
        } else if (k == SDLK_MINUS || k == SDLK_KP_MINUS) {
          adjustTimeScale(-0.25);
        } else if (k == SDLK_EQUALS || k == SDLK_PLUS || k == SDLK_KP_PLUS) {
          adjustTimeScale(0.25);
        } else if (show_menu && k == SDLK_1) {
          spawnCarFromMenu();
        }
        continue;
      }

      if (e.type == SDL_MOUSEMOTION) {
        if (eventWindowId(e) != main_window_id) continue;
        const int mx = e.motion.x;
        const int my = e.motion.y;
        if (dragging_time_slider) {
          setTimeScaleFromSliderX(mx);
          continue;
        }
        if (mx < CANVAS_WIDTH) {
          const int gx = mx / GRID_SIZE;
          const int gy = my / GRID_SIZE;
          hover_gx = gx;
          hover_gy = gy;
          if (left_button_down && current_mode == MODE_NONE) {
            const auto cur = std::make_pair(gx, gy);
            if (cur != last_grid) {
              graph.addNode(gx, gy);
              last_grid = cur;
            }
          }
        }
        continue;
      }

      if (e.type == SDL_MOUSEBUTTONDOWN) {
        if (eventWindowId(e) != main_window_id) continue;
        const int mx = e.button.x;
        const int my = e.button.y;
        if (mx >= CANVAS_WIDTH) {
          if (e.button.button == SDL_BUTTON_LEFT) {
            SDL_Rect tr = timeSliderTrackRect();
            SDL_Rect hit = {tr.x - 8, tr.y - 8, tr.w + 16, tr.h + 16};
            if (mx >= hit.x && mx < hit.x + hit.w && my >= hit.y && my < hit.y + hit.h) {
              dragging_time_slider = true;
              setTimeScaleFromSliderX(mx);
              continue;
            }
          }
          continue;
        }

        const int gx = mx / GRID_SIZE;
        const int gy = my / GRID_SIZE;

        if (e.button.button == SDL_BUTTON_LEFT) {
          show_menu = false;
          left_button_down = true;
          last_grid = {gx, gy};
          if (current_mode == MODE_SET_START) {
            PathNode* node = nullptr;
            if (canPlaceBuildingPreview(gx, gy))
              node = graph.tryPlaceStartBuilding(gx, gy, selected_building_preset);
            if (!node) node = graph.setStartNode(gx, gy);
            if (node)
              std::printf("设置起点 %d 在 (%d,%d) 建筑预设%d\n", node->start_id, gx, gy,
                          node->building_preset);
            else
              std::printf("无法设置起点：不可放置建筑或无路径节点\n");
          } else if (current_mode == MODE_SET_END) {
            PathNode* node = graph.tryPlaceEndBuilding(gx, gy, selected_building_preset);
            if (node)
              std::printf("设置终点(建筑)%d 锚点格(%d,%d) 预设%d\n", node->end_id, gx, gy,
                          selected_building_preset);
            else
              std::printf("无法放置建筑物：越界或与已有路径点/重叠\n");
          } else {
            graph.addNode(gx, gy);
          }
        } else if (e.button.button == SDL_BUTTON_RIGHT) {
          PathNode* node = graph.getNodeAtPixel(mx, my);
          if (node && node->is_start) {
            selected_node = node;
            spawnCarFromMenu();
          } else if (node && !node->is_start) {
            std::printf("只能在起点生成小车\n");
          }
        } else if (e.button.button == SDL_BUTTON_MIDDLE) {
          show_menu = false;
          Car* car = car_manager.findCarAtPosition(mx, my);
          if (car) {
            std::printf("删除小车 %d\n", car->id);
            car_manager.removeCar(car);
            continue;
          }
          PathNode* bhit = graph.findStartOrEndCoveringGrid(gx, gy);
          if (bhit) {
            if (bhit->is_end) {
              std::printf("删除终点 %d\n", bhit->end_id);
              graph.removeEndNode(bhit->grid_x, bhit->grid_y);
            } else if (bhit->is_start) {
              std::printf("删除起点 %d\n", bhit->start_id);
              graph.removeStartNode(bhit->grid_x, bhit->grid_y);
            }
            continue;
          }
          PathNode* node = graph.getNodeAtPixel(mx, my);
          if (node) {
            if (node->is_start) {
              std::printf("删除起点 %d\n", node->start_id);
              graph.removeStartNode(node->grid_x, node->grid_y);
            } else if (node->is_end) {
              std::printf("删除终点 %d\n", node->end_id);
              graph.removeEndNode(node->grid_x, node->grid_y);
            } else {
              std::printf("删除路径节点 (%d, %d)\n", node->grid_x, node->grid_y);
              graph.removeNode(node->grid_x, node->grid_y);
            }
          }
        }
        continue;
      }

      if (e.type == SDL_MOUSEBUTTONUP && e.button.button == SDL_BUTTON_LEFT) {
        if (eventWindowId(e) != main_window_id) continue;
        left_button_down = false;
        dragging_time_slider = false;
        last_grid = {-1, -1};
      }
    }
  }

  void run() {
    while (running) {
      handleEvents();
      if (!paused)
        car_manager.updateAllCars(SDL_GetTicks(), time_scale);
      draw();
    }
  }

  void shutdown() {
    delete btn_none;
    delete btn_set_start;
    delete btn_set_end;
    delete btn_reset;
    delete btn_pause;
    delete btn_speed_down;
    delete btn_speed_up;
    delete btn_save;
    delete btn_load_prev;
    btn_none = btn_set_start = btn_set_end = btn_reset = nullptr;
    btn_pause = btn_speed_down = btn_speed_up = nullptr;
    btn_save = btn_load_prev = nullptr;
    for (Button* b : building_preset_buttons) delete b;
    building_preset_buttons.clear();
    if (font_title && font_title != font_body) TTF_CloseFont(font_title);
    if (font_small && font_small != font_body && font_small != font_button) TTF_CloseFont(font_small);
    if (font_button && font_button != font_body) TTF_CloseFont(font_button);
    if (font_body) TTF_CloseFont(font_body);
    font_title = font_body = font_small = font_button = id_font = nullptr;
    destroyMonitorWindow();
    if (renderer) SDL_DestroyRenderer(renderer);
    if (window) SDL_DestroyWindow(window);
    TTF_Quit();
    SDL_Quit();
  }
};

}  // namespace

int main(int argc, char* argv[]) {
  (void)argc;
  (void)argv;
  Game game;
  if (!game.init()) {
    std::fprintf(stderr, "初始化失败: %s\n", SDL_GetError());
    return 1;
  }
  game.run();
  game.shutdown();
  return 0;
}
