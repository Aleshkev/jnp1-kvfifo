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

  // Silna gwarancja odporności na wyjątki: w ogólnym przypadku, przed
  // jakąkolwiek modyfikacją naszych danych, najpierw tworzymy struktury ze
  // wszystkimi modyfikacjami jakie chcemy wprowadzić. Następnie, kiedy już
  // wszystko musi się dalej udać, aktualizujemy nasze dane.
  //
  // Używamy metod jak splice(), merge(), swap(), które zawsze się udają, bo
  // przenoszą już zaalokowane elementy. I, co ważne, iteratory nadal działają.
  // Metody usuwające elementy, jak std::map::erase, std::map::clear, też zawsze
  // się udają.

  // Robi, że jesteśmy jedynym obiektem z dostępem do naszych dzielonych danych.
  // Silna gwarancja odporności na wyjątki. Unieważnia wszystkie referencje.
  // TODO: TO OSTATNIE TO DUŻY PROBLEM O KTÓRYM NIE POMYŚLAŁEM >:CC
  // funkcje używają detach() na początku, a referencje mają się nie unieważniać
  // jak funkcja się nie uda
  void detach() {
    if (items.unique() && items_by_key.unique()) {
      return;
    }

    // Zapisujemy co trzeba zrobić. Skopiować wszystkie elementy, i stworzyć
    // nowe referencje do nich.
    auto new_items = std::make_shared<items_t>(*items);
    auto new_items_by_key = std::make_shared<items_by_key_t>();
    for (auto walk = new_items->begin(); walk != new_items->end(); ++walk) {
      (*new_items_by_key)[walk->key].push_back(walk);
    }

    // Zawsze się uda.
    items.swap(new_items);
    items_by_key.swap(new_items_by_key);
    external_ref_exists = false;
  }

 public:
  // Konstruktory: bezparametrowy tworzący pustą kolejkę, kopiujący i
  // przenoszący. Złożoność O(1).
  kvfifo() noexcept
      : items(std::make_shared<items_t>()),
        items_by_key(std::make_shared<items_by_key_t>()){};
  kvfifo(kvfifo const &that) noexcept
      : items(that.items), items_by_key(that.items_by_key) {
    if (that.external_ref_exists) {
      detach();
    }
  }
  kvfifo(kvfifo &&that) noexcept
      : items(that.items), items_by_key(that.items_by_key) {
    if (that.external_ref_exists) {
      detach();
    }
  }

  // Operator przypisania przyjmujący argument przez wartość. Złożoność O(1)
  // plus czas niszczenia nadpisywanego obiektu.
  kvfifo &operator=(kvfifo that) {
    auto new_items = that.items;
    auto new_items_by_key = that.items_by_key;

    // Żeby ten obiekt się nie zmienił przy błędzie, *ten drugi* obiekt ma się
    // skopiować jeśli to potrzebne. Jak się nie uda, on się nie zmieni, i my
    // też się nie zmienimy. Jak się uda, to się uda.
    if (that.external_ref_exists) {
      that.detach();
    }

    // Dalej bez wyjątków.

    items = new_items;
    items_by_key = new_items_by_key;

    return (*this);
  }

  // Metoda push wstawia wartość v na koniec kolejki, nadając jej klucz k.
  // Złożoność O(log n).
  void push(K const &k, V const &v) {
    detach();

    // Zapisujemy, co trzeba zrobić.
    // Trzeba dodać nowy element na koniec items. Trzeba zapisać referencję do
    // niego w odpowiednim miejscu w items_by_key.
    items_t new_item = {{k, v}};
    // Zapisujemy nowy element mapy dla przypadku gdy trzeba stworzyć nowy
    // element mapy, i nowy element listy dla przypadku gdy już jest w mapie.
    // TODO: usunąć niepotrzebne tworzenie obiektu, ale po testowaniu xd
    items_by_key_t new_items_at_key = {{k, {new_item.begin()}}};
    item_iterators_t new_item_reference = {new_item.begin()};

    // Dalej bez wyjątków.

    items->splice(items->end(), new_item);
    auto items_at_key = items_by_key->find(k);
    if (items_at_key == items_by_key->end()) {  // Klucz nie istniał.
      items_by_key->merge(new_items_at_key);
    } else {  // Istniał.
      items_at_key->second.splice(items_at_key->second.end(),
                                  new_item_reference);
    }

    external_ref_exists = false;  // Bo modyfikacja unieważnia.
  }

  // Metoda pop() usuwa pierwszy element z kolejki. Jeśli kolejka jest pusta, to
  // podnosi wyjątek std::invalid_argument. Złożoność O(log n).
  void pop() {
    assert_nonempty();
    detach();

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
    detach();

    // Dalej bez wyjątków.

    auto items_at_key = items_by_key->find(k);  // O(log n)
    auto node = items_at_key->front();
    items_at_key->pop_front();
    if (items_at_key->empty()) items_by_key->erase(items_at_key);
    items->erase(node);

    external_ref_exists = false;  // Bo modyfikacja unieważnia.
  }

  // Metoda move_to_back przesuwa elementy o kluczu k na koniec kolejki,
  // zachowując ich kolejność względem siebie. Zgłasza wyjątek
  // std::invalid_argument, gdy elementu o podanym kluczu nie ma w kolejce.
  // Złożoność O(m + log n), gdzie m to liczba przesuwanych elementów.
  void move_to_back(K const &k) {
    assert_key_exists(k);
    detach();

    // Zapisujemy, co trzeba zrobić.
    // Trzeba zamienić wszystkie elementy z kluczem k: usunąć wszystkie z items,
    // i dodać nowe na koniec. Przez to trzeba zamienić referencje w
    // items_by_key (zmienią się wszystkie).
    item_iterators_t new_items_at_key;
    items_t items_to_push_back;
    item_iterators_t items_to_erase;
    auto &items_at_key = items_by_key->at(k);
    for (const auto &node : items_at_key) {
      items_to_push_back.push_back(*node);
      new_items_at_key.push_back(std::prev(items_to_push_back.end()));
      items_to_erase.push_back(node);
    }

    // Dalej bez wyjątków.

    items->splice(items->end(), items_to_push_back);
    for (const auto &node : items_to_erase) items->erase(node);
    items_at_key.swap(new_items_at_key);

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
    detach();
    external_ref_exists = true;

    return items->front().as_pair();
  }
  std::pair<K const &, V const &> front() const {
    assert_nonempty();

    return items->front().as_pair();
  }
  std::pair<K const &, V &> back() {
    assert_nonempty();
    detach();
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
    detach();
    external_ref_exists = true;

    return items_by_key->at(k).front()->as_pair();
  }
  std::pair<K const &, V const &> first(K const &k) const {
    assert_key_exists(k);

    return items_by_key->at(k).front()->as_pair();
  }
  std::pair<K const &, V &> last(K const &k) {
    assert_key_exists(k);
    detach();
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
    auto it = items_by_key->find(k);
    if (it == items_by_key->end()) {
      return 0;
    }
    return it->second.size();
  }

  // Metoda clear usuwa wszystkie elementy z kolejki. Złożoność O(n).
  void clear() noexcept {
    detach();

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
  k_iterator k_begin() noexcept { return k_iterator(items_by_key->begin()); }
  k_iterator k_end() noexcept { return k_iterator(items_by_key->end()); }

  // TODO: remove when done debugging
  std::ostream &print(std::ostream &o) const {
    o << "[";
    bool first = true;
    for (auto entry : *items) {
      if (!first) o << ",  ";
      first = false;
      o << entry.key << ": " << entry.value;
    }
    return o << "]";
  }
};

// TODO: remove when done debugging
template <typename K, typename V>
std::ostream &operator<<(std::ostream &o, const kvfifo<K, V> &q) {
  return q.print(o);
}

#endif