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

template <typename K, typename V>
class kvfifo_simple {
 private:
  struct entry {
    K key;
    V value;

    std::pair<K const &, V const &> as_pair() const { return {key, value}; }
    std::pair<K const &, V &> as_pair() { return {key, value}; }
  };

  // Struktura danych: trzymamy wszystkie elementy kolejki na liście.
  // Dla szybkiego dostępu do elementów o danym kluczu, mamy mapę dla danego
  // klucza trzymającą referencje (iteratory) do wszystkich wystąpień elementów
  // o tym kluczu.
  //
  // Wykorzystuje to zachowanie std::list polegające na tym, że iterator dla
  // elementu unieważnia się tylko gdy ten element jest usuwany.

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

  // Wyrzuca std::invalid_argument jeśli nie ma żadnego elementu z danym
  // kluczem. W szczególności też jeśli nie ma żadnych elementów.
  K assert_key_exists(const K &k) const {
    if (count(k) == 0) throw std::invalid_argument("key missing");
    return k;
  }

  // Wyrzuca std::invalid_argument jeśli nie ma żadnych elementów.
  void assert_nonempty() const {
    if (empty()) throw std::invalid_argument("empty");
  }

 public:
  kvfifo_simple() noexcept
      : items(std::make_shared<items_t>()),
        items_by_key(std::make_shared<items_by_key_t>()) {}

  kvfifo_simple &operator=(kvfifo_simple that) {
    auto new_items = that.item;
    auto new_items_by_key = that.items_by_key;

    return (*this);
  }

  bool has_external_refs() const noexcept { return external_ref_exists; }

  std::shared_ptr<kvfifo_simple> copy() const {
    auto copy = std::make_shared<kvfifo_simple<K, V>>();

    auto new_items = std::make_shared<items_t>(*items);
    auto new_items_by_key = std::make_shared<items_by_key_t>();
    for (auto walk = new_items->begin(); walk != new_items->end(); ++walk) {
      (*new_items_by_key)[walk->key].push_back(walk);
    }

    copy->items = new_items;
    copy->items_by_key = new_items_by_key;

    return copy;
  }

  void push(K const &k, V const &v) {
    // Trzeba dodać nowy element na koniec items. Trzeba zapisać referencję do
    // niego w odpowiednim miejscu w items_by_key.
    items_t items_please_push_back = {{k, v}};
    // Zapisujemy nowy element mapy dla przypadku gdy trzeba stworzyć nowy
    // element mapy, i nowy element listy dla przypadku gdy już jest w mapie.
    items_by_key_t items_by_key_please_insert_maybe = {
        {k, {items_please_push_back.begin()}}};
    item_iterators_t item_at_key_please_push_back_maybe = {
        items_please_push_back.begin()};

    // Dalej bez wyjątków.

    items->splice(items->end(), items_please_push_back);
    auto items_at_key = items_by_key->find(k);
    if (items_at_key == items_by_key->end()) {
      // Klucz nie istniał.
      items_by_key->merge(items_by_key_please_insert_maybe);
    } else {
      // Istniał.
      items_at_key->second.splice(items_at_key->second.end(),
                                  item_at_key_please_push_back_maybe);
    }

    // Bo modyfikacja unieważnia.
    external_ref_exists = false;
  }

  void pop() {
    assert_nonempty();

    // Dalej bez wyjątków.

    auto [key, value] = items->front();
    items->pop_front();
    auto &items_at_key = items_by_key->at(key);
    items_at_key.pop_front();
    if (items_at_key.empty()) items_by_key->erase(key);

    // Bo modyfikacja unieważnia.
    external_ref_exists = false;
  }

  void pop(K const &k) {
    assert_key_exists(k);

    // Dalej bez wyjątków.

    auto &items_at_key = items_by_key->at(k);
    const auto &node = items_at_key.front();
    items_at_key.pop_front();
    if (items_at_key.empty()) items_by_key->erase(k);
    items->erase(node);

    // Bo modyfikacja unieważnia.
    external_ref_exists = false;
  }

  void move_to_back(K const &k) {
    assert_key_exists(k);

    // Trzeba zamienić wszystkie elementy z kluczem k: usunąć wszystkie z items,
    // i dodać nowe na koniec. Przez to trzeba zamienić referencje w
    // items_by_key (zmienią się wszystkie).
    item_iterators_t items_at_key_please_swap;
    items_t items_please_push_back;
    item_iterators_t items_please_erase;
    auto &items_at_key = items_by_key->at(k);
    for (const auto &node : items_at_key) {
      items_please_push_back.push_back(*node);
      items_at_key_please_swap.push_back(
          std::prev(items_please_push_back.end()));
      items_please_erase.push_back(node);
    }

    // Dalej bez wyjątków.

    items->splice(items->end(), items_please_push_back);
    for (const auto &node : items_please_erase) items->erase(node);
    items_at_key.swap(items_at_key_please_swap);

    // Bo modyfikacja unieważnia.
    external_ref_exists = false;
  }

  std::pair<K const &, V &> front() {
    assert_nonempty();

    external_ref_exists = true;
    return items->front().as_pair();
  }
  std::pair<K const &, V const &> front() const {
    assert_nonempty();

    return items->front().as_pair();
  }
  std::pair<K const &, V &> back() {
    assert_nonempty();

    external_ref_exists = true;
    return items->back().as_pair();
  }
  std::pair<K const &, V const &> back() const {
    assert_nonempty();

    return items->back().as_pair();
  }

  std::pair<K const &, V &> first(K const &k) {
    assert_key_exists(k);

    external_ref_exists = true;
    return items_by_key->at(k).front()->as_pair();
  }
  std::pair<K const &, V const &> first(K const &k) const {
    assert_key_exists(k);

    return items_by_key->at(k).front()->as_pair();
  }
  std::pair<K const &, V &> last(K const &k) {
    assert_key_exists(k);

    external_ref_exists = true;
    return items_by_key->at(k).back()->as_pair();
  }
  std::pair<K const &, V const &> last(K const &k) const {
    assert_key_exists(k);

    return items_by_key->at(k).back()->as_pair();
  }

  size_t size() const noexcept { return items->size(); }

  bool empty() const noexcept { return items->empty(); }

  size_t count(K const &k) const noexcept {
    // Bez wyjątków.
    auto it = items_by_key->find(k);
    if (it == items_by_key->end()) {
      return 0;
    }
    return it->second.size();
  }

  void clear() noexcept {
    if (empty()) return;

    // Bez wyjątków.
    items->clear();
    items_by_key->clear();
  }

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
  k_iterator k_end() const noexcept { return k_iterator(items_by_key->end()); }
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
  kvfifo(kvfifo &&that) noexcept : simple(that.simple) {
    that.simple = nullptr;
  }

  kvfifo &operator=(kvfifo that) noexcept {
    simple =
        that.simple->has_external_refs() ? that.simple->copy() : that.simple;

    return (*this);
  }

  void push(K const &k, V const &v) {
    auto simple_2 = (simple.unique() ? simple : simple->copy());
    simple_2->push(k, v);
    simple = simple_2;
  }

  void pop() {
    auto simple_2 = (simple.unique() ? simple : simple->copy());
    simple_2->pop();
    simple = simple_2;
  }

  void pop(K const &k) {
    auto simple_2 = (simple.unique() ? simple : simple->copy());
    simple_2->pop(k);
    simple = simple_2;
  }

  void move_to_back(K const &k) {
    auto simple_2 = (simple.unique() ? simple : simple->copy());
    simple_2->move_to_back(k);
    simple = simple_2;
  }

  std::pair<K const &, V &> front() {
    auto simple_2 = (simple.unique() ? simple : simple->copy());
    simple = simple_2;

    return simple_2->front();
  }
  std::pair<K const &, V const &> front() const { return simple->front(); }
  std::pair<K const &, V &> back() {
    auto simple_2 = (simple.unique() ? simple : simple->copy());
    simple = simple_2;

    return simple_2->back();
  }
  std::pair<K const &, V const &> back() const { return simple->back(); }
  std::pair<K const &, V &> first(K const &k) {
    auto simple_2 = (simple.unique() ? simple : simple->copy());
    simple = simple_2;

    return simple_2->first(k);
  }
  std::pair<K const &, V const &> first(K const &k) const {
    return simple->first(k);
  }
  std::pair<K const &, V &> last(K const &k) {
    auto simple_2 = (simple.unique() ? simple : simple->copy());
    simple = simple_2;

    return simple_2->last(k);
  }
  std::pair<K const &, V const &> last(K const &k) const {
    return simple->last(k);
  }

  size_t size() const noexcept { return simple->size(); }

  bool empty() const noexcept { return simple->empty(); }

  size_t count(K const &k) const noexcept { return simple->count(k); }

  void clear() noexcept {
    auto simple_2 = (simple.unique() ? simple : simple->copy());
    simple_2->clear();
    simple = simple_2;
  }

  k_iterator k_begin() const noexcept {
    return simple->k_begin();
  }
  k_iterator k_end() const noexcept {
    return simple->k_end();
  }
};

#endif