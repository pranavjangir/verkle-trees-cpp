#include <bits/stdc++.h>
extern "C" {
#include "bls12_381.h"
#include "blst.h"
#include "c_kzg.h"
}

using namespace std;

void generate_trusted_setup(g1_t *s1, g2_t *s2, const scalar_t *secret,
                            const uint64_t n) {
  fr_t s_pow, s;
  fr_from_scalar(&s, secret);
  s_pow = fr_one;

  for (uint64_t i = 0; i < n; i++) {
    g1_mul(s1 + i, &g1_generator, &s_pow);
    g2_mul(s2 + i, &g2_generator, &s_pow);
    fr_mul(&s_pow, &s_pow, &s);
  }
}

void prfr(fr_t x) {
  int sz = 256 / 8 / sizeof(limb_t);
  for (int i = 0; i < sz; ++i) {
    cout << "i = " << i << " --- " << x.l[i] << endl;
  }
  cout << "<><><><><><><><><><><><><><>\n";
}

static const scalar_t secret = {0xa4, 0x73, 0x31, 0x95, 0x28, 0xc8, 0xb6, 0xea,
                                0x4d, 0x08, 0xcc, 0x53, 0x18, 0x00, 0x00, 0x00,
                                0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                                0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};

int main() {
  // Create a polynomial and ask for proof from the zkg library!
  cout << "Checking kzg multi proof correctness!\n";
  uint64_t coeffs[] = {1, 2, 3, 4, 7, 7, 7, 7, 13, 13, 13, 13, 13, 13, 13, 13};
  int poly_len = sizeof coeffs / sizeof coeffs[0];
  FFTSettings fs1, fs2;
  KZGSettings ks1, ks2;
  poly p;
  g1_t commitment, proof;
  fr_t x, tmp;
  bool result;

  // Compute proof at 2^coset_scale points
  int coset_scale = 3, coset_len = (1 << coset_scale);
  fr_t y[coset_len];

  uint64_t secrets_len = poly_len > coset_len ? poly_len + 1 : coset_len + 1;
  g1_t s1[secrets_len];
  g2_t s2[secrets_len];

  // Create the polynomial
  new_poly(&p, poly_len);
  for (int i = 0; i < poly_len; i++) {
    fr_from_uint64(&p.coeffs[i], coeffs[i]);
  }

  // Initialise the secrets and data structures
  generate_trusted_setup(s1, s2, &secret, secrets_len);
  assert(C_KZG_OK == new_fft_settings(&fs1, 4));  // ln_2 of poly_len
  assert(C_KZG_OK == new_kzg_settings(&ks1, s1, s2, secrets_len, &fs1));

  // Commit to the polynomial
  assert(C_KZG_OK == commit_to_poly(&commitment, &p, &ks1));

  assert(C_KZG_OK == new_fft_settings(&fs2, coset_scale));
  assert(C_KZG_OK == new_kzg_settings(&ks2, s1, s2, secrets_len, &fs2));

  // Compute proof at the points [x * root_i] 0 <= i < coset_len
  // IMP NOTE : In actual implementation, this needs to change.
  // This should be able to take multiple different values
  // and the polynomial wont be x^n - x0^n.
  // Check
  // https://dankradfeist.de/ethereum/2020/06/16/kate-polynomial-commitments.html
  // for how multiproofs should actually be done.
  fr_from_uint64(&x, 1);
  assert(C_KZG_OK == compute_proof_multi(&proof, &p, &x, coset_len, &ks2));

  // y_i is the value of the polynomial at each x_i
  for (int i = 0; i < coset_len; i++) {
    fr_mul(&tmp, &x, &ks2.fs->expanded_roots_of_unity[i]);
    prfr(tmp);
    cout << ">>>>>>>>>>>>>>>>>>>>>\n";
    eval_poly(&y[i], &p, &tmp);
    prfr(y[i]);
  }

  // Verify the proof that the (unknown) polynomial has value y_i at x_i
  assert(C_KZG_OK == check_proof_multi(&result, &commitment, &proof, &x, y,
                                       coset_len, &ks2));
  assert(true == result);
  cout << "First attempt : " << result << endl;

  // Change a value and check that the proof fails
  fr_add(y + coset_len / 2, y + coset_len / 2, &fr_one);
  assert(C_KZG_OK == check_proof_multi(&result, &commitment, &proof, &x, y,
                                       coset_len, &ks2));
  assert(false == result);
  cout << "Second attempt : " << result << endl;

  free_fft_settings(&fs1);
  free_fft_settings(&fs2);
  free_kzg_settings(&ks1);
  free_kzg_settings(&ks2);
  free_poly(&p);
}
