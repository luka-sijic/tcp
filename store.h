#ifndef STORE_H
#define STORE_H

#include <unordered_map>
#include <string>

template<typename K, typename V>
class Store {
public:
  void set(K key, V value);
  V get(K Key);
  V get_all();
private:
  std::unordered_map<K, V> store;
};

template<typename K, typename V>
void Store<K,V>::set(K key, V value) {
  store[key] = value;
}

template<typename K, typename V>
V Store<K, V>::get(K key) {
  return store[key];
}

template<typename K, typename V>
V Store<K, V>::get_all() {
  std::string res = "KEYS:\n";
  for (auto& pair : store) {
    res += std::to_string(pair.first) + " " + pair.second + "\n";
  }
  return res;
}

#endif
