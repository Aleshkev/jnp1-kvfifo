#include "kvfifo.h"

template <typename K, typename V>
K kvfifo_simple<K, V>::assert_key_exists(const K &k) const {
  if (count(k) == 0) throw std::invalid_argument("key missing");
    return k;
}

template <typename K, typename V>
void kvfifo_simple<K, V>::assert_nonempty() const {
  if (empty()) throw std::invalid_argument("empty");
}

template <typename K, typename V>
kvfifo_simple<K, V> &kvfifo_simple<K, V>::operator=(kvfifo_simple that) {
  auto new_items = that.item;
  auto new_items_by_key = that.items_by_key;

  return (*this);
}

template <typename K, typename V>
bool kvfifo_simple<K, V>::has_external_refs() const noexcept {
  return external_ref_exists;
}

template <typename K, typename V>
std::shared_ptr<kvfifo_simple<K, V>> kvfifo_simple<K, V>::copy() const {
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

template <typename K, typename V>
void kvfifo_simple<K, V>::push(K const &k, V const &v) {
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

template <typename K, typename V>
void kvfifo_simple<K, V>::pop() {
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

template <typename K, typename V>
void kvfifo_simple<K, V>::pop(K const &k) {
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

template <typename K, typename V>
void kvfifo_simple<K, V>::move_to_back(K const &k) {
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

template <typename K, typename V>
std::pair<K const &, V &> kvfifo_simple<K, V>::front() {
  assert_nonempty();

  external_ref_exists = true;
  return items->front().as_pair();
}

template <typename K, typename V>
std::pair<K const &, V const &> kvfifo_simple<K, V>::front() const {
  assert_nonempty();

  return items->front().as_pair();
}

template <typename K, typename V>
std::pair<K const &, V &> kvfifo_simple<K, V>::back() {
  assert_nonempty();

  external_ref_exists = true;
  return items->back().as_pair();
}

template <typename K, typename V>
std::pair<K const &, V const &> kvfifo_simple<K, V>::back() const {
  assert_nonempty();

  return items->back().as_pair();
}

template <typename K, typename V>
std::pair<K const &, V &> kvfifo_simple<K, V>::first(K const &k) {
  assert_key_exists(k);

  external_ref_exists = true;
  return items_by_key->at(k).front()->as_pair();
}

template <typename K, typename V>
std::pair<K const &, V const &> kvfifo_simple<K, V>::first(K const &k) const {
  assert_key_exists(k);

  return items_by_key->at(k).front()->as_pair();
}

template <typename K, typename V>
std::pair<K const &, V &> kvfifo_simple<K, V>::last(K const &k) {
  assert_key_exists(k);

  external_ref_exists = true;
  return items_by_key->at(k).back()->as_pair();
}

template <typename K, typename V>
std::pair<K const &, V const &> kvfifo_simple<K, V>::last(K const &k) const {
  assert_key_exists(k);

  return items_by_key->at(k).back()->as_pair();
}

template <typename K, typename V>
size_t kvfifo_simple<K, V>::size() const noexcept {
  return items->size();
}

template <typename K, typename V>
bool kvfifo_simple<K, V>::empty() const noexcept {
  return items->empty();
}

template <typename K, typename V>
size_t kvfifo_simple<K, V>::count(K const &k) const noexcept {
  // Bez wyjątków.
  auto it = items_by_key->find(k);
  if (it == items_by_key->end()) {
    return 0;
  }
  return it->second.size();
}

template <typename K, typename V>
void kvfifo_simple<K, V>::clear() noexcept {
  if (empty()) return;

  // Bez wyjątków.
  items->clear();
  items_by_key->clear();
}

template <typename K, typename V>
kvfifo_simple<K, V>::k_iterator kvfifo_simple<K, V>::k_begin() const noexcept {
  return k_iterator(items_by_key->begin());
}

template <typename K, typename V>
kvfifo_simple<K, V>::k_iterator kvfifo_simple<K, V>::k_end() const noexcept {
  return k_iterator(items_by_key->end());
}

template <typename K, typename V>
kvfifo<K, V> &kvfifo<K, V>::operator=(kvfifo that) noexcept {
  simple =
      that.simple->has_external_refs() ? that.simple->copy() : that.simple;

  return (*this);
}

template <typename K, typename V>
void kvfifo<K, V>::push(K const &k, V const &v) {
  auto simple_2 = (simple.unique() ? simple : simple->copy());
  simple_2->push(k, v);
  simple = simple_2;
}

template <typename K, typename V>
void kvfifo<K, V>::pop() {
  auto simple_2 = (simple.unique() ? simple : simple->copy());
  simple_2->pop();
  simple = simple_2;
}

template <typename K, typename V>
void kvfifo<K, V>::pop(K const &k) {
  auto simple_2 = (simple.unique() ? simple : simple->copy());
  simple_2->pop(k);
  simple = simple_2;
}

template <typename K, typename V>
void kvfifo<K, V>::move_to_back(K const &k) {
  auto simple_2 = (simple.unique() ? simple : simple->copy());
  simple_2->move_to_back(k);
  simple = simple_2;
}

template <typename K, typename V>
std::pair<K const &, V &> kvfifo<K, V>::front() {
  auto simple_2 = (simple.unique() ? simple : simple->copy());
  simple = simple_2;

  return simple_2->front();
}

template <typename K, typename V>
std::pair<K const &, V const &> kvfifo<K, V>::front() const {
  return simple->front();
}

template <typename K, typename V>
std::pair<K const &, V &> kvfifo<K, V>::back() {
  auto simple_2 = (simple.unique() ? simple : simple->copy());
  simple = simple_2;

  return simple_2->back();
}

template <typename K, typename V>
std::pair<K const &, V const &> kvfifo<K, V>::back() const {
  return simple->back();
}

template <typename K, typename V>
std::pair<K const &, V &> kvfifo<K, V>::first(K const &k) {
  auto simple_2 = (simple.unique() ? simple : simple->copy());
  simple = simple_2;

  return simple_2->first(k);
}

template <typename K, typename V>
std::pair<K const &, V const &> kvfifo<K, V>::first(K const &k) const {
  return simple->first(k);
}

template <typename K, typename V>
std::pair<K const &, V &> kvfifo<K, V>::last(K const &k) {
  auto simple_2 = (simple.unique() ? simple : simple->copy());
  simple = simple_2;

  return simple_2->last(k);
}

template <typename K, typename V>
std::pair<K const &, V const &> kvfifo<K, V>::last(K const &k) const {
  return simple->last(k);
}

template <typename K, typename V>
size_t kvfifo<K, V>::size() const noexcept {
  return simple->size();
}

template <typename K, typename V>
bool kvfifo<K, V>::empty() const noexcept {
  return simple->empty();
}

template <typename K, typename V>
size_t kvfifo<K, V>::count(K const &k) const noexcept {
  return simple->count(k);
}

template <typename K, typename V>
void kvfifo<K, V>::clear() noexcept {
  auto simple_2 = (simple.unique() ? simple : simple->copy());
  simple_2->clear();
  simple = simple_2;
}

template <typename K, typename V>
kvfifo<K, V>::k_iterator kvfifo<K, V>::k_begin() const noexcept {
  return simple->k_begin();
}

template <typename K, typename V>
kvfifo<K, V>::k_iterator kvfifo<K, V>::k_end() const noexcept {
  return simple->k_end();
}
