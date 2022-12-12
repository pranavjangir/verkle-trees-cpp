#include "verkle.h"
#include "blst.hpp"

//using namespace std;

std::vector<int> VerkleTree::get_key_path(const std::string& key) {
    // TODO(pranav): Remove this assertion later.
    assert(WIDTH == 4);
    std::vector<int> out;
    std::string stripped = key;
    if (key.length() == 64 + 2) { // key = 0x34fd....
        stripped = key.substr(2);
    }
    for (int i = 0; i < stripped.length(); ++i) {
        int val = -1;
        if (stripped[i] >= '0' && stripped[i] <= '9') 
            val = (stripped[i] - '0');
        if (stripped[i] >= 'a' && stripped[i] <= 'f') 
            val = 10 + (stripped[i] - 'a');
        assert(val >= 0 && val <= 15);
        out.push_back(val);
    }
    reverse(out.begin(), out.end());
    return out;
}

void VerkleTree::plain_insert_verkle_node(const std::string& key,
 const std::string value) {
    // starting from root, iterate downwards and add the corresponding node.
    VerkleNode* cur = &root_;
    VerkleNode* prev = cur;
    // keep moving to children till you encounter a node that is not leaf.
    // or we may exit early as well.
    std::vector<int> ids = get_key_path(key);
    int ptr = 0;
    int idx = 0;
    while (cur->is_leaf == false && ptr < ids.size()) {
        prev = cur;
        idx = ids[ptr];
        ptr++;
        // Found a child.
        if (cur->childs.find(idx) != cur->childs.end()) {
            cur = &cur->childs[idx];
        } else {
            VerkleNode nwnode;
            nwnode.is_leaf = true;
            nwnode.key = key;
            nwnode.value = value;
            cur->childs[idx] = nwnode;
            return;
        }
    }
    assert(cur->is_leaf == true);
    if (cur->key == key) {
        cur->value = value;
        return;
    }
    // Child must be present if we reach here.
    // and the node must be expanded.
    assert(prev->childs.find(idx) != prev->childs.end());
    std::string cur_key = prev->childs[idx].key;
    std::string cur_val = prev->childs[idx].value;
    VerkleNode nwnode;
    // Delete existing leaf.
    cur->childs[idx] = nwnode;
    // Insert the req node.
    plain_insert_verkle_node(key, value);
    // Reinsert the deleted leaf.
    plain_insert_verkle_node(cur_key, cur_val);
}

// Naive byte hashing function.
// hashes 48 bytes to a uint64_t
uint64_t hash_bytes(const uint8_t b[48]) {
    uint64_t op = 0;
    auto hasher = std::hash<uint8_t>();
    for (int i = 0; i < 48; ++i) {
        op += hasher(b[i]);
    }
    return op;
}

void dfs_commitment(VerkleNode& x) {
    if (x.is_leaf) {
        auto hasher = std::hash<std::string>();
        uint64_t hashv = hasher(x.key);
        hashv += hasher(x.value);
        x.hash = hashv;
        return;
    }
    std::vector<uint64_t> child_hashes(WIDTH, 0);
    for (int i = 0; i < WIDTH; ++i) {
        if (x.childs.find(i) != x.childs.end()) {
            dfs_commitment(x.childs[i]);
            child_hashes[i] = x.childs[i].hash;
        }
    }
    // TODO(pranav): change this to actual commitment calculation.
    x.commitment = g1_generator;
    uint8_t com_bytes[48];
    //(&com_bytes, &x.commitment);
    x.hash = hash_bytes(com_bytes);
}

void VerkleTree::compute_commitments() {
    // Computes the commitments for all the nodes in tree, starting top-down from root.
    dfs_commitment(root_);
}

int main() {
    std::cout << "Dummy main function!";
    return 0;
}