#include <bits/stdc++.h>

#include "blst.h"
#include "gmpxx.h"
// #include "gmp.h"

using namespace std;

int main() {
  cout << "Hello!!!\n";
  auto p1 = blst_p1_generator();
  auto p2 = blst_p1_generator();
  auto p3 = blst_p1_generator();
  auto p4 = blst_p1_generator();
  blst_p1_affine affinept;
  cout << "p1 = p4 ? " << blst_p1_is_equal(p1, p4) << endl;
  cout << "p2 = p3 ? " << blst_p1_is_equal(p2, p3) << endl;
  blst_p1_to_affine(&affinept, p1);
  cout << "is it in g1? : " << blst_p1_in_g1(p1) << endl;
  cout << affinept.x.l[4] << " -- " << affinept.y.l[5] << endl;
  mpz_t modulus;
  mpz_init(modulus);
  char* prme =
      (char*)"73eda753299d7d483339d80809a1d80553bda402fffe5bfeffffffff00000001";
  mpz_set_str(modulus, prme, 16);
  cout << "modulus : " << modulus << endl;
  mpz_t g;
  mpz_init(g);
  mpz_set_ui(g, 7);
  mpz_t ssquare;
  mpz_init(ssquare);
  mpz_t smod;
  mpz_init(smod);
  mpz_t mod2;
  mpz_init(mod2);
  mpz_sub_ui(mod2, modulus, 1);
  mpz_powm_ui(ssquare, g, 2, modulus);
  cout << "7sqr : " << ssquare << endl;
  mpz_powm(smod, g, mod2, modulus);
  cout << "7mod : " << smod << endl;
  mpz_clear(modulus);
  mpz_clear(ssquare);
  mpz_clear(smod);
  mpz_clear(g);
  mpz_class svn(g);
  cout << svn << endl;
  cout << svn - 5 << endl;
}
