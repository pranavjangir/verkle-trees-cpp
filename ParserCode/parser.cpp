#include <fstream>
#include <iostream>
#include <string>
#include <unordered_map>
#include <vector>

#include "json.hpp"

using json = nlohmann::json;

int main() {
  // Open the file
  std::ifstream file("data2000-1.json");
  if (!file.is_open()) {
    std::cerr << "Failed to open file" << std::endl;
    return 1;
  }

  // Parse the JSON object from the file
  json data;
  file >> data;

  // Create a map to store the key-value pairs
  std::unordered_map<std::string, std::vector<std::string>> map;

  // Iterate over the keys in the JSON object
  for (json::iterator it = data.begin(); it != data.end(); ++it) {
    // Get the key and value from the iterator
    std::string key = it.key();
    json value = it.value();

    // Make sure the value is a JSON array
    if (!value.is_array()) {
      std::cerr << "JSON value is not an array" << std::endl;
      return 1;
    }

    // Create a vector to store the strings in the JSON array
    std::vector<std::string> vec;

    // Iterate over the elements in the JSON array
    for (json::iterator jt = value.begin(); jt != value.end(); ++jt) {
      // Make sure the element is a string
      if (!jt->is_string()) {
        std::cerr << "JSON array element is not a string" << std::endl;
        return 1;
      }

      // Add the string to the vector
      vec.push_back(*jt);
    }

    // Add the key-value pair to the map
    map[key] = vec;
  }

  // Print the map to verify that it was constructed correctly
  for (const auto& kv : map) {
    std::cout << kv.first << ": [";
    for (const auto& str : kv.second) {
      std::cout << str << ", ";
    }
    std::cout << "]" << std::endl;
  }

  return 0;
}
