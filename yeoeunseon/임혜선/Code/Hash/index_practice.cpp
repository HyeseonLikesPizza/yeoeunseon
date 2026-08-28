// =====================================================================
//  index_practice.cpp
//
//  [ 연습 주제 ]
//    해시가 아닌 다른 자료구조(vector)에 데이터가 저장되어 있을 때,
//    그 구조를 그대로 두고 해시맵을 "인덱스"로 얹어 탐색을 가속한다.
//
//  [ 상황 설정 ]
//    로그 레코드가 vector 에 시간순으로 쌓여 있다. (append-only)
//
//    이 vector 는 버릴 수 없다. 이유는:
//      - 시간순 순회가 필요하다        (for (auto& e : logs))
//      - 인덱스 접근이 필요하다        ("1234번째 로그를 보여줘")
//      - 삽입은 항상 맨 뒤에만 일어난다 (로그의 본질)
//
//    그런데 요구사항이 하나 추가됐다:
//      "user 이름으로 그 유저의 모든 로그를 찾아라"
//      "id 로 특정 로그 하나를 찾아라"
//
//    순진하게 하면 매 조회마다 전체를 훑는다 -> O(N).
//    조회를 M 번 하면 O(N * M).
//
//  [ 해결 아이디어 ]
//    vector 는 그대로 두고, 옆에 해시맵 인덱스를 만든다.
//
//        unordered_map<string, vector<size_t>>  user -> 로그들의 "위치"
//        unordered_map<int,    size_t>          id   -> 로그의 "위치"
//
//    핵심은 데이터를 복사하는 게 아니라 "위치(첨자)"만 저장한다는 것.
//    DB 의 인덱스와 정확히 같은 개념이다.
//
//  [ 할 일 ]
//    아래 TODO 1 ~ TODO 3 을 채우면 된다.
//
//  [ 컴파일 ]
//    g++ -std=c++17 index_practice.cpp -o indexdemo
// =====================================================================

#include <iostream>
#include <iomanip>
#include <string>
#include <vector>
#include <unordered_map>
#include <chrono>
#include <random>

// ---------------------------------------------------------------------
//  1. 데이터 정의 (이미 완성됨)
// ---------------------------------------------------------------------

struct LogEntry
{
    int         id;          // 로그 고유 번호 (중복 없음)
    std::string user;        // 유저 이름 (중복 있음 - 한 유저가 여러 로그를 남긴다)
    long long   timestamp;   // 시각
    std::string msg;         // 내용
};

// ---------------------------------------------------------------------
//  2. 인덱스 없는 버전 - 선형 탐색 (이미 완성됨, 비교 대상)
// ---------------------------------------------------------------------

// compareCount 는 "몇 번 비교했는지"를 밖으로 알려주기 위한 참조 인자다.
// 실행 시간은 컴퓨터 상태에 따라 흔들리지만 비교 횟수는 정직하다.

std::vector<std::size_t> findByUser_linear(const std::vector<LogEntry>& logs,
                                           const std::string& user,
                                           long long& compareCount)
{
    std::vector<std::size_t> result;
    for (std::size_t i = 0; i < logs.size(); ++i)
    {
        ++compareCount;                  // 전체를 다 훑어야 한다.
        if (logs[i].user == user)        // 뒤에 더 있을지 모르니 중간에 멈출 수도 없다.
            result.push_back(i);
    }
    return result;
}

// id 로 찾기. id 는 유일하므로 찾으면 바로 멈출 수 있지만,
// 없는 id 를 찾거나 뒤쪽에 있으면 여전히 O(N) 이다.
bool findById_linear(const std::vector<LogEntry>& logs,
                     int id,
                     std::size_t& outPos,
                     long long& compareCount)
{
    for (std::size_t i = 0; i < logs.size(); ++i)
    {
        ++compareCount;
        if (logs[i].id == id)
        {
            outPos = i;
            return true;
        }
    }
    return false;
}

// ---------------------------------------------------------------------
//  3. 인덱스가 붙은 로그 저장소  <- 여기가 연습 구간
// ---------------------------------------------------------------------

class IndexedLogStore
{
public:
    // -----------------------------------------------------------------
    //  TODO 1 : 로그 추가 + 인덱스 갱신
    // -----------------------------------------------------------------
    //
    //  [ 해야 할 일 ]
    //    (1) entry 를 logs_ 의 맨 뒤에 넣는다.
    //    (2) 방금 넣은 원소의 "위치"를 구한다.
    //          push_back 한 뒤라면 그 위치는 logs_.size() - 1 이다.
    //    (3) userIndex_ 에 그 위치를 추가한다.
    //          userIndex_[entry.user].push_back(위치);
    //          -> unordered_map 의 operator[] 는 키가 없으면
    //             빈 vector 를 자동으로 만들어준다. 따로 검사할 필요 없다.
    //    (4) idIndex_ 에 그 위치를 등록한다.
    //          idIndex_[entry.id] = 위치;
    //          -> id 는 유일하므로 vector 가 아니라 값 하나면 된다.
    //
    //  [ 왜 포인터가 아니라 위치(size_t) 를 저장하나 ]
    //    vector 는 공간이 꽉 차면 더 큰 메모리를 새로 잡고 전부 옮긴다.
    //    이때 기존 원소의 "주소"는 전부 무효가 된다.
    //    LogEntry* 를 저장해뒀다면 그 포인터는 전부 쓰레기값이 된다.
    //    하지만 "몇 번째"라는 첨자는 재할당과 무관하게 그대로 유효하다.
    //    -> 그래서 인덱스에는 포인터가 아니라 첨자를 저장한다.
    //
    void add(const LogEntry& entry)
    {
        // ------------------ TODO 1 : 여기를 채우세요 ------------------

        (void)entry;   // 채우고 나면 이 줄은 지울 것

        // ------------------------------------------------------------
    }

    // -----------------------------------------------------------------
    //  TODO 2 : user 로 로그 위치들을 찾기
    // -----------------------------------------------------------------
    //
    //  [ 해야 할 일 ]
    //    userIndex_ 에서 user 를 찾는다.
    //      - 찾았으면 그 vector<size_t> 를 그대로 반환
    //      - 없으면 빈 vector 반환
    //
    //  [ 힌트 ]
    //    auto it = userIndex_.find(user);
    //    if (it == userIndex_.end()) return {};
    //    return it->second;
    //
    //  [ 주의 : operator[] 를 쓰면 안 된다 ]
    //    userIndex_[user] 라고 쓰면, 없는 키일 때 빈 항목을 "새로 만들어 넣는다".
    //    조회만 하려던 건데 맵이 조용히 커진다. 게다가 const 함수에서는
    //    아예 컴파일도 안 된다. 조회는 반드시 find() 를 쓸 것.
    //
    //  [ 비교 횟수를 세는 법 ]
    //    해시 조회는 사실상 한 번의 해시 계산 + 소수의 비교로 끝난다.
    //    선형 탐색과 공정하게 비교하기 위해, 여기서는
    //    "조회 1회 = 비교 1회" 로 세기로 한다.
    //    compareCount += 1;  을 넣으면 된다.
    //
    std::vector<std::size_t> findByUser(const std::string& user,
                                        long long& compareCount) const
    {
        // ------------------ TODO 2 : 여기를 채우세요 ------------------

        (void)user;
        (void)compareCount;
        return {};

        // ------------------------------------------------------------
    }

    // -----------------------------------------------------------------
    //  TODO 3 : id 로 로그 하나의 위치를 찾기
    // -----------------------------------------------------------------
    //
    //  [ 해야 할 일 ]
    //    idIndex_ 에서 id 를 찾는다.
    //      - 찾았으면 outPos 에 위치를 넣고 true 반환
    //      - 없으면 false 반환
    //    compareCount += 1; 도 잊지 말 것.
    //
    bool findById(int id, std::size_t& outPos, long long& compareCount) const
    {
        // ------------------ TODO 3 : 여기를 채우세요 ------------------

        (void)id;
        (void)outPos;
        (void)compareCount;
        return false;

        // ------------------------------------------------------------
    }

    // -----------------------------------------------------------------
    //  아래는 이미 완성된 부분
    // -----------------------------------------------------------------

    // 원본 vector 에 그대로 접근할 수 있다.
    // -> 인덱스를 붙였다고 해서 순서대로 순회하는 능력을 잃지 않는다.
    //    이게 "자료구조를 바꾸지 않고 얹기만 한다"의 의미다.
    const std::vector<LogEntry>& all() const { return logs_; }

    const LogEntry& at(std::size_t pos) const { return logs_[pos]; }

    std::size_t size() const { return logs_.size(); }

    // 인덱스가 쓰는 대략적인 메모리량 (원본 대비 얼마나 되는지 보려고)
    std::size_t indexMemoryEstimate() const
    {
        std::size_t bytes = 0;
        for (const auto& kv : userIndex_)
            bytes += kv.first.capacity()
                   + kv.second.size() * sizeof(std::size_t);
        bytes += idIndex_.size() * (sizeof(int) + sizeof(std::size_t));
        return bytes;
    }

    std::size_t distinctUsers() const { return userIndex_.size(); }

private:
    std::vector<LogEntry> logs_;   // 원본. 시간순. 이건 그대로 둔다.

    // 인덱스들. 데이터를 복사하지 않고 "위치"만 갖는다.
    std::unordered_map<std::string, std::vector<std::size_t>> userIndex_;
    std::unordered_map<int, std::size_t>                      idIndex_;
};

// ---------------------------------------------------------------------
//  4. 테스트 데이터 생성 (이미 완성됨)
// ---------------------------------------------------------------------

std::vector<LogEntry> makeLogs(int count, int userCount)
{
    std::vector<std::string> users;
    for (int i = 0; i < userCount; ++i)
        users.push_back("user_" + std::to_string(i));

    std::mt19937 rng(12345);                     // 고정 시드 -> 실행할 때마다 같은 데이터
    std::uniform_int_distribution<int> pick(0, userCount - 1);

    std::vector<LogEntry> logs;
    logs.reserve(count);

    for (int i = 0; i < count; ++i)
    {
        LogEntry e;
        e.id        = 10000 + i;
        e.user      = users[pick(rng)];
        e.timestamp = 1700000000LL + i;
        e.msg       = "action #" + std::to_string(i);
        logs.push_back(e);
    }
    return logs;
}

// ---------------------------------------------------------------------
//  main
// ---------------------------------------------------------------------

int main()
{
    const int LOG_COUNT   = 50000;   // 로그 5만 개
    const int USER_COUNT  = 200;     // 유저 200명
    const int QUERY_COUNT = 1000;    // 조회 1000번

    std::vector<LogEntry> raw = makeLogs(LOG_COUNT, USER_COUNT);

    // 인덱스 버전에도 같은 데이터를 넣는다
    IndexedLogStore store;
    for (const auto& e : raw)
        store.add(e);

    // 조회할 유저 목록을 미리 뽑아둔다 (양쪽에 똑같이 쓴다)
    std::mt19937 rng(999);
    std::uniform_int_distribution<int> pickUser(0, USER_COUNT - 1);
    std::uniform_int_distribution<int> pickId(10000, 10000 + LOG_COUNT - 1);

    std::vector<std::string> userQueries;
    std::vector<int>         idQueries;
    for (int i = 0; i < QUERY_COUNT; ++i)
    {
        userQueries.push_back("user_" + std::to_string(pickUser(rng)));
        idQueries.push_back(pickId(rng));
    }

    std::cout << "\n";
    std::cout << "===========================================================================\n";
    std::cout << " vector + 해시 인덱스 실험\n";
    std::cout << "===========================================================================\n";
    std::cout << " 로그      : " << LOG_COUNT  << " 개 (vector 에 시간순 저장)\n";
    std::cout << " 유저      : " << USER_COUNT << " 명\n";
    std::cout << " 조회 횟수 : " << QUERY_COUNT << " 번\n\n";

    using Clock = std::chrono::high_resolution_clock;

    // ---------------- user 조회 : 선형 ----------------
    long long linearUserCmp = 0;
    std::size_t linearUserHits = 0;
    auto t0 = Clock::now();
    for (const auto& u : userQueries)
        linearUserHits += findByUser_linear(raw, u, linearUserCmp).size();
    auto t1 = Clock::now();
    double linearUserMs = std::chrono::duration<double, std::milli>(t1 - t0).count();

    // ---------------- user 조회 : 인덱스 ----------------
    long long indexUserCmp = 0;
    std::size_t indexUserHits = 0;
    t0 = Clock::now();
    for (const auto& u : userQueries)
        indexUserHits += store.findByUser(u, indexUserCmp).size();
    t1 = Clock::now();
    double indexUserMs = std::chrono::duration<double, std::milli>(t1 - t0).count();

    // ---------------- id 조회 : 선형 ----------------
    long long linearIdCmp = 0;
    std::size_t linearIdHits = 0;
    std::size_t pos = 0;
    t0 = Clock::now();
    for (int id : idQueries)
        if (findById_linear(raw, id, pos, linearIdCmp)) ++linearIdHits;
    t1 = Clock::now();
    double linearIdMs = std::chrono::duration<double, std::milli>(t1 - t0).count();

    // ---------------- id 조회 : 인덱스 ----------------
    long long indexIdCmp = 0;
    std::size_t indexIdHits = 0;
    t0 = Clock::now();
    for (int id : idQueries)
        if (store.findById(id, pos, indexIdCmp)) ++indexIdHits;
    t1 = Clock::now();
    double indexIdMs = std::chrono::duration<double, std::milli>(t1 - t0).count();

    // ---------------- 결과 출력 ----------------
    std::cout << std::left
              << std::setw(26) << "method"
              << std::right
              << std::setw(16) << "comparisons"
              << std::setw(14) << "time(ms)"
              << std::setw(14) << "found"
              << "\n";
    std::cout << std::string(70, '-') << "\n";

    std::cout << std::left << std::setw(26) << "[user] linear scan"
              << std::right << std::setw(16) << linearUserCmp
              << std::setw(14) << std::fixed << std::setprecision(2) << linearUserMs
              << std::setw(14) << linearUserHits << "\n";

    std::cout << std::left << std::setw(26) << "[user] hash index"
              << std::right << std::setw(16) << indexUserCmp
              << std::setw(14) << std::fixed << std::setprecision(2) << indexUserMs
              << std::setw(14) << indexUserHits << "\n";

    std::cout << std::string(70, '-') << "\n";

    std::cout << std::left << std::setw(26) << "[id]   linear scan"
              << std::right << std::setw(16) << linearIdCmp
              << std::setw(14) << std::fixed << std::setprecision(2) << linearIdMs
              << std::setw(14) << linearIdHits << "\n";

    std::cout << std::left << std::setw(26) << "[id]   hash index"
              << std::right << std::setw(16) << indexIdCmp
              << std::setw(14) << std::fixed << std::setprecision(2) << indexIdMs
              << std::setw(14) << indexIdHits << "\n";

    std::cout << "\n";

    // ---------------- 정확성 확인 ----------------
    std::cout << "---------------------------------------------------------------------------\n";
    std::cout << " 정확성 확인 (두 방식의 결과가 같아야 한다)\n";
    std::cout << "---------------------------------------------------------------------------\n";
    std::cout << " user 조회 결과 일치 : "
              << (linearUserHits == indexUserHits ? "OK" : "불일치 - TODO 를 확인하세요") << "\n";
    std::cout << " id   조회 결과 일치 : "
              << (linearIdHits == indexIdHits ? "OK" : "불일치 - TODO 를 확인하세요") << "\n\n";

    // ---------------- 인덱스 비용 ----------------
    std::cout << "---------------------------------------------------------------------------\n";
    std::cout << " 인덱스가 치르는 비용\n";
    std::cout << "---------------------------------------------------------------------------\n";
    std::cout << " 등록된 유저 수    : " << store.distinctUsers() << "\n";
    std::cout << " 인덱스 메모리     : 약 "
              << store.indexMemoryEstimate() / 1024 << " KB\n";
    std::cout << " 원본 데이터       : 약 "
              << (raw.size() * sizeof(LogEntry)) / 1024 << " KB (문자열 본체 제외)\n";
    std::cout << " -> 인덱스는 공짜가 아니다. 메모리를 더 쓰고, 삽입할 때\n";
    std::cout << "    갱신 비용도 든다. 조회가 많을 때만 이득이다.\n\n";

    // ---------------- 순회는 그대로 된다는 확인 ----------------
    std::cout << "---------------------------------------------------------------------------\n";
    std::cout << " vector 의 능력은 그대로 남아있다\n";
    std::cout << "---------------------------------------------------------------------------\n";
    if (store.size() >= 3)
    {
        std::cout << " 시간순 앞에서 3개:\n";
        for (std::size_t i = 0; i < 3; ++i)
        {
            const LogEntry& e = store.at(i);
            std::cout << "   [" << i << "] id=" << e.id
                      << " user=" << e.user
                      << " ts=" << e.timestamp
                      << " msg=" << e.msg << "\n";
        }
    }
    std::cout << "\n";

    return 0;
}

// =====================================================================
//  [ 정답 ]  스스로 풀어본 뒤에 열어볼 것
// =====================================================================
//
//  --- TODO 1 : add ---
//
//  void add(const LogEntry& entry)
//  {
//      logs_.push_back(entry);
//      std::size_t pos = logs_.size() - 1;
//
//      userIndex_[entry.user].push_back(pos);
//      idIndex_[entry.id] = pos;
//  }
//
//  --- TODO 2 : findByUser ---
//
//  std::vector<std::size_t> findByUser(const std::string& user,
//                                      long long& compareCount) const
//  {
//      compareCount += 1;
//      auto it = userIndex_.find(user);
//      if (it == userIndex_.end())
//          return {};
//      return it->second;
//  }
//
//  --- TODO 3 : findById ---
//
//  bool findById(int id, std::size_t& outPos, long long& compareCount) const
//  {
//      compareCount += 1;
//      auto it = idIndex_.find(id);
//      if (it == idIndex_.end())
//          return false;
//      outPos = it->second;
//      return true;
//  }
//
// ---------------------------------------------------------------------
//  [ 예상 결과 ]
//
//   method                       comparisons    time(ms)      found
//   [user] linear scan            50,000,000     ~1400       249870
//   [user] hash index                  1,000        ~0.7      249870
//   [id]   linear scan            25,524,518      ~200          1000
//   [id]   hash index                  1,000        ~0.3        1000
//
//   비교 횟수가 5천만 -> 1천 으로 줄었다. 5만 배다.
//   시간으로도 1400ms -> 0.7ms 로 약 2000배 빨라졌다.
//   그런데 원본 vector 는 손 하나 안 댔다.
//
//   (시간은 컴퓨터 상태에 따라 달라진다. 비교 횟수를 보는 게 더 정확하다.
//    비교 횟수는 어느 컴퓨터에서 돌려도 똑같이 나온다.)
//
// ---------------------------------------------------------------------
//  [ 이 방식의 함정 - 반드시 알아둘 것 ]
//
//  1. 포인터를 저장하면 안 되는 이유
//     std::vector<LogEntry*> 로 인덱스를 만들었다고 해보자.
//     push_back 으로 vector 가 재할당되는 순간 모든 포인터가 무효가 된다.
//     프로그램은 크래시하지 않고 조용히 이상한 값을 읽는다. 최악의 버그다.
//     첨자(size_t)는 재할당과 무관하므로 안전하다.
//
//     -> 직접 확인해보고 싶으면:
//        vector 에 push_back 하기 전후로 &logs_[0] 을 출력해보면
//        어느 순간 주소가 바뀌는 걸 볼 수 있다.
//
//  2. 중간 삭제를 하면 인덱스가 전부 깨진다
//     logs_.erase(logs_.begin() + 5) 를 하면 6번 이후 원소가 전부 앞으로
//     한 칸씩 밀린다. 인덱스에 저장된 첨자는 전부 한 칸씩 어긋난다.
//
//     해결책 두 가지:
//       (a) append-only 로 제한한다 (로그는 원래 그렇다)
//       (b) tombstone : 실제로 지우지 않고 bool deleted 플래그만 켠다.
//           조회할 때 deleted 인 항목을 걸러낸다. 첨자는 그대로 유지된다.
//
//  3. 인덱스는 원본과 항상 동기화되어야 한다
//     add() 안에서 원본 삽입과 인덱스 갱신을 같이 처리한 이유가 이거다.
//     logs_ 를 public 으로 열어두고 밖에서 아무나 push_back 하게 만들면
//     인덱스가 조용히 낡는다. 그래서 logs_ 는 private 이고
//     읽기 전용 접근(all(), at())만 제공한다.
//
// ---------------------------------------------------------------------
//  [ 더 해볼 것 ]
//
//  1. timestamp 범위 조회 ("1700000100 ~ 1700000200 사이 로그")를 추가해보자.
//     해시로는 이게 안 된다. 해시는 "정확히 일치"만 빠르다.
//     범위 조회에는 정렬된 구조(std::map, 또는 정렬된 vector + 이분 탐색)가 필요하다.
//     -> 인덱스는 "질문의 종류"에 맞춰 고르는 것이지 무조건 해시가 아니다.
//        여기 로그는 timestamp 순으로 이미 정렬돼 있으니
//        std::lower_bound 로 O(log N) 에 해결된다. 인덱스조차 필요 없다.
//
//  2. msg 에 특정 단어가 들어간 로그 찾기를 추가해보자.
//     이것도 해시로는 안 된다. 부분 문자열 검색은 다른 문제다.
//     (역색인 inverted index 라는 걸 만들어야 한다 - 단어 -> 로그 위치 목록)
//     실은 userIndex_ 가 바로 그 역색인의 가장 단순한 형태다.
//
//  3. LOG_COUNT 를 5000 으로 줄여보고 다시 재보자.
//     데이터가 작으면 선형 탐색이 오히려 빠를 수도 있다.
//     vector 순회는 메모리에 연속으로 놓여 있어서 CPU 캐시에 아주 잘 맞는다.
//     반면 해시맵은 메모리 여기저기를 튀어다닌다.
//     -> "O(1) 이 O(N) 보다 항상 빠르다"는 건 N 이 충분히 클 때 얘기다.
//
//  4. 인덱스 구축을 add() 마다 하지 말고, 데이터를 다 넣은 뒤
//     buildIndex() 를 한 번 호출하는 방식으로 바꿔보자.
//     장점: 삽입이 빨라진다. 단점: 인덱스가 없는 동안은 조회를 못 한다.
//     DB 에서 대량 적재 후 인덱스를 만드는 것과 같은 이유다.
// =====================================================================
