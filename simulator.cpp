#include <chrono>
#include <iostream>

#include "ParserCode/json.hpp"
#include "verkle.h"

using json = nlohmann::json;

using namespace std;

std::unordered_map<std::string, std::vector<std::string>> read_block_file(
    string file_name) {
  std::ifstream file(file_name);
  if (!file.is_open()) {
    std::cerr << "Failed to open file" << std::endl;
    return {};
  }

  json data;
  file >> data;

  std::unordered_map<std::string, std::vector<std::string>> map;

  for (json::iterator it = data.begin(); it != data.end(); ++it) {
    std::string key = it.key();
    json value = it.value();

    // Make sure the value is a JSON array
    if (!value.is_array()) {
      std::cerr << "JSON value is not an array" << std::endl;
      return {};
    }

    // Create a vector to store the strings in the JSON array
    std::vector<std::string> vec;

    // Iterate over the elements in the JSON array
    for (json::iterator jt = value.begin(); jt != value.end(); ++jt) {
      // Make sure the element is a string
      if (!jt->is_string()) {
        std::cerr << "JSON array element is not a string" << std::endl;
        return {};
      }
      vec.push_back(*jt);
    }
    map[key] = vec;
  }
  return map;
}

void run_n_blocks(int n, VerkleTree& vt,
std::unordered_map<std::string, std::vector<std::string>>& map) {
  cout << "Start tree operations now!" << endl;
  vector<string> keys_for_proof;
  int num_blocks = n;
  for (const auto& block : map) {
    if (num_blocks == 0) break;
    for (const auto& key : block.second) {
      keys_for_proof.push_back(key);
    }
    num_blocks--;
  }
  cout << keys_for_proof.size() << " is the amount of key proofs we want!"
       << endl;
  auto proof_start = std::chrono::high_resolution_clock::now();
  auto proof = vt.get_verkle_multiproof(keys_for_proof);
  auto proof_end = std::chrono::high_resolution_clock::now();
  auto time_proof =
      std::chrono::duration_cast<std::chrono::seconds>(proof_end - proof_start);

  auto verification_start = std::chrono::high_resolution_clock::now();
  bool success = vt.check_verkle_multiproof(keys_for_proof, proof);
  auto verification_end = std::chrono::high_resolution_clock::now();
  auto time_verification = std::chrono::duration_cast<std::chrono::seconds>(
      verification_end - verification_start);
}

int main() {
  auto map = read_block_file("ParserCode/data2000-1.json");

  auto insert_start = std::chrono::high_resolution_clock::now();
  VerkleTree vt(KZG);
  for (const auto& block : map) {
    for (const auto& key : block.second) {
      vt.plain_insert_verkle_node(key, "pranav");
    }
  }
  vt.compute_commitments();
  auto insert_end = std::chrono::high_resolution_clock::now();
  auto time_insert = std::chrono::duration_cast<std::chrono::seconds>(
      insert_end - insert_start);
  cout << "Time to insert all keys : " << time_insert.count() << " seconds."
       << endl;

  for (int i = 1 ; i <= 12; ++i) {
    cout << "Running for last " << i << " blocks | " << "Width_bits = " << WIDTH_BITS << endl;
    run_n_blocks(i, vt, map);
    cout << "____________________________________________________________\n";
  }
  return 0;
}