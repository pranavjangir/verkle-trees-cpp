#include "verkle.h"

#include <chrono>

#include "ParserCode/json.hpp"
#include "blst.hpp"

using namespace std;

using json = nlohmann::json;

// Fiat shamir heuristic random values.
// TODO(pranav): These should ideally be calculated
// using a suitable hash function.
// For benchmarking purposes, we are keeping them constants.
int rr = 22;
int tt = 42;

std::vector<int> VerkleTree::get_key_path(const std::string& key) {
  // Bits must be multiple of 4.
  assert(WIDTH_BITS % 4 == 0);
  std::vector<int> out;
  std::string stripped = key;
  if (key.length() == 64 + 2) {  // key = 0x34fd....
    stripped = key.substr(2);
  }
  for (int i = 0; i < stripped.length(); i += (WIDTH_BITS / 4)) {
    int pw = 1;
    int idx = 0;
    for (int j = i; j < i + (WIDTH_BITS / 4) && j < stripped.length(); ++j) {
      int val = -1;
      if (stripped[j] >= '0' && stripped[j] <= '9') {
        val = (stripped[j] - '0');
      } else if (stripped[j] >= 'a' && stripped[j] <= 'f') {
        val = 10 + (stripped[j] - 'a');
      }
      if (val < 0 || val > 15) {
        cout << stripped[j] << " ---- " << val << endl;
      }
      assert(val >= 0 && val <= 15);
      idx += pw * val;
      pw *= 16;
    }
    out.push_back(idx);
  }
  reverse(out.begin(), out.end());
  return out;
}

void VerkleTree::plain_insert_verkle_node(const std::string& key,
                                          const std::string value) {
  // starting from root, iterate downwards and add the corresponding node.
  shared_ptr<VerkleNode> cur = root_;
  shared_ptr<VerkleNode> prev = root_;
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
      cur = cur->childs[idx];
    } else {
      shared_ptr<VerkleNode> nwnode = make_shared<VerkleNode>();
      nwnode->is_leaf = true;
      nwnode->key = key;
      nwnode->value = value;
      cur->childs[idx] = std::move(nwnode);
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
  std::string cur_key = prev->childs[idx]->key;
  std::string cur_val = prev->childs[idx]->value;
  shared_ptr<VerkleNode> nwnode = make_shared<VerkleNode>();
  // Delete existing leaf.
  prev->childs[idx] = std::move(nwnode);
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
  for (int i = 0; i < 384 / 8 / sizeof(limb_t); ++i) {
    op += hasher(comm->x.l[i]);
    op += hasher(comm->y.l[i]);
    op += hasher(comm->z.l[i]);
  }
  return op;
}

void VerkleTree::poly_commitment(g1_t* out, const vector<uint64_t>& vals) {
  fr_t valfr[WIDTH];
  for (int i = 0; i < WIDTH; ++i) {
    fr_from_uint64(&valfr[i], vals[i]);
  }
  // apply pippenger
  g1_linear_combination(out, s1_lagrange, valfr, WIDTH);
}

// Same as above but different input parameters.
// I messed up while writing lol.
void VerkleTree::poly_commitment_fr(g1_t* out, const vector<fr_t>& vals) {
  fr_t valfr[WIDTH];
  for (int i = 0; i < WIDTH; ++i) {
    valfr[i] = vals[i];
  }
  // apply pippenger
  g1_linear_combination(out, s1_lagrange, valfr, WIDTH);
}

// apply pippenger on given values.
void general_pippenger(g1_t* out, const vector<g1_t>& A,
                       const vector<fr_t>& B) {
  // Calculate A*B dot product.
  assert(A.size() == B.size());
  int n = A.size();
  // Forced to do this because pippenger APIs expect arrays.
  g1_t* a = new g1_t[n];
  fr_t* b = new fr_t[n];
  for (int i = 0; i < n; ++i) {
    a[i] = A[i];
    b[i] = B[i];
  }
  // TODO(pranav): make sure that the value pointed to by `out` is correct.
  g1_linear_combination(out, a, b, n);
  free(a);
  free(b);
}

void VerkleTree::dfs_commitment(shared_ptr<VerkleNode>& x) {
  if (x->is_leaf) {
    auto hasher = std::hash<std::string>();
    uint64_t hashv = hasher(x->key);
    hashv += hasher(x->value);
    x->hash = hashv;
    // cerr << "leaf hash : " << x->hash << endl;
    // No need to calculate commitment.
    return;
  }
  std::vector<uint64_t> child_hashes(WIDTH, 0);
  for (int i = 0; i < WIDTH; ++i) {
    if (x->childs.find(i) != x->childs.end()) {
      dfs_commitment(x->childs[i]);
      child_hashes[i] = x->childs[i]->hash;
    }
  }
  poly_commitment(&x->commitment, child_hashes);
  x->hash = hash_commitment(&x->commitment);
  // cerr << "internal hash : " << x->hash << endl;
}

void VerkleTree::compute_commitments() {
  // Computes the commitments for all the nodes in tree, starting top-down from
  // root.
  dfs_commitment(root_);
}

std::vector<pair<vector<int>, pair<shared_ptr<VerkleNode>, int>>>
VerkleTree::get_path(const string& key) {
  // start from root and get the path.

  vector<pair<vector<int>, pair<shared_ptr<VerkleNode>, int>>> out;
  auto curnode = root_;
  auto idx = get_key_path(key);
  int ptr = 0;
  vector<int> path_so_far;
  path_so_far.push_back(-1);
  while (curnode->is_leaf == false) {
    int nxt = idx[ptr];
    ptr++;
    out.push_back(make_pair(path_so_far, make_pair(curnode, nxt)));
    curnode = curnode->childs[nxt];
    path_so_far.push_back(nxt);
  }
  if (!curnode->is_leaf) {
    cout << "get_path::End node is not leaf." << endl;
    assert(false);
  }
  if (curnode->key != key) {
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

  for (int i = 0; i < WIDTH; ++i) {
    if (i == idx) continue;  // We cannot do a simple division in this case.
    fr_sub(&out[i], &in[i], &y);
    fr_mul(&out[i], &out[i],
           &ffts_.expanded_roots_of_unity[(WIDTH - i) % WIDTH]);
    fr_mul(&out[i], &out[i], &inv[(WIDTH + idx - i) % WIDTH]);
  }
  // Now compute out[idx].
  for (int i = 0; i < WIDTH; ++i) {
    if (i == idx) continue;
    fr_t tmp;
    fr_mul(&tmp, &out[i],
           &ffts_.expanded_roots_of_unity[(i - idx + WIDTH) % WIDTH]);
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

struct flat_node {
  g1_t commitment;
  vector<fr_t> p;
  int idx;
};

// For debugging purposes.
// Prints prime field element.
void prfp(fp_t x) {
  int sz = 384 / 8 / sizeof(limb_t);
  for (int i = 0; i < sz; ++i) {
    cout << "i = " << i << " --- " << x.l[i] << endl;
  }
  cout << "<><><><><><><><><><><><><><>\n";
}

// For debugging purposes.
// Prints field element.
void prfr(fr_t x) {
  int sz = 256 / 8 / sizeof(limb_t);
  for (int i = 0; i < sz; ++i) {
    cout << "i = " << i << " --- " << x.l[i] << endl;
  }
  cout << "<><><><><><><><><><><><><><>\n";
}

VerkleProof VerkleTree::kzg_gen_multiproof(
    vector<pair<shared_ptr<VerkleNode>, set<int>>> nodes) {
  VerkleProof out;
  // De-duplicated commitments :
  for (auto& x : nodes) {
    out.commitments.push_back(x.first->commitment);
  }
  cout << "Proof size (# of elements) : " << out.commitments.size() << endl;
  // Generate a list of <commitment, poly, single_index_to_proof>
  // That is, flatten the `nodes` structure.
  vector<flat_node> X;
  for (auto& nod : nodes) {
    vector<fr_t> ptmp(WIDTH);
    const auto& vnode = nod.first;
    for (int i = 0; i < WIDTH; ++i) {
      if (vnode->childs.find(i) == vnode->childs.end()) {
        fr_from_uint64(&ptmp[i], 0);
      } else {
        fr_from_uint64(&ptmp[i], vnode->childs.at(i)->hash);
      }
    }
    for (auto idx : nod.second) {
      flat_node nw;
      nw.commitment = vnode->commitment;
      nw.p = ptmp;
      nw.idx = idx;
      X.push_back(nw);
    }
  }

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
  g1_t gCommit;  // D as per the document.
  poly gpoly;
  new_poly(&gpoly, g.size());
  for (int i = 0; i < g.size(); ++i) gpoly.coeffs[i] = g[i];
  commit_to_poly(&gCommit, &gpoly, &kzgs_);
  free_poly(&gpoly);

  fr_t tfr;
  fr_from_uint64(&tfr, tt);
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
  g1_t E;
  poly hpoly;
  new_poly(&hpoly, h.size());
  for (int i = 0; i < h.size(); ++i) hpoly.coeffs[i] = h[i];
  commit_to_poly(&E, &hpoly, &kzgs_);
  free_poly(&hpoly);

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
  g1_mul(&out.D, &out.D, &qq);
  out.eval = h_eval_proof.first;
  out.eval2 = g_eval_proof.first;
  out.commitments.resize(1);
  out.commitments[0] = E;
  g1_add_or_dbl(&out.commitments[0], &out.commitments[0], &out.D);
  return out;
}

VerkleProof VerkleTree::get_verkle_multiproof(const vector<string>& keys) {
  map<vector<int>, pair<shared_ptr<VerkleNode>, set<int>>> required_proofs;
  for (auto& key : keys) {
    auto node_path = get_path(key);
    // update the main database.
    for (auto& ni : node_path) {
      // add indexes to the node.
      if (required_proofs.find(ni.first) == required_proofs.end()) {
        set<int> tmp;
        tmp.insert(ni.second.second);
        required_proofs[ni.first] = (make_pair(ni.second.first, tmp));
      } else {
        auto& val = required_proofs[ni.first];
        val.second.insert(ni.second.second);
      }
    }
  }
  vector<pair<shared_ptr<VerkleNode>, set<int>>> to_proof;
  for (auto path_and_req_proof : required_proofs) {
    to_proof.push_back(path_and_req_proof.second);
  }
  VerkleProof out = kzg_gen_multiproof(to_proof);
  return out;
}

bool VerkleTree::kzg_check_multiproof(const vector<g1_t>& commitments,
                                      const vector<int>& indices,
                                      const vector<fr_t>& Y,
                                      const VerkleProof& proof) {
  // calculate E(t) and g2(t).
  // Using given D (in `proof`) :=
  // Verify that (D - E - g2(t))
  assert(commitments.size() == indices.size());
  assert(Y.size() == commitments.size());
  fr_t r, t, q, g2_at_t;
  fr_from_uint64(&r, rr);
  fr_from_uint64(&t, tt);
  fr_from_uint64(&q, 22);
  fr_t rpow;
  fr_from_uint64(&rpow, 1);
  fr_from_uint64(&g2_at_t, 0);
  vector<fr_t> e_c(commitments.size());
  for (int i = 0; i < commitments.size(); ++i) {
    fr_t tmp, tmp2;
    fr_sub(&tmp, &t, &ffts_.expanded_roots_of_unity[indices[i]]);
    fr_div(&e_c[i], &rpow, &tmp);
    fr_mul(&tmp2, &e_c[i], &Y[i]);
    fr_add(&g2_at_t, &g2_at_t, &tmp2);
    fr_mul(&rpow, &rpow, &r);
  }
  g1_t E;
  general_pippenger(&E, commitments, e_c);
  fr_t w;
  fr_sub(&w, &proof.eval, &g2_at_t);

  g1_t final_compressed_commit;
  g1_mul(&final_compressed_commit, &proof.D, &q);
  g1_add_or_dbl(&final_compressed_commit, &final_compressed_commit, &E);
  fr_t val_to_prove = proof.eval2;
  fr_mul(&val_to_prove, &val_to_prove, &q);
  fr_add(&val_to_prove, &val_to_prove, &proof.eval);

  // Final verification using a single pairing call.
  bool verification = false;
  check_proof_single(&verification, &proof.commitments[0], &proof.proof, &t,
                     &val_to_prove, &kzgs_);
  return verification;
}

bool VerkleTree::check_verkle_multiproof(const vector<string>& keys,
                                         const VerkleProof& proof) {
  map<vector<int>, pair<shared_ptr<VerkleNode>, set<int>>> required_proofs;
  for (auto& key : keys) {
    auto node_path = get_path(key);
    // update the main database.
    for (auto& ni : node_path) {
      // add indexes to the node.
      if (required_proofs.find(ni.first) == required_proofs.end()) {
        set<int> tmp;
        tmp.insert(ni.second.second);
        required_proofs[ni.first] = (make_pair(ni.second.first, tmp));
      } else {
        auto& val = required_proofs[ni.first];
        val.second.insert(ni.second.second);
        assert(val.first->hash == ni.second.first->hash);  // assert same node.
      }
    }
  }
  vector<pair<shared_ptr<VerkleNode>, set<int>>> to_proof;
  for (auto path_and_req_proof : required_proofs) {
    to_proof.push_back(path_and_req_proof.second);
  }

  vector<flat_node> X;
  for (auto& nod : to_proof) {
    vector<fr_t> ptmp(WIDTH);
    const auto& vnode = nod.first;
    for (int i = 0; i < WIDTH; ++i) {
      if (vnode->childs.find(i) == vnode->childs.end()) {
        fr_from_uint64(&ptmp[i], 0);
      } else {
        fr_from_uint64(&ptmp[i], vnode->childs.at(i)->hash);
      }
    }
    for (auto idx : nod.second) {
      flat_node nw;
      nw.commitment = vnode->commitment;
      nw.p = ptmp;
      nw.idx = idx;
      X.push_back(nw);
    }
  }
  vector<g1_t> commitments;
  vector<int> indices;
  vector<fr_t> Y;

  for (auto& x : X) {
    commitments.push_back(x.commitment);
    indices.push_back(x.idx);
    Y.push_back(x.p[x.idx]);
  }

  return kzg_check_multiproof(commitments, indices, Y, proof);
}

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