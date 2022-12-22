#include "verkle.h"

#include <chrono>

#include "ParserCode/json.hpp"
#include "blst.hpp"

using namespace std;

using json = nlohmann::json;

// Fiat shamir heuristic random value seeds.
int rr = 22;
int tt = 42;

std::vector<int> VerkleTree::get_key_path(const std::string& key) {
  // Bits must be multiple of 4.
  if (vctype_ != BMERKLE) {
    if (vctype_ == PMERKLE) {
      assert(WIDTH_BITS == 4);
    } else {
      assert(WIDTH_BITS % 4 == 0);
    }
  } else {
    assert(WIDTH_BITS == 1);
  }
  std::vector<int> out;
  std::string stripped = key;
  if (key.length() == 64 + 2) {  // key = 0x34fd....
    stripped = key.substr(2);
  }
  if (vctype_ == BMERKLE) {
    // binary conversion.
    for (int i = stripped.length() - 1; i >= 0; i--) {
      int val = -1;
      if (stripped[i] >= '0' && stripped[i] <= '9') {
        val = (stripped[i] - '0');
      } else if (stripped[i] >= 'a' && stripped[i] <= 'f') {
        val = 10 + (stripped[i] - 'a');
      }
      assert(val >= 0 && val <= 15);
      for (int j = 0; j < 4; ++j) {
        out.push_back(val % 2);
        val /= 2;
      }
    }
    return out;
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

void VerkleTree::ipa_poly_commitment(g1_t* out, const vector<uint64_t>& vals) {
  vector<fr_t> v(vals.size());
  for (int i = 0; i < vals.size(); ++i) {
    fr_from_uint64(&v[i], vals[i]);
  }
  ipa_poly_commitment_fr(out, v);
}

void VerkleTree::ipa_poly_commitment_fr(g1_t* out, const vector<fr_t>& vals) {
  fr_t valfr[WIDTH + 1];
  for (int i = 0; i < WIDTH; ++i) {
    valfr[i] = vals[i];
  }
  g1_linear_combination(out, G, valfr, WIDTH);
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
  if (vctype_ == KZG) {
    poly_commitment(&x->commitment, child_hashes);
    x->hash = hash_commitment(&x->commitment);
    return;
  } else if (vctype_ == IPA) {
    ipa_poly_commitment(&x->commitment, child_hashes);
    x->hash = hash_commitment(&x->commitment);
    return;
  }
  assert(is_merkle_);
  auto uint_hasher = std::hash<uint64_t>();
  for (auto& hash : child_hashes) {
    x->hash ^= uint_hasher(hash);
  }
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
fr_t VerkleTree::eval_poly_evaluation_form(const vector<fr_t>& p, fr_t pt) {
  fr_t out = fr_zero;
  fr_t inv_W;
  fr_t ptpow;
  fr_from_uint64(&inv_W, WIDTH);
  fr_inv(&inv_W, &inv_W);
  assert(p.size() == WIDTH);
  fr_pow(&ptpow, &pt, WIDTH);

  for (int i = 0; i < WIDTH; ++i) {
    fr_t num, denom;
    fr_mul(&num, &p[i], ffts_.expanded_roots_of_unity + i);
    fr_sub(&denom, &pt, ffts_.expanded_roots_of_unity + i);
    fr_div(&num, &num, &denom);
    fr_add(&out, &out, &num);
  }
  fr_t tmp;
  fr_sub(&tmp, &ptpow, &fr_one);
  fr_mul(&out, &out, &inv_W);
  fr_mul(&out, &out, &tmp);
  return out;
}

fr_t dotp_fr(const vector<fr_t>& A, const vector<fr_t>& B) {
  assert(A.size() == B.size());  // dot product requires vectors of equal size.
  fr_t out = fr_zero;
  for (int i = 0; i < A.size(); ++i) {
    fr_t tmp;
    fr_mul(&tmp, &A[i], &B[i]);
    fr_add(&out, &out, &tmp);
  }
  return out;
}

vector<fr_t> multiply_scalar_fr(const vector<fr_t>& A, const fr_t& B) {
  vector<fr_t> out(A.size());
  for (int i = 0; i < A.size(); ++i) {
    fr_mul(&out[i], &A[i], &B);
  }
  return out;
}

vector<g1_t> multiply_scalar_g1(const vector<g1_t>& A, const fr_t& B) {
  vector<g1_t> out(A.size());
  for (int i = 0; i < A.size(); ++i) {
    g1_mul(&out[i], &A[i], &B);
  }
  return out;
}

vector<fr_t> add_vector_fr(const vector<fr_t>& A, const vector<fr_t>& B) {
  assert(A.size() == B.size());  // addition requires vectors of equal size.
  vector<fr_t> out(A.size());
  for (int i = 0; i < A.size(); ++i) {
    fr_add(&out[i], &A[i], &B[i]);
  }
  return out;
}

vector<g1_t> add_vector_g1(const vector<g1_t>& A, const vector<g1_t>& B) {
  assert(A.size() == B.size());  // addition requires vectors of equal size.
  vector<g1_t> out(A.size());
  for (int i = 0; i < A.size(); ++i) {
    g1_add_or_dbl(&out[i], &A[i], &B[i]);
  }
  return out;
}

template <typename T>
pair<vector<T>, vector<T>> break_in_two(const vector<T>& A) {
  vector<T> L, R;
  int sz = A.size();  // should be a power of 2.
  for (int i = 0; i < sz / 2; ++i) L.push_back(A[i]);
  for (int i = sz / 2; i < sz; ++i) R.push_back(A[i]);
  return make_pair(L, R);
}

pair<fr_t, IPAProof> VerkleTree::ipa_eval_and_proof(const vector<fr_t>& p,
                                                    fr_t pt, g1_t C) {
  assert(p.size() ==
         WIDTH);  // Only support eval and proof for WIDTH size poly for now.
  fr_t evaluation = eval_poly_evaluation_form(p, pt);
  pair<fr_t, IPAProof> out;
  out.first = evaluation;

  // Proof computation.

  int sz = p.size();  // TODO(pranav): Assert that this should be a power of 2.

  // Set the value of Field vectors a and b.
  // Poly evaluation at `pt` = a dot b.
  vector<fr_t> a = p;
  vector<fr_t> b(sz);
  vector<g1_t> gg(sz);
  vector<g1_t> hh(sz);

  fr_t inv_W;
  fr_t ptpow;
  fr_from_uint64(&inv_W, WIDTH);
  fr_inv(&inv_W, &inv_W);
  fr_pow(&ptpow, &pt, WIDTH);

  fr_t k = ptpow;
  fr_sub(&k, &k, &fr_one);
  fr_mul(&k, &k, &inv_W);

  for (int i = 0; i < WIDTH; ++i) {
    b[i] = k;
    fr_mul(&b[i], &b[i], ffts_.expanded_roots_of_unity + i);
    fr_t tmp = pt;
    fr_sub(&tmp, &tmp, ffts_.expanded_roots_of_unity + i);
    fr_inv(&tmp, &tmp);
    fr_mul(&b[i], &b[i], &tmp);

    gg[i] = G[i];
    hh[i] = H[i];
  }
  fr_t xx, xxinv;  // TODO(pranav): Make this random.
  fr_from_uint64(&xx, 42);
  fr_inv(&xxinv, &xx);
  vector<pair<g1_t, g1_t>>
      ans;  // Pair of commitments CL and CR at every level.
  while (sz > 1) {
    auto alar = break_in_two<fr_t>(a);
    auto blbr = break_in_two<fr_t>(b);
    auto glgr = break_in_two<g1_t>(gg);
    auto hlhr = break_in_two<g1_t>(hh);

    fr_t zL = dotp_fr(alar.second, blbr.first);
    fr_t zR = dotp_fr(alar.first, blbr.second);

    g1_t CL = g1_identity;
    g1_t CR = CL;
    g1_t tmp;
    general_pippenger(&CL, glgr.first, alar.second);
    general_pippenger(&CR, glgr.second, alar.first);
    general_pippenger(&tmp, hlhr.second, blbr.first);
    g1_add_or_dbl(&CL, &CL, &tmp);
    g1_mul(&tmp, &Q, &zL);
    g1_add_or_dbl(&CL, &CL, &tmp);

    general_pippenger(&tmp, hlhr.first, blbr.second);
    g1_add_or_dbl(&CR, &CR, &tmp);
    g1_mul(&tmp, &Q, &zR);
    g1_add_or_dbl(&CR, &CR, &tmp);

    vector<fr_t> newa = multiply_scalar_fr(alar.second, xx);
    newa = add_vector_fr(alar.first, newa);

    vector<fr_t> newb = multiply_scalar_fr(blbr.second, xxinv);
    newb = add_vector_fr(blbr.first, newb);

    a = newa;
    b = newb;

    vector<g1_t> newg = multiply_scalar_g1(glgr.second, xxinv);
    newg = add_vector_g1(glgr.first, newg);

    vector<g1_t> newh = multiply_scalar_g1(hlhr.second, xx);
    newh = add_vector_g1(hlhr.first, newh);

    gg = newg;
    hh = newh;

    ans.push_back(make_pair(CL, CR));
    sz /= 2;
  }

  assert(a.size() == b.size());
  assert(a.size() == 1);

  // Proof is the list of CL and CR plus the scalar final values of a and b.
  out.second.a = a[0];
  out.second.b = b[0];
  out.second.C = ans;

  return out;
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

VerkleProof VerkleTree::ipa_gen_multiproof(
    vector<pair<shared_ptr<VerkleNode>, set<int>>> nodes) {
  VerkleProof out;
  // De-duplicated commitments :
  for (auto& x : nodes) {
    out.commitments.push_back(x.first->commitment);
  }
  cout << "Proof size IPA (# of elements) : " << out.commitments.size() << endl;
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
  g1_t gCommit;
  ipa_poly_commitment_fr(&gCommit, g);

  // Do the same for h(x) now.

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
  ipa_poly_commitment_fr(&E, h);

  auto h_eval_proof = ipa_eval_and_proof(h, tfr, E);
  auto g_eval_proof = ipa_eval_and_proof(g, tfr, gCommit);

  out.eval = h_eval_proof.first;
  out.eval2 = g_eval_proof.first;

  out.D = gCommit;
  out.E = E;

  out.ipa_h = h_eval_proof.second;
  out.ipa_g = g_eval_proof.second;

  return out;
}

VerkleProof VerkleTree::kzg_gen_multiproof(
    vector<pair<shared_ptr<VerkleNode>, set<int>>> nodes) {
  VerkleProof out;
  // De-duplicated commitments :
  for (auto& x : nodes) {
    out.commitments.push_back(x.first->commitment);
  }
  cout << "Proof size KZG (# of elements) : " << out.commitments.size() << endl;
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

VerkleProof VerkleTree::merkle_gen_multiproof(
    vector<pair<shared_ptr<VerkleNode>, set<int>>> nodes) {
  cout << "Generating proofs for merkle tree with width : " << WIDTH << endl;
  VerkleProof out;
  vector<pair<shared_ptr<VerkleNode>, vector<uint64_t>>> node_and_hashes;
  int proof_size = 0;
  for (auto& node_index : nodes) {
    vector<uint64_t> hashes;
    auto node = node_index.first;
    for (int i = 0; i < WIDTH; ++i) {
      if (node_index.second.find(i) == node_index.second.end() && 
      node->childs.find(i) != node->childs.end()) {
        hashes.push_back(node->childs[i]->hash);
      }
    }
    node_and_hashes.push_back(make_pair(std::move(node), hashes));
    proof_size += hashes.size();
  }
  out.node_and_hashes = node_and_hashes;
  cout << "Total merkle proof size for width : " << WIDTH << " is : " << proof_size << endl;
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
  VerkleProof out;
  auto proof_start = std::chrono::high_resolution_clock::now();
  if (vctype_ == KZG) {
    out = kzg_gen_multiproof(to_proof);
  } else if (vctype_ == IPA) {
    out = ipa_gen_multiproof(to_proof);
  } else {
    out = merkle_gen_multiproof(to_proof);
  }
  auto proof_end = std::chrono::high_resolution_clock::now();
  auto time_proof =
      std::chrono::duration_cast<std::chrono::seconds>(proof_end - proof_start);
  cout << "Time to ACTUALLY generate proof for all keys : " << time_proof.count()
       << " seconds." << endl;
  return out;
}

bool VerkleTree::ipa_verify(const g1_t C, const IPAProof& proof) {
  g1_t Cnew = C;
  vector<g1_t> gg(WIDTH);
  vector<g1_t> hh(WIDTH);
  int sz = WIDTH;
  // `Cptr` is used to iterate over the CL and CR commitments sent by the
  // prover.
  int Cptr = 0;

  fr_t xx, xxinv;  // TODO(pranav): Make this random.
  fr_from_uint64(&xx, 42);
  fr_inv(&xxinv, &xx);

  while (sz > 1) {
    g1_t CL = proof.C[Cptr].first;
    g1_t CR = proof.C[Cptr].second;
    g1_t tmp;
    g1_mul(&tmp, &CL, &xx);
    g1_add_or_dbl(&Cnew, &Cnew, &tmp);
    g1_mul(&tmp, &CR, &xxinv);
    g1_add_or_dbl(&Cnew, &Cnew, &tmp);

    auto glgr = break_in_two(gg);
    auto hlhr = break_in_two(hh);
    vector<g1_t> newg = multiply_scalar_g1(glgr.second, xxinv);
    newg = add_vector_g1(glgr.first, newg);

    vector<g1_t> newh = multiply_scalar_g1(hlhr.second, xx);
    newh = add_vector_g1(hlhr.first, newh);

    gg = newg;
    hh = newh;

    sz /= 2;
    Cptr++;
  }
  assert(Cptr == proof.C.size());
  assert(gg.size() == 1);
  assert(hh.size() == 1);

  g1_t computed_commitment = Q;
  fr_t tmp;
  g1_t gtmp;
  fr_mul(&tmp, &proof.a, &proof.b);
  g1_mul(&computed_commitment, &computed_commitment, &tmp);
  g1_mul(&gtmp, &hh[0], &proof.b);
  g1_add_or_dbl(&computed_commitment, &computed_commitment, &gtmp);
  g1_mul(&gtmp, &gg[0], &proof.a);
  g1_add_or_dbl(&computed_commitment, &computed_commitment, &gtmp);

  return g1_equal(&Cnew, &computed_commitment);
}

bool VerkleTree::ipa_check_multiproof(const vector<g1_t>& commitments,
                                      const vector<int>& indices,
                                      const vector<fr_t>& Y,
                                      const VerkleProof& proof) {
  bool verification = true;
  verification &= ipa_verify(proof.D, proof.ipa_g);
  verification &= ipa_verify(proof.E, proof.ipa_h);
  return verification;
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

bool VerkleTree::merkle_check_multiproof(
    const vector<string>& keys,
    const vector<pair<shared_ptr<VerkleNode>, set<int>>> to_proof,
    const VerkleProof& proof) {
  cout << "Checking merkle multiproofs for width : " << WIDTH << endl;
  auto verification_start = std::chrono::high_resolution_clock::now();
  bool ok = true;
  auto& nodes_and_hashes = proof.node_and_hashes;
  auto hasher = std::hash<uint64_t>();
  for (auto& nh : nodes_and_hashes) {
    auto node = nh.first;
    uint64_t hash_to_verify = node->hash;
    uint64_t computed_hash = 0;
    for (int i = 0; i < WIDTH; ++i) {
      if (node->childs.find(i) == node->childs.end()) continue;
      computed_hash ^= hasher(node->childs[i]->hash);
    }
    if (computed_hash != hash_to_verify) ok = false;
  }
  auto verification_end = std::chrono::high_resolution_clock::now();
  auto time_verification = std::chrono::duration_cast<std::chrono::seconds>(
      verification_end - verification_start);

  cout << "Time to ACTUALLY merkle verify proof for all keys : " << time_verification.count()
       << " seconds." << endl;
  return ok;
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

  // If the tree is merkle tree, then no need to flatten.
  if (is_merkle_) {
    return merkle_check_multiproof(keys, to_proof, proof);
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
  bool out = true;
  auto verification_start = std::chrono::high_resolution_clock::now();
  if (vctype_ == IPA) {
    out =  ipa_check_multiproof(commitments, indices, Y, proof);
  } else if (vctype_ == KZG) {
    out =  kzg_check_multiproof(commitments, indices, Y, proof);
  }
  auto verification_end = std::chrono::high_resolution_clock::now();
  auto time_verification = std::chrono::duration_cast<std::chrono::seconds>(
      verification_end - verification_start);

  cout << "Time to ACTUALLY verify proof for all keys : " << time_verification.count()
       << " seconds." << endl;
  return out;
}