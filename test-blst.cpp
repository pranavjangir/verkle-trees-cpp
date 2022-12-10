#include <bits/stdc++.h>
#include "blst.h"

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
    cout <<"is it in g1? : "<< blst_p1_in_g1(p1) << endl;
    cout << affinept.x.l[4] << " -- " << affinept.y.l[5] << endl;
}
