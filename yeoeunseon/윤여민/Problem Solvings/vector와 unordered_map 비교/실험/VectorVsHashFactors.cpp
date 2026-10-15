#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <iomanip>
#include <iostream>
#include <random>
#include <stdexcept>
#include <string>
#include <thread>
#include <type_traits>
#include <unordered_map>
#include <vector>
#define NOMINMAX
#include <Windows.h>

using Clock = std::chrono::steady_clock;
volatile std::uint64_t Sink = 0;
constexpr int Samples = 7;

template <std::size_t Bytes> struct Enemy
{
    int Id, HP;
    std::array<unsigned char, Bytes - 8> Payload{};
};
template <> struct Enemy<8> { int Id, HP; };

template <int Mode> struct Hash
{
    std::size_t operator()(int id) const noexcept
    {
        if (Mode == 1) return 0;
        std::size_t h = std::hash<int>{}(id);
        if (Mode == 2)
            for (int i = 0; i < 12; ++i)
                h = (h ^ (h >> 13)) * 0x9e3779b1u + 17;
        return h;
    }
};
struct CountEqual
{
    std::uint64_t* Count;
    bool operator()(int a, int b) const { ++*Count; return a == b; }
};

template <std::size_t Bytes>
__declspec(noinline) std::uint64_t VectorLookup(
    const std::vector<Enemy<Bytes>>& data, const std::vector<int>& queries)
{
    std::uint64_t sum = 0;
    for (int id : queries)
        for (const auto& e : data)
            if (e.Id == id) { sum += static_cast<unsigned>(e.HP); break; }
    return sum;
}
template <std::size_t Bytes, class Hasher>
__declspec(noinline) std::uint64_t MapLookup(
    const std::unordered_map<int, Enemy<Bytes>, Hasher>& data,
    const std::vector<int>& queries)
{
    std::uint64_t sum = 0;
    for (int id : queries)
    {
        auto found = data.find(id);
        if (found != data.end()) sum += static_cast<unsigned>(found->second.HP);
    }
    return sum;
}
template <std::size_t Bytes>
__declspec(noinline) std::uint64_t VectorScan(const std::vector<Enemy<Bytes>>& data)
{
    std::uint64_t sum = 0;
    for (const auto& e : data) sum += static_cast<unsigned>(e.HP);
    return sum;
}
template <std::size_t Bytes, class Hasher>
__declspec(noinline) std::uint64_t MapScan(
    const std::unordered_map<int, Enemy<Bytes>, Hasher>& data)
{
    std::uint64_t sum = 0;
    for (const auto& pair : data) sum += static_cast<unsigned>(pair.second.HP);
    return sum;
}

struct Stats { double Median, Min, Max; int Repeats; };
template <class Function> double Time(Function fn, int repeats, std::uint64_t expected)
{
    std::uint64_t last = 0;
    const auto start = Clock::now();
    for (int i = 0; i < repeats; ++i)
    {
        std::atomic_signal_fence(std::memory_order_seq_cst);
        last = fn();
        Sink = last;
    }
    const auto end = Clock::now();
    if (last != expected) throw std::runtime_error("Result mismatch");
    return std::chrono::duration<double, std::nano>(end - start).count() / repeats;
}
template <class A, class B> std::pair<Stats, Stats> Compare(
    A a, B b, std::uint64_t expected, std::size_t units)
{
    if (a() != expected || b() != expected) throw std::runtime_error("Warm-up mismatch");
    // Calibrate each implementation independently to at least ~2 ms/sample.
    // Both process identical query lists per pass; report normalized ns/query.
    const double pilotA = Time(a, 1, expected), pilotB = Time(b, 1, expected);
    const int ra = std::clamp(static_cast<int>(2000000 / std::max(pilotA, 1.0)) + 1, 1, 1000000);
    const int rb = std::clamp(static_cast<int>(2000000 / std::max(pilotB, 1.0)) + 1, 1, 1000000);
    std::vector<double> ta, tb;
    for (int s = 0; s < Samples; ++s)
    {
        double x, y;
        if (s % 2 == 0) { x = Time(a, ra, expected); y = Time(b, rb, expected); }
        else { y = Time(b, rb, expected); x = Time(a, ra, expected); }
        ta.push_back(x / units); tb.push_back(y / units);
    }
    std::sort(ta.begin(), ta.end()); std::sort(tb.begin(), tb.end());
    return {{ta[Samples / 2], ta.front(), ta.back(), ra},
            {tb[Samples / 2], tb.front(), tb.back(), rb}};
}

template <std::size_t Bytes, int Mode = 0> void Run(std::size_t n, const std::string& pattern,
                                    int hashMode = 0, float load = 1, int seed = 1,
                                    bool scan = false, int profileSeconds = 0,
                                    bool profileMap = false, std::size_t queryOverride = 0)
{
    std::mt19937 rng(20261010u + seed);
    std::vector<Enemy<Bytes>> v;
    v.reserve(n);
    for (std::size_t i = 0; i < n; ++i)
    {
        int id = static_cast<int>((static_cast<std::uint32_t>(i + 1) * 2654435761u) & 0x3fffffffu);
        v.push_back({id, static_cast<int>(100 + i % 901)});
    }
    std::shuffle(v.begin(), v.end(), rng);
    using ActiveHash = std::conditional_t<Mode == 0, std::hash<int>, Hash<Mode>>;
    std::unordered_map<int, Enemy<Bytes>, ActiveHash> m;
    m.max_load_factor(load); m.reserve(n);
    for (const auto& e : v) m.emplace(e.Id, e);
    const std::size_t qn = queryOverride ? queryOverride : (n >= 16384 ? 512 : 4096);
    std::vector<int> queries;
    std::uint64_t expected = 0, comparisons = 0;
    std::uniform_int_distribution<std::size_t> pick(0, n - 1);
    for (std::size_t q = 0; q < qn; ++q)
    {
        std::size_t p = pick(rng);
        if (pattern == "front") p = 0;
        if (pattern == "last") p = n - 1;
        if (pattern == "hot") p = n / 2;
        bool miss = pattern == "miss" || (pattern == "mixed" && q % 2);
        queries.push_back(miss ? -v[p].Id - 1 : v[p].Id);
        comparisons += miss ? n : p + 1;
        if (!miss) expected += static_cast<unsigned>(v[p].HP);
    }
    if (scan)
    {
        expected = 0;
        for (const auto& e : v) expected += static_cast<unsigned>(e.HP);
    }
    auto a = [&] { return scan ? VectorScan(v) : VectorLookup(v, queries); };
    auto b = [&] { return scan ? MapScan(m) : MapLookup(m, queries); };
    if (profileSeconds)
    {
        if (a() != expected || b() != expected)
            throw std::runtime_error("Profile warm-up mismatch");
        const auto start = Clock::now();
        const auto until = start + std::chrono::seconds(profileSeconds);
        std::uint64_t passes = 0;
        while (Clock::now() < until)
        {
            std::atomic_signal_fence(std::memory_order_seq_cst);
            Sink = profileMap ? b() : a();
            ++passes;
        }
        const double elapsed = std::chrono::duration<double>(Clock::now() - start).count();
        if (Sink != expected) throw std::runtime_error("Profile result mismatch");
        std::cout << std::setprecision(9) << "profile," << (profileMap ? "map" : "vector") << ',' << Bytes
                  << ',' << n << ",queries_per_pass=" << qn << ",passes=" << passes
                  << ",elapsed_seconds=" << elapsed << ",checksum=" << Sink
                  << ",mismatch=0\n";
        return;
    }
    auto result = Compare(a, b, expected, scan ? n : qn);
    std::uint64_t equalities = 0;
    // Count comparisons in a SEPARATE map, outside all performance samples.
    std::unordered_map<int, Enemy<Bytes>, ActiveHash, CountEqual> diagnostic(
        0, ActiveHash{}, CountEqual{&equalities});
    diagnostic.max_load_factor(load); diagnostic.reserve(n);
    for (const auto& e : v) diagnostic.emplace(e.Id, e);
    equalities = 0;
    if (!scan) for (int id : queries) (void)diagnostic.find(id);
    std::size_t biggest = 0;
    for (std::size_t bucket = 0; bucket < m.bucket_count(); ++bucket)
        biggest = std::max(biggest, m.bucket_size(bucket));
    const auto& x = result.first; const auto& y = result.second;
    std::cout << (scan ? "scan" : pattern) << ',' << n << ',' << sizeof(Enemy<Bytes>)
        << ',' << hashMode << ',' << load << ',' << seed << ',' << qn
        << ',' << x.Median << ',' << y.Median << ',' << x.Median / y.Median
        << ',' << x.Min << ',' << x.Max << ',' << y.Min << ',' << y.Max
        << ',' << x.Repeats << ',' << y.Repeats << ',' << m.bucket_count()
        << ',' << m.load_factor() << ',' << biggest
        << ',' << (scan ? 0.0 : static_cast<double>(comparisons) / qn)
        << ',' << static_cast<double>(equalities) / qn << ",0\n";
}

// String keys: same loop, varying key length. Construction is not timed.
__declspec(noinline) std::uint64_t StringVector(
    const std::vector<std::pair<std::string, int>>& v, const std::vector<std::string>& q)
{
    std::uint64_t sum = 0;
    for (const auto& key : q)
        for (const auto& e : v) if (e.first == key) { sum += e.second; break; }
    return sum;
}
__declspec(noinline) std::uint64_t StringMap(
    const std::unordered_map<std::string, int>& m, const std::vector<std::string>& q)
{
    std::uint64_t sum = 0;
    for (const auto& key : q) { auto f = m.find(key); if (f != m.end()) sum += f->second; }
    return sum;
}
void Strings(std::size_t n, int length)
{
    std::vector<std::pair<std::string, int>> v;
    std::unordered_map<std::string, int> m;
    m.reserve(n);
    for (std::size_t i = 0; i < n; ++i)
    {
        std::string key(length - 8, 'x');
        key += std::to_string(10000000 + i); // Long common prefix; difference at end.
        v.emplace_back(key, static_cast<int>(i + 100)); m.emplace(key, static_cast<int>(i + 100));
    }
    std::mt19937 rng(42);
    std::uniform_int_distribution<std::size_t> pick(0, n - 1);
    std::vector<std::string> q;
    std::uint64_t expected = 0;
    for (int i = 0; i < 2048; ++i) { auto p = pick(rng); q.push_back(v[p].first); expected += v[p].second; }
    auto r = Compare([&] { return StringVector(v, q); }, [&] { return StringMap(m, q); }, expected, q.size());
    std::cout << "string" << length << ',' << n << ",0,0,1,42,2048,"
        << r.first.Median << ',' << r.second.Median << ',' << r.first.Median / r.second.Median
        << ',' << r.first.Min << ',' << r.first.Max << ',' << r.second.Min << ',' << r.second.Max
        << ',' << r.first.Repeats << ',' << r.second.Repeats << ',' << m.bucket_count()
        << ',' << m.load_factor() << ",0,0,0,0\n";
}

__declspec(noinline) std::uint64_t KeyOnlyLookup(
    const std::vector<int>& ids, const std::vector<int>& hp, const std::vector<int>& queries)
{
    std::uint64_t sum = 0;
    for (int key : queries)
        for (std::size_t i = 0; i < ids.size(); ++i)
            if (ids[i] == key) { sum += static_cast<unsigned>(hp[i]); break; }
    return sum;
}
void LayoutFollowup(std::size_t n)
{
    std::vector<Enemy<256>> data;
    for (std::size_t i = 0; i < n; ++i) data.push_back({static_cast<int>(i + 1), 100});
    std::mt19937 rng(42); std::shuffle(data.begin(), data.end(), rng);
    std::vector<int> ids, hp, queries;
    for (const auto& e : data) { ids.push_back(e.Id); hp.push_back(e.HP); }
    std::uint64_t expected = 0;
    for (int i = 0; i < 512; ++i)
    {
        bool hit = i % 2 == 0;
        queries.push_back(hit ? ids[rng() % n] : -1);
        if (hit) expected += 100;
    }
    auto r = Compare([&] { return VectorLookup(data, queries); },
        [&] { return KeyOnlyLookup(ids, hp, queries); }, expected, queries.size());
    std::cout << "layout_followup,n=" << n << ",AoS256_ns=" << r.first.Median
        << ",IDs4_ns=" << r.second.Median << ",ratio=" << r.first.Median / r.second.Median
        << ",mismatch=0\n";
}

// Reuse the large-record experiment's keys and query list. Keep both layouts
// alive in each process so the only timed choice is which lookup to execute.
void ProfileSplitKeys(bool split, int seconds, int cpu)
{
    if (cpu < 0 || cpu >= static_cast<int>(sizeof(DWORD_PTR) * 8)
        || !SetThreadAffinityMask(GetCurrentThread(), DWORD_PTR{1} << cpu))
        throw std::runtime_error("Cannot set split-profile affinity");
    constexpr std::size_t n = 50000, queryCount = 512;
    std::mt19937 rng(20261011u);
    std::vector<Enemy<256>> data;
    data.reserve(n);
    for (std::size_t i = 0; i < n; ++i)
    {
        const int id = static_cast<int>((static_cast<std::uint32_t>(i + 1) * 2654435761u) & 0x3fffffffu);
        data.push_back({id, static_cast<int>(100 + i % 901)});
    }
    std::shuffle(data.begin(), data.end(), rng);
    std::vector<int> ids, hp, queries;
    ids.reserve(n); hp.reserve(n); queries.reserve(queryCount);
    for (const auto& e : data) { ids.push_back(e.Id); hp.push_back(e.HP); }
    std::uniform_int_distribution<std::size_t> pick(0, n - 1);
    std::uint64_t expected = 0, comparisons = 0;
    for (std::size_t q = 0; q < queryCount; ++q)
    {
        const auto position = pick(rng);
        const bool miss = q % 2;
        queries.push_back(miss ? -data[position].Id - 1 : data[position].Id);
        comparisons += miss ? n : position + 1;
        if (!miss) expected += static_cast<unsigned>(data[position].HP);
    }
    if (VectorLookup(data, queries) != expected || KeyOnlyLookup(ids, hp, queries) != expected)
        throw std::runtime_error("Split-profile warm-up mismatch");
    // Building/synchronizing the split arrays is outside the measured loop.
    const auto start = Clock::now();
    const auto until = start + std::chrono::seconds(seconds);
    std::uint64_t passes = 0, last = 0;
    while (Clock::now() < until)
    {
        std::atomic_signal_fence(std::memory_order_seq_cst);
        last = split ? KeyOnlyLookup(ids, hp, queries) : VectorLookup(data, queries);
        Sink = last;
        ++passes;
    }
    const double elapsed = std::chrono::duration<double>(Clock::now() - start).count();
    if (last != expected) throw std::runtime_error("Split-profile result mismatch");
    std::cout << std::setprecision(9) << "split_profile," << (split ? "ids4" : "aos256")
              << ",n=" << n << ",queries_per_pass=" << queryCount << ",passes=" << passes
              << ",elapsed_seconds=" << elapsed << ",checksum=" << last
              << ",comparisons_per_query=" << static_cast<double>(comparisons) / queryCount
              << ",cpu=" << GetCurrentProcessorNumber() << ",mismatch=0\n";
}

__declspec(noinline) std::uint64_t BuildVector(const std::vector<Enemy<8>>& input, bool reserve)
{
    std::vector<Enemy<8>> v;
    if (reserve) v.reserve(input.size());
    for (const auto& e : input) v.push_back(e);
    return VectorScan(v); // Includes allocation, population, scan, destruction.
}
__declspec(noinline) std::uint64_t BuildMap(const std::vector<Enemy<8>>& input, bool reserve)
{
    std::unordered_map<int, Enemy<8>> m;
    if (reserve) m.reserve(input.size());
    for (const auto& e : input) m.emplace(e.Id, e);
    return MapScan(m);
}
void Mutations(std::size_t n)
{
    std::vector<Enemy<8>> input;
    std::uint64_t sum = 0, remaining = 0;
    for (std::size_t i = 0; i < n; ++i)
    { input.push_back({static_cast<int>(i + 1), 100}); sum += 100; if (i % 2) remaining += 100; }
    for (bool reserve : {false, true})
    {
        auto r = Compare([&] { return BuildVector(input, reserve); },
                         [&] { return BuildMap(input, reserve); }, sum, n);
        std::cout << "build_cycle,n=" << n << ",reserve=" << reserve
            << ",vector_ns_per_input=" << r.first.Median << ",map_ns_per_input=" << r.second.Median
            << ",mismatch=0\n";
    }
    // Set up independent copies BEFORE timing. Preserve vector order on erase.
    const std::size_t copies = n == 16 ? 256 : 2;
    std::vector<double> tv, tm;
    for (int s = 0; s < Samples; ++s)
    {
        std::vector<std::vector<Enemy<8>>> vectors(copies, input);
        std::vector<std::unordered_map<int, Enemy<8>>> maps(copies);
        for (auto& m : maps) { m.reserve(n); for (const auto& e : input) m.emplace(e.Id, e); }
        auto eraseV = [&]
        {
            auto start = Clock::now();
            for (auto& v : vectors)
                for (std::size_t i = 0; i < n; i += 2)
                {
                    auto found = std::find_if(v.begin(), v.end(), [&](const auto& e) { return e.Id == input[i].Id; });
                    v.erase(found);
                }
            return std::chrono::duration<double, std::nano>(Clock::now() - start).count();
        };
        auto eraseM = [&]
        {
            auto start = Clock::now();
            for (auto& m : maps) for (std::size_t i = 0; i < n; i += 2) m.erase(input[i].Id);
            return std::chrono::duration<double, std::nano>(Clock::now() - start).count();
        };
        double a, b;
        if (s % 2 == 0) { a = eraseV(); b = eraseM(); } else { b = eraseM(); a = eraseV(); }
        for (const auto& v : vectors) if (v.size() != n / 2 || VectorScan(v) != remaining) throw std::runtime_error("Erase mismatch");
        for (const auto& m : maps) if (m.size() != n / 2 || MapScan(m) != remaining) throw std::runtime_error("Erase mismatch");
        tv.push_back(a / (copies * (n / 2))); tm.push_back(b / (copies * (n / 2)));
    }
    std::sort(tv.begin(), tv.end()); std::sort(tm.begin(), tm.end());
    std::cout << "erase_by_id,n=" << n << ",vector_ns_per_erase=" << tv[Samples / 2]
        << ",map_ns_per_erase=" << tm[Samples / 2] << ",mismatch=0\n";
}

// A fixed query array can be cheap because of its repeatable order, because it
// touches few keys, or both. Keep array length fixed and vary these separately.
void QueryPatternFollowup(bool referenceControl = false)
{
    std::cout << "case,n,queries,unique_queries,vector_comparisons,map_equalities,vector_ns,map_ns,mismatch";
    if (referenceControl) std::cout << ",reference_before_ns,reference_after_ns";
    std::cout << '\n';
    auto experiment = [referenceControl](std::size_t n, std::size_t active, bool mixed, int seed, bool reorder)
    {
        std::mt19937 rng(20261010u + seed);
        std::vector<Enemy<8>> data;
        for (std::size_t i = 0; i < n; ++i)
            data.push_back({static_cast<int>(i + 1), static_cast<int>(100 + i % 901)});
        std::shuffle(data.begin(), data.end(), rng);
        std::unordered_map<int, Enemy<8>> map;
        map.reserve(n);
        std::uint64_t equalities = 0;
        std::unordered_map<int, Enemy<8>, std::hash<int>, CountEqual> diagnostic(
            0, std::hash<int>{}, CountEqual{&equalities});
        diagnostic.reserve(n);
        for (const auto& e : data) { map.emplace(e.Id, e); diagnostic.emplace(e.Id, e); }

        constexpr std::size_t queryCount = 65536;
        std::vector<int> queries;
        std::uint64_t expected = 0, comparisons = 0;
        // For order tests, repeat the SAME 512-query motif in a long array.
        // Then shuffle/group that entire array without changing its multiset.
        const std::size_t motifSize = reorder ? 512 : queryCount;
        for (std::size_t i = 0; i < motifSize; ++i)
        {
            const std::size_t position = (static_cast<std::size_t>(rng()) % active) * n / active;
            const bool miss = mixed && i % 2;
            queries.push_back(miss ? -data[position].Id : data[position].Id);
            comparisons += miss ? n : position + 1;
            if (!miss) expected += static_cast<unsigned>(data[position].HP);
        }
        const auto motif = queries;
        const std::size_t copies = queryCount / motifSize;
        for (std::size_t i = 1; i < copies; ++i)
            queries.insert(queries.end(), motif.begin(), motif.end());
        expected *= copies;
        comparisons *= copies;
        auto sorted = queries;
        std::sort(sorted.begin(), sorted.end());
        const auto unique = static_cast<std::size_t>(std::unique(sorted.begin(), sorted.end()) - sorted.begin());

        const auto referenceQueries = queries;
        for (int order = 0; order < (reorder ? 3 : 1); ++order)
        {
            if (order == 1) std::shuffle(queries.begin(), queries.end(), rng);
            if (order == 2) std::sort(queries.begin(), queries.end());
            equalities = 0;
            for (int key : queries) (void)diagnostic.find(key);
            // The reference never changes order. If it also slows down, the
            // candidate's delta cannot all be assigned to its query order.
            const double before = referenceControl ? Time([&] { return VectorLookup(data, referenceQueries); }, 1, expected) / queryCount : 0;
            auto r = Compare([&] { return VectorLookup(data, queries); },
                             [&] { return MapLookup(map, queries); }, expected, queries.size());
            const double after = referenceControl ? Time([&] { return VectorLookup(data, referenceQueries); }, 1, expected) / queryCount : 0;
            const char* label = reorder ? (order == 0 ? "periodic" : order == 1 ? "shuffled" : "grouped") : "key_coverage";
            std::cout << label << (mixed ? "_mixed" : "_hit") << "_seed" << seed
                << "_active" << active << ',' << n << ',' << queries.size() << ',' << unique
                << ',' << static_cast<double>(comparisons) / queryCount
                << ',' << static_cast<double>(equalities) / queryCount
                << ',' << r.first.Median << ',' << r.second.Median << ",0";
            if (referenceControl) std::cout << ',' << before << ',' << after;
            std::cout << '\n';
        }
    };
    for (int seed : {1, 2})
        for (auto n : {8u, 16u, 4096u})
            for (bool mixed : {false, true})
                if (!referenceControl || n == 4096) experiment(n, n, mixed, seed, true);
    // Same 256 KiB query-array footprint. Change the pool of searchable keys.
    // This changes candidate-node coverage and key frequencies, not just cache.
    if (!referenceControl)
        for (auto active : {8u, 64u, 512u, 4096u, 50000u})
            experiment(50000, active, false, 1, false);
}

// Match the 8-element mixed-query follow-up exactly. Only query order changes;
// each pass contains the same 65,536 queries and must produce the same checksum.
void ProfileQueryOrder(bool useMap, bool shuffled, int seconds)
{
    constexpr std::size_t n = 8, queryCount = 65536, motifSize = 512;
    std::mt19937 rng(20261011u);
    std::vector<Enemy<8>> data;
    for (std::size_t i = 0; i < n; ++i)
        data.push_back({static_cast<int>(i + 1), static_cast<int>(100 + i)});
    std::shuffle(data.begin(), data.end(), rng);
    std::unordered_map<int, Enemy<8>> map;
    map.reserve(n);
    for (const auto& e : data) map.emplace(e.Id, e);
    std::vector<int> queries;
    std::uint64_t expected = 0;
    for (std::size_t i = 0; i < motifSize; ++i)
    {
        const auto position = static_cast<std::size_t>(rng()) % n;
        const bool miss = i % 2;
        queries.push_back(miss ? -data[position].Id : data[position].Id);
        if (!miss) expected += static_cast<unsigned>(data[position].HP);
    }
    const auto motif = queries;
    for (std::size_t i = 1; i < queryCount / motifSize; ++i)
        queries.insert(queries.end(), motif.begin(), motif.end());
    expected *= queryCount / motifSize;
    if (shuffled) std::shuffle(queries.begin(), queries.end(), rng);
    if (VectorLookup(data, queries) != expected || MapLookup(map, queries) != expected)
        throw std::runtime_error("Profile warm-up mismatch");
    const auto start = Clock::now();
    const auto until = start + std::chrono::seconds(seconds);
    std::uint64_t passes = 0, last = 0;
    while (Clock::now() < until)
    {
        std::atomic_signal_fence(std::memory_order_seq_cst);
        last = useMap ? MapLookup(map, queries) : VectorLookup(data, queries);
        Sink = last;
        ++passes;
    }
    const double elapsed = std::chrono::duration<double>(Clock::now() - start).count();
    if (last != expected) throw std::runtime_error("Profile result mismatch");
    std::cout << "order_profile," << (useMap ? "map" : "vector") << ','
              << (shuffled ? "shuffled" : "periodic") << ",n=" << n
              << ",queries_per_pass=" << queryCount << ",passes=" << passes
              << ",elapsed_seconds=" << elapsed << ",checksum=" << last
              << ",mismatch=0\n";
}

// Same map, successful-query count and query-array footprint. Broaden only
// the pool of keys requested. This is a warm repeated workload, not cold-cache.
void ProfileMapCoverage(std::size_t active, int seconds, int cpu)
{
    constexpr std::size_t n = 50000, queryCount = 65536;
    if ((active != 512 && active != n) || seconds < 1 || cpu < 0
        || cpu >= static_cast<int>(sizeof(DWORD_PTR) * 8)
        || !SetThreadAffinityMask(GetCurrentThread(), DWORD_PTR{1} << cpu))
        throw std::runtime_error("Invalid coverage arguments or affinity");
    std::unordered_map<int, Enemy<8>> map;
    map.max_load_factor(1.0f);
    map.reserve(n);
    for (std::size_t i = 0; i < n; ++i)
        map.emplace(static_cast<int>(i + 1), Enemy<8>{static_cast<int>(i + 1), static_cast<int>(100 + i % 901)});
    std::mt19937 rng(20261011u);
    std::vector<int> queries;
    std::vector<bool> seen(n, false);
    std::uint64_t expected = 0;
    std::size_t unique = 0;
    for (std::size_t i = 0; i < queryCount; ++i)
    {
        const auto position = (static_cast<std::size_t>(rng()) % active) * n / active;
        queries.push_back(static_cast<int>(position + 1));
        expected += static_cast<unsigned>(100 + position % 901);
        if (!seen[position]) { seen[position] = true; ++unique; }
    }
    // Count candidate comparisons outside the timed loop, with identical keys,
    // insertion order and bucket policy. Destroy the diagnostic map afterwards.
    std::uint64_t equalities = 0;
    {
        std::unordered_map<int, Enemy<8>, std::hash<int>, CountEqual> diagnostic(
            0, std::hash<int>{}, CountEqual{&equalities});
        diagnostic.max_load_factor(1.0f);
        diagnostic.reserve(n);
        for (std::size_t i = 0; i < n; ++i)
            diagnostic.emplace(static_cast<int>(i + 1), Enemy<8>{static_cast<int>(i + 1), static_cast<int>(100 + i % 901)});
        equalities = 0;
        for (int key : queries) (void)diagnostic.find(key);
        if (diagnostic.bucket_count() != map.bucket_count())
            throw std::runtime_error("Diagnostic bucket mismatch");
    }
    if (MapLookup(map, queries) != expected) throw std::runtime_error("Coverage warm-up mismatch");
    const auto start = Clock::now(), until = start + std::chrono::seconds(seconds);
    std::uint64_t passes = 0, last = 0;
    while (Clock::now() < until)
    {
        std::atomic_signal_fence(std::memory_order_seq_cst);
        last = MapLookup(map, queries);
        Sink = last;
        ++passes;
    }
    const double elapsed = std::chrono::duration<double>(Clock::now() - start).count();
    if (last != expected) throw std::runtime_error("Coverage result mismatch");
    std::cout << std::setprecision(9) << "coverage_profile,active=" << active
        << ",n=" << n << ",queries_per_pass=" << queryCount << ",unique=" << unique
        << ",passes=" << passes << ",elapsed_seconds=" << elapsed
        << ",checksum=" << last << ",equalities_per_query=" << static_cast<double>(equalities) / queryCount
        << ",buckets=" << map.bucket_count() << ",load=" << map.load_factor()
        << ",cpu=" << GetCurrentProcessorNumber() << ",mismatch=0\n";
}

int main(int argc, char** argv)
{
    std::cout << std::fixed << std::setprecision(3);
    if (argc > 1 && std::string(argv[1]) == "profile-coverage")
    {
        if (argc != 5) throw std::runtime_error("Usage: profile-coverage 512|50000 seconds logical_cpu_index");
        ProfileMapCoverage(static_cast<std::size_t>(std::atoi(argv[2])), std::atoi(argv[3]), std::atoi(argv[4]));
        return 0;
    }
    if (argc > 1 && std::string(argv[1]) == "profile-split")
    {
        if (argc != 5 || (std::string(argv[2]) != "aos256" && std::string(argv[2]) != "ids4")
            || std::atoi(argv[3]) < 1)
            throw std::runtime_error("Usage: profile-split aos256|ids4 seconds logical_cpu_index");
        ProfileSplitKeys(std::string(argv[2]) == "ids4", std::atoi(argv[3]), std::atoi(argv[4]));
        return 0;
    }
    if (argc > 1 && std::string(argv[1]) == "profile-order")
    {
        if (argc != 5 || (std::string(argv[2]) != "map" && std::string(argv[2]) != "vector")
            || (std::string(argv[3]) != "periodic" && std::string(argv[3]) != "shuffled")
            || std::atoi(argv[4]) < 1)
            throw std::runtime_error("Usage: profile-order vector|map periodic|shuffled seconds");
        ProfileQueryOrder(std::string(argv[2]) == "map", std::string(argv[3]) == "shuffled", std::atoi(argv[4]));
        return 0;
    }
    if (argc > 1 && std::string(argv[1]) == "patterns-control")
    {
        QueryPatternFollowup(true);
        return 0;
    }
    if (argc > 1 && std::string(argv[1]) == "patterns")
    {
        QueryPatternFollowup();
        return 0;
    }
    // Follow-up: keep the container size fixed, broaden the queried key set.
    // This is still a warm workload; it does not force cache eviction.
    if (argc > 1 && std::string(argv[1]) == "coverage")
    {
        std::cout << "query_coverage_followup,MSVC=" << _MSC_FULL_VER << '\n';
        for (auto n : {4096u, 50000u})
            for (auto q : {512u, 65536u})
                for (const std::string p : {"hit", "mixed"})
                    Run<8>(n, p, 0, 1, 1, false, 0, false, q);
        return 0;
    }
    if (argc > 1 && std::string(argv[1]) == "profile")
    {
        if (argc < 6 || argc > 7 || (std::string(argv[2]) != "map" && std::string(argv[2]) != "vector")
            || (std::atoi(argv[3]) != 8 && std::atoi(argv[3]) != 256)
            || std::atoi(argv[4]) < 1 || std::atoi(argv[5]) < 1)
            throw std::runtime_error("Usage: profile vector|map 8|256 count seconds [logical_cpu_index]");
        // Pin only this benchmark thread, never the user's editor or profiler.
        // Hybrid-CPU counters differ by core type; keep the same core for A/B.
        if (argc == 7)
        {
            const int cpu = std::atoi(argv[6]);
            if (cpu < 0 || cpu >= static_cast<int>(sizeof(DWORD_PTR) * 8)
                || !SetThreadAffinityMask(GetCurrentThread(), DWORD_PTR{1} << cpu))
                throw std::runtime_error("Cannot set profile thread affinity");
            std::cout << "profile_affinity,requested_cpu=" << cpu
                      << ",current_cpu=" << GetCurrentProcessorNumber() << '\n';
        }
        bool map = std::string(argv[2]) == "map";
        int bytes = std::atoi(argv[3]);
        auto n = static_cast<std::size_t>(std::atoi(argv[4]));
        int seconds = std::atoi(argv[5]);
        if (bytes == 8) Run<8>(n, "mixed", 0, 1, 1, false, seconds, map);
        else Run<256>(n, "mixed", 0, 1, 1, false, seconds, map);
        return 0;
    }
    std::cout << "MSVC=" << _MSC_FULL_VER << " hardware_concurrency="
        << std::thread::hardware_concurrency() << " samples=" << Samples << '\n';
    std::cout << "pattern,n,bytes,hash,requested_load,seed,queries,vector_ns,map_ns,ratio,"
        "vector_min,vector_max,map_min,map_max,vector_repeats,map_repeats,buckets,actual_load,"
        "max_bucket,vector_comparisons,map_equalities,mismatch\n";
    // 1. Size and distribution; three data/query seeds, not one lucky list.
    for (int seed : {1, 2, 3})
        for (auto n : {8u, 16u, 32u, 128u, 1024u, 4096u, 50000u})
            for (const std::string p : {"hit", "miss", "mixed"}) Run<8>(n, p, 0, 1, seed);
    // 2. Same keys/order/queries, change only payload size.
    for (auto n : {16u, 256u, 4096u, 50000u})
    {
        Run<8>(n, "mixed"); Run<64>(n, "mixed"); Run<256>(n, "mixed");
    }
    // 3. Where/how often the successful key is found.
    for (auto n : {16u, 128u, 4096u})
        for (const std::string p : {"front", "last", "hot", "hit"}) Run<8>(n, p);
    // 4. Bucket budget, pathological collisions, and hash computation cost.
    for (float load : {0.25f, 1.0f, 4.0f}) Run<8>(4096, "mixed", 0, load);
    for (auto n : {16u, 256u, 4096u})
    { Run<8>(n, "mixed"); Run<8, 1>(n, "mixed", 1); Run<8, 2>(n, "mixed", 2); }
    // 5. Full scans are a different operation from ID lookup.
    for (auto n : {16u, 4096u, 50000u})
    { Run<8>(n, "hit", 0, 1, 1, true); Run<256>(n, "hit", 0, 1, 1, true); }
    // 6. String key hashing/comparison, 8-byte vs 128-byte keys.
    for (auto n : {16u, 128u, 4096u}) { Strings(n, 8); Strings(n, 128); }
    // 7. Follow-up: repeated short list vs long list, at the SAME data size.
    for (auto n : {8u, 16u})
        for (auto q : {4096u, 262144u})
            for (const std::string p : {"hit", "miss", "mixed"})
                Run<8>(n, p, 0, 1, 1, false, 0, false, q);
    // 8. Same scan semantics, same Id order: change key stride only.
    for (auto n : {4096u, 50000u}) LayoutFollowup(n);
    // 9. Construction cycles and order-preserving erase-by-ID.
    for (auto n : {16u, 4096u, 50000u}) Mutations(n);
}
