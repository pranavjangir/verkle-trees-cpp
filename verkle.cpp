#include "verkle.h"
#include "blst.hpp"

using namespace std;

// Fiat shamir heuristic random values.
// TODO(pranav): These should ideally be calculated 
// using a suitable hash function.
// For benchmarking purposes, we are keeping them constants.
int rr = 22;
int tt = 42;

std::vector<int> VerkleTree::get_key_path(const std::string& key) {
    // TODO(pranav): Remove this assertion later.
    assert(WIDTH_BITS == 4);
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
// hashes 384 bits to a uint64_t
// TODO(pranav): NOT A GOOD HASH FUNCTION.
uint64_t hash_commitment(g1_t* comm) {
    uint64_t op = 0;
    auto hasher = std::hash<uint64_t>();
    for (int i = 0; i < 384/8/sizeof(limb_t); ++i) {
        op += hasher(comm->x.l[i]);
        op += hasher(comm->y.l[i]);
        op += hasher(comm->z.l[i]);
    }
    return op;
}

void VerkleTree::poly_commitment(g1_t* out, const vector<uint64_t>& vals) {
    fr_t valfr[WIDTH + 1];
    for (int i = 0; i < WIDTH; ++i) {
        fr_from_uint64(&valfr[i], vals[i]);
    }
    // apply pippenger
    g1_linear_combination(out, s1_lagrange, valfr, WIDTH);
}

// Same as above but different input parameters. 
// I messed up while writing lol.
void VerkleTree::poly_commitment_fr(g1_t* out, const vector<fr_t>& vals) {
    fr_t valfr[WIDTH + 1];
    for (int i = 0; i < WIDTH; ++i) {
        valfr[i] = vals[i];
    }
    // apply pippenger
    g1_linear_combination(out, s1_lagrange, valfr, WIDTH);
}

void VerkleTree::dfs_commitment(VerkleNode& x) {
    if (x.is_leaf) {
        auto hasher = std::hash<std::string>();
        uint64_t hashv = hasher(x.key);
        hashv += hasher(x.value);
        x.hash = hashv;
        cerr << "leaf hash : " << x.hash << endl;
        // No need to calculate commitment.
        return;
    }
    std::vector<uint64_t> child_hashes(WIDTH, 0);
    for (int i = 0; i < WIDTH; ++i) {
        if (x.childs.find(i) != x.childs.end()) {
            dfs_commitment(x.childs[i]);
            child_hashes[i] = x.childs[i].hash;
        }
    }
    poly_commitment(&x.commitment, child_hashes);
    x.hash = hash_commitment(&x.commitment);
    cerr << "internal hash : " << x.hash << endl;
}

void VerkleTree::compute_commitments() {
    // Computes the commitments for all the nodes in tree, starting top-down from root.
    dfs_commitment(root_);
}

std::vector< pair<vector<int>, pair<VerkleNode, int>> > 
    VerkleTree::get_path(const string& key) {
    // start from root and get the path.

    vector< pair<vector<int>, pair<VerkleNode, int>> > out;
    auto curnode = root_;
    auto idx = get_key_path(key);
    int ptr = 0;
    vector<int> path_so_far;
    path_so_far.push_back(-1);
    while(curnode.is_leaf == false) {
        int nxt = idx[ptr];
        ptr++;
        out.push_back(make_pair(path_so_far, make_pair(curnode, nxt)));
        curnode = curnode.childs[nxt];
        path_so_far.push_back(nxt);
    }
    if (!curnode.is_leaf) {
        cout << "get_path::End node is not leaf." << endl;
        assert(false);
    }
    if (curnode.key != key) {
        // It is expected that there will be some path.
        cout << "get_path::No path found." << endl;
        assert(false);
    }
    return out;
}

vector<fr_t> VerkleTree::in_domain_q(const vector<fr_t>& in, int idx) {
    assert(in.size() == WIDTH);
    assert(idx < in.size());

    vector<fr_t> out(WIDTH);
    for (int i = 0; i < WIDTH; ++i) {
        fr_from_uint64(&out[i], 0);
    }

    fr_t y = in[idx];

    for (int i = 0 ; i < WIDTH; ++i) {
        if (i == idx) continue; // We cannot do a simple division in this case.
        fr_sub(&out[i], &in[i], &y);
        fr_mul(&out[i], &out[i], 
                &ffts_.expanded_roots_of_unity[(WIDTH - i)%WIDTH]);
        fr_mul(&out[i], &out[i], &inv[(WIDTH + idx - i)%WIDTH]);
    }
    // Now compute out[idx].
    for (int i = 0; i < WIDTH; ++i) {
        if (i == idx) continue;
        fr_t tmp;
        fr_mul(&tmp, &out[i], 
                &ffts_.expanded_roots_of_unity[(i - idx + WIDTH)%WIDTH]);
        fr_sub(&out[idx], &out[idx], &tmp);
    }
    return out;
}

pair<fr_t, g1_t> VerkleTree::eval_and_proof(const vector<fr_t>& p, fr_t pt) {
    // Poly structure as followed by z-kzg library.
    // C semantics.
    assert(p.size() == WIDTH);
    poly ply;
    new_poly(&ply, WIDTH);
    for (int i = 0; i < WIDTH; ++i) {
        ply.coeffs[i] = p[i];
    }
    fr_t eval;
    g1_t proof;
    // TODO(pranav): Add the assert sanity checks for all c-kzg lib calls.
    eval_poly(&eval, &ply, &pt);
    assert(compute_proof_single(&proof, &ply, &pt, &kzgs_) == C_KZG_OK);
    free_poly(&ply);
    return make_pair(eval, proof);
}

VerkleProof VerkleTree::kzg_gen_multiproof(vector< pair<VerkleNode,
                                         set<int> > > nodes) {
    VerkleProof out;
    // Compressed commitments :
    for (auto& x : nodes) {
        out.commitments.push_back(x.first.commitment);
    }
    // Generate a list of <commitment, poly, single_index_to_proof>
    // That is, flatten the `nodes` structure.
    struct flat_node {
        g1_t commitment;
        vector<fr_t> p;
        int idx;
    };
    vector< flat_node > X;
    for (auto& nod : nodes) {
        vector<fr_t> ptmp(WIDTH);
        const auto& vnode = nod.first;
        for (int i = 0; i < WIDTH; ++i) {
            if (vnode.childs.find(i) == vnode.childs.end()) {
                fr_from_uint64(&ptmp[i], 0);
            } else {
                fr_from_uint64(&ptmp[i], vnode.childs.at(i).hash);
            }
        }
        for (auto idx : nod.second) {
            flat_node nw;
            nw.commitment = vnode.commitment;
            nw.p = ptmp;
            nw.idx = idx;
            X.push_back(nw);
        }
    }
    // Now we get the multiproofs over X.
    fr_t r;
    fr_t rpow;
    fr_from_uint64(&rpow, 1);
    fr_from_uint64(&r, (uint64_t)rr);
    vector<fr_t> g(WIDTH);
    for (int i = 0; i < WIDTH; ++i) {
        fr_from_uint64(&g[i], 0);
    }
    for (auto& x : X) {
        auto q = in_domain_q(x.p, x.idx);
        assert(q.size() == WIDTH);
        for (int i = 0; i < WIDTH; ++i) {
            fr_t tmp;
            fr_mul(&tmp, &rpow, &q[i]);
            fr_add(&g[i], &g[i], &tmp);
        }
        fr_mul(&rpow, &rpow, &r);
    }
    g1_t gCommit; // D as per the document.
    poly_commitment_fr(&gCommit, g);

    // TODO(pranav): change to actual randomness.
    uint64_t t = 56;
    fr_t tfr;
    fr_from_uint64(&tfr, t);
    // Do the same thing for h, but @ t.
    vector<fr_t> h(WIDTH);
    for (int i = 0; i < WIDTH; ++i) {
        fr_from_uint64(&h[i], 0);
    }
    fr_from_uint64(&rpow, 1);
    fr_from_uint64(&r, (uint64_t)rr);
    for (auto& x : X) {
        fr_t denom_inv = tfr;
        fr_sub(&denom_inv, &denom_inv, ffts_.expanded_roots_of_unity + x.idx);
        fr_inv(&denom_inv, &denom_inv);
        for (int i = 0; i < WIDTH; ++i) {
            fr_t tmp = rpow;
            fr_mul(&tmp, &tmp, &x.p[i]);
            fr_mul(&tmp, &tmp, &denom_inv);
            fr_add(&h[i], &h[i], &tmp);
        }
        fr_mul(&rpow, &rpow, &r);
    }

    // get proofs (and values) for h and g evaluated at random t.
    auto h_eval_proof = eval_and_proof(h, tfr);
    auto g_eval_proof = eval_and_proof(g, tfr);

    fr_t qq;
    fr_from_uint64(&qq, 22);
    g1_t combined_proof;
    g1_mul(&combined_proof, &g_eval_proof.second, &qq);
    g1_add_or_dbl(&combined_proof, &combined_proof, &h_eval_proof.second);
    out.proof = combined_proof;
    out.D = gCommit;
    out.eval = h_eval_proof.first;
    return out;
}

VerkleProof VerkleTree::get_verkle_multiproof(const vector<string>& keys) {

    map<vector<int>, pair<VerkleNode, set<int> > > required_proofs;
    for (auto& key : keys) {
        auto node_path = get_path(key);
        // update the main database.
        for (auto& ni : node_path) {
            // add indexes to the node.
            if (required_proofs.find(ni.first) == required_proofs.end()) {
                set<int> tmp;
                tmp.insert(ni.second.second);
                required_proofs[ni.first] = 
                    (make_pair(ni.second.first, tmp));
            } else {
                auto& val = required_proofs[ni.first];
                val.second.insert(ni.second.second);
            }
        }
    }
    vector< pair<VerkleNode, set<int> > > to_proof;
    for (auto path_and_req_proof : required_proofs) {
        to_proof.push_back(path_and_req_proof.second);
    }
    VerkleProof out = kzg_gen_multiproof(to_proof);
    cout << "Proof size (# of elements) : "  << out.commitments.size() << endl;
    return out;
}

int main() {
    std::cout << "Dummy main function!\n";
    VerkleTree vt;
    string key = "0x";
    string value = "abcdefg";
    for (int i = 0; i < 64; ++i) {
        key += 'a';
    }
    vt.plain_insert_verkle_node(key, value);
    vt.compute_commitments();
    return 0;
}