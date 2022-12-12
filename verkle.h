#include <bits/stdc++.h>

extern "C" {
  //#include "blst.h"
  #include "c_kzg.h"
  #include "bls12_381.h"
}
using namespace std;

// Constants

// TODO(pranav): Make this tunable.
// This means that each verkle tree node will have 2^WIDTH_BITS = 16 children.
constexpr int WIDTH_BITS = 4;
constexpr int KEYSIZE = 265;
constexpr int WIDTH = (1 << WIDTH_BITS);

struct VerkleNode {
  bool is_leaf = false;
  // Dummy commitment. Will be updated eventually.
  g1_t commitment = g1_generator;
  std::string key = "";
  std::string value = "";
  std::map<int, VerkleNode> childs;
  uint64_t hash = 0;
};

class VerkleTree {
  public:
  VerkleTree() {
    // special type = root.
    root_.is_leaf = false;
    // random commitment for now.
    root_.commitment = g1_generator;
    root_.childs.clear();
  }
  VerkleNode getRoot() {
    return root_;
  }

  void setRoot(VerkleNode& r) {
    root_ = r;
  }
  std::vector<int> get_key_path(const std::string& key);
  void plain_insert_verkle_node(const std::string& key, const std::string value);
  void compute_commitments();
  private:
  VerkleNode root_;
};

