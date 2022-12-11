#include <bits/stdc++.h>
extern "C" {
  #include "blst.h"
  #include "c_kzg.h"
  #include "bls12_381.h"
}
//#include "gmpxx.h"

using namespace std;

int main() {
    // Create a polynomial and ask for proof from the zkg library!
    cout << "Hello!!!\n";
    FFTSettings fft;
    unsigned int max_scale = 10;
    assert(C_KZG_OK == new_fft_settings(&fft, max_scale));
    // new_fft_settings(&fft, max_scale);
    cout << fft.max_width << endl;
    free_fft_settings(&fft);
}
