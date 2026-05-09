#include <cstddef>
#include <cstdint>

#include <benchmark/benchmark.h>
#include <utility>

#include "engine/path.h"
#include "engine/point.hpp"
#include "engine/points_in_rectangle_range.hpp"
#include "engine/size.hpp"

namespace devilution {
namespace {

struct Map {
	Size size;
	const char *data;
	char operator[](const Point &p) const { return data[p.y * size.width + p.x]; }
};

std::pair<Point, Point> FindStartDest(const Map &m)
{
	Point start, dest;
	for (const Point p : PointsInRectangle(Rectangle(Point { 0, 0 }, m.size))) {
		switch (m[p]) {
		case 'S':
			start = p;
			break;
		case 'E':
			dest = p;
			break;
		}
	}
	return { start, dest };
}

void BenchmarkMap(const Map &map, benchmark::State &state)
{
	const auto [start, dest] = FindStartDest(map);
	const auto posOk = /*posOk=*/[&map](Point p) { return map[p] != '#'; };
	constexpr size_t MaxPathLength = 25;
	for (auto _ : state) {
		int8_t path[MaxPathLength];
		int result = FindPath(/*canStep=*/[](Point, Point) { return true; },
		    posOk, start, dest, path, MaxPathLength);
		benchmark::DoNotOptimize(result);
	}
}

void BM_SinglePath(benchmark::State &state)
{
	BenchmarkMap(
	    Map {
	        Size { 15, 15 },
	        "###############"
	        "#...#...#.....#"
	        "#.#.#.#.#.###.#"
	        "#S#...#.#.#...#"
	        "#######.#.#.###"
	        "##...##.#.#E..#"
	        "#######.#.###.#"
	        "###...#...#...#"
	        "###.#######.###"
	        "#...###...#...#"
	        "#.#####.#.###.#"
	        "#.#...#.#.#...#"
	        "#.#.#.#.#.#.###"
	        "#...#...#...###"
	        "###############" },
	    state);
}

void BM_Bridges(benchmark::State &state)
{
	BenchmarkMap(
	    Map {
	        Size { 15, 15 },
	        "###############"
	        "#.S...........#"
	        "#.............#"
	        "#.............#"
	        "#.............#"
	        "#.............#"
	        "############.##"
	        "#.............#"
	        "#.............#"
	        "#.............#"
	        "#.............#"
	        "#.............#"
	        "###.###########"
	        "#.....E.......#"
	        "###############" },
	    state);
}

void BM_NoPath(benchmark::State &state)
{
	BenchmarkMap(
	    Map {
	        Size { 15, 15 },
	        "###############"
	        "#.S...........#"
	        "#.............#"
	        "#.............#"
	        "#.............#"
	        "#.............#"
	        "#.............#"
	        "#.............#"
	        "#.............#"
	        "#.............#"
	        "#.............#"
	        "#.............#"
	        "###############"
	        "#.....E.......#"
	        "###############" },
	    state);
}

void BM_NoPathBig(benchmark::State &state)
{
	BenchmarkMap(
	    Map {
	        Size { 30, 30 },
	        "##############################"
	        "#............................#"
	        "#............................#"
	        "#............................#"
	        "#............................#"
	        "#............................#"
	        "#............................#"
	        "#............................#"
	        "#............................#"
	        "#............................#"
	        "#............................#"
	        "#............................#"
	        "#............................#"
	        "#............................#"
	        "#............................#"
	        "#............................#"
	        "#............................#"
	        "#............................#"
	        "#............................#"
	        "#............................#"
	        "#............................#"
	        "#............................#"
	        "#............................#"
	        "#............................#"
	        "#............................#"
	        "#............................#"
	        "#.....S......................#"
	        "##############################"
	        "#.....E......................#"
	        "##############################" },
	    state);
}

BENCHMARK(BM_SinglePath);
BENCHMARK(BM_Bridges);
BENCHMARK(BM_NoPath);
BENCHMARK(BM_NoPathBig);

} // namespace
} // namespace devilution
