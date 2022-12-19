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
  // Konstruktory: bezparametrowy tworzący pustą kolejkę, kopiujący i
  // przenoszący. Złożoność O(1).
  kvfifo_simple() noexcept
      : items(std::make_shared<items_t>()),
        items_by_key(std::make_shared<items_by_key_t>()) {}
  kvfifo_simple(kvfifo_simple &&that)
    : items(std::move(that.items)),
      items_by_key(std::move(that.items_by_key)) {}
  kvfifo_simple(
    std::shared_ptr<items_t> new_items,
    std::shared_ptr<items_by_key_t> new_items_by_key
  ) : items(new_items), items_by_key(new_items_by_key) {}

  // Operator przypisania przyjmujący argument przez wartość. Złożoność O(1)
  // plus czas niszczenia nadpisywanego obiektu.
  kvfifo_simple &operator=(kvfifo_simple that) {
    auto new_items = that.item;
    auto new_items_by_key = that.items_by_key;

    return (*this);
  }

  bool has_external_refs() const noexcept {
    return external_ref_exists;
  }

  std::shared_ptr<kvfifo_simple> copy() const {
    auto new_items = std::make_shared<items_t>(*items);
    auto new_items_by_key = std::make_shared<items_by_key_t>();
    for (auto walk = new_items->begin(); walk != new_items->end(); ++walk) {
      (*new_items_by_key)[walk->key].push_back(walk);
    }

    return std::make_shared<kvfifo_simple>(new_items, new_items_by_key);
  }

  // Metoda push wstawia wartość v na koniec kolejki, nadając jej klucz k.
  // Złożoność O(log n).
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
    if (items_at_key == items_by_key->end()) {  // Klucz nie istniał.
      items_by_key->merge(items_by_key_please_insert_maybe);
    } else {  // Istniał.
      items_at_key->second.splice(items_at_key->second.end(),
                                  item_at_key_please_push_back_maybe);
    }

    external_ref_exists = false;  // Bo modyfikacja unieważnia.
  }

  // Metoda pop() usuwa pierwszy element z kolejki. Jeśli kolejka jest pusta, to
  // podnosi wyjątek std::invalid_argument. Złożoność O(log n).
  void pop() {
    assert_nonempty();

    // Dalej bez wyjątków.

    auto [key, value] = items->front();
    items->pop_front();
    auto &items_at_key = items_by_key->at(key);
    items_at_key.pop_front();
    if (items_at_key.empty()) items_by_key->erase(key);

    external_ref_exists = false;  // Bo modyfikacja unieważnia.
  }

  // Metoda pop(k) usuwa pierwszy element o podanym kluczu z kolejki. Jeśli
  // podanego klucza nie ma w kolejce, to podnosi wyjątek std::invalid_argument.
  // Złożoność O(log n).
  void pop(K const &k) {
    assert_key_exists(k);

    // Dalej bez wyjątków.

    auto &items_at_key = items_by_key->at(k);  // O(log n)
    const auto &node = items_at_key.front();
    items_at_key.pop_front();
    if (items_at_key.empty()) items_by_key->erase(k);  // O(log n)
    items->erase(node);                                // O(1)

    external_ref_exists = false;  // Bo modyfikacja unieważnia.
  }

  // Metoda move_to_back przesuwa elementy o kluczu k na koniec kolejki,
  // zachowując ich kolejność względem siebie. Zgłasza wyjątek
  // std::invalid_argument, gdy elementu o podanym kluczu nie ma w kolejce.
  // Złożoność O(m + log n), gdzie m to liczba przesuwanych elementów.
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

    external_ref_exists = false;  // Bo modyfikacja unieważnia.
  }

  // Metody front i back zwracają parę referencji do klucza i wartości
  // znajdującej się odpowiednio na początku i końcu kolejki. W wersji nie-const
  // zwrócona para powinna umożliwiać modyfikowanie wartości, ale nie klucza.
  // Dowolna operacja modyfikująca kolejkę może unieważnić zwrócone referencje.
  // Jeśli kolejka jest pusta, to podnosi wyjątek std::invalid_argument.
  // Złożoność O(1).
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

  // Metody first i last zwracają odpowiednio pierwszą i ostatnią parę
  // klucz-wartość o danym kluczu, podobnie jak front i back. Jeśli podanego
  // klucza nie ma w kolejce, to podnosi wyjątek std::invalid_argument.
  // Złożoność O(log n).
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

  // Metoda size zwraca liczbę elementów w kolejce. Złożoność O(1).
  size_t size() const noexcept { return items->size(); }

  // Metoda empty zwraca true, gdy kolejka jest pusta, a false w przeciwnym
  // przypadku. Złożoność O(1).
  bool empty() const noexcept { return items->empty(); }

  // Metoda count zwraca liczbę elementów w kolejce o podanym kluczu.
  // Złożoność O(log n).
  size_t count(K const &k) const noexcept {
    // Bez wyjątków.
    auto it = items_by_key->find(k);
    if (it == items_by_key->end()) {
      return 0;
    }
    return it->second.size();
  }

  // Metoda clear usuwa wszystkie elementy z kolejki. Złożoność O(n).
  void clear() noexcept {
    if (empty()) return;

    // Bez wyjątków.
    items->clear();
    items_by_key->clear();
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
    explicit k_iterator(keys_iterator_t iterator_) : keys_iterator(iterator_) {}
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
  k_iterator k_begin() noexcept { return k_iterator(items_by_key->begin()); }
  k_iterator k_end() noexcept { return k_iterator(items_by_key->end()); }

  // TODO: remove when done debugging
  // std::ostream &print(std::ostream &o) const {
  //   o << "[";
  //   bool first = true;
  //   for (auto entry : *items) {
  //     if (!first) o << ",  ";
  //     first = false;
  //     o << entry.key << ": " << entry.value;
  //   }
  //   return o << "]";
  // }
};

// TODO: remove when done debugging
// template <typename K, typename V>
// std::ostream &operator<<(std::ostream &o, const kvfifo<K, V> &q) {
//   return q.print(o);
// }

template <typename K, typename V>
class kvfifo {
 private:
  std::shared_ptr<kvfifo_simple<K, V>> simple;

 public:
  // Konstruktory: bezparametrowy tworzący pustą kolejkę, kopiujący i
  // przenoszący. Złożoność O(1).
  kvfifo() noexcept
      : simple(std::make_shared<kvfifo_simple<K, V>>()) {}
  kvfifo(kvfifo const &that) noexcept
      : simple(that.simple->has_external_refs()
        ? that.simple->copy()
        : that.simple) {}
  kvfifo(kvfifo &&that) noexcept
      : simple(std::move(that.simple)) {}

  // Operator przypisania przyjmujący argument przez wartość. Złożoność O(1)
  // plus czas niszczenia nadpisywanego obiektu.
  kvfifo &operator=(kvfifo that) noexcept {
    simple = 
      that.simple->has_external_refs()
        ? that.simple->copy()
        : that.simple;

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
  std::pair<K const &, V const &> front() const {
    return simple->front();
  }
  std::pair<K const &, V &> back() {
    auto simple_2 = (simple.unique() ? simple : simple->copy());
    simple = simple_2;
    
    return simple_2->back();
  }
  std::pair<K const &, V const &> back() const {
    return simple->back();
  }
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

  // TODO: This probably can't be defined like this
  kvfifo_simple<K, V>::k_iterator k_begin() noexcept { return simple->k_begin(); }
  kvfifo_simple<K, V>::k_iterator k_end() noexcept { return simple->k_end(); }
};

#endif