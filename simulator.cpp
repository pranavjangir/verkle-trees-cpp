#include <chrono>
#include <iostream>

#include "ParserCode/json.hpp"
#include "verkle.h"

using json = nlohmann::json;

using namespace std;

int main() {
  std::ifstream file("ParserCode/data1.json");
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

  cout << "Start tree operations now!" << endl;
  auto insert_start = std::chrono::high_resolution_clock::now();
  VerkleTree vt;
  for (const auto& block : map) {
    for (const auto& key : block.second) {
      vt.plain_insert_verkle_node(key, "pranav");
    }
  }
  auto insert_end = std::chrono::high_resolution_clock::now();
  auto time_insert = std::chrono::duration_cast<std::chrono::seconds>(
      insert_end - insert_start);
  cout << "Time to insert all keys : " << time_insert.count() << " seconds."
       << endl;
  vt.compute_commitments();
  vector<string> keys_for_proof;
  for (const auto& block : map) {
    for (const auto& key : block.second) {
      keys_for_proof.push_back(key);
      // if (keys_for_proof.size() > 200) break;
    }
    // if (keys_for_proof.size() > 200) break;
  }
  cout << keys_for_proof.size() << " is the amount of key proofs we want!"
       << endl;
  auto proof_start = std::chrono::high_resolution_clock::now();
  auto proof = vt.get_verkle_multiproof(keys_for_proof);
  auto proof_end = std::chrono::high_resolution_clock::now();
  auto time_proof =
      std::chrono::duration_cast<std::chrono::seconds>(proof_end - proof_start);
  cout << "Time to generate proof for all keys : " << time_proof.count()
       << " seconds." << endl;

  auto verification_start = std::chrono::high_resolution_clock::now();
  bool success = vt.check_verkle_multiproof(keys_for_proof, proof);
  auto verification_end = std::chrono::high_resolution_clock::now();
  auto time_verification = std::chrono::duration_cast<std::chrono::seconds>(
      verification_end - verification_start);

  cout << "Time to verify proof for all keys : " << time_verification.count()
       << " seconds." << endl;
  cout << "SUCCESS? :::: " << success << endl;
  return 0;
}