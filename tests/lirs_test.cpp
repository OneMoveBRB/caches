#include <gtest/gtest.h>

#include <memory>
#include <optional>
#include <string>
#include <utility>

#include "caches/lirs.hpp"

namespace {

using IntLIRS = caches::LIRS<int, int>;
using StrLIRS = caches::LIRS<int, std::string>;

// ---------- Конструктор ----------

TEST(LIRSConstructor, ResolvesCapacities) {
  IntLIRS cache(4, 2);
  EXPECT_EQ(cache.capacity(), 4u);
  EXPECT_EQ(cache.size(), 0u);
  EXPECT_TRUE(cache.empty());
}

// ---------- Базовые операции ----------

TEST(LIRSBasic, EmptyCacheMisses) {
  IntLIRS cache(4, 2);
  EXPECT_EQ(cache.get(42), std::nullopt);
  EXPECT_EQ(cache.hits(), 0u);
  EXPECT_EQ(cache.misses(), 1u);
}

TEST(LIRSBasic, PutThenGetHits) {
  IntLIRS cache(4, 2);
  cache.put(1, 100);
  EXPECT_EQ(cache.size(), 1u);
  EXPECT_EQ(cache.get(1), 100);
  EXPECT_EQ(cache.hits(), 1u);
  EXPECT_EQ(cache.misses(), 0u);
}

TEST(LIRSBasic, PutUpdatesExistingKey) {
  IntLIRS cache(4, 2);
  cache.put(1, 100);
  cache.put(1, 111);
  EXPECT_EQ(cache.size(), 1u);
  EXPECT_EQ(cache.get(1), 111);
}

TEST(LIRSBasic, GetMissingKeyIsMiss) {
  IntLIRS cache(4, 2);
  cache.put(1, 10);
  EXPECT_EQ(cache.get(999), std::nullopt);
  EXPECT_EQ(cache.size(), 1u);
  EXPECT_EQ(cache.misses(), 1u);
}

// ---------- Заполнение LIR-сета ----------

TEST(LIRSFill, FirstLirCapacityEntriesAreLIR) {
  IntLIRS cache(4, 2);
  cache.put(1, 10);
  cache.put(2, 20);
  // 1 и 2 — LIR (lir_size == lir_capacity == 2)
  EXPECT_EQ(cache.size(), 2u);
  EXPECT_EQ(cache.get(1), 10);  // hit LIR
  EXPECT_EQ(cache.get(2), 20);  // hit LIR
}

TEST(LIRSFill, EntriesBeyondLirCapacityAreHIR) {
  IntLIRS cache(4, 2);
  cache.put(1, 10);  // LIR
  cache.put(2, 20);  // LIR
  cache.put(3, 30);  // HIR (resident, in stack + queue)
  EXPECT_EQ(cache.size(), 3u);
  EXPECT_EQ(cache.get(3), 30);  // hit HIR in stack -> promote to LIR
}

// ---------- Вытеснение ----------

TEST(LIRSEviction, EvictsOldestHirFromQueue) {
  IntLIRS cache(4, 2);
  cache.put(1, 10);  // LIR
  cache.put(2, 20);  // LIR
  cache.put(3, 30);  // HIR
  cache.put(4, 40);  // HIR
  // теперь resident: 1,2 (LIR), 3,4 (HIR); queue: 4,3 (MRU..LRU)

  cache.put(5, 50);  // size==capacity -> evict 3 (LRU HIR)
  EXPECT_EQ(cache.get(3), std::nullopt);
  EXPECT_EQ(cache.get(1), 10);
  EXPECT_EQ(cache.get(2), 20);
  EXPECT_EQ(cache.get(4), 40);
  EXPECT_EQ(cache.get(5), 50);
}

TEST(LIRSEviction, RepeatedHirAccessPromotesToLirAndDemotesOldestLir) {
  IntLIRS cache(4, 2);
  cache.put(1, 10);  // LIR
  cache.put(2, 20);  // LIR
  cache.put(3, 30);  // HIR
  cache.put(4, 40);  // HIR
  // stack: [4,3,2,1], queue: [4,3]

  cache.get(3);      // 3 в стеке -> promote; demote bottom LIR = 1
  // LIR: 2,3; HIR resident: 1,4; queue: [1,4]; stack: [3,4,2]

  cache.put(5, 50);  // size==4 -> evict LRU HIR = 4 (queue back)
  // LIR: 2,3; HIR resident: 1,5; 4 -> ghost (в стеке)

  EXPECT_EQ(cache.get(1), 10);       // 1 всё ещё resident HIR
  EXPECT_EQ(cache.get(2), 20);       // 2 LIR
  EXPECT_EQ(cache.get(3), 30);       // 3 LIR
  EXPECT_EQ(cache.get(4), std::nullopt);  // 4 вытеснен
  EXPECT_EQ(cache.get(5), 50);       // 5 HIR resident
}

TEST(LIRSEviction, HitsOnResidentHirInQueueWithoutStackPromotes) {
  IntLIRS cache(4, 2);
  cache.put(1, 10);  // LIR
  cache.put(2, 20);  // LIR
  cache.put(3, 30);  // HIR
  cache.put(4, 40);  // HIR

  cache.get(3);      // promote 3 -> LIR, demote 1 -> HIR
  // LIR: 2,3; HIR resident: 1,4

  cache.put(5, 50);  // evict 4 (LRU HIR в queue)
  EXPECT_EQ(cache.get(3), 30);       // 3 теперь LIR
  EXPECT_EQ(cache.get(1), 10);       // 1 resident HIR
  EXPECT_EQ(cache.get(4), std::nullopt);  // 4 вытеснен
}

// ---------- Ghost (non-resident HIR) ----------

TEST(LIRSGhost, ReaccessToGhostHirPromotesWithoutEviction) {
  IntLIRS cache(4, 2);
  cache.put(1, 10);  // LIR
  cache.put(2, 20);  // LIR
  cache.put(3, 30);  // HIR
  cache.put(4, 40);  // HIR
  cache.put(5, 50);  // evict 3 (queue bottom), 3 -> ghost (if still in stack)

  // 3 не в resident -> get(miss) не промоутит; put(3) должен найти ghost
  // в стеке и промоутировать без вытеснения (reclaim_queue не нужен)
  EXPECT_EQ(cache.get(3), std::nullopt);  // miss, 3 ghost
  cache.put(3, 33);  // ghost hit -> promote to LIR, resident_size растёт

  EXPECT_EQ(cache.size(), 4u);  // 3 снова resident, размер прежний (capacity)
  EXPECT_EQ(cache.get(3), 33);
}

TEST(LIRSGhost, GhostKeyMayBeForgottenAfterStackPrune) {
  // После вытеснения из Q и попадания в ghost, если ключ выпадает из стека
  // (prune_stack), entries_ его забывает; повторный put создаст с нуля.
  IntLIRS cache(3, 1);
  cache.put(1, 10);  // LIR
  cache.put(2, 20);  // HIR
  cache.put(3, 30);  // HIR -> capacity full
  cache.put(4, 40);  // evict 2 (LRU HIR), 2 -> ghost

  // 4 теперь HIR; трогаем 1 (LIR hit) -> prune_stack пройдёт по стеку,
  // если 2 в стеке ещё на позиции non-LIR внизу, он будет забыт.
  EXPECT_EQ(cache.get(1), 10);

  // 2 либо ghost, либо забыт; put(2) в обоих случаях должен работать
  cache.put(2, 22);
  EXPECT_EQ(cache.get(2), 22);
}

// ---------- Инварианты ----------

TEST(LIRSInvariants, SizeNeverExceedsCapacity) {
  IntLIRS cache(8, 3);
  for (int i = 0; i < 100; ++i) {
    cache.put(i, i * 10);
    EXPECT_LE(cache.size(), cache.capacity());
  }
}

TEST(LIRSInvariants, LirCountNeverExceedsLirCapacity) {
  // Косвенно: если бы LIR-count > lir_capacity, то при промоушене
  // демотировался бы старый LIR; проверим, что после серии промоушенов
  // вытеснение идёт корректно (не падает и размер соблюдён).
  IntLIRS cache(6, 2);
  for (int i = 0; i < 50; ++i) {
    cache.put(i, i * 10);
    cache.get(i);  // промоушен в LIR
    EXPECT_LE(cache.size(), cache.capacity());
  }
}

// ---------- Статистика ----------

TEST(LIRSStats, HitsAndMissesCounted) {
  IntLIRS cache(4, 2);
  cache.get(1);  // miss
  cache.put(1, 10);
  cache.get(1);  // hit
  cache.get(2);  // miss
  cache.put(2, 20);
  cache.get(2);  // hit
  cache.get(2);  // hit
  EXPECT_EQ(cache.hits(), 3u);
  EXPECT_EQ(cache.misses(), 2u);
}

TEST(LIRSStats, ClearResetsStatsAndContent) {
  IntLIRS cache(4, 2);
  cache.put(1, 10);
  cache.get(1);
  cache.get(2);
  cache.clear();
  EXPECT_EQ(cache.size(), 0u);
  EXPECT_TRUE(cache.empty());
  EXPECT_EQ(cache.hits(), 0u);
  EXPECT_EQ(cache.misses(), 0u);
  EXPECT_EQ(cache.get(1), std::nullopt);
}

// ---------- Move-семантика ----------

TEST(LIRSMove, MoveConstructTransfersState) {
  IntLIRS src(4, 2);
  src.put(1, 10);
  src.put(2, 20);
  src.get(1);

  IntLIRS dst(std::move(src));

  EXPECT_EQ(dst.capacity(), 4u);
  EXPECT_EQ(dst.size(), 2u);
  EXPECT_EQ(dst.get(1), 10);
  EXPECT_EQ(dst.get(2), 20);
}

TEST(LIRSMove, MoveAssignTransfersState) {
  IntLIRS src(4, 2);
  src.put(7, 70);

  IntLIRS dst(6, 2);
  dst.put(1, 10);

  dst = std::move(src);

  EXPECT_EQ(dst.capacity(), 4u);
  EXPECT_EQ(dst.size(), 1u);
  EXPECT_EQ(dst.get(7), 70);
}

TEST(LIRSMove, MovedFromCacheRemainsUsable) {
  IntLIRS src(4, 2);
  src.put(1, 10);
  IntLIRS dst(std::move(src));

  EXPECT_EQ(src.size(), 0u);
  src.put(5, 50);
  EXPECT_EQ(src.get(5), 50);
  EXPECT_EQ(dst.get(1), 10);
}

// ---------- Полиморфизм ----------

TEST(LIRSPolymorphic, WorksThroughBasePointer) {
  std::unique_ptr<caches::ICache<int, int>> cache =
      std::make_unique<IntLIRS>(4, 2);
  cache->put(1, 10);
  cache->put(2, 20);
  EXPECT_EQ(cache->get(1), 10);
  EXPECT_EQ(cache->size(), 2u);
  EXPECT_EQ(cache->capacity(), 4u);
  EXPECT_EQ(cache->hits(), 1u);
  EXPECT_EQ(cache->misses(), 0u);
}

// ---------- Нетривиальные типы ----------

TEST(LIRSTypes, WorksWithStringPage) {
  StrLIRS cache(4, 2);
  cache.put(1, "one");
  cache.put(2, "two");
  cache.put(3, "three");
  EXPECT_EQ(cache.get(1), std::string("one"));
  EXPECT_EQ(cache.get(3), std::string("three"));
}

// ---------- Компиляционные инварианты ----------

TEST(LIRSCompileTime, SpecialMembers) {
  static_assert(!std::is_copy_constructible_v<IntLIRS>);
  static_assert(!std::is_copy_assignable_v<IntLIRS>);
  static_assert(std::is_move_constructible_v<IntLIRS>);
  static_assert(std::is_move_assignable_v<IntLIRS>);
  static_assert(std::has_virtual_destructor_v<caches::ICache<int, int>>);
}

}  // namespace
