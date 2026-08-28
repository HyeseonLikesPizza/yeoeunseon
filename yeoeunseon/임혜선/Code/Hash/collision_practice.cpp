// =====================================================================
//  collision_practice.cpp
//
//  [ 연습 주제 ]
//    해시 충돌이 심하게 일어나는 상황을 직접 만들어보고,
//    해시 함수를 개선해서 버킷에 데이터가 고르게 퍼지도록 고쳐본다.
//
//  [ 상황 설정 ]
//    사번 문자열 "EMP-2024-0001" ~ "EMP-2024-5000" 을 키로 쓴다.
//    이 키들은 앞부분("EMP-2024-")이 전부 똑같고 뒤 4글자만 다르다.
//    (사번, 주문번호, 차량번호, MAC 주소 등 실무에서 아주 흔한 패턴)
//
//    std::unordered_map 은 내부 해시가 이미 잘 만들어져 있어서
//    충돌 실험이 안 된다. 그래서 체이닝 방식 해시테이블을 직접 만든다.
//
//  [ 할 일 ]
//    아래 TODO 1, TODO 2 두 곳을 채우면 된다.
//
//  [ 컴파일 ]
//    g++ -std=c++17 collision_practice.cpp -o collision
//    (Visual Studio 라면 이 파일 하나만 프로젝트에 넣고 빌드)
// =====================================================================

#include <iostream>
#include <iomanip>
#include <string>
#include <vector>
#include <list>
#include <cmath>
#include <algorithm>

// ---------------------------------------------------------------------
//  1. 나쁜 해시 함수 (이미 완성됨 - 비교 대상)
// ---------------------------------------------------------------------

// 나쁜 해시 #1 : 문자열 앞 4글자만 사용한다.
//   -> "EMP-" 가 전부 똑같으므로 모든 키가 같은 값을 뱉는다.
//      즉 5000개 전부 단 하나의 버킷으로 들어간다. 최악의 경우.
std::size_t badHash_prefix(const std::string& key)
{
    std::size_t h = 0;
    std::size_t n = std::min<std::size_t>(4, key.size());
    for (std::size_t i = 0; i < n; ++i)
        h += static_cast<unsigned char>(key[i]);
    return h;
}

// 나쁜 해시 #2 : 모든 문자의 ASCII 값을 그냥 더한다.
//   -> 순서를 무시하므로 "AB" 와 "BA" 가 충돌한다.
//   -> 더 큰 문제는 값의 "범위"다. 문자 하나가 최대 127 정도이고
//      키 길이가 13글자면 합계는 대략 1000~1300 사이에만 머문다.
//      버킷이 1024개여도 실제로 쓰이는 건 좁은 구간뿐이라 몰림이 생긴다.
std::size_t badHash_sum(const std::string& key)
{
    std::size_t h = 0;
    for (char c : key)
        h += static_cast<unsigned char>(c);
    return h;
}

// ---------------------------------------------------------------------
//  2. TODO 1 : 좋은 해시 함수를 직접 작성해보자
// ---------------------------------------------------------------------

// [ 목표 ]
//   같은 데이터 5000개, 같은 버킷 개수인데도
//   최장 체인 길이가 5000 -> 10 이하로 줄어들게 만든다.
//
// [ 좋은 해시가 갖춰야 할 성질 ]
//   (1) 키의 "모든" 문자를 사용한다.
//       - 앞 4글자만 보면 접두사가 같은 키를 구분할 수 없다.
//   (2) 문자의 "순서"가 결과에 반영돼야 한다.
//       - 단순 덧셈은 순서를 잃는다. 이전 결과에 곱셈을 섞어야 한다.
//   (3) 결과가 넓은 범위에 퍼져야 한다 (avalanche, 눈사태 효과).
//       - 입력이 1비트만 달라져도 출력의 절반 정도 비트가 바뀌는 게 이상적.
//       - 곱셈은 하위 비트의 변화를 상위 비트까지 밀어올려 준다.
//       - XOR 은 비트를 골고루 뒤집어 준다.
// 
// [ 힌트 : FNV-1a 알고리즘 ]
//   널리 쓰이는 아주 짧은 해시다. 의사코드는 다음과 같다.
//
//       h = FNV_OFFSET_BASIS          // 시작값 (아래 상수 사용)
//       for (문자 c : key)
//           h = h XOR c               // 먼저 XOR 로 섞고
//           h = h * FNV_PRIME         // 그 다음 소수를 곱해서 퍼뜨린다
//       return h
//
//   XOR 을 먼저 하고 곱셈을 나중에 하는 순서가 중요하다.
//   (곱셈 먼저 하는 버전은 FNV-1 이고, FNV-1a 가 분포가 더 좋다.)
//
// [ 참고 : djb2 로 해도 된다 ]
//       h = 5381
//       for (문자 c : key)
//           h = h * 33 + c            // h * 33 은 (h << 5) + h 와 같다
//       return h
//
// [ 주의 ]
//   char 는 컴파일러/플랫폼에 따라 signed 일 수 있다.
//   한글이나 0x80 이상 바이트가 들어오면 음수가 되어 결과가 달라진다.
//   반드시 static_cast<unsigned char>(c) 로 변환해서 쓸 것.

std::size_t goodHash(const std::string& key)
{
    // 64비트 FNV-1a 상수
    const std::size_t FNV_OFFSET_BASIS = 1469598103934665603ULL;
    const std::size_t FNV_PRIME        = 1099511628211ULL;

    // ------------------ TODO 1 : 여기를 채우세요 ------------------
    //
    // 위 힌트의 의사코드를 그대로 옮기면 된다.
    // 지금은 컴파일만 되도록 0 을 반환하고 있다.
    //
    // 다 작성했으면 실행해서, 출력 표의 "최장 체인" 값이
    // 5000 이나 수백 단위에서 한 자리 수로 떨어지는지 확인할 것.

    auto h = FNV_OFFSET_BASIS;
    
    for (auto ch : key)
    {
        h = h xor ch;
        h = h * FNV_PRIME;
    }

    return h;

    // ------------------------------------------------------------
}

// ---------------------------------------------------------------------
//  3. 체이닝 해시테이블 (이미 완성됨)
//
//     버킷 하나가 연결 리스트를 갖고, 충돌한 키들은 그 리스트에 이어 붙는다.
//     이게 std::unordered_map 이 실제로 쓰는 방식이기도 하다.
// ---------------------------------------------------------------------

class ChainedHashTable
{
public:
    // 해시 함수를 함수 포인터로 받는다.
    // 같은 테이블 구조에 해시만 갈아끼워 비교하기 위해서다.
    using HashFunc = std::size_t(*)(const std::string&);

    ChainedHashTable(std::size_t bucketCount, HashFunc fn)
        : buckets_(bucketCount), hashFn_(fn), probeCount_(0)
    {
    }

    void insert(const std::string& key, int value)
    {
        std::size_t idx = bucketIndex(key);
        for (auto& kv : buckets_[idx])
        {
            if (kv.first == key)        // 이미 있으면 값만 갱신
            {
                kv.second = value;
                return;
            }
        }
        buckets_[idx].emplace_back(key, value);
    }

    // 탐색. 찾으면 true 를 반환하고 out 에 값을 채운다.
    // 이 과정에서 몇 번 비교했는지를 probeCount_ 에 누적한다.
    // -> 이 숫자가 곧 "해시가 얼마나 좋은가"의 실제 비용이다.
    bool find(const std::string& key, int& out)
    {
        std::size_t idx = bucketIndex(key);
        for (const auto& kv : buckets_[idx])
        {
            ++probeCount_;              // 키를 하나 비교할 때마다 +1
            if (kv.first == key)
            {
                out = kv.second;
                return true;
            }
        }
        return false;
    }

    // --------- 통계 ---------

    std::size_t longestChain() const
    {
        std::size_t maxLen = 0;
        for (const auto& b : buckets_)
            maxLen = std::max(maxLen, b.size());
        return maxLen;
    }

    std::size_t emptyBuckets() const
    {
        std::size_t cnt = 0;
        for (const auto& b : buckets_)
            if (b.empty()) ++cnt;
        return cnt;
    }

    std::size_t usedBuckets() const
    {
        return buckets_.size() - emptyBuckets();
    }

    double averageChain() const
    {
        std::size_t used = usedBuckets();
        if (used == 0) return 0.0;
        return static_cast<double>(totalItems()) / static_cast<double>(used);
    }

    // 표준편차. 0 에 가까울수록 버킷마다 고르게 들어갔다는 뜻.
    double stdDeviation() const
    {
        double mean = static_cast<double>(totalItems())
                    / static_cast<double>(buckets_.size());
        double sum = 0.0;
        for (const auto& b : buckets_)
        {
            double d = static_cast<double>(b.size()) - mean;
            sum += d * d;
        }
        return std::sqrt(sum / static_cast<double>(buckets_.size()));
    }

    std::size_t totalItems() const
    {
        std::size_t n = 0;
        for (const auto& b : buckets_) n += b.size();
        return n;
    }

    std::size_t bucketCount()  const { return buckets_.size(); }
    std::size_t probeCount()   const { return probeCount_; }
    void        resetProbes()        { probeCount_ = 0; }

private:
    std::size_t bucketIndex(const std::string& key) const
    {
        // 해시값을 버킷 개수로 나눈 나머지가 버킷 번호가 된다.
        //
        // 참고 : 버킷 개수가 2의 거듭제곱이면 % 대신 & (bucketCount-1) 로
        //        더 빠르게 계산할 수 있지만, 그 경우 해시의 "하위 비트"만
        //        쓰게 되므로 해시 품질이 더욱 중요해진다.
        //        여기서는 이해하기 쉽도록 % 를 쓴다.
        return hashFn_(key) % buckets_.size();
    }

    std::vector<std::list<std::pair<std::string, int>>> buckets_;
    HashFunc    hashFn_;
    std::size_t probeCount_;
};

// ---------------------------------------------------------------------
//  4. 테스트 데이터 생성 & 결과 출력 (이미 완성됨)
// ---------------------------------------------------------------------

std::vector<std::string> makeEmployeeIds(int count)
{
    std::vector<std::string> ids;
    ids.reserve(count);

    for (int i = 1; i <= count; ++i)
    {
        // "EMP-2024-0001" 형태로 만든다.
        std::string num = std::to_string(i);
        while (num.size() < 4) num = "0" + num;   // 4자리로 0 채우기
        ids.push_back("EMP-2024-" + num);
    }
    return ids;
}

struct Result
{
    std::string label;
    std::size_t longest;
    std::size_t used;
    std::size_t buckets;
    double      avg;
    double      stddev;
    double      avgProbe;
};

Result runTest(const std::string& label,
               ChainedHashTable::HashFunc fn,
               const std::vector<std::string>& keys,
               std::size_t bucketCount)
{
    ChainedHashTable table(bucketCount, fn);

    // 넣기
    for (std::size_t i = 0; i < keys.size(); ++i)
        table.insert(keys[i], static_cast<int>(i));

    // 모든 키를 한 번씩 찾아보면서 비교 횟수를 센다
    table.resetProbes();
    int out = 0;
    for (const auto& k : keys)
        table.find(k, out);

    Result r;
    r.label    = label;
    r.longest  = table.longestChain();
    r.used     = table.usedBuckets();
    r.buckets  = table.bucketCount();
    r.avg      = table.averageChain();
    r.stddev   = table.stdDeviation();
    r.avgProbe = static_cast<double>(table.probeCount())
               / static_cast<double>(keys.size());
    return r;
}

void printHeader()
{
    std::cout << std::left
              << std::setw(26) << "hash function"
              << std::right
              << std::setw(10) << "maxChain"
              << std::setw(14) << "usedBuckets"
              << std::setw(12) << "avgChain"
              << std::setw(12) << "stddev"
              << std::setw(14) << "avgProbe"
              << "\n";
    std::cout << std::string(90, '-') << "\n";
}

void printResult(const Result& r)
{
    std::string usedStr = std::to_string(r.used) + " / " + std::to_string(r.buckets);

    std::cout << std::left
              << std::setw(26) << r.label
              << std::right
              << std::setw(10) << r.longest
              << std::setw(14) << usedStr
              << std::setw(12) << std::fixed << std::setprecision(2) << r.avg
              << std::setw(12) << std::fixed << std::setprecision(2) << r.stddev
              << std::setw(14) << std::fixed << std::setprecision(2) << r.avgProbe
              << "\n";
}

// ---------------------------------------------------------------------
//  main
// ---------------------------------------------------------------------

int main()
{
    const int         DATA_COUNT   = 5000;
    const std::size_t BUCKET_COUNT = 1024;

    std::vector<std::string> keys = makeEmployeeIds(DATA_COUNT);

    std::cout << "\n";
    std::cout << "===========================================================================\n";
    std::cout << " 해시 충돌 실험\n";
    std::cout << "===========================================================================\n";
    std::cout << " 데이터   : " << DATA_COUNT << " 개  ("
              << keys.front() << " ~ " << keys.back() << ")\n";
    std::cout << " 버킷     : " << BUCKET_COUNT << " 개\n";
    std::cout << " 이상적   : 버킷당 약 "
              << std::fixed << std::setprecision(1)
              << static_cast<double>(DATA_COUNT) / BUCKET_COUNT
              << " 개씩 고르게 분포\n\n";

    printHeader();
    printResult(runTest("badHash_prefix (first 4)", badHash_prefix, keys, BUCKET_COUNT));
    printResult(runTest("badHash_sum (ASCII sum)",  badHash_sum,    keys, BUCKET_COUNT));
    printResult(runTest("goodHash (YOUR CODE)",     goodHash,       keys, BUCKET_COUNT));

    std::cout << "\n";
    std::cout << "---------------------------------------------------------------------------\n";
    std::cout << " 읽는 법\n";
    std::cout << "---------------------------------------------------------------------------\n";
    std::cout << " maxChain    : 가장 붐비는 버킷의 원소 수. 최악의 탐색 비용.\n";
    std::cout << " usedBuckets : 실제로 쓰인 버킷 / 전체 버킷. 1024/1024 에 가까울수록 좋다.\n";
    std::cout << " avgChain    : 원소가 들어있는 버킷들의 평균 체인 길이.\n";
    std::cout << " stddev      : 버킷별 원소 수의 편차. 0 에 가까울수록 고르다.\n";
    std::cout << " avgProbe    : 조회 1회당 실제로 문자열을 몇 번 비교했는가.\n";
    std::cout << "               이게 1 에 가까워야 진짜 O(1) 이다.\n\n";
    std::cout << " goodHash 를 채우기 전에는 값이 0 으로 나온다.\n";
    std::cout << " (모든 키가 0 번 버킷으로 가므로 badHash_prefix 와 같은 최악의 결과)\n\n";

    return 0;
}

// =====================================================================
//  [ 정답 ]  goodHash 본체 - 스스로 풀어본 뒤에 열어볼 것
// =====================================================================
//
//  std::size_t goodHash(const std::string& key)
//  {
//      const std::size_t FNV_OFFSET_BASIS = 1469598103934665603ULL;
//      const std::size_t FNV_PRIME        = 1099511628211ULL;
//
//      std::size_t h = FNV_OFFSET_BASIS;
//      for (char c : key)
//      {
//          h ^= static_cast<unsigned char>(c);   // 먼저 XOR
//          h *= FNV_PRIME;                       // 그 다음 곱셈
//      }
//      return h;
//  }
//
//  djb2 버전도 결과가 거의 비슷하게 좋다.
//
//  std::size_t goodHash(const std::string& key)
//  {
//      std::size_t h = 5381;
//      for (char c : key)
//          h = h * 33 + static_cast<unsigned char>(c);
//      return h;
//  }
//
// ---------------------------------------------------------------------
//  [ 예상 결과 ]
//
//   hash function            maxChain  usedBuckets  avgChain   stddev  avgProbe
//   badHash_prefix (first 4)     5000     1 / 1024   5000.00   156.17   2500.50
//   badHash_sum (ASCII sum)       365    31 / 1024    161.29    35.75    133.78
//   goodHash (FNV-1a)               9  1022 / 1024      4.89     1.53      3.18
//
//   -> 조회 1번에 문자열 비교를 2500번 하던 게 3번으로 줄었다.
//      자료구조는 하나도 안 바꿨고 해시 함수만 바꿨다.
//
//   (버킷 1024개에 5000개를 넣으면 평균 4.89 가 이론적 하한이다.
//    goodHash 의 avgChain 4.89 는 사실상 완벽한 분포라는 뜻이다.)
//
// ---------------------------------------------------------------------
//  [ 더 해볼 것 ]
//
//  1. BUCKET_COUNT 를 1024 -> 8192 로 늘려보자.
//     badHash_prefix 는 여전히 최장체인 5000 이다.
//     즉 "버킷을 늘려도 해시가 나쁘면 아무 소용이 없다."
//
//  2. bucketIndex() 의 % 를 & (buckets_.size() - 1) 로 바꿔보자.
//     (버킷 수가 2의 거듭제곱일 때만 가능하다)
//     이러면 해시의 하위 비트만 쓰게 된다. badHash_sum 은 더 나빠지지만
//     FNV-1a 는 곱셈으로 비트를 섞어놨기 때문에 거의 그대로 좋다.
//
//  3. 키 형식을 "0001-EMP-2024" 처럼 뒤집어보자.
//     badHash_prefix 는 갑자기 좋아진다. 해시의 좋고 나쁨은
//     해시 함수만의 문제가 아니라 "데이터 분포와의 궁합"이라는 걸 알 수 있다.
//
//  4. insert 에도 비교 횟수를 세보자. 삽입도 중복 검사 때문에
//     체인을 훑으므로 충돌이 심하면 삽입까지 O(N) 이 된다.
// =====================================================================
