#pragma once

#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <unordered_map>
#include <variant>
#include <vector>

namespace toml {

class Value;
class Table;
class Array;

using TomlInt = int64_t;
using TomlFloat = double;
using TomlBool = bool;
using TomlString = std::string;

using TomlValue =
    std::variant<std::monostate, TomlBool, TomlInt, TomlFloat, TomlString,
                 std::unique_ptr<Table>, std::unique_ptr<Array>>;

class Array {
  public:
    Array() = default;

    // size of array
    size_t size() const { return values_.size(); }
    bool empty() const { return values_.empty(); }

    // get value at index
    template <typename T> std::optional<T> get(size_t index) const {
        if (index >= values_.size())
            return std::nullopt;

        if (auto *val = std::get_if<T>(&values_[index]))
            return *val;
        return std::nullopt;
    }

    std::optional<std::string> getString(size_t index) const;
    std::optional<bool> getBool(size_t index) const;
    std::optional<int64_t> getInt(size_t index) const;
    std::optional<double> getDouble(size_t index) const;

    // get table at index
    const Table *getTable(size_t index) const;
    Table *getTable(size_t index);

    // get array at index
    const Array *getArray(size_t index) const;
    Array *getArray(size_t index);

    // get vectors of primitives
    std::vector<std::string> getStringVector() const;
    std::vector<bool> getBoolVector() const;
    std::vector<int64_t> getIntVector() const;
    std::vector<double> getDoubleVector() const;

    // get vector of tables
    std::vector<const Table *> getTableVector() const;

    void push(TomlValue value) { values_.push_back(std::move(value)); }

  private:
    std::vector<TomlValue> values_;
};

class Table {
  public:
    Table() = default;

    // get value with default value if not found
    template <typename T>
    T get(const std::string &key, T default_value = T{}) const {
        auto it = values_.find(key);
        if (it == values_.end())
            return default_value;

        if (auto *val = std::get_if<T>(&it->second))
            return *val;
        return default_value;
    }

    std::optional<std::string> getString(const std::string &key) const;
    std::optional<bool> getBool(const std::string &key) const;
    std::optional<int64_t> getInt(const std::string &key) const;
    std::optional<double> getDouble(const std::string &key) const;

    const Table *getTable(const std::string &key) const;
    Table *getTable(const std::string &key);
    const Array *getArray(const std::string &key) const;
    Array *getArray(const std::string &key);

    // check if key exists
    bool has(const std::string &key) const {
        return values_.find(key) != values_.end();
    }

    // get all keys
    std::vector<std::string> keys() const;

    void set(const std::string &key, TomlValue value) {
        values_[key] = std::move(value);
    }

  private:
    std::unordered_map<std::string, TomlValue> values_;
};

// parse result
struct ParseResult {
    std::unique_ptr<Table> table;
    std::string error;

    operator bool() const { return table != nullptr; }
};

ParseResult parse(const std::string &content);
ParseResult parseFile(const std::string &path);

} // namespace toml
