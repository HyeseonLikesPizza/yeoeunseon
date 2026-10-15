#include <algorithm>
#include <chrono>
#include <cstdint>
#include <iomanip>
#include <iostream>
#include <random>
#include <stdexcept>
#include <thread>
#include <unordered_map>
#include <vector>

struct Enemy
{
    int Id;
    int HP;
};

using Clock = std::chrono::steady_clock;
volatile std::uint64_t ResultSink = 0;

// Measure the whole query list. No comparison counter in the timed loop.
// Keeping a returned checksum prevents unused lookup results being discarded.
__declspec(noinline) std::uint64_t FindInVector(
    const std::vector<Enemy>& enemies, const std::vector<int>& queries)
{
    std::uint64_t checksum = 0;
    for (int id : queries)
    {
        for (const Enemy& enemy : enemies)
        {
            if (enemy.Id == id)
            {
                checksum += static_cast<unsigned>(enemy.HP);
                break;
            }
        }
    }
    return checksum;
}

__declspec(noinline) std::uint64_t FindInMap(
    const std::unordered_map<int, Enemy>& enemies, const std::vector<int>& queries)
{
    std::uint64_t checksum = 0;
    for (int id : queries)
    {
        auto found = enemies.find(id);
        if (found != enemies.end())
            checksum += static_cast<unsigned>(found->second.HP);
    }
    return checksum;
}

struct Measurement
{
    double NsPerQuery;
    std::uint64_t Checksum;
};

template <typename Lookup>
Measurement Measure(Lookup lookup, std::size_t queryCount)
{
    const auto start = Clock::now();
    const auto checksum = lookup();
    const auto end = Clock::now();
    ResultSink = checksum;
    return {std::chrono::duration<double, std::nano>(end - start).count()
                / static_cast<double>(queryCount), checksum};
}

double Median(std::vector<double> times)
{
    std::sort(times.begin(), times.end());
    return times[times.size() / 2];
}

void Compare(std::size_t count, bool successfulQueries)
{
    std::mt19937 random(static_cast<unsigned>(20261010 + count));
    std::vector<Enemy> vector;
    std::unordered_map<int, Enemy> map;
    vector.reserve(count);
    map.max_load_factor(1.0f);
    map.reserve(count);

    // Unique, non-consecutive positive IDs; shuffle vector storage order.
    // Odd multiplication modulo 2^30 gives unique IDs for these input indices.
    for (std::size_t i = 0; i < count; ++i)
    {
        const int id = static_cast<int>((static_cast<std::uint32_t>(i + 1)
            * 2654435761u) & 0x3fffffffu);
        vector.push_back({id, static_cast<int>(100 + i % 901)});
    }
    std::shuffle(vector.begin(), vector.end(), random);
    for (const Enemy& enemy : vector)
        map.emplace(enemy.Id, enemy);

    // Enough queries for timing, while limiting large linear-scan runs.
    const std::size_t queryCount = std::clamp<std::size_t>(
        16000000 / count, 4096, 262144);
    std::vector<int> queries;
    queries.reserve(queryCount);
    std::uniform_int_distribution<std::size_t> pick(0, count - 1);
    for (std::size_t i = 0; i < queryCount; ++i)
    {
        const int id = vector[pick(random)].Id;
        queries.push_back(successfulQueries ? id : -id - 1);
    }

    std::uint64_t expected = 0;
    // An independently prepared expected result also checks all-miss cases.
    for (int id : queries)
    {
        if (successfulQueries)
            expected += static_cast<unsigned>(map.at(id).HP);
    }
    auto vectorLookup = [&] { return FindInVector(vector, queries); };
    auto mapLookup = [&] { return FindInMap(map, queries); };

    // Warm up both implementations; verify output before timing.
    if (vectorLookup() != expected || mapLookup() != expected)
        throw std::runtime_error("Warm-up result mismatch");

    constexpr int Samples = 9;
    std::vector<double> vectorTimes, mapTimes;
    for (int sample = 0; sample < Samples; ++sample)
    {
        Measurement v{}, m{};
        // Alternate order to reduce a systematic first/second-run bias.
        if (sample % 2 == 0)
        {
            v = Measure(vectorLookup, queryCount);
            m = Measure(mapLookup, queryCount);
        }
        else
        {
            m = Measure(mapLookup, queryCount);
            v = Measure(vectorLookup, queryCount);
        }
        if (v.Checksum != expected || m.Checksum != expected)
            throw std::runtime_error("Timed result mismatch");
        vectorTimes.push_back(v.NsPerQuery);
        mapTimes.push_back(m.NsPerQuery);
    }

    const double v = Median(vectorTimes), m = Median(mapTimes);
    std::size_t maxBucket = 0;
    for (std::size_t b = 0; b < map.bucket_count(); ++b)
        maxBucket = std::max(maxBucket, map.bucket_size(b));

    std::cout << (successfulQueries ? "hit" : "miss") << ',' << count << ','
        << queryCount << ',' << sizeof(Enemy) << ',' << v << ',' << m << ','
        << v / m << ',' << *std::min_element(vectorTimes.begin(), vectorTimes.end())
        << ',' << *std::max_element(vectorTimes.begin(), vectorTimes.end())
        << ',' << *std::min_element(mapTimes.begin(), mapTimes.end())
        << ',' << *std::max_element(mapTimes.begin(), mapTimes.end())
        << ',' << map.bucket_count() << ',' << map.load_factor()
        << ',' << maxBucket << ',' << expected << ",0\n";
}

int main()
{
    std::cout << "MSVC=" << _MSC_FULL_VER
        << " sizeof(Enemy)=" << sizeof(Enemy)
        << " hardware_concurrency=" << std::thread::hardware_concurrency()
        << " samples=9\n";
    std::cout << "mode,count,queries,element_bytes,vector_median_ns,map_median_ns,"
        "vector_over_map,vector_min_ns,vector_max_ns,map_min_ns,map_max_ns,"
        "buckets,load_factor,max_bucket,checksum,mismatch\n";
    std::cout << std::fixed << std::setprecision(3);
    for (bool hits : {true, false})
        for (std::size_t count : {4u, 8u, 16u, 32u, 64u, 128u, 256u, 1024u, 4096u, 16384u, 50000u})
            Compare(count, hits);
}
