#include <compare>
#include <cstdint>

class Counter {
  public:
    constexpr Counter() : count(0) {}
    constexpr Counter(uint64_t initialValue) : count(initialValue) {}

    constexpr uint64_t getCount() const {
      return count;
    }

    constexpr Counter &operator++() {
      count++;
    }

    constexpr Counter &operator--() {
      count--;
    }

    constexpr std::strong_ordering operator<=>(uint64_t value) {
      if (count < value) {
        return std::strong_ordering::less;
      } if (count > value) {
        return std::strong_ordering::greater;
      } else {
        return std::strong_ordering::equal;
      }
    }

  private:
    uint64_t count;
};