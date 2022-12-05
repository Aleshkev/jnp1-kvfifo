#ifndef KVFIFO_H
#define KVFIFO_H

#include <cstddef>
#include <iterator>
#include <utility>

template <typename K, typename V>
class kvfifo {
 public:
  // Konstruktory: bezparametrowy tworzący pustą kolejkę, kopiujący i
  // przenoszący. Złożoność O(1).
  kvfifo();
  kvfifo(kvfifo const &);
  kvfifo(kvfifo &&);

  // Operator przypisania przyjmujący argument przez wartość. Złożoność O(1)
  // plus czas niszczenia nadpisywanego obiektu.
  kvfifo &operator=(kvfifo other);

  // Metoda push wstawia wartość v na koniec kolejki, nadając jej klucz k.
  // Złożoność O(log n).
  void push(K const &k, V const &v);

  // Metoda pop() usuwa pierwszy element z kolejki. Jeśli kolejka jest pusta, to
  // podnosi wyjątek std::invalid_argument. Złożoność O(log n).
  void pop();

  // Metoda pop(k) usuwa pierwszy element o podanym kluczu z kolejki. Jeśli
  // podanego klucza nie ma w kolejce, to podnosi wyjątek std::invalid_argument.
  // Złożoność O(log n).
  void pop(K const &);

  // Metoda move_to_back przesuwa elementy o kluczu k na koniec kolejki,
  // zachowując ich kolejność względem siebie. Zgłasza wyjątek
  // std::invalid_argument, gdy elementu o podanym kluczu nie ma w kolejce.
  // Złożoność O(m + log n), gdzie m to liczba przesuwanych elementów.
  void move_to_back(K const &k);

  // Metody front i back zwracają parę referencji do klucza i wartości
  // znajdującej się odpowiednio na początku i końcu kolejki. W wersji nie-const
  // zwrócona para powinna umożliwiać modyfikowanie wartości, ale nie klucza.
  // Dowolna operacja modyfikująca kolejkę może unieważnić zwrócone referencje.
  // Jeśli kolejka jest pusta, to podnosi wyjątek std::invalid_argument.
  // Złożoność O(1).
  std::pair<K const &, V &> front();
  std::pair<K const &, V const &> front() const;
  std::pair<K const &, V &> back();
  std::pair<K const &, V const &> back() const;

  // Metody first i last zwracają odpowiednio pierwszą i ostatnią parę
  // klucz-wartość o danym kluczu, podobnie jak front i back. Jeśli podanego
  // klucza nie ma w kolejce, to podnosi wyjątek std::invalid_argument.
  // Złożoność O(log n).
  std::pair<K const &, V &> first(K const &key);
  std::pair<K const &, V const &> first(K const &key) const;
  std::pair<K const &, V &> last(K const &key);
  std::pair<K const &, V const &> last(K const &key) const;

  // Metoda size zwraca liczbę elementów w kolejce. Złożoność O(1).
  size_t size() const;

  // Metoda empty zwraca true, gdy kolejka jest pusta, a false w przeciwnym
  // przypadku. Złożoność O(1).
  bool empty() const;

  // Metoda count zwraca liczbę elementów w kolejce o podanym kluczu.
  // Złożoność O(log n).
  size_t count(K const &) const;

  // Metoda clear usuwa wszystkie elementy z kolejki. Złożoność O(n).
  void clear();

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
   public:
    using iterator_category = std::bidirectional_iterator_tag;
    using difference_type = std::ptrdiff_t;
    using value_type = V;
    using pointer = V *;
    using reference = V &;

    reference operator*();
    pointer operator->();

    k_iterator &operator++();
    k_iterator operator++(int);

    bool operator==(const kvfifo<K, V>::k_iterator &that);
    bool operator!=(const kvfifo<K, V>::k_iterator &that);
  };
  k_iterator k_begin();
  k_iterator k_end();
};

#endif