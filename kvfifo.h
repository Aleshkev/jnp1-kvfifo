#ifndef KVFIFO_H
#define KVFIFO_H

#include <cstddef>
#include <iostream>
#include <iterator>
#include <list>
#include <map>
#include <set>
#include <utility>

// BIG TODO: exception correctness

// TODO: wymagania na typy K, V
template <typename K, typename V>
class kvfifo {
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
  using items_t = std::list<entry>;
  using items_iterator_t = items_t::iterator;
  items_t items;
  using items_by_key_t = std::map<K, std::list<items_iterator_t>>;
  items_by_key_t items_by_key;

  K assert_key_exists(const K &k) const {
    if (count(k) == 0) throw std::invalid_argument("key missing");
    return k;
  }

 public:
  // TODO: use shared memory for these
  // btw, these objects must be copied or else iterators used to modify things
  // are all wrong

  // Konstruktory: bezparametrowy tworzący pustą kolejkę, kopiujący i
  // przenoszący. Złożoność O(1).
  kvfifo() = default;
  kvfifo(kvfifo const &that) {
    items = std::list(that.items);
    for (auto walk = items.begin(); walk != items.end(); ++walk) {
      items_by_key[walk->key].push_back(walk);
    }
  }
  kvfifo(kvfifo &&that) noexcept {
    items = std::list(that.items);
    for (auto walk = items.begin(); walk != items.end(); ++walk) {
      items_by_key[walk->key].push_back(walk);
    }
  }

  // Operator przypisania przyjmujący argument przez wartość. Złożoność O(1)
  // plus czas niszczenia nadpisywanego obiektu.
  kvfifo &operator=(kvfifo that) {
    items = std::list(that.items);
    items_by_key.clear();
    for (auto walk = items.begin(); walk != items.end(); ++walk) {
      items_by_key[walk->key].push_back(walk);
    }
    return (*this);
  }

  // Metoda push wstawia wartość v na koniec kolejki, nadając jej klucz k.
  // Złożoność O(log n).
  void push(K const &k, V const &v) {
    items.push_back({k, v});
    items_by_key[k].push_back(std::prev(items.end()));
  }

  // Metoda pop() usuwa pierwszy element z kolejki. Jeśli kolejka jest pusta, to
  // podnosi wyjątek std::invalid_argument. Złożoność O(log n).
  void pop() {
    if (empty()) {
      throw std::invalid_argument("queue is empty");
    }

    auto [key, value] = items.front();
    items.pop_front();
    items_by_key[key].pop_front();
  }

  // Metoda pop(k) usuwa pierwszy element o podanym kluczu z kolejki. Jeśli
  // podanego klucza nie ma w kolejce, to podnosi wyjątek std::invalid_argument.
  // Złożoność O(log n).
  void pop(K const &k) {
    assert_key_exists(k);

    items_iterator_t node = items_by_key[k].front();
    items_by_key.pop_front();
    items.erase(node);
  }

  // Metoda move_to_back przesuwa elementy o kluczu k na koniec kolejki,
  // zachowując ich kolejność względem siebie. Zgłasza wyjątek
  // std::invalid_argument, gdy elementu o podanym kluczu nie ma w kolejce.
  // Złożoność O(m + log n), gdzie m to liczba przesuwanych elementów.
  void move_to_back(K const &k) {
    std::list<items_iterator_t> new_values_by_k;
    for (auto node : items_by_key[k]) {
      items.push_back(*node);
      new_values_by_k.push_back(prev(items.end()));
      items.erase(node);
    }
    items_by_key[k] = new_values_by_k;
  }

  // Metody front i back zwracają parę referencji do klucza i wartości
  // znajdującej się odpowiednio na początku i końcu kolejki. W wersji nie-const
  // zwrócona para powinna umożliwiać modyfikowanie wartości, ale nie klucza.
  // Dowolna operacja modyfikująca kolejkę może unieważnić zwrócone referencje.
  // Jeśli kolejka jest pusta, to podnosi wyjątek std::invalid_argument.
  // Złożoność O(1).
  std::pair<K const &, V &> front() { return items.front().as_pair(); }
  std::pair<K const &, V const &> front() const {
    return items.front().as_pair();
  }
  std::pair<K const &, V &> back() { return items.back().as_pair(); }
  std::pair<K const &, V const &> back() const {
    return items.back().as_pair();
  }

  // Metody first i last zwracają odpowiednio pierwszą i ostatnią parę
  // klucz-wartość o danym kluczu, podobnie jak front i back. Jeśli podanego
  // klucza nie ma w kolejce, to podnosi wyjątek std::invalid_argument.
  // Złożoność O(log n).
  std::pair<K const &, V &> first(K const &k) {
    return items_by_key.at(assert_key_exists(k)).front()->as_pair();
  }
  std::pair<K const &, V const &> first(K const &k) const {
    return items_by_key.at(assert_key_exists(k)).front()->as_pair();
  }
  std::pair<K const &, V &> last(K const &k) {
    return items_by_key.at(assert_key_exists(k)).back()->as_pair();
  }
  std::pair<K const &, V const &> last(K const &k) const {
    return items_by_key.at(assert_key_exists(k)).back()->as_pair();
  }

  // Metoda size zwraca liczbę elementów w kolejce. Złożoność O(1).
  size_t size() const { return items.size(); }

  // Metoda empty zwraca true, gdy kolejka jest pusta, a false w przeciwnym
  // przypadku. Złożoność O(1).
  bool empty() const { return items.empty(); }

  // Metoda count zwraca liczbę elementów w kolejce o podanym kluczu.
  // Złożoność O(log n).
  size_t count(K const &k) const {
    auto it = items_by_key.find(k);
    if (it == items_by_key.end()) return 0;
    return it->second.size();
  }

  // Metoda clear usuwa wszystkie elementy z kolejki. Złożoność O(n).
  void clear() {
    items.clear();
    items_by_key.clear();
  }

  // TODO: invalidate iterators (to make sure
  // Iterator k_iterator oraz metody k_begin i k_end, pozwalające przeglądać
  // zbiór kluczy w rosnącej kolejności wartości. Iteratory mogą być
  // unieważnione przez dowolną zakończoną powodzeniem operację modyfikującą
  // kolejkę oraz operacje front, back, first i last w wersjach bez const.
  // Iterator musi spełniać koncept std::bidirectional_iterator. Wszelkie
  // operacje w czasie O(log n). Przeglądanie całej kolejki w czasie O(n).
  // Iterator służy jedynie do przeglądania kolejki i za jego pomocą nie można
  // modyfikować kolejki, więc zachowuje się jak const_iterator z biblioteki
  // standardowej.
  class k_iterator {
   private:
    using keys_iterator_t = items_by_key_t::iterator;
    keys_iterator_t keys_iterator;

   public:
    k_iterator(keys_iterator_t iterator_) : keys_iterator(iterator_) {}
    k_iterator() = default;
    k_iterator(const k_iterator &that) : keys_iterator(that.keys_iterator) {}
    // TODO: i'm not sure i fully understand how iterators work
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
      ++keys_iterator;
      return *this;
    }
    k_iterator operator--(int) {
      auto old = *this;
      ++(*this);
      return old;
    }

    bool operator==(const kvfifo<K, V>::k_iterator &that) const {
      return keys_iterator == that.keys_iterator;
    }
    bool operator!=(const kvfifo<K, V>::k_iterator &that) const {
      return keys_iterator != that.keys_iterator;
    }
  };
  static_assert(std::bidirectional_iterator<k_iterator>);
  k_iterator k_begin() { return k_iterator(items_by_key.begin()); }
  k_iterator k_end() { return k_iterator(items_by_key.end()); }

  // TODO: remove when done debugging
  std::ostream &print(std::ostream &o) const {
    bool first = true;
    for (auto entry : items) {
      if (!first) o << ", ";
      first = false;
      o << "(" << entry.key << ": " << entry.value << ")";
    }
    return o;
  }
};

// TODO: remove when done debugging
template <typename K, typename V>
std::ostream &operator<<(std::ostream &o, const kvfifo<K, V> &q) {
  return q.print(o);
}

#endif