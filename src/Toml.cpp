#include "Toml.h"
#include <cstring>
#include <fstream>
#include <memory>
#include <sstream>

extern "C" {
#include "toml.h"
}

namespace toml {

// wrapper for toml_datum_t cleanup
struct DatumDeleter {
    void operator()(char *ptr) const {
        if (ptr)
            free(ptr);
    }
    void operator()(toml_timestamp_t *ptr) const {
        if (ptr)
            free(ptr);
    }
};

static Table parseTomlTable(toml_table_t *raw_table);
static Array parseTomlArray(toml_array_t *raw_array);

static Array parseTomlArray(toml_array_t *raw_array) {
    Array result;

    if (!raw_array) {
        return result;
    }

    int size = toml_array_nelem(raw_array);
    if (size <= 0) {
        return result;
    }

    // determine array type
    char kind = toml_array_kind(raw_array);
    if (kind == 't') {
        // array of tables
        for (int i = 0; i < size; i++)
            if (toml_table_t *table = toml_table_at(raw_array, i))
                result.push(std::make_unique<Table>(parseTomlTable(table)));
    } else if (kind == 'a') {
        // array of arrays
        for (int i = 0; i < size; i++)
            if (toml_array_t *array = toml_array_at(raw_array, i))
                result.push(std::make_unique<Array>(parseTomlArray(array)));
    } else {
        // array of values
        char type = toml_array_type(raw_array);
        for (int i = 0; i < size; i++) {
            switch (type) {
            case 's': {
                if (toml_datum_t datum = toml_string_at(raw_array, i);
                    datum.ok) {
                    result.push(TomlString(datum.u.s));
                    free(datum.u.s);
                }
                break;
            }
            case 'b': {
                if (toml_datum_t datum = toml_bool_at(raw_array, i); datum.ok)
                    result.push(TomlBool(datum.u.b != 0));
                break;
            }
            case 'i': {
                if (toml_datum_t datum = toml_int_at(raw_array, i); datum.ok)
                    result.push(TomlInt(datum.u.i));
                break;
            }
            case 'd': {
                if (toml_datum_t datum = toml_double_at(raw_array, i); datum.ok)
                    result.push(TomlFloat(datum.u.d));
                break;
            }
            default:
                break;
            }
        }
    }

    return result;
}

static Table parseTomlTable(toml_table_t *raw_table) {
    Table result;

    if (!raw_table)
        return result;

    // iterate through keys
    for (int i = 0;; i++) {
        const char *key = toml_key_in(raw_table, i);
        if (!key)
            break;

        std::string key_str(key);

        // string
        if (toml_datum_t str_val = toml_string_in(raw_table, key); str_val.ok) {
            result.set(key_str, TomlString(str_val.u.s));
            free(str_val.u.s);
            continue;
        }

        // bool
        if (toml_datum_t bool_val = toml_bool_in(raw_table, key); bool_val.ok) {
            result.set(key_str, TomlBool(bool_val.u.b != 0));
            continue;
        }

        // int
        if (toml_datum_t int_val = toml_int_in(raw_table, key); int_val.ok) {
            result.set(key_str, TomlInt(int_val.u.i));
            continue;
        }

        // double
        if (toml_datum_t double_val = toml_double_in(raw_table, key);
            double_val.ok) {
            result.set(key_str, TomlFloat(double_val.u.d));
            continue;
        }

        // table
        if (toml_table_t *table = toml_table_in(raw_table, key)) {
            result.set(key_str, std::make_unique<Table>(parseTomlTable(table)));
            continue;
        }

        // array
        if (toml_array_t *array = toml_array_in(raw_table, key)) {
            result.set(key_str, std::make_unique<Array>(parseTomlArray(array)));
            continue;
        }
    }

    return result;
}

std::optional<std::string> Table::getString(const std::string &key) const {
    auto it = values_.find(key);
    if (it == values_.end())
        return std::nullopt;

    if (auto *val = std::get_if<TomlString>(&it->second))
        return *val;
    return std::nullopt;
}

std::optional<bool> Table::getBool(const std::string &key) const {
    auto it = values_.find(key);
    if (it == values_.end())
        return std::nullopt;

    if (auto *val = std::get_if<TomlBool>(&it->second))
        return *val;
    return std::nullopt;
}

std::optional<int64_t> Table::getInt(const std::string &key) const {
    auto it = values_.find(key);
    if (it == values_.end())
        return std::nullopt;

    if (auto *val = std::get_if<TomlInt>(&it->second))
        return *val;
    return std::nullopt;
}

std::optional<double> Table::getDouble(const std::string &key) const {
    auto it = values_.find(key);
    if (it == values_.end())
        return std::nullopt;

    if (auto *val = std::get_if<TomlFloat>(&it->second))
        return *val;
    return std::nullopt;
}

const Table *Table::getTable(const std::string &key) const {
    auto it = values_.find(key);
    if (it == values_.end())
        return nullptr;

    if (auto *val = std::get_if<std::unique_ptr<Table>>(&it->second))
        return val->get();
    return nullptr;
}

Table *Table::getTable(const std::string &key) {
    auto it = values_.find(key);
    if (it == values_.end())
        return nullptr;

    if (auto *val = std::get_if<std::unique_ptr<Table>>(&it->second))
        return val->get();
    return nullptr;
}

const Array *Table::getArray(const std::string &key) const {
    auto it = values_.find(key);
    if (it == values_.end())
        return nullptr;

    if (auto *val = std::get_if<std::unique_ptr<Array>>(&it->second))
        return val->get();
    return nullptr;
}

Array *Table::getArray(const std::string &key) {
    auto it = values_.find(key);
    if (it == values_.end())
        return nullptr;

    if (auto *val = std::get_if<std::unique_ptr<Array>>(&it->second))
        return val->get();
    return nullptr;
}

std::vector<std::string> Table::keys() const {
    std::vector<std::string> result;
    result.reserve(values_.size());
    for (const auto &[key, _] : values_) {
        result.push_back(key);
    }
    return result;
}

// Array methods
std::optional<std::string> Array::getString(size_t index) const {
    if (index >= values_.size())
        return std::nullopt;

    if (auto *val = std::get_if<TomlString>(&values_[index]))
        return *val;
    return std::nullopt;
}

std::optional<bool> Array::getBool(size_t index) const {
    if (index >= values_.size())
        return std::nullopt;

    if (auto *val = std::get_if<TomlBool>(&values_[index]))
        return *val;
    return std::nullopt;
}

std::optional<int64_t> Array::getInt(size_t index) const {
    if (index >= values_.size())
        return std::nullopt;

    if (auto *val = std::get_if<TomlInt>(&values_[index]))
        return *val;
    return std::nullopt;
}

std::optional<double> Array::getDouble(size_t index) const {
    if (index >= values_.size())
        return std::nullopt;

    if (auto *val = std::get_if<TomlFloat>(&values_[index]))
        return *val;
    return std::nullopt;
}

const Table *Array::getTable(size_t index) const {
    if (index >= values_.size())
        return nullptr;

    if (auto *val = std::get_if<std::unique_ptr<Table>>(&values_[index]))
        return val->get();
    return nullptr;
}

Table *Array::getTable(size_t index) {
    if (index >= values_.size())
        return nullptr;

    if (auto *val = std::get_if<std::unique_ptr<Table>>(&values_[index]))
        return val->get();
    return nullptr;
}

const Array *Array::getArray(size_t index) const {
    if (index >= values_.size())
        return nullptr;

    if (auto *val = std::get_if<std::unique_ptr<Array>>(&values_[index]))
        return val->get();
    return nullptr;
}

Array *Array::getArray(size_t index) {
    if (index >= values_.size())
        return nullptr;

    if (auto *val = std::get_if<std::unique_ptr<Array>>(&values_[index]))
        return val->get();
    return nullptr;
}

std::vector<std::string> Array::getStringVector() const {
    std::vector<std::string> result;
    result.reserve(values_.size());

    for (const auto &val : values_) {
        if (auto *str = std::get_if<TomlString>(&val)) {
            result.push_back(*str);
        }
    }

    return result;
}

std::vector<bool> Array::getBoolVector() const {
    std::vector<bool> result;
    result.reserve(values_.size());

    for (const auto &val : values_) {
        if (auto *b = std::get_if<TomlBool>(&val)) {
            result.push_back(*b);
        }
    }

    return result;
}

std::vector<int64_t> Array::getIntVector() const {
    std::vector<int64_t> result;
    result.reserve(values_.size());

    for (const auto &val : values_) {
        if (auto *i = std::get_if<TomlInt>(&val)) {
            result.push_back(*i);
        }
    }

    return result;
}

std::vector<double> Array::getDoubleVector() const {
    std::vector<double> result;
    result.reserve(values_.size());

    for (const auto &val : values_) {
        if (auto *d = std::get_if<TomlFloat>(&val)) {
            result.push_back(*d);
        }
    }

    return result;
}

std::vector<const Table *> Array::getTableVector() const {
    std::vector<const Table *> result;
    result.reserve(values_.size());

    for (const auto &val : values_) {
        if (auto *table_ptr = std::get_if<std::unique_ptr<Table>>(&val)) {
            result.push_back(table_ptr->get());
        }
    }

    return result;
}

ParseResult parse(const std::string &content) {
    ParseResult result;

    // make mutable copy
    std::vector<char> buffer(content.begin(), content.end());
    buffer.push_back('\0');

    char errbuf[200];
    toml_table_t *raw_table = toml_parse(buffer.data(), errbuf, sizeof(errbuf));

    if (!raw_table) {
        result.error = errbuf[0] ? std::string(errbuf) : "unknown error";
        return result;
    }

    result.table = std::make_unique<Table>(parseTomlTable(raw_table));
    toml_free(raw_table);

    return result;
}

ParseResult parseFile(const std::string &path) {
    ParseResult result;

    std::ifstream file(path);
    if (!file) {
        result.error = "Could not open file: " + path;
        return result;
    }

    std::stringstream buffer;
    buffer << file.rdbuf();

    return parse(buffer.str());
}

} // namespace toml
