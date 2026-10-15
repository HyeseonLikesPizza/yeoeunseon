// 일반 C++ 정확성 테스트다. Unreal 의존성과 성능 시간 측정은 없다.
// MSVC /utf-8 /std:c++17 /EHsc로 빌드한다. /O2에서도 실패 검사는 생략되지 않는다.
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <iostream>
#include <limits>
#include <map>
#include <random>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

struct Enemy { int Id; double X, Y; };
struct Query { double X, Y, Radius; };
using Cell = std::pair<int, int>;
using Hits = std::vector<int>;

void Require(bool condition, const std::string& message)
{
    if (!condition) throw std::runtime_error(message);
}

enum class Search { Original, FloorOnly, FixedNineCells, Corrected };

class World
{
public:
    explicit World(double cellSize) : CellSize(cellSize)
    {
        Require(std::isfinite(cellSize) && cellSize > 0, "Invalid cell size");
    }

    void Add(Enemy enemy)
    {
        const Cell cell = Locate(enemy.X, enemy.Y);
        Require(Enemies.emplace(enemy.Id, enemy).second, "Duplicate EnemyId");
        Cells[cell].push_back(enemy.Id);
    }

    void Move(int id, double x, double y)
    {
        Enemy& enemy = Enemies.at(id);
        const Cell oldCell = Locate(enemy.X, enemy.Y), newCell = Locate(x, y);
        // 위치는 매번 갱신하지만, 칸이 바뀔 때만 이전 목록에서 빼고 새 목록에 넣는다.
        if (oldCell != newCell)
        {
            Unregister(oldCell, id);
            Cells[newCell].push_back(id);
        }
        enemy.X = x;
        enemy.Y = y;
    }

    void Remove(int id)
    {
        const Enemy enemy = Enemies.at(id);
        Unregister(Locate(enemy.X, enemy.Y), id);
        Enemies.erase(id);
    }

    Hits FullScan(Query q) const
    {
        Validate(q);
        Hits hits;
        // 기준 답: 그리드 목록을 보지 않고 모든 적에게 거리 검사를 한다.
        for (const auto& entry : Enemies)
        {
            const Enemy& e = entry.second;
            const double dx = e.X - q.X, dy = e.Y - q.Y;
            if (dx * dx + dy * dy <= q.Radius * q.Radius) hits.push_back(e.Id);
        }
        return hits;
    }

    Hits Find(Query q, Search mode = Search::Corrected) const
    {
        Validate(q);
        Hits hits;
        int minX, maxX, minY, maxY;
        if (mode == Search::Original)
        {
            // 비교용 오답: 정수 변환 + 플레이어 칸 하나만 검사하는 원래 코드를 재현한다.
            minX = maxX = static_cast<int>(q.X / CellSize);
            minY = maxY = static_cast<int>(q.Y / CellSize);
        }
        else if (mode == Search::FloorOnly || mode == Search::FixedNineCells)
        {
            const int margin = mode == Search::FixedNineCells ? 1 : 0;
            minX = ToCell(q.X) - margin; maxX = ToCell(q.X) + margin;
            minY = ToCell(q.Y) - margin; maxY = ToCell(q.Y) + margin;
        }
        else
        {
            // 수정: 원을 감싼 사각형의 최소/최대 칸 번호를 구한다.
            minX = ToCell(q.X - q.Radius); maxX = ToCell(q.X + q.Radius);
            minY = ToCell(q.Y - q.Radius); maxY = ToCell(q.Y + q.Radius);
        }
        const double radiusSquared = q.Radius * q.Radius;
        // cy는 행, cx는 열. 위에서 구한 범위의 모든 칸을 방문한다.
        for (int cy = minY; cy <= maxY; ++cy)
            for (int cx = minX; cx <= maxX; ++cx)
            {
                const auto cell = Cells.find({cx, cy});
                if (cell == Cells.end()) continue; // 빈 칸이면 다음 칸으로 간다.
                // cell->second는 지금 방문한 한 칸의 EnemyId 목록이다.
                for (int id : cell->second)
                {
                    const Enemy& e = Enemies.at(id);
                    const double dx = e.X - q.X, dy = e.Y - q.Y;
                    // 이 칸에 있다는 것만으로 공격 원 안이라는 뜻은 아니다. 거리로 거른다.
                    if (dx * dx + dy * dy <= radiusSquared) hits.push_back(id);
                }
            }
        return hits;
    }

    void CheckRegistration() const
    {
        std::map<int, int> counts;
        for (const auto& cell : Cells)
            for (int id : cell.second)
            {
                const auto found = Enemies.find(id);
                Require(found != Enemies.end(), "Deleted ID left in grid");
                Require(Locate(found->second.X, found->second.Y) == cell.first,
                        "Enemy registered in wrong cell");
                ++counts[id];
            }
        for (const auto& enemy : Enemies)
            Require(counts[enemy.first] == 1, "Missing or duplicate registration");
    }

private:
    // 이 테스트는 칸에 ID 목록을 저장하고, Enemies에서 현재 상태를 가져온다.
    // 컨테이너 원소의 포인터 수명 문제와 검색 범위 문제를 섞지 않기 위한 구조다.
    double CellSize;
    std::map<int, Enemy> Enemies;
    std::map<Cell, Hits> Cells;

    int ToCell(double coordinate) const
    {
        // 음수도 올바른 칸에 들어가도록 내림한 뒤 정수로 변환한다.
        const double cell = std::floor(coordinate / CellSize);
        Require(std::isfinite(cell) && cell > std::numeric_limits<int>::min() + 1.0
                && cell < std::numeric_limits<int>::max() - 1.0, "Cell outside test range");
        return static_cast<int>(cell);
    }
    Cell Locate(double x, double y) const { return {ToCell(x), ToCell(y)}; }
    void Validate(Query q) const
    {
        Require(std::isfinite(q.X) && std::isfinite(q.Y) && std::isfinite(q.Radius)
                && q.Radius >= 0 && std::isfinite(q.Radius * q.Radius), "Invalid query");
        (void)ToCell(q.X - q.Radius); (void)ToCell(q.X + q.Radius);
        (void)ToCell(q.Y - q.Radius); (void)ToCell(q.Y + q.Radius);
    }
    void Unregister(Cell cell, int id)
    {
        auto& ids = Cells.at(cell);
        const auto found = std::find(ids.begin(), ids.end(), id);
        Require(found != ids.end(), "Registration not found");
        ids.erase(found);
        if (ids.empty()) Cells.erase(cell);
    }
};

Hits Sorted(Hits hits) { std::sort(hits.begin(), hits.end()); return hits; }
int Checks = 0, BugDetections = 0;

void Compare(const World& world, Query q)
{
    // 순서는 무시하되 중복은 제거하지 않는다. 같은 적을 두 번 반환해도 실패해야 한다.
    Require(Sorted(world.FullScan(q)) == Sorted(world.Find(q)), "Grid/full-scan mismatch");
    ++Checks;
}

void Known(const std::string& name, const World& world, Query q, Hits expected)
{
    world.CheckRegistration();
    Require(Sorted(world.FullScan(q)) == Sorted(expected), name + ": wrong reference result");
    Compare(world, q);
    std::cout << "PASS " << name << " / hits=" << expected.size() << "\n";
}

void Detect(const std::string& name, const World& world, Query q, Search broken)
{
    Require(Sorted(world.FullScan(q)) != Sorted(world.Find(q, broken)),
            "Test failed to detect deliberate bug: " + name);
    ++BugDetections;
    std::cout << "DETECTED " << name << "\n";
}

int main()
{
    try
    {
        std::cout << "Grid radius correctness / no timing benchmark\n";
        World boundary(4);
        boundary.Add({1, 3.8, 1}); boundary.Add({2, 4.1, 1});
        boundary.Add({3, 6.1, 1}); boundary.Add({4, 5.8, 2.9});
        Known("neighbor across cell boundary", boundary, {3.9, 1, 2}, {1, 2});
        Detect("original same-cell search misses neighbor", boundary, {3.9, 1, 2}, Search::Original);
        Detect("floor alone still misses neighbor", boundary, {3.9, 1, 2}, Search::FloorOnly);

        World negative(4);
        negative.Add({10, -0.1, 1}); negative.Add({11, 0.1, 1});
        Known("negative coordinate", negative, {-0.1, 1, 0.05}, {10});
        Detect("integer truncation selects wrong negative cell", negative, {-0.1, 1, 0.05}, Search::Original);

        World edges(4);
        edges.Add({20, -4, 0}); edges.Add({21, -4.0001, 0}); edges.Add({22, -3.9999, 0});
        Known("exact negative cell edge and zero radius", edges, {-4, 0, 0}, {20});

        World circle(4);
        circle.Add({30, 1.9, 1.9}); circle.Add({31, 1, 1});
        circle.Add({32, 2, 0}); circle.Add({33, std::nextafter(2.0, 3.0), 0});
        Known("square corner excluded; radius edge included", circle, {0, 0, 2}, {31, 32});

        World large(4);
        large.Add({40, 9, 0}); large.Add({41, -9, 0}); large.Add({42, 0, 10});
        large.Add({43, 10, 0}); large.Add({44, 6, 8}); large.Add({45, 10.0001, 0});
        Known("radius spanning more than nine cells", large, {0, 0, 10}, {40, 41, 42, 43, 44});
        Detect("fixed nine-cell search misses distant candidates", large, {0, 0, 10}, Search::FixedNineCells);

        World empty(4);
        Known("empty world", empty, {0, 0, 20}, {});
        World crowded(4);
        for (int i = 0; i < 20000; ++i) crowded.Add({i, 0.25, 0.25});
        Hits all;
        for (int i = 0; i < 20000; ++i) all.push_back(i);
        Known("20000 enemies in one cell", crowded, {0, 0, 2}, all);

        World moving(4);
        moving.Add({100, 3.9, 1});
        Known("before move", moving, {3.9, 1, 0}, {100});
        moving.Move(100, 4.1, 1);
        Known("move across cell boundary", moving, {4.1, 1, 0}, {100});
        Known("old position no longer hits", moving, {3.9, 1, 0}, {});
        moving.Move(100, 4.5, 1);
        Known("move inside same cell", moving, {4.5, 1, 0}, {100});
        moving.Move(100, -0.1, -0.1);
        Known("move into negative cell", moving, {-0.1, -0.1, 0}, {100});
        moving.Remove(100);
        Known("deleted enemy disappears", moving, {-0.1, -0.1, 0}, {});
        moving.Add({101, -0.1, -0.1});
        Known("new enemy appears", moving, {-0.1, -0.1, 0}, {101});

        // 같은 seed의 재현 가능한 데이터로 칸 크기를 바꿔 비교한다.
        // 이동/삭제/추가 후에도 결과와 칸 등록 상태를 다시 검사한다.
        for (unsigned seed : {17u, 42u})
            for (double cellSize : {1.0, 4.0, 16.0})
            {
                std::mt19937 rng(seed);
                auto coordinate = [&]() { return (static_cast<int>(rng() % 16001) - 8000) / 16.0; };
                World world(cellSize);
                for (int i = 0; i < 20000; ++i) world.Add({i, coordinate(), coordinate()});
                for (int phase = 0; phase < 2; ++phase)
                {
                    if (phase == 1)
                    {
                        for (int i = 0; i < 200; ++i) world.Move(i, coordinate(), coordinate());
                        for (int i = 200; i < 250; ++i) world.Remove(i);
                        for (int i = 20000; i < 20050; ++i) world.Add({i, coordinate(), coordinate()});
                    }
                    world.CheckRegistration();
                    const double radii[] = {0, 2, 10, 40};
                    for (int i = 0; i < 100; ++i)
                        Compare(world, {coordinate(), coordinate(), radii[i % 4]});
                }
                std::cout << "PASS seeded queries / enemies=20000 / seed=" << seed
                          << " / cell=" << cellSize << " / queries=200 / includes mutations\n";
            }
        std::cout << "SUCCESS / corrected comparisons=" << Checks << " / mismatch=0"
                  << " / deliberate bug detections=" << BugDetections << "/4\n";
        return 0;
    }
    catch (const std::exception& error)
    {
        std::cerr << "FAIL " << error.what() << "\n";
        return 1;
    }
}
