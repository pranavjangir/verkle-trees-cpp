#include <bits/stdc++.h>

extern "C" {
//#include "blst.h"
#include "bls12_381.h"
#include "c_kzg.h"
}
using namespace std;

// Constants

// TODO(pranav): Make this tunable from CLI.
// This means that each verkle tree node will have 2^WIDTH_BITS = 16 children.
constexpr int WIDTH_BITS = 4;
constexpr int KEYSIZE = 265;  // Fixed keysize of 256 bits.
constexpr int WIDTH = (1 << WIDTH_BITS);

static const scalar_t secret = {0xa4, 0x73, 0x31, 0x95, 0x28, 0xc8, 0xb6, 0xea,
                                0x4d, 0x08, 0xcc, 0x53, 0x18, 0x00, 0x00, 0x00,
                                0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                                0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};

struct VerkleNode {
  bool is_leaf = false;
  // Dummy commitment. Will be updated eventually.
  g1_t commitment = g1_generator;
  std::string key = "";
  std::string value = "";
  std::map<int, shared_ptr<VerkleNode>> childs;
  uint64_t hash = 0;
};

struct VerkleProof {
  vector<g1_t> commitments;
  g1_t D;
  g1_t proof;
  g1_t proof2;
  fr_t eval;
  fr_t eval2;
};

class VerkleTree {
 public:
  VerkleTree() {
    // special type = root.
    root_ = make_shared<VerkleNode>();
    root_->is_leaf = false;
    // random commitment for now.
    root_->commitment = g1_generator;
    root_->childs.clear();

    fr_t s_pow, s;
    fr_from_scalar(&s, &secret);
    s_pow = fr_one;

    for (uint64_t i = 0; i < WIDTH; i++) {
      g1_mul(&s1[i], &g1_generator, &s_pow);
      g2_mul(&s2[i], &g2_generator, &s_pow);
      fr_mul(&s_pow, &s_pow, &s);
    }
    new_fft_settings(&ffts_, WIDTH_BITS);
    new_kzg_settings(&kzgs_, s1, s2, WIDTH, &ffts_);

    // compute lagrange coeff
    fft_g1(s1_lagrange, s1, /*inverse = */ true, WIDTH, &ffts_);

    // calc ters 1 / (1 - omega^i) where omega = root of unity.
    fr_from_uint64(&inv[0], 0);
    for (int i = 1; i < WIDTH; ++i) {
      fr_t tmp;
      fr_from_uint64(&tmp, 1);
      fr_sub(&tmp, &tmp, (ffts_.expanded_roots_of_unity + i));
      fr_inv(&inv[i], &tmp);
    }
  }
  ~VerkleTree() {
    cout << "Destructor called. This is important\n";
    free_fft_settings(&ffts_);
    free_kzg_settings(&kzgs_);
  }
  std::vector<int> get_key_path(const std::string& key);
  void plain_insert_verkle_node(const std::string& key,
                                const std::string value);
  void compute_commitments();
  void dfs_commitment(shared_ptr<VerkleNode>& x);
  void poly_commitment(g1_t* out, const vector<uint64_t>& vals);
  void poly_commitment_fr(g1_t* out, const vector<fr_t>& vals);
  std::vector<pair<vector<int>, pair<shared_ptr<VerkleNode>, int>>> get_path(
      const string& key);
  VerkleProof get_verkle_multiproof(const vector<string>& keys);
  // For given keys, check that the values match using proofs.
  // Commitments list and proofs are present in `proof`.
  bool check_verkle_multiproof(const vector<string>& keys,
                               const VerkleProof& proof);
  VerkleProof kzg_gen_multiproof(
      vector<pair<shared_ptr<VerkleNode>, set<int>>> nodes);
  bool kzg_check_multiproof(const vector<g1_t>& commitments,
                            const vector<int>& indices, const vector<fr_t>& Y,
                            const VerkleProof& proof);
  // Computes (f(x) - f(idx)) / (x - w^idx)
  // Where f(x) is the poly corresponding to `in`.
  vector<fr_t> in_domain_q(const vector<fr_t>& in, int idx);
  // Wrapper around the c-kzg library for evaluation and proof of
  // a poly at a certain point.
  pair<fr_t, g1_t> eval_and_proof(const vector<fr_t>& p, fr_t pt);

 private:
  shared_ptr<VerkleNode> root_;
  FFTSettings ffts_;
  KZGSettings kzgs_;
  g1_t s1[WIDTH + 1];  // Consider giving extra space.
  g2_t s2[WIDTH + 1];
  g1_t s1_lagrange[WIDTH + 1];
  fr_t inv[WIDTH + 1];
};
