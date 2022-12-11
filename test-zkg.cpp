#include <bits/stdc++.h>
#include "blst.h"
//#include "gmpxx.h"
#include "c_kzg.h"
#include "bls12_381.h"

using namespace std;

int main() {
    // Create a polynomial and ask for proof from the zkg library!
    cout << "Hello!!!\n";
    FFTSettings fft;
    unsigned int max_scale = (1 << 10);
    new_fft_settings(&fft, max_scale);
    cout << fft.max_width << endl;
    free_fft_settings(&fft);
}
