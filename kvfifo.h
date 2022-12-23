#ifndef KVFIFO_H
#define KVFIFO_H

#include <cstddef>
#include <iostream>
#include <iterator>
#include <list>
#include <map>
#include <memory>
#include <set>
#include <utility>
#include <vector>

// Struktura danych: trzymamy wszystkie elementy kolejki na liście.
// Dla szybkiego dostępu do elementów o danym kluczu, mamy mapę dla danego
// klucza trzymającą referencje (iteratory) do wszystkich wystąpień elementów
// o tym kluczu.
//
// Wykorzystuje to zachowanie std::list polegające na tym, że iterator dla
// elementu unieważnia się tylko gdy ten element jest usuwany.

template <typename K, typename V>
class kvfifo_simple {
 private:
  struct entry {
    K key;
    V value;

    std::pair<K const &, V const &> as_pair() const { return {key, value}; }
    std::pair<K const &, V &> as_pair() { return {key, value}; }
  };

  // Lista elementów.
  using items_t = std::list<entry>;
  using shared_items_t = std::shared_ptr<items_t>;
  using item_iterator_t = items_t::iterator;
  // Lista iteratorów do elementów.
  using item_iterators_t = std::list<item_iterator_t>;
  // Mapa z klucza na listę iteratorów do elementów.
  using items_by_key_t = std::map<K, item_iterators_t>;
  using shared_items_by_key_t = std::shared_ptr<items_by_key_t>;

  // Wszystkie elementy.
  shared_items_t items;
  // Referencje do elementów o danym kluczu.
  shared_items_by_key_t items_by_key;
  // Prawda jeśli na zewnątrz (bo zwróciliśmy w jakiejś metodzie) istnieje
  // aktualna non-const referencja.
  bool external_ref_exists = false;

  // Rzuca std::invalid_argument jeśli nie ma żadnego elementu z danym
  // kluczem. W szczególności też jeśli nie ma żadnych elementów.
  K assert_key_exists(const K &k) const;

  // Wyrzuca std::invalid_argument jeśli nie ma żadnych elementów.
  void assert_nonempty() const {
    if (empty()) throw std::invalid_argument("empty");
  }

 public:
  kvfifo_simple() noexcept
      : items(std::make_shared<items_t>()),
        items_by_key(std::make_shared<items_by_key_t>()) {}

  kvfifo_simple &operator=(kvfifo_simple that);

  bool has_external_refs() const noexcept {
    return external_ref_exists;
  }

  // Tworzy kopię obiektu.
  std::shared_ptr<kvfifo_simple> copy() const;

  void push(K const &k, V const &v);

  void pop();
  
  void pop(K const &k);

  void move_to_back(K const &k);

  std::pair<K const &, V &> front();
  std::pair<K const &, V const &> front() const;

  std::pair<K const &, V &> back();
  std::pair<K const &, V const &> back() const;

  std::pair<K const &, V &> first(K const &k);
  std::pair<K const &, V const &> first(K const &k) const;

  std::pair<K const &, V &> last(K const &k);
  std::pair<K const &, V const &> last(K const &k) const;

  size_t size() const noexcept {
    return items->size();
  }

  bool empty() const noexcept {
    return items->empty();
  }

  size_t count(K const &k) const noexcept;

  void clear() noexcept;

  class k_iterator {
   private:
    using keys_iterator_t = items_by_key_t::iterator;
    keys_iterator_t keys_iterator;

   public:
    explicit k_iterator(keys_iterator_t iterator_) : keys_iterator(iterator_) {}
    k_iterator() = default;
    k_iterator(const k_iterator &that) : keys_iterator(that.keys_iterator) {}
    using iterator_category = std::bidirectional_iterator_tag;
    using difference_type = std::ptrdiff_t;
    using value_type = K;
    using reference = K &;

    const K &operator*() const { return keys_iterator->first; }

    k_iterator &operator++() {
      ++keys_iterator;
      return *this;
    }
    k_iterator operator++(int) {
      auto old = *this;
      ++(*this);
      return old;
    }
    k_iterator &operator--() {
      --keys_iterator;
      return *this;
    }
    k_iterator operator--(int) {
      auto old = *this;
      --(*this);
      return old;
    }

    bool operator==(const kvfifo_simple<K, V>::k_iterator &that) const {
      return keys_iterator == that.keys_iterator;
    }
    bool operator!=(const kvfifo_simple<K, V>::k_iterator &that) const {
      return keys_iterator != that.keys_iterator;
    }
  };
  static_assert(std::bidirectional_iterator<k_iterator>);

  k_iterator k_begin() const noexcept {
    return k_iterator(items_by_key->begin());
  }
  k_iterator k_end() const noexcept {
    return k_iterator(items_by_key->end());
  }
};

template <typename K, typename V>
class kvfifo {
 private:
  std::shared_ptr<kvfifo_simple<K, V>> simple;
  using k_iterator = kvfifo_simple<K, V>::k_iterator;

 public:
  kvfifo() noexcept : simple(std::make_shared<kvfifo_simple<K, V>>()) {}
  kvfifo(kvfifo const &that) noexcept
      : simple(that.simple->has_external_refs() ? that.simple->copy()
                                                : that.simple) {}
  // TODO: Look into this and decide if this really is noexcept
  kvfifo(kvfifo &&that) noexcept : simple(that.simple) {
    that.simple = std::make_shared<kvfifo_simple<K, V>>();
  }

  kvfifo &operator=(kvfifo that) noexcept;

  void push(K const &k, V const &v);

  void pop();

  void pop(K const &k);

  void move_to_back(K const &k);

  std::pair<K const &, V &> front();
  std::pair<K const &, V const &> front() const;

  std::pair<K const &, V &> back();
  std::pair<K const &, V const &> back() const;

  std::pair<K const &, V &> first(K const &k);
  std::pair<K const &, V const &> first(K const &k) const;

  std::pair<K const &, V &> last(K const &k);
  std::pair<K const &, V const &> last(K const &k) const;

  size_t size() const noexcept {
    return simple->size();
  }

  bool empty() const noexcept {
    return simple->empty();
  }

  size_t count(K const &k) const noexcept {
    return simple->count(k);
  }

  void clear() noexcept;

  k_iterator k_begin() const noexcept {
    return simple->k_begin();
  }
  k_iterator k_end() const noexcept {
    return simple->k_end();
  }
};

#endif