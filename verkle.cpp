#include "verkle.h"

using namespace std;

vector<int> VerkleTree::get_key_path(const string& key) {
    // TODO(pranav): Remove this assertion later.
    assert(WIDTH == 4);
    vector<int> out;
    string stripped = key;
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

void VerkleTree::plain_insert_verkle_node(const string& key,
 const string value) {
    // starting from root, iterate downwards and add the corresponding node.
    VerkleNode* cur = &root_;
    VerkleNode* prev = cur;
    // keep moving to children till you encounter a node that is not leaf.
    // or we may exit early as well.
    vector<int> ids = get_key_path(key);
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
    string cur_key = prev->childs[idx].key;
    string cur_val = prev->childs[idx].value;
    VerkleNode nwnode;
    // Delete existing leaf.
    cur->childs[idx] = nwnode;
    // Insert the req node.
    plain_insert_verkle_node(key, value);
    // Reinsert the deleted leaf.
    plain_insert_verkle_node(cur_key, cur_val);
}

void dfs_commitment(VerkleNode& x) {
    
}

void VerkleTree::compute_commitments() {
    // Computes the commitments for all the nodes in tree, starting top-down from root.
    dfs_commitment(root_);
}

int main() {
    cout << "Dummy main function!";
    return 0;
}