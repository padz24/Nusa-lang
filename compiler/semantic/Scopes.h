#pragma once
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

namespace nusa {

// Tumpukan scope leksikal (v0.6.0): setiap Block mendorong scope baru.
//  - insert: ke scope TERDALAM; false bila nama sudah ada di scope itu
//    (shadowing antar-scope diizinkan, duplikat se-scope ditolak).
//  - lookup: dari terdalam ke terluar.
// Dipakai symbol values (semantic), env tipe (checker), dan bindings
// (codegen) — ketiganya harus berperilaku scope-identik.
template <typename V> class Scopes {
public:
  void push() { scopes_.emplace_back(); }
  void pop() {
    if (!scopes_.empty())
      scopes_.pop_back();
  }
  void clear() { scopes_.clear(); }
  bool empty() const { return scopes_.empty(); }

  bool insert(const std::string &name, V value) {
    if (scopes_.empty())
      push();
    auto &top = scopes_.back();
    if (top.count(name))
      return false;
    top.emplace(name, std::move(value));
    return true;
  }

  // Tulis ke scope terdalam tanpa peduli isi sebelumnya (untuk jalur
  // pemulihan error; pemakaian normal memakai insert).
  void set(const std::string &name, V value) {
    if (scopes_.empty())
      push();
    scopes_.back()[name] = std::move(value);
  }

  std::optional<V> lookup(const std::string &name) const {
    for (auto it = scopes_.rbegin(); it != scopes_.rend(); ++it) {
      auto f = it->find(name);
      if (f != it->end())
        return f->second;
    }
    return std::nullopt;
  }

private:
  std::vector<std::unordered_map<std::string, V>> scopes_;
};

} // namespace nusa
